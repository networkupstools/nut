/* upsd.c - watches ups state files and answers queries

   Copyright (C)
	1999		Russell Kroll <rkroll@exploits.org>
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>
	2011 - 2012	Arnaud Quette <arnaud.quette.free.fr>
	2019 		Eaton (author: Arnaud Quette <ArnaudQuette@eaton.com>)

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

#include "config.h"	/* must be the first header */

#include "upsd.h"
#include "upstype.h"
#include "conf.h"

#include "netcmds.h"
#include "upsconf.h"

#ifndef WIN32
# include <sys/un.h>
# include <sys/socket.h>
# include <netdb.h>

# ifdef HAVE_SYS_SIGNAL_H
#  include <sys/signal.h>
# endif
# ifdef HAVE_SIGNAL_H
#  include <signal.h>
/* #include <poll.h> */
# endif
#else
/* Those 2 files for support of getaddrinfo, getnameinfo and freeaddrinfo
   on Windows 2000 and older versions */
# include <ws2tcpip.h>
# include <wspiapi.h>
/* This override network system calls to adapt to Windows specificity */
# define W32_NETWORK_CALL_OVERRIDE
# include "wincompat.h"
# undef W32_NETWORK_CALL_OVERRIDE
# include <getopt.h>
#endif

#include "user.h"
#include "nut_ctype.h"
#include "nut_stdint.h"
#include "stype.h"
#include "netssl.h"
#include "sstate.h"
#include "desc.h"
#include "neterr.h"

#ifdef HAVE_WRAP
#include <tcpd.h>
int	allow_severity = LOG_INFO;
int	deny_severity = LOG_WARNING;
#endif	/* HAVE_WRAP */

/* externally-visible settings and pointers */

upstype_t	*firstups = NULL;

/* default 15 seconds before data is marked stale */
int	maxage = 15;

/* default to 1h before cleaning up status tracking entries */
int	tracking_delay = 3600;

/*
 * Preloaded to ALLOW_NO_DEVICE from upsd.conf or environment variable
 * (with higher prio for envvar); defaults to disabled for legacy compat.
 */
int allow_no_device = 0;

/*
 * Preloaded to ALLOW_NOT_ALL_LISTENERS from upsd.conf or environment variable
 * (with higher prio for envvar); defaults to disabled for legacy compat.
 */
int allow_not_all_listeners = 0;

/* preloaded to {OPEN_MAX} in main, can be overridden via upsd.conf */
nfds_t	maxconn = 0;

/* preloaded to STATEPATH in main, can be overridden via upsd.conf */
char	*statepath = NULL;

/* preloaded to NUT_DATADIR in main(), can be overridden via upsd.conf */
char	*datapath = NULL;

/* everything else */
static const char	*progname;

nut_ctype_t	*firstclient = NULL;
/* static nut_ctype_t	*lastclient = NULL; */

/* default is to listen on all local interfaces */
static stype_t	*firstaddr = NULL;

static int 	opt_af = AF_UNSPEC;

typedef enum {
	DRIVER = 1,
	CLIENT,
	SERVER
#ifdef WIN32
	,NAMED_PIPE
#endif

} handler_type_t;

typedef struct {
	handler_type_t	type;
	void		*data;
} handler_t;

/* Commands and settings status tracking */

/* general enable/disable status info for commands and settings
 * (disabled by default)
 * Note that only client that requested it will have it enabled
 * (see nut_ctype.h) */
static int	tracking_enabled = 0;

/* Commands and settings status tracking structure */
typedef struct tracking_s {
	char	*id;
	int	status;
	time_t	request_time; /* for cleanup */
	/* doubly linked list */
	struct tracking_s	*prev;
	struct tracking_s	*next;
} tracking_t;

static tracking_t	*tracking_list = NULL;

#ifndef WIN32
	/* pollfd  */
static struct pollfd	*fds = NULL;
#else
static HANDLE		*fds = NULL;
static HANDLE		mutex = INVALID_HANDLE_VALUE;
#endif
static handler_t	*handler = NULL;

	/* pid file */
static char	pidfn[SMALLBUF];

	/* set by signal handlers */
static int	reload_flag = 0, exit_flag = 0;

/* Minimalistic support for UUID v4 */
/* Ref: RFC 4122 https://tools.ietf.org/html/rfc4122#section-4.1.2 */
#define UUID4_BYTESIZE 16

#ifdef HAVE_SYSTEMD
# define SERVICE_UNIT_NAME "nut-server.service"
#endif

static const char *inet_ntopW (struct sockaddr_storage *s)
{
	static char str[40];

	switch (s->ss_family)
	{
	case AF_INET:
		return inet_ntop (AF_INET, &(((struct sockaddr_in *)s)->sin_addr), str, 16);
	case AF_INET6:
		return inet_ntop (AF_INET6, &(((struct sockaddr_in6 *)s)->sin6_addr), str, 40);
	default:
		errno = EAFNOSUPPORT;
		return NULL;
	}
}

/* return a pointer to the named ups if possible */
upstype_t *get_ups_ptr(const char *name)
{
	upstype_t	*tmp;

	if (!name) {
		return NULL;
	}

	for (tmp = firstups; tmp; tmp = tmp->next) {
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

	ups->stale = 0;

	upslogx(LOG_NOTICE, "UPS [%s] data is no longer stale", ups->name);
}

/* add another listening address */
void listen_add(const char *addr, const char *port)
{
	stype_t	*server;

	/* don't change listening addresses on reload */
	if (reload_flag) {
		return;
	}

	/* grab some memory and add the info */
	server = xcalloc(1, sizeof(*server));
	server->addr = xstrdup(addr);
	server->port = xstrdup(port);
	server->sock_fd = ERROR_FD_SOCK;
	server->next = NULL;

	if (firstaddr) {
		stype_t	*tmp;
		for (tmp = firstaddr; tmp->next; tmp = tmp->next);
		tmp->next = server;
	} else {
		firstaddr = server;
	}

	upsdebugx(3, "listen_add: added %s:%s", server->addr, server->port);
}

/* Close the connection if needed and free the allocated memory.
 * WARNING: it is up to the caller to rewrite the "next" pointer
 * in whoever points to this server instance (if needed)! */
static void stype_free(stype_t *server)
{
	if (VALID_FD_SOCK(server->sock_fd)) {
		close(server->sock_fd);
	}

	free(server->addr);
	free(server->port);
	free(server);
}

/* create a listening socket for tcp connections */
static void setuptcp(stype_t *server)
{
#ifdef WIN32
	WSADATA WSAdata;
	WSAStartup(2,&WSAdata);
	atexit((void(*)(void))WSACleanup);
#endif
	struct addrinfo		hints, *res, *ai;
	int	v = 0, one = 1;

	if (VALID_FD_SOCK(server->sock_fd)) {
		/* Already bound, e.g. thanks to 'LISTEN *' handling and injection
		 * into the list we loop over */
		upsdebugx(6, "setuptcp: SKIP bind to %s port %s: entry already initialized",
			server->addr, server->port);
		return;
	}

	upsdebugx(3, "setuptcp: try to bind to %s port %s", server->addr, server->port);
	if (!strcmp(server->addr, "localhost")) {
		/* Warn about possible surprises with IPv4 vs. IPv6 */
		upsdebugx(1,
			"setuptcp: WARNING: requested to LISTEN on 'localhost' "
			"by name - will use the first system-resolved "
			"IP address for that");
	}

	/* Special handling note for `LISTEN * <port>` directive with the
	 * literal asterisk on systems with RFC-3493 (no relation!) support
	 * for "IPv4-mapped addresses": it is possible (and technically
	 * suffices) to LISTEN on "::" (aka "::0" or "0:0:0:0:0:0:0:0") and
	 * also get an IPv4 any-address listener automatically. More so,
	 * they would conflict and listening on one such socket precludes
	 * listening on the other. On other systems (or with disabled
	 * mapping so IPv6 really means "IPv6 only") we need both sockets.
	 * NUT asks the system for "IPv6 only" mode when listening on any
	 * sort of IPv6 addresses; it is however up to the system to implement
	 * that ability and comply with our request.
	 * Here we jump through some hoops:
	 * * Try to get IPv6 any-address (unless constrained by CLI to IPv4);
	 * * Try to get IPv4 any-address (unless constrained by CLI to IPv6),
	 *   log information for the sysadmin that it might conflict with the
	 *   IPv6 listener (IFF we have just opened one);
	 * * Remember the one or two linked-list entries used, to release later.
	 */
	if (!strcmp(server->addr, "*")) {
		stype_t	*serverAnyV4 = NULL, *serverAnyV6 = NULL;
		int	canhaveAnyV4 = 0, canhaveAnyV6 = 0;

		/* Note: default opt_af==AF_UNSPEC so not constrained to only one protocol */
		if (opt_af != AF_INET6) {
			/* Not constrained to IPv6 */
			upsdebugx(1, "%s: handling 'LISTEN * %s' with IPv4 any-address support",
				__func__, server->port);
			serverAnyV4 = xcalloc(1, sizeof(*serverAnyV4));
			serverAnyV4->addr = xstrdup("0.0.0.0");
			serverAnyV4->port = xstrdup(server->port);
			serverAnyV4->sock_fd = ERROR_FD_SOCK;
			serverAnyV4->next = NULL;
		}

		if (opt_af != AF_INET) {
			/* Not constrained to IPv4 */
			upsdebugx(1, "%s: handling 'LISTEN * %s' with IPv6 any-address support",
				__func__, server->port);
			serverAnyV6 = xcalloc(1, sizeof(*serverAnyV6));
			serverAnyV6->addr = xstrdup("::0");
			serverAnyV6->port = xstrdup(server->port);
			serverAnyV6->sock_fd = ERROR_FD_SOCK;
			serverAnyV6->next = NULL;
		}

		if (serverAnyV6) {
			setuptcp(serverAnyV6);
			if (VALID_FD_SOCK(serverAnyV6->sock_fd)) {
				canhaveAnyV6 = 1;
			} else {
				upsdebugx(3,
					"%s: Could not bind to %s:%s trying to handle a 'LISTEN *' directive",
					__func__, serverAnyV6->addr, serverAnyV6->port);
			}
		}

		if (serverAnyV4) {
			/* Try to get this listener if we can (no IPv4-mapped
			 * IPv6 support was in force on this platform or its
			 * configuration in some way that setsockopt(IPV6_V6ONLY)
			 * failed to cancel).
			 */
			upsdebugx(3, "%s: try taking IPv4 'ANY'%s",
				__func__,
				canhaveAnyV6 ? " (if dual-stack IPv6 'ANY' did not grab it)" : "");
			setuptcp(serverAnyV4);
			if (VALID_FD_SOCK(serverAnyV4->sock_fd)) {
				canhaveAnyV4 = 1;
			} else {
				upsdebugx(3,
					"%s: Could not bind to IPv4 %s:%s%s",
					__func__, serverAnyV4->addr, serverAnyV4->port,
					canhaveAnyV6 ? (" after trying to bind to IPv6: "
						"assuming dual-stack support on this "
						"system could not be disabled") : "");
			}
		}

		if (!canhaveAnyV4 && !canhaveAnyV6) {
			fatalx(EXIT_FAILURE,
				"Handling of 'LISTEN * %s' directive failed to bind to 'ANY' address",
				server->port);
		}

		/* Finalize our findings and reset to normal operation
		 * Note that at least one of these addresses is usable
		 * and we keep it (and replace original "server" entry
		 * keeping its place in the list).
		 */
		free(server->addr);
		free(server->port);
		if (canhaveAnyV4) {
			upsdebugx(3, "%s: remembering IPv4 'ANY' instead of 'LISTEN *'", __func__);
			server->addr = serverAnyV4->addr;
			server->port = serverAnyV4->port;
			server->sock_fd = serverAnyV4->sock_fd;
			/* ...and keep whatever server->next there was */

			/* Free the ghost, all needed info was relocated */
			free(serverAnyV4);
		} else {
			if (serverAnyV4) {
				/* Free any contents there were too */
				stype_free(serverAnyV4);
			}
		}
		serverAnyV4 = NULL;

		if (canhaveAnyV6) {
			if (canhaveAnyV4) {
				/* "server" already populated by excerpts from V4, attach to it */
				upsdebugx(3, "%s: also remembering IPv6 'ANY' instead of 'LISTEN *'", __func__);
				serverAnyV6->next = server->next;
				server->next = serverAnyV6;
			} else {
				/* Only retain V6 info */
				upsdebugx(3, "%s: remembering IPv6 'ANY' instead of 'LISTEN *'", __func__);
				server->addr = serverAnyV6->addr;
				server->port = serverAnyV6->port;
				server->sock_fd = serverAnyV6->sock_fd;
				/* ...and keep whatever server->next there was */

				/* Free the ghost, all needed info was relocated */
				free(serverAnyV6);
			}
		} else {
			if (serverAnyV6) {
				/* Free any contents there were too */
				stype_free(serverAnyV6);
			}
		}
		serverAnyV6 = NULL;

		return;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_PASSIVE;
	hints.ai_family		= opt_af;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_protocol	= IPPROTO_TCP;

	if ((v = getaddrinfo(server->addr, server->port, &hints, &res)) != 0) {
		if (v == EAI_SYSTEM) {
			fatal_with_errno(EXIT_FAILURE, "getaddrinfo");
		}

		fatalx(EXIT_FAILURE, "getaddrinfo: %s", gai_strerror(v));
	}

	for (ai = res; ai; ai = ai->ai_next) {
		TYPE_FD_SOCK sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (INVALID_FD_SOCK(sock_fd)) {
			upsdebug_with_errno(3, "setuptcp: socket");
			continue;
		}

		if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one)) != 0) {
			fatal_with_errno(EXIT_FAILURE, "setuptcp: setsockopt");
		}

		/* Ordinarily we request that IPv6 listeners handle only IPv6
		 * and not IPv4 mapped addresses - if the OS would honour that.
		 * TOTHINK: Does any platform need `#ifdef IPV6_V6ONLY` given
		 * that we apparently already have AF_INET6 OS support everywhere?
		 */
		if (ai->ai_family == AF_INET6) {
			if (setsockopt(sock_fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&one, sizeof(one)) != 0) {
				upsdebug_with_errno(3, "setuptcp: setsockopt IPV6_V6ONLY");
				/* ack, ignore */
			}
		}

		if (bind(sock_fd, ai->ai_addr, ai->ai_addrlen) < 0) {
			upsdebug_with_errno(3, "setuptcp: bind");
			close(sock_fd);
			continue;
		}

/* WSAEventSelect automatically set the socket to nonblocking mode */
#ifndef WIN32
		if ((v = fcntl(sock_fd, F_GETFL, 0)) == -1) {
			fatal_with_errno(EXIT_FAILURE, "setuptcp: fcntl(get)");
		}

		if (fcntl(sock_fd, F_SETFL, v | O_NDELAY) == -1) {
			fatal_with_errno(EXIT_FAILURE, "setuptcp: fcntl(set)");
		}
#endif

		if (listen(sock_fd, 16) < 0) {
			upsdebug_with_errno(3, "setuptcp: listen");
			close(sock_fd);
			continue;
		}

		if (ai->ai_next) {
			char ipaddrbuf[SMALLBUF];
			const char *ipaddr;
			snprintf(ipaddrbuf, sizeof(ipaddrbuf), " as ");
			ipaddr = inet_ntop(ai->ai_family, ai->ai_addr,
				ipaddrbuf + strlen(ipaddrbuf),
				sizeof(ipaddrbuf));
			upslogx(LOG_WARNING,
				"setuptcp: bound to %s%s but there seem to be "
				"further (ignored) addresses resolved for this name",
				server->addr,
				ipaddr == NULL ? "" : ipaddrbuf);
		}

		server->sock_fd = sock_fd;
		break;
	}

#ifdef WIN32
		server->Event = CreateEvent(NULL, /* Security */
				FALSE, /* auto-reset */
				FALSE, /* initial state */
				NULL); /* no name */

		/* Associate socket event to the socket via its Event object */
		WSAEventSelect( server->sock_fd, server->Event, FD_ACCEPT );
#endif

	freeaddrinfo(res);

	/* leave up to the caller, server_load(), to fail silently if there is
	 * no other valid LISTEN interface */
	if (INVALID_FD_SOCK(server->sock_fd)) {
		upslogx(LOG_ERR, "not listening on %s port %s", server->addr, server->port);
	} else {
		upslogx(LOG_INFO, "listening on %s port %s", server->addr, server->port);
	}

	return;
}

/* decrement the login counter for this ups */
static void declogins(const char *upsname)
{
	upstype_t	*ups;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		upslogx(LOG_INFO, "Tried to decrement invalid ups name (%s)", upsname);
		return;
	}

	ups->numlogins--;

	if (ups->numlogins < 0) {
		upslogx(LOG_ERR, "Programming error: UPS [%s] has numlogins=%d", ups->name, ups->numlogins);
	}
}

/* disconnect a client connection and free all related memory */
static void client_disconnect(nut_ctype_t *client)
{
	if (!client) {
		return;
	}

	upsdebugx(2, "Disconnect from %s", client->addr);

	shutdown(client->sock_fd, 2);
	close(client->sock_fd);

#ifdef WIN32
	CloseHandle(client->Event);
#endif

	if (client->loginups) {
		declogins(client->loginups);
	}

	ssl_finish(client);

	pconf_finish(&client->ctx);

	if (client->prev) {
		client->prev->next = client->next;
	} else {
		/* deleting first entry */
		firstclient = client->next;
	}

	if (client->next) {
		client->next->prev = client->prev;
	} else {
		/* deleting last entry */
		/* lastclient = client->prev; */
	}

	free(client->addr);
	free(client->loginups);
	free(client->password);
	free(client->username);
	free(client);

	return;
}

/* send the buffer <sendbuf> of length <sendlen> to host <dest>
 * returns effectively a boolean: 0 = failed, 1 = sent ok
 */
int sendback(nut_ctype_t *client, const char *fmt, ...)
{
	ssize_t	res;
	size_t	len;
	char	ans[NUT_NET_ANSWER_MAX+1];
	va_list	ap;

	if (!client) {
		return 0;
	}

	va_start(ap, fmt);
	vsnprintf(ans, sizeof(ans), fmt, ap);
	va_end(ap);

	len = strlen(ans);

	/* System write() and our ssl_write() have a loophole that they write a
	 * size_t amount of bytes and upon success return that in ssize_t value
	 */
	assert(len < SSIZE_MAX);

#ifdef WITH_SSL
	if (client->ssl) {
		res = ssl_write(client, ans, len);
	} else
#endif /* WITH_SSL */
	{
		res = write(client->sock_fd, ans, len);
	}

	{ /* scoping */
		char * s = str_rtrim(ans, '\n');
		upsdebugx(2, "write: [destfd=%d] [len=%" PRIuSIZE "] [%s]", client->sock_fd, len, s);
	}

	if (res < 0 || len != (size_t)res) {
		upslog_with_errno(LOG_NOTICE, "write() failed for %s", client->addr);
		client->last_heard = 0;
		return 0;	/* failed */
	}

	return 1;	/* OK */
}

/* just a simple wrapper for now */
int send_err(nut_ctype_t *client, const char *errtype)
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
	nut_ctype_t	*client, *cnext;

	for (client = firstclient; client; client = cnext) {

		cnext = client->next;

		/* if it's not logged in, don't check it */
		if (!client->loginups) {
			continue;
		}

		if (!strcmp(client->loginups, upsname)) {
			upslogx(LOG_INFO, "Kicking client %s (was on UPS [%s])\n", client->addr, upsname);
			client_disconnect(client);
		}
	}
}

/* make sure a UPS is sane - connected, with fresh data */
int ups_available(const upstype_t *ups, nut_ctype_t *client)
{
	if (!ups) {
		/* Should never happen, but handle this
		 * just in case instead of segfaulting */
		upsdebugx(1, "%s: ERROR, called with a NULL ups pointer", __func__);
		send_err(client, NUT_ERR_FEATURE_NOT_SUPPORTED);
		return 0;
	}

	if (INVALID_FD(ups->sock_fd)) {
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
static void check_command(int cmdnum, nut_ctype_t *client, size_t numarg,
	const char **arg)
{
	upsdebugx(6, "Entering %s: %s", __func__, numarg > 0 ? arg[0] : "<>");

	if (netcmds[cmdnum].flags & FLAG_USER) {
		/* command requires previous authentication */
#ifdef HAVE_WRAP
		struct request_info	req;
#endif	/* HAVE_WRAP */

		if (!client->username) {
			upsdebugx(1, "%s: client not logged in yet", __func__);
			send_err(client, NUT_ERR_USERNAME_REQUIRED);
			return;
		}

		if (!client->password) {
			upsdebugx(1, "%s: client not logged in yet", __func__);
			send_err(client, NUT_ERR_PASSWORD_REQUIRED);
			return;
		}

#ifdef HAVE_WRAP
		request_init(&req, RQ_DAEMON, progname, RQ_FILE, client->sock_fd, RQ_USER, client->username, 0);
		fromhost(&req);

		if (!hosts_access(&req)) {
			upsdebugx(1,
				"%s: while authenticating %s found that "
				"tcp-wrappers says access should be denied",
				__func__, client->username);
			send_err(client, NUT_ERR_ACCESS_DENIED);
			return;
		}
#endif	/* HAVE_WRAP */
	}

	upsdebugx(6, "%s: Calling command handler for %s", __func__, numarg > 0 ? arg[0] : "<>");

	/* looks good - call the command */
	netcmds[cmdnum].func(client, (numarg < 2) ? 0 : (numarg - 1), (numarg > 1) ? &arg[1] : NULL);
}

/* parse requests from the network */
static void parse_net(nut_ctype_t *client)
{
	int	i;

	/* shouldn't happen */
	if (client->ctx.numargs < 1) {
		send_err(client, NUT_ERR_UNKNOWN_COMMAND);
		return;
	}

	for (i = 0; netcmds[i].name; i++) {
		if (!strcasecmp(netcmds[i].name, client->ctx.arglist[0])) {
			check_command(i, client, client->ctx.numargs, (const char **) client->ctx.arglist);
			return;
		}
	}

	/* fallthrough = not matched by any entry in netcmds */

	send_err(client, NUT_ERR_UNKNOWN_COMMAND);
}

/* answer incoming tcp connections */
static void client_connect(stype_t *server)
{
	struct	sockaddr_storage csock;
#if defined(__hpux) && !defined(_XOPEN_SOURCE_EXTENDED)
	int	clen;
#else
	socklen_t	clen;
#endif
	int		fd;
	nut_ctype_t		*client;

	clen = sizeof(csock);
	fd = accept(server->sock_fd, (struct sockaddr *) &csock, &clen);

	if (fd < 0) {
		return;
	}

	client = xcalloc(1, sizeof(*client));

	client->sock_fd = fd;

	time(&client->last_heard);

	client->addr = xstrdup(inet_ntopW(&csock));

	client->tracking = 0;

#ifdef WIN32
	client->Event = CreateEvent(NULL, /* Security, */
				FALSE,    /* auto-reset */
				FALSE,    /* initial state */
				NULL);    /* no name */

	/* Associate socket event to the socket via its Event object */
	WSAEventSelect( client->sock_fd, client->Event, FD_READ );
#endif

	pconf_init(&client->ctx, NULL);

	if (firstclient) {
		firstclient->prev = client;
		client->next = firstclient;
	}

	firstclient = client;

/*
	if (lastclient) {
		client->prev = lastclient;
		lastclient->next = client;
	}

	lastclient = client;
 */
	upsdebugx(2, "Connect from %s", client->addr);
}

/* read tcp messages and handle them */
static void client_readline(nut_ctype_t *client)
{
	char	buf[SMALLBUF];
	int	i;
	ssize_t	ret;

#ifdef WITH_SSL
	if (client->ssl) {
		ret = ssl_read(client, buf, sizeof(buf));
	} else
#endif /* WITH_SSL */
	{
		ret = read(client->sock_fd, buf, sizeof(buf));
	}

	if (ret < 0) {
		upsdebug_with_errno(2, "Disconnect %s (read failure)", client->addr);
		client_disconnect(client);
		return;
	}

	if (ret == 0) {
		upsdebugx(2, "Disconnect %s (no data available)", client->addr);
		client_disconnect(client);
		return;
	}

	/* fragment handling code */
	for (i = 0; i < ret; i++) {

		/* add to the receive queue one by one */
		switch (pconf_char(&client->ctx, buf[i]))
		{
		case 1:
			time(&client->last_heard);	/* command received */
			parse_net(client);
			continue;

		case 0:
			continue;	/* haven't gotten a line yet */

		default:
			/* parse error */
			upslogx(LOG_NOTICE, "Parse error on sock: %s", client->ctx.errmsg);
			return;
		}
	}

	return;
}

void server_load(void)
{
	stype_t	*server;
	size_t	listenersTotal = 0, listenersValid = 0,
		listenersTotalLocalhost = 0, listenersValidLocalhost = 0,
		listenersLocalhostName = 0,
		listenersLocalhostName6 = 0,
		listenersLocalhostIPv4 = 0,
		listenersLocalhostIPv6 = 0,
		listenersValidLocalhostName = 0,
		listenersValidLocalhostName6 = 0,
		listenersValidLocalhostIPv4 = 0,
		listenersValidLocalhostIPv6 = 0;

	/* default behaviour if no LISTEN address has been specified */
	if (!firstaddr) {
		/* Note: default opt_af==AF_UNSPEC so not constrained to only one protocol */
		if (opt_af != AF_INET) {
			upsdebugx(1, "%s: No LISTEN configuration provided, will try IPv6 localhost", __func__);
			listen_add("::1", string_const(PORT));
		}

		if (opt_af != AF_INET6) {
			upsdebugx(1, "%s: No LISTEN configuration provided, will try IPv4 localhost", __func__);
			listen_add("127.0.0.1", string_const(PORT));
		}
	}

	for (server = firstaddr; server; server = server->next) {
		setuptcp(server);
	}

	/* Account separately from setuptcp() because it can edit the list,
	 * e.g. when handling `LISTEN *` lines.
	 */
	for (server = firstaddr; server; server = server->next) {
		listenersTotal++;
		if (VALID_FD_SOCK(server->sock_fd)) {
			listenersValid++;
		}

		if (!strcmp(server->addr, "localhost")) {
			listenersLocalhostName++;
			listenersTotalLocalhost++;
			if (VALID_FD_SOCK(server->sock_fd)) {
				listenersValidLocalhostName++;
				listenersValidLocalhost++;
			}
		}

		if (!strcmp(server->addr, "localhost6")) {
			listenersLocalhostName6++;
			listenersTotalLocalhost++;
			if (VALID_FD_SOCK(server->sock_fd)) {
				listenersValidLocalhostName6++;
				listenersValidLocalhost++;
			}
		}

		if (!strcmp(server->addr, "127.0.0.1")) {
			listenersLocalhostIPv4++;
			listenersTotalLocalhost++;
			if (VALID_FD_SOCK(server->sock_fd)) {
				listenersValidLocalhostIPv4++;
				listenersValidLocalhost++;
			}
		}

		if (!strcmp(server->addr, "::1")) {
			listenersLocalhostIPv6++;
			listenersTotalLocalhost++;
			if (VALID_FD_SOCK(server->sock_fd)) {
				listenersValidLocalhostIPv6++;
				listenersValidLocalhost++;
			}
		}
	}

	upsdebugx(1, "%s: tried to set up %" PRIuSIZE
		" listening sockets, succeeded with %" PRIuSIZE,
		__func__, listenersTotal, listenersValid);
	upsdebugx(3, "%s: ...of those related to localhost: "
		"overall: %" PRIuSIZE " tried, %" PRIuSIZE " succeeded; "
		"by name: %" PRIuSIZE "T/%" PRIuSIZE "S; "
		"by name(6): %" PRIuSIZE "T/%" PRIuSIZE "S; "
		"by IPv4 addr: %" PRIuSIZE "T/%" PRIuSIZE "S; "
		"by IPv6 addr: %" PRIuSIZE "T/%" PRIuSIZE "S",
		__func__,
		listenersTotalLocalhost, listenersValidLocalhost,
		listenersLocalhostName, listenersValidLocalhostName,
		listenersLocalhostName6, listenersValidLocalhostName6,
		listenersLocalhostIPv4, listenersValidLocalhostIPv4,
		listenersLocalhostIPv6, listenersValidLocalhostIPv6
		);

	/* check if we have at least 1 valid LISTEN interface */
	if (!listenersValid) {
		fatalx(EXIT_FAILURE, "no listening interface available");
	}

	/* is everything requested - handled okay? */
	if (listenersTotal == listenersValid)
		return;

	/* check for edge cases we can let slide */
	if ( (listenersTotal - listenersValid) ==
	     (listenersTotalLocalhost - listenersValidLocalhost)
	) {
		/* Note that we can also get into this situation
		 * when "dual-stack" IPv6 listener also handles
		 * IPv4 connections, and precludes successful
		 * setup of the IPv4 listener later.
		 *
		 * FIXME? Can we get into this situation the other
		 * way around - an IPv4 listener precluding the
		 * IPv6 one, so end-user actually lacks one of the
		 * requested connection types?
		 */
		upsdebugx(1, "%s: discrepancy corresponds to "
			"addresses related to localhost; assuming "
			"that it was attempted under several names "
			"which resolved to same IP:PORT socket specs "
			"(so only the first one of each succeeded)",
			__func__);
		return;
	}

	if (allow_not_all_listeners) {
		upslogx(LOG_WARNING,
			"WARNING: some listening interfaces were "
			"not available, but the ALLOW_NOT_ALL_LISTENERS "
			"setting is active");
	} else {
		upsdebugx(0,
			"Reconcile available NUT server IP addresses "
			"and LISTEN configuration, or consider the "
			"ALLOW_NOT_ALL_LISTENERS setting!");
		fatalx(EXIT_FAILURE,
			"Fatal error: some listening interfaces were "
			"not available");
	}
}

void server_free(void)
{
	stype_t	*server, *snext;

	/* cleanup server fds */
	for (server = firstaddr; server; server = snext) {
		snext = server->next;
		stype_free(server);
	}

	firstaddr = NULL;
}

static void client_free(void)
{
	nut_ctype_t		*client, *cnext;

	/* cleanup client fds */
	for (client = firstclient; client; client = cnext) {
		cnext = client->next;
		client_disconnect(client);
	}
}

static void driver_free(void)
{
	upstype_t	*ups, *unext;

	for (ups = firstups; ups; ups = unext) {
		upsdebugx(1, "%s: forgetting UPS [%s] (FD %d)",
			__func__, ups->name, ups->sock_fd);

		unext = ups->next;

		if (VALID_FD(ups->sock_fd)) {
#ifndef WIN32
			close(ups->sock_fd);
#else
			DisconnectNamedPipe(ups->sock_fd);
			CloseHandle(ups->sock_fd);
#endif
			ups->sock_fd = ERROR_FD;
		}

		sstate_infofree(ups);
		sstate_cmdfree(ups);

		pconf_finish(&ups->sock_ctx);

		free(ups->fn);
		free(ups->name);
		free(ups->desc);
		free(ups);
	}
}

static void upsd_cleanup(void)
{
	if (strlen(pidfn) > 0) {
		unlink(pidfn);
	}

	/* dump everything */

	user_flush();
	desc_free();

	server_free();
	client_free();
	driver_free();
	tracking_free();

	free(statepath);
	free(datapath);
	free(certfile);
	free(certname);
	free(certpasswd);

	free(fds);
	free(handler);

#ifdef WIN32
	if (mutex != INVALID_HANDLE_VALUE) {
		ReleaseMutex(mutex);
		CloseHandle(mutex);
	}
#endif
}

static void poll_reload(void)
{
#ifndef WIN32
	long	ret;
	size_t	maxalloc;

	ret = sysconf(_SC_OPEN_MAX);

	if ((intmax_t)ret < (intmax_t)maxconn) {
		fatalx(EXIT_FAILURE,
			"Your system limits the maximum number of connections to %ld\n"
			"but you requested %" PRIdMAX ". The server won't start until this\n"
			"problem is resolved.\n", ret, (intmax_t)maxconn);
	}

	if (1 > maxconn) {
		fatalx(EXIT_FAILURE,
			"You requested %" PRIdMAX " as maximum number of connections.\n"
			"The server won't start until this problem is resolved.\n", (intmax_t)maxconn);
	}

	/* How many items can we stuff into the array? */
	maxalloc = SIZE_MAX / sizeof(void *);
	if ((uintmax_t)maxalloc < (uintmax_t)maxconn) {
		fatalx(EXIT_FAILURE,
			"You requested %" PRIdMAX " as maximum number of connections, but we can only allocate %" PRIuSIZE ".\n"
			"The server won't start until this problem is resolved.\n", (intmax_t)maxconn, maxalloc);
	}

	/* The checks above effectively limit that maxconn is in size_t range */
	fds = xrealloc(fds, (size_t)maxconn * sizeof(*fds));
	handler = xrealloc(handler, (size_t)maxconn * sizeof(*handler));
#else
	fds = xrealloc(fds, (size_t)MAXIMUM_WAIT_OBJECTS * sizeof(*fds));
	handler = xrealloc(handler, (size_t)MAXIMUM_WAIT_OBJECTS * sizeof(*handler));
#endif
}

/* instant command and setvar status tracking */

/* allocate a new status tracking entry */
int tracking_add(const char *id)
{
	tracking_t	*item;

	if ((!tracking_enabled) || (!id))
		return 0;

	item = xcalloc(1, sizeof(*item));

	item->id = xstrdup(id);
	item->status = STAT_PENDING;
	time(&item->request_time);

	if (tracking_list) {
		tracking_list->prev = item;
		item->next = tracking_list;
	}

	tracking_list = item;

	return 1;
}

/* set status of a specific tracking entry */
int tracking_set(const char *id, const char *value)
{
	tracking_t	*item, *next_item;

	/* sanity checks */
	if ((!tracking_list) || (!id) || (!value))
		return 0;

	for (item = tracking_list; item; item = next_item) {

		next_item = item->next;

		if (!strcasecmp(item->id, id)) {
			item->status = atoi(value);
			return 1;
		}
	}

	return 0; /* id not found! */
}

/* free a specific tracking entry */
int tracking_del(const char *id)
{
	tracking_t	*item, *next_item;

	/* sanity check */
	if ((!tracking_list) || (!id))
		return 0;

	upsdebugx(3, "%s: deleting id %s", __func__, id);

	for (item = tracking_list; item; item = next_item) {

		next_item = item->next;

		if (strcasecmp(item->id, id))
			continue;

		if (item->prev)
			item->prev->next = item->next;
		else
			/* deleting first entry */
			tracking_list = item->next;

		if (item->next)
			item->next->prev = item->prev;

		free(item->id);
		free(item);

		return 1;

	}

	return 0; /* id not found! */
}

/* free all status tracking entries */
void tracking_free(void)
{
	tracking_t	*item, *next_item;

	/* sanity check */
	if (!tracking_list)
		return;

	upsdebugx(3, "%s", __func__);

	for (item = tracking_list; item; item = next_item) {
		next_item = item->next;
		tracking_del(item->id);
	}
}

/* cleanup status tracking entries according to their age and tracking_delay */
void tracking_cleanup(void)
{
	tracking_t	*item, *next_item;
	time_t	now;

	/* sanity check */
	if (!tracking_list)
		return;

	time(&now);

	upsdebugx(3, "%s", __func__);

	for (item = tracking_list; item; item = next_item) {

		next_item = item->next;

		if (difftime(now, item->request_time) > tracking_delay) {
			tracking_del(item->id);
		}
	}
}

/* get status of a specific tracking entry */
char *tracking_get(const char *id)
{
	tracking_t	*item, *next_item;

	/* sanity checks */
	if ((!tracking_list) || (!id))
		return "ERR UNKNOWN";

	for (item = tracking_list; item; item = next_item) {

		next_item = item->next;

		if (strcasecmp(item->id, id))
			continue;

		switch (item->status)
		{
		case STAT_PENDING:
			return "PENDING";
		case STAT_HANDLED:
			return "SUCCESS";
		case STAT_UNKNOWN:
			return "ERR UNKNOWN";
		case STAT_INVALID:
			return "ERR INVALID-ARGUMENT";
		case STAT_FAILED:
			return "ERR FAILED";
		}
	}

	return "ERR UNKNOWN"; /* id not found! */
}

/* enable general status tracking (tracking_enabled) and return its value (1). */
int tracking_enable(void)
{
	tracking_enabled = 1;

	return tracking_enabled;
}

/* disable general status tracking only if no client use it anymore.
 * return the new value for tracking_enabled */
int tracking_disable(void)
{
	nut_ctype_t		*client, *cnext;

	for (client = firstclient; client; client = cnext) {
		cnext = client->next;
		if (client->tracking == 1)
			return 1;
	}
	return 0;
}

/* return current general status of tracking (tracking_enabled). */
int tracking_is_enabled(void)
{
	return tracking_enabled;
}

/* UUID v4 basic implementation
 * Note: 'dest' must be at least `UUID4_LEN` long */
int nut_uuid_v4(char *uuid_str)
{
	size_t		i;
	uint8_t nut_uuid[UUID4_BYTESIZE];

	if (!uuid_str)
		return 0;

	for (i = 0; i < UUID4_BYTESIZE; i++)
		nut_uuid[i] = (uint8_t)rand() + (uint8_t)rand();

	/* set variant and version */
	nut_uuid[6] = (nut_uuid[6] & 0x0F) | 0x40;
	nut_uuid[8] = (nut_uuid[8] & 0x3F) | 0x80;

	return snprintf(uuid_str, UUID4_LEN,
		"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		nut_uuid[0], nut_uuid[1], nut_uuid[2], nut_uuid[3],
		nut_uuid[4], nut_uuid[5], nut_uuid[6], nut_uuid[7],
		nut_uuid[8], nut_uuid[9], nut_uuid[10], nut_uuid[11],
		nut_uuid[12], nut_uuid[13], nut_uuid[14], nut_uuid[15]);
}

static void set_exit_flag(int sig)
{
	exit_flag = sig;
}

static void set_reload_flag(int sig)
{
	NUT_UNUSED_VARIABLE(sig);
	reload_flag = 1;
}

/* service requests and check on new data */
static void mainloop(void)
{
#ifndef WIN32
	int	ret;
	nfds_t	i;
#else
	DWORD	ret;
	pipe_conn_t * conn;
#endif

	nfds_t	nfds = 0;
	upstype_t	*ups;
	nut_ctype_t		*client, *cnext;
	stype_t		*server;
	time_t	now;

	upsnotify(NOTIFY_STATE_WATCHDOG, NULL);

	time(&now);

	if (reload_flag) {
		upsnotify(NOTIFY_STATE_RELOADING, NULL);
		conf_reload();
		poll_reload();
		reload_flag = 0;
		upsnotify(NOTIFY_STATE_READY, NULL);
	}

	/* cleanup instcmd/setvar status tracking entries if needed */
	tracking_cleanup();

#ifndef WIN32
	/* scan through driver sockets */
	for (ups = firstups; ups && (nfds < maxconn); ups = ups->next) {

		/* see if we need to (re)connect to the socket */
		if (INVALID_FD(ups->sock_fd)) {
			upsdebugx(1, "%s: UPS [%s] is not currently connected, "
				"trying to reconnect",
				__func__, ups->name);
			ups->sock_fd = sstate_connect(ups);
			if (INVALID_FD(ups->sock_fd)) {
				upsdebugx(1, "%s: UPS [%s] is still not connected (FD %d)",
					__func__, ups->name, ups->sock_fd);
			} else {
				upsdebugx(1, "%s: UPS [%s] is now connected as FD %d",
					__func__, ups->name, ups->sock_fd);
			}
			continue;
		}

		/* throw some warnings if it's not feeding us data any more */
		if (sstate_dead(ups, maxage)) {
			ups_data_stale(ups);
		} else {
			ups_data_ok(ups);
		}

		fds[nfds].fd = ups->sock_fd;
		fds[nfds].events = POLLIN;

		handler[nfds].type = DRIVER;
		handler[nfds].data = ups;

		nfds++;
	}

	/* scan through client sockets */
	for (client = firstclient; client; client = cnext) {

		cnext = client->next;

		if (difftime(now, client->last_heard) > 60) {
			/* shed clients after 1 minute of inactivity */
			/* FIXME: create an upsd.conf parameter (CLIENT_INACTIVITY_DELAY) */
			client_disconnect(client);
			continue;
		}

		if (nfds >= maxconn) {
			/* ignore clients that we are unable to handle */
			continue;
		}

		fds[nfds].fd = client->sock_fd;
		fds[nfds].events = POLLIN;

		handler[nfds].type = CLIENT;
		handler[nfds].data = client;

		nfds++;
	}

	/* scan through server sockets */
	for (server = firstaddr; server && (nfds < maxconn); server = server->next) {

		if (server->sock_fd < 0) {
			continue;
		}

		fds[nfds].fd = server->sock_fd;
		fds[nfds].events = POLLIN;

		handler[nfds].type = SERVER;
		handler[nfds].data = server;

		nfds++;
	}

	upsdebugx(2, "%s: polling %" PRIdMAX " filedescriptors", __func__, (intmax_t)nfds);

	ret = poll(fds, nfds, 2000);

	if (ret == 0) {
		upsdebugx(2, "%s: no data available", __func__);
		return;
	}

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "%s", __func__);
		return;
	}

	for (i = 0; i < nfds; i++) {

		if (fds[i].revents & (POLLHUP|POLLERR|POLLNVAL)) {

			switch(handler[i].type)
			{
			case DRIVER:
				sstate_disconnect((upstype_t *)handler[i].data);
				break;
			case CLIENT:
				client_disconnect((nut_ctype_t *)handler[i].data);
				break;
			case SERVER:
				upsdebugx(2, "%s: server disconnected", __func__);
				break;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
			/* All enum cases defined as of the time of coding
			 * have been covered above. Handle later definitions,
			 * memory corruptions and buggy inputs below...
			 */
			default:
				upsdebugx(2, "%s: <unknown> disconnected", __func__);
				break;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif

			}

			continue;
		}

		if (fds[i].revents & POLLIN) {

			switch(handler[i].type)
			{
			case DRIVER:
				sstate_readline((upstype_t *)handler[i].data);
				break;
			case CLIENT:
				client_readline((nut_ctype_t *)handler[i].data);
				break;
			case SERVER:
				client_connect((stype_t *)handler[i].data);
				break;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
			/* All enum cases defined as of the time of coding
			 * have been covered above. Handle later definitions,
			 * memory corruptions and buggy inputs below...
			 */
			default:
				upsdebugx(2, "%s: <unknown> has data available", __func__);
				break;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
			}

			continue;
		}
	}
#else
	/* scan through driver sockets */
	for (ups = firstups; ups && (nfds < maxconn); ups = ups->next) {

		/* see if we need to (re)connect to the socket */
		if (INVALID_FD(ups->sock_fd)) {
			upsdebugx(1, "%s: UPS [%s] is not currently connected, "
				"trying to reconnect",
				__func__, ups->name);
			ups->sock_fd = sstate_connect(ups);
			if (INVALID_FD(ups->sock_fd)) {
				upsdebugx(1, "%s: UPS [%s] is still not connected (FD %d)",
					__func__, ups->name, ups->sock_fd);
			} else {
				upsdebugx(1, "%s: UPS [%s] is now connected as FD %d",
					__func__, ups->name, ups->sock_fd);
			}
			continue;
		}

		/* throw some warnings if it's not feeding us data any more */
		if (sstate_dead(ups, maxage)) {
			ups_data_stale(ups);
		} else {
			ups_data_ok(ups);
		}

		/* FIXME: Is the conditional needed? We got here... */
		if (VALID_FD(ups->sock_fd)) {
			fds[nfds] = ups->read_overlapped.hEvent;

			handler[nfds].type = DRIVER;
			handler[nfds].data = ups;

			nfds++;
		}
	}

	/* scan through client sockets */
	for (client = firstclient; client; client = cnext) {

		cnext = client->next;

		if (difftime(now, client->last_heard) > 60) {
			/* shed clients after 1 minute of inactivity */
			client_disconnect(client);
			continue;
		}

		if (nfds >= maxconn) {
			/* ignore clients that we are unable to handle */
			continue;
		}

		fds[nfds] = client->Event;

		handler[nfds].type = CLIENT;
		handler[nfds].data = client;

		nfds++;
	}

	/* scan through server sockets */
	for (server = firstaddr; server && (nfds < maxconn); server = server->next) {

		if (INVALID_FD_SOCK(server->sock_fd)) {
			continue;
		}

		fds[nfds] = server->Event;

		handler[nfds].type = SERVER;
		handler[nfds].data = server;

		nfds++;
	}

	/* Wait on the read IO on named pipe  */
	for (conn = pipe_connhead; conn; conn = conn->next) {
		fds[nfds] = conn->overlapped.hEvent;
		handler[nfds].type = NAMED_PIPE;
		handler[nfds].data = (void *)conn;
		nfds++;
	}
	/* Add the new named pipe connected event */
	fds[nfds] = pipe_connection_overlapped.hEvent;
	handler[nfds].type = NAMED_PIPE;
	handler[nfds].data = NULL;
	nfds++;

	upsdebugx(2, "%s: wait for %d filedescriptors", __func__, nfds);

	/* https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects */
	ret = WaitForMultipleObjects(nfds,fds,FALSE,2000);

	upsdebugx(6, "%s: wait for filedescriptors done: %" PRIu64, __func__, ret);

	if (ret == WAIT_TIMEOUT) {
		upsdebugx(2, "%s: no data available", __func__);
		return;
	}

	if (ret == WAIT_FAILED) {
		DWORD err = GetLastError();
		err = err; /* remove compile time warning */
		upslog_with_errno(LOG_ERR, "%s", __func__);
		upsdebugx(2, "%s: wait failed: code 0x%" PRIx64, __func__, err);
		return;
	}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	if (ret >= WAIT_ABANDONED_0 && ret <= WAIT_ABANDONED_0 + nfds - 1) {
		/* One abandoned mutex object that satisfied the wait? */
		ret = ret - WAIT_ABANDONED_0;
		upsdebugx(5, "%s: got abandoned FD array item: %" PRIu64, __func__, nfds, ret);
		/* FIXME: Should this be handled somehow? Cleanup? Abort?.. */
	} else
	if (ret >= WAIT_OBJECT_0 && ret <= WAIT_OBJECT_0 + nfds - 1) {
		/* Which one handle was triggered this time? */
		/* Note: WAIT_OBJECT_0 may be currently defined as 0,
		 * but docs insist on checking and shifting the range */
		ret = ret - WAIT_OBJECT_0;
		upsdebugx(5, "%s: got event on FD array item: %" PRIu64, __func__, nfds, ret);
	}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic pop
#endif

	if (ret >= nfds) {
		/* Array indexes are [0..nfds-1] */
		upsdebugx(2, "%s: unexpected response to query about data available: %" PRIu64, __func__, ret);
		return;
	}

	upsdebugx(6, "%s: requesting handler[%" PRIu64 "]", __func__, ret);
	upsdebugx(6, "%s: handler.type=%d handler.data=%p", __func__, handler[ret].type, handler[ret].data);

	switch(handler[ret].type) {
		case DRIVER:
			upsdebugx(4, "%s: calling sstate_readline() for DRIVER", __func__);
			sstate_readline((upstype_t *)handler[ret].data);
			break;
		case CLIENT:
			upsdebugx(4, "%s: calling client_readline() for CLIENT", __func__);
			client_readline((nut_ctype_t *)handler[ret].data);
			break;
		case SERVER:
			upsdebugx(4, "%s: calling client_connect() for SERVER", __func__);
			client_connect((stype_t *)handler[ret].data);
			break;
		case NAMED_PIPE:
			/* a new pipe connection has been signaled */
			if (fds[ret] == pipe_connection_overlapped.hEvent) {
				upsdebugx(4, "%s: calling pipe_connect() for NAMED_PIPE", __func__);
				pipe_connect();
			}
			/* one of the read event handle has been signaled */
			else {
				upsdebugx(4, "%s: calling pipe_ready() for NAMED_PIPE", __func__);
				pipe_conn_t * conn = handler[ret].data;
				if ( pipe_ready(conn) ) {
					if (!strncmp(conn->buf, SIGCMD_STOP, sizeof(SIGCMD_STOP))) {
						set_exit_flag(1);
					}
					else if (!strncmp(conn->buf, SIGCMD_RELOAD, sizeof(SIGCMD_RELOAD))) {
						set_reload_flag(1);
					}
					else {
						upslogx(LOG_ERR,"Unknown signal"
						       );
					}

					upsdebugx(4, "%s: calling pipe_disconnect() for NAMED_PIPE", __func__);
					pipe_disconnect(conn);
				}
			}
			break;
		default:
			upsdebugx(2, "%s: <unknown> has data available", __func__);
			break;
	}
#endif
}

static void help(const char *arg_progname)
	__attribute__((noreturn));

static void help(const char *arg_progname)
{
	printf("Network server for UPS data.\n\n");
	printf("usage: %s [OPTIONS]\n", arg_progname);

	printf("\n");
	printf("  -c <command>	send <command> via signal to background process\n");
	printf("		commands:\n");
	printf("		 - reload: reread configuration files\n");
	printf("		 - stop: stop process and exit\n");
#ifndef WIN32
	printf("  -P <pid>	send the signal above to specified PID (bypassing PID file)\n");
#endif
	printf("  -D		raise debugging level (and stay foreground by default)\n");
	printf("  -F		stay foregrounded even if no debugging is enabled\n");
	printf("  -FF		stay foregrounded and still save the PID file\n");
	printf("  -B		stay backgrounded even if debugging is bumped\n");
	printf("  -h		display this help text\n");
	printf("  -V		display the version of this software\n");
	printf("  -r <dir>	chroots to <dir>\n");
	printf("  -q		raise log level threshold\n");
	printf("  -u <user>	switch to <user> (if started as root)\n");
	printf("  -4		IPv4 only\n");
	printf("  -6		IPv6 only\n");

	nut_report_config_flags();

	exit(EXIT_SUCCESS);
}

static void setup_signals(void)
{
#ifndef WIN32
	struct sigaction	sa;

	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGHUP);
	sa.sa_flags = 0;

	/* basic signal setup to ignore SIGPIPE */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
	sa.sa_handler = SIG_IGN;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic pop
#endif
	sigaction(SIGPIPE, &sa, NULL);

	/* handle shutdown signals */
	sa.sa_handler = set_exit_flag;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* handle reloading */
	sa.sa_handler = set_reload_flag;
	sigaction(SIGHUP, &sa, NULL);
#else
	pipe_create(UPSD_PIPE_NAME);
#endif
}

void check_perms(const char *fn)
{
#ifndef WIN32
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
#else
	NUT_UNUSED_VARIABLE(fn);
#endif
}

int main(int argc, char **argv)
{
	int	i, cmdret = 0, foreground = -1;
#ifndef WIN32
	int	cmd = 0;
	pid_t	oldpid = -1;
#else
	const char * cmd = NULL;
#endif
	char	*chroot_path = NULL;
	const char	*user = RUN_AS_USER;
	struct passwd	*new_uid = NULL;

	progname = xbasename(argv[0]);

	/* yes, xstrdup - the conf handlers call free on this later */
	statepath = xstrdup(dflt_statepath());
#ifndef WIN32
	datapath = xstrdup(NUT_DATADIR);
#else
	datapath = getfullpath(PATH_SHARE);

	/* remove trailing .exe */
	char * drv_name;
	drv_name = (char *)xbasename(argv[0]);
	char * name = strrchr(drv_name,'.');
	if( name != NULL ) {
		if(strcasecmp(name, ".exe") == 0 ) {
			progname = strdup(drv_name);
			char * t = strrchr(progname,'.');
			*t = 0;
		}
	}
	else {
		progname = drv_name;
	}
#endif

	/* set up some things for later */
	snprintf(pidfn, sizeof(pidfn), "%s/%s.pid", altpidpath(), progname);

	printf("Network UPS Tools %s %s\n", progname, UPS_VERSION);

	while ((i = getopt(argc, argv, "+h46p:qr:i:fu:Vc:P:DFB")) != -1) {
		switch (i) {
			case 'p':
			case 'i':
				fatalx(EXIT_FAILURE, "Specifying a listening addresses with '-i <address>' and '-p <port>'\n"
					"is deprecated. Use 'LISTEN <address> [<port>]' in 'upsd.conf' instead.\n"
					"See 'man 8 upsd.conf' for more information.");
#ifndef HAVE___ATTRIBUTE__NORETURN
					exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif

			case 'q':
				nut_log_level++;
				break;

			case 'r':
				chroot_path = optarg;
				break;

			case 'u':
				user = optarg;
				break;

			case 'V':
				/* Note - we already printed the banner for program name */
				nut_report_config_flags();

				exit(EXIT_SUCCESS);

			case 'c':
				if (!strncmp(optarg, "reload", strlen(optarg))) {
					cmd = SIGCMD_RELOAD;
				} else
				if (!strncmp(optarg, "stop", strlen(optarg))) {
					cmd = SIGCMD_STOP;
				}

				/* bad command given */
				if (cmd == 0)
					help(progname);
				break;

#ifndef WIN32
			case 'P':
				if ((oldpid = parsepid(optarg)) < 0)
					help(progname);
				break;
#endif

			case 'D':
				nut_debug_level++;
				nut_debug_level_args++;
				break;
			case 'F':
				if (foreground > 0) {
					/* specified twice to save PID file anyway */
					foreground = 2;
				} else {
					foreground = 1;
				}
				break;
			case 'B':
				foreground = 0;
				break;

			case '4':
				opt_af = AF_INET;
				break;

			case '6':
				opt_af = AF_INET6;
				break;

			case 'h':
			default:
				help(progname);
		}
	}

	if (foreground < 0) {
		if (nut_debug_level > 0) {
			foreground = 1;
		} else {
			foreground = 0;
		}
	}

	{ /* scoping */
		char *s = getenv("NUT_DEBUG_LEVEL");
		int l;
		if (s && str_to_int(s, &l, 10)) {
			if (l > 0 && nut_debug_level_args < 1) {
				upslogx(LOG_INFO, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
					"since none was requested by command-line options", l);
				nut_debug_level = l;
				nut_debug_level_args = l;
			}	/* else follow -D settings */
		}	/* else nothing to bother about */
	}

	/* Note: "cmd" may be non-trivial to command that instance by
	 * explicit PID number or lookup in PID file (error if absent).
	 * Otherwise, we are being asked to start and "cmd" is 0/NULL -
	 * for probing whether a competing older instance of this program
	 * is running (error if it is).
	 */
#ifndef WIN32
	/* If cmd == 0 we are starting and check if a previous instance
	 * is running by sending signal '0' (i.e. 'kill <pid> 0' equivalent)
	 */

	if (oldpid < 0) {
		cmdret = sendsignalfn(pidfn, cmd);
	} else {
		cmdret = sendsignalpid(oldpid, cmd);
	}
#else	/* if WIN32 */
	if (cmd) {
		/* Command the running daemon, it should be there */
		cmdret = sendsignal(UPSD_PIPE_NAME, cmd);
	} else {
		/* Starting new daemon, check for competition */
		mutex = CreateMutex(NULL, TRUE, UPSD_PIPE_NAME);
		if (mutex == NULL) {
			if (GetLastError() != ERROR_ACCESS_DENIED) {
				fatalx(EXIT_FAILURE,
					"Can not create mutex %s : %d.\n",
					UPSD_PIPE_NAME, (int)GetLastError());
			}
		}

		cmdret = -1; /* unknown, maybe ok */
		if (GetLastError() == ERROR_ALREADY_EXISTS
		||  GetLastError() == ERROR_ACCESS_DENIED
		) {
			cmdret = 0; /* known conflict */
		}
	}
#endif	/* WIN32 */

	switch (cmdret) {
	case 0:
		if (cmd) {
			upsdebugx(1, "Signaled old daemon OK");
		} else {
			printf("Fatal error: A previous upsd instance is already running!\n");
			printf("Either stop the previous instance first, or use the 'reload' command.\n");
			exit(EXIT_FAILURE);
		}
		break;

	case -3:
	case -2:
		/* if starting new daemon, no competition running -
		 *    maybe OK (or failed to detect it => problem)
		 * if signaling old daemon - certainly have a problem
		 */
		upslogx(LOG_WARNING, "Could not %s PID file '%s' "
			"to see if previous upsd instance is "
			"already running!",
			(cmdret == -3 ? "find" : "parse"),
			pidfn);
		break;

	case -1:
	case 1:	/* WIN32 */
	default:
		/* if cmd was nontrivial - speak up below, else be quiet */
		upsdebugx(1, "Just failed to send signal, no daemon was running");
		break;
	}

	if (cmd) {
		/* We were signalling a daemon, successfully or not - exit now... */
		if (cmdret != 0) {
			/* sendsignal*() above might have logged more details
			 * for troubleshooting, e.g. about lack of PID file
			 */
			upslogx(LOG_NOTICE, "Failed to signal the currently running daemon (if any)");
#ifndef WIN32
# ifdef HAVE_SYSTEMD
			switch (cmd) {
			case SIGCMD_RELOAD:
				upslogx(LOG_NOTICE, "Try 'systemctl reload %s'%s",
					SERVICE_UNIT_NAME,
					(oldpid < 0 ? " or add '-P $PID' argument" : ""));
				break;
			case SIGCMD_STOP:
				upslogx(LOG_NOTICE, "Try 'systemctl stop %s'%s",
					SERVICE_UNIT_NAME,
					(oldpid < 0 ? " or add '-P $PID' argument" : ""));
				break;
			default:
				upslogx(LOG_NOTICE, "Try 'systemctl <command> %s'%s",
					SERVICE_UNIT_NAME,
					(oldpid < 0 ? " or add '-P $PID' argument" : ""));
				break;
			}
			/* ... or edit nut-server.service locally to start `upsd -FF`
			 * and so save the PID file for ability to manage the daemon
			 * beside the service framework, possibly confusing things...
			 */
# else
			if (oldpid < 0) {
				upslogx(LOG_NOTICE, "Try to add '-P $PID' argument");
			}
# endif
#endif	/* not WIN32 */
		}

		exit((cmdret == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	argc -= optind;
	argv += optind;

	if (argc != 0) {
		help(progname);
	}

	atexit(upsd_cleanup);

	setup_signals();

	open_syslog(progname);

	/* send logging to the syslog pre-background for later use */
	syslogbit_set();

	/* do this here, since getpwnam() might not work in the chroot */
	new_uid = get_user_pwent(user);

	if (chroot_path) {
		chroot_start(chroot_path);
	}

#ifndef WIN32
	/* default to system limit (may be overridden in upsd.conf) */
	/* FIXME: Check for overflows (and int size of nfds_t vs. long) - see get_max_pid_t() for example */
	maxconn = (nfds_t)sysconf(_SC_OPEN_MAX);
#else
	maxconn = 64;  /*FIXME : arbitrary value, need adjustement */
#endif

	/* handle upsd.conf */
	load_upsdconf(0);	/* 0 = initial */

	/* CLI debug level can not be smaller than debug_min specified
	 * in upsd.conf. Note that non-zero debug_min does not impact
	 * foreground running mode.
	 */
	if (nut_debug_level_global > nut_debug_level)
		nut_debug_level = nut_debug_level_global;
	upsdebugx(1, "debug level is '%d'", nut_debug_level);

	{ /* scope */
	/* As documented above, the ALLOW_NO_DEVICE can be provided via
	 * envvars and then has higher priority than an upsd.conf setting
	 */
	const char *envvar = getenv("ALLOW_NO_DEVICE");
	if ( envvar != NULL) {
		if ( (!strncasecmp("TRUE", envvar, 4)) || (!strncasecmp("YES", envvar, 3)) || (!strncasecmp("ON", envvar, 2)) || (!strncasecmp("1", envvar, 1)) ) {
			/* Admins of this server expressed a desire to serve
			 * anything on the NUT protocol, even if nothing is
			 * configured yet - tell the clients so, properly.
			 */
			allow_no_device = 1;
		} else if ( (!strncasecmp("FALSE", envvar, 5)) || (!strncasecmp("NO", envvar, 2)) || (!strncasecmp("OFF", envvar, 3)) || (!strncasecmp("0", envvar, 1)) ) {
			/* Admins of this server expressed a desire to serve
			 * anything on the NUT protocol, even if nothing is
			 * configured yet - tell the clients so, properly.
			 */
			allow_no_device = 0;
		}
	}
	} /* scope */

	{ /* scope */
	/* As documented above, the ALLOW_NOT_ALL_LISTENERS can be provided via
	 * envvars and then has higher priority than an upsd.conf setting
	 */
	const char *envvar = getenv("ALLOW_NOT_ALL_LISTENERS");
	if ( envvar != NULL) {
		if ( (!strncasecmp("TRUE", envvar, 4)) || (!strncasecmp("YES", envvar, 3)) || (!strncasecmp("ON", envvar, 2)) || (!strncasecmp("1", envvar, 1)) ) {
			/* Admins of this server expressed a desire to serve
			 * NUT protocol if at least one configured listener
			 * works (some may be missing and clients using those
			 * addresses would not be served!)
			 */
			allow_not_all_listeners = 1;
		} else if ( (!strncasecmp("FALSE", envvar, 5)) || (!strncasecmp("NO", envvar, 2)) || (!strncasecmp("OFF", envvar, 3)) || (!strncasecmp("0", envvar, 1)) ) {
			/* Admins of this server expressed a desire to serve
			 * NUT protocol only if all configured listeners work
			 * (default for least surprise - admins must address
			 * any configuration inconsistencies!)
			 */
			allow_not_all_listeners = 0;
		}
	}
	} /* scope */

	/* start server */
	server_load();

	become_user(new_uid);
#ifndef WIN32
	if (chdir(statepath)) {
		fatal_with_errno(EXIT_FAILURE, "Can't chdir to %s", statepath);
	} else {
		upsdebugx(1, "chdired into statepath %s for driver sockets", statepath);
	}
#endif

	/* check statepath perms */
	check_perms(statepath);

	/* handle ups.conf */
	read_upsconf(1);	/* 1 = may abort upon fundamental errors */
	upsconf_add(0);		/* 0 = initial */
	poll_reload();

	if (num_ups == 0) {
		if (allow_no_device) {
			upslogx(LOG_WARNING, "Normally at least one UPS must be defined in ups.conf, currently there are none (please configure the file and reload the service)");
		} else {
			fatalx(EXIT_FAILURE, "Fatal error: at least one UPS must be defined in ups.conf");
		}
	} else {
		upslogx(LOG_INFO, "Found %d UPS defined in ups.conf", num_ups);
	}

	/* try to bring in the var/cmd descriptions */
	desc_load();

	/* handle upsd.users */
	user_load();

	if (!foreground) {
		background();
		writepid(pidfn);
	} else {
		if (foreground == 2) {
			upslogx(LOG_WARNING, "Running as foreground process, but saving a PID file anyway");
			writepid(pidfn);
		} else {
			upslogx(LOG_WARNING, "Running as foreground process, not saving a PID file");
			memset(pidfn, 0, sizeof(pidfn));
		}
	}

	/* initialize SSL (keyfile must be readable by nut user) */
	ssl_init();

	upsnotify(NOTIFY_STATE_READY_WITH_PID, NULL);

	while (!exit_flag) {
		/* Note: mainloop() calls upsnotify(NOTIFY_STATE_WATCHDOG, NULL); */
		mainloop();
	}

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
	upsnotify(NOTIFY_STATE_STOPPING, "Signal %d: exiting", exit_flag);

	ssl_cleanup();
	return EXIT_SUCCESS;
}
