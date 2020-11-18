/* dstate.c - Network UPS Tools driver-side state management

   Copyright (C)
	2003		Russell Kroll <rkroll@exploits.org>
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>
	2012 - 2017	Arnaud Quette <arnaud.quette@free.fr>

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

#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "dstate.h"
#include "state.h"
#include "parseconf.h"
#include "attribute.h"

	static int	sockfd = -1, stale = 1, alarm_active = 0, ignorelb = 0;
	static char	*sockfn = NULL;
	static char	status_buf[ST_MAX_VALUE_LEN], alarm_buf[LARGEBUF];
	static st_tree_t	*dtree_root = NULL;
	static conn_t	*connhead = NULL;
	static cmdlist_t *cmdhead = NULL;

	struct ups_handler	upsh;

/* this may be a frequent stumbling point for new users, so be verbose here */
static void sock_fail(const char *fn)
	__attribute__((noreturn));

static void sock_fail(const char *fn)
{
	int	sockerr;
	struct passwd	*user;

	/* save this so it doesn't get overwritten */
	sockerr = errno;

	/* dispense with the usual upslog stuff since we have stderr here */

	printf("\nFatal error: unable to create listener socket\n\n");
	printf("bind %s failed: %s\n", fn, strerror(sockerr));

	user = getpwuid(getuid());

	if (!user) {
		fatal_with_errno(EXIT_FAILURE, "getpwuid");
	}

	/* deal with some common problems */
	switch (errno)
	{
	case EACCES:
		printf("\nCurrent user: %s (UID %d)\n\n",
			user->pw_name, (int)user->pw_uid);

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
	}

	/*
	 * there - that wasn't so bad.  every helpful line of code here
	 * prevents one more "help me" mail to the list a year from now
	 */

	printf("\n");
	fatalx(EXIT_FAILURE, "Exiting.");
}

static int sock_open(const char *fn)
{
	int	ret, fd;
	struct sockaddr_un	ssaddr;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0) {
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

	return fd;
}

static void sock_disconnect(conn_t *conn)
{
	close(conn->fd);

	pconf_finish(&conn->ctx);

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

	free(conn);
}

static void send_to_all(const char *fmt, ...)
{
	int	ret;
	char	buf[ST_SOCK_BUF_LEN];
	va_list	ap;
	conn_t	*conn, *cnext;

	va_start(ap, fmt);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	if (ret < 1) {
		upsdebugx(2, "%s: nothing to write", __func__);
		return;
	}

	upsdebugx(5, "%s: %.*s", __func__, ret-1, buf);

	for (conn = connhead; conn; conn = cnext) {
		cnext = conn->next;

		ret = write(conn->fd, buf, strlen(buf));

		if (ret != (int)strlen(buf)) {
			upsdebugx(1, "write %d bytes to socket %d failed", (int)strlen(buf), conn->fd);
			sock_disconnect(conn);
		}
	}
}

static int send_to_one(conn_t *conn, const char *fmt, ...)
{
	int	ret;
	va_list	ap;
	char	buf[ST_SOCK_BUF_LEN];

	va_start(ap, fmt);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	upsdebugx(2, "%s: sending %.*s", __func__, (int)strcspn(buf, "\n"), buf);
	if (ret < 1) {
		upsdebugx(2, "%s: nothing to write", __func__);
		return 1;
	}

	upsdebugx(5, "%s: %.*s", __func__, ret-1, buf);

	ret = write(conn->fd, buf, strlen(buf));

	if (ret != (int)strlen(buf)) {
		upsdebugx(1, "write %d bytes to socket %d failed", (int)strlen(buf), conn->fd);
		sock_disconnect(conn);
		return 0;	/* failed */
	}

	return 1;	/* OK */
}

static void sock_connect(int sock)
{
	int	fd, ret;
	conn_t	*conn;
	struct sockaddr_un sa;
#if defined(__hpux) && !defined(_XOPEN_SOURCE_EXTENDED)
	int	salen;
#else
	socklen_t	salen;
#endif
	salen = sizeof(sa);
	fd = accept(sock, (struct sockaddr *) &sa, &salen);

	if (fd < 0) {
		upslog_with_errno(LOG_ERR, "accept on unix fd failed");
		return;
	}

	/* enable nonblocking I/O */
	if (!do_synchronous) {
		ret = fcntl(fd, F_GETFL, 0);

		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "fcntl get on unix fd failed");
			close(fd);
			return;
		}

		ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on unix fd failed");
			close(fd);
			return;
		}
	}

	conn = xcalloc(1, sizeof(*conn));
	conn->fd = fd;

	pconf_init(&conn->ctx, NULL);

	if (connhead) {
		conn->next = connhead;
		connhead->prev = conn;
	}

	connhead = conn;

	upsdebugx(3, "new connection on fd %d", fd);
}

static int st_tree_dump_conn(st_tree_t *node, conn_t *conn)
{
	int	ret;
	enum_t	*etmp;
	range_t	*rtmp;

	if (!node) {
		return 1;	/* not an error */
	}

	if (node->left) {
		ret = st_tree_dump_conn(node->left, conn);

		if (!ret) {
			return 0;	/* write failed in the child */
		}
	}

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

static int sock_arg(conn_t *conn, int numarg, char **arg)
{
	if (numarg < 1) {
		return 0;
	}

	if (!strcasecmp(arg[0], "DUMPALL")) {

		/* first thing: the staleness flag */
		if ((stale == 1) && !send_to_one(conn, "DATASTALE\n")) {
			return 1;
		}

		if (!st_tree_dump_conn(dtree_root, conn)) {
			return 1;
		}

		if (!cmd_dump_conn(conn)) {
			return 1;
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

	if (numarg < 2) {
		return 0;
	}

	/* INSTCMD <cmdname> [<cmdparam>] [TRACKING <id>] */
	if (!strcasecmp(arg[0], "INSTCMD")) {
		int ret;
		char *cmdname = arg[1];
		char *cmdparam = NULL;
		char *cmdid = NULL;

		/* Check if <cmdparam> and TRACKING were provided */
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

		/* try the new handler first if present */
		if (upsh.instcmd) {
			ret = upsh.instcmd(cmdname, cmdparam);

			/* send back execution result */
			if (cmdid)
				send_tracking(conn, cmdid, ret);

			/* The command was handled, status is a separate consideration */
			return 1;
		}
		upslogx(LOG_NOTICE, "Got INSTCMD, but driver lacks a handler");
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

		/* try the new handler first if present */
		if (upsh.setvar) {
			ret = upsh.setvar(arg[1], arg[2]);

			/* send back execution result */
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
	int	i, ret;
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

	for (i = 0; i < ret; i++) {

		switch(pconf_char(&conn->ctx, buf[i]))
		{
		case 0: /* nothing to parse yet */
			continue;

		case 1: /* try to use it, and complain about unknown commands */
			if (!sock_arg(conn, conn->ctx.numargs, conn->ctx.arglist)) {
				size_t	arg;

				upslogx(LOG_INFO, "Unknown command on socket: ");

				for (arg = 0; arg < conn->ctx.numargs; arg++) {
					upslogx(LOG_INFO, "arg %d: %s", (int)arg, conn->ctx.arglist[arg]);
				}
			}
			continue;

		default: /* nothing parsed */
			upslogx(LOG_NOTICE, "Parse error on sock: %s", conn->ctx.errmsg);
			return;
		}
	}
}

static void sock_close(void)
{
	conn_t	*conn, *cnext;

	if (sockfd != -1) {
		close(sockfd);
		sockfd = -1;

		if (sockfn) {
			unlink(sockfn);
			free(sockfn);
			sockfn = NULL;
		}
	}

	for (conn = connhead; conn; conn = cnext) {
		cnext = conn->next;
		sock_disconnect(conn);
	}

	connhead = NULL;
	/* conntail = NULL; */
}

/* interface */

void dstate_init(const char *prog, const char *devname)
{
	char	sockname[SMALLBUF];

	/* do this here for now */
	signal(SIGPIPE, SIG_IGN);

	if (devname) {
		snprintf(sockname, sizeof(sockname), "%s/%s-%s", dflt_statepath(), prog, devname);
	} else {
		snprintf(sockname, sizeof(sockname), "%s/%s", dflt_statepath(), prog);
	}

	sockfd = sock_open(sockname);

	upsdebugx(2, "dstate_init: sock %s open on fd %d", sockname, sockfd);
}

/* returns 1 if timeout expired or data is available on UPS fd, 0 otherwise */
int dstate_poll_fds(struct timeval timeout, int extrafd)
{
	int	ret, maxfd, overrun = 0;
	fd_set	rfds;
	struct timeval	now;
	conn_t	*conn, *cnext;

	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);

	maxfd = sockfd;

	if (extrafd != -1) {
		FD_SET(extrafd, &rfds);

		if (extrafd > maxfd) {
			maxfd = extrafd;
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
			upslog_with_errno(LOG_ERR, "select unix sockets failed");
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

	/* tell the caller if that fd woke up */
	if ((extrafd != -1) && (FD_ISSET(extrafd, &rfds))) {
		return 1;
	}

	return overrun;
}

int dstate_setinfo(const char *var, const char *fmt, ...)
{
	int	ret;
	char	value[ST_MAX_VALUE_LEN];
	va_list	ap;

	va_start(ap, fmt);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vsnprintf(value, sizeof(value), fmt, ap);
#if defined (__GNUC__) || defined (__clang__)
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
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vsnprintf(value, sizeof(value), fmt, ap);
#if defined (__GNUC__) || defined (__clang__)
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
		upslogx(LOG_ERR, "dstate_setaux: base variable (%s) does not exist", var);
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
}

/* add a status element */
void status_set(const char *buf)
{
	if (ignorelb && !strcasecmp(buf, "LB")) {
		upsdebugx(2, "%s: ignoring LB flag from device", __func__);
		return;
	}

	/* separate with a space if multiple elements are present */
	if (strlen(status_buf) > 0) {
		snprintfcat(status_buf, sizeof(status_buf), " %s", buf);
	} else {
		snprintfcat(status_buf, sizeof(status_buf), "%s", buf);
	}
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

	if (alarm_active) {
		dstate_setinfo("ups.status", "ALARM %s", status_buf);
	} else {
		dstate_setinfo("ups.status", "%s", status_buf);
	}
}

/* similar handlers for ups.alarm */

void alarm_init(void)
{
	/* reinit global counter */
	alarm_active = 0;

	device_alarm_init();
}

void alarm_set(const char *buf)
{
	if (strlen(alarm_buf) > 0) {
		snprintfcat(alarm_buf, sizeof(alarm_buf), " %s", buf);
	} else {
		snprintfcat(alarm_buf, sizeof(alarm_buf), "%s", buf);
	}
}

/* write the status_buf into the info array */
void alarm_commit(void)
{
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
		int bufrw_max;
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
		{ strncpy(bufrw_ptr, suffix, bufrw_max); \
		  if ( (var = dstate_getinfo(buf)) ) { \
		    if ( (var[0] == '0' && var[1] == '\0') || \
		         (var[0] == '\0') ) { \
		      var = NULL; \
		    } \
		  } \
		} ;

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
static int dstate_tree_dump(st_tree_t *node)
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
	upsdebugx(3, "Entering %s", __func__);

	st_tree_t *node = (st_tree_t *)dstate_getroot();

	dstate_tree_dump(node);
}
