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
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include "upssched.h"
#include "timehead.h"

typedef struct ttype_s {
	char	*name;
	time_t	etime;
	struct ttype_s	*next;
} ttype_t;

	ttype_t	*thead = NULL;
	static	conn_t	*connhead = NULL;
	char	*cmdscript = NULL, *pipefn = NULL, *lockfn = NULL;
	int	verbose = 0;		/* use for debugging */


	/* ups name and notify type (string) as received from upsmon */
	const	char	*upsname, *notify_type;

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

		if (verbose)
			upslogx(LOG_INFO, "Timer queue empty, exiting");

#ifdef UPSSCHED_RACE_TEST
		upslogx(LOG_INFO, "triggering race: sleeping 15 sec before exit");
		sleep(15);
#endif

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
			if (verbose)
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
	int	ofs;
	ttype_t	*tmp, *last;

	/* get the time */
	time(&now);

	/* add an event for <now> + <time> */
	ofs = strtol(ofsstr, (char **) NULL, 10);

	if (ofs < 0) {
		upslogx(LOG_INFO, "bogus offset for timer, ignoring");
		return;
	}

	if (verbose)
		upslogx(LOG_INFO, "New timer: %s (%d seconds)", name, ofs);

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
			if (verbose)
				upslogx(LOG_INFO, "Cancelling timer: %s", name);
			removetimer(tmp);
			return;
		}
	}

	/* this is not necessarily an error */
	if (cname && cname[0]) {
		if (verbose)
			upslogx(LOG_INFO, "Cancel %s, event: %s", name, cname);

		exec_cmd(cname);
	}
}

static void us_serialize(int op)
{
	static	int	pipefd[2];
	int	ret;
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
	}
}

static int open_sock(void)
{
	int	ret, fd;
	struct	sockaddr_un	ssaddr;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
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
	fcntl(fd, F_SETFD, FD_CLOEXEC);

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
	int	ret;
	va_list	ap;
	char	buf[US_SOCK_BUF_LEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	ret = write(conn->fd, buf, strlen(buf));

	if ((ret < 1) || (ret != (int) strlen(buf))) {
		upsdebugx(2, "write to fd %d failed", conn->fd);

		close(conn->fd);
		conn_del(conn);

		return 0;	/* failed */
	}

	return 1;	/* OK */
}

static void conn_add(int sockfd)
{
	int	acc, ret;
	conn_t	*tmp, *last;
	struct	sockaddr_un	saddr;
#if defined(__hpux) && !defined(_XOPEN_SOURCE_EXTENDED)
	int			salen;
#else
	socklen_t	salen;
#endif

	salen = sizeof(saddr);
	acc = accept(sockfd, (struct sockaddr *) &saddr, &salen);

	if (acc < 0) {
		upslog_with_errno(LOG_ERR, "accept on unix fd failed");
		return;
	}

	/* don't leak connection to CMDSCRIPT */
	fcntl(acc, F_SETFD, FD_CLOEXEC);

	/* enable nonblocking I/O */

	ret = fcntl(acc, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl get on unix fd failed");
		close(acc);
		return;
	}

	ret = fcntl(acc, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on unix fd failed");
		close(acc);
		return;
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

static void log_unknown(int numarg, char **arg)
{
	int	i;

	upslogx(LOG_INFO, "Unknown command on socket: ");

	for (i = 0; i < numarg; i++)
		upslogx(LOG_INFO, "arg %d: %s", i, arg[i]);
}

static int sock_read(conn_t *conn)
{
	int	i, ret;
	char	ch;

	for (i = 0; i < US_MAX_READ; i++) {

		ret = read(conn->fd, &ch, 1);

		if (ret < 1) {

			/* short read = no parsing, come back later */
			if ((ret == -1) && (errno == EAGAIN))
				return 0;

			/* some other problem */
			return -1;	/* error */
		}

		ret = pconf_char(&conn->ctx, ch);

		if (ret == 0)		/* nothing to parse yet */
			continue;

		if (ret == -1) {
			upslogx(LOG_NOTICE, "Parse error on sock: %s",
				conn->ctx.errmsg);

			return 0;	/* nothing parsed */
		}

		/* try to use it, and complain about unknown commands */
		if (!sock_arg(conn)) {
			log_unknown(conn->ctx.numargs, conn->ctx.arglist);
			send_to_one(conn, "ERR UNKNOWN\n");
		}

		return 1;	/* we did some work */
	}

	return 0;	/* fell out without parsing anything */
}

static void start_daemon(int lockfd)
{
	int	maxfd, pid, pipefd, ret;
	struct	timeval	tv;
	fd_set	rfds;
	conn_t	*tmp, *tmpnext;

	us_serialize(SERIALIZE_INIT);

	if ((pid = fork()) < 0)
		fatal_with_errno(EXIT_FAILURE, "Unable to enter background");

	if (pid != 0) {		/* parent */

		/* wait for child to set up the listener */
		us_serialize(SERIALIZE_WAIT);

		return;
	}

	/* child */

	close(0);
	close(1);
	close(2);

	/* make fds 0-2 point somewhere defined */
	if (open("/dev/null", O_RDWR) != 0)
		fatal_with_errno(EXIT_FAILURE, "open /dev/null");

	if (dup(0) == -1)
		fatal_with_errno(EXIT_FAILURE, "dup");

	if (dup(0) == -1)
		fatal_with_errno(EXIT_FAILURE, "dup");

	pipefd = open_sock();

	if (verbose)
		upslogx(LOG_INFO, "Timer daemon started");

	/* release the parent */
	us_serialize(SERIALIZE_SET);

	/* drop the lock now that the background is running */
	unlink(lockfn);
	close(lockfd);

	/* now watch for activity */

	for (;;) {
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
					if (sock_read(tmp) < 0) {
						close(tmp->fd);
						conn_del(tmp);
					}
				}

				tmp = tmpnext;
			}
		}

		checktimers();
	}
}

/* --- 'client' functions --- */

static int try_connect(void)
{
	int	pipefd, ret;
	struct	sockaddr_un saddr;

	memset(&saddr, '\0', sizeof(saddr));
	saddr.sun_family = AF_UNIX;
	snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", pipefn);

	pipefd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (pipefd < 0)
		fatal_with_errno(EXIT_FAILURE, "socket");

	ret = connect(pipefd, (const struct sockaddr *) &saddr, sizeof(saddr));

	if (ret != -1)
		return pipefd;

	return -1;
}

static int get_lock(const char *fn)
{
	return open(fn, O_RDONLY | O_CREAT | O_EXCL, 0);

}

/* try to connect to bg process, and start one if necessary */
static int check_parent(const char *cmd, const char *arg2)
{
	int	pipefd, lockfd, tries = 0;

	for (tries = 0; tries < MAX_TRIES; tries++) {

		pipefd = try_connect();

		if (pipefd != -1)
			return pipefd;

		/* timer daemon isn't running */

		/* it's not running, so there's nothing to cancel */
		if (!strcmp(cmd, "CANCEL") && (arg2 == NULL))
			return PARENT_UNNECESSARY;

		/* arg2 non-NULL means there is a cancel action available */

		/* we need to start the daemon, so try to get the lock */

		lockfd = get_lock(lockfn);

		if (lockfd != -1) {
			start_daemon(lockfd);
			return PARENT_STARTED;	/* started successfully */
		}

		/* we didn't get the lock - must be two upsscheds running */

		/* blow this away in case we crashed before */
		unlink(lockfn);

		/* give the other one a chance to start it, then try again */
		usleep(250000);
	}

	upslog_with_errno(LOG_ERR, "Failed to connect to parent and failed to create parent");
	exit(EXIT_FAILURE);
}

static void read_timeout(int sig)
{
	NUT_UNUSED_VARIABLE(sig);

	/* ignore this */
	return;
}

static void setup_sigalrm(void)
{
	struct  sigaction sa;
	sigset_t nut_upssched_sigmask;

	sigemptyset(&nut_upssched_sigmask);
	sa.sa_mask = nut_upssched_sigmask;
	sa.sa_flags = 0;
	sa.sa_handler = read_timeout;
	sigaction(SIGALRM, &sa, NULL);
}

static void sendcmd(const char *cmd, const char *arg1, const char *arg2)
{
	int	i, pipefd, ret;
	char	buf[SMALLBUF], enc[SMALLBUF + 8];

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

	/* see if the parent needs to be started (and maybe start it) */

	for (i = 0; i < MAX_TRIES; i++) {

		pipefd = check_parent(cmd, arg2);

		if (pipefd == PARENT_STARTED) {

			/* loop back and try to connect now */
			usleep(250000);
			continue;
		}

		/* special case for CANCEL when no parent is running */
		if (pipefd == PARENT_UNNECESSARY)
			return;

		/* we're connected now */

		ret = write(pipefd, enc, strlen(enc));

		/* if we can't send the whole thing, loop back and try again */
		if ((ret < 1) || (ret != (int) strlen(enc))) {
			upslogx(LOG_ERR, "write failed, trying again");
			close(pipefd);
			continue;
		}

		/* ugh - probably should use select here... */
		setup_sigalrm();

		alarm(2);
		ret = read(pipefd, buf, sizeof(buf));
		alarm(0);

		signal(SIGALRM, SIG_IGN);

		close(pipefd);

		/* same idea: no OK = go try it all again */
		if (ret < 2) {
			upslogx(LOG_ERR, "read confirmation failed, trying again");
			continue;
		}

		if (!strncmp(buf, "OK", 2))
			return;		/* success */

		upslogx(LOG_ERR, "read confirmation got [%s]", buf);

		/* try again ... */
	}

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
	if (strcmp(upsname, un) != 0)
		if (strcmp(un, "*") != 0)
			return;		/* not for us, and not the wildcard */

	/* see if the current notify type matches the one from the .conf */
	if (strcasecmp(notify_type, ntype) != 0)
		return;

	/* if command is valid, send it to the daemon (which may start it) */

	if (!strcmp(cmd, "START-TIMER")) {
		sendcmd("START", ca1, ca2);
		return;
	}

	if (!strcmp(cmd, "CANCEL-TIMER")) {
		sendcmd("CANCEL", ca1, ca2);
		return;
	}

	if (!strcmp(cmd, "EXECUTE")) {
		if (ca1[0] == '\0') {
			upslogx(LOG_ERR, "Empty EXECUTE command argument");
			return;
		}

		if (verbose)
			upslogx(LOG_INFO, "Executing command: %s", ca1);

		exec_cmd(ca1);
		return;
	}

	upslogx(LOG_ERR, "Invalid command: %s", cmd);
}

static int conf_arg(int numargs, char **arg)
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
		pipefn = xstrdup(arg[1]);
		return 1;
	}

	/* LOCKFN <filename> */
	if (!strcmp(arg[0], "LOCKFN")) {
		lockfn = xstrdup(arg[1]);
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
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

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

			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}

	pconf_finish(&ctx);
}

int main(int argc, char **argv)
{
	const char	*prog = NULL;
	/* More a use for argc to avoid warnings than a real need: */
	if (argc > 0) {
		xbasename(argv[0]);
	} else {
		xbasename("upssched");
	}

	verbose = 1;		/* TODO: remove when done testing, or add -D */

	/* normally we don't have stderr, so get this going to syslog early */
	open_syslog(prog);
	syslogbit_set();

	upsname = getenv("UPSNAME");
	notify_type = getenv("NOTIFYTYPE");

	if ((!upsname) || (!notify_type)) {
		printf("Error: UPSNAME and NOTIFYTYPE must be set.\n");
		printf("This program should only be run from upsmon.\n");
		exit(EXIT_FAILURE);
	}

	/* see if this matches anything in the config file */
	checkconf();

	exit(EXIT_SUCCESS);
}
