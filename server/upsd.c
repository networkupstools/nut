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
#include <netdb.h>

#ifdef HAVE_SSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include "user.h"
#include "access.h"
#include "ctype.h"
#include "stype.h"
#include "ssl.h"
#include "sstate.h"
#include "desc.h"
#include "neterr.h"

	/* externally-visible settings and pointers */

	upstype_t	*firstups = NULL;

	/* default 15 seconds before data is marked stale */
	int	maxage = 15;

	/* preloaded to STATEPATH in main, can be overridden via upsd.conf */
	char	*statepath = NULL;

	/* preloaded to DATADIR in main, can be overridden via upsd.conf */
	char	*datapath = NULL;

	/* everything else */

static ctype_t	*firstclient = NULL;

	/* default is to listen on all local interfaces */
static stype_t	*firstaddr = NULL;

#ifdef	HAVE_IPV6
static int 	opt_af = AF_UNSPEC;
#endif

	/* signal handlers */
static struct sigaction	sa;
static sigset_t	nut_upsd_sigmask;

	/* pid file */
static	char	pidfn[SMALLBUF];

	/* set by signal handlers */
static	int	reload_flag = 0, exit_flag = 0;

#ifdef	HAVE_IPV6
static const char *inet_ntopW (struct sockaddr_storage *s) {
	static char str[40];

	switch (s->ss_family) {
	case AF_INET:
		return inet_ntop (AF_INET, &(((struct sockaddr_in *)s)->sin_addr), str, 16);
	case AF_INET6:
		return inet_ntop (AF_INET6, &(((struct sockaddr_in6 *)s)->sin6_addr), str, 40);
	default:
		errno = EAFNOSUPPORT;
		return NULL;
	}
}
#endif

/* return a pointer to the named ups if possible */
upstype_t *get_ups_ptr(const char *name)
{
	upstype_t	*tmp;

	if (!name) {
		return NULL;
	}

	for (tmp = firstups; tmp != NULL; tmp = tmp->next) {
		if (!strcasecmp(tmp->name, name)) {
			return tmp;
		}
	}

	return NULL;
}

/* mark the data stale if this is new, otherwise cleanup any remaining junk */
static void ups_data_stale(upstype_t *ups)
{
	/* don't complain again if it's already known to be stale */
	if (ups->stale == 1) {
		return;
	}

	ups->stale = 1;

	upslogx(LOG_NOTICE, "Data for UPS [%s] is stale - check driver", ups->name);
}

/* mark the data ok if this is new, otherwise do nothing */
static void ups_data_ok(upstype_t *ups)
{
	if (ups->stale == 0) {
		return;
	}

	upslogx(LOG_NOTICE, "UPS [%s] data is no longer stale", ups->name);
	ups->stale = 0;
}

/* make sure this UPS is connected and has fresh data */
static void check_ups(upstype_t *ups)
{
	/* sanity checks */
	if ((!ups) || (!ups->fn)) {
		return;
	}

	/* see if we need to (re)connect to the socket */
	if (ups->sock_fd == -1) {
		ups->sock_fd = sstate_connect(ups);
	}

	/* throw some warnings if it's not feeding us data any more */
	if (sstate_dead(ups, maxage)) {
		ups_data_stale(ups);
	} else {
		ups_data_ok(ups);
	}
}

/* add another listening address */
void listen_add(const char *addr, const char *port)
{
	stype_t	*stmp, *last;

	/* don't change listening addresses on reload */
	if (reload_flag) {
		return;
	}

	stmp = last = firstaddr;

	/* find end of linked list */
	while (stmp != NULL) {
		last = stmp;
		stmp = stmp->next;
	}

	/* grab some memory and add the info */
	stmp = xmalloc(sizeof(stype_t));
	stmp->addr = xstrdup(addr);
	stmp->port = xstrdup(port);
	stmp->sock_fd = -1;
	stmp->next = NULL;

	if (last == NULL) {
		firstaddr = stmp;
	} else {
		last->next = stmp;
	}

	upsdebugx(3, "listen_add: added %s:%s", stmp->addr, stmp->port);
}

/* create a listening socket for tcp connections */
static void setuptcp(stype_t *serv)
{
#ifndef	HAVE_IPV6
	struct hostent		*host;
	struct sockaddr_in	server;
	int	res, one = 1;

	host = gethostbyname(serv->addr);

	if (host == NULL) {
		struct  in_addr	listenaddr;

		if (!inet_aton(serv->addr, &listenaddr)) {
			fatal_with_errno(EXIT_FAILURE, "inet_aton");
		}

		host = gethostbyaddr(&listenaddr, sizeof(listenaddr), AF_INET);

		if (host == NULL) {
			fatal_with_errno(EXIT_FAILURE, "gethostbyaddr");
		}
	}

	if ((serv->sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fatal_with_errno(EXIT_FAILURE, "socket");
	}

	res = setsockopt(serv->sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &one, sizeof(one));

	if (res != 0) {
		fatal_with_errno(EXIT_FAILURE, "setsockopt(SO_REUSEADDR)");
	}

	memset(&server, '\0', sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(serv->port));

	memcpy(&server.sin_addr, host->h_addr, host->h_length);

	if (bind(serv->sock_fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
		fatal_with_errno(EXIT_FAILURE, "Can't bind TCP port %s", serv->port);
	}

	if ((res = fcntl(serv->sock_fd, F_GETFL, 0)) == -1) {
		fatal_with_errno(EXIT_FAILURE, "fcntl(get)");
	}

	if (fcntl(serv->sock_fd, F_SETFL, res | O_NDELAY) == -1) {
		fatal_with_errno(EXIT_FAILURE, "fcntl(set)");
	}

	if (listen(serv->sock_fd, 16)) {
		fatal_with_errno(EXIT_FAILURE, "listen");
	}
#else
	struct addrinfo		hints, *res, *ai;
	int	v = 0, one = 1;

	upsdebugx(3, "setuptcp: try to bind to %s port %s", serv->addr, serv->port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_PASSIVE;
	hints.ai_family		= opt_af;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_protocol	= IPPROTO_TCP;

        if ((v = getaddrinfo(serv->addr, serv->port, &hints, &res)) != 0) {
		if (v == EAI_SYSTEM) {
                        fatal_with_errno(EXIT_FAILURE, "getaddrinfo");
		}

                fatalx(EXIT_FAILURE, "getaddrinfo: %s", gai_strerror(v));
        }

       for (ai = res; ai != NULL; ai = ai->ai_next) {
		int sock_fd;

		if ((sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0) {
			upsdebug_with_errno(3, "setuptcp: socket");
			continue;
		}
		
		if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one)) != 0) {
			fatal_with_errno(EXIT_FAILURE, "setuptcp: setsockopt");
		}

		if (bind(sock_fd, ai->ai_addr, ai->ai_addrlen) < 0) {
			upsdebug_with_errno(3, "setuptcp: bind");
			close(sock_fd);
			continue;
		}

		if ((v = fcntl(sock_fd, F_GETFL, 0)) == -1) {
			fatal_with_errno(EXIT_FAILURE, "setuptcp: fcntl(get)");
		}

		if (fcntl(sock_fd, F_SETFL, v | O_NDELAY) == -1) {
			fatal_with_errno(EXIT_FAILURE, "setuptcp: fcntl(set)");
		}

		if (listen(sock_fd, 16) < 0) {
			upsdebug_with_errno(3, "setuptcp: listen");
			close(sock_fd);
			continue;
		}

		serv->sock_fd = sock_fd;
		break;
	}

	freeaddrinfo(res);
#endif

	/* don't fail silently */
	if (serv->sock_fd < 0) {
		fatalx(EXIT_FAILURE, "not listening on %s port %s", serv->addr, serv->port);
	} else {
		upslogx(LOG_INFO, "listening on %s port %s", serv->addr, serv->port);
	}

	return;
}

/* decrement the login counter for this ups */
static void declogins(const char *upsname)
{
	upstype_t	*ups;

	ups = get_ups_ptr(upsname);

	if (ups == NULL) {
		upslogx(LOG_INFO, "Tried to decrement invalid ups name (%s)", upsname);
		return;
	}

	ups->numlogins--;

	if (ups->numlogins < 0) {
		upslogx(LOG_ERR, "Programming error: UPS [%s] has numlogins=%d", ups->name, ups->numlogins);
	}
}

/* disconnect a client connection and free all related memory */
static void delclient(ctype_t *dclient)
{
	ctype_t	*tmp, *last;

	if (dclient == NULL) {
		return;
	}

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

			free(tmp->username);
			free(tmp->password);

#ifdef HAVE_SSL
			if (tmp->ssl) {
				SSL_free(tmp->ssl);
			}
#endif

			pconf_finish(&tmp->ctx);

			if (last == NULL) {	/* deleting first entry */
				firstclient = tmp->next;
			} else {
				last->next = tmp->next;
			}

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
int sendback(ctype_t *client, const char *fmt, ...)
{
	int	res, len;
	char ans[NUT_NET_ANSWER_MAX+1];
	va_list ap;

	if (!client) {
		return 0;
	}

	if (client->delete) {
		return 0;
	}

	va_start(ap, fmt);
	vsnprintf(ans, sizeof(ans), fmt, ap);
	va_end(ap);

	len = strlen(ans);

	if (client->ssl) {
		res = ssl_write(client, ans, len);
	} else {
		res = write(client->fd, ans, len);
	}

	upsdebugx(2, "write: [destfd=%d] [len=%d] [%s]", client->fd, len, rtrim(ans, '\n'));

	if (len != res) {
		upslog_with_errno(LOG_NOTICE, "write() failed for %s", client->addr);
		client->delete = 1;
		return 0;	/* failed */
	}

	return 1;	/* OK */
}

/* just a simple wrapper for now */
int send_err(ctype_t *client, const char *errtype)
{
	if (!client) {
		return -1;
	}

	upsdebugx(4, "Sending error [%s] to client %s", errtype, client->addr);

	return sendback(client, "ERR %s\n", errtype);
}

/* disconnect anyone logged into this UPS */
void kick_login_clients(const char *upsname)
{
	ctype_t   *tmp, *next;

	tmp = firstclient;

	while (tmp) {
		next = tmp->next;

		/* if it's not logged in, don't check it */
		if (!tmp->loginups) {
			tmp = next;
			continue;
		}

		if (!strcmp(tmp->loginups, upsname)) {
			upslogx(LOG_INFO, "Kicking client %s (was on UPS [%s])\n", tmp->addr, upsname);
			delclient(tmp);
		}

		tmp = next;
	}
}

/* make sure a UPS is sane - connected, with fresh data */
int ups_available(const upstype_t *ups, ctype_t *client)
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
static void check_command(int cmdnum, ctype_t *client, int numarg, 
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
static void parse_net(ctype_t *client)
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
	upstype_t *ups;

	ups = firstups;

	while (ups != NULL) {
		check_ups(ups);
		ups = ups->next;
	}
}

/* answer incoming tcp connections */
static void answertcp(stype_t *serv)
#ifndef	HAVE_IPV6
{
	struct	sockaddr_in csock;
#else
{
	struct	sockaddr_storage csock;
#endif
	int	acc;
	ctype_t	*tmp, *last;
	socklen_t	clen;

	clen = sizeof(csock);
	acc = accept(serv->sock_fd, (struct sockaddr *) &csock, &clen);

	if (acc < 0) {
		return;
	}

	if (!access_check(&csock)) {
		upslogx(LOG_NOTICE, "Rejecting TCP connection from %s", 
#ifndef	HAVE_IPV6
	 		inet_ntoa(csock.sin_addr));
#else
			inet_ntopW(&csock));
#endif
		shutdown(acc, shutdown_how);
		close(acc);
		return;
	}

	last = tmp = firstclient;

	while (tmp != NULL) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ctype_t));

	tmp->fd = acc;
	tmp->delete = 0;

#ifndef	HAVE_IPV6
	tmp->addr = xstrdup(inet_ntoa(csock.sin_addr));
	memcpy(&tmp->sock, &csock, sizeof(struct sockaddr_in));
#else
	tmp->addr = xstrdup(inet_ntopW(&csock));
	memcpy(&tmp->sock, &csock, sizeof(struct sockaddr_storage));
#endif

	tmp->rqpos = 0;
	memset(tmp->rq, '\0', sizeof(tmp->rq));

	pconf_init(&tmp->ctx, NULL);

	tmp->loginups = NULL;		/* for upsmon */
	tmp->username = NULL;
	tmp->password = NULL;

	tmp->ssl = NULL;
	tmp->ssl_connected = 0;

	tmp->next = NULL;

	if (last == NULL) {
 		firstclient = tmp;
	} else {
		last->next = tmp;
	}

	upslogx(LOG_DEBUG, "Connection from %s", tmp->addr);
}

/* read tcp messages and handle them */
static void readtcp(ctype_t *client)
{
	char	buf[SMALLBUF];
	int	i, ret;

	memset(buf, '\0', sizeof(buf));

	if (client->ssl) {
		ret = ssl_read(client, buf, sizeof(buf));
	} else {
		ret = read(client->fd, buf, sizeof(buf));
	}

	if (ret < 1) {
		upslogx(LOG_INFO, "Host %s disconnected (read failure)", client->addr);
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

void server_load(void)
{
	stype_t	*serv;

	/* default behaviour if no LISTEN addres has been specified */
	if (firstaddr == NULL) {
		listen_add("0.0.0.0", string_const(PORT));
	}

	for (serv = firstaddr; serv != NULL; serv = serv->next) {
		setuptcp(serv);
	}
}

void server_free(void)
{
	stype_t	*stmp, *snext;

	/* cleanup server fds */
	stmp = firstaddr;

	while (stmp) {
		snext = stmp->next;

		if (stmp->sock_fd != -1)
			close(stmp->sock_fd);

		free(stmp->addr);
		free(stmp->port);
		free(stmp);

		stmp = snext;
	}

	firstaddr = NULL;
}

static void upsd_cleanup(void)
{
	ctype_t	*tmpcli, *tmpnext;
	upstype_t	*ups, *unext;

	/* cleanup client fds */
	tmpcli = firstclient;

	while (tmpcli) {
		tmpnext = tmpcli->next;
		delclient(tmpcli);
		tmpcli = tmpnext;
	}

	if (strcmp(pidfn, "") != 0) {
		unlink(pidfn);
	}

	ups = firstups;

	while (ups) {
		unext = ups->next;

		if (ups->sock_fd != -1) {
			close(ups->sock_fd);
		}

		sstate_infofree(ups);
		sstate_cmdfree(ups);
		pconf_finish(&ups->sock_ctx);

		free(ups->fn);
		free(ups->name);
		free(ups->desc);
		free(ups);

		ups = unext;
	}

	/* dump everything */

	acl_free();
	access_free();
	user_flush();
	desc_free();
	server_free();

	free(statepath);
	free(datapath);
	free(certfile);
}

/* service requests and check on new data */
static void mainloop(void)
{
	fd_set	rfds;
	struct	timeval	tv;
	int	res, maxfd = -1;
	ctype_t	*tmpcli, *tmpnext;
	upstype_t	*utmp, *unext;
	stype_t	*stmp, *snext;

	if (reload_flag) {
		conf_reload();
		reload_flag = 0;
	}
	
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	FD_ZERO(&rfds);

	/* scan through servers and add to FD_SET */
	for (stmp = firstaddr; stmp != NULL; stmp = stmp->next) {
		if (stmp->sock_fd != -1) {
			FD_SET(stmp->sock_fd, &rfds);

			if (stmp->sock_fd > maxfd) {
				maxfd = stmp->sock_fd;
			}
		}
	}

	/* scan through clients and add to FD_SET */
	for (tmpcli = firstclient; tmpcli != NULL; tmpcli = tmpcli->next) {
		if (tmpcli->fd != -1) {
			FD_SET(tmpcli->fd, &rfds);

			if (tmpcli->fd > maxfd) {
				maxfd = tmpcli->fd;
			}
		}
	}

	/* also add new driver sockets */
	for (utmp = firstups; utmp != NULL; utmp = utmp->next) {
		if (utmp->sock_fd != -1) {
			FD_SET(utmp->sock_fd, &rfds);

			if (utmp->sock_fd > maxfd) {
				maxfd = utmp->sock_fd;
			}
		}
	}
	
	res = select(maxfd + 1, &rfds, NULL, NULL, &tv);

	if (res > 0) {
		/* scan servers for activity */
		stmp = firstaddr;

		while (stmp) {
			snext = stmp->next;

			if (stmp->sock_fd != -1) {
				if (FD_ISSET(stmp->sock_fd, &rfds)) {
					answertcp(stmp);
				}
			}

			stmp = snext;
		}
		
		/* scan clients for activity */
		tmpcli = firstclient;

		while (tmpcli != NULL) {

			/* preserve for later since delclient may run */
			tmpnext = tmpcli->next;

			if (tmpcli->fd != -1) {
				if (FD_ISSET(tmpcli->fd, &rfds)) {
					readtcp(tmpcli);
				}
			}

			tmpcli = tmpnext;
		}

		/* now scan ups sockets for activity */
		utmp = firstups;

		while (utmp) {
			unext = utmp->next;

			if (utmp->sock_fd != -1) {
				if (FD_ISSET(utmp->sock_fd, &rfds)) {
					sstate_sock_read(utmp);
				}

			}

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
	printf("  -r <dir>	chroots to <dir>\n");
	printf("  -u <user>	switch to <user> (if started as root)\n");
	printf("  -V		display the version of this software\n");
#ifdef	HAVE_IPV6
	printf("  -4		IPv4 only\n");
	printf("  -6		IPv6 only\n");
#endif

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

void check_perms(const char *fn)
{
	int	ret;
	struct stat	st;

	ret = stat(fn, &st);

	if (ret != 0) {
		fatal_with_errno(EXIT_FAILURE, "stat %s", fn);
	}

	/* include the x bit here in case we check a directory */
	if (st.st_mode & (S_IROTH | S_IXOTH)) {
		upslogx(LOG_WARNING, "%s is world readable", fn);
	}
}	

int main(int argc, char **argv)
{
	int	i, cmd = 0;
	int	do_background = 1;
	char	*progname, *chroot_path = NULL;
	const char	*user = NULL;
	struct passwd	*new_uid = NULL;

	progname = argv[0];

	/* pick up a default from configure --with-user */
	user = RUN_AS_USER;

	/* yes, xstrdup - the conf handlers call free on this later */
	statepath = xstrdup(dflt_statepath());
	datapath = xstrdup(DATADIR);

	/* set up some things for later */
	snprintf(pidfn, sizeof(pidfn), "%s/upsd.pid", altpidpath());

	printf("Network UPS Tools upsd %s\n", UPS_VERSION);

	while ((i = getopt(argc, argv, "+h46p:r:i:fu:Vc:D")) != -1) {
		switch (i) {
			case 'h':
				help(progname);
				break;
			case 'p':
			case 'i':
				fatalx(EXIT_FAILURE, "Specifying a listening addresses with '-i <address>' and '-p <port>'\n"
					"is deprecated. Use 'LISTEN <address> [<port>]' in 'upsd.conf' instead.\n"
					"See 'man 8 upsd.conf' for more information.");
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

#ifdef	HAVE_IPV6
		  case '4':
				opt_af = AF_INET;
				break;

		  case '6':
				opt_af = AF_INET6;
				break;
#endif

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

	if (argc != 0) {
		help(progname);
	}

	setupsignals();

	open_syslog("upsd");

	/* send logging to the syslog pre-background for later use */
	syslogbit_set();

	/* do this here, since getpwnam() might not work in the chroot */
	new_uid = get_user_pwent(user);

	if (chroot_path) {
		chroot_start(chroot_path);
	}

	/* handle upsd.conf */
	load_upsdconf(0);	/* 0 = initial */

	/* start server */
	server_load();

	become_user(new_uid);

	if (chdir(statepath)) {
		fatal_with_errno(EXIT_FAILURE, "Can't chdir to %s", statepath);
	}

	/* check statepath perms */
	check_perms(statepath);

	/* handle ups.conf */
	read_upsconf();
	upsconf_add(0);		/* 0 = initial */

	if (num_ups == 0) {
		fatalx(EXIT_FAILURE, "Fatal error: at least one UPS must be defined in ups.conf");
	}

	ssl_init();

	/* try to bring in the var/cmd descriptions */
	desc_load();

	/* handle upsd.users */
	user_load();

	if (do_background) {
		background();

		writepid(pidfn);
	} else {
		memset(&pidfn, '\0', sizeof(pidfn));
	}

	while (exit_flag == 0) {
		mainloop();
	}

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
	upsd_cleanup();

	exit(EXIT_SUCCESS);
}
