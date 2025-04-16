/* pipe.c - Network UPS Tools driver-server pipe debugger (WIN32 builds)

   Copyright (C) 2012  Frederic Bohe <fredericbohe@eaton.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


#include "common.h"
#include "parseconf.h"


PCONF_CTX_t	pipe_ctx;

static void pipe_arg(int numarg, char **arg)
{
	int	i;

	printf("numarg=%d : ", numarg);

	for (i = 0; i < numarg; i++)
		printf("[%s] ", arg[i]);

	printf("\n");
	fflush(stdout);
}

static HANDLE pipe_connect(const char *pipefn)
{
	HANDLE	fd;
	char	pipename[NUT_PATH_MAX];
	BOOL	result = FALSE;

	snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\%s", pipefn);

	result = WaitNamedPipe(pipename,NMPWAIT_USE_DEFAULT_WAIT);

	if( result == FALSE ) {
		printf("WaitNamedPipe : %d\n",GetLastError());
		exit(EXIT_FAILURE);
	}

	fd = CreateFile(
			pipename,       /* pipe name */
			GENERIC_READ |  /* read and write access */
			GENERIC_WRITE,
			0,              /* no sharing */
			NULL,           /* default security attributes FIXME */
			OPEN_EXISTING,  /* opens existing pipe */
			FILE_FLAG_OVERLAPPED, /*  enable async IO */
			NULL);          /* no template file */

	if (fd == INVALID_HANDLE_VALUE) {
		printf("CreateFile : %d\n",GetLastError());
		exit(EXIT_FAILURE);
	}

	return fd;
}

static void read_buf(char * buf, DWORD num)
{
	unsigned int	i;

	for (i = 0; i < num; i++) {

		switch (pconf_char(&pipe_ctx, buf[i])) {
			case 1:
				pipe_arg(pipe_ctx.numargs, pipe_ctx.arglist);
				break;

			case -1:
				printf("Parse error: [%s]\n", pipe_ctx.errmsg);
				break;
		}
	}
}

DWORD WINAPI ReadThread( LPVOID lpParameter )
{
	HANDLE	pipefd = *((HANDLE *)lpParameter);
	DWORD	bytes_read;
	char	pipe_buf[SMALLBUF];
	OVERLAPPED	pipe_overlapped;

	pconf_init(&pipe_ctx, NULL);

	memset(&pipe_overlapped,0,sizeof(pipe_overlapped));
	pipe_overlapped.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

	for (;;) {
		memset(pipe_buf,0,sizeof(pipe_buf));
		ReadFile(pipefd,pipe_buf,sizeof(pipe_buf),NULL,&pipe_overlapped);
		GetOverlappedResult(pipefd,&pipe_overlapped,&bytes_read,TRUE);
		read_buf(pipe_buf,bytes_read);
	}
}

DWORD WINAPI WriteThread( LPVOID lpParameter )
{
	HANDLE	pipefd = *((HANDLE *)lpParameter);
	HANDLE	hStdin;
	DWORD	bytes_read;
	char	stdin_buf[SMALLBUF];
	OVERLAPPED	pipe_overlapped;

	hStdin = GetStdHandle(STD_INPUT_HANDLE);

	memset(&pipe_overlapped,0,sizeof(pipe_overlapped));
	pipe_overlapped.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

	for (;;) {
		ReadFile(hStdin,stdin_buf,sizeof(stdin_buf),&bytes_read,NULL);
		WriteFile(pipefd,stdin_buf,bytes_read,NULL,&pipe_overlapped);
	}
}
int main(int argc, char **argv)
{
	const char	*prog = xbasename(argv[0]);
	HANDLE	pipefd;
	HANDLE	thread[2];

	if (argc != 2
	|| (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
	) {
		fprintf(stderr, "usage: %s <pipe name>\n", prog);
		fprintf(stderr, "       %s apcsmart-com1\n",
			argv[0]);

		fprintf(stderr, "\n%s", suggest_doc_links(prog, NULL));

		exit(EXIT_SUCCESS);
	}

	pipefd = pipe_connect(argv[1]);

	printf("connected: fd %d\n", pipefd);
	fflush(stdout);

	thread[0] = CreateThread(
					NULL,	/* security */
					0,	/* stack size */
					ReadThread, /* func */
					&pipefd,/* func param */
					0,	/* flags */
					NULL ); /* thread id */

	if(thread[0] == NULL) {
		fprintf(stderr, "CreateThread ReadThread failed\n");
		exit(EXIT_FAILURE);
	}

	thread[1] = CreateThread(
					NULL,	/* security */
					0,	/* stack size */
					WriteThread, /* func */
					&pipefd,/* func param */
					0,	/* flags */
					NULL ); /* thread id */

	if(thread[1] == NULL) {
		fprintf(stderr, "CreateThread WriteThread failed\n");
		exit(EXIT_FAILURE);
	}

	WaitForMultipleObjects(2,thread,TRUE,INFINITE);

	/* NOTREACHED */
	exit(EXIT_FAILURE);
}
