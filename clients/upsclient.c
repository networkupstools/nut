/* upsclient - network communications functions for UPS clients

   Copyright (C)
	2002	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#include "config.h"	/* safe because it doesn't contain prototypes */

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "upsclient.h"
#include "timehead.h"

#define UPSCLIENT_MAGIC 0x19980308

#define SMALLBUF	512

#ifdef SHUT_RDWR
#define shutdown_how SHUT_RDWR
#else
#define shutdown_how 2
#endif

struct {
	int	flags;
	const	char	*str;
} upscli_errlist[] =
{
	{ 0, "Unknown error"			},	/*  0: UPSCLI_ERR_UNKNOWN */
	{ 0, "Variable not supported by UPS"	},	/*  1: UPSCLI_ERR_VARNOTSUPP */
	{ 0, "No such host"			},	/*  2: UPSCLI_ERR_NOSUCHHOST */
	{ 0, "Invalid response from server"	},	/*  3: UPSCLI_ERR_INVRESP */
	{ 0, "Unknown UPS"			},	/*  4: UPSCLI_ERR_UNKNOWNUPS */
	{ 0, "Invalid list type"		},	/*  5: UPSCLI_ERR_INVLISTTYPE */
	{ 0, "Access denied"			},	/*  6: UPSCLI_ERR_ACCESSDENIED */
	{ 0, "Password required"		},	/*  7: UPSCLI_ERR_PWDREQUIRED */
	{ 0, "Password incorrect"		},	/*  8: UPSCLI_ERR_PWDINCORRECT */
	{ 0, "Missing argument"			},	/*  9: UPSCLI_ERR_MISSINGARG */
	{ 0, "Data stale"			},	/* 10: UPSCLI_ERR_DATASTALE */
	{ 0, "Variable unknown"			},	/* 11: UPSCLI_ERR_VARUNKNOWN */
	{ 0, "Already logged in"		},	/* 12: UPSCLI_ERR_LOGINTWICE */
	{ 0, "Already set password"		},	/* 13: UPSCLI_ERR_PWDSETTWICE */
	{ 0, "Unknown variable type"		},	/* 14: UPSCLI_ERR_UNKNOWNTYPE */
	{ 0, "Unknown variable"			},	/* 15: UPSCLI_ERR_UNKNOWNVAR */
	{ 0, "Read-only variable"		},	/* 16: UPSCLI_ERR_VARREADONLY */
	{ 0, "New value is too long"		},	/* 17: UPSCLI_ERR_TOOLONG */
	{ 0, "Invalid value for variable"	},	/* 18: UPSCLI_ERR_INVALIDVALUE */
	{ 0, "Set command failed"		},	/* 19: UPSCLI_ERR_SETFAILED */
	{ 0, "Unknown instant command"		},	/* 20: UPSCLI_ERR_UNKINSTCMD */
	{ 0, "Instant command failed"		},	/* 21: UPSCLI_ERR_CMDFAILED */
	{ 0, "Instant command not supported"	},	/* 22: UPSCLI_ERR_CMDNOTSUPP */
	{ 0, "Invalid username"			},	/* 23: UPSCLI_ERR_INVUSERNAME */
	{ 0, "Already set username"		},	/* 24: UPSCLI_ERR_USERSETTWICE */
	{ 0, "Unknown command"			},	/* 25: UPSCLI_ERR_UNKCOMMAND */
	{ 0, "Invalid argument"			},	/* 26: UPSCLI_ERR_INVALIDARG */
	{ 1, "Send failure: %s"			},	/* 27: UPSCLI_ERR_SENDFAILURE */
	{ 1, "Receive failure: %s"		},	/* 28: UPSCLI_ERR_RECVFAILURE */
	{ 1, "socket failure: %s"		},	/* 29: UPSCLI_ERR_SOCKFAILURE */
	{ 1, "bind failure: %s"			},	/* 30: UPSCLI_ERR_BINDFAILURE */
	{ 1, "Connection failure: %s"		},	/* 31: UPSCLI_ERR_CONNFAILURE */
	{ 1, "Write error: %s"			},	/* 32: UPSCLI_ERR_WRITE */
	{ 1, "Read error: %s"			},	/* 33: UPSCLI_ERR_READ */
	{ 0, "Invalid password"			},	/* 34: UPSCLI_ERR_INVPASSWORD */
	{ 0, "Username required"		},	/* 35: UPSCLI_ERR_USERREQUIRED */
	{ 0, "SSL is not available",		},	/* 36: UPSCLI_ERR_SSLFAIL */
	{ 2, "SSL error: %s",			},	/* 37: UPSCLI_ERR_SSLERR */
	{ 0, "Server disconnected",		},	/* 38: UPSCLI_ERR_SRVDISC */
	{ 0, "Driver not connected",		},	/* 39: UPSCLI_ERR_DRVNOTCONN */
	{ 0, "Memory allocation failure",	},	/* 40: UPSCLI_ERR_NOMEM */
	{ 3, "Parse error: %s",			},	/* 41: UPSCLI_ERR_PARSE */
	{ 0, "Protocol error",			},	/* 42: UPSCLI_ERR_PROTOCOL */
};

const char *upscli_strerror(UPSCONN_t *ups)
{
#ifdef HAVE_SSL
	unsigned long	err;
	char	sslbuf[UPSCLI_ERRBUF_LEN];
#endif

	if (!ups) {
		return upscli_errlist[UPSCLI_ERR_INVALIDARG].str;
	}

	if (ups->upsclient_magic != UPSCLIENT_MAGIC) {
		return upscli_errlist[UPSCLI_ERR_INVALIDARG].str;
	}

	if (ups->upserror > UPSCLI_ERR_MAX) {
		return "Invalid error number";
	}

	switch (upscli_errlist[ups->upserror].flags) {

	case 0:		/* simple error */
		return upscli_errlist[ups->upserror].str;

	case 1:		/* add message from system's strerror */
		snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN,
			upscli_errlist[ups->upserror].str,
			strerror(ups->syserrno));
		return ups->errbuf;

	case 2:		/* SSL error */
#ifdef HAVE_SSL
		err = ERR_get_error();
		if (err) {
			ERR_error_string(err, sslbuf);
			snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN,
				upscli_errlist[ups->upserror].str,
				sslbuf);
		} else {
			snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN,
				upscli_errlist[ups->upserror].str,
				"peer disconnected");
		}
#else
		snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN, 
			"SSL error, but SSL wasn't enabled at compile-time");
#endif	/* HAVE_SSL */
		return ups->errbuf;

	case 3:		/* parsing (parseconf) error */
		snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN,
			upscli_errlist[ups->upserror].str,
			ups->pc_ctx.errmsg);
		return ups->errbuf;
	}

	/* fallthrough */

	snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN, "Unknown error flag %d",
		upscli_errlist[ups->upserror].flags);

	return ups->errbuf;
}

/* Read up to buflen bytes from fd and return the number of bytes
   read. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
static int select_read(const int fd, void *buf, const size_t buflen, const long d_sec, const long d_usec)
{
	int		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	ret = select(fd + 1, &fds, NULL, NULL, &tv);

	if (ret < 1) {
		return ret;
	}

	return read(fd, buf, buflen);
}

/* internal: abstract the SSL calls for the other functions */
static int net_read(UPSCONN_t *ups, char *buf, size_t buflen)
{
	int	ret;

#ifdef HAVE_SSL
	if (ups->ssl) {
		ret = SSL_read(ups->ssl, buf, buflen);

		if (ret < 1) {
			ups->upserror = UPSCLI_ERR_SSLERR;
		}

		return ret;
	}
#endif

	ret = select_read(ups->fd, buf, buflen, 5, 0);

	/* error reading data, server disconnected? */
	if (ret < 0) {
		ups->upserror = UPSCLI_ERR_READ;
		ups->syserrno = errno;
	}

	/* no data available, server disconnected? */
	if (ret == 0) {
		ups->upserror = UPSCLI_ERR_SRVDISC;
	}

	return ret;
}

/* Write up to buflen bytes to fd and return the number of bytes
   written. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
static int select_write(const int fd, const void *buf, const size_t buflen, const long d_sec, const long d_usec)
{
	int		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	ret = select(fd + 1, NULL, &fds, NULL, &tv);

	if (ret < 1) {
		return ret;
	}

	return write(fd, buf, buflen);
}

/* internal: abstract the SSL calls for the other functions */
static int net_write(UPSCONN_t *ups, const char *buf, size_t buflen)
{
	int	ret;

#ifdef HAVE_SSL
	if (ups->ssl) {
		ret = SSL_write(ups->ssl, buf, buflen);

		if (ret < 1) {
			ups->upserror = UPSCLI_ERR_SSLERR;
		}

		return ret;
	}
#endif

	ret = select_write(ups->fd, buf, buflen, 0, 0);

	/* error writing data, server disconnected? */
	if (ret < 0) {
		ups->upserror = UPSCLI_ERR_WRITE;
		ups->syserrno = errno;
	}

	/* not ready for writing, server disconnected? */
	if (ret == 0) {
		ups->upserror = UPSCLI_ERR_SRVDISC;
	}

	return ret;
}

/* stub first */
#ifndef HAVE_SSL
static int upscli_sslinit(UPSCONN_t *ups)
{
	return 0;	/* not supported */
}

int upscli_sslcert(UPSCONN_t *ups, const char *dir, const char *file, int verify)
{
	if (!ups) {
		return -1;
	}

	/* if forcing the verification, this fails since we have no SSL */
	if (verify == 1) {
		ups->upserror = UPSCLI_ERR_SSLFAIL;
		return -1;
	}
		
	return 0;		/* not supported */
}

#else

static int upscli_sslinit(UPSCONN_t *ups)
{
	char	buf[UPSCLI_NETBUF_LEN];

	/* see if upsd even talks SSL/TLS */
	snprintf(buf, sizeof(buf), "STARTTLS\n");

	if (upscli_sendline(ups, buf, strlen(buf)) != 0) {
		return -1;
	}

	if (upscli_readline(ups, buf, sizeof(buf)) != 0) {
		return -1;
	}

	if (strncmp(buf, "OK STARTTLS", 11) != 0) {
		return 0;		/* not supported */
	}

	/* upsd is happy, so let's crank up the client */

	SSL_library_init();
	SSL_load_error_strings();

	ups->ssl_ctx = SSL_CTX_new(TLSv1_client_method());

	if (!ups->ssl_ctx) {
		return 0;
	}

	ups->ssl = SSL_new(ups->ssl_ctx);

	if (!ups->ssl) {
		return 0;
	}

	if (SSL_set_fd(ups->ssl, ups->fd) != 1) {
		return -1;
	}

	SSL_set_connect_state(ups->ssl);

	return 1;	/* OK */
}

/* set the paths for the certs to verify the server */
int upscli_sslcert(UPSCONN_t *ups, const char *file, const char *path, int verify)
{
	int	ret, ssl_mode = SSL_VERIFY_NONE;

	if (!ups) {
		return -1;
	}

	if (!ups->ssl_ctx) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	switch(verify) {

	case 0:
		ssl_mode = SSL_VERIFY_NONE;
		break;
	case 1:
		ssl_mode = SSL_VERIFY_PEER;
		break;
	default:
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	ret = SSL_CTX_load_verify_locations(ups->ssl_ctx, file, path);

	if (ret != 1) {
		ups->upserror = UPSCLI_ERR_SSLERR;
		return -1;
	}

	SSL_set_verify(ups->ssl, ssl_mode, NULL);

	return 1;
}

#endif	/* HAVE_SSL */

int upscli_connect(UPSCONN_t *ups, const char *host, int port, int flags)
{
	int	sock_fd;
#ifndef	HAVE_IPV6
	struct sockaddr_in	local, server;
	struct hostent		*serv;
#else
	struct addrinfo	hints, *res, *ai;
	char			sport[NI_MAXSERV];
	int			v;
#endif

	if (!ups) {
		return -1;
	}

	/* clear out any lingering junk */
	memset(ups, 0, sizeof(*ups));
	ups->upsclient_magic = UPSCLIENT_MAGIC;
	ups->fd = -1;

	if (!host) {
		ups->upserror = UPSCLI_ERR_NOSUCHHOST;
		return -1;
	}

#ifndef	HAVE_IPV6
	serv = gethostbyname(host);

	if (!serv) {
		struct  in_addr	listenaddr;

		if (!inet_aton(host, &listenaddr)) {
			ups->upserror = UPSCLI_ERR_NOSUCHHOST;
			return -1;
		}

		serv = gethostbyaddr(&listenaddr, sizeof(listenaddr), AF_INET);

		if (!serv) {
			ups->upserror = UPSCLI_ERR_NOSUCHHOST;
			return -1;
		}
	}

	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ups->upserror = UPSCLI_ERR_SOCKFAILURE;
		ups->syserrno = errno;
		close(sock_fd);
		return -1;
	}

	memset(&local, '\0', sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(INADDR_ANY);

	memset(&server, '\0', sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	memcpy(&server.sin_addr, serv->h_addr, serv->h_length);

	if (bind(sock_fd, (struct sockaddr *) &local, sizeof(local)) < 0) {
		ups->upserror = UPSCLI_ERR_BINDFAILURE;
		ups->syserrno = errno;
		close(sock_fd);
		return -1;
	}

	if (connect(sock_fd, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) < 0) {
		ups->upserror = UPSCLI_ERR_CONNFAILURE;
		ups->syserrno = errno;
		close(sock_fd);
		return -1;
	}

	ups->fd = sock_fd;
#else
	snprintf(sport, sizeof(sport), "%hu", (unsigned short int)port);

	memset(&hints, 0, sizeof(hints));

	if (flags & UPSCLI_CONN_INET6) {
		hints.ai_family = AF_INET6;
	} else if (flags & UPSCLI_CONN_INET) {
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_UNSPEC;
	}

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	while ((v = getaddrinfo(host, sport, &hints, &res)) != 0) {
		switch (v)
		{
		case EAI_AGAIN:
			continue;
		case EAI_NONAME:
			ups->upserror = UPSCLI_ERR_NOSUCHHOST;
			return -1;
		case EAI_MEMORY:
			ups->upserror = UPSCLI_ERR_NOMEM;
			return -1;
		case EAI_SYSTEM:
			ups->syserrno = errno;
			break;
		}

		ups->upserror = UPSCLI_ERR_UNKNOWN;
		return -1;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {

		sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (sock_fd < 0) {
			switch (errno)
			{
			case EAFNOSUPPORT:
			case EINVAL:
                                break;
			default:
				ups->upserror = UPSCLI_ERR_SOCKFAILURE;
				ups->syserrno = errno;
			}
			continue;
		}

		while ((v = connect(sock_fd, ai->ai_addr, ai->ai_addrlen)) < 0) {
			switch (errno)
			{
			case EAFNOSUPPORT:
				break;
			case EINTR:
			case EAGAIN:
				continue;
			default:
				ups->upserror = UPSCLI_ERR_CONNFAILURE;
				ups->syserrno = errno;
			}
			break;
		}

		if (v < 0) {
			close(sock_fd);
			continue;
		}

		ups->fd = sock_fd;
		ups->upserror = 0;
		ups->syserrno = 0;
		break;
	}

	freeaddrinfo(res);

	if (ups->fd < 0) {
		return -1;
	}
#endif
	pconf_init(&ups->pc_ctx, NULL);

	ups->host = strdup(host);

	if (!ups->host) {
		ups->upserror = UPSCLI_ERR_NOMEM;
		upscli_disconnect(ups);
		return -1;
	}

	ups->port = port;

	if (flags & UPSCLI_CONN_TRYSSL) {
		upscli_sslinit(ups);

		/* see if something made us die inside sslinit */
		if (ups->upserror != 0) {
			upscli_disconnect(ups);
			return -1;
		}
	}

	if ((flags & UPSCLI_CONN_REQSSL) && (upscli_sslinit(ups) != 1)) {
		ups->upserror = UPSCLI_ERR_SSLFAIL;
		upscli_disconnect(ups);
		return -1;
	}
		
	return 0;
}

/* map upsd error strings back to upsclient internal numbers */
static struct {
	int	errnum;
	const	char	*text;
}	upsd_errlist[] =
{
	{ UPSCLI_ERR_VARNOTSUPP,	"VAR-NOT-SUPPORTED"	},	
	{ UPSCLI_ERR_UNKNOWNUPS,	"UNKNOWN-UPS"		},
	{ UPSCLI_ERR_ACCESSDENIED, 	"ACCESS-DENIED"		},
	{ UPSCLI_ERR_PWDREQUIRED,	"PASSWORD-REQUIRED"	},
	{ UPSCLI_ERR_PWDINCORRECT,	"PASSWORD-INCORRECT"	},
	{ UPSCLI_ERR_MISSINGARG,	"MISSING-ARGUMENT"	},
	{ UPSCLI_ERR_DATASTALE,		"DATA-STALE"		},
	{ UPSCLI_ERR_VARUNKNOWN,	"VAR-UNKNOWN"		},
	{ UPSCLI_ERR_LOGINTWICE,	"ALREADY-LOGGED-IN"	},
	{ UPSCLI_ERR_PWDSETTWICE,	"ALREADY-SET-PASSWORD"	},
	{ UPSCLI_ERR_UNKNOWNTYPE,	"UNKNOWN-TYPE"		},
	{ UPSCLI_ERR_UNKNOWNVAR,	"UNKNOWN-VAR"		},
	{ UPSCLI_ERR_VARREADONLY,	"READONLY"		},
	{ UPSCLI_ERR_TOOLONG,		"TOO-LONG"		},
	{ UPSCLI_ERR_INVALIDVALUE,	"INVALID-VALUE"		},
	{ UPSCLI_ERR_SETFAILED,		"SET-FAILED"		},
	{ UPSCLI_ERR_UNKINSTCMD,	"UNKNOWN-INSTCMD"	},
	{ UPSCLI_ERR_CMDFAILED,		"INSTCMD-FAILED"	},
	{ UPSCLI_ERR_CMDNOTSUPP,	"CMD-NOT-SUPPORTED"	},
	{ UPSCLI_ERR_INVUSERNAME,	"INVALID-USERNAME"	},
	{ UPSCLI_ERR_USERSETTWICE,	"ALREADY-SET-USERNAME"	},
	{ UPSCLI_ERR_UNKCOMMAND,	"UNKNOWN-COMMAND"	},
	{ UPSCLI_ERR_INVPASSWORD,	"INVALID-PASSWORD"	},
	{ UPSCLI_ERR_USERREQUIRED,	"USERNAME-REQUIRED"	},
	{ UPSCLI_ERR_DRVNOTCONN,	"DRIVER-NOT-CONNECTED"	},
	
	{ 0,			NULL,		}
};

static int upscli_errcheck(UPSCONN_t *ups, char *buf)
{
	int	i;

	if (!ups) {
		return -1;
	}

	if (!buf) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	/* see if it's even an error now */
	if (strncmp(buf, "ERR", 3) != 0) {
		return 0;
	}

	/* look it up in the table */
	for (i = 0; upsd_errlist[i].text != NULL; i++) {
		if (!strncmp(&buf[4], upsd_errlist[i].text,
			strlen(upsd_errlist[i].text))) {
			ups->upserror = upsd_errlist[i].errnum;
			return -1;
		}
	}

	/* hmm - don't know what upsd is telling us */
	ups->upserror = UPSCLI_ERR_UNKNOWN;
	return -1;
}

static void build_cmd(char *buf, size_t bufsize, const char *cmdname,
	int numarg, const char **arg)
{
	int	i;
	size_t	len;
	char	enc[UPSCLI_NETBUF_LEN];
	const char	*format;

	memset(buf, '\0', bufsize);
	snprintf(buf, bufsize, "%s", cmdname);

	/* encode all arguments so they arrive intact */
	for (i = 0; i < numarg; i++) {

		if (strchr(arg[i], ' ')) {
			format = " \"%s\"";	/* wrap in "" */
		} else {
			format = " %s";
		}

		/* snprintfcat would tie us to common */

		len = strlen(buf);
		snprintf(buf + len, bufsize - len, format, 
			pconf_encode(arg[i], enc, sizeof(enc)));
	}

	len = strlen(buf);
	snprintf(buf + len, bufsize - len, "\n");
}

/* make sure upsd is giving us what we asked for */
static int verify_resp(int num, const char **q, char **a)
{
	int	i;

	for (i = 0; i < num; i++) {
		if (strcasecmp(q[i], a[i]) != 0) {

			/* FUTURE: handle -/+ options here */
			return 0;	/* mismatch */
		}
	}

	return 1;	/* OK */
}

int upscli_get(UPSCONN_t *ups, unsigned int numq, const char **query, 
		unsigned int *numa, char ***answer)
{
	char	cmd[UPSCLI_NETBUF_LEN], tmp[UPSCLI_NETBUF_LEN];
	
	if (!ups) {
		return -1;
	}

	if (numq < 1) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	/* create the string to send to upsd */
	build_cmd(cmd, sizeof(cmd), "GET", numq, query);

	if (upscli_sendline(ups, cmd, strlen(cmd)) != 0) {
		return -1;
	}

	if (upscli_readline(ups, tmp, sizeof(tmp)) != 0) {
		return -1;
	}

	if (upscli_errcheck(ups, tmp) != 0) {
		return -1;
	}

	if (!pconf_line(&ups->pc_ctx, tmp)) {
		ups->upserror = UPSCLI_ERR_PARSE;
		return -1;
	}

	/* q: [GET] VAR <ups> <var>   *
	 * a: VAR <ups> <var> <val> */

	if (ups->pc_ctx.numargs < numq) {
		ups->upserror = UPSCLI_ERR_PROTOCOL;
		return -1;
	}

	if (!verify_resp(numq, query, ups->pc_ctx.arglist)) {
		ups->upserror = UPSCLI_ERR_PROTOCOL;
		return -1;
	}

	*numa = ups->pc_ctx.numargs;
	*answer = ups->pc_ctx.arglist;

	return 0;
}

int upscli_list_start(UPSCONN_t *ups, unsigned int numq, const char **query)
{
	char	cmd[UPSCLI_NETBUF_LEN], tmp[UPSCLI_NETBUF_LEN];

	if (!ups) {
		return -1;
	}

	if (numq < 1) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	/* create the string to send to upsd */
	build_cmd(cmd, sizeof(cmd), "LIST", numq, query);

	if (upscli_sendline(ups, cmd, strlen(cmd)) != 0) {
		return -1;
	}

	if (upscli_readline(ups, tmp, sizeof(tmp)) != 0) {
		return -1;
	}

	if (upscli_errcheck(ups, tmp) != 0) {
		return -1;
	}

	if (!pconf_line(&ups->pc_ctx, tmp)) {
		ups->upserror = UPSCLI_ERR_PARSE;
		return -1;
	}

	if (ups->pc_ctx.numargs < 2) {
		ups->upserror = UPSCLI_ERR_PROTOCOL;
		return -1;
	}

	/* the response must start with BEGIN LIST */
	if ((strcasecmp(ups->pc_ctx.arglist[0], "BEGIN") != 0) ||
		(strcasecmp(ups->pc_ctx.arglist[1], "LIST") != 0)) {
		ups->upserror = UPSCLI_ERR_PROTOCOL;
		return -1;
	}

	/* q: [LIST] VAR <ups>       *
	 * a: [BEGIN LIST] VAR <ups> */

	/* compare q[0]... to a[2]... */

	if (!verify_resp(numq, query, &ups->pc_ctx.arglist[2])) {
		ups->upserror = UPSCLI_ERR_PROTOCOL;
		return -1;
	}

	return 0;
}

int upscli_list_next(UPSCONN_t *ups, unsigned int numq, const char **query, 
		unsigned int *numa, char ***answer)
{
	char	tmp[UPSCLI_NETBUF_LEN];

	if (!ups) {
		return -1;
	}

	if (upscli_readline(ups, tmp, sizeof(tmp)) != 0) {
		return -1;
	}

	if (upscli_errcheck(ups, tmp) != 0) {
		return -1;
	}

	if (!pconf_line(&ups->pc_ctx, tmp)) {
		ups->upserror = UPSCLI_ERR_PARSE;
		return -1;
	}

	if (ups->pc_ctx.numargs < 1) {
		ups->upserror = UPSCLI_ERR_PROTOCOL;
		return -1;
	}

	*numa = ups->pc_ctx.numargs;
	*answer = ups->pc_ctx.arglist;

	/* see if this is the end */
	if (ups->pc_ctx.numargs >= 2) {
		if ((!strcmp(ups->pc_ctx.arglist[0], "END")) &&
			(!strcmp(ups->pc_ctx.arglist[1], "LIST")))
			return 0;
	}

	/* q: VAR <ups> */
	/* a: VAR <ups> <val> */

	if (!verify_resp(numq, query, ups->pc_ctx.arglist)) {
		ups->upserror = UPSCLI_ERR_PROTOCOL;
		return -1;
	}

	/* just another part of the list */
	return 1;
}

int upscli_sendline(UPSCONN_t *ups, const char *buf, size_t buflen)
{
	int	ret;

	if (!ups) {
		return -1;
	}

	if (ups->fd < 0) {
		ups->upserror = UPSCLI_ERR_DRVNOTCONN;
		return -1;
	}

	if ((!buf) || (buflen < 1)) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	if (ups->upsclient_magic != UPSCLIENT_MAGIC) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	ret = net_write(ups, buf, buflen);

	if (ret < 1) {
		upscli_disconnect(ups);
		return -1;
	}

	return 0;
}

int upscli_readline(UPSCONN_t *ups, char *buf, size_t buflen)
{
	int	ret;
	size_t	recv;

	if (!ups) {
		return -1;
	}

	if (ups->fd < 0) {
		ups->upserror = UPSCLI_ERR_DRVNOTCONN;
		return -1;
	}

	if ((!buf) || (buflen < 1)) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	if (ups->upsclient_magic != UPSCLIENT_MAGIC) {
		ups->upserror = UPSCLI_ERR_INVALIDARG;
		return -1;
	}

	for (recv = 0; recv < (buflen-1); recv++) {

		if (ups->readidx == ups->readlen) {

			ret = net_read(ups, ups->readbuf, sizeof(ups->readbuf));

			if (ret < 1) {
				upscli_disconnect(ups);
				return -1;
			}

			ups->readlen = ret;
			ups->readidx = 0;
		}

		buf[recv] = ups->readbuf[ups->readidx++];

		if (buf[recv] == '\n') {
			break;
		}
	}

	buf[recv] = '\0';
	return 0;
}

/* split upsname[@hostname[:port]] into separate components */
int upscli_splitname(const char *buf, char **upsname, char **hostname, int *port)
{
	char	*s, tmp[SMALLBUF], *last = NULL;

	/* paranoia */
	if ((!buf) || (!upsname) || (!hostname) || (!port)) {
		return -1;
	}

	if (snprintf(tmp, sizeof(tmp), "%s", buf) < 1) {
		fprintf(stderr, "upscli_splitname: can't parse empty string\n");
		return -1;
	}

	s = strchr(tmp, '@');

	if ((*upsname = strdup(strtok_r(tmp, "@", &last))) == NULL) {
		fprintf(stderr, "upscli_splitname: strdup failed\n");
		return -1;
	}

	/* only a upsname is specified, fill in defaults */
	if (s == NULL) {
		if ((*hostname = strdup("localhost")) == NULL) {
			fprintf(stderr, "upscli_splitname: strdup failed\n");
			return -1;
		}

		*port = PORT;
		return 0;
	}

	return upscli_splitaddr(s+1, hostname, port);
}

/* split hostname[:port] into separate components */
int upscli_splitaddr(const char *buf, char **hostname, int *port)
{
	char	*s, tmp[SMALLBUF], *last = NULL;

	/* paranoia */
	if ((!buf) || (!hostname) || (!port)) {
		return -1;
	}

	if (snprintf(tmp, sizeof(tmp), "%s", buf) < 1) {
		fprintf(stderr, "upscli_splitaddr: can't parse empty string\n");
		return -1;
	}

	if (*tmp == '[') {
		if (strchr(tmp, ']') == NULL) {
			fprintf(stderr, "upscli_splitaddr: missing closing bracket in [domain literal]\n");
			return -1;
		}

		if ((*hostname = strdup(strtok_r(tmp+1, "]", &last))) == NULL) {
			fprintf(stderr, "upscli_splitaddr: strdup failed\n");
			return -1;
		}

		/* no port specified, use default */
		if (((s = strtok_r(NULL, "\0", &last)) == NULL) || (*s != ':')) {
			*port = PORT;
			return 0;
		}
	} else {
		s = strchr(tmp, ':');

		if ((*hostname = strdup(strtok_r(tmp, ":", &last))) == NULL) {
			fprintf(stderr, "upscli_splitaddr: strdup failed\n");
			return -1;
		}

		/* no port specified, use default */
		if (s == NULL) {
			*port = PORT;
			return 0;
		}
	}

	if ((*(++s) == '\0') || ((*port = strtol(s, NULL, 10)) < 1 )) {
		fprintf(stderr, "upscli_splitaddr: no port specified after ':' separator\n");
		return -1;
	}

	return 0;
}

int upscli_disconnect(UPSCONN_t *ups)
{
	if (!ups) {
		return -1;
	}

	if (ups->upsclient_magic != UPSCLIENT_MAGIC) {
		return -1;
	}

	pconf_finish(&ups->pc_ctx);

	free(ups->host);
	ups->host = NULL;

	if (ups->fd < 0) {
		return 0;
	}

	net_write(ups, "LOGOUT\n", 7);

#ifdef HAVE_SSL
	if (ups->ssl) {
		SSL_shutdown(ups->ssl);
		SSL_free(ups->ssl);
		ups->ssl = NULL;
	}

	if (ups->ssl_ctx) {
		SSL_CTX_free(ups->ssl_ctx);
		ups->ssl_ctx = NULL;
	}
#endif

	shutdown(ups->fd, shutdown_how);

	close(ups->fd);
	ups->fd = -1;

	return 0;
}

int upscli_fd(UPSCONN_t *ups)
{
	if (!ups) {
		return -1;
	}

	if (ups->upsclient_magic != UPSCLIENT_MAGIC) {
		return -1;
	}

	return ups->fd;
}

int upscli_upserror(UPSCONN_t *ups)
{
	if (!ups) {
		return -1;
	}

	if (ups->upsclient_magic != UPSCLIENT_MAGIC) {
		return -1;
	}

	return ups->upserror;
}

int upscli_ssl(UPSCONN_t *ups)
{
	if (!ups) {
		return -1;
	}

	if (ups->upsclient_magic != UPSCLIENT_MAGIC) {
		return -1;
	}

	if (ups->ssl) {
		return 1;
	}

	return 0;
}
