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
#include "nut_platform.h"

#ifdef HAVE_PTHREAD
/* this include is needed on AIX to have errno stored in thread local storage */
#include <pthread.h>
#endif

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "upsclient.h"
#include "common.h"
#include "timehead.h"

/* WA for Solaris/i386 bug: non-blocking connect sets errno to ENOENT */
#if (defined NUT_PLATFORM_SOLARIS && CPU_TYPE == i386)
	#define SOLARIS_i386_NBCONNECT_ENOENT(status) (ENOENT == (status))
#else
	#define SOLARIS_i386_NBCONNECT_ENOENT(status) (0)
#endif  /* end of Solaris/i386 WA for non-blocking connect */

/* WA for AIX bug: non-blocking connect sets errno to 0 */
#if (defined NUT_PLATFORM_AIX)
	#define AIX_NBCONNECT_0(status) (0 == (status))
#else
	#define AIX_NBCONNECT_0(status) (0)
#endif  /* end of AIX WA for non-blocking connect */

#ifdef WITH_NSS
	#include <prerror.h>
	#include <prinit.h>
	#include <pk11func.h>
	#include <prtypes.h>
	#include <ssl.h>
	#include <private/pprio.h>
#endif /* WITH_NSS */

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


typedef struct HOST_CERT_s {
	const char	*host;
	const char	*certname;
	int			certverify;
	int			forcessl;
	
	struct HOST_CERT_s	*next;
}	HOST_CERT_t;
static HOST_CERT_t* upscli_find_host_cert(const char* hostname);


static int upscli_initialized = 0;

#ifdef WITH_OPENSSL
static SSL_CTX	*ssl_ctx;
#elif defined(WITH_NSS) /* WITH_OPENSLL */
static int verify_certificate = 1;
static HOST_CERT_t *first_host_cert = NULL;
static char* nsscertname = NULL;
static char* nsscertpasswd = NULL;
#endif /* WITH_OPENSSL | WITH_NSS */


#ifdef WITH_OPENSSL

static void ssl_debug(void)
{
	int	e;
	char	errmsg[SMALLBUF];

	while ((e = ERR_get_error()) != 0) {
		ERR_error_string_n(e, errmsg, sizeof(errmsg));
		upsdebugx(2, "ssl_debug: %s", errmsg);
	}
}

static int ssl_error(SSL *ssl, int ret)
{
	int	e;

	e = SSL_get_error(ssl, ret);

	switch (e)
	{
	case SSL_ERROR_WANT_READ:
		upslogx(LOG_ERR, "ssl_error() ret=%d SSL_ERROR_WANT_READ", ret);
		break;

	case SSL_ERROR_WANT_WRITE:
		upslogx(LOG_ERR, "ssl_error() ret=%d SSL_ERROR_WANT_WRITE", ret);
		break;

	case SSL_ERROR_SYSCALL:
		if (ret == 0 && ERR_peek_error() == 0) {
			upslogx(LOG_ERR, "ssl_error() EOF from client");
		} else {
			upslogx(LOG_ERR, "ssl_error() ret=%d SSL_ERROR_SYSCALL", ret);
		}
		break;

	default:
		upslogx(LOG_ERR, "ssl_error() ret=%d SSL_ERROR %d", ret, e);
		ssl_debug();
	}

	return -1;
}

#elif defined(WITH_NSS) /* WITH_OPENSSL */

static char *nss_password_callback(PK11SlotInfo *slot, PRBool retry, 
		void *arg)
{
	upslogx(LOG_INFO, "Intend to retrieve password for %s / %s: password %sconfigured",
		PK11_GetSlotName(slot), PK11_GetTokenName(slot), nsscertpasswd?"":"not ");
	return nsscertpasswd ? PL_strdup(nsscertpasswd) : NULL;
}

static void nss_error(const char* funcname)
{
	char buffer[SMALLBUF];
	PRInt32 length = PR_GetErrorText(buffer);
	if (length > 0 && length < SMALLBUF) {
		upsdebugx(1, "nss_error %ld in %s : %s", (long)PR_GetError(), funcname, buffer);
	}else{
		upsdebugx(1, "nss_error %ld in %s", (long)PR_GetError(), funcname);
	}
}

static SECStatus AuthCertificate(CERTCertDBHandle *arg, PRFileDesc *fd,
	PRBool checksig, PRBool isServer)
{
	UPSCONN_t *ups   = (UPSCONN_t *)SSL_RevealPinArg(fd);
	SECStatus status = SSL_AuthCertificate(arg, fd, checksig, isServer);
	upslogx(LOG_INFO, "Intend to authenticate server %s : %s",
		ups?ups->host:"<unnamed>",
		status==SECSuccess?"SUCCESS":"FAILED");
	if (status != SECSuccess) {
		nss_error("SSL_AuthCertificate");
	}
	return status;
}

static SECStatus AuthCertificateDontVerify(CERTCertDBHandle *arg, PRFileDesc *fd,
	PRBool checksig, PRBool isServer)
{
	UPSCONN_t *ups   = (UPSCONN_t *)SSL_RevealPinArg(fd);
	upslogx(LOG_INFO, "Do not intend to authenticate server %s",
		ups?ups->host:"<unnamed>");
	return SECSuccess;
}

static SECStatus BadCertHandler(UPSCONN_t *arg, PRFileDesc *fd)
{
	HOST_CERT_t* cert;
	upslogx(LOG_WARNING, "Certificate validation failed for %s",
		(arg&&arg->host)?arg->host:"<unnamed>");
	/* BadCertHandler is called when the NSS certificate validation is failed.
	 * If the certificate verification (user conf) is mandatory, reject authentication
	 * else accept it.
	 */
	cert = upscli_find_host_cert(arg->host);
	if (cert != NULL) {
		return cert->certverify==0 ?  SECSuccess : SECFailure;
	} else {
		return verify_certificate==0 ? SECSuccess : SECFailure;
	}
}

static SECStatus GetClientAuthData(UPSCONN_t *arg, PRFileDesc *fd,
	CERTDistNames *caNames, CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey)
{
	CERTCertificate *cert;
	SECKEYPrivateKey *privKey;
	SECStatus status = NSS_GetClientAuthData(arg, fd, caNames, pRetCert, pRetKey);
	if (status == SECFailure) {
		if (nsscertname != NULL) {
			cert = PK11_FindCertFromNickname(nsscertname, NULL);
			if(cert==NULL)	{
				upslogx(LOG_ERR, "Can not find self-certificate");
				nss_error("GetClientAuthData / PK11_FindCertFromNickname");
			}else{
				privKey = PK11_FindKeyByAnyCert(cert, NULL);
				if(privKey==NULL){
					upslogx(LOG_ERR, "Can not find private key related to self-certificate");
					nss_error("GetClientAuthData / PK11_FindKeyByAnyCert");
				}else{
					*pRetCert = cert;
					*pRetKey = privKey;
					status = SECSuccess;
				}
			}
		} else {
			upslogx(LOG_ERR, "Self-certificate name not configured");
		}
	}

	return status;
}

static void HandshakeCallback(PRFileDesc *fd, UPSCONN_t *client_data)
{
	upslogx(LOG_INFO, "SSL handshake done successfully with server %s",
		client_data->host);
}

#endif /* WITH_OPENSSL | WITH_NSS */

int upscli_init(int certverify, const char *certpath, 
					const char *certname, const char *certpasswd)
{
#ifdef WITH_OPENSSL
	int ret, ssl_mode = SSL_VERIFY_NONE;
#elif defined(WITH_NSS) /* WITH_OPENSSL */
	SECStatus	status;
#endif /* WITH_OPENSSL | WITH_NSS */


	if (upscli_initialized == 1) {
		upslogx(LOG_WARNING, "upscli already initialized");
		return -1;
	}
	
#ifdef WITH_OPENSSL

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_load_error_strings();
	SSL_library_init();

	ssl_ctx = SSL_CTX_new(SSLv23_client_method());
#else
	ssl_ctx = SSL_CTX_new(TLS_client_method());
#endif

	if (!ssl_ctx) {
		upslogx(LOG_ERR, "Can not initialize SSL context");
		return -1;
	}
	
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* set minimum protocol TLSv1 */
	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
#else
	ret = SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_VERSION);
	if (ret != 1) {
		upslogx(LOG_ERR, "Can not set minimum protocol to TLSv1");
		return -1;
	}
#endif

	if (!certpath) {
		if (certverify == 1) {
			upslogx(LOG_ERR, "Can not verify certificate if any is specified");
			return -1;	/* Failed : cert is mandatory but no certfile */
		}
	} else {
		switch(certverify) {

		case 0:
			ssl_mode = SSL_VERIFY_NONE;
			break;
		default:
			ssl_mode = SSL_VERIFY_PEER;
			break;
		}

		ret = SSL_CTX_load_verify_locations(ssl_ctx, NULL, certpath);
		if (ret != 1) {
			upslogx(LOG_ERR, "Failed to load certificate from pemfile %s", certpath);
			return -1;
		}

		SSL_CTX_set_verify(ssl_ctx, ssl_mode, NULL);		
	}
#elif defined(WITH_NSS) /* WITH_OPENSSL */
	PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
	
	PK11_SetPasswordFunc(nss_password_callback);
	
	if (certpath) {
		upslogx(LOG_INFO, "Init SSL with cerificate database located at %s", certpath);
		status = NSS_Init(certpath);
	} else {
		upslogx(LOG_NOTICE, "Init SSL without certificate database");
		status = NSS_NoDB_Init(NULL);
	}
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL context");
		nss_error("upscli_init / NSS_[NoDB]_Init");
		return -1;
	}
	
	status = NSS_SetDomesticPolicy();
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL policy");
		nss_error("upscli_init / NSS_SetDomesticPolicy");
		return -1;
	}
	
	SSL_ClearSessionCache();
	
	status = SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_TRUE);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not enable SSLv3");
		nss_error("upscli_init / SSL_OptionSetDefault(SSL_ENABLE_SSL3)");
		return -1;
	}
	status = SSL_OptionSetDefault(SSL_ENABLE_TLS, PR_TRUE);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not enable TLSv1");
		nss_error("upscli_init / SSL_OptionSetDefault(SSL_ENABLE_TLS)");
		return -1;
	}
	status = SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, PR_FALSE);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not disable SSLv2 hello compatibility");
		nss_error("upscli_init / SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO)");
		return -1;
	}
	if (certname) {
		nsscertname = xstrdup(certname);
	}
	if (certpasswd) {
		nsscertpasswd = xstrdup(certpasswd);
	}
	verify_certificate = certverify;
#endif /* WITH_OPENSSL | WITH_NSS */
	
	upscli_initialized = 1;
	return 1;
}

void upscli_add_host_cert(const char* hostname, const char* certname, int certverify, int forcessl)
{
#ifdef WITH_NSS
	HOST_CERT_t* cert = xmalloc(sizeof(HOST_CERT_t));
	cert->next = first_host_cert;
	cert->host = xstrdup(hostname);
	cert->certname = xstrdup(certname);
	cert->certverify = certverify;
	cert->forcessl = forcessl;
	first_host_cert = cert;
#endif /* WITH_NSS */
}

static HOST_CERT_t* upscli_find_host_cert(const char* hostname)
{
#ifdef WITH_NSS
	HOST_CERT_t* cert = first_host_cert;
	if (hostname != NULL) {
		while (cert != NULL) {
			if (cert->host != NULL && strcmp(cert->host, hostname)==0 ) {
				return cert;
			}
			cert = cert->next;
		}
	}
#endif /* WITH_NSS */
	return NULL;
}

int upscli_cleanup(void)
{
#ifdef WITH_OPENSSL
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
	}

#endif /* WITH_OPENSSL */

#ifdef WITH_NSS	
	/* Called to force cache clearing to prevent NSS shutdown failures.
	 * http://www.mozilla.org/projects/security/pki/nss/ref/ssl/sslfnc.html#1138601
	 */
	SSL_ClearSessionCache();
	NSS_Shutdown();
	PR_Cleanup();
	/* Called to release memory arena used by NSS/NSPR.
	 * Prevent to show all PL_ArenaAllocate mem alloc as leaks.
	 * https://developer.mozilla.org/en/NSS_Memory_allocation
	 */
	PL_ArenaFinish();
#endif /* WITH_NSS */
	
	upscli_initialized = 0;
	return 1;
}

const char *upscli_strerror(UPSCONN_t *ups)
{
#ifdef WITH_OPENSSL
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
#ifdef WITH_OPENSSL
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
#elif defined(WITH_NSS) /* WITH_OPENSSL */
		if (PR_GetErrorTextLength() < UPSCLI_ERRBUF_LEN) {
			PR_GetErrorText(ups->errbuf);
		} else {
			snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN, 
				"SSL error #%ld, message too long to be displayed",
				(long)PR_GetError());
		}
#else
		snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN, 
			"SSL error, but SSL wasn't enabled at compile-time");
#endif	/* WITH_OPENSSL | WITH_NSS */
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
static int upscli_select_read(const int fd, void *buf, const size_t buflen, const long d_sec, const long d_usec)
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
static int net_read(UPSCONN_t *ups, char *buf, size_t buflen, unsigned int timeout)
{
	int	ret = -1;

#ifdef WITH_SSL
	if (ups->ssl) {
#ifdef WITH_OPENSSL
		ret = SSL_read(ups->ssl, buf, buflen);
#elif defined(WITH_NSS) /* WITH_OPENSSL */
		ret = PR_Read(ups->ssl, buf, buflen);
#endif	/* WITH_OPENSSL | WITH_NSS*/

		if (ret < 1) {
			ups->upserror = UPSCLI_ERR_SSLERR;
		}

		return ret;
	}
#endif

	ret = upscli_select_read(ups->fd, buf, buflen, timeout, 0);

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
static int upscli_select_write(const int fd, const void *buf, const size_t buflen, const long d_sec, const long d_usec)
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
static int net_write(UPSCONN_t *ups, const char *buf, size_t buflen, unsigned int timeout)
{
	int	ret = -1;

#ifdef WITH_SSL
	if (ups->ssl) {
#ifdef WITH_OPENSSL
		ret = SSL_write(ups->ssl, buf, buflen);
#elif defined(WITH_NSS) /* WITH_OPENSSL */
		ret = PR_Write(ups->ssl, buf, buflen);
#endif /* WITH_OPENSSL | WITH_NSS */

		if (ret < 1) {
			ups->upserror = UPSCLI_ERR_SSLERR;
		}

		return ret;
	}
#endif

	ret = upscli_select_write(ups->fd, buf, buflen, timeout, 0);

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


#ifdef WITH_SSL

/*
 * 1 : OK
 * -1 : ERROR
 * 0 : SSL NOT SUPPORTED 
 */
static int upscli_sslinit(UPSCONN_t *ups, int verifycert)
{
#ifdef WITH_OPENSSL
	int res;
#elif defined(WITH_NSS) /* WITH_OPENSSL */
	SECStatus	status;
	PRFileDesc	*socket;
	HOST_CERT_t *cert; 
#endif /* WITH_OPENSSL | WITH_NSS */
	char	buf[UPSCLI_NETBUF_LEN];

	/* Intend to initialize upscli with no ssl db if not already done.
	 * Compatibility stuff for old clients which do not initialize them.
	 */
	if (upscli_initialized==0) {
		upsdebugx(3, "upscli not initialized, "
			"force initialisation without SSL configuration");
		upscli_init(0, NULL, NULL, NULL);
	}
	
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
	
#ifdef WITH_OPENSSL

	if (!ssl_ctx) {
		upsdebugx(3, "SSL context is not available");
		return 0;
	}

	ups->ssl = SSL_new(ssl_ctx);
	if (!ups->ssl) {
		upsdebugx(3, "Can not create SSL socket");
		return 0;
	}

	if (SSL_set_fd(ups->ssl, ups->fd) != 1) {
		upsdebugx(3, "Can not bind file descriptor to SSL socket");
		return -1;
	}

	if (verifycert != 0) {
		SSL_set_verify(ups->ssl, SSL_VERIFY_PEER, NULL);
	} else {
		SSL_set_verify(ups->ssl, SSL_VERIFY_NONE, NULL);
	}

	res = SSL_connect(ups->ssl);
	switch(res)
	{
	case 1:
		upsdebugx(3, "SSL connected (%s)", SSL_get_version(ups->ssl));
		break;
	case 0:
		upslog_with_errno(1, "SSL_connect do not accept handshake.");
		ssl_error(ups->ssl, res);
		return -1;
	default:
		upslog_with_errno(1, "Unknown return value from SSL_connect %d", res);
		ssl_error(ups->ssl, res);
		return -1;
	}	

	return 1;

#elif defined(WITH_NSS) /* WITH_OPENSSL */

	socket = PR_ImportTCPSocket(ups->fd);
	if (socket == NULL){
		nss_error("upscli_sslinit / PR_ImportTCPSocket");
		return -1;
	}

	ups->ssl = SSL_ImportFD(NULL, socket);
	if (ups->ssl == NULL){
		nss_error("upscli_sslinit / SSL_ImportFD");
		return -1;
	}
	
	if (SSL_SetPKCS11PinArg(ups->ssl, ups) == -1){
		nss_error("upscli_sslinit / SSL_SetPKCS11PinArg");
		return -1;
	}
	
	if (verifycert) {
		status = SSL_AuthCertificateHook(ups->ssl,
			(SSLAuthCertificate)AuthCertificate, CERT_GetDefaultCertDB());
	} else {
		status = SSL_AuthCertificateHook(ups->ssl,
			(SSLAuthCertificate)AuthCertificateDontVerify, CERT_GetDefaultCertDB());
	}
	if (status != SECSuccess) {
		nss_error("upscli_sslinit / SSL_AuthCertificateHook");
		return -1;
	}
	
	status = SSL_BadCertHook(ups->ssl, (SSLBadCertHandler)BadCertHandler, ups);
	if (status != SECSuccess) {
		nss_error("upscli_sslinit / SSL_BadCertHook");
		return -1;
	}
	
	status = SSL_GetClientAuthDataHook(ups->ssl, (SSLGetClientAuthData)GetClientAuthData, ups);
	if (status != SECSuccess) {
		nss_error("upscli_sslinit / SSL_GetClientAuthDataHook");
		return -1;
	}
	
	status = SSL_HandshakeCallback(ups->ssl, (SSLHandshakeCallback)HandshakeCallback, ups);
	if (status != SECSuccess) {
		nss_error("upscli_sslinit / SSL_HandshakeCallback");
		return -1;
	}

	cert = upscli_find_host_cert(ups->host);
	if (cert != NULL && cert->certname != NULL) {
		upslogx(LOG_INFO, "Connecting in SSL to '%s' and look at certificate called '%s'",
			ups->host, cert->certname);
		status = SSL_SetURL(ups->ssl, cert->certname);
	} else {
		upslogx(LOG_NOTICE, "Connecting in SSL to '%s' (no certificate name specified)", ups->host);
		status = SSL_SetURL(ups->ssl, ups->host);
	}
	if (status != SECSuccess) {
		nss_error("upscli_sslinit / SSL_SetURL");
		return -1;
	}

	status = SSL_ResetHandshake(ups->ssl, PR_FALSE);
	if (status != SECSuccess) {
		nss_error("upscli_sslinit / SSL_ResetHandshake");
		ups->ssl = NULL;
		/* EKI wtf unimport or free the socket ? */
		return -1;
	}

	status = SSL_ForceHandshake(ups->ssl);
	if (status != SECSuccess) {
		nss_error("upscli_sslinit / SSL_ForceHandshake");
		ups->ssl = NULL;
		/* EKI wtf unimport or free the socket ? */
		/* TODO : Close the connection. */
		return -1;
	}
	
	return 1;
	
#endif /* WITH_OPENSSL | WITH_NSS */
}

#else /* WITH_SSL */

static int upscli_sslinit(UPSCONN_t *ups, int verifycert)
{
	return 0;	/* not supported */
}

#endif /* WITH_SSL */

int upscli_tryconnect(UPSCONN_t *ups, const char *host, int port, int flags,struct timeval * timeout)
{
	int				sock_fd;
	struct addrinfo	hints, *res, *ai;
	char			sport[NI_MAXSERV];
	int				v, certverify, tryssl, forcessl, ret;
	HOST_CERT_t*	hostcert;
	fd_set 			wfds;
	int			error;
	socklen_t		error_size;
	long			fd_flags;

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

		/* non blocking connect */
		if(timeout != NULL) {
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags |= O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
		}

		while ((v = connect(sock_fd, ai->ai_addr, ai->ai_addrlen)) < 0) {
			if(errno == EINPROGRESS || SOLARIS_i386_NBCONNECT_ENOENT(errno) || AIX_NBCONNECT_0(errno)) {
				FD_ZERO(&wfds);
				FD_SET(sock_fd, &wfds);
				select(sock_fd+1,NULL,&wfds,NULL,
						timeout);
				if (FD_ISSET(sock_fd, &wfds)) {
					error_size = sizeof(error);
					getsockopt(sock_fd,SOL_SOCKET,SO_ERROR,
							&error,&error_size);
					if( error == 0) {
						/* connect successful */
						v = 0;
						break;
					}
					errno = error;
				}
				else {
					/* Timeout */
					v = -1;
					break;
				}
			}

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

		/* switch back to blocking operation */
		if(timeout != NULL) {
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags &= ~O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
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

	pconf_init(&ups->pc_ctx, NULL);

	ups->host = strdup(host);

	if (!ups->host) {
		ups->upserror = UPSCLI_ERR_NOMEM;
		upscli_disconnect(ups);
		return -1;
	}

	ups->port = port;

	hostcert = upscli_find_host_cert(host);
	
	if (hostcert != NULL) {
		/* An host security rule is specified. */
		certverify	= hostcert->certverify;
		forcessl	= hostcert->forcessl;
	} else {
		certverify	= (flags & UPSCLI_CONN_CERTVERIF) != 0 ? 1 : 0;
		forcessl	= (flags & UPSCLI_CONN_REQSSL) != 0 ? 1 : 0;
	}
	tryssl = (flags & UPSCLI_CONN_TRYSSL) != 0 ? 1 : 0;
	
	if (tryssl || forcessl) {
		ret = upscli_sslinit(ups, certverify);
		if (forcessl && ret != 1) {
			upslogx(LOG_ERR, "Can not connect to %s in SSL, disconnect", host);
			ups->upserror = UPSCLI_ERR_SSLFAIL;
			upscli_disconnect(ups);
			return -1;
		} else if (tryssl && ret == -1) {
			upslogx(LOG_NOTICE, "Error while connecting to %s, disconnect", host);
			upscli_disconnect(ups);
			return -1;
		} else if (tryssl && ret == 0) {
			if (certverify != 0) {
				upslogx(LOG_NOTICE, "Can not connect to %s in SSL and "
				"certificate is needed, disconnect", host);
				upscli_disconnect(ups);
				return -1;
			}
			upsdebugx(3, "Can not connect to %s in SSL, continue unencrypted", host);
		} else {
			upslogx(LOG_INFO, "Connected to %s in SSL", host);
			if (certverify == 0) {
				/* you REALLY should set CERTVERIFY to 1 if using SSL... */
				upslogx(LOG_WARNING, "Certificate verification is disabled");
			}
		}
	}
	
	return 0;
}

int upscli_connect(UPSCONN_t *ups, const char *host, int port, int flags)
{
	return upscli_tryconnect(ups,host,port,flags,NULL);
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

int upscli_sendline_timeout(UPSCONN_t *ups, const char *buf, size_t buflen, unsigned int timeout)
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

	ret = net_write(ups, buf, buflen, timeout);

	if (ret < 1) {
		upscli_disconnect(ups);
		return -1;
	}

	return 0;
}

int upscli_sendline(UPSCONN_t *ups, const char *buf, size_t buflen)
{
	return upscli_sendline_timeout(ups, buf, buflen, 0);
}

int upscli_readline_timeout(UPSCONN_t *ups, char *buf, size_t buflen, unsigned int timeout)
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

			ret = net_read(ups, ups->readbuf, sizeof(ups->readbuf), timeout);

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

int upscli_readline(UPSCONN_t *ups, char *buf, size_t buflen)
{
	return upscli_readline_timeout(ups, buf, buflen, DEFAULT_NETWORK_TIMEOUT);
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

	net_write(ups, "LOGOUT\n", 7, 0);

#ifdef WITH_OPENSSL
	if (ups->ssl) {
		SSL_shutdown(ups->ssl);
		SSL_free(ups->ssl);
		ups->ssl = NULL;
	}
#elif defined(WITH_NSS) /* WITH_OPENSSL */
	if (ups->ssl) {
		PR_Shutdown(ups->ssl, PR_SHUTDOWN_BOTH);
		PR_Close(ups->ssl);
		ups->ssl = NULL;
	}
#endif /* WITH_OPENSSL | WITH_NSS */

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

#ifdef WITH_SSL
	if (ups->ssl) {
		return 1;
	}
#endif /* WITH_SSL */

	return 0;
}
