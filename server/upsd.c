/* upsd.c - watches ups state files and answers queries 

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "upsd.h"
#include "upstype.h"
#include "conf.h"

#include "netcmds.h"
#include "upsconf.h"

#include <sys/un.h>
#include <sys/socket.h>

#ifdef HAVE_SSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include "user.h"
#include "access.h"
#include "ctype.h"
#include "ssl.h"
#include "sstate.h"
#include "desc.h"
#include "neterr.h"

	/* externally-visible settings and pointers */

	upstype	*firstups = NULL;

	/* default 15 seconds before data is marked stale */
	int	maxage = 15;

	/* preloaded to STATEPATH in main, can be overridden via upsd.conf */
	char	*statepath = NULL;

	/* preloaded to DATADIR in main, can be overridden via upsd.conf */
	char	*datapath = NULL;

	/* everything else */

static	ctype	*firstclient = NULL;

static	int	listenfd, net_port = PORT;

	/* default is to listen on all local interfaces */
static	struct	in_addr	listenaddr;

	/* signal handlers */
static	struct sigaction sa;
static	sigset_t nut_upsd_sigmask;

	/* pid file */
static	char	pidfn[SMALLBUF];

	/* set by signal handlers */
static	int	reload_flag = 0, exit_flag = 0;

/* return a pointer to the named ups if possible */
upstype *get_ups_ptr(const char *name)
{
	upstype	*tmp;

	if (!name)
		return NULL;

	for (tmp = firstups; tmp != NULL; tmp = tmp->next)
		if (!strcasecmp(tmp->name, name))
			return tmp;

	return NULL;
}

/* mark the data stale if this is new, otherwise cleanup any remaining junk */
static void ups_data_stale(upstype *ups)
{
	/* don't complain again if it's already known to be stale */
	if (ups->stale == 1)
		return;

	ups->stale = 1;

	upslogx(LOG_NOTICE, "Data for UPS [%s] is stale - check driver",
		ups->name);
}

/* mark the data ok if this is new, otherwise do nothing */
static void ups_data_ok(upstype *ups)
{
	if (ups->stale == 0)
		return;

	upslogx(LOG_NOTICE, "UPS [%s] data is no longer stale", ups->name);
	ups->stale = 0;
}

/* make sure this UPS is connected and has fresh data */
static void check_ups(upstype *ups)
{
	/* sanity checks */
	if ((!ups) || (!ups->fn))
		return;

	/* see if we need to (re)connect to the socket */
	if (ups->sock_fd == -1)
		ups->sock_fd = sstate_connect(ups);

	/* throw some warnings if it's not feeding us data any more */
	if (sstate_dead(ups, maxage))
		ups_data_stale(ups);
	else
		ups_data_ok(ups);
}

/* create a listening socket for tcp connections */
static void setuptcp(void)
{
	struct	sockaddr_in	server;
	int	res, one = 1;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatal_with_errno("socket");

	res = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void *) &one, 
		sizeof(one));

	if (res != 0)
		fatal_with_errno("setsockopt(SO_REUSEADDR)");

	memset(&server, '\0', sizeof(server));
	server.sin_addr = listenaddr;
	server.sin_family = AF_INET;
	server.sin_port = htons(net_port);

	if (bind(listenfd, (struct sockaddr *) &server, sizeof(server)) == -1)
		fatal_with_errno("Can't bind TCP port number %d", net_port);

	if ((res = fcntl(listenfd, F_GETFL, 0)) == -1)
		fatal_with_errno("fcntl(get)");

	if (fcntl(listenfd, F_SETFL, res | O_NDELAY) == -1)
		fatal_with_errno("fcntl(set)");

	if (listen(listenfd, 16))
		fatal_with_errno("listen");

	return;
}

/* decrement the login counter for this ups */
static void declogins(const char *upsname)
{
	upstype	*ups;

	ups = get_ups_ptr(upsname);

	if (ups == NULL) {
		upslogx(LOG_INFO, "Tried to decrement invalid ups name (%s)", upsname);
		return;
	}

	ups->numlogins--;

	if (ups->numlogins < 0)
		upslogx(LOG_ERR, "Programming error: UPS [%s] has numlogins=%d",
			ups->name, ups->numlogins);
}

/* disconnect a client connection and free all related memory */
static void delclient(ctype *dclient)
{
	ctype	*tmp, *last;

	if (dclient == NULL)
		return;

	last = NULL;
	tmp = firstclient;

	while (tmp != NULL) {
		if (tmp == dclient) {		/* found it */
			shutdown(dclient->fd, 2);
			close(dclient->fd);

			free(tmp->addr);

			if (tmp->loginups != NULL) {
				declogins(tmp->loginups);
				free(tmp->loginups);
			}

			if (tmp->password != NULL)
				free(tmp->password);

#ifdef HAVE_SSL
			if (tmp->ssl)
				SSL_free(tmp->ssl);
#endif

			pconf_finish(&tmp->ctx);

			if (last == NULL)	/* deleting first entry */
				firstclient = tmp->next;
			else
				last->next = tmp->next;

			free(tmp);
			return;
		}

		last = tmp;
		tmp = tmp->next;
	}

	/* not found?! */
	upslogx(LOG_WARNING, "Tried to delete client struct that doesn't exist!");

	return;
}

/* send the buffer <sendbuf> of length <sendlen> to host <dest> */
int sendback(ctype *client, const char *fmt, ...)
{
	int	res, len;
	char ans[NUT_NET_ANSWER_MAX+1];
	va_list ap;

	if (!client)
		return 0;

	if (client->delete)
		return 0;

	va_start(ap, fmt);
	vsnprintf(ans, sizeof(ans), fmt, ap);
	va_end(ap);

	len = strlen(ans);

	upsdebugx(2, "write: [destfd=%d] [len=%d] [%s]", 
		client->fd, len, ans);

	if (client->ssl)
		res = ssl_write(client, ans, len);
	else
		res = write(client->fd, ans, len);

	if (len != res) {
		upslog_with_errno(LOG_NOTICE, "write() failed for %s", client->addr);
		client->delete = 1;
		return 0;	/* failed */
	}

	return 1;	/* OK */
}

/* just a simple wrapper for now */
int send_err(ctype *client, const char *errtype)
{
	if (!client)
		return -1;

	upsdebugx(4, "Sending error [%s] to client %s", 
		errtype, client->addr);

	return sendback(client, "ERR %s\n", errtype);
}

/* disconnect anyone logged into this UPS */
void kick_login_clients(const char *upsname)
{
	ctype   *tmp, *next;

	tmp = firstclient;

	while (tmp) {
		next = tmp->next;

		/* if it's not logged in, don't check it */
		if (!tmp->loginups) {
			tmp = next;
			continue;
		}

		if (!strcmp(tmp->loginups, upsname)) {
			upslogx(LOG_INFO, "Kicking client %s (was on UPS [%s])\n", 
				tmp->addr, upsname);
			delclient(tmp);
		}

		tmp = next;
	}
}

/* make sure a UPS is sane - connected, with fresh data */
int ups_available(const upstype *ups, ctype *client)
{
	if (ups->sock_fd == -1) {
		send_err(client, NUT_ERR_DRIVER_NOT_CONNECTED);
		return 0;
	}

	if (ups->stale) {
		send_err(client, NUT_ERR_DATA_STALE);
		return 0;
	}

	/* must be OK */
	return 1;
}

/* check flags and access for an incoming command from the network */
static void check_command(int cmdnum, ctype *client, int numarg, 
	const char **arg)
{
	if (netcmds[cmdnum].flags & FLAG_USER) {
		if (!client->username) {
			send_err(client, NUT_ERR_USERNAME_REQUIRED);
			return;
		}

		if (!client->password) {
			send_err(client, NUT_ERR_PASSWORD_REQUIRED);
			return;
		}
	}

	/* looks good - call the command */
	netcmds[cmdnum].func(client, numarg - 1, &arg[1]);
}

/* parse requests from the network */
static void parse_net(ctype *client)
{
	int	i;

	/* see if this client is still allowed to talk to us */
	if (!access_check(&client->sock)) {
		send_err(client, NUT_ERR_ACCESS_DENIED);
		client->delete = 1;
		return;
	}

	/* paranoia */
	client->rq[client->rqpos] = '\0';

	/* by this point we should always have a usable line */
	if (pconf_line(&client->ctx, client->rq) != 1) {

		/* shouldn't happen */
		upslogx(LOG_WARNING, "pconf_line couldn't handle the input");
		send_err(client, NUT_ERR_UNKNOWN_COMMAND);
		return;
	}

	if (pconf_parse_error(&client->ctx)) {
		upsdebugx(4, "parse error on net read");
		send_err(client, NUT_ERR_UNKNOWN_COMMAND);
		return;
	}

	/* shouldn't happen */
	if (client->ctx.numargs < 1) {
		send_err(client, NUT_ERR_UNKNOWN_COMMAND);
		return;
	}

	for (i = 0; netcmds[i].name != NULL; i++) {
		if (!strcasecmp(netcmds[i].name, client->ctx.arglist[0])) {
			check_command(i, client, client->ctx.numargs, 
			(const char **) client->ctx.arglist);

			return;
		}
	}

	/* fallthrough = not matched by any entry in netcmds */

	send_err(client, NUT_ERR_UNKNOWN_COMMAND);
}

/* scan the list of UPSes for sanity */
static void check_every_ups(void)
{
	upstype *ups;

	ups = firstups;

	while (ups != NULL) {
		check_ups(ups);
		ups = ups->next;
	}
}

/* answer incoming tcp connections */
static void answertcp(void)
{
	int	acc;
	struct	sockaddr_in csock;
	ctype	*tmp, *last;
	socklen_t	clen;

	clen = sizeof(csock);
	acc = accept(listenfd, (struct sockaddr *) &csock, &clen);

	if (acc < 0)
		return;

	if (!access_check(&csock)) {
		upslogx(LOG_NOTICE, "Rejecting TCP connection from %s", 
			inet_ntoa(csock.sin_addr));
		shutdown(acc, shutdown_how);
		close(acc);
		return;
	}

	last = tmp = firstclient;

	while (tmp != NULL) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ctype));

	tmp->addr = xstrdup(inet_ntoa(csock.sin_addr));
	tmp->fd = acc;
	tmp->delete = 0;
	memcpy(&tmp->sock, &csock, sizeof(struct sockaddr_in));

	tmp->rqpos = 0;
	memset(tmp->rq, '\0', sizeof(tmp->rq));

	pconf_init(&tmp->ctx, NULL);

	tmp->loginups = NULL;		/* for upsmon */
	tmp->username = NULL;
	tmp->password = NULL;

	tmp->ssl = NULL;
	tmp->ssl_connected = 0;

	tmp->next = NULL;

	if (last == NULL)
 		firstclient = tmp;
	else
		last->next = tmp;

	upslogx(LOG_INFO, "Connection from %s", inet_ntoa(csock.sin_addr));
}

/* read tcp messages and handle them */
static void readtcp(ctype *client)
{
	char	buf[SMALLBUF];
	int	i, ret;

	memset(buf, '\0', sizeof(buf));

	if (client->ssl)
		ret = ssl_read(client, buf, sizeof(buf));
	else
		ret = read(client->fd, buf, sizeof(buf));

	if (ret < 1) {
		upslogx(LOG_INFO, "Host %s disconnected (read failure)",
			client->addr);
		delclient(client);
		return;
	}

	/* if an overflow will happen, then clear the queue */
	if ((ret + client->rqpos) >= sizeof(client->rq)) {
		memset(client->rq, '\0', sizeof(client->rq));
		client->rqpos = 0;
	}

	/* fragment handling code */

	for (i = 0; i < ret; i++) {
		/* add to the receive queue one by one */
		client->rq[client->rqpos++] = buf[i];

		/* parse on linefeed ("blah blah\n") */
		if (buf[i] == 10) {	/* LF */
			parse_net(client);

			/* bail out if the connection closed in parse_net */
			if (client->delete) {
				delclient(client);
				return;
			}
			
			/* reset queue */
			client->rqpos = 0;
			memset(client->rq, '\0', sizeof(client->rq));
		}
	}

	return;
}

static void upsd_cleanup(void)
{
	ctype	*tmpcli, *tmpnext;
	upstype	*ups, *unext;

	/* cleanup client fds */
	tmpcli = firstclient;

	while (tmpcli) {
		tmpnext = tmpcli->next;
		delclient(tmpcli);
		tmpcli = tmpnext;
	}

	if (strcmp(pidfn, "") != 0)
		unlink(pidfn);

	ups = firstups;

	while (ups) {
		unext = ups->next;

		if (ups->sock_fd != -1)
			close(ups->sock_fd);

		sstate_infofree(ups);
		sstate_cmdfree(ups);
		pconf_finish(&ups->sock_ctx);

		if (ups->fn)
			free(ups->fn);
		if (ups->name)
			free(ups->name);
		if (ups->desc)
			free(ups->desc);
		free(ups);

		ups = unext;
	}

	/* dump everything */

	acl_free();
	access_free();
	user_flush();
	desc_free();

	if (statepath)
		free(statepath);
	if (datapath)
		free(datapath);
	if (certfile)
		free(certfile);
}

/* service requests and check on new data */
static void mainloop(void)
{
	fd_set	rfds;
	struct	timeval	tv;
	int	res, maxfd;
	ctype	*tmpcli, *tmpnext;
	upstype	*utmp, *unext;

	if (reload_flag) {
		conf_reload();
		reload_flag = 0;
	}

	FD_ZERO(&rfds);
	FD_SET(listenfd, &rfds);
	
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	maxfd = listenfd;

	/* scan through clients and add to FD_SET */
	for (tmpcli = firstclient; tmpcli != NULL; tmpcli = tmpcli->next) {
		FD_SET(tmpcli->fd, &rfds);
		if (tmpcli->fd > maxfd)
			maxfd = tmpcli->fd;
	}

	/* also add new driver sockets */
	for (utmp = firstups; utmp != NULL; utmp = utmp->next) {
		if (utmp->sock_fd != -1) {
			FD_SET(utmp->sock_fd, &rfds);

			if (utmp->sock_fd > maxfd)
				maxfd = utmp->sock_fd;
		}
	}
	
	res = select(maxfd + 1, &rfds, NULL, NULL, &tv);

	if (res > 0) {
		if (FD_ISSET(listenfd, &rfds))
			answertcp();

		/* scan clients for activity */

		tmpcli = firstclient;

		while (tmpcli != NULL) {

			/* preserve for later since delclient may run */
			tmpnext = tmpcli->next;

			if (FD_ISSET(tmpcli->fd, &rfds))
				readtcp(tmpcli);

			tmpcli = tmpnext;
		}

		/* now scan ups sockets for activity */
		utmp = firstups;

		while (utmp) {
			unext = utmp->next;

			if (utmp->sock_fd != -1)
				if (FD_ISSET(utmp->sock_fd, &rfds))
					sstate_sock_read(utmp);

			utmp = unext;
		}
	}

	check_every_ups();
}

static void help(const char *progname) 
{
	printf("Network server for UPS data.\n\n");
	printf("usage: %s [OPTIONS]\n", progname);

	printf("\n");
	printf("  -c <command>	send <command> via signal to background process\n");
	printf("		commands:\n");
	printf("		 - reload: reread configuration files\n");
	printf("		 - stop: stop process and exit\n");
	printf("  -D		raise debugging level\n");
	printf("  -f		stay in the foreground for testing\n");
	printf("  -h		display this help\n");
	printf("  -i <address>	binds to interface <address>\n");
	printf("  -p <port>	sets network port (default: TCP port %d)\n", 
		PORT);
	printf("  -r <dir>	chroots to <dir>\n");
	printf("  -u <user>	switch to <user> (if started as root)\n");
	printf("  -V		display the version of this software\n");

	exit(EXIT_SUCCESS);
}

static void set_reload_flag(int sig)
{
	reload_flag = 1;
}

static void set_exit_flag(int sig)
{
	exit_flag = sig;
}

/* basic signal setup to ignore SIGPIPE */
static void setupsignals(void)
{
	sigemptyset(&nut_upsd_sigmask);
	sa.sa_mask = nut_upsd_sigmask;
	sa.sa_flags = 0;

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);

	/* handle shutdown signals */
	sa.sa_handler = set_exit_flag;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* handle reloading */
	sa.sa_handler = set_reload_flag;
	sigaction(SIGHUP, &sa, NULL);
}

/* try not to exit before the DUMPDONE hits, so clients work on the first try */
static void initial_dump_wait(void)
{
	int	maxfd, ret, numups, numdone;
	fd_set	rfds;
	time_t	start, now;
	struct	timeval	tv;
	upstype	*utmp, *unext;

	printf("Synchronizing...");
	fflush(stdout);

	time(&start);
	time(&now);

	while (difftime(now, start) < INITIAL_WAIT_MAX) {

		/* check this now in case the user is trying to ^C us */
		if (exit_flag) {
			upsd_cleanup();
			exit(EXIT_FAILURE);
		}

		maxfd = 0;
		numups = 0;
		numdone = 0;

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		FD_ZERO(&rfds);

		for (utmp = firstups; utmp != NULL; utmp = utmp->next) {
			if (utmp->sock_fd != -1) {
				FD_SET(utmp->sock_fd, &rfds);

				if (utmp->sock_fd > maxfd)
					maxfd = utmp->sock_fd;

				numups++;
			}
		}

		ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

		if (ret < 1) {
			printf(".");
			fflush(stdout);

			time(&now);
			continue;
		}

		utmp = firstups;
		while (utmp) {
			unext = utmp->next;

			if (utmp->sock_fd != -1) {
				if (FD_ISSET(utmp->sock_fd, &rfds))
					sstate_sock_read(utmp);

				if (utmp->dumpdone == 1)
					numdone++;
			}

			utmp = unext;
		}

		/* if they're all done, then exit early */
		if (numdone >= numups) {
			printf("done\n");
			fflush(stdout);
			return;
		}

		time(&now);
	}

	printf(" giving up\n");
	fflush(stdout);
}

void check_perms(const char *fn)
{
	int	ret;
	struct stat	st;

	ret = stat(fn, &st);

	if (ret != 0)
		fatal_with_errno("stat %s", fn);

	/* include the x bit here in case we check a directory */
	if (st.st_mode & (S_IROTH | S_IXOTH))
		upslogx(LOG_WARNING, "%s is world readable", fn);
}	

int main(int argc, char **argv)
{
	int	i, cmd = 0;
	const char *nut_statepath_env = getenv("NUT_STATEPATH");
	int	do_background = 1;
	const	char *user = NULL;
	char	*progname, *chroot_path = NULL;
	struct	passwd	*new_uid = NULL;

	progname = argv[0];

	/* pick up a default from configure --with-user */
	user = RUN_AS_USER;

	/* yes, xstrdup - the conf handlers call free on this later */
	statepath = xstrdup(STATEPATH);
	datapath = xstrdup(DATADIR);

	/* set up some things for later */

	listenaddr.s_addr = INADDR_ANY;
	snprintf(pidfn, sizeof(pidfn), "%s/upsd.pid", altpidpath());

	printf("Network UPS Tools upsd %s\n", UPS_VERSION);

	while ((i = getopt(argc, argv, "+hp:r:i:fu:Vc:D")) != EOF) {
		switch (i) {
			case 'h':
				help(progname);
				break;
			case 'p':
				net_port = atoi(optarg);
				break;
			case 'i':
				if (!inet_aton(optarg, &listenaddr))
					fatal_with_errno("Invalid IP address");
				break;
			case 'r':
				chroot_path = optarg;
				break;
			case 'f':
				do_background = 0;
				break;
			case 'u':
				user = optarg;
				break;
			case 'V':

				/* do nothing - we already printed the banner */
				exit(EXIT_SUCCESS);

			case 'c':
				if (!strncmp(optarg, "reload", strlen(optarg)))
					cmd = SIGCMD_RELOAD;
				if (!strncmp(optarg, "stop", strlen(optarg)))
					cmd = SIGCMD_STOP;

				/* bad command given */
				if (cmd == 0)
					help(progname);
				break;

			case 'D':
				do_background = 0;
				nut_debug_level++;
				break;
			default:
				help(progname);
				break;
		}
	}

	if (cmd) {
		sendsignalfn(pidfn, cmd);
		exit(EXIT_SUCCESS);
	}

	argc -= optind;
	argv += optind;

	if (argc != 0)
		help(progname);

	setupsignals();
	setuptcp();

	open_syslog("upsd");

	/* send logging to the syslog pre-background for later use */
	syslogbit_set();

	if (nut_statepath_env)
		statepath = xstrdup(nut_statepath_env);

	/* do this here, since getpwnam() might not work in the chroot */
	new_uid = get_user_pwent(user);

	if (chroot_path)
		chroot_start(chroot_path);

	become_user(new_uid);

	if (chdir(statepath))
		fatal_with_errno("Can't chdir to %s", statepath);

	/* check statepath perms */
	check_perms(statepath);

	/* read data from config files - upsd.conf, ups.conf, upsd.users */
	conf_load();

	if (num_ups == 0)
		fatalx("Fatal error: at least one UPS must be defined in ups.conf");

	ssl_init();

	/* try to bring in the var/cmd descriptions */
	desc_load();

	/* try to get a full dump for each UPS before exiting */
	initial_dump_wait();

	/* this is after the uid change to detect permission problems */
	check_every_ups();

	if (do_background) {
		background();

		writepid(pidfn);
	} else {
		memset(&pidfn, '\0', sizeof(pidfn));
	}

	while (exit_flag == 0)
		mainloop();

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
	upsd_cleanup();

	exit(EXIT_SUCCESS);
}
