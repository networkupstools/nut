/* sockdebug.c - Network UPS Tools driver-server socket debugger

   Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>

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


#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"
#include "parseconf.h"

	PCONF_CTX_t	sock_ctx;

static void sock_arg(int numarg, char **arg)
{
	int	i;

	printf("numarg=%d : ", numarg);

	for (i = 0; i < numarg; i++)
		printf("[%s] ", arg[i]);

	printf("\n");
}

static int socket_connect(const char *sockfn)
{
	int	ret, fd;
	struct	sockaddr_un sa;

	memset(&sa, '\0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", sockfn);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	ret = connect(fd, (struct sockaddr *) &sa, sizeof(sa));

	if (ret < 0) {
		perror("connect");
		exit(EXIT_FAILURE);
	}

#if 0
	ret = fcntl(fd, F_GETFL, 0);

	if (ret < 0) {
		perror("fcntl(get)");
		exit(EXIT_FAILURE);
	}

	ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		perror("fcntl(set)");
		exit(EXIT_FAILURE);
	}
#endif

	return fd;
}

static void read_sock(int fd)
{
	int	i, ret;
	char	buf[SMALLBUF];

	ret = read(fd, buf, sizeof(buf));

	if (ret == 0) {
		fprintf(stderr, "read on socket returned 0\n");
		exit(EXIT_FAILURE);
	}

	if (ret < 0) {
		perror("read sockfd");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < ret; i++) {

		switch (pconf_char(&sock_ctx, buf[i])) {
			case 1:
				sock_arg(sock_ctx.numargs, sock_ctx.arglist);
				break;

			case -1:
				printf("Parse error: [%s]\n", sock_ctx.errmsg);
				break;
		}
	}
}

int main(int argc, char **argv)
{
	const char	*prog = xbasename(argv[0]);
	int	ret, sockfd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <socket name>\n", prog);
		fprintf(stderr, "       %s /var/state/ups/apcsmart-ttyS1.newsock\n",
			argv[0]);
		exit(EXIT_SUCCESS);
	}

	sockfd = socket_connect(argv[1]);

	printf("connected: fd %d\n", sockfd);

	pconf_init(&sock_ctx, NULL);

	for (;;) {
		struct	timeval	tv;
		fd_set	rfds;
		int	maxfd;

		tv.tv_sec = 2;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(fileno(stdin), &rfds);
		FD_SET(sockfd, &rfds);

		/* paranoia */
		maxfd = (sockfd > fileno(stdin)) ? sockfd : fileno(stdin);

		ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

		if (FD_ISSET(sockfd, &rfds))
			read_sock(sockfd);

		if (FD_ISSET(fileno(stdin), &rfds)) {
			char	buf[SMALLBUF];

			fgets(buf, sizeof(buf), stdin);

			ret = write(sockfd, buf, strlen(buf));

			if ((ret < 0) || (ret != (int) strlen(buf))) {
				perror("write to socket");
				exit(EXIT_FAILURE);
			}
		}
	}

	/* NOTREACHED */
	exit(EXIT_FAILURE);
}
