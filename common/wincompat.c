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
#include "winevent.h"

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

void syslog(int priority, const char *fmt, ...)
{
	LPCTSTR strings[2];
	char buf[LARGEBUF];
	va_list ap;
	HANDLE EventSource;

	if( EventLogName == NULL ) {
		return;
	}
	va_start(ap,fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	strings[0] = buf;
	strings[1] = EventLogName;

	EventSource = RegisterEventSource(NULL, EventLogName);

	if( NULL != EventSource ) {
		ReportEvent(	EventSource,		/* event log handle */
				priority,		/* event type */
				0,			/* event category */
				SVC_ERROR,		/* event identifier */
				NULL,			/* no security identifier */
				2,			/* size of string array */
				0,			/* no binary data */
				strings,		/* array of string */
				NULL);			/* no binary data */

		DeregisterEventSource(EventSource);

	}
}
#endif
