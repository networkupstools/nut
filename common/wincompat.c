/* 

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, WRITE to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.              
*/
#ifdef WIN32
#include "common.h"
#include <winsock2.h>
#include <windows.h>

extern int errno;

const char * EventLogName = NULL;

int sktconnect(int fh, struct sockaddr * name, int len)
{
	int ret = connect(fh,name,len);
	errno = WSAGetLastError();
	return ret;
}
int sktread(int fh, char *buf, int size)
{
	int ret = recv(fh,buf,size,0);
	errno = WSAGetLastError();
	return ret;
}
int sktwrite(int fh, char *buf, int size)
{
	int ret = send(fh,buf,size,0);
	errno = WSAGetLastError();
	return ret;
}
int sktclose(int fh)
{
	int ret = closesocket((SOCKET)fh);
	errno = WSAGetLastError();
	return ret;
}

/* syslog sends a message through a pipe to the wininit service. Which is
in charge of adding an event in the Windows event logger.
The message is made of 4 bytes containing the priority followed by an array
of chars containing the message to display (no terminal 0 required here) */
void syslog(int priority, const char *fmt, ...)
{
	char buf1[LARGEBUF];
	char buf2[LARGEBUF];
	va_list ap;
	HANDLE pipe;
	BOOL result = FALSE;
	DWORD bytesWritten = 0;

	if( EventLogName == NULL ) {
		return;
	}

	/* Format message */
	va_start(ap,fmt);
	vsnprintf(buf1, sizeof(buf1), fmt, ap);
	va_end(ap);

	/* Add progname */
	snprintf(buf2,sizeof(buf2),"%s - %s",EventLogName,buf1);

	/* Add priority to create the whole frame */
	*((DWORD *)buf1) = (DWORD)priority;
	memcpy(buf1+sizeof(DWORD),buf2,sizeof(buf2));

	pipe = CreateFile(
			EVENTLOG_PIPE_NAME,	/* pipe name */
			GENERIC_WRITE,
			0,			/* no sharing */
			NULL,			/* default security attributes FIXME */
			OPEN_EXISTING,		/* opens existing pipe */
			FILE_FLAG_OVERLAPPED,	/* enable async IO */
			NULL);			/* no template file */


	if (pipe == INVALID_HANDLE_VALUE) {
		return;
	}

	result = WriteFile (pipe,buf1,strlen(buf2)+sizeof(DWORD),&bytesWritten,NULL);

	/* testing result is useless. If we have an error and try to report it,
	this will probably lead to a call to this function and an infinite 
	loop */
	/*
	if (result == 0 || bytesWritten != strlen(buf2)+sizeof(DWORD) ) {
	return;;
	}
	*/
	CloseHandle(pipe);
}
#endif
