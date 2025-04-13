/* dstate.c - Network UPS Tools driver-side state management

   Copyright (C)
	2003		Russell Kroll <rkroll@exploits.org>
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>
	2012-2017	Arnaud Quette <arnaud.quette@free.fr>
	2020-2025	Jim Klimov <jimklimov+nut@gmail.com>

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

#include "config.h" /* must be the first header */

#include <stdio.h>
#ifndef WIN32
# include <stdarg.h>
# include <sys/stat.h>
# include <pwd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
#else	/* WIN32 */
# include <strings.h>
# include "wincompat.h"
#endif	/* WIN32 */

#include "common.h"
#include "dstate.h"
#include "state.h"
#include "parseconf.h"
#include "attribute.h"
#include "nut_stdint.h"

	static TYPE_FD	sockfd = ERROR_FD;
#ifndef WIN32
	static char	*sockfn = NULL;
#else	/* WIN32 */
	static OVERLAPPED	connect_overlapped;
	static char	*pipename = NULL;
#endif	/* WIN32 */
	static int	stale = 1, alarm_active = 0, alarm_status = 0, ignorelb = 0;
	static char	status_buf[ST_MAX_VALUE_LEN], alarm_buf[ST_MAX_VALUE_LEN],
			buzzmode_buf[ST_MAX_VALUE_LEN];
	static conn_t	*connhead = NULL;
	static st_tree_t	*dtree_root = NULL;
	static cmdlist_t	*cmdhead = NULL;

	struct ups_handler	upsh;

#ifndef WIN32
/* this may be a frequent stumbling point for new users, so be verbose here */
static void sock_fail(const char *fn)
	__attribute__((noreturn));

static void sock_fail(const char *fn)
{
	int	sockerr;
	struct passwd	*pwuser;

	/* save this so it doesn't get overwritten */
	sockerr = errno;

	/* dispense with the usual upslog stuff since we have stderr here */

	printf("\nFatal error: unable to create listener socket\n\n");
	printf("bind %s failed: %s\n", fn, strerror(sockerr));

	pwuser = getpwuid(getuid());

	if (!pwuser) {
		fatal_with_errno(EXIT_FAILURE, "getpwuid");
	}

	/* deal with some common problems */
	switch (errno)
	{
	case EACCES:
		printf("\nCurrent user: %s (UID %d)\n\n",
			pwuser->pw_name, (int)pwuser->pw_uid);

		printf("Things to try:\n\n");
		printf(" - set different owners or permissions on %s\n\n",
			dflt_statepath());
		printf(" - run this as some other user "
			"(try -u <username>)\n");
		break;

	case ENOENT:
		printf("\nThings to try:\n\n");
		printf(" - mkdir %s\n", dflt_statepath());
		break;

	case ENOTDIR:
		printf("\nThings to try:\n\n");
		printf(" - rm %s\n\n", dflt_statepath());
		printf(" - mkdir %s\n", dflt_statepath());
		break;

	default:
		break;
	}

	/*
	 * there - that wasn't so bad.  every helpful line of code here
	 * prevents one more "help me" mail to the list a year from now
	 */

	printf("\n");
	fatalx(EXIT_FAILURE, "Exiting.");
}
#endif	/* !WIN32 */

static TYPE_FD sock_open(const char *fn)
{
	TYPE_FD	fd;

#ifndef WIN32
	int	ret;
	struct sockaddr_un	ssaddr;

	check_unix_socket_filename(fn);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (INVALID_FD(fd)) {
		fatal_with_errno(EXIT_FAILURE, "Can't create a unix domain socket");
	}

	/* keep this around for the unlink() when exiting */
	sockfn = xstrdup(fn);

	ssaddr.sun_family = AF_UNIX;
	snprintf(ssaddr.sun_path, sizeof(ssaddr.sun_path), "%s", sockfn);

	unlink(sockfn);

	/* group gets access so upsd can be a different user but same group */
	umask(0007);

	ret = bind(fd, (struct sockaddr *) &ssaddr, sizeof ssaddr);

	if (ret < 0) {
		sock_fail(sockfn);
	}

	ret = chmod(sockfn, 0660);

	if (ret < 0) {
		fatal_with_errno(EXIT_FAILURE, "chmod(%s, 0660) failed", sockfn);
	}

	ret = listen(fd, DS_LISTEN_BACKLOG);

	if (ret < 0) {
		fatal_with_errno(EXIT_FAILURE, "listen(%d, %d) failed", fd, DS_LISTEN_BACKLOG);
	}

	if (!getenv("NUT_QUIET_INIT_LISTENER"))
		upslogx(LOG_INFO, "Listening on socket %s", sockfn);

#else /* WIN32 */

	fd = CreateNamedPipe(
			fn,			/* pipe name */
			PIPE_ACCESS_DUPLEX |	/* read/write access */
			FILE_FLAG_OVERLAPPED,	/* async IO */
			PIPE_TYPE_BYTE |
			PIPE_READMODE_BYTE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,	/* max. instances */
			ST_SOCK_BUF_LEN,	/* output buffer size */
			ST_SOCK_BUF_LEN,	/* input buffer size */
			0,			/* client time-out */
			NULL);			/* FIXME: default security attribute */

	if (INVALID_FD(fd)) {
		fatal_with_errno(EXIT_FAILURE,
			"Can't create a state socket (windows named pipe)");
	}

	/* Prepare an async wait on a connection on the pipe */
	memset(&connect_overlapped, 0, sizeof(connect_overlapped));
	connect_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if (connect_overlapped.hEvent == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	/* Wait for a connection */
	ConnectNamedPipe(fd, &connect_overlapped);

	if (!getenv("NUT_QUIET_INIT_LISTENER"))
		upslogx(LOG_INFO, "Listening on named pipe %s", fn);

#endif	/* WIN32 */

	return fd;
}

static void sock_disconnect(conn_t *conn)
{
#ifndef WIN32
	upsdebugx(3, "%s: disconnecting socket %d", __func__, (int)conn->fd);
	close(conn->fd);
#else	/* WIN32 */
	/* FIXME NUT_WIN32_INCOMPLETE not sure if this is the right way to close a connection */
	if (conn->read_overlapped.hEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(conn->read_overlapped.hEvent);
		conn->read_overlapped.hEvent = INVALID_HANDLE_VALUE;
	}
	upsdebugx(3, "%s: disconnecting named pipe handle %p", __func__, conn->fd);
	DisconnectNamedPipe(conn->fd);
#endif	/* WIN32 */

	upsdebugx(5, "%s: finishing parsing context", __func__);
	pconf_finish(&conn->ctx);

	upsdebugx(5, "%s: relinking the chain of connections", __func__);
	if (conn->prev) {
		conn->prev->next = conn->next;
	} else {
		connhead = conn->next;
	}

	if (conn->next) {
		conn->next->prev = conn->prev;
	} else {
		/* conntail = conn->prev; */
	}

	upsdebugx(5, "%s: freeing the conn object", __func__);
	free(conn);
}

static void send_to_all(const char *fmt, ...)
{
	ssize_t	ret;
	char	buf[ST_SOCK_BUF_LEN];
	size_t	buflen;
	va_list	ap;
	conn_t	*conn, *cnext;

	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	if (ret < 1) {
		upsdebugx(2, "%s: nothing to write", __func__);
		return;
	}

	if (ret <= INT_MAX)
		upsdebugx(5, "%s: %.*s", __func__, (int)(ret-1), buf);

	buflen = strlen(buf);
	if (buflen >= SSIZE_MAX) {
		/* Can't compare buflen to ret... though should not happen with ST_SOCK_BUF_LEN */
		upslog_with_errno(LOG_NOTICE, "%s failed: buffered message too large", __func__);
		return;
	}

	for (conn = connhead; conn; conn = cnext) {
		cnext = conn->next;
		if (conn->nobroadcast)
			continue;

#ifndef WIN32
		ret = write(conn->fd, buf, buflen);
#else	/* WIN32 */
		DWORD bytesWritten = 0;
		BOOL  result = FALSE;

		result = WriteFile (conn->fd, buf, buflen, &bytesWritten, NULL);
		if( result == 0 ) {
			upsdebugx(2, "%s: write failed on handle %p, disconnecting", __func__, conn->fd);
			sock_disconnect(conn);
			continue;
		}
		else  {
			ret = (ssize_t)bytesWritten;
		}
#endif	/* WIN32 */

		if ((ret < 1) || (ret != (ssize_t)buflen)) {
#ifndef WIN32
			upsdebug_with_errno(0, "WARNING: %s: write %" PRIuSIZE " bytes to "
				"socket %d failed (ret=%" PRIiSIZE "), disconnecting.",
				__func__, buflen, (int)conn->fd, ret);
#else	/* WIN32 */
			upsdebug_with_errno(0, "WARNING: %s: write %" PRIuSIZE " bytes to "
				"handle %p failed (ret=%" PRIiSIZE "), disconnecting.",
				__func__, buflen, conn->fd, ret);
#endif	/* WIN32 */
			upsdebugx(6, "%s: failed write: %s", __func__, buf);

			sock_disconnect(conn);

			/* TOTHINK: Maybe fallback elsewhere in other cases? */
			if (ret < 0 && errno == EAGAIN && do_synchronous == -1) {
				upsdebugx(0, "%s: synchronous mode was 'auto', "
					"will try 'on' for next connections",
					__func__);
				do_synchronous = 1;
			}

			dstate_setinfo("driver.parameter.synchronous", "%s",
				(do_synchronous==1)?"yes":((do_synchronous==0)?"no":"auto"));
		} else {
			upsdebugx(6, "%s: write %" PRIuSIZE " bytes to socket %d succeeded "
				"(ret=%" PRIiSIZE "): %s",
				__func__, buflen, conn->fd, ret, buf);
		}
	}
}

static int send_to_one(conn_t *conn, const char *fmt, ...)
{
	ssize_t	ret;
	va_list	ap;
	char	buf[ST_SOCK_BUF_LEN];
	size_t	buflen;
#ifdef WIN32
	DWORD bytesWritten = 0;
	BOOL  result = FALSE;
#endif	/* WIN32 */

	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	upsdebugx(2, "%s: sending %.*s", __func__, (int)strcspn(buf, "\n"), buf);
	if (ret < 1) {
		upsdebugx(2, "%s: nothing to write", __func__);
		return 1;
	}

	buflen = strlen(buf);
	if (buflen >= SSIZE_MAX) {
		/* Can't compare buflen to ret... though should not happen with ST_SOCK_BUF_LEN */
		upslog_with_errno(LOG_NOTICE, "%s failed: buffered message too large", __func__);
		return 0;	/* failed */
	}

	if (ret <= INT_MAX)
		upsdebugx(5, "%s: %.*s", __func__, (int)(ret-1), buf);

/*
	upsdebugx(0, "%s: writing %" PRIiSIZE " bytes to socket %d: %s",
		__func__, buflen, conn->fd, buf);
*/

#ifndef WIN32
	ret = write(conn->fd, buf, buflen);
#else	/* WIN32 */
	result = WriteFile (conn->fd, buf, buflen, &bytesWritten, NULL);
	if( result == 0 ) {
		ret = 0;
	}
	else  {
		ret = (ssize_t)bytesWritten;
	}
#endif	/* WIN32 */

	if (ret < 0) {
		/* Hacky bugfix: throttle down for upsd to read that */
#ifndef WIN32
		upsdebug_with_errno(1, "%s: had to throttle down to retry "
			"writing %" PRIuSIZE " bytes to socket %d (ret=%" PRIiSIZE ") : %s",
			__func__, buflen, (int)conn->fd, ret, buf);
#else	/* WIN32 */
		upsdebug_with_errno(1, "%s: had to throttle down to retry "
			"writing %" PRIuSIZE " bytes to handle %p (ret=%" PRIiSIZE ") : %s",
			__func__, buflen, conn->fd, ret, buf);
#endif	/* WIN32 */

		usleep(200);

#ifndef WIN32
		ret = write(conn->fd, buf, buflen);
#else	/* WIN32 */
		result = WriteFile (conn->fd, buf, buflen, &bytesWritten, NULL);
		if( result == 0 ) {
			ret = 0;
		}
		else  {
			ret = (ssize_t)bytesWritten;
		}
#endif	/* WIN32 */
		if (ret == (ssize_t)buflen) {
			upsdebugx(1, "%s: throttling down helped", __func__);
		}
	}

	if ((ret < 1) || (ret != (ssize_t)buflen)) {
#ifndef WIN32
		upsdebug_with_errno(0, "WARNING: %s: write %" PRIuSIZE " bytes to "
			"socket %d failed (ret=%" PRIiSIZE "), disconnecting.",
			__func__, buflen, (int)conn->fd, ret);
#else	/* WIN32 */
		upsdebug_with_errno(0, "WARNING: %s: write %" PRIuSIZE " bytes to "
			"handle %p failed (ret=%" PRIiSIZE "), disconnecting.",
			__func__, buflen, conn->fd, ret);
#endif	/* WIN32 */
		upsdebugx(6, "%s: failed write: %s", __func__, buf);
		sock_disconnect(conn);

		/* TOTHINK: Maybe fallback elsewhere in other cases? */
		if (ret < 0 && errno == EAGAIN && do_synchronous == -1) {
			upsdebugx(0, "%s: synchronous mode was 'auto', "
				"will try 'on' for next connections",
				__func__);
			do_synchronous = 1;
		}

		dstate_setinfo("driver.parameter.synchronous", "%s",
			(do_synchronous==1)?"yes":((do_synchronous==0)?"no":"auto"));

		return 0;	/* failed */
	} else {
#ifndef WIN32
		upsdebugx(6, "%s: write %" PRIuSIZE " bytes to socket %d succeeded "
			"(ret=%" PRIiSIZE "): %s",
			__func__, buflen, conn->fd, ret, buf);
#else	/* WIN32 */
		upsdebugx(6, "%s: write %" PRIuSIZE " bytes to handle %p succeeded "
			"(ret=%" PRIiSIZE "): %s",
			__func__, buflen, conn->fd, ret, buf);
#endif	/* WIN32 */
	}

	return 1;	/* OK */
}

static void sock_connect(TYPE_FD sock)
{
	conn_t	*conn;

#ifndef WIN32
	int	ret;
	int	fd;

	struct sockaddr_un sa;
# if defined(__hpux) && !defined(_XOPEN_SOURCE_EXTENDED)
	int	salen;
# else
	socklen_t	salen;
# endif
	salen = sizeof(sa);
	fd = accept(sock, (struct sockaddr *) &sa, &salen);

	if (INVALID_FD(fd)) {
		upslog_with_errno(LOG_ERR, "%s: accept on unix fd failed", __func__);
		return;
	}

	/* enable nonblocking I/O?
	 * -1 = auto (try async, allow fallback to sync)
	 *  0 = async
	 *  1 = sync
	 */
	if (do_synchronous < 1) {
		upsdebugx(0, "%s: enabling asynchronous mode (%s)",
			__func__, (do_synchronous<0)?"auto":"fixed");

		ret = fcntl(fd, F_GETFL, 0);

		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "%s: fcntl get on unix fd failed", __func__);
			close(fd);
			return;
		}

		ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "%s: fcntl set O_NDELAY on unix fd failed", __func__);
			close(fd);
			return;
		}
	}
	else {
		upsdebugx(0, "%s: keeping default synchronous mode", __func__);
	}

	conn = (conn_t *)xcalloc(1, sizeof(*conn));
	conn->fd = fd;

#else /* WIN32 */

	/* We have detected a connection on the opened pipe.
	 * So we start by saving its handle and creating
	 * a new pipe for future connection */
	conn = xcalloc(1, sizeof(*conn));
	conn->fd = sock;

	/* sockfd is the handle of the connection pending pipe */
	sockfd = CreateNamedPipe(
			pipename,		/* pipe name */
			PIPE_ACCESS_DUPLEX |	/* read/write access */
			FILE_FLAG_OVERLAPPED,	/* async IO */
			PIPE_TYPE_BYTE |
			PIPE_READMODE_BYTE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,	/* max. instances */
			ST_SOCK_BUF_LEN,	/* output buffer size */
			ST_SOCK_BUF_LEN,	/* input buffer size */
			0,			/* client time-out */
			NULL);			/* FIXME: default security attribute */

	if (INVALID_FD(sockfd)) {
		fatal_with_errno(EXIT_FAILURE,
			"Can't create a state socket (windows named pipe)");
	}

	/* Prepare a new async wait for a connection on the pipe */
	CloseHandle(connect_overlapped.hEvent);
	memset(&connect_overlapped,0,sizeof(connect_overlapped));
	connect_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if(connect_overlapped.hEvent == NULL ) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	/* Wait for a connection */
	ConnectNamedPipe(sockfd,&connect_overlapped);

	/* A new pipe waiting for new client connection has been created. We could manage the current connection now */
	/* Start a read operation on the newly connected pipe so we could wait on the event associated to this IO */
	memset(&conn->read_overlapped,0,sizeof(conn->read_overlapped));
	memset(conn->buf,0,sizeof(conn->buf));
	conn->read_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if(conn->read_overlapped.hEvent == NULL ) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	ReadFile (conn->fd, conn->buf,
		sizeof(conn->buf) - 1, /* -1 to be sure to have a trailling 0 */
		NULL, &(conn->read_overlapped));
#endif	/* WIN32 */

	conn->nobroadcast = 0;
	conn->readzero = 0;
	conn->closing = 0;
	pconf_init(&conn->ctx, NULL);

	if (connhead) {
		conn->next = connhead;
		connhead->prev = conn;
	}

	connhead = conn;

#ifndef WIN32
	upsdebugx(3, "%s: new connection on fd %d", __func__, fd);
#else	/* WIN32 */
	upsdebugx(3, "%s: new connection on handle %p", __func__, sock);
#endif	/* WIN32 */

}

static int st_tree_dump_conn_one_node(st_tree_t *node, conn_t *conn)
{
	enum_t	*etmp;
	range_t	*rtmp;

	if (!send_to_one(conn, "SETINFO %s \"%s\"\n", node->var, node->val)) {
		return 0;	/* write failed, bail out */
	}

	/* send any enums */
	for (etmp = node->enum_list; etmp; etmp = etmp->next) {
		if (!send_to_one(conn, "ADDENUM %s \"%s\"\n", node->var, etmp->val)) {
			return 0;
		}
	}

	/* send any ranges */
	for (rtmp = node->range_list; rtmp; rtmp = rtmp->next) {
		if (!send_to_one(conn, "ADDRANGE %s %i %i\n", node->var, rtmp->min, rtmp->max)) {
			return 0;
		}
	}

	/* provide any auxiliary data */
	if (node->aux) {
		if (!send_to_one(conn, "SETAUX %s %ld\n", node->var, node->aux)) {
			return 0;
		}
	}

	/* finally report any flags */
	if (node->flags) {
		char	flist[SMALLBUF];

		/* build the list */
		snprintf(flist, sizeof(flist), "%s", node->var);

		if (node->flags & ST_FLAG_RW) {
			snprintfcat(flist, sizeof(flist), " RW");
		}
		if (node->flags & ST_FLAG_STRING) {
			snprintfcat(flist, sizeof(flist), " STRING");
		}
		if (node->flags & ST_FLAG_NUMBER) {
			snprintfcat(flist, sizeof(flist), " NUMBER");
		}

		if (!send_to_one(conn, "SETFLAGS %s\n", flist)) {
			return 0;
		}
	}

	return 1;	/* everything's OK here ... */
}

static int st_tree_dump_conn(st_tree_t *node, conn_t *conn)
{
	int	ret;

	if (!node) {
		return 1;	/* not an error */
	}

	if (node->left) {
		ret = st_tree_dump_conn(node->left, conn);

		if (!ret) {
			return 0;	/* write failed in the child */
		}
	}

	if (!st_tree_dump_conn_one_node(node, conn))
		return 0;	/* one of writes failed, bail out */

	if (node->right) {
		return st_tree_dump_conn(node->right, conn);
	}

	return 1;	/* everything's OK here ... */
}

static int cmd_dump_conn(conn_t *conn)
{
	cmdlist_t	*cmd;

	for (cmd = cmdhead; cmd; cmd = cmd->next) {
		if (!send_to_one(conn, "ADDCMD %s\n", cmd->name)) {
			return 0;
		}
	}

	return 1;
}


static void send_tracking(conn_t *conn, const char *id, int value)
{
	send_to_one(conn, "TRACKING %s %i\n", id, value);
}

static int sock_arg(conn_t *conn, size_t numarg, char **arg)
{
#ifdef WIN32
	char *sockfn = pipename;	/* Just for the report below; not a global var in WIN32 builds */
#endif	/* WIN32 */

	upsdebugx(6, "%s: Driver on %s is now handling %s with %" PRIuSIZE " args",
		__func__, sockfn, numarg ? arg[0] : "<skipped: no command>", numarg);

	if (numarg < 1) {
		return 0;
	}

	if (!strcasecmp(arg[0], "LOGOUT")) {
		send_to_one(conn, "OK Goodbye\n");
#ifndef WIN32
		upsdebugx(2, "%s: received LOGOUT on socket %d, will be disconnecting", __func__, (int)conn->fd);
#else	/* WIN32 */
		upsdebugx(2, "%s: received LOGOUT on handle %p, will be disconnecting", __func__, conn->fd);
#endif	/* WIN32 */
		/* Let the system flush the reply somehow (or the other
		 * side to just see it) before we drop the pipe */
		usleep(1000000);
		/* err on the safe side, and actually close/free conn separately */
		conn->closing = 1;
		upsdebugx(4, "%s: LOGOUT processing finished", __func__);
		return 2;
	}

	if (!strcasecmp(arg[0], "GETPID")) {
		send_to_one(conn, "PID %" PRIiMAX "\n", (intmax_t)getpid());
		return 1;
	}

	if (!strcasecmp(arg[0], "DUMPALL") || !strcasecmp(arg[0], "DUMPSTATUS") || (!strcasecmp(arg[0], "DUMPVALUE") && numarg > 1)) {
		/* first thing: the staleness flag (see also below) */
		if ((stale == 1) && !send_to_one(conn, "DATASTALE\n")) {
			return 1;
		}

		if (!strcasecmp(arg[0], "DUMPALL")) {
			if (!st_tree_dump_conn(dtree_root, conn)) {
				return 1;
			}

			if (!cmd_dump_conn(conn)) {
				return 1;
			}
		} else {
			/* A cheaper version of the dump */
			char	*varname = (!strcasecmp(arg[0], "DUMPSTATUS") ? "ups.status" : (numarg > 1 ? arg[1] : NULL));
			st_tree_t	*sttmp = (varname ? state_tree_find(dtree_root, varname) : NULL);

			if (!sttmp) {
				upsdebugx(1, "%s: %s was requested but currently no %s is known",
					__func__, arg[0], NUT_STRARG(varname));
			} else {
				if (!st_tree_dump_conn_one_node(sttmp, conn))
					return 1;
			}
		}

		if ((stale == 0) && !send_to_one(conn, "DATAOK\n")) {
			return 1;
		}

		send_to_one(conn, "DUMPDONE\n");
		return 1;
	}

	if (!strcasecmp(arg[0], "PING")) {
		send_to_one(conn, "PONG\n");
		return 1;
	}

	if (!strcasecmp(arg[0], "NOBROADCAST")) {
		char buf[SMALLBUF];
		conn->nobroadcast = 1;
#ifndef WIN32
		snprintf(buf, sizeof(buf), "socket %d", conn->fd);
#else	/* WIN32 */
		snprintf(buf, sizeof(buf), "handle %p", conn->fd);
#endif	/* WIN32 */
		upsdebugx(1, "%s: %s requested NOBROADCAST mode",
			__func__, buf);
		return 1;
	}

	/* BROADCAST <0|1> */
	if (!strcasecmp(arg[0], "BROADCAST")) {
		int i;
		char buf[SMALLBUF];
		conn->nobroadcast = 0;
		if (numarg > 1 && str_to_int(arg[1], &i, 10)) {
			if (i < 1)
				conn->nobroadcast = 1;
		}
#ifndef WIN32
		snprintf(buf, sizeof(buf), "socket %d", conn->fd);
#else	/* WIN32 */
		snprintf(buf, sizeof(buf), "handle %p", conn->fd);
#endif	/* WIN32 */
		upsdebugx(1,
			"%s: %s requested %sBROADCAST mode",
			__func__, buf,
			conn->nobroadcast ? "NO" : "");
		return 1;
	}

	if (numarg < 2) {
		return 0;
	}

	/* INSTCMD <cmdname> [<cmdparam>] [TRACKING <id>] */
	if (!strcasecmp(arg[0], "INSTCMD")) {
		int ret;
		char *cmdname = arg[1];
		char *cmdparam = NULL;
		char *cmdid = NULL;

		/* Check if <cmdparam> and/or TRACKING were provided */
		if (numarg == 3) {
			cmdparam = arg[2];
		} else if (numarg == 4 && !strcasecmp(arg[2], "TRACKING")) {
			cmdid = arg[3];
		} else if (numarg == 5 && !strcasecmp(arg[3], "TRACKING")) {
			cmdparam = arg[2];
			cmdid = arg[4];
		} else if (numarg != 2) {
			upslogx(LOG_NOTICE, "Malformed INSTCMD request");
			return 0;
		}

		if (cmdid)
			upsdebugx(3, "%s: TRACKING = %s", __func__, cmdid);

		/* try the handler shared by all drivers first */
		ret = main_instcmd(cmdname, cmdparam, conn);
		if (ret != STAT_INSTCMD_UNKNOWN) {
			/* The command was acknowledged by shared handler, and
			 * either handled successfully, or failed, or was not
			 * valid in current circumstances - in any case, we do
			 * not pass to driver-provided logic. */

			/* send back execution result if requested */
			if (cmdid)
				send_tracking(conn, cmdid, ret);

			/* The command was handled, status is a separate consideration */
			return 1;
		} /* else try other handler(s) */

		/* try the driver-provided handler if present */
		if (upsh.instcmd) {
			ret = upsh.instcmd(cmdname, cmdparam);

			/* send back execution result if requested */
			if (cmdid)
				send_tracking(conn, cmdid, ret);

			/* The command was handled, status is a separate consideration */
			return 1;
		}

		if (cmdparam) {
			upslogx(LOG_NOTICE,
				"Got INSTCMD '%s' '%s', but driver lacks a handler",
				NUT_STRARG(cmdname), NUT_STRARG(cmdparam));
		} else {
			upslogx(LOG_NOTICE,
				"Got INSTCMD '%s', but driver lacks a handler",
				NUT_STRARG(cmdname));
		}

		/* Send back execution result (here, STAT_INSTCMD_UNKNOWN)
		 * if TRACKING was requested.
		 * Note that in practice we should not get here often: if the
		 * instcmd was not registered, it may be rejected earlier in
		 * call stack, or returned by a driver's handler (for unknown
		 * commands) just a bit above.
		 */
		if (cmdid)
			send_tracking(conn, cmdid, ret);

		/* The command was handled, status is a separate consideration */
		return 1;
	}

	if (numarg < 3) {
		return 0;
	}

	/* SET <var> <value> [TRACKING <id>] */
	if (!strcasecmp(arg[0], "SET")) {
		int ret;
		char *setid = NULL;

		/* Check if TRACKING was provided */
		if (numarg == 5) {
			if (!strcasecmp(arg[3], "TRACKING")) {
				setid = arg[4];
			}
			else {
				upslogx(LOG_NOTICE, "Got SET <var> with unsupported parameters (%s/%s)",
					arg[3], arg[4]);
				return 0;
			}
			upsdebugx(3, "%s: TRACKING = %s", __func__, setid);
		}

		/* try the handler shared by all drivers first */
		ret = main_setvar(arg[1], arg[2], conn);
		if (ret != STAT_SET_UNKNOWN) {
			/* The command was acknowledged by shared handler, and
			 * either handled successfully, or failed, or was not
			 * valid in current circumstances - in any case, we do
			 * not pass to driver-provided logic. */

			/* send back execution result if requested */
			if (setid)
				send_tracking(conn, setid, ret);

			/* The command was handled, status is a separate consideration */
			return 1;
		} /* else try other handler(s) */

		/* try the driver-provided handler if present */
		if (upsh.setvar) {
			ret = upsh.setvar(arg[1], arg[2]);

			/* send back execution result if requested */
			if (setid)
				send_tracking(conn, setid, ret);

			/* The command was handled, status is a separate consideration */
			return 1;
		}

		upslogx(LOG_NOTICE, "Got SET, but driver lacks a handler");
		return 1;
	}

	/* unknown */
	return 0;
}

static void sock_read(conn_t *conn)
{
	ssize_t	ret, i;
	int	ret_arg = -1;

#ifndef WIN32
	char	buf[SMALLBUF];

	ret = read(conn->fd, buf, sizeof(buf));

	if (ret < 0) {
		switch(errno)
		{
		case EINTR:
		case EAGAIN:
			return;

		default:
			sock_disconnect(conn);
			return;
		}
	}

	if (ret == 0) {
		int	flags = fcntl(conn->fd, F_GETFL), is_closed = 0;
		upsdebugx(2, "%s: read() returned 0; flags=%04X O_NDELAY=%04X",
			__func__, (unsigned int)flags, (unsigned int)O_NDELAY);
		if (flags & O_NDELAY || O_NDELAY == 0) {
			/* O_NDELAY with zero bytes means nothing to read but
			 * since read() follows a successful select() with
			 * ready file descriptor, ret shouldn't be 0.
			 * This may also mean that the counterpart has exited
			 * and the file descriptor should be reaped.
			 * e.g. a `driver -c reload -a testups` fires its
			 * message over Unix socket and disconnects.
			 */
			is_closed = 1;
		} else {
			/* assume we will soon have data waiting in the buffer */
			conn->readzero++;
			upsdebugx(1, "%s: got zero-sized reads %d times in a row", __func__, conn->readzero);
			if (conn->readzero > DSTATE_CONN_READZERO_THROTTLE_MAX) {
				is_closed = 2;
			} else {
				usleep(DSTATE_CONN_READZERO_THROTTLE_USEC);
			}
		}

		if (is_closed) {
			upsdebugx(1, "%s: it seems the other side has closed the connection", __func__);
			sock_disconnect(conn);
			return;
		}
	} else {
		conn->readzero = 0;
	}
#else	/* WIN32 */
	char *buf = conn->buf;
	DWORD bytesRead;
	BOOL res;
	res = GetOverlappedResult(conn->fd, &conn->read_overlapped, &bytesRead, FALSE);
	if( res == 0 ) {
		upslogx(LOG_INFO, "Read error : %d",(int)GetLastError());
		sock_disconnect(conn);
		return;
	}
	ret = bytesRead;

	/* Special case for signals */
	if (!strncmp(conn->buf, COMMAND_STOP, sizeof(COMMAND_STOP))) {
		set_exit_flag(1);
		return;
	}
#endif	/* WIN32 */

	for (i = 0; i < ret; i++) {

		switch(pconf_char(&conn->ctx, buf[i]))
		{
		case 0: /* nothing to parse yet */
			continue;

		case 1: /* try to use it, and complain about unknown commands */
			ret_arg = sock_arg(conn, conn->ctx.numargs, conn->ctx.arglist);
			if (!ret_arg) {
				size_t	arg;

				upslogx(LOG_INFO, "Unknown command on socket: ");

				for (arg = 0; arg < conn->ctx.numargs && arg < INT_MAX; arg++) {
					upslogx(LOG_INFO, "arg %d: %s", (int)arg, conn->ctx.arglist[arg]);
				}
			} else if (ret_arg == 2) {
				/* closed by LOGOUT processing, conn is free()'d */
				if (i < ret)
					upsdebugx(1, "%s: returning early, socket may be not valid anymore", __func__);
				return;
			}

			continue;

		default: /* nothing parsed */
			upslogx(LOG_NOTICE, "Parse error on sock: %s", conn->ctx.errmsg);
			return;
		}
	}

#ifdef WIN32
	/* Restart async read */
	memset(conn->buf,0,sizeof(conn->buf));
	ReadFile(conn->fd,conn->buf,sizeof(conn->buf)-1,NULL,&(conn->read_overlapped)); /* -1 to be sure to have a trailling 0 */
#endif	/* WIN32 */
}

static void sock_close(void)
{
	conn_t	*conn, *cnext;

	if (VALID_FD(sockfd)) {
#ifndef WIN32
		close(sockfd);

		if (sockfn) {
			unlink(sockfn);
			free(sockfn);
			sockfn = NULL;
		}
#else	/* WIN32 */
		FlushFileBuffers(sockfd);
		CloseHandle(sockfd);
#endif	/* WIN32 */

		sockfd = ERROR_FD;
	}

	for (conn = connhead; conn; conn = cnext) {
		cnext = conn->next;
		sock_disconnect(conn);
	}

	connhead = NULL;
	/* conntail = NULL; */
}

/* interface */

char * dstate_init(const char *prog, const char *devname)
{
	char	sockname[NUT_PATH_MAX + 1];

#ifndef WIN32
	/* do this here for now */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
	signal(SIGPIPE, SIG_IGN);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic pop
#endif

	if (devname) {
		snprintf(sockname, sizeof(sockname), "%s/%s-%s", dflt_statepath(), prog, devname);
	} else {
		snprintf(sockname, sizeof(sockname), "%s/%s", dflt_statepath(), prog);
	}
#else	/* WIN32 */
	/* upsname (and so devname) is now mandatory so no need to test it */
	snprintf(sockname, sizeof(sockname), "\\\\.\\pipe\\%s-%s", prog, devname);
	pipename = xstrdup(sockname);
#endif	/* WIN32 */

	sockfd = sock_open(sockname);

#ifndef WIN32
	upsdebugx(2, "%s: sock %s open on fd %d", __func__, sockname, sockfd);
#else	/* WIN32 */
	upsdebugx(2, "%s: sock %s open on handle %p", __func__, sockname, sockfd);
#endif	/* WIN32 */

	/* NOTE: Caller must free this string */
	return xstrdup(sockname);
}

/* returns 1 if timeout expired or data is available on UPS fd, 0 otherwise */
int dstate_poll_fds(struct timeval timeout, TYPE_FD arg_extrafd)
{
	int	maxfd = 0; /* Unidiomatic use vs. "sockfd" below, which is "int" on non-WIN32 */
	int	overrun = 0;
	conn_t	*conn, *cnext;
	struct timeval	now;

#ifndef WIN32
	int	ret;
	fd_set	rfds;

	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);

	maxfd = sockfd;

	if (VALID_FD(arg_extrafd)) {
		FD_SET(arg_extrafd, &rfds);

		if (arg_extrafd > maxfd) {
			maxfd = arg_extrafd;
		}
	}

	for (conn = connhead; conn; conn = conn->next) {
		FD_SET(conn->fd, &rfds);

		if (conn->fd > maxfd) {
			maxfd = conn->fd;
		}
	}

	gettimeofday(&now, NULL);

	/* number of microseconds should always be positive */
	if (timeout.tv_usec < now.tv_usec) {
		timeout.tv_sec -= 1;
		timeout.tv_usec += 1000000;
	}

	if (timeout.tv_sec < now.tv_sec) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		overrun = 1;	/* no time left */
	} else {
		timeout.tv_sec -= now.tv_sec;
		timeout.tv_usec -= now.tv_usec;
	}

	ret = select(maxfd + 1, &rfds, NULL, NULL, &timeout);

	if (ret == 0) {
		return 1;	/* timer expired */
	}

	if (ret < 0) {
		switch (errno)
		{
		case EINTR:
		case EAGAIN:
			/* ignore interruptions from signals */
			break;

		default:
			upslog_with_errno(LOG_ERR, "%s: select unix sockets failed", __func__);
		}

		return overrun;
	}

	if (FD_ISSET(sockfd, &rfds)) {
		sock_connect(sockfd);
	}

	for (conn = connhead; conn; conn = cnext) {
		cnext = conn->next;

		if (FD_ISSET(conn->fd, &rfds)) {
			sock_read(conn);
		}
	}

	for (conn = connhead; conn; conn = cnext) {
		cnext = conn->next;

		if (conn->closing) {
			sock_disconnect(conn);
		}
	}

	/* tell the caller if that fd woke up */
	if (VALID_FD(arg_extrafd) && (FD_ISSET(arg_extrafd, &rfds))) {
		return 1;
	}

#else /* WIN32 */

	DWORD	ret;
	HANDLE	rfds[32];
	DWORD	timeout_ms;

	/* FIXME: Should such table (and limit) be used in reality? */
	NUT_UNUSED_VARIABLE(arg_extrafd);
/*
	if (VALID_FD(arg_extrafd)) {
		rfds[maxfd] = arg_extrafd;
		maxfd++;
	}
*/

	gettimeofday(&now, NULL);

	/* number of microseconds should always be positive */
	if (timeout.tv_usec < now.tv_usec) {
		timeout.tv_sec -= 1;
		timeout.tv_usec += 1000000;
	}

	if (timeout.tv_sec < now.tv_sec) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		overrun = 1;	/* no time left */
	} else {
		timeout.tv_sec -= now.tv_sec;
		timeout.tv_usec -= now.tv_usec;
	}

	timeout_ms = (timeout.tv_sec * 1000) + (timeout.tv_usec / 1000);

	/* Wait on the read IO of each connections */
	for (conn = connhead; conn; conn = conn->next) {
		rfds[maxfd] = conn->read_overlapped.hEvent;
		maxfd++;
	}
	/* Add the connect event */
	rfds[maxfd] = connect_overlapped.hEvent;
	maxfd++;

	ret = WaitForMultipleObjects(
				maxfd,	/* number of objects in array */
				rfds,	/* array of objects */
				FALSE,	/* wait for any object */
				timeout_ms); /* timeout in millisecond */

	if (ret == WAIT_TIMEOUT) {
		return 1;	/* timer expired */
	}

	if (ret == WAIT_FAILED) {
		upslog_with_errno(LOG_ERR, "%s: waitfor failed", __func__);
		return overrun;
	}

	/* Retrieve the signaled connection */
	for (conn = connhead; conn != NULL; conn = conn->next) {
		if( conn->read_overlapped.hEvent == rfds[ret-WAIT_OBJECT_0]) {
			break;
		}
	}

	/* the connection event handle has been signaled */
	if (rfds[ret] == connect_overlapped.hEvent) {
		sock_connect(sockfd);
	}
	/* one of the read event handle has been signaled */
	else {
		if (conn != NULL) {
			sock_read(conn);
		}
	}

	for (conn = connhead; conn; conn = cnext) {
		cnext = conn->next;

		if (conn->closing) {
			sock_disconnect(conn);
		}
	}

	/* tell the caller if that fd woke up */
/*
	if (VALID_FD(arg_extrafd) && (ret == arg_extrafd)) {
		return 1;
	}
*/
#endif	/* WIN32 */

	return overrun;
}

/******************************************************************
 * COMMON
 ******************************************************************/

int dstate_setinfo(const char *var, const char *fmt, ...)
{
	int	ret;
	char	value[ST_MAX_VALUE_LEN];
	va_list	ap;

	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vsnprintf(value, sizeof(value), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	ret = state_setinfo(&dtree_root, var, value);

	if (ret == 1) {
		send_to_all("SETINFO %s \"%s\"\n", var, value);
	}

	return ret;
}

int dstate_addenum(const char *var, const char *fmt, ...)
{
	int	ret;
	char	value[ST_MAX_VALUE_LEN];
	va_list	ap;

	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vsnprintf(value, sizeof(value), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	ret = state_addenum(dtree_root, var, value);

	if (ret == 1) {
		send_to_all("ADDENUM %s \"%s\"\n", var, value);
	}

	return ret;
}

int dstate_addrange(const char *var, const int min, const int max)
{
	int	ret;

	ret = state_addrange(dtree_root, var, min, max);

	if (ret == 1) {
		send_to_all("ADDRANGE %s %i %i\n", var, min, max);
		/* Also add the "NUMBER" flag for ranges */
		dstate_addflags(var, ST_FLAG_NUMBER);
	}

	return ret;
}

void dstate_setflags(const char *var, int flags)
{
	st_tree_t	*sttmp;
	char	flist[SMALLBUF];

	/* find the dtree node for var */
	sttmp = state_tree_find(dtree_root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "%s: base variable (%s) does not exist", __func__, var);
		return;
	}

	if (sttmp->flags & ST_FLAG_IMMUTABLE) {
		upslogx(LOG_WARNING, "%s: base variable (%s) is immutable", __func__, var);
		return;
	}

	if (sttmp->flags == flags) {
		return;		/* no change */
	}

	sttmp->flags = flags;

	/* build the list */
	snprintf(flist, sizeof(flist), "%s", var);

	if (flags & ST_FLAG_RW) {
		snprintfcat(flist, sizeof(flist), " RW");
	}

	if (flags & ST_FLAG_STRING) {
		snprintfcat(flist, sizeof(flist), " STRING");
	}

	if (flags & ST_FLAG_NUMBER) {
		snprintfcat(flist, sizeof(flist), " NUMBER");
	}

	/* update listeners */
	send_to_all("SETFLAGS %s\n", flist);
}

void dstate_addflags(const char *var, const int addflags)
{
	int	flags = state_getflags(dtree_root, var);

	if (flags == -1) {
		upslogx(LOG_ERR, "%s: cannot get flags of '%s'", __func__, var);
		return;
	}

	/* Already set */
	if ((flags & addflags) == addflags)
		return;

	flags |= addflags;

	dstate_setflags(var, flags);
}

void dstate_delflags(const char *var, const int delflags)
{
	int	flags = state_getflags(dtree_root, var);

	if (flags == -1) {
		upslogx(LOG_ERR, "%s: cannot get flags of '%s'", __func__, var);
		return;
	}

	/* Already not set */
	if (!(flags & delflags))
		return;

	flags &= ~delflags;

	dstate_setflags(var, flags);
}

void dstate_setaux(const char *var, long aux)
{
	st_tree_t	*sttmp;

	/* find the dtree node for var */
	sttmp = state_tree_find(dtree_root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "%s: base variable (%s) does not exist", __func__, var);
		return;
	}

	if (sttmp->aux == aux) {
		return;		/* no change */
	}

	sttmp->aux = aux;

	/* update listeners */
	send_to_all("SETAUX %s %ld\n", var, aux);
}

const char *dstate_getinfo(const char *var)
{
	return state_getinfo(dtree_root, var);
}

void dstate_addcmd(const char *cmdname)
{
	int	ret;

	ret = state_addcmd(&cmdhead, cmdname);

	/* update listeners */
	if (ret == 1) {
		send_to_all("ADDCMD %s\n", cmdname);
	}
}

int dstate_delinfo(const char *var)
{
	int	ret;

	ret = state_delinfo(&dtree_root, var);

	/* update listeners */
	if (ret == 1) {
		send_to_all("DELINFO %s\n", var);
	}

	return ret;
}

int dstate_delinfo_olderthan(const char *var, const st_tree_timespec_t *cutoff)
{
	int	ret;

	ret = state_delinfo_olderthan(&dtree_root, var, cutoff);

	/* update listeners */
	if (ret == 1) {
		send_to_all("DELINFO %s\n", var);
	}

	return ret;
}

int dstate_delenum(const char *var, const char *val)
{
	int	ret;

	ret = state_delenum(dtree_root, var, val);

	/* update listeners */
	if (ret == 1) {
		send_to_all("DELENUM %s \"%s\"\n", var, val);
	}

	return ret;
}

int dstate_delrange(const char *var, const int min, const int max)
{
	int	ret;

	ret = state_delrange(dtree_root, var, min, max);

	/* update listeners */
	if (ret == 1) {
		send_to_all("DELRANGE %s %i %i\n", var, min, max);
	}

	return ret;
}

int dstate_delcmd(const char *cmd)
{
	int	ret;

	ret = state_delcmd(&cmdhead, cmd);

	/* update listeners */
	if (ret == 1) {
		send_to_all("DELCMD %s\n", cmd);
	}

	return ret;
}

void dstate_free(void)
{
	state_infofree(dtree_root);
	dtree_root = NULL;

	state_cmdfree(cmdhead);
	cmdhead = NULL;

	sock_close();
}

const st_tree_t *dstate_getroot(void)
{
	return dtree_root;
}

const cmdlist_t *dstate_getcmdlist(void)
{
	return cmdhead;
}

void dstate_dataok(void)
{
	if (stale == 1) {
		stale = 0;
		send_to_all("DATAOK\n");
	}
}

void dstate_datastale(void)
{
	if (stale == 0) {
		stale = 1;
		send_to_all("DATASTALE\n");
	}
}

int dstate_is_stale(void)
{
	return stale;
}

/* ups.status management functions - reducing duplication in the drivers */

/* clean out the temp space for a new pass */
void status_init(void)
{
	if (dstate_getinfo("driver.flag.ignorelb")) {
		ignorelb = 1;
	}

	memset(status_buf, 0, sizeof(status_buf));
	alarm_status = 0;
}

/* check if a status element has been set, return 0 if not, 1 if yes
 * (considering a whole-word token in temporary status_buf) */
int status_get(const char *buf)
{
	return str_contains_token(status_buf, buf);
}

/* add a status element */
static int status_set_callback(char *tgt, size_t tgtsize, const char *token)
{
	if (tgt != status_buf || tgtsize != sizeof(status_buf)) {
		upsdebugx(2, "%s: called for wrong use-case", __func__);
		return 0;
	}

	if (ignorelb && !strcasecmp(token, "LB")) {
		upsdebugx(2, "%s: ignoring LB flag from device", __func__);
		return 0;
	}

	if (!strcasecmp(token, "ALARM")) {
		/* Drivers really should not raise alarms this way,
		 * but for the sake of third-party forks, we handle
		 * the possibility...
		 */
		upsdebugx(2, "%s: (almost) ignoring ALARM set as a status", __func__);
		if (!alarm_status && !alarm_active && strlen(alarm_buf) == 0) {
			alarm_init();	/* no-op currently, but better be proper about it */
			alarm_set("[N/A]");
		}
		alarm_status++;
		return 0;
	}

	/* Proceed adding the token */
	return 1;
}

void status_set(const char *buf)
{
#ifdef DEBUG
	upsdebugx(3, "%s: '%s'\n", __func__, buf);
#endif
	str_add_unique_token(status_buf, sizeof(status_buf), buf, status_set_callback, NULL);
}

/* write the status_buf into the externally visible dstate storage */
void status_commit(void)
{
	while (ignorelb) {
		const char	*val, *low;

		val = dstate_getinfo("battery.charge");
		low = dstate_getinfo("battery.charge.low");

		if (val && low && (strtol(val, NULL, 10) < strtol(low, NULL, 10))) {
			snprintfcat(status_buf, sizeof(status_buf), " LB");
			upsdebugx(2, "%s: appending LB flag [charge '%s' below '%s']", __func__, val, low);
			break;
		}

		val = dstate_getinfo("battery.runtime");
		low = dstate_getinfo("battery.runtime.low");

		if (val && low && (strtol(val, NULL, 10) < strtol(low, NULL, 10))) {
			snprintfcat(status_buf, sizeof(status_buf), " LB");
			upsdebugx(2, "%s: appending LB flag [runtime '%s' below '%s']", __func__, val, low);
			break;
		}

		/* LB condition not detected */
		break;
	}

	/* NOTE: Not sure if any clients rely on ALARM being first if raised,
	 * but note that if someone also uses status_set("ALARM") we can end
	 * up with a "[N/A]" alarm value injected (if no other alarm was set)
	 * and only add the token here so it remains first.
	 *
	 * NOTE: alarm_commit() must be executed before status_commit() for
	 * this report to work!
	 * * If a driver only called status_set("ALARM") and did not bother
	 *   with alarm_commit(), the "ups.alarm" value queries would have
	 *   returned NULL if not for the "sloppy driver" fix below, although
	 *   the "ups.status" value would report an ALARM token.
	 * * If a driver properly used alarm_init() and alarm_set(), but then
	 *   called status_commit() before alarm_commit(), the "ups.status"
	 *   value would not know to report an ALARM token, as before.
	 * * If a driver used both status_set("ALARM") and alarm_set() later,
	 *   the injected "[N/A]" value of the alarm (if that's its complete
	 *   value) would be overwritten by the explicitly assigned contents,
	 *   and an explicit alarm_commit() would be required for proper
	 *   reporting from a non-sloppy driver.
	 */

	if (!alarm_active && alarm_status && !strcmp(alarm_buf, "[N/A]")) {
		upsdebugx(2, "%s: Assume sloppy driver coding that ignored alarm methods and used status_set(\"ALARM\") instead: commit the injected N/A ups.alarm value", __func__);
		alarm_commit();
	}

	if (alarm_active) {
		dstate_setinfo("ups.status", "ALARM %s", status_buf);
	} else {
		dstate_setinfo("ups.status", "%s", status_buf);
	}
}

/* similar functions for experimental.ups.mode.buzzwords, where tracked
 * dynamically (e.g. due to ECO/ESS/HE/Smart modes supported by the device) */
void buzzmode_init(void)
{
	memset(buzzmode_buf, 0, sizeof(buzzmode_buf));
}

int  buzzmode_get(const char *buf)
{
	return str_contains_token(buzzmode_buf, buf);
}

void buzzmode_set(const char *buf)
{
	str_add_unique_token(buzzmode_buf, sizeof(buzzmode_buf), buf, NULL, NULL);
}

void buzzmode_commit(void)
{
	if (!*buzzmode_buf) {
		dstate_delinfo("experimental.ups.mode.buzzwords");
		return;
	}

	dstate_setinfo("experimental.ups.mode.buzzwords", "%s", buzzmode_buf);
}

/* similar handlers for ups.alarm */

void alarm_init(void)
{
	/* reinit global counter */
	alarm_active = 0;

	device_alarm_init();
}

#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
void alarm_set(const char *buf)
{
	/* NOTE: Differs from status_set() since we can add whole sentences
	 *  here, not just unique tokens. Drivers are encouraged to wrap such
	 *  sentences into brackets, especially when many alarms raised at once
	 *  are anticipated, for readability.
	 */
	int ret;
	if (strlen(alarm_buf) < 1 || (alarm_status && !strcmp(alarm_buf, "[N/A]"))) {
		ret = snprintf(alarm_buf, sizeof(alarm_buf), "%s", buf);
	} else {
		ret = snprintfcat(alarm_buf, sizeof(alarm_buf), " %s", buf);
	}

	if (ret < 0) {
		/* Should we also try to print the potentially unusable buf?
		 * Generally - likely not. But if it is short enough...
		 * Note: LARGEBUF was the original limit mismatched vs. alarm_buf
		 * size before PR #986.
		 */
		char	alarm_tmp[LARGEBUF];
		int	ibuflen;
		size_t	buflen;

		memset(alarm_tmp, 0, sizeof(alarm_tmp));
		/* A bit of complexity to keep both (int)snprintf(...) and (size_t)sizeof(...) happy */
		ibuflen = snprintf(alarm_tmp, sizeof(alarm_tmp), "%s", buf);
		if (ibuflen < 0) {
			alarm_tmp[0] = 'N';
			alarm_tmp[1] = '/';
			alarm_tmp[2] = 'A';
			alarm_tmp[3] = '\0';
			buflen = strlen(alarm_tmp);
		} else {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
/* Note for gating macros above: unsuffixed HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP
 * means support of contexts both inside and outside function body, so the push
 * above and pop below (outside this finction) are not used.
 */
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
/* Note that the individual warning pragmas for use inside function bodies
 * are named without a _INSIDEFUNC suffix, for simplicity and legacy reasons
 */
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
			if ((unsigned long long int)ibuflen < SIZE_MAX) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic pop
#endif
				buflen = (size_t)ibuflen;
			} else {
				buflen = SIZE_MAX;
			}
		}
		upslogx(LOG_ERR, "%s: error setting alarm_buf to: %s%s",
			__func__, alarm_tmp, ( (buflen < sizeof(alarm_tmp)) ? "" : "...<truncated>" ) );
	} else if ((size_t)ret > sizeof(alarm_buf)) {
		char	alarm_tmp[LARGEBUF];
		int	ibuflen;
		size_t	buflen;

		memset(alarm_tmp, 0, sizeof(alarm_tmp));
		ibuflen = snprintf(alarm_tmp, sizeof(alarm_tmp), "%s", buf);
		if (ibuflen < 0) {
			alarm_tmp[0] = 'N';
			alarm_tmp[1] = '/';
			alarm_tmp[2] = 'A';
			alarm_tmp[3] = '\0';
			buflen = strlen(alarm_tmp);
		} else {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
			if ((unsigned long long int)ibuflen < SIZE_MAX) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic pop
#endif
				buflen = (size_t)ibuflen;
			} else {
				buflen = SIZE_MAX;
			}
		}
		upslogx(LOG_WARNING, "%s: result was truncated while setting or appending "
			"alarm_buf (limited to %" PRIuSIZE " bytes), with message: %s%s",
			__func__, sizeof(alarm_buf), alarm_tmp,
			( (buflen < sizeof(alarm_tmp)) ? "" : "...<also truncated>" ));
	}
}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic pop
#endif

/* write the status_buf into the info array for "ups.alarm" */
void alarm_commit(void)
{
	/* Note this is a bit different from `device_alarm_commit(0);`
	 * because here we also increase AND zero out the alarm count.
	 *		alarm_active = 0; device_alarm_commit(0);
	 * would be equivalent, but too intimate for later maintenance.
	 */

	if (strlen(alarm_buf) > 0) {
		dstate_setinfo("ups.alarm", "%s", alarm_buf);
		alarm_active = 1;
	} else {
		dstate_delinfo("ups.alarm");
		alarm_active = 0;
	}
}

void device_alarm_init(void)
{
	/* only clear the buffer, don't touch the alarms counter */
	memset(alarm_buf, 0, sizeof(alarm_buf));
}

/* same as above, but writes to "device.X.ups.alarm" or "ups.alarm" */
/* Note that 20 chars below just allow for a 2-digit "X" */
/* FIXME? Shouldn't this be changed to be a LARGEBUF aka sizeof(alarm_buf) ? */
void device_alarm_commit(const int device_number)
{
	char info_name[20];

	memset(info_name, 0, 20);

	if (device_number != 0) /* would then go into "device.%i.alarm" */
		snprintf(info_name, 20, "device.%i.ups.alarm", device_number);
	else /* would then go into "device.alarm" */
		snprintf(info_name, 20, "ups.alarm");

	/* Daisychain subdevices note:
	 * increase the counter when alarms are present on a subdevice, but
	 * don't decrease the count. Otherwise, we may not get the ALARM flag
	 * in ups.status, while there are some alarms present on device.X */
	if (strlen(alarm_buf) > 0) {
		dstate_setinfo(info_name, "%s", alarm_buf);
		alarm_active++;
	} else {
		dstate_delinfo(info_name);
	}
}

/* For devices where we do not have phase-count info (no mapping provided
 * in the tables), nor in the device data/protocol, we can still guesstimate
 * and report a value. This routine may also replace an existing value, e.g.
 * if we've found new data disproving old one (e.g. if the 3-phase UPS was
 * disbalanced when the driver was started, so we thought it is 1-phase in
 * practice, and then the additional lines came up loaded, hence the bools
 * "may_reevaluate" and the readonly flag "may_change_dstate" (so the caller
 * can query the current apparent situation, without changing any dstates).
 * It is up to callers to decide if they already have data they want to keep.
 * The "xput_prefix" is e.g. "input." or "input.bypass." or "output." with
 * the trailing dot where applicable - we use this string verbatim below.
 * The "inited_phaseinfo" and "num_phases" are addresses of caller's own
 * variables to store the flag (if we have successfully inited) and the
 * discovered amount of phases, or NULL if caller does not want to track it.
 *
 * NOTE: The code below can detect if the device is 1, 2 (split phase) or 3
 * phases.
 *
 * Returns:
 *   -1     Runtime/input error (non fatal, but routine was skipped)
 *    0     Nothing changed: could not determine a value
 *    1     A phase value was just determined (and set, if not read-only mode)
 *    2     Nothing changed: already inited (and may_reevaluate==false)
 *    3     Nothing changed: detected a value but it is already published
 *          as a dstate; populated inited_phaseinfo and num_phases though
 */
int dstate_detect_phasecount(
		const char *xput_prefix,
		const int may_change_dstate,
		int *inited_phaseinfo,
		int *num_phases,
		const int may_reevaluate
) {
	/* If caller does not want either of these back - loopback the values below */
	int local_inited_phaseinfo = 0, local_num_phases = -1;
	/* Temporary local value storage */
	int old_num_phases = -1, detected_phaseinfo = 0;

	if (!inited_phaseinfo)
		inited_phaseinfo = &local_inited_phaseinfo;
	if (!num_phases)
		num_phases = &local_num_phases;
	old_num_phases = *num_phases;

	upsdebugx(3, "Entering %s('%s', %i, %i, %i, %i)", __func__,
		xput_prefix, may_change_dstate, *inited_phaseinfo, *num_phases, may_reevaluate);

	if (!(*inited_phaseinfo) || may_reevaluate) {
		const char *v1,  *v2,  *v3,  *v0,
		           *v1n, *v2n, *v3n,
		           *v12, *v23, *v31,
		           *c1,  *c2,  *c3,  *c0;
		char buf[MAX_STRING_SIZE]; /* For concatenation of "xput_prefix" with items we want to query */
		size_t xput_prefix_len;
		size_t bufrw_max;
		char *bufrw_ptr = NULL;

		if (!xput_prefix) {
			upsdebugx(0, "%s(): Bad xput_prefix was passed: it is NULL - function skipped", __func__);
			return -1;
		}

		xput_prefix_len = strlen(xput_prefix);
		if (xput_prefix_len < 1) {
			upsdebugx(0, "%s(): Bad xput_prefix was passed: it is empty - function skipped", __func__);
			return -1;
		}

		bufrw_max = sizeof(buf) - xput_prefix_len;
		if (bufrw_max <= 15) {
			/* We need to append max ~13 chars per below, so far */
			upsdebugx(0, "%s(): Bad xput_prefix was passed: it is too long - function skipped", __func__);
			return -1;
		}
		memset(buf, 0, sizeof(buf));
		strncpy(buf, xput_prefix, sizeof(buf));
		bufrw_ptr = buf + xput_prefix_len ;

		/* We either have defined and non-zero (numeric) values below, or NULLs.
		 * Note that as "zero" we should expect any valid numeric representation
		 * of a zero value as some drivers may save strangely formatted values.
		 * For now, we limit the level of paranoia with missing dstate entries,
		 * empty entries, and actual single zero character as contents of the
		 * string. Other obscure cases (string of multiple zeroes, a floating
		 * point zero, surrounding whitespace etc. may be solved if the need
		 * does arise in the future. Arguably, drivers' translation/mapping
		 * tables should take care of this with converion routine and numeric
		 * data type flags. */
#define dstate_getinfo_nonzero(var, suffix) \
		do { strncpy(bufrw_ptr, suffix, bufrw_max); \
		  if ( (var = dstate_getinfo(buf)) ) { \
		    if ( (var[0] == '0' && var[1] == '\0') || \
		         (var[0] == '\0') ) { \
		      var = NULL; \
		    } \
		  } \
		} while(0)

		dstate_getinfo_nonzero(v1,  "L1.voltage");
		dstate_getinfo_nonzero(v2,  "L2.voltage");
		dstate_getinfo_nonzero(v3,  "L3.voltage");
		dstate_getinfo_nonzero(v1n, "L1-N.voltage");
		dstate_getinfo_nonzero(v2n, "L2-N.voltage");
		dstate_getinfo_nonzero(v3n, "L3-N.voltage");
		dstate_getinfo_nonzero(v1n, "L1-N.voltage");
		dstate_getinfo_nonzero(v12, "L1-L2.voltage");
		dstate_getinfo_nonzero(v23, "L2-L3.voltage");
		dstate_getinfo_nonzero(v31, "L3-L1.voltage");
		dstate_getinfo_nonzero(c1,  "L1.current");
		dstate_getinfo_nonzero(c2,  "L2.current");
		dstate_getinfo_nonzero(c3,  "L3.current");
		dstate_getinfo_nonzero(v0,  "voltage");
		dstate_getinfo_nonzero(c0,  "current");

		if ( (v1 && v2 && !v3) ||
		     (v1n && v2n && !v3n) ||
		     (c1 && c2 && !c3) ||
		     (v12 && !v23 && !v31) ) {
			upsdebugx(5, "%s(): determined a 2-phase case", __func__);
			*num_phases = 2;
			*inited_phaseinfo = 1;
			detected_phaseinfo = 1;
		} else if ( (v1 && v2 && v3) ||
		     (v1n && v2n && v3n) ||
		     (c1 && (c2 || c3)) ||
		     (c2 && (c1 || c3)) ||
		     (c3 && (c1 || c2)) ||
		     v12 || v23 || v31 ) {
			upsdebugx(5, "%s(): determined a 3-phase case", __func__);
			*num_phases = 3;
			*inited_phaseinfo = 1;
			detected_phaseinfo = 1;
		} else if ( /* We definitely have only one non-zero line */
		     !v12 && !v23 && !v31 && (
		     (c0 && !c1 && !c2 && !c3) ||
		     (v0 && !v1 && !v2 && !v3) ||
		     (c1 && !c2 && !c3) ||
		     (!c1 && c2 && !c3) ||
		     (!c1 && !c2 && c3) ||
		     (v1 && !v2 && !v3) ||
		     (!v1 && v2 && !v3) ||
		     (!v1 && !v2 && v3) ||
		     (v1n && !v2n && !v3n) ||
		     (!v1n && v2n && !v3n) ||
		     (!v1n && !v2n && v3n) ) ) {
			*num_phases = 1;
			*inited_phaseinfo = 1;
			detected_phaseinfo = 1;
			upsdebugx(5, "%s(): determined a 1-phase case", __func__);
		} else {
			upsdebugx(5, "%s(): could not determine the phase case", __func__);
		}

		if (detected_phaseinfo) {
			const char *oldphases;
			strncpy(bufrw_ptr, "phases", bufrw_max);
			oldphases = dstate_getinfo(buf);

			if (oldphases) {
				if (atoi(oldphases) == *num_phases) {
					/* Technically, a bit has changed: we have set the flag which may have been missing before */
					upsdebugx(5, "%s(): Nothing changed, with a valid reason; dstate already published with the same value: %s=%s (detected %d)",
						__func__, buf, oldphases, *num_phases);
					return 3;
				}
			}

			if ( (*num_phases != old_num_phases) || (!oldphases) ) {
				if (may_change_dstate) {
					dstate_setinfo(buf, "%d", *num_phases);
					upsdebugx(3, "%s(): calculated non-XML value for NUT variable %s was set to %d",
						__func__, buf, *num_phases);
				} else {
					upsdebugx(3, "%s(): calculated non-XML value for NUT variable %s=%d but did not set its dstate (read-only request)",
						__func__, buf, *num_phases);
				}
				return 1;
			}
		}

		upsdebugx(5, "%s(): Nothing changed: could not determine a value", __func__);
		return 0;
	}

	upsdebugx(5, "%s(): Nothing changed, with a valid reason; already inited", __func__);
	return 2;
}

/* Dump the data tree (in upsc-like format) to stdout */
/* Actual implementation */
static int dstate_tree_dump(const st_tree_t *node)
{
	int	ret;

	if (!node) {
		return 1;	/* not an error */
	}

	if (node->left) {
		ret = dstate_tree_dump(node->left);

		if (!ret) {
			return 0;	/* write failed in the child */
		}
	}

	printf("%s: %s\n", node->var, node->val);

	if (node->right) {
		return dstate_tree_dump(node->right);
	}

	return 1;	/* everything's OK here ... */
}

/* Dump the data tree (in upsc-like format) to stdout */
/* Public interface */
void dstate_dump(void)
{
	const st_tree_t *node;

	upsdebugx(3, "Entering %s", __func__);

	node = (const st_tree_t *)dstate_getroot();
	fflush(stderr);

	dstate_tree_dump(node);

	/* Make sure it lands in one piece and is logged where called */
	fflush(stdout);
	fflush(stderr);
}
