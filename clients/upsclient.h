/* upsclient.h - definitions for upsclient functions

   Copyright (C)
        2002	Russell Kroll <rkroll@exploits.org>
        2020 - 2025	Jim Klimov <jimklimov+nu@gmail.com>

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

#ifndef UPSCLIENT_H_SEEN
#define UPSCLIENT_H_SEEN 1

#ifdef WITH_OPENSSL
#	include <openssl/err.h>
#	include <openssl/ssl.h>
#elif defined(WITH_NSS) /* WITH_OPENSSL */
#	include <nss.h>
#	include <ssl.h>
#endif  /* WITH_OPENSSL | WITH_NSS */

/* Not including nut_stdint.h because this is part of end-user API */
#if defined HAVE_INTTYPES_H
#	include <inttypes.h>
#endif

#if defined HAVE_STDINT_H
#	include <stdint.h>
#endif

#if defined HAVE_LIMITS_H
#	include <limits.h>
#endif

/* Not including NUT timehead.h because this is part of end-user API */
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define UPSCLI_ERRBUF_LEN	256
#define UPSCLI_NETBUF_LEN	512	/* network i/o buffer */

#include "parseconf.h"

typedef struct {
	char	*host;
	uint16_t	port;
	int	fd;
	int	flags;
	int	upserror;
	int	syserrno;
	int	upsclient_magic;

	PCONF_CTX_t	pc_ctx;

	char	errbuf[UPSCLI_ERRBUF_LEN];

#ifdef WITH_OPENSSL
	SSL	*ssl;
#elif defined(WITH_NSS) /* WITH_OPENSSL */
	PRFileDesc *ssl;
#else /* WITH_OPENSSL | WITH_NSS */
	void *ssl;
#endif /* WITH_OPENSSL | WITH_NSS */

	char	readbuf[64];
	size_t	readlen;
	size_t	readidx;

}	UPSCONN_t;

const char *upscli_strerror(UPSCONN_t *ups);

/* NOTE: effectively only runs once; re-runs quickly skip out */
int upscli_init(int certverify, const char *certpath, const char *certname, const char *certpasswd);
int upscli_cleanup(void);

int upscli_tryconnect(UPSCONN_t *ups, const char *host, uint16_t port, int flags, struct timeval *tv);
/* blocking unless default timeout is specified, see also: upscli_init_default_connect_timeout() */
int upscli_connect(UPSCONN_t *ups, const char *host, uint16_t port, int flags);

void upscli_add_host_cert(const char* hostname, const char* certname, int certverify, int forcessl);

/* --- functions that only use the new names --- */

int upscli_get(UPSCONN_t *ups, size_t numq, const char **query,
		size_t *numa, char ***answer);

int upscli_list_start(UPSCONN_t *ups, size_t numq, const char **query);

int upscli_list_next(UPSCONN_t *ups, size_t numq, const char **query,
		size_t *numa, char ***answer);

ssize_t upscli_sendline_timeout(UPSCONN_t *ups, const char *buf, size_t buflen, const time_t timeout);
ssize_t upscli_sendline(UPSCONN_t *ups, const char *buf, size_t buflen);

ssize_t upscli_readline_timeout(UPSCONN_t *ups, char *buf, size_t buflen, const time_t timeout);
ssize_t upscli_readline(UPSCONN_t *ups, char *buf, size_t buflen);

int upscli_splitname(const char *buf, char **upsname, char **hostname,
			uint16_t *port);

int upscli_splitaddr(const char *buf, char **hostname, uint16_t *port);

int upscli_disconnect(UPSCONN_t *ups);

/* these functions return elements from UPSCONN_t to avoid direct references */

int upscli_fd(UPSCONN_t *ups);
int upscli_upserror(UPSCONN_t *ups);

/* returns 1 if SSL mode is active for this connection */
int upscli_ssl(UPSCONN_t *ups);

/* Assign default upscli_connect() from string; return 0 if OK, or
 * return -1 if parsing failed and current value was kept  */
int upscli_set_default_connect_timeout(const char *secs);
/* If ptv!=NULL, populate it with a copy of last assigned internal timeout */
void upscli_get_default_connect_timeout(struct timeval *ptv);
/* Initialize default upscli_connect() timeout from a number of sources:
 * built-in (0 = blocking), envvar NUT_DEFAULT_CONNECT_TIMEOUT,
 * or specified strings (may be NULL) most-preferred first.
 * Returns 0 if any provided value was valid and applied,
 * or if none were provided so the built-in default was applied;
 * returns -1 if all provided values were not valid (so the built-in
 * default was applied) - not necessarily fatal, rather useful to report.
 */
int upscli_init_default_connect_timeout(const char *cli_secs, const char *config_secs, const char *default_secs);

/* upsclient error list */

#define UPSCLI_ERR_NONE		-1	/* No known error (internally used in tools like upsmon, not set by upsclient.c) */

#define UPSCLI_ERR_UNKNOWN	0	/* Unknown error */
#define UPSCLI_ERR_VARNOTSUPP	1	/* Variable not supported by UPS */
#define UPSCLI_ERR_NOSUCHHOST	2	/* No such host */
#define UPSCLI_ERR_INVRESP	3	/* Invalid response from server */
#define UPSCLI_ERR_UNKNOWNUPS	4	/* Unknown UPS */
#define UPSCLI_ERR_INVLISTTYPE	5	/* Invalid list type */
#define UPSCLI_ERR_ACCESSDENIED	6	/* Access denied */
#define UPSCLI_ERR_PWDREQUIRED	7	/* Password required */
#define UPSCLI_ERR_PWDINCORRECT	8	/* Password incorrect */
#define UPSCLI_ERR_MISSINGARG	9	/* Missing argument */
#define UPSCLI_ERR_DATASTALE	10	/* Data stale */
#define UPSCLI_ERR_VARUNKNOWN	11	/* Variable unknown */
#define UPSCLI_ERR_LOGINTWICE	12	/* Already logged in */
#define UPSCLI_ERR_PWDSETTWICE	13	/* Already set password */
#define UPSCLI_ERR_UNKNOWNTYPE	14	/* Unknown variable type */
#define UPSCLI_ERR_UNKNOWNVAR	15	/* Unknown variable */
#define UPSCLI_ERR_VARREADONLY	16	/* Read-only variable */
#define UPSCLI_ERR_TOOLONG	17	/* New value is too long */
#define UPSCLI_ERR_INVALIDVALUE	18	/* Invalid value for variable */
#define UPSCLI_ERR_SETFAILED	19	/* Set command failed */
#define UPSCLI_ERR_UNKINSTCMD	20	/* Unknown instant command */
#define UPSCLI_ERR_CMDFAILED	21	/* Instant command failed */
#define UPSCLI_ERR_CMDNOTSUPP	22	/* Instant command not supported */
#define UPSCLI_ERR_INVUSERNAME	23	/* Invalid username */
#define UPSCLI_ERR_USERSETTWICE	24	/* Already set username */
#define UPSCLI_ERR_UNKCOMMAND	25	/* Unknown command */
#define UPSCLI_ERR_INVALIDARG	26	/* Invalid argument */
#define UPSCLI_ERR_SENDFAILURE	27	/* Send failure: %s */
#define UPSCLI_ERR_RECVFAILURE	28	/* Receive failure: %s */
#define UPSCLI_ERR_SOCKFAILURE	29	/* socket failure: %s */
#define UPSCLI_ERR_BINDFAILURE	30	/* bind failure: %s */
#define UPSCLI_ERR_CONNFAILURE	31	/* Connection failure: %s */
#define UPSCLI_ERR_WRITE	32	/* Write error: %s */
#define UPSCLI_ERR_READ		33	/* Read error: %s */
#define UPSCLI_ERR_INVPASSWORD	34	/* Invalid password */
#define UPSCLI_ERR_USERREQUIRED	35	/* Username required */
#define UPSCLI_ERR_SSLFAIL	36	/* SSL is not available */
#define UPSCLI_ERR_SSLERR	37	/* SSL error: %s */
#define UPSCLI_ERR_SRVDISC	38	/* Server disconnected */
#define UPSCLI_ERR_DRVNOTCONN	39	/* Driver not connected */
#define UPSCLI_ERR_NOMEM	40	/* Memory allocation failure */
#define UPSCLI_ERR_PARSE	41	/* Parse error: %s */
#define UPSCLI_ERR_PROTOCOL	42	/* Protocol error */

#define UPSCLI_ERR_MAX		42	/* stop here */

/* list types for use with upscli_getlist */

#define UPSCLI_LIST_VARS	1	/* all variables */
#define UPSCLI_LIST_RW		2	/* just read/write variables */
#define UPSCLI_LIST_CMDS	3	/* instant commands */

/* flags for use with upscli_connect */

#define UPSCLI_CONN_TRYSSL		0x0001	/* try SSL, OK if not supported   */
#define UPSCLI_CONN_REQSSL		0x0002	/* try SSL, fail if not supported */
#define UPSCLI_CONN_INET		0x0004	/* IPv4 only */
#define UPSCLI_CONN_INET6		0x0008	/* IPv6 only */
#define UPSCLI_CONN_CERTVERIF	0x0010	/* Verify certificates for SSL	*/

/******************************************************************************
 * String methods for space-separated token lists, used originally in dstate  *
 * These methods should ease third-party NUT clients' parsing of `ups.status` *
 ******************************************************************************/

/* Return non-zero if "string" contains "token" (case-sensitive),
 * either surrounded by space character(s) or start/end of "string",
 * or 0 if that token is not there, or if either string is NULL or empty.
 */
int	upscli_str_contains_token(const char *string, const char *token);

/* Add "token" to end of string "tgt", if it is not yet there
 * (prefix it with a space character if "tgt" is not empty).
 * Return 0 if already there, 1 if token was added successfully,
 * -1 if we needed to add it but it did not fit under the tgtsize limit,
 * -2 if either string was NULL or "token" was empty.
 * NOTE: If token contains space(s) inside, recurse to treat it
 * as several tokens to add independently.
 * Optionally calls "callback_always" (if not NULL) after checking
 * for spaces (and maybe recursing) and before checking if the token
 * is already there, and/or "callback_unique" (if not NULL) after
 * checking for uniqueness and going to add a newly seen token.
 * If such callback returns 0, abort the addition of token and return -3.
 */
int	upscli_str_add_unique_token(char *tgt, size_t tgtsize, const char *token,
				int (*callback_always)(char *, size_t, const char *),
				int (*callback_unique)(char *, size_t, const char *)
);


#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif	/* UPSCLIENT_H_SEEN */
