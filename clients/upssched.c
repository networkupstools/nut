/* upssched.c - upsmon's scheduling helper for offset timers

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>

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

/* design notes for the curious:
 *
 * 1. we get called with a upsname and notifytype from upsmon
 * 2. the config file is searched for an AT condition that matches
 * 3. the conditions on any matching lines are parsed
 *
 * starting a timer: the timer is added to the daemon's timer queue
 * cancelling a timer: the timer is removed from that queue
 * execute a command: the command is passed straight to the cmdscript
 *
 * if the daemon is not already running and is required (to start a timer)
 * it will be started automatically
 *
 * when the time arrives, the command associated with a timer will be
 * executed by the daemon (via the cmdscript)
 *
 * timers can be cancelled at any time before they trigger
 *
 * the daemon will shut down automatically when no more timers are active
 *
 */

#include "common.h"

#include <sys/types.h>
#ifndef WIN32
# include <sys/wait.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <unistd.h>
# include <fcntl.h>
# include <poll.h>
#else	/* WIN32 */
# include "wincompat.h"
# include <winsock2.h>
# include <ws2tcpip.h>
#endif	/* WIN32 */

#include "upssched.h"
#include "timehead.h"
#include "nut_stdint.h"

typedef struct ttype_s {
	char	*name;
	time_t	etime;
	struct ttype_s	*next;
} ttype_t;

static ttype_t	*thead = NULL;
static conn_t	*connhead = NULL;
static char	*cmdscript = NULL, *pipefn = NULL, *lockfn = NULL;

/* ups name and notify type (string) as received from upsmon */
static const	char	*upsname, *notify_type, *prog = NULL;

#ifdef WIN32
static OVERLAPPED connect_overlapped;
# define BUF_LEN 512
#endif	/* WIN32 */

#define PARENT_STARTED		-2
#define PARENT_UNNECESSARY	-3
#define MAX_TRIES 		30
#define EMPTY_WAIT		15	/* min passes with no timers to exit */
#define US_LISTEN_BACKLOG	16
#define US_SOCK_BUF_LEN		256
#define US_MAX_READ		128

/* --- server functions --- */

static void exec_cmd(const char *cmd)
{
	int	err;
	char	buf[LARGEBUF];

	snprintf(buf, sizeof(buf), "%s %s", cmdscript, cmd);

	err = system(buf);
#ifndef WIN32
	if (WIFEXITED(err)) {
		if (WEXITSTATUS(err)) {
			upslogx(LOG_INFO, "exec_cmd(%s) returned %d", buf, WEXITSTATUS(err));
		}
	} else {
		if (WIFSIGNALED(err)) {
			upslogx(LOG_WARNING, "exec_cmd(%s) terminated with signal %d", buf, WTERMSIG(err));
		} else {
			upslogx(LOG_ERR, "Execute command failure: %s", buf);
		}
	}
#else	/* WIN32 */
	if(err != -1) {
		upslogx(LOG_INFO, "Execute command \"%s\" OK", buf);
	}
	else {
		upslogx(LOG_ERR, "Execute command failure : %s", buf);
	}
#endif	/* WIN32 */

	return;
}

static void removetimer(ttype_t *tfind)
{
	ttype_t	*tmp, *last;

	last = NULL;
	tmp = thead;

	while (tmp) {
		if (tmp == tfind) {	/* found it */
			if (last == NULL)	/* deleting first */
				thead = tmp->next;
			else
				last->next = tmp->next;

			free(tmp->name);
			free(tmp);
			return;
		}

		last = tmp;
		tmp = tmp->next;
	}

	/* this one should never happen */

	upslogx(LOG_ERR, "removetimer: failed to locate target at %p", (void *)tfind);
}

static void checktimers(void)
{
	ttype_t	*tmp, *tmpnext;
	time_t	now;
	static	int	emptyctr = 0;

	/* if the queue is empty we might be ready to exit */
	if (!thead) {

		emptyctr++;

		/* wait a little while in case someone wants us again */
		if (emptyctr < EMPTY_WAIT)
			return;

		if (nut_debug_level)
			upslogx(LOG_INFO, "Timer queue empty, exiting");

#ifdef UPSSCHED_RACE_TEST
		upslogx(LOG_INFO, "triggering race: sleeping 15 sec before exit");
		sleep(15);
#endif

		upsdebugx(1, "Timer queue empty, closing pipe and exiting upssched daemon");
		unlink(pipefn);
		exit(EXIT_SUCCESS);
	}

	emptyctr = 0;

	/* flip through LL, look for activity */
	tmp = thead;

	time(&now);
	while (tmp) {
		tmpnext = tmp->next;

		if (now >= tmp->etime) {
			if (nut_debug_level)
				upslogx(LOG_INFO, "Event: %s ", tmp->name);

			exec_cmd(tmp->name);

			/* delete from queue */
			removetimer(tmp);
		}

		tmp = tmpnext;
	}
}

static void start_timer(const char *name, const char *ofsstr)
{
	time_t	now;
	long	ofs;
	ttype_t	*tmp, *last;

	/* get the time */
	time(&now);

	/* add an event for <now> + <time> */
	ofs = strtol(ofsstr, (char **) NULL, 10);

	if (ofs < 0) {
		upslogx(LOG_INFO, "bogus offset for timer, ignoring");
		return;
	}

	if (nut_debug_level)
		upslogx(LOG_INFO, "New timer: %s (%ld seconds)", name, ofs);

	/* now add to the queue */
	tmp = last = thead;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ttype_t));
	tmp->name = xstrdup(name);
	tmp->etime = now + ofs;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		thead = tmp;
}

static void cancel_timer(const char *name, const char *cname)
{
	ttype_t	*tmp;

	for (tmp = thead; tmp != NULL; tmp = tmp->next) {
		if (!strcmp(tmp->name, name)) {		/* match */
			if (nut_debug_level)
				upslogx(LOG_INFO, "Cancelling timer: %s", name);
			removetimer(tmp);
			return;
		}
	}

	/* this is not necessarily an error */
	if (cname && cname[0]) {
		if (nut_debug_level)
			upslogx(LOG_INFO, "Cancel %s, event: %s", name, cname);

		exec_cmd(cname);
	}
}

#ifndef WIN32
static void us_serialize(int op)
{
	static	int	pipefd[2];
	ssize_t	ret;
	char	ch;

	switch(op) {
		case SERIALIZE_INIT:
			ret = pipe(pipefd);

			if (ret != 0)
				fatal_with_errno(EXIT_FAILURE, "serialize: pipe");

			break;

		case SERIALIZE_SET:
			close(pipefd[0]);
			close(pipefd[1]);
			break;

		case SERIALIZE_WAIT:
			close(pipefd[1]);
			ret = read(pipefd[0], &ch, 1);
			close(pipefd[0]);
			break;

		default:
			break;
	}
}
#endif	/* !WIN32 */

static TYPE_FD open_sock(void)
{
	TYPE_FD fd;

#ifndef WIN32
	int	ret;
	struct	sockaddr_un	ssaddr;

	check_unix_socket_filename(pipefn);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (INVALID_FD(fd))
		fatal_with_errno(EXIT_FAILURE, "Can't create a unix domain socket");

	ssaddr.sun_family = AF_UNIX;
	snprintf(ssaddr.sun_path, sizeof(ssaddr.sun_path), "%s", pipefn);

	unlink(pipefn);

	umask(0007);

	ret = bind(fd, (struct sockaddr *) &ssaddr, sizeof ssaddr);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "bind %s failed", pipefn);

	ret = chmod(pipefn, 0660);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "chmod(%s, 0660) failed", pipefn);

	ret = listen(fd, US_LISTEN_BACKLOG);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "listen(%d, %d) failed", fd, US_LISTEN_BACKLOG);

	/* don't leak socket to CMDSCRIPT */
	set_close_on_exec(fd);

#else /* WIN32 */

	fd = CreateNamedPipe(
			pipefn, /* pipe name */
			PIPE_ACCESS_DUPLEX | /* read/write access */
			FILE_FLAG_OVERLAPPED, /* async IO */
			PIPE_TYPE_BYTE |
			PIPE_READMODE_BYTE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES, /* max. instances */
			BUF_LEN, /* output buffer size */
			BUF_LEN, /* input buffer size */
			0, /* client time-out */
			NULL); /* FIXME: default security attributes */

	if (INVALID_FD(fd)) {
		fatal_with_errno(EXIT_FAILURE,
			"Can't create a state socket (windows named pipe)");
	}

	/* Prepare an async wait on a connection on the pipe */
	memset(&connect_overlapped,0,sizeof(connect_overlapped));
	connect_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if (connect_overlapped.hEvent == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	/* Wait for a connection */
	ConnectNamedPipe(fd,&connect_overlapped);
#endif /* WIN32 */

	return fd;
}

static void conn_del(conn_t *target)
{
	conn_t	*tmp, *last = NULL;

	tmp = connhead;

	while (tmp) {
		if (tmp == target) {

			if (last)
				last->next = tmp->next;
			else
				connhead = tmp->next;

			pconf_finish(&tmp->ctx);

			free(tmp);
			return;
		}

		last = tmp;
		tmp = tmp->next;
	}

	upslogx(LOG_ERR, "Tried to delete a bogus state connection");
}

static int send_to_one(conn_t *conn, const char *fmt, ...)
{
	ssize_t	ret;
	size_t	buflen;
	va_list	ap;
	char	buf[US_SOCK_BUF_LEN];

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
	vsnprintf(buf, sizeof(buf), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	buflen = strlen(buf);
	if (buflen >= SSIZE_MAX) {
		/* Can't compare buflen to ret */
		upsdebugx(2, "send_to_one(): buffered message too large");

		if (VALID_FD(conn->fd)) {
#ifndef WIN32
			close(conn->fd);
#else	/* WIN32 */
			FlushFileBuffers(conn->fd);
			CloseHandle(conn->fd);
#endif	/* WIN32 */
			conn->fd = ERROR_FD;
		}

		conn_del(conn);

		return 0;	/* failed */
	}

#ifndef WIN32
	ret = write(conn->fd, buf, buflen);

	if ((ret < 1) || (ret != (ssize_t) buflen)) {
		upsdebugx(2, "write failed on socket %d, disconnecting", conn->fd);

		close(conn->fd);
		conn_del(conn);

		return 0;	/* failed */
	}
#else	/* WIN32 */
	DWORD bytesWritten = 0;
	BOOL  result = FALSE;

	result = WriteFile (conn->fd, buf, buflen, &bytesWritten, NULL);
	if (result == 0) {
		upsdebugx(2, "write failed on handle %p, disconnecting", conn->fd);

		/* FIXME not sure this is the right way to close a connection */
		if( conn->read_overlapped.hEvent != INVALID_HANDLE_VALUE) {
			CloseHandle(conn->read_overlapped.hEvent);
			conn->read_overlapped.hEvent = INVALID_HANDLE_VALUE;
		}
		DisconnectNamedPipe(conn->fd);
		CloseHandle(conn->fd);
		conn_del(conn);

		return 0;
	}
	else {
		ret = (ssize_t)bytesWritten;
	}

	if ((ret < 1) || (ret != (ssize_t)buflen)) {
		upsdebugx(2, "write to fd %p failed", conn->fd);
		/* FIXME not sure this is the right way to close a connection */
		if (conn->read_overlapped.hEvent != INVALID_HANDLE_VALUE) {
			CloseHandle(conn->read_overlapped.hEvent);
			conn->read_overlapped.hEvent = INVALID_HANDLE_VALUE;
		}
		DisconnectNamedPipe(conn->fd);
		CloseHandle(conn->fd);

		return 0;	/* failed */
	}
#endif /* WIN32 */

	return 1;	/* OK */
}

static TYPE_FD conn_add(TYPE_FD sockfd)
{
	TYPE_FD	acc;

#ifndef WIN32
	int	ret;
	conn_t	*tmp, *last;
	struct	sockaddr_un	saddr;
# if defined(__hpux) && !defined(_XOPEN_SOURCE_EXTENDED)
	int			salen;
# else
	socklen_t	salen;
# endif

	salen = sizeof(saddr);
	acc = accept(sockfd, (struct sockaddr *) &saddr, &salen);

	if (INVALID_FD(acc)) {
		upslog_with_errno(LOG_ERR, "accept on unix fd failed");
		return ERROR_FD;
	}

	/* don't leak connection to CMDSCRIPT */
	set_close_on_exec(acc);

	/* enable nonblocking I/O */

	ret = fcntl(acc, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl get on unix fd failed");
		close(acc);
		return ERROR_FD;
	}

	ret = fcntl(acc, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on unix fd failed");
		close(acc);
		return ERROR_FD;
	}

	tmp = last = connhead;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(conn_t));
	tmp->fd = acc;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		connhead = tmp;

	upsdebugx(3, "new connection on fd %d", acc);

	pconf_init(&tmp->ctx, NULL);

#else /* WIN32 */

	conn_t	*conn, *tmp, *last;

	/* We have detected a connection on the opened pipe. So we start
	   by saving its handle  and create a new pipe for future connection */
	conn = xcalloc(1, sizeof(*conn));
	conn->fd = sockfd;

	/* sock is the handle of the connection pending pipe */
	acc = CreateNamedPipe(
			pipefn, /* pipe name */
			PIPE_ACCESS_DUPLEX |  /* read/write access */
			FILE_FLAG_OVERLAPPED, /* async IO */
			PIPE_TYPE_BYTE |
			PIPE_READMODE_BYTE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES, /* max. instances */
			BUF_LEN, /* output buffer size */
			BUF_LEN, /* input buffer size */
			0, /* client time-out */
			NULL); /* FIXME: default security attribute */

	if (INVALID_FD(acc)) {
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
	if (connect_overlapped.hEvent == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	/* Wait for a connection */
	ConnectNamedPipe(acc,&connect_overlapped);

	/* A new pipe waiting for new client connection has been created.
	   We could manage the current connection now */
	/* Start a read operation on the newly connected pipe so we could wait
	   on the event associated to this IO */
	memset(&conn->read_overlapped,0,sizeof(conn->read_overlapped));
	memset(conn->buf,0,sizeof(conn->buf));
	conn->read_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if (conn->read_overlapped.hEvent == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	ReadFile (conn->fd,conn->buf,1,NULL,&(conn->read_overlapped));

	conn->next = NULL;

	tmp = last = connhead;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	if (last)
		last->next = conn;
	else
		connhead = conn;

	upsdebugx(3, "new connection on handle %p", acc);

	pconf_init(&conn->ctx, NULL);
#endif /* WIN32 */

	return acc;
}

static int sock_arg(conn_t *conn)
{
	if (conn->ctx.numargs < 1)
		return 0;

	/* CANCEL <name> [<cmd>] */
	if (!strcmp(conn->ctx.arglist[0], "CANCEL")) {

		if (conn->ctx.numargs < 3)
			cancel_timer(conn->ctx.arglist[1], NULL);
		else
			cancel_timer(conn->ctx.arglist[1], conn->ctx.arglist[2]);

		send_to_one(conn, "OK\n");
		return 1;
	}

	if (conn->ctx.numargs < 3)
		return 0;

	/* START <name> <length> */
	if (!strcmp(conn->ctx.arglist[0], "START")) {
		start_timer(conn->ctx.arglist[1], conn->ctx.arglist[2]);
		send_to_one(conn, "OK\n");
		return 1;
	}

	/* unknown */
	return 0;
}

static void log_unknown(size_t numarg, char **arg)
{
	size_t	i;

	upslogx(LOG_INFO, "Unknown command on socket: ");

	for (i = 0; i < numarg; i++)
		upslogx(LOG_INFO, "arg %" PRIuSIZE ": %s", i, arg[i]);
}

static int sock_read(conn_t *conn)
{
	int	i;
	ssize_t	ret;
	char	ch;

	upsdebugx(6, "Starting sock_read()");
	for (i = 0; i < US_MAX_READ; i++) {
		/* NOTE: This does not imply that each command line must
		 * fit in the US_MAX_READ length limit - at worst we would
		 * "return 0", and continue with pconf_char() next round.
		 */
		size_t numarg;
#ifndef WIN32
		errno = 0;
		ret = read(conn->fd, &ch, 1);

		if (ret > 0)
			upsdebug_with_errno(6, "read() from fd %d returned %" PRIiSIZE " (bytes): '%c'",
				conn->fd, ret, ch);

		if (ret < 1) {

			/* short read = no parsing, come back later */
			if ((ret == -1) && (errno == EAGAIN)) {
				upsdebugx(6, "Ending sock_read(): short read");
				return 0;
			}

			/* O_NDELAY with zero bytes means nothing to read but
			 * since read() follows a successful select() with
			 * ready file descriptor, ret shouldn't be 0.
			 * This may also mean that the counterpart has exited
			 * and the file descriptor should be reaped.
			 */
			if (ret == 0) {
				struct pollfd pfd;
				pfd.fd = conn->fd;
				pfd.events = 0;
				pfd.revents = 0;
				/* Note: we check errno twice, since it could
				 * have been set by read() above or by one
				 * of the probing routines below
				 */
				if (errno
				|| (fcntl(conn->fd, F_GETFD) < 0)
				|| (poll(&pfd, 1, 0) <= 0)
				||  errno
				) {
					upsdebug_with_errno(4, "read() from fd %d returned 0", conn->fd);
					return -1;	/* connection closed, probably */
				}
				if (i == (US_MAX_READ - 1)) {
					upsdebug_with_errno(4, "read() from fd %d returned 0 "
						"too many times in a row, aborting "
						"sock_read()", conn->fd);
					return -1;	/* connection closed, probably */
				}
				continue;
			}

			/* some other problem */
			upsdebugx(6, "Ending sock_read(): some other problem");
			return -1;	/* error */
		}
#else	/* WIN32 */
		DWORD bytesRead;
		GetOverlappedResult(conn->fd, &conn->read_overlapped, &bytesRead,FALSE);
		if( bytesRead < 1 ) {
			/* Restart async read */
			memset(conn->buf,0,sizeof(conn->buf));
			ReadFile(conn->fd,conn->buf,1,NULL,&(conn->read_overlapped));
			return 0;
		}

		ch = conn->buf[0];

		/* Restart async read */
		memset(conn->buf,0,sizeof(conn->buf));
		ReadFile(conn->fd,conn->buf,1,NULL,&(conn->read_overlapped));
#endif /* WIN32 */

		ret = pconf_char(&conn->ctx, ch);

		if (ret == 0)	/* nothing to parse yet */
			continue;

		if (ret == -1) {
			upslogx(LOG_NOTICE, "Parse error on sock: %s",
				conn->ctx.errmsg);

			upsdebugx(6, "Ending sock_read(): parse error");
			return 0;	/* nothing parsed */
		}

		/* try to use it, and complain about unknown commands */
		upsdebugx(3, "Ending sock_read() on a good note: try to use command:");
		for (numarg = 0; numarg < conn->ctx.numargs; numarg++)
			upsdebugx(3, "\targ %" PRIuSIZE ": %s", numarg, conn->ctx.arglist[numarg]);
		if (!sock_arg(conn)) {
			log_unknown(conn->ctx.numargs, conn->ctx.arglist);
			send_to_one(conn, "ERR UNKNOWN\n");
		}

		return 1;	/* we did some work */
	}

	upsdebug_with_errno(6, "sock_read() from fd %d returned nothing "
		"(maybe still collecting the command line); ", conn->fd);

	return 0;	/* fell out without parsing anything */
}

static void start_daemon(TYPE_FD lockfd)
{
	int	maxfd = 0;	/* Unidiomatic use vs. "pipefd" below, which is "int" on non-WIN32 */
	TYPE_FD pipefd;
	struct	timeval	tv;
	conn_t	*tmp;

#ifndef WIN32
	int	pid, ret;
	fd_set	rfds;
	conn_t	*tmpnext;

	us_serialize(SERIALIZE_INIT);

	if ((pid = fork()) < 0)
		fatal_with_errno(EXIT_FAILURE, "Unable to enter background");

	if (pid != 0) {		/* parent */

		/* wait for child to set up the listener */
		us_serialize(SERIALIZE_WAIT);

		return;
	}

	/* child */

	/* make fds 0-2 (typically) point somewhere defined */
# ifdef HAVE_DUP2
	/* system can close (if needed) and (re-)open a specific FD number */
	if (1) { /* scoping */
		TYPE_FD devnull = open("/dev/null", O_RDWR);
		if (devnull < 0)
			fatal_with_errno(EXIT_FAILURE, "open /dev/null");

		if (dup2(devnull, STDIN_FILENO) != STDIN_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");
		if (dup2(devnull, STDOUT_FILENO) != STDOUT_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");

		if (nut_debug_level) {
			upsdebugx(1, "Keeping stderr open due to debug verbosity %d", nut_debug_level);
		} else {
			if (dup2(devnull, STDERR_FILENO) != STDERR_FILENO)
				fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");
		}

		close(devnull);
	}
# else /* not HAVE_DUP2 */
#  ifdef HAVE_DUP
	/* opportunistically duplicate to the "lowest-available" FD number */
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (dup(STDIN_FILENO) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDOUT");

	if (nut_debug_level) {
		upsdebugx(1, "Keeping stderr open due to debug verbosity %d", nut_debug_level);
	} else {
		close(STDERR_FILENO);
		if (dup(STDIN_FILENO) != STDERR_FILENO)
			fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDERR");
	}
#  else /* not HAVE_DUP */
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (open("/dev/null", O_RDWR) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");

	if (nut_debug_level) {
		upsdebugx(1, "Keeping stderr open due to debug verbosity %d", nut_debug_level);
	} else {
		close(STDERR_FILENO);
		if (open("/dev/null", O_RDWR) != STDERR_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");
	}
#  endif /* not HAVE_DUP */
# endif /* not HAVE_DUP2 */

	/* Still in child, non-WIN32 - work as timer daemon (infinite loop) */
	pipefd = open_sock();

	if (nut_debug_level)
		upslogx(LOG_INFO, "Timer daemon started");

	/* release the parent */
	us_serialize(SERIALIZE_SET);

	/* drop the lock now that the background is running */
	unlink(lockfn);
	close(lockfd);
	writepid(prog);

	/* Whatever upsmon envvars were set when this daemon started, would be
	 * irrelevant and only confusing at the moment a particular timer causes
	 * CMDSCRIPT to run */
	unsetenv("NOTIFYTYPE");
	unsetenv("UPSNAME");

	/* now watch for activity */
	upsdebugx(2, "Timer daemon waiting for connections on pipefd %d",
		pipefd);

	for (;;) {
		int	zero_reads = 0, total_reads = 0;
		struct timeval	start, now;

		gettimeofday(&start, NULL);

		/* wait at most 1s so we can check our timers regularly */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		FD_ZERO(&rfds);
		FD_SET(pipefd, &rfds);

		maxfd = pipefd;

		for (tmp = connhead; tmp != NULL; tmp = tmp->next) {
			FD_SET(tmp->fd, &rfds);

			if (tmp->fd > maxfd)
				maxfd = tmp->fd;
		}

		ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

		if (ret > 0) {
			if (FD_ISSET(pipefd, &rfds))
				conn_add(pipefd);

			tmp = connhead;

			while (tmp) {
				tmpnext = tmp->next;

				if (FD_ISSET(tmp->fd, &rfds)) {
					total_reads++;
					ret = sock_read(tmp);
					if (ret < 0) {
						upsdebugx(3, "closing connection on fd %d", tmp->fd);
						close(tmp->fd);
						conn_del(tmp);
					}
					if (ret == 0)
						zero_reads++;
				}

				tmp = tmpnext;
			}
		}

		checktimers();

		/* upsdebugx(6, "zero_reads=%d total_reads=%d", zero_reads, total_reads); */
		if (zero_reads && zero_reads == total_reads) {
			/* Catch run-away loops - that is, consider
			 * throttling the cycle as to not hog CPU:
			 * did select() spend its second to reply,
			 * or had something to say immediately?
			 * Note that while select() may have changed
			 * "tv" to deduct the time waited, our further
			 * processing loops could eat some more time.
			 * So we just check the difference of "start"
			 * and "now". If we did spend a substantial
			 * part of the second, do not delay further.
			 */
			double d;
			gettimeofday(&now, NULL);
			d = difftimeval(now, start);
			upsdebugx(6, "difftimeval() => %f sec", d);
			if (d > 0 && d < 0.2) {
				d = (1.0 - d) * 1000000.0;
				upsdebugx(5, "Enforcing a throttling sleep: %f usec", d);
				usleep((useconds_t)d);
			}
		}
	}

#else /* WIN32 */

	DWORD timeout_ms;
	HANDLE rfds[32];

	char module[NUT_PATH_MAX + 1];
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;
	if (!GetModuleFileName(NULL, module, sizeof(module))) {
		fatal_with_errno(EXIT_FAILURE, "Can't retrieve module name");
	}
	memset(&sinfo,0,sizeof(sinfo));
	if (!CreateProcess(module, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo)) {
		fatal_with_errno(EXIT_FAILURE, "Can't create child process");
	}
	pipefd = open_sock();

	if (nut_debug_level)
		upslogx(LOG_INFO, "Timer daemon started");

	/* drop the lock now that the background is running */
	CloseHandle(lockfd);
	DeleteFile(lockfn);
	writepid(prog);

	/* Whatever upsmon envvars were set when this daemon started, would be
	 * irrelevant and only confusing at the moment a particular timer causes
	 * CMDSCRIPT to run */
	unsetenv("NOTIFYTYPE");
	unsetenv("UPSNAME");

	/* now watch for activity */

	for (;;) {
		/* wait at most 1s so we can check our timers regularly */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		timeout_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

		maxfd = 0;

		/* Wait on the read IO of each connections */
		for (tmp = connhead; tmp != NULL; tmp = tmp->next) {
			rfds[maxfd] = tmp->read_overlapped.hEvent;
			maxfd++;
		}
		/* Add the connect event */
		rfds[maxfd] = connect_overlapped.hEvent;
		maxfd++;
		DWORD ret_val;
		ret_val = WaitForMultipleObjects(
				maxfd,  /* number of objects in array */
				rfds,   /* array of objects */
				FALSE,  /* wait for any object */
				timeout_ms); /* timeout in millisecond */

		if (ret_val == WAIT_FAILED) {
			upslog_with_errno(LOG_ERR, "waitfor failed");
			return;
		}

		/* timer has not expired */
		if (ret_val != WAIT_TIMEOUT) {
			/* Retrieve the signaled connection */
			for(tmp = connhead; tmp != NULL; tmp = tmp->next) {
				if( tmp->read_overlapped.hEvent == rfds[ret_val-WAIT_OBJECT_0]) {
					break;
				}
			}

			/* the connection event handle has been signaled */
			if (rfds[ret_val] == connect_overlapped.hEvent) {
				pipefd = conn_add(pipefd);
			}
			/* one of the read event handle has been signaled */
			else {
				if( tmp != NULL) {
					if (sock_read(tmp) < 0) {
						upsdebugx(3, "closing connection on handle %p", tmp->fd);
						CloseHandle(tmp->fd);
						conn_del(tmp);
					}
				}
			}

		}

		checktimers();
	}
#endif /* WIN32 */
}

/* --- 'client' functions --- */

static TYPE_FD try_connect(void)
{
	TYPE_FD pipefd;

#ifndef WIN32
	int	ret;
	struct	sockaddr_un saddr;

	check_unix_socket_filename(pipefn);

	memset(&saddr, '\0', sizeof(saddr));
	saddr.sun_family = AF_UNIX;
	snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", pipefn);

	pipefd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (INVALID_FD(pipefd))
		fatal_with_errno(EXIT_FAILURE, "socket");

	ret = connect(pipefd, (const struct sockaddr *) &saddr, sizeof(saddr));

	if (ret != -1)
		return pipefd;

#else /* WIN32 */

	BOOL   result = FALSE;

	result = WaitNamedPipe(pipefn,NMPWAIT_USE_DEFAULT_WAIT);

	if (result == FALSE) {
		return ERROR_FD;
	}

	pipefd = CreateFile(
			pipefn,       /* pipe name */
			GENERIC_READ |  /* read and write access */
			GENERIC_WRITE,
			0,              /* no sharing */
			NULL,           /* default security attributes FIXME */
			OPEN_EXISTING,  /* opens existing pipe */
			FILE_FLAG_OVERLAPPED,   /*  enable async IO */
			NULL);          /* no template file */

	if (VALID_FD(pipefd))
		return pipefd;

#endif /* WIN32 */

	return ERROR_FD;
}

static TYPE_FD get_lock(const char *fn)
{
#ifndef WIN32
	return open(fn, O_RDONLY | O_CREAT | O_EXCL, 0);
#else	/* WIN32 */
	return CreateFile(fn,GENERIC_ALL,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
#endif	/* WIN32 */
}

/* try to connect to bg process, and start one if necessary */
static TYPE_FD check_parent(const char *cmd, const char *arg2)
{
	TYPE_FD	pipefd, lockfd;
	int	tries = 0;

	for (tries = 0; tries < MAX_TRIES; tries++) {
		pipefd = try_connect();

		if (VALID_FD(pipefd))
			return pipefd;

		/* timer daemon isn't running */

		/* it's not running, so there's nothing to cancel */
		if (!strcmp(cmd, "CANCEL") && (arg2 == NULL))
			return (TYPE_FD)PARENT_UNNECESSARY;

		/* arg2 non-NULL means there is a cancel action available */

		/* we need to start the daemon, so try to get the lock */

		lockfd = get_lock(lockfn);

		if (VALID_FD(lockfd)) {
			start_daemon(lockfd);
			return (TYPE_FD)PARENT_STARTED;	/* started successfully */
		}

		/* we didn't get the lock - must be two upsscheds running */

		/* blow this away in case we crashed before */
#ifndef WIN32
		unlink(lockfn);
#else	/* WIN32 */
		DeleteFile(lockfn);
#endif	/* WIN32 */

		/* give the other one a chance to start it, then try again */
		usleep(250000);
	}

	upslog_with_errno(LOG_ERR, "Failed to connect to parent and failed to create parent");
	exit(EXIT_FAILURE);
}

static void sendcmd(const char *cmd, const char *arg1, const char *arg2)
{
	int	i;
	ssize_t	ret;
	size_t	enclen, buflen;
	char buf[SMALLBUF], enc[SMALLBUF + 8];
#ifndef WIN32
	int	ret_s;
	struct	timeval tv;
	fd_set	fdread;
#else	/* WIN32 */
	DWORD bytesWritten = 0;
#endif	/* WIN32 */
	TYPE_FD pipefd;

	/* insanity */
	if (!arg1)
		return;

	/* build the request */
	snprintf(buf, sizeof(buf), "%s \"%s\"",
		cmd, pconf_encode(arg1, enc, sizeof(enc)));

	if (arg2)
		snprintfcat(buf, sizeof(buf), " \"%s\"",
			pconf_encode(arg2, enc, sizeof(enc)));

	snprintf(enc, sizeof(enc), "%s\n", buf);

	/* Sanity checks, for static analyzers to sleep well */
	enclen = strlen(enc);
	buflen = strlen(buf);
	if (enclen >= SSIZE_MAX || buflen >= SSIZE_MAX) {
		/* Can't compare enclen to ret below */
		fatalx(EXIT_FAILURE, "Unable to connect to daemon: buffered message too large");
	}

	/* see if the parent needs to be started (and maybe start it) */

	for (i = 0; i < MAX_TRIES; i++) {

		pipefd = check_parent(cmd, arg2);

		if (pipefd == (TYPE_FD)PARENT_STARTED) {
			/* loop back and try to connect now */
			usleep(250000);
			continue;
		}

		/* special case for CANCEL when no parent is running */
		if (pipefd == (TYPE_FD)PARENT_UNNECESSARY)
			return;

		/* we're connected now */
#ifndef WIN32
		ret = write(pipefd, enc, enclen);

		/* if we can't send the whole thing, loop back and try again */
		if ((ret < 1) || (ret != (ssize_t)enclen)) {
			upslogx(LOG_ERR, "write failed, trying again");
			close(pipefd);
			continue;
		}

		/* select on child's pipe fd */
		do {
			/* set timeout every time before call select() */
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			FD_ZERO(&fdread);
			FD_SET(pipefd, &fdread);

			ret_s = select(pipefd + 1, &fdread, NULL, NULL, &tv);
			switch(ret_s) {
				/* select error */
				case -1:
					upslog_with_errno(LOG_DEBUG, "parent select error");
					break;

				/* nothing to read */
				case 0:
					break;

				/* available data to read */
				default:
					ret = read(pipefd, buf, sizeof(buf));
					break;
			}
		} while (ret_s <= 0);

		close(pipefd);

		/* same idea: no OK = go try it all again */
		if (ret < 2) {
			upslogx(LOG_ERR, "read confirmation failed, trying again");
			continue;
		}

#else /* WIN32 */
		ret = WriteFile(pipefd, enc, enclen, &bytesWritten, NULL);
		if (ret == 0 || bytesWritten != enclen) {
			upslogx(LOG_ERR, "write failed, trying again");
			CloseHandle(pipefd);
			continue;
		}

		OVERLAPPED read_overlapped;
		DWORD ret;

		memset(&read_overlapped,0,sizeof(read_overlapped));
		memset(buf,0,sizeof(buf));
		read_overlapped.hEvent = CreateEvent(NULL, /*Security*/
				FALSE, /* auto-reset*/
				FALSE, /* inital state = non signaled*/
				NULL /* no name*/);
		if(read_overlapped.hEvent == NULL ) {
			fatal_with_errno(EXIT_FAILURE, "Can't create event");
		}

		ReadFile(pipefd,buf,sizeof(buf)-1,NULL,&(read_overlapped));

		ret = WaitForSingleObject(read_overlapped.hEvent,2000);

		if (ret == WAIT_TIMEOUT || ret == WAIT_FAILED) {
			upslogx(LOG_ERR, "read confirmation failed, trying again");
			CloseHandle(pipefd);
			continue;
		}
#endif /* WIN32 */

		if (!strncmp(buf, "OK", 2))
			return;		/* success */

		upslogx(LOG_ERR, "read confirmation got [%s]", buf);

		/* try again ... */
	}	/* loop until MAX_TRIES if no success above */

	fatalx(EXIT_FAILURE, "Unable to connect to daemon and unable to start daemon");
}

static void parse_at(const char *ntype, const char *un, const char *cmd,
		const char *ca1, const char *ca2)
{
	/* complain both ways in case we don't have a tty */

	if (!cmdscript) {
		printf("CMDSCRIPT must be set before any ATs in the config file!\n");
		fatalx(EXIT_FAILURE, "CMDSCRIPT must be set before any ATs in the config file!");
	}

	if (!pipefn) {
		printf("PIPEFN must be set before any ATs in the config file!\n");
		fatalx(EXIT_FAILURE, "PIPEFN must be set before any ATs in the config file!");
	}

	if (!lockfn) {
		printf("LOCKFN must be set before any ATs in the config file!\n");
		fatalx(EXIT_FAILURE, "LOCKFN must be set before any ATs in the config file!");
	}

	/* check upsname: does this apply to us? */
	upsdebugx(2, "%s: is '%s' in AT command the '%s' we were launched to process?",
		__func__, un, upsname);
	if (strcmp(upsname, un) != 0) {
		if (strcmp(un, "*") != 0) {
			upsdebugx(1, "%s: SKIP: '%s' in AT command "
				"did not match the '%s' UPSNAME "
				"we were launched to process",
				__func__, un, upsname);
			return;		/* not for us, and not the wildcard */
		} else {
			upsdebugx(1, "%s: this AT command is for a wildcard: matched", __func__);
		}
	} else {
		upsdebugx(1, "%s: '%s' in AT command matched the '%s' "
			"UPSNAME we were launched to process",
			__func__, un, upsname);
	}

	/* see if the current notify type matches the one from the .conf */
	if (strcasecmp(notify_type, ntype) != 0) {
		upsdebugx(1, "%s: SKIP: '%s' in AT command "
			"did not match the '%s' NOTIFYTYPE "
			"we were launched to process",
			__func__, ntype, notify_type);
		return;
	}

	/* if command is valid, send it to the daemon (which may start it) */

	if (!strcmp(cmd, "START-TIMER")) {
		upsdebugx(1, "%s: processing %s", __func__, cmd);
		sendcmd("START", ca1, ca2);
		return;
	}

	if (!strcmp(cmd, "CANCEL-TIMER")) {
		upsdebugx(1, "%s: processing %s", __func__, cmd);
		sendcmd("CANCEL", ca1, ca2);
		return;
	}

	if (!strcmp(cmd, "EXECUTE")) {
		upsdebugx(1, "%s: processing %s", __func__, cmd);

		if (ca1[0] == '\0') {
			upslogx(LOG_ERR, "Empty EXECUTE command argument");
			return;
		}

		if (nut_debug_level)
			upslogx(LOG_INFO, "Executing command: %s", ca1);

		exec_cmd(ca1);
		return;
	}

	upslogx(LOG_ERR, "Invalid command: %s", cmd);
}

static int conf_arg(size_t numargs, char **arg)
{
	if (numargs < 2)
		return 0;

	/* CMDSCRIPT <scriptname> */
	if (!strcmp(arg[0], "CMDSCRIPT")) {
		cmdscript = xstrdup(arg[1]);
		return 1;
	}

	/* PIPEFN <pipename> */
	if (!strcmp(arg[0], "PIPEFN")) {
#ifndef WIN32
		pipefn = xstrdup(arg[1]);
#else	/* WIN32 */
		pipefn = xstrdup("\\\\.\\pipe\\upssched");
#endif	/* WIN32 */
		return 1;
	}

	/* LOCKFN <filename> */
	if (!strcmp(arg[0], "LOCKFN")) {
#ifndef WIN32
		lockfn = xstrdup(arg[1]);
#else	/* WIN32 */
		lockfn = filter_path(arg[1]);
#endif	/* WIN32 */
		return 1;
	}

	if (numargs < 5)
		return 0;

	/* AT <notifytype> <upsname> <command> <cmdarg1> [<cmdarg2>] */
	if (!strcmp(arg[0], "AT")) {

		/* don't use arg[5] unless we have it... */
		if (numargs > 5)
			parse_at(arg[1], arg[2], arg[3], arg[4], arg[5]);
		else
			parse_at(arg[1], arg[2], arg[3], arg[4], NULL);

		return 1;
	}

	return 0;
}

/* called for fatal errors in parseconf like malloc failures */
static void upssched_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(upssched.conf): %s", errmsg);
}

static void checkconf(void)
{
	char	fn[NUT_PATH_MAX + 1];
	PCONF_CTX_t	ctx;
	int	numerrors = 0;

	snprintf(fn, sizeof(fn), "%s/upssched.conf", confpath());

	pconf_init(&ctx, upssched_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);
		fatalx(EXIT_FAILURE, "%s", ctx.errmsg);
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			numerrors++;
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!conf_arg(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg),
				"upssched.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s",
					ctx.arglist[i]);

			numerrors++;
			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}


	/* FIXME: Per legacy behavior, we silently went on.
	 * Maybe should abort on unusable configs?
	 */
	if (numerrors) {
		upslogx(LOG_ERR, "Encountered %d config errors, those entries were ignored", numerrors);
	}

	pconf_finish(&ctx);
}

static void help(const char *arg_progname)
	__attribute__((noreturn));

static void help(const char *arg_progname)
{
	printf("upssched: upsmon's scheduling helper for offset timers\n");
	printf("Practical behavior is managed by UPSNAME and NOTIFYTYPE envvars\n");

	printf("\nUsage: %s [OPTIONS]\n\n", arg_progname);
	printf("  -D		raise debugging level (NOTE: keeps reporting when daemonized)\n");
	printf("  -V		display the version of this software\n");
	printf("  -h		display this help\n");

	nut_report_config_flags();

	printf("\n%s", suggest_doc_links(arg_progname, "upsmon.conf"));

	exit(EXIT_SUCCESS);
}


int main(int argc, char **argv)
{
	int i;

	if (argc > 0)
		prog = xbasename(argv[0]);
	if (!prog)
		prog = "upssched";

	while ((i = getopt(argc, argv, "+DVh")) != -1) {
		switch (i) {
			case 'D':
				nut_debug_level++;
				break;

			case 'h':
				help(argv[0]);
#ifndef HAVE___ATTRIBUTE__NORETURN
				break;
#endif

			case 'V':
				/* just show the optional CONFIG_FLAGS banner */
				nut_report_config_flags();
				exit(EXIT_SUCCESS);

			default:
				fatalx(EXIT_FAILURE,
					"Error: unknown option -%c. Try -h for help.",
					(char)i);
		}
	}

	{ /* scoping */
		char *s = getenv("NUT_DEBUG_LEVEL");
		int l;
		if (s && str_to_int(s, &l, 10)) {
			if (l > 0 && nut_debug_level < 1) {
				upslogx(LOG_INFO, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
					"since none was requested by command-line options", l);
				nut_debug_level = l;
			}	/* else follow -D settings */
		}	/* else nothing to bother about */
	}

	/* normally we don't have stderr, so get this going to syslog early */
	open_syslog(prog);
	syslogbit_set();

	upsname = getenv("UPSNAME");
	notify_type = getenv("NOTIFYTYPE");

	if ((!upsname) || (!notify_type)) {
		printf("Error: environment variables UPSNAME and NOTIFYTYPE must be set.\n");
		printf("This program should only be run from upsmon.\n");
		exit(EXIT_FAILURE);
	}

	/* see if this matches anything in the config file */
	/* This is actually the processing loop:
	 * checkconf -> conf_arg -> parse_at -> sendcmd -> daemon if needed
	 *  -> start_daemon -> conn_add(pipefd) or sock_read(conn)
	 */
	checkconf();

	upsdebugx(1, "Exiting upssched (CLI process)");
	exit(EXIT_SUCCESS);
}
