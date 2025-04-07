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
#include "config.h" /* should be first */

#include "wincompat.h"
#include "nut_stdint.h"

#if ! HAVE_INET_PTON
# include <string.h>
# include <ctype.h>
# if HAVE_WINSOCK2_H
#  include <winsock2.h>
# endif
# if HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
# endif
#endif

#if (0)
extern int errno;
#endif

const char * EventLogName = NULL;

struct passwd wincompat_passwd;
char wincompat_user_name[SMALLBUF];
char wincompat_password[SMALLBUF];

uid_t getuid(void)
{
	DWORD size = sizeof(wincompat_user_name);
	if( !GetUserName(wincompat_user_name,&size) ) {
		return NULL;
	}

	return wincompat_user_name;
}

struct passwd *getpwuid(uid_t uid)
{
	wincompat_passwd.pw_name = uid;
	wincompat_passwd.pw_uid = 0;
	return &wincompat_passwd;
}

char *getpass( const char *prompt)
{
	HANDLE hStdin;
	DWORD mode;

	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if(hStdin == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	printf("%s",prompt);

	GetConsoleMode( hStdin, &mode );
	mode &= ~ENABLE_ECHO_INPUT;
	SetConsoleMode( hStdin , mode);

	if (fgets(wincompat_password, sizeof(wincompat_password), stdin) == NULL) {
		upsdebug_with_errno(LOG_INFO, "%s", __func__);
		return NULL;
	}

	/* deal with that pesky newline */
	if (strlen(wincompat_password) > 1) {
		wincompat_password[strlen(wincompat_password) - 1] = '\0';
	};

	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode( hStdin, &mode );
	mode |= ENABLE_ECHO_INPUT;
	SetConsoleMode( hStdin , mode);

	return wincompat_password;
}

#ifndef HAVE_USLEEP
/* Verbatim from
http://cygwin.com/cgi-bin/cvsweb.cgi/~checkout~/src/winsup/mingw/mingwex/usleep.c?rev=1.2&cvsroot=src */
/* int __cdecl usleep(unsigned int useconds) */
int __cdecl usleep(useconds_t useconds)
{
	if(useconds == 0)
		return 0;

	if(useconds >= 1000000)
		return EINVAL;

	Sleep((useconds + 999) / 1000);

	return 0;
}
#endif /* !HAVE_USLEEP */

char * strtok_r(char *str, const char *delim, char **saveptr)
{
	char *token_start, *token_end;

	/* Subsequent call ? */
	token_start = str ? str : *saveptr;

	/* Skip delim characters */
	token_start += strspn(token_start, delim);
	if (*token_start == '\0') {
		/* No more token */
		*saveptr = "";
		return NULL;
	}

	/* Skip NO delim characters */
	token_end = token_start + strcspn(token_start, delim);

	/* Prepare token to be a null terminated string */
	if (*token_end != '\0')
		*token_end++ = '\0';

	*saveptr = token_end;

	return token_start;
}

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

#if ! HAVE_INET_NTOP
# if (0)
/* Some old winapi? or just sloppy original commits? */
const char* inet_ntop(int af, const void* src, char* dst, int cnt)
# else
const char* inet_ntop(int af, const void* src, char* dst, size_t cnt)
# endif
{
/* Instead of WSAAddressToString() consider getnameinfo() if this would in fact
 * return decorated addresses (brackets, ports...) as discussed below:
 * https://users.ipv6.narkive.com/RXpR5aML/windows-and-inet-ntop-vs-wsaaddresstostring
 * https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getnameinfo
 * https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaaddresstostringa
 */
	switch (af) {
	case AF_INET:
		{
			struct sockaddr_in srcaddr;
			memset(&srcaddr, 0, sizeof(struct sockaddr_in));
			memcpy(&(srcaddr.sin_addr), src, sizeof(srcaddr.sin_addr));
			srcaddr.sin_family = af;
			if (WSAAddressToString((struct sockaddr*) &srcaddr, sizeof(struct sockaddr_in), 0, dst, (LPDWORD) &cnt) != 0) {
				WSAGetLastError();
				return NULL;
			}
		}
		break;

	case AF_INET6:
		/* NOTE: Since WinXP SP1, with IPv6 installed on the system */
		{
			struct sockaddr_in6 srcaddr;
			memset(&srcaddr, 0, sizeof(struct sockaddr_in6));
			memcpy(&(srcaddr.sin6_addr), src, sizeof(srcaddr.sin6_addr));
			srcaddr.sin6_family = af;
			if (WSAAddressToString((struct sockaddr*) &srcaddr, sizeof(struct sockaddr_in6), 0, dst, (LPDWORD) &cnt) != 0) {
				WSAGetLastError();
				return NULL;
			}
		}
		break;

	default:
		errno = EAFNOSUPPORT;
		return NULL;
	} /* switch */

	return dst;
}
#endif	/* HAVE_INET_NTOP */

#if ! HAVE_INET_PTON
/* Fallback implementation of inet_pton() for systems that lack it,
 * such as older versions of Windows (including MinGW builds that do
 * not specifically target _WIN32_WINNT or newer).
 *
 * Based on code attributed to Paul Vixie, 1996,
 * sourced from https://stackoverflow.com/a/15370175/4715872
 */

#define NS_INADDRSZ	sizeof(struct in_addr)	/*  4 */
#define NS_IN6ADDRSZ	sizeof(struct in6_addr)	/* 16 */
#define NS_INT16SZ	sizeof(uint16_t)	/*  2 */

static int inet_pton4(const char *src, void *dst)
{
	uint8_t tmp[NS_INADDRSZ], *tp;	/* for struct in_addr *dst */

	int saw_digit = 0;
	int octets = 0;
	int ch;

	*(tp = tmp) = 0;

	while ((ch = *src++) != '\0')
	{
		if (ch >= '0' && ch <= '9')
		{
			uint32_t n = *tp * 10 + (ch - '0');

			if (saw_digit && *tp == 0)
				return 0;

			if (n > 255)
				return 0;

			*tp = n;
			if (!saw_digit)
			{
				if (++octets > 4)
					return 0;
				saw_digit = 1;
			}
		}
		else if (ch == '.' && saw_digit)
		{
			if (octets == 4)
				return 0;
			*++tp = 0;
			saw_digit = 0;
		}
		else
			return 0;
	}
	if (octets < 4)
		return 0;

	memcpy(dst, tmp, NS_INADDRSZ);

	return 1;
}

static int inet_pton6(const char *src, void *dst)
{
	static const char xdigits[] = "0123456789abcdef";
	uint8_t tmp[NS_IN6ADDRSZ];	/* for struct in6_addr *dst */

	uint8_t *tp = (uint8_t*) memset(tmp, '\0', NS_IN6ADDRSZ);
	uint8_t *endp = tp + NS_IN6ADDRSZ;
	uint8_t *colonp = NULL;

	const char *curtok = NULL;
	int saw_xdigit = 0;
	uint32_t val = 0;
	int ch;

	/* Leading :: requires some special handling. */
	if (*src == ':')
	{
		if (*++src != ':')
			return 0;
	}

	curtok = src;

	while ((ch = tolower(*src++)) != '\0')
	{
		const char *pch = strchr(xdigits, ch);
		if (pch != NULL)
		{
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return 0;
			saw_xdigit = 1;
			continue;
		}
		if (ch == ':')
		{
			curtok = src;
			if (!saw_xdigit)
			{
				if (colonp)
					return 0;
				colonp = tp;
				continue;
			}
			else if (*src == '\0')
			{
				return 0;
			}
			if (tp + NS_INT16SZ > endp)
				return 0;
			*tp++ = (uint8_t) (val >> 8) & 0xff;
			*tp++ = (uint8_t) val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) &&
				inet_pton4(curtok, (char*) tp) > 0)
		{
			tp += NS_INADDRSZ;
			saw_xdigit = 0;
			break; /* '\0' was seen by inet_pton4(). */
		}
		return 0;
	}
	if (saw_xdigit)
	{
		if (tp + NS_INT16SZ > endp)
			return 0;
		*tp++ = (uint8_t) (val >> 8) & 0xff;
		*tp++ = (uint8_t) val & 0xff;
	}
	if (colonp != NULL)
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		if (tp == endp)
			return 0;

		for (i = 1; i <= n; i++)
		{
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return 0;

	memcpy(dst, tmp, NS_IN6ADDRSZ);

	return 1;
}

int inet_pton(int af, const char *src, void *dst)
{
	switch (af)
	{
	case AF_INET:
		return inet_pton4(src, dst);
	case AF_INET6:
		return inet_pton6(src, dst);
	default:
		return -1;
	}
}
#endif	/* ! HAVE_INET_PTON */

/* "system" call seems to handle path with blank name incorrectly */
int win_system(const char * command)
{
	BOOL res;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	memset(&si,0,sizeof(si));
	si.cb = sizeof(si);
	memset(&pi,0,sizeof(pi));

	res = CreateProcess(NULL,(char *)command,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);

	if( res != 0 ) {
		return 0;
	}

	return -1;
}

/* the " character is forbiden in Windows files , so we filter this character
in data file paths to be coherent with command line which require " to
distinguish the command from its parameter. This avoid complicated
explanation in the documentation */
char * filter_path(const char * source)
{
	char * res;
	unsigned int i,j;

	if( source == NULL ) {
		return NULL;
	}

	res = xmalloc(strlen(source)+1);
	for(i=0,j=0;i<=strlen(source);i++) {
		if(source[i] != '"') {
			res[j] = source[i];
			j++;
		}
	}

	return res;
}


/* syslog sends a message through a pipe to the wininit service. Which is
   in charge of adding an event in the Windows event logger.
   The message is made of 4 bytes containing the priority followed by an array
   of chars containing the message to display (no terminal 0 required here) */
void syslog(int priority, const char *fmt, ...)
{
	char pipe_name[] = "\\\\.\\pipe\\"EVENTLOG_PIPE_NAME;
	char buf1[LARGEBUF+sizeof(DWORD)];
	char buf2[LARGEBUF];
	va_list ap;
	HANDLE pipe;
	DWORD bytesWritten = 0;

	if( EventLogName == NULL ) {
		return;
	}

	/* Format message */
	va_start(ap,fmt);
	vsnprintf(buf1, sizeof(buf1), fmt, ap);
	va_end(ap);

	/* Add progname to the formated message */
	snprintf(buf2, sizeof(buf2), "%s - %s", EventLogName, buf1);

	/* Create the frame */
	/* first 4 bytes are priority */
	memcpy(buf1,&priority,sizeof(DWORD));
	/* then comes the message */
	memcpy(buf1+sizeof(DWORD),buf2,sizeof(buf2));

	pipe = CreateFile(
			pipe_name,	/* pipe name */
			GENERIC_WRITE,
			0,			/* no sharing */
			NULL,			/* default security attributes FIXME */
			OPEN_EXISTING,		/* opens existing pipe */
			FILE_FLAG_OVERLAPPED,	/* enable async IO */
			NULL);			/* no template file */


	if (pipe == INVALID_HANDLE_VALUE) {
		return;
	}

	WriteFile (pipe,buf1,strlen(buf2)+sizeof(DWORD),&bytesWritten,NULL);

	/* testing result is useless. If we have an error and try to report it,
	   this will probably lead to a call to this function and an infinite
	   loop */
	CloseHandle(pipe);
}

/* Signal emulation via NamedPipe */

static HANDLE		pipe_connection_handle;
OVERLAPPED		pipe_connection_overlapped;
pipe_conn_t		*pipe_connhead = NULL;
static const char	*named_pipe_name=NULL;

void pipe_create(const char * pipe_name)
{
	BOOL ret;
	char pipe_full_name[NUT_PATH_MAX + 1];

	/* save pipe name for further use in pipe_connect */
	if (pipe_name == NULL) {
		if (named_pipe_name == NULL) {
			return;
		}
	}
	else {
		named_pipe_name = pipe_name;
	}

	snprintf(pipe_full_name, sizeof(pipe_full_name),
		"\\\\.\\pipe\\%s", named_pipe_name);

	if( pipe_connection_overlapped.hEvent != 0 ) {
		CloseHandle(pipe_connection_overlapped.hEvent);
	}
	memset(&pipe_connection_overlapped,0,sizeof(pipe_connection_overlapped));
	pipe_connection_handle = CreateNamedPipe(
			pipe_full_name,
			PIPE_ACCESS_INBOUND |   /* to server only */
			FILE_FLAG_OVERLAPPED,   /* async IO */
			PIPE_TYPE_MESSAGE |
			PIPE_READMODE_MESSAGE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES, /* max. instances */
			LARGEBUF,               /* output buffer size */
			LARGEBUF,               /* input buffer size */
			0,                      /* client time-out */
			NULL);  /* FIXME: default security attribute */

	if (pipe_connection_handle == INVALID_HANDLE_VALUE) {
		upslogx(LOG_ERR, "Error creating named pipe");
		fatal_with_errno(EXIT_FAILURE,
			"Can't create a state socket (windows named pipe)");
	}

	/* Prepare an async wait on a connection on the pipe */
	pipe_connection_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if(pipe_connection_overlapped.hEvent == NULL ) {
		upslogx(LOG_ERR, "Error creating event");
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	/* Wait for a connection */
	ret = ConnectNamedPipe(pipe_connection_handle,&pipe_connection_overlapped);
	if(ret == 0 && GetLastError() != ERROR_IO_PENDING ) {
		upslogx(LOG_ERR,"ConnectNamedPipe error");
	}
}

void pipe_connect()
{
	/* We have detected a connection on the opened pipe. So we start by saving its handle and create a new pipe for future connections */
	pipe_conn_t *conn;

	conn = xcalloc(1,sizeof(*conn));
	conn->handle = pipe_connection_handle;

	/* restart a new listening pipe */
	pipe_create(NULL);

	/* A new pipe waiting for new client connection has been created. We could manage the current connection now */
	/* Start a read operation on the newly connected pipe so we could wait on the event associated to this IO */
	memset(&conn->overlapped,0,sizeof(conn->overlapped));
	memset(conn->buf,0,sizeof(conn->buf));
	conn->overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if(conn->overlapped.hEvent == NULL ) {
		upslogx(LOG_ERR,"Can't create event for reading event log");
		return;
	}

	ReadFile (conn->handle, conn->buf,
		sizeof(conn->buf)-1, /* -1 to be sure to have a trailling 0 */
		NULL, &(conn->overlapped));

	if (pipe_connhead) {
		conn->next = pipe_connhead;
		pipe_connhead->prev = conn;
	}

	pipe_connhead = conn;
}

void pipe_disconnect(pipe_conn_t *conn)
{
	if( conn->overlapped.hEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(conn->overlapped.hEvent);
		conn->overlapped.hEvent = INVALID_HANDLE_VALUE;
	}
	if( conn->handle != INVALID_HANDLE_VALUE) {
		if ( DisconnectNamedPipe(conn->handle) == 0 ) {
			upslogx(LOG_ERR,
				"DisconnectNamedPipe error : %d",
				(int)GetLastError());
		}
		CloseHandle(conn->handle);
		conn->handle = INVALID_HANDLE_VALUE;
	}
	if (conn->prev) {
		conn->prev->next = conn->next;
	} else {
		pipe_connhead = conn->next;
	}

	if (conn->next) {
		conn->next->prev = conn->prev;
	} else {
		/* conntail = conn->prev; */
	}

	free(conn);
}

int pipe_ready(pipe_conn_t *conn)
{
	DWORD   bytesRead;
	BOOL    res;

	res = GetOverlappedResult(conn->handle, &conn->overlapped, &bytesRead, FALSE);
	if( res == 0 ) {
		upslogx(LOG_ERR, "Pipe read error");
		pipe_disconnect(conn);
		return 0;
	}
	return 1;
}

/* return 1 on error, 0 if OK */
int send_to_named_pipe(const char * pipe_name, const char * data)
{
	HANDLE pipe;
	BOOL result = FALSE;
	DWORD bytesWritten = 0;
	char buf[SMALLBUF];

	snprintf(buf, sizeof(buf), "\\\\.\\pipe\\%s", pipe_name);

	pipe = CreateFile(
			buf,
			GENERIC_WRITE,
			0,			/* no sharing */
			NULL,			/* default security attributes FIXME */
			OPEN_EXISTING,		/* opens existing pipe */
			FILE_FLAG_OVERLAPPED,	/* enable async IO */
			NULL);			/* no template file */


	if (pipe == INVALID_HANDLE_VALUE) {
		return 1;
	}

	result = WriteFile (pipe,data,strlen(data)+1,&bytesWritten,NULL);

	if (result == 0 || bytesWritten != strlen(data)+1 ) {
		CloseHandle(pipe);
		return 1;
	}

	CloseHandle(pipe);
	return 0;
}

int w32_setcomm ( serial_handler_t * h, int * flags )
{
	int ret = 0;

	if( *flags & TIOCM_DTR ) {
		if( !EscapeCommFunction(h->handle,SETDTR) ) {
			errno = EIO;
			ret = -1;
		}
	}
	else {
		if( !EscapeCommFunction(h->handle,CLRDTR) ) {
			errno = EIO;
			ret = -1;
		}
	}

	if( *flags & TIOCM_RTS ) {
		if( !EscapeCommFunction(h->handle,SETRTS) ) {
			errno = EIO;
			ret = -1;
		}
	}
	else {
		if( !EscapeCommFunction(h->handle,CLRRTS) ) {
			errno = EIO;
			ret = -1;
		}
	}

	return ret;
}

int w32_getcomm ( serial_handler_t * h, int * flags )
{
	BOOL ret_val;
	DWORD f;

	ret_val = GetCommModemStatus(h->handle, &f);
	if (ret_val == 0) {
		errno = EIO;
		return -1;
	}

	*flags = f;

	return 0;
}

/* Serial port wrapper inspired by :
http://serial-programming-in-win32-os.blogspot.com/2008/07/convert-linux-code-to-windows-serial.html */

void overlapped_setup (serial_handler_t * sh)
{
	memset (&sh->io_status, 0, sizeof (sh->io_status));
	sh->io_status.hEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
	sh->overlapped_armed = 0;
}

int w32_serial_read (serial_handler_t * sh, void *ptr, size_t ulen, DWORD timeout)
{
	int tot;
	DWORD num;
	HANDLE w4;
	DWORD minchars = sh->vmin_ ? sh->vmin_ : ulen;

	errno = 0;

	w4 = sh->io_status.hEvent;

	upsdebugx(4,
		"w32_serial_read : ulen %" PRIuSIZE ", vmin_ %d, vtime_ %d, hEvent %p",
		ulen, sh->vmin_, sh->vtime_, sh->io_status.hEvent);
	if (!sh->overlapped_armed) {
		SetCommMask (sh->handle, EV_RXCHAR);
		ResetEvent (sh->io_status.hEvent);
	}

	for (num = 0, tot = 0; ulen; ulen -= num, ptr = (char *)ptr + num) {
		DWORD ev;
		COMSTAT st;
		DWORD inq = 1;

		num = 0;

		if (!sh->vtime_ && !sh->vmin_) {
			inq = ulen;
		}
		else if (sh->vtime_) {
			/* non-interruptible -- have to use kernel timeouts
			   also note that this is not strictly correct.
			   if vmin > ulen then things won't work right.
			   sh->overlapped_armed = -1;
			 */
			inq = ulen;
		}

		if (!ClearCommError (sh->handle, &ev, &st)) {
			goto err;
		}
		else if (ev) {
			upsdebugx(4,
				"w32_serial_read : error detected %x",
				(int)ev);
		}
		else if (st.cbInQue) {
			inq = st.cbInQue;
		}
		else if (!sh->overlapped_armed) {
			if ((size_t)tot >= minchars) {
				break;
			}
			else if (WaitCommEvent (sh->handle, &ev, &sh->io_status)) {
				/* WaitCommEvent succeeded */
				if (!ev) {
					continue;
				}
			}
			else if (GetLastError () != ERROR_IO_PENDING) {
				goto err;
			}
			else {
				sh->overlapped_armed = 1;
				switch (WaitForSingleObject (w4, timeout)) {
					case WAIT_OBJECT_0:
						if (!GetOverlappedResult (sh->handle, &sh->io_status, &num, FALSE)) {
							goto err;
						}
						upsdebugx(4,
							"w32_serial_read : characters are available on input buffer");
						break;
					case WAIT_TIMEOUT:
						if(!tot) {
							CancelIo(sh->handle);
							sh->overlapped_armed = 0;
							ResetEvent (sh->io_status.hEvent);
							upsdebugx(4,
								"w32_serial_read : timeout %d ms elapsed",
								(int)timeout);
							SetLastError(WAIT_TIMEOUT);
							errno = 0;
							return 0;
						}
					default:
						goto err;
				}
			}
		}

		sh->overlapped_armed = 0;
		ResetEvent (sh->io_status.hEvent);
		if (inq > ulen) {
			inq = ulen;
		}
		upsdebugx(4,
			"w32_serial_read : Reading %d characters",
			(int)inq);
		if (ReadFile (sh->handle, ptr, min (inq, ulen), &num, &sh->io_status)) {
			/* Got something */;
		}
		else if (GetLastError () != ERROR_IO_PENDING) {
			goto err;
		}
		else if (!GetOverlappedResult (sh->handle, &sh->io_status, &num, TRUE)) {
			goto err;
		}

		tot += num;
		upsdebugx(4,
			"w32_serial_read : total characters read = %d",
			tot);
		if (sh->vtime_ || !sh->vmin_ || !num) {
			break;
		}
		continue;

err:
		PurgeComm (sh->handle, PURGE_RXABORT);
		upsdebugx(4,
			"w32_serial_read : err %d",
			(int)GetLastError());
		if (GetLastError () == ERROR_OPERATION_ABORTED) {
			num = 0;
		}
		else
		{
			errno = EIO;
			tot = -1;
			break;
		}
	}

	return tot;
}

/* Cover function to WriteFile to provide Posix interface and semantics
   (as much as possible).  */
int w32_serial_write (serial_handler_t * sh, const void *ptr, size_t len)
{
	DWORD bytes_written;
	OVERLAPPED write_status;

	errno = 0;

	memset (&write_status, 0, sizeof (write_status));
	write_status.hEvent = CreateEvent (NULL, TRUE, FALSE, NULL);

	for (;;)
	{
		if (WriteFile (sh->handle, ptr, len, &bytes_written, &write_status))
			break;

		switch (GetLastError ())
		{
			case ERROR_OPERATION_ABORTED:
				continue;
			case ERROR_IO_PENDING:
				break;
			default:
				goto err;
		}

		if (!GetOverlappedResult (sh->handle, &write_status, &bytes_written, TRUE))
			goto err;

		break;
	}

	CloseHandle(write_status.hEvent);

	return bytes_written;

err:
	CloseHandle(write_status.hEvent);
	errno = EIO;
	return -1;
}

serial_handler_t * w32_serial_open (const char *name, int flags)
{
	/* flags are currently ignored, it's here just to have the same
	   interface as POSIX open */
	NUT_UNUSED_VARIABLE(flags);
	COMMTIMEOUTS to;

	errno = 0;

	upslogx(LOG_INFO, "w32_serial_open (%s)", name);

	serial_handler_t * sh;

	sh = xmalloc(sizeof(serial_handler_t));
	memset(sh,0,sizeof(serial_handler_t));

	sh->handle = CreateFile(name,
		GENERIC_READ|GENERIC_WRITE,
		0, 0, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		0);

	if(sh->handle == INVALID_HANDLE_VALUE) {
		upslogx(LOG_ERR, "could not open %s", name);
		errno = EPERM;
		return NULL;
	}

	SetCommMask (sh->handle, EV_RXCHAR);

	overlapped_setup (sh);

	memset (&to, 0, sizeof (to));
	SetCommTimeouts (sh->handle, &to);

	/* Reset serial port to known state of 9600-8-1-no flow control
	   on open for better behavior under Win 95.
	 */
	DCB state;
	GetCommState (sh->handle, &state);
	upslogx (LOG_INFO, "setting initial state on %s", name);
	state.BaudRate = CBR_9600;
	state.ByteSize = 8;
	state.StopBits = ONESTOPBIT;
	state.Parity = NOPARITY; /* FIXME: correct default? */
	state.fBinary = TRUE; /* binary xfer */
	state.EofChar = 0; /* no end-of-data in binary mode */
	state.fNull = FALSE; /* don't discard nulls in binary mode */
	state.fParity = FALSE; /* ignore parity errors */
	state.fErrorChar = FALSE;
	state.fTXContinueOnXoff = TRUE; /* separate TX and RX flow control */
	state.fOutX = FALSE; /* disable transmission flow control */
	state.fInX = FALSE; /* disable reception flow control */
	state.XonChar = 0x11;
	state.XoffChar = 0x13;
	state.fOutxDsrFlow = FALSE; /* disable DSR flow control */
	state.fRtsControl = RTS_CONTROL_ENABLE; /* ignore lead control except
						   DTR */
	state.fOutxCtsFlow = FALSE; /* disable output flow control */
	state.fDtrControl = DTR_CONTROL_ENABLE; /* assert DTR */
	state.fDsrSensitivity = FALSE; /* don't assert DSR */
	state.fAbortOnError = TRUE;

	if (!SetCommState (sh->handle, &state)) {
		upslogx (LOG_ERR,
			"couldn't set initial state for %s",
			name);
	}

	SetCommMask (sh->handle, EV_RXCHAR);

	upslogx (LOG_INFO,
		"%p = w32_serial_open (%s)",
		sh->handle, name);
	return sh;
}

int w32_serial_close (serial_handler_t * sh)
{
	if( sh->io_status.hEvent != INVALID_HANDLE_VALUE ) {
		CloseHandle (sh->io_status.hEvent);
	}
	if( sh->handle != INVALID_HANDLE_VALUE ) {
		CloseHandle (sh->handle);
	}
	free(sh);

	errno = 0;

	return 0;
}

/* tcsendbreak: POSIX 7.2.2.1 */
/* Break for 250-500 milliseconds if duration == 0 */
/* Otherwise, units for duration are undefined */
int tcsendbreak (serial_handler_t * sh, int duration)
{
	unsigned int sleeptime = 300000;

	errno = 0;

	if (duration > 0)
		sleeptime *= duration;

	if (SetCommBreak (sh->handle) == 0) {
		errno = EIO;
		return -1;
	}

	/* FIXME: need to send zero bits during duration */
	usleep (sleeptime);

	if (ClearCommBreak (sh->handle) == 0) {
		errno = EIO;
		return -1;
	}

	upslogx(LOG_DEBUG, "0 = tcsendbreak (%d)", duration);

	return 0;
}

/* tcdrain: POSIX 7.2.2.1 */
int tcdrain (serial_handler_t * sh)
{
	errno = 0;

	if (FlushFileBuffers (sh->handle) == 0) {
		errno = EIO;
		return -1;
	}

	return 0;
}

/* tcflow: POSIX 7.2.2.1 */
int tcflow (serial_handler_t * sh, int action)
{
	DWORD win32action = 0;
	DCB dcb;
	char xchar;

	errno = 0;

	upslogx(LOG_DEBUG, "action %d", action);

	switch (action)
	{
		case TCOOFF:
			win32action = SETXOFF;
			break;
		case TCOON:
			win32action = SETXON;
			break;
		case TCION:
		case TCIOFF:
			if (GetCommState (sh->handle, &dcb) == 0)
				return -1;
			if (action == TCION)
				xchar = (dcb.XonChar ? dcb.XonChar : 0x11);
			else
				xchar = (dcb.XoffChar ? dcb.XoffChar : 0x13);
			if (TransmitCommChar (sh->handle, xchar) == 0)
				return -1;
			return 0;
			break;
		default:
			return -1;
			break;
	}

	if (EscapeCommFunction (sh->handle, win32action) == 0) {
		errno = EIO;
		return -1;
	}

	return 0;
}

/* tcflush: POSIX 7.2.2.1 */
int tcflush (serial_handler_t * sh, int queue)
{
	int max;

	errno = 0;

	if (queue == TCOFLUSH || queue == TCIOFLUSH)
		PurgeComm (sh->handle, PURGE_TXABORT | PURGE_TXCLEAR);

	if ((queue == TCIFLUSH) | (queue == TCIOFLUSH))
		/* Input flushing by polling until nothing turns up
		   (we stop after 1000 chars anyway) */
		for (max = 1000; max > 0; max--)
		{
			DWORD ev;
			COMSTAT st;
			if (!PurgeComm (sh->handle, PURGE_RXABORT | PURGE_RXCLEAR))
				break;
			Sleep (100);
			if (!ClearCommError (sh->handle, &ev, &st) || !st.cbInQue)
				break;
		}

	return 0;
}

/* tcsetattr: POSIX 7.2.1.1 */
int tcsetattr (serial_handler_t * sh, int action, const struct termios *t)
{
	/* Possible actions:
TCSANOW:   immediately change attributes.
TCSADRAIN: flush output, then change attributes.
TCSAFLUSH: flush output and discard input, then change attributes.
	 */

	BOOL dropDTR = FALSE;
	COMMTIMEOUTS to;
	DCB ostate, state;
	unsigned int ovtime = sh->vtime_, ovmin = sh->vmin_;

	errno = 0;

	upslogx(LOG_DEBUG, "action %d", action);
	if ((action == TCSADRAIN) || (action == TCSAFLUSH))
	{
		FlushFileBuffers (sh->handle);
		upslogx(LOG_DEBUG, "flushed file buffers");
	}
	if (action == TCSAFLUSH)
		PurgeComm (sh->handle, (PURGE_RXABORT | PURGE_RXCLEAR));

	/* get default/last comm state */
	if (!GetCommState (sh->handle, &ostate)) {
		errno = EIO;
		return -1;
	}

	state = ostate;

	/* -------------- Set baud rate ------------------ */
	/* FIXME: WIN32 also has 14400, 56000, 128000, and 256000.
	   Unix also has 230400. */

	switch (t->c_ospeed)
	{
		case B0: /* drop DTR */
			dropDTR = TRUE;
			state.BaudRate = 0;
			break;
		case B110:
			state.BaudRate = CBR_110;
			break;
		case B300:
			state.BaudRate = CBR_300;
			break;
		case B600:
			state.BaudRate = CBR_600;
			break;
		case B1200:
			state.BaudRate = CBR_1200;
			break;
		case B2400:
			state.BaudRate = CBR_2400;
			break;
		case B4800:
			state.BaudRate = CBR_4800;
			break;
		case B9600:
			state.BaudRate = CBR_9600;
			break;
		case B19200:
			state.BaudRate = CBR_19200;
			break;
		case B38400:
			state.BaudRate = CBR_38400;
			break;
		case B57600:
			state.BaudRate = CBR_57600;
			break;
		case B115200:
			state.BaudRate = CBR_115200;
			break;
		default:
			/* Unsupported baud rate! */
			upslogx(LOG_ERR,
				"Invalid t->c_ospeed %d",
				t->c_ospeed);
			errno = EINVAL;
			return -1;
	}

	/* -------------- Set byte size ------------------ */

	switch (t->c_cflag & CSIZE)
	{
		case CS5:
			state.ByteSize = 5;
			break;
		case CS6:
			state.ByteSize = 6;
			break;
		case CS7:
			state.ByteSize = 7;
			break;
		case CS8:
			state.ByteSize = 8;
			break;
		default:
			/* Unsupported byte size! */
			upslogx(LOG_ERR,
				"Invalid t->c_cflag byte size %d",
				t->c_cflag & CSIZE);
			errno = EINVAL;
			return -1;
	}

	/* -------------- Set stop bits ------------------ */

	if (t->c_cflag & CSTOPB)
		state.StopBits = TWOSTOPBITS;
	else
		state.StopBits = ONESTOPBIT;

	/* -------------- Set parity ------------------ */

	if (t->c_cflag & PARENB)
		state.Parity = (t->c_cflag & PARODD) ? ODDPARITY : EVENPARITY;
	else
		state.Parity = NOPARITY;

	state.fBinary = TRUE;     /* Binary transfer */
	state.EofChar = 0;     /* No end-of-data in binary mode */
	state.fNull = FALSE;      /* Don't discard nulls in binary mode */

	/* -------------- Parity errors ------------------ */
	/* fParity combines the function of INPCK and NOT IGNPAR */

	if ((t->c_iflag & INPCK) && !(t->c_iflag & IGNPAR))
		state.fParity = TRUE;   /* detect parity errors */
	else
		state.fParity = FALSE;  /* ignore parity errors */

	/* Only present in Win32, Unix has no equivalent */
	state.fErrorChar = FALSE;
	state.ErrorChar = 0;

	/* -------------- Set software flow control ------------------ */
	/* Set fTXContinueOnXoff to FALSE.  This prevents the triggering of a
	   premature XON when the remote device interprets a received character
	   as XON (same as IXANY on the remote side).  Otherwise, a TRUE
	   value separates the TX and RX functions. */

	state.fTXContinueOnXoff = TRUE;     /* separate TX and RX flow control */

	/* Transmission flow control */
	if (t->c_iflag & IXON)
		state.fOutX = TRUE;   /* enable */
	else
		state.fOutX = FALSE;  /* disable */

	/* Reception flow control */
	if (t->c_iflag & IXOFF)
		state.fInX = TRUE;    /* enable */
	else
		state.fInX = FALSE;   /* disable */

	/* XoffLim and XonLim are left at default values */

	state.XonChar = (t->c_cc[VSTART] ? t->c_cc[VSTART] : 0x11);
	state.XoffChar = (t->c_cc[VSTOP] ? t->c_cc[VSTOP] : 0x13);

	/* -------------- Set hardware flow control ------------------ */

	/* Disable DSR flow control */
	state.fOutxDsrFlow = FALSE;

	/* Some old flavors of Unix automatically enabled hardware flow
	   control when software flow control was not enabled.  Since newer
	   Unices tend to require explicit setting of hardware flow-control,
	   this is what we do. */

	/* RTS/CTS flow control */
	if (t->c_cflag & CRTSCTS)
	{       /* enable */
		state.fOutxCtsFlow = TRUE;
		state.fRtsControl = RTS_CONTROL_HANDSHAKE;
	}
	else
	{       /* disable */
		state.fRtsControl = RTS_CONTROL_ENABLE;
		state.fOutxCtsFlow = FALSE;
	}

	/*
	   if (t->c_cflag & CRTSXOFF)
	   state.fRtsControl = RTS_CONTROL_HANDSHAKE;
	 */

	/* -------------- DTR ------------------ */
	/* Assert DTR on device open */

	state.fDtrControl = DTR_CONTROL_ENABLE;

	/* -------------- DSR ------------------ */
	/* Assert DSR at the device? */

	if (t->c_cflag & CLOCAL)
		state.fDsrSensitivity = FALSE;  /* no */
	else
		state.fDsrSensitivity = TRUE;   /* yes */

	/* -------------- Error handling ------------------ */
	/* Since read/write operations terminate upon error, we
	   will use ClearCommError() to resume. */

	state.fAbortOnError = TRUE;

	/* -------------- Set state and exit ------------------ */
	if (memcmp (&ostate, &state, sizeof (state)) != 0)
		SetCommState (sh->handle, &state);

	sh->r_binary = ((t->c_iflag & IGNCR) ? 0 : 1);
	sh->w_binary = ((t->c_oflag & ONLCR) ? 0 : 1);

	if (dropDTR == TRUE)
		EscapeCommFunction (sh->handle, CLRDTR);
	else
	{
		/* FIXME: Sometimes when CLRDTR is set, setting
		   state.fDtrControl = DTR_CONTROL_ENABLE will fail.  This
		   is a problem since a program might want to change some
		   parameters while DTR is still down. */

		EscapeCommFunction (sh->handle, SETDTR);
	}

	/*
	   The following documentation on was taken from "Linux Serial Programming
	   HOWTO".  It explains how MIN (t->c_cc[VMIN] || vmin_) and TIME
	   (t->c_cc[VTIME] || vtime_) is to be used.

	   In non-canonical input processing mode, input is not assembled into
	   lines and input processing (erase, kill, delete, etc.) does not
	   occur. Two parameters control the behavior of this mode: c_cc[VTIME]
	   sets the character timer, and c_cc[VMIN] sets the minimum number of
	   characters to receive before satisfying the read.

	   If MIN > 0 and TIME = 0, MIN sets the number of characters to receive
	   before the read is satisfied. As TIME is zero, the timer is not used.

	   If MIN = 0 and TIME > 0, TIME serves as a timeout value. The read will
	   be satisfied if a single character is read, or TIME is exceeded (t =
	   TIME *0.1 s). If TIME is exceeded, no character will be returned.

	   If MIN > 0 and TIME > 0, TIME serves as an inter-character timer. The
	   read will be satisfied if MIN characters are received, or the time
	   between two characters exceeds TIME. The timer is restarted every time
	   a character is received and only becomes active after the first
	   character has been received.

	   If MIN = 0 and TIME = 0, read will be satisfied immediately. The
	   number of characters currently available, or the number of characters
	   requested will be returned. According to Antonino (see contributions),
	   you could issue a fcntl(fd, F_SETFL, FNDELAY); before reading to get
	   the same result.
	 */

	if (t->c_lflag & ICANON)
	{
		sh->vmin_ = MAXDWORD;
		sh->vtime_ = 0;
	}
	else
	{
		sh->vtime_ = t->c_cc[VTIME] * 100;
		sh->vmin_ = t->c_cc[VMIN];
	}

	upslogx(LOG_DEBUG,
		"vtime %d, vmin %d\n",
		sh->vtime_, sh->vmin_);

	if (ovmin == sh->vmin_ && ovtime == sh->vtime_) {
		errno = EINVAL;
		return 0;
	}

	memset (&to, 0, sizeof (to));

	if ((sh->vmin_ > 0) && (sh->vtime_ == 0))
	{
		/* Returns immediately with whatever is in buffer on a ReadFile();
		   or blocks if nothing found.  We will keep calling ReadFile(); until
		   vmin_ characters are read */
		to.ReadIntervalTimeout = to.ReadTotalTimeoutMultiplier = MAXDWORD;
		to.ReadTotalTimeoutConstant = MAXDWORD - 1;
	}
	else if ((sh->vmin_ == 0) && (sh->vtime_ > 0))
	{
		/* set timeoout constant appropriately and we will only try to
		   read one character in ReadFile() */
		to.ReadTotalTimeoutConstant = sh->vtime_;
		to.ReadIntervalTimeout = to.ReadTotalTimeoutMultiplier = MAXDWORD;
	}
	else if ((sh->vmin_ > 0) && (sh->vtime_ > 0))
	{
		/* time applies to the interval time for this case */
		to.ReadIntervalTimeout = sh->vtime_;
	}
	else if ((sh->vmin_ == 0) && (sh->vtime_ == 0))
	{
		/* returns immediately with whatever is in buffer as per
		   Time-Outs docs in Win32 SDK API docs */
		to.ReadIntervalTimeout = MAXDWORD;
	}

	upslogx(LOG_DEBUG,
		"ReadTotalTimeoutConstant %d, "
		"ReadIntervalTimeout %d, "
		"ReadTotalTimeoutMultiplier %d",
		(int)to.ReadTotalTimeoutConstant,
		(int)to.ReadIntervalTimeout,
		(int)to.ReadTotalTimeoutMultiplier);

	int res = SetCommTimeouts(sh->handle, &to);
	if (!res)
	{
		upslogx(LOG_ERR, "SetCommTimeout failed");
		errno = EIO;
		return -1;
	}

	return 0;
}

/* tcgetattr: POSIX 7.2.1.1 */
int tcgetattr (serial_handler_t * sh, struct termios *t)
{
	DCB state;

	errno = 0;

	/* Get current Win32 comm state */
	if (GetCommState (sh->handle, &state) == 0) {
		errno = EIO;
		return -1;
	}

	/* for safety */
	memset (t, 0, sizeof (*t));

	/* -------------- Baud rate ------------------ */

	switch (state.BaudRate)
	{
		case 0:
			/* FIXME: need to drop DTR */
			t->c_cflag = t->c_ospeed = t->c_ispeed = B0;
			break;
		case CBR_110:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B110;
			break;
		case CBR_300:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B300;
			break;
		case CBR_600:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B600;
			break;
		case CBR_1200:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B1200;
			break;
		case CBR_2400:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B2400;
			break;
		case CBR_4800:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B4800;
			break;
		case CBR_9600:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B9600;
			break;
		case CBR_19200:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B19200;
			break;
		case CBR_38400:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B38400;
			break;
		case CBR_57600:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B57600;
			break;
		case CBR_115200:
			t->c_cflag = t->c_ospeed = t->c_ispeed = B115200;
			break;
		default:
			/* Unsupported baud rate! */
			upslogx(LOG_ERR,
				"Invalid baud rate %d",
				(int)state.BaudRate);
			errno = EINVAL;
			return -1;
	}

	/* -------------- Byte size ------------------ */

	switch (state.ByteSize)
	{
		case 5:
			t->c_cflag |= CS5;
			break;
		case 6:
			t->c_cflag |= CS6;
			break;
		case 7:
			t->c_cflag |= CS7;
			break;
		case 8:
			t->c_cflag |= CS8;
			break;
		default:
			/* Unsupported byte size! */
			upslogx(LOG_ERR,
				"Invalid byte size %d",
				state.ByteSize);
			errno = EINVAL;
			return -1;
	}

	/* -------------- Stop bits ------------------ */

	if (state.StopBits == TWOSTOPBITS)
		t->c_cflag |= CSTOPB;

	/* -------------- Parity ------------------ */

	if (state.Parity == ODDPARITY)
		t->c_cflag |= (PARENB | PARODD);
	if (state.Parity == EVENPARITY)
		t->c_cflag |= PARENB;

	/* -------------- Parity errors ------------------ */

	/* fParity combines the function of INPCK and NOT IGNPAR */
	if (state.fParity == TRUE)
		t->c_iflag |= INPCK;
	else
		t->c_iflag |= IGNPAR; /* not necessarily! */

	/* -------------- Software flow control ------------------ */

	/* transmission flow control */
	if (state.fOutX)
		t->c_iflag |= IXON;

	/* reception flow control */
	if (state.fInX)
		t->c_iflag |= IXOFF;

	t->c_cc[VSTART] = (state.XonChar ? state.XonChar : 0x11);
	t->c_cc[VSTOP] = (state.XoffChar ? state.XoffChar : 0x13);

	/* -------------- Hardware flow control ------------------ */
	/* Some old flavors of Unix automatically enabled hardware flow
	   control when software flow control was not enabled.  Since newer
	   Unices tend to require explicit setting of hardware flow-control,
	   this is what we do. */

	/* Input flow-control */
	if ((state.fRtsControl == RTS_CONTROL_HANDSHAKE) &&
			(state.fOutxCtsFlow == TRUE))
		t->c_cflag |= CRTSCTS;
	/*
	   if (state.fRtsControl == RTS_CONTROL_HANDSHAKE)
	   t->c_cflag |= CRTSXOFF;
	 */

	/* -------------- CLOCAL --------------- */
	/* DSR is only lead toggled only by CLOCAL.  Check it to see if
	   CLOCAL was called. */
	/* FIXME: If tcsetattr() hasn't been called previously, this may
	   give a false CLOCAL. */

	if (state.fDsrSensitivity == FALSE)
		t->c_cflag |= CLOCAL;

	/* FIXME: need to handle IGNCR */
#if 0
	if (!sh->r_binary ())
		t->c_iflag |= IGNCR;
#endif

	if (!sh->w_binary)
		t->c_oflag |= ONLCR;

	upslogx (LOG_DEBUG,
		"vmin_ %d, vtime_ %d",
		sh->vmin_, sh->vtime_);
	if (sh->vmin_ == MAXDWORD)
	{
		t->c_lflag |= ICANON;
		t->c_cc[VTIME] = t->c_cc[VMIN] = 0;
	}
	else
	{
		t->c_cc[VTIME] = sh->vtime_ / 100;
		t->c_cc[VMIN] = sh->vmin_;
	}

	return 0;
}

/* FIXME no difference between ispeed and ospeed */
void cfsetispeed(struct termios * t, speed_t speed)
{
	t->c_ispeed = t->c_ospeed = speed;
}
void cfsetospeed(struct termios * t, speed_t speed)
{
	t->c_ispeed = t->c_ospeed = speed;
}
speed_t cfgetispeed(const struct termios *t)
{
	return t->c_ispeed;
}

speed_t cfgetospeed(const struct termios *t)
{
	return t->c_ospeed;
}

#else	/* !WIN32 */

/* Just avoid: ISO C forbids an empty translation unit [-Werror=pedantic] */
int main (int argc, char ** argv);

#endif	/* !WIN32 */
