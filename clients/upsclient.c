/* upsclient - network communications functions for UPS clients

   Copyright (C)
	2002	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2020 - 2026	Jim Klimov <jimklimov+nut@gmail.com>

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

#define NUT_WANT_INET_NTOP_XX	1

#include "config.h"	/* safe because it doesn't contain prototypes */
#include "nut_platform.h"

#ifndef WIN32
# ifdef HAVE_PTHREAD
/* this include is needed on AIX to have errno stored in thread local storage */
#  include <pthread.h>
# endif
#endif	/* !WIN32 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef WIN32
# include <sys/select.h>	/* fd_set and select(); (or sys/time.h on older BSDs) */
# include <netdb.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <fcntl.h>
# define SOCK_OPT_CAST
#else /* => WIN32 */
# define SOCK_OPT_CAST (char *)
/* Those 2 files for support of getaddrinfo, getnameinfo and freeaddrinfo
   on Windows 2000 and older versions */
# include <ws2tcpip.h>
# include <wspiapi.h>
/* This override network system calls to adapt to Windows specificity */
# define W32_NETWORK_CALL_OVERRIDE
# include "wincompat.h"
# undef W32_NETWORK_CALL_OVERRIDE
#endif	/* WIN32 */

#include "common.h"
#include "nut_stdint.h"
#include "nut_float.h"
#include "timehead.h"
#include "upsclient.h"

/* WA for Solaris/i386 bug: non-blocking connect sets errno to ENOENT */
#if (defined NUT_PLATFORM_SOLARIS)
#	define SOLARIS_i386_NBCONNECT_ENOENT(status) ( (!strcmp("i386", CPU_TYPE)) ? (ENOENT == (status)) : 0 )
#else
#	define SOLARIS_i386_NBCONNECT_ENOENT(status) (0)
#endif	/* end of Solaris/i386 WA for non-blocking connect */

/* WA for AIX bug: non-blocking connect sets errno to 0 */
#if (defined NUT_PLATFORM_AIX)
#	define AIX_NBCONNECT_0(status) (0 == (status))
#else
#	define AIX_NBCONNECT_0(status) (0)
#endif	/* end of AIX WA for non-blocking connect */

#ifdef WITH_OPENSSL
# include <openssl/x509v3.h>
#endif

#ifdef WITH_NSS
#	include <prerror.h>
#	include <prinit.h>
#	include <pk11func.h>
#	include <prtypes.h>
#	include <ssl.h>
#	include <private/pprio.h>
#endif	/* WITH_NSS */

#define UPSCLIENT_MAGIC	0x19980308

#define SMALLBUF	512

#ifdef SHUT_RDWR
#	define shutdown_how	SHUT_RDWR
#else
#	define shutdown_how	2
#endif

#include "nut_version.h"
static const char *UPSCLI_VERSION = NUT_VERSION_MACRO;

static struct {
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

/* Flag for SSL init */
static int upscli_initialized = 0;

/* 0 means no timeout in upscli_connect(), aka built-in(blocking) default */
static struct timeval upscli_default_connect_timeout = {0, 0};
static int upscli_default_connect_timeout_initialized = 0;

#ifdef WITH_OPENSSL
static SSL_CTX	*ssl_ctx;
#endif	/* WITH_OPENSSL */

#ifdef WITH_NSS
static int verify_certificate = 1;
#endif	/* WITH_NSS */

#if defined(WITH_OPENSSL) || defined(WITH_NSS)
static HOST_CERT_t *first_host_cert = NULL;
static char* sslcertname = NULL;
static char* sslcertpasswd = NULL;
#endif	/* WITH_OPENSSL | WITH_NSS */


#ifdef WITH_OPENSSL

static void ssl_debug(void)
{
	unsigned long	e;
	char	errmsg[SMALLBUF];

	while ((e = ERR_get_error()) != 0) {
		ERR_error_string_n(e, errmsg, sizeof(errmsg));
		upsdebugx(2, "ssl_debug: %s", errmsg);
	}
}

static int ssl_error(SSL *ssl, ssize_t ret)
{
	int	e;

	if (ret >= INT_MAX) {
		upslogx(LOG_ERR, "ssl_error() ret=%" PRIiSIZE " would not fit in an int", ret);
		return -1;
	}
	e = SSL_get_error(ssl, (int)ret);

	switch (e)
	{
	case SSL_ERROR_WANT_READ:
		upsdebugx(4, "ssl_error() ret=%" PRIiSIZE " SSL_ERROR_WANT_READ", ret);
		return 0;

	case SSL_ERROR_WANT_WRITE:
		upsdebugx(4, "ssl_error() ret=%" PRIiSIZE " SSL_ERROR_WANT_WRITE", ret);
		return 0;

	case SSL_ERROR_SYSCALL:
		if (ret == 0 && ERR_peek_error() == 0) {
			upslogx(LOG_ERR, "ssl_error() EOF from server");
		} else {
			upslogx(LOG_ERR, "ssl_error() ret=%" PRIiSIZE " SSL_ERROR_SYSCALL", ret);
		}
		break;

	default:
		upslogx(LOG_ERR, "ssl_error() ret=%" PRIiSIZE " SSL_ERROR %d", ret, e);
		ssl_debug();
	}

	return -1;
}

#elif defined(WITH_NSS) /* WITH_OPENSSL */

static char *nss_password_callback(PK11SlotInfo *slot, PRBool retry,
		void *arg)
{
	NUT_UNUSED_VARIABLE(retry);
	NUT_UNUSED_VARIABLE(arg);

	upslogx(LOG_INFO, "Intend to retrieve password for %s / %s: password %sconfigured",
		PK11_GetSlotName(slot), PK11_GetTokenName(slot), sslcertpasswd?"":"not ");
	return sslcertpasswd ? PL_strdup(sslcertpasswd) : NULL;
}

/** Detail the currently raised NSS error code if possible, and debug-log
 *  it with caller-provided text (typically the calling function name). */
static void nss_error(const char* text)
{
	char	err_name_buf[SMALLBUF];
	PRErrorCode	err_num = PR_GetError();
	const char	*err_name = PR_ErrorToName(err_num);
	PRInt32	err_len = PR_GetErrorTextLength();

	if (err_name) {
		size_t	len = snprintf(err_name_buf, sizeof(err_name_buf), " (%s)", err_name);
		if (len > sizeof(err_name_buf) - 2) {
			err_name_buf[sizeof(err_name_buf) - 2] = ')';
			err_name_buf[sizeof(err_name_buf) - 1] = '\0';
		}
	} else {
		err_name_buf[0] = '\0';
	}

	if (err_len > 0) {
		char	*buffer = (char *)calloc(err_len + 1, sizeof(char));
		if (buffer) {
			PR_GetErrorText(buffer);
			upsdebugx(1, "nss_error %ld%s in %s : %s",
				(long)err_num,
				err_name_buf,
				text,
				buffer);
			free(buffer);
		} else {
			upsdebugx(1, "nss_error %ld%s in %s : "
				"Failed to allocate internal error buffer "
				"for detailed error text, needs %ld bytes",
				(long)err_num,
				err_name_buf,
				text,
				(long)err_len);
		}
	} else {
		/* The code above may be obsolete or not ubiquitous, try another way */
		const char	*err_text = PR_ErrorToString(err_num, PR_LANGUAGE_I_DEFAULT);
		if (err_text && *err_text) {
			upsdebugx(1, "nss_error %ld%s in %s : %s",
				(long)err_num,
				err_name_buf,
				text,
				err_text);
		} else {
			upsdebugx(1, "nss_error %ld%s in %s",
				(long)err_num,
				err_name_buf,
				text);
		}
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
	NUT_UNUSED_VARIABLE(arg);
	NUT_UNUSED_VARIABLE(checksig);
	NUT_UNUSED_VARIABLE(isServer);

	upslogx(LOG_INFO, "Do not intend to authenticate server %s",
		ups?ups->host:"<unnamed>");
	return SECSuccess;
}

static SECStatus BadCertHandler(UPSCONN_t *arg, PRFileDesc *fd)
{
	HOST_CERT_t* cert;
	NUT_UNUSED_VARIABLE(fd);

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
		if (sslcertname != NULL) {
			cert = PK11_FindCertFromNickname(sslcertname, NULL);
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
	NUT_UNUSED_VARIABLE(fd);

	upslogx(LOG_INFO, "SSL handshake done successfully with server %s",
		client_data->host);
}

#endif /* WITH_OPENSSL | WITH_NSS */

#ifdef WITH_OPENSSL
static int openssl_password_callback(char *buf, int size, int rwflag, void *userdata)
{
	/* See https://docs.openssl.org/1.0.2/man3/SSL_CTX_set_default_passwd_cb */
	/* is callback used for reading/decryption (rwflag=0) or writing/encryption (rwflag=1)? */
	NUT_UNUSED_VARIABLE(rwflag);
	/* "userdata" is generally the user-provided password, possibly cached
	 * from an earlier loop (e.g. to check interactively typing it twice,
	 * or to probe several items in a loop). For us, it should be sslcertpasswd
	 * via SSL_CTX_set_default_passwd_cb_userdata(), but most programs out
	 * there do not have just one variable with one password to think about. */

	if (!buf || size < 1) {
		/* Can not even set buf[0] */
		return 0;
	}

	if (!userdata || !*((char*)userdata)) {
		/* Use what we were told to use (or not), do not surprise
		 * anyone by some hard-coded fallback to sslcertpasswd here! */
		buf[0] = '\0';
		return 0;
	}

	if (strlen((char*)userdata) >= (size_t)size) {
		/* Do not return truncated trash, just say we could not do it */
		return 0;
	}

	strncpy(buf, (char*)userdata, (size_t)size);
	buf[size - 1] = '\0';
	return (int)strlen(buf);
}
#endif

/* Legacy API, without support for client's own certificate in OpenSSL builds */
int upscli_init(int certverify, const char *certpath,
					const char *certname, const char *certpasswd)
{
	return upscli_init2(certverify, certpath, certname, certpasswd, NULL);
}

int upscli_init2(int certverify, const char *certpath,
					const char *certname, const char *certpasswd,
					const char *certfile)
{
	const char	*quiet_init_ssl;
#ifdef WITH_OPENSSL
	long	ret;
	int	ssl_mode = SSL_VERIFY_NONE;
#elif defined(WITH_NSS)	/* WITH_OPENSSL */
	SECStatus	status;
#endif	/* WITH_OPENSSL | WITH_NSS */

#if defined(WITH_OPENSSL) || defined(WITH_NSS)
	if (certname) {
		sslcertname = xstrdup(certname);
	}
	if (certpasswd) {
		sslcertpasswd = xstrdup(certpasswd);
	}
#else	/* neither backend: */
	/* See comment above */
	NUT_UNUSED_VARIABLE(certverify);
	NUT_UNUSED_VARIABLE(certpath);
	NUT_UNUSED_VARIABLE(certname);
	NUT_UNUSED_VARIABLE(certpasswd);
	NUT_UNUSED_VARIABLE(certfile);
#endif	/* WITH_OPENSSL | WITH_NSS */

	if (upscli_initialized == 1) {
		upslogx(LOG_WARNING, "upscli already initialized");
		return -1;
	}

	if (upscli_default_connect_timeout_initialized == 0) {
		/* There may be an envvar waiting to be parsed */
		upsdebugx(1, "%s: upscli_default_connect_timeout was not initialized, checking now",
			__func__);
		upscli_init_default_connect_timeout(NULL, NULL, NULL);
	}

	quiet_init_ssl = getenv("NUT_QUIET_INIT_SSL");
	if (quiet_init_ssl != NULL) {
		if (*quiet_init_ssl == '\0'
			|| (strncmp(quiet_init_ssl, "true", 4)
			&&  strncmp(quiet_init_ssl, "TRUE", 4)
			&&  strncmp(quiet_init_ssl, "1", 1) )
		) {
			upsdebugx(1, "NUT_QUIET_INIT_SSL='%s' value was not recognized, ignored", quiet_init_ssl);
			quiet_init_ssl = NULL;
		}
	}

#ifdef WITH_OPENSSL

# if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_load_error_strings();
	SSL_library_init();

	ssl_ctx = SSL_CTX_new(SSLv23_client_method());
# else
	ssl_ctx = SSL_CTX_new(TLS_client_method());
# endif

	if (!ssl_ctx) {
		upslogx(LOG_ERR, "Can not initialize SSL context");
		return -1;
	}

# if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* set minimum protocol TLSv1 */
	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
# else
	ret = SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_VERSION);
	if (ret != 1) {
		upslogx(LOG_ERR, "Can not set minimum protocol to TLSv1");
		return -1;
	}
# endif

	if (!certpath) {
		if (certverify == 1) {
			upslogx(LOG_ERR, "Can not verify certificate if any is specified: no CERTPATH was given");
			/* Failed: checking the server cert is mandatory, but no
			 * collection of trusted CA/server cert files was given */
			return -1;
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
			upslogx(LOG_ERR, "Failed to load CA certificate(s) from directory %s", certpath);
			return -1;
		}

		SSL_CTX_set_verify(ssl_ctx, ssl_mode, NULL);
	}

	if (sslcertpasswd) {
#  if OPENSSL_VERSION_NUMBER < 0x10100000L
		/* Per https://docs.openssl.org/3.5/man3/SSL_CTX_set_default_passwd_cb,
		 * the `SSL_CTX*` variants were added in 1.1.
		 * The SSL_set_default_passwd_cb() and SSL_set_default_passwd_cb_userdata()
		 * for `SSL*` argument were around since the turn of millennium, approx 0.9.6+
		 * per https://github.com/openssl/openssl/commit/66ebbb6a56bc1688fa37878e4feec985b0c260d7
		 *
		 * But to use those, we would need to get that SSL* (connection-oriented,
		 * maybe from socket FD or dummy SSL_new() with subsequent SSL_shutdown()
		 * and SSL_free(?); that would also unlock us using the ssl_error() elsewhere.
		 */
		upslogx(LOG_ERR, "Private key password support not implemented for OpenSSL < 1.1 yet");
		return -1;
#  else
		/* OpenSSL 1.1.0+ */
		SSL_CTX_set_default_passwd_cb(ssl_ctx, openssl_password_callback);
		SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, (void*)sslcertpasswd);
#  endif
	}

	if (certfile) {
		/* Note: same certfile PEM for cert and private key,
		 * which is optionally protected by sslcertpasswd */
		int	ssl_ret;
		if ((ssl_ret = SSL_CTX_use_certificate_chain_file(ssl_ctx, certfile)) != 1) {
			upslogx(LOG_ERR, "Failed to load client certificate from %s", certfile);
			ssl_debug();
			return -1;
		}
		if ((ssl_ret = SSL_CTX_use_PrivateKey_file(ssl_ctx, certfile, SSL_FILETYPE_PEM)) != 1) {
			upslogx(LOG_ERR, "Failed to load client private key from %s", certfile);
			ssl_debug();
			return -1;
		}
		if ((ssl_ret = SSL_CTX_check_private_key(ssl_ctx)) != 1) {
			upslogx(LOG_ERR, "Failed to check client private key from %s", certfile);
			ssl_debug();
			return -1;
		}

		if (sslcertname && *sslcertname) {
#  if OPENSSL_VERSION_NUMBER >= 0x10002000L
			X509	*x509 = SSL_CTX_get0_certificate(ssl_ctx);
			if (x509) {
				/* Check if sslcertname matches the host (CN or SAN) */
				if (X509_check_host(x509, (const char *)sslcertname, 0, 0, NULL) != 1
				 && X509_check_ip_asc(x509, (const char *)sslcertname, 0) != 1
				) {
					char	*subject = X509_NAME_oneline(X509_get_subject_name(x509), NULL, 0);
					char	*subject_CN = (subject ? (char*)strstr(subject, "CN=") + 3 : NULL);
					size_t	sslcertname_len = strlen(sslcertname);

					upsdebugx(4, "%s: My certificate subject: '%s'; CN: '%s'; CERTIDENT: [%" PRIuSIZE "]'%s'",
						__func__, NUT_STRARG(subject), NUT_STRARG(subject_CN),
						sslcertname_len, NUT_STRARG(sslcertname));

					/* Check if sslcertname matches the whole subject or just .../CN=.../ part as a string */
					if (!subject || !(
						strcmp(subject, sslcertname) == 0
						|| (subject_CN && !strncmp(subject_CN, sslcertname, sslcertname_len)
							&& (subject_CN[sslcertname_len] == '\0' || subject_CN[sslcertname_len] == '/') )
					)) {
						/* This way or that, the names differ */
						upslogx(LOG_ERR, "Certificate subject (%s) does not match CERTIDENT name (%s)",
							subject ? subject : "unknown", sslcertname);
						if (subject) {
							OPENSSL_free(subject);
						}
						upslogx(LOG_ERR, "Unexpected certificate provided");
						return -1;
					} else {
						upsdebugx(2, "Certificate subject verified against CERTIDENT subject name (%s)", sslcertname);
					}
				} else {
					upsdebugx(2, "Certificate subject verified against CERTIDENT host name (%s)", sslcertname);
				}
			}
#  else
			upslogx(LOG_ERR, "Can not verify CERTIDENT '%s': not supported in this OpenSSL build (too old)", sslcertname);
			return -1;
#  endif
		}
	} else {
		if (sslcertname && *sslcertname) {
			upslogx(LOG_ERR, "Can not verify CERTIDENT '%s': no CERTFILE was provided", sslcertname);
			return -1;
		}
	}

#elif defined(WITH_NSS) /* WITH_OPENSSL */

	PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

	PK11_SetPasswordFunc(nss_password_callback);

	if (certfile) {
		upsdebugx(1, "%s: certfile is not used for NSS init, ignored", __func__);
	}

	if (certpath) {
		if (quiet_init_ssl != NULL) {
			upsdebugx(1, "Init SSL with certificate database located at %s", certpath);
		} else {
			upslogx(LOG_INFO, "Init SSL with certificate database located at %s", certpath);
		}
		status = NSS_Init(certpath);
	} else {
		if (quiet_init_ssl != NULL) {
			upsdebugx(1, "Init SSL without certificate database");
		} else {
			upslogx(LOG_NOTICE, "Init SSL without certificate database");
		}
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
	verify_certificate = certverify;
#else
	/* Note: historically we do not return with error here,
	 * and nowadays have the default timeout handling etc.,
	 * just fall through to below and treat as initialized.
	 */
	if (certverify || certpath || certname || certpasswd || certfile) {
		upslogx(LOG_ERR, "upscli_init called but SSL wasn't compiled in");
	}
#endif /* WITH_OPENSSL | WITH_NSS */

	upscli_initialized = 1;

	upsdebugx(1, "%s: completed", __func__);
	return 1;
}

void upscli_add_host_cert(const char* hostname, const char* certname, int certverify, int forcessl)
{
#if defined(WITH_OPENSSL) || defined(WITH_NSS)
	HOST_CERT_t* cert = (HOST_CERT_t *)xmalloc(sizeof(HOST_CERT_t));
	cert->next = first_host_cert;
	cert->host = xstrdup(hostname);
	cert->certname = xstrdup(certname);
	cert->certverify = certverify;
	cert->forcessl = forcessl;
	first_host_cert = cert;
#else
	NUT_UNUSED_VARIABLE(hostname);
	NUT_UNUSED_VARIABLE(certname);
	NUT_UNUSED_VARIABLE(certverify);
	NUT_UNUSED_VARIABLE(forcessl);

	upsdebugx(1, "%s: no-op when libupsclient was not built WITH_SSL", __func__);
#endif /* WITH_NSS */
}

static HOST_CERT_t* upscli_find_host_cert(const char* hostname)
{
#if defined(WITH_OPENSSL) || defined(WITH_NSS)
	HOST_CERT_t* cert = first_host_cert;
	if (hostname != NULL) {
		while (cert != NULL) {
			if (cert->host != NULL && strcmp(cert->host, hostname)==0 ) {
				return cert;
			}
			cert = cert->next;
		}
	}
#else
	NUT_UNUSED_VARIABLE(hostname);

	upsdebugx(4, "%s: no-op when libupsclient was not built WITH_SSL", __func__);
#endif /* WITH_OPENSSL | WITH_NSS */
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

	if (ups->upserror < 0 || ups->upserror > UPSCLI_ERR_MAX) {
		return "Invalid error number";
	}

	switch (upscli_errlist[ups->upserror].flags) {

	case 0:		/* simple error */
		return upscli_errlist[ups->upserror].str;

	case 1:		/* add message from system's strerror */
		snprintf_dynamic(
			ups->errbuf, UPSCLI_ERRBUF_LEN,
			upscli_errlist[ups->upserror].str,
			"%s", strerror(ups->syserrno));
		return ups->errbuf;

	case 2:		/* SSL error, with 1 arg */
#ifdef WITH_OPENSSL
		err = ERR_get_error();
		if (err) {
			ERR_error_string(err, sslbuf);
			snprintf_dynamic(
				ups->errbuf, UPSCLI_ERRBUF_LEN,
				upscli_errlist[ups->upserror].str,
				"%s", sslbuf);
		} else {
			snprintf_dynamic(
				ups->errbuf, UPSCLI_ERRBUF_LEN,
				upscli_errlist[ups->upserror].str,
				"%s", "peer disconnected");
		}
#elif defined(WITH_NSS) /* WITH_OPENSSL */
		if (PR_GetErrorTextLength() > 0 && PR_GetErrorTextLength() + strlen(upscli_errlist[ups->upserror].str) < UPSCLI_ERRBUF_LEN) {
			char	errbuf[UPSCLI_ERRBUF_LEN];
			memset(errbuf, 0, UPSCLI_ERRBUF_LEN);
			PR_GetErrorText(errbuf);
			snprintf_dynamic(
				ups->errbuf, UPSCLI_ERRBUF_LEN,
				upscli_errlist[ups->upserror].str,
				"%s", errbuf);
		} else {
			/* Retry with other metods before giving up, see nss_error() */
			char	err_name_buf[SMALLBUF];
			PRErrorCode	err_num = PR_GetError();
			const char	*err_name = PR_ErrorToName(err_num),
					*err_text = PR_ErrorToString(err_num, PR_LANGUAGE_I_DEFAULT);

			if (err_name) {
				size_t	len = snprintf(err_name_buf, sizeof(err_name_buf), " (%s)", err_name);
				if (len > sizeof(err_name_buf) - 2) {
					err_name_buf[sizeof(err_name_buf) - 2] = ')';
					err_name_buf[sizeof(err_name_buf) - 1] = '\0';
				}
			} else {
				err_name_buf[0] = '\0';
			}

			if (err_text && *err_text
			 && strlen(err_text) + strlen(upscli_errlist[ups->upserror].str) < UPSCLI_ERRBUF_LEN
			) {
				snprintf_dynamic(
					ups->errbuf, UPSCLI_ERRBUF_LEN,
					upscli_errlist[ups->upserror].str,
					"%s", err_text);
				if (err_name && strlen(err_name_buf) + strlen(ups->errbuf) < UPSCLI_ERRBUF_LEN) {
					strncat(ups->errbuf, err_name_buf, UPSCLI_ERRBUF_LEN - strlen(ups->errbuf) - 1);
				}
			} else {
				snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN,
					"SSL error #%ld, message too %s to be displayed",
					(long)PR_GetError(),
					PR_GetErrorTextLength() > 0 ? "long" : "short");
			}
		}
#else
		snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN,
			"SSL error, but SSL wasn't enabled at compile-time");
#endif	/* WITH_OPENSSL | WITH_NSS */
		return ups->errbuf;

	case 3:		/* parsing (parseconf) error */
		snprintf_dynamic(
			ups->errbuf, UPSCLI_ERRBUF_LEN,
			upscli_errlist[ups->upserror].str,
			"%s", ups->pc_ctx.errmsg);
		return ups->errbuf;

	default:
		break;
	}

	/* fallthrough */

	snprintf(ups->errbuf, UPSCLI_ERRBUF_LEN, "Unknown error flag %d",
		upscli_errlist[ups->upserror].flags);

	return ups->errbuf;
}

/* Read up to buflen bytes from fd and return the number of bytes
   read. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
static ssize_t upscli_select_read(const int fd, void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec)
{
	ssize_t		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	upsdebugx(6, "%s: will wait on select() for up to %" PRIuMAX ".%" PRIuMAX " seconds",
		__func__, (uintmax_t)d_sec, (uintmax_t)d_usec);
	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	errno = 0;
	ret = select(fd + 1, &fds, NULL, NULL, &tv);

	if (ret < 1) {
		upsdebug_with_errno(3, "%s: select() failed: %" PRIiSIZE, __func__, ret);
		return ret;
	}

	errno = 0;
	ret = read(fd, buf, buflen);
	if (ret < 1) {
		upsdebug_with_errno(3, "%s: read() failed: %" PRIiSIZE, __func__, ret);
	}

	return ret;
}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
/* internal: abstract the SSL calls for the other functions */
static ssize_t net_read(UPSCONN_t *ups, char *buf, size_t buflen, const time_t timeout)
{
	ssize_t	ret = -1;

#ifdef WITH_SSL
	if (ups->ssl) {
# ifdef WITH_OPENSSL
		int	iret, ssl_err, ssl_retries = 0;
		/* Cap retries to avoid spinning forever on a broken socket.
		 * 250 * 20 ms = 5 s maximum wait, which is generous for a
		 * local handshake while being safe for CI timeouts.
		 */
		const int	SSL_IO_MAX_RETRIES = 250;
		fd_set	fds;
		struct timeval	tv;

		/* SSL_* routines deal with int type for return and buflen
		 * We might need to window our I/O if we exceed 2GB (in
		 * 32-bit builds)... Not likely to exceed in 64-bit builds,
		 * but smaller systems with 16-bits might be endangered :)
		 */
		assert(buflen <= INT_MAX);

		while (ssl_retries < SSL_IO_MAX_RETRIES) {
			iret = SSL_read(ups->ssl, buf, (int)buflen);

			assert(iret <= SSIZE_MAX);
			if (iret > 0) {
				ret = (ssize_t)iret;
				break;
			}

			if (iret == 0) {
				/* Orderly shutdown or actual EOF */
				ret = 0;
				break;
			}

			ssl_err = SSL_get_error(ups->ssl, iret);
			if (ssl_err == SSL_ERROR_WANT_READ
			 || ssl_err == SSL_ERROR_WANT_WRITE
			) {
				FD_ZERO(&fds);
				FD_SET(ups->fd, &fds);
				tv.tv_sec  = 0;
				tv.tv_usec = 20000;	/* 20 ms */

				if (select(ups->fd + 1,
					(ssl_err == SSL_ERROR_WANT_READ)  ? &fds : NULL,
					(ssl_err == SSL_ERROR_WANT_WRITE) ? &fds : NULL,
					NULL, &tv) < 0
				) {
					/* select failure is fatal enough to stop retrying */
					upsdebugx(3, "%s: SSL_read and subsequent select() failed", __func__);
					ssl_error(ups->ssl, (ssize_t)iret);
					ups->upserror = UPSCLI_ERR_SSLERR;
					return -1;
				}
				ssl_retries++;
				continue;
			}

			/* Other errors are fatal */
			upsdebugx(3, "%s: SSL_read failed: %" PRIiSIZE, __func__, (ssize_t)iret);
			ssl_error(ups->ssl, (ssize_t)iret);
			ups->upserror = UPSCLI_ERR_SSLERR;
			return -1;
		}

		if (ssl_retries >= SSL_IO_MAX_RETRIES) {
			upslogx(LOG_ERR, "%s: SSL_read timed out after %d retries", __func__, ssl_retries);
			ups->upserror = UPSCLI_ERR_SSLERR;
			return -1;
		}

		if (ret < 1) {
			upsdebugx(3, "%s: SSL_read failed: %" PRIiSIZE, __func__, ret);
		}
# elif defined(WITH_NSS) /* WITH_OPENSSL */
		/* PR_* routines deal in PRInt32 type
		 * We might need to window our I/O if we exceed 2GB :) */
		assert(buflen <= PR_INT32_MAX);
		ret = PR_Read(ups->ssl, buf, (PRInt32)buflen);
		if (ret < 1) {
			upsdebugx(3, "%s: PR_read failed: %" PRIiSIZE, __func__, ret);
		}
# endif	/* WITH_OPENSSL | WITH_NSS*/

		if (ret < 1) {
			ups->upserror = UPSCLI_ERR_SSLERR;
		}

		return ret;
	}	/* end of: if (ups->ssl) */
#endif	/* WITH_SSL */

	/* Plaintext read */
	ret = upscli_select_read(ups->fd, buf, buflen, timeout, 0);

	/* error reading data, server disconnected? */
	if (ret < 0) {
		upsdebugx(3, "%s: upscli_select_read failed: %" PRIiSIZE, __func__, ret);
		ups->upserror = UPSCLI_ERR_READ;
		ups->syserrno = errno;
	}

	/* no data available, server disconnected? */
	if (ret == 0) {
		upsdebugx(3, "%s: upscli_select_read failed (disconnected?): %" PRIiSIZE, __func__, ret);
		ups->upserror = UPSCLI_ERR_SRVDISC;
	}

	return ret;
}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic pop
#endif

/* Write up to buflen bytes to fd and return the number of bytes
   written. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
static ssize_t upscli_select_write(const int fd, const void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec)
{
	ssize_t		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	errno = 0;
	ret = select(fd + 1, NULL, &fds, NULL, &tv);

	if (ret < 1) {
		upsdebug_with_errno(3, "%s: select() failed: %" PRIiSIZE, __func__, ret);
		return ret;
	}

	errno = 0;
	ret = write(fd, buf, buflen);
	if (ret < 1) {
		upsdebug_with_errno(3, "%s: write() failed: %" PRIiSIZE, __func__, ret);
	}

	return ret;
}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
/* internal: abstract the SSL calls for the other functions */
static ssize_t net_write(UPSCONN_t *ups, const char *buf, size_t buflen, const time_t timeout)
{
	ssize_t	ret = -1;

#ifdef WITH_SSL
	if (ups->ssl) {
# ifdef WITH_OPENSSL
		int	iret, ssl_err, ssl_retries = 0;
		/* Cap retries to avoid spinning forever on a broken socket.
		 * 250 * 20 ms = 5 s maximum wait, which is generous for a
		 * local handshake while being safe for CI timeouts.
		 */
		const int	SSL_IO_MAX_RETRIES = 250;
		fd_set	fds;
		struct timeval	tv;

		/* SSL_* routines deal with int type for return and buflen
		 * We might need to window our I/O if we exceed 2GB (in
		 * 32-bit builds)... Not likely to exceed in 64-bit builds,
		 * but smaller systems with 16-bits might be endangered :)
		 */
		assert(buflen <= INT_MAX);

		while (ssl_retries < SSL_IO_MAX_RETRIES) {
			iret = SSL_write(ups->ssl, buf, (int)buflen);

			assert(iret <= SSIZE_MAX);
			if (iret > 0) {
				ret = (ssize_t)iret;
				break;
			}

			ssl_err = SSL_get_error(ups->ssl, iret);
			if (ssl_err == SSL_ERROR_WANT_READ
			 || ssl_err == SSL_ERROR_WANT_WRITE
			) {
				FD_ZERO(&fds);
				FD_SET(ups->fd, &fds);
				tv.tv_sec  = 0;
				tv.tv_usec = 20000;	/* 20 ms */

				if (select(ups->fd + 1,
					(ssl_err == SSL_ERROR_WANT_READ)  ? &fds : NULL,
					(ssl_err == SSL_ERROR_WANT_WRITE) ? &fds : NULL,
					NULL, &tv) < 0
				) {
					/* select failure is fatal enough to stop retrying */
					upsdebugx(3, "%s: SSL_write and subsequent select() failed", __func__);
					ssl_error(ups->ssl, (ssize_t)iret);
					ups->upserror = UPSCLI_ERR_SSLERR;
					return -1;
				}
				ssl_retries++;
				continue;
			}

			/* Other errors (including iret=0) are fatal */
			upsdebugx(3, "%s: SSL_write failed: %" PRIiSIZE, __func__, (ssize_t)iret);
			ssl_error(ups->ssl, (ssize_t)iret);
			ups->upserror = UPSCLI_ERR_SSLERR;
			return -1;
		}

		if (ssl_retries >= SSL_IO_MAX_RETRIES) {
			upslogx(LOG_ERR, "%s: SSL_write timed out after %d retries", __func__, ssl_retries);
			ups->upserror = UPSCLI_ERR_SSLERR;
			return -1;
		}

		if (ret < 1) {
			upsdebugx(3, "%s: SSL_write failed: %" PRIiSIZE, __func__, ret);
		}
# elif defined(WITH_NSS) /* WITH_OPENSSL */
		/* PR_* routines deal in PRInt32 type
		 * We might need to window our I/O if we exceed 2GB :) */
		assert(buflen <= PR_INT32_MAX);
		ret = PR_Write(ups->ssl, buf, (PRInt32)buflen);
		if (ret < 1) {
			upsdebugx(3, "%s: PR_write failed: %" PRIiSIZE, __func__, ret);
		}
# endif /* WITH_OPENSSL | WITH_NSS */

		if (ret < 1) {
			ups->upserror = UPSCLI_ERR_SSLERR;
		}

		return ret;
	}	/* end of: if (ups->ssl) */
#endif	/* WITH_SSL */

	/* Plaintext write */
	ret = upscli_select_write(ups->fd, buf, buflen, timeout, 0);

	/* error writing data, server disconnected? */
	if (ret < 0) {
		upsdebugx(3, "%s: upscli_select_write failed: %" PRIiSIZE, __func__, ret);
		ups->upserror = UPSCLI_ERR_WRITE;
		ups->syserrno = errno;
	}

	/* not ready for writing, server disconnected? */
	if (ret == 0) {
		upsdebugx(3, "%s: upscli_select_write failed (disconnected?): %" PRIiSIZE, __func__, ret);
		ups->upserror = UPSCLI_ERR_SRVDISC;
	}

	return ret;
}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic pop
#endif

/*
 * 1  : OK
 * -1 : ERROR
 * 0  : SSL NOT SUPPORTED (whether by library or by server)
 */
static int upscli_sslinit(UPSCONN_t *ups, int verifycert)
{
#ifndef WITH_SSL
	NUT_UNUSED_VARIABLE(ups);
	NUT_UNUSED_VARIABLE(verifycert);

	upsdebugx(1, "%s: no-op when libupsclient was not built WITH_SSL", __func__);

	return 0;	/* not supported */

#else	/* WITH_SSL */

# ifdef WITH_OPENSSL
	int res;
# elif defined(WITH_NSS) /* WITH_OPENSSL */
	SECStatus	status;
	PRFileDesc	*socket;
	HOST_CERT_t *cert;
# endif /* WITH_OPENSSL | WITH_NSS */
	char	buf[UPSCLI_NETBUF_LEN];

	/* Intend to initialize upscli with no ssl db if not already done.
	 * Compatibility stuff for old clients which do not initialize them.
	 */
	if (upscli_initialized==0) {
		upsdebugx(3, "upscli not initialized, "
			"force initialisation without SSL configuration");
		upscli_init(0, NULL, NULL, NULL);
	}

	upsdebugx(3, "%s: Trying to STARTTLS", __func__);
	/* see if upsd even talks SSL/TLS */
	snprintf(buf, sizeof(buf), "STARTTLS\n");

	if (upscli_sendline(ups, buf, strlen(buf)) != 0) {
		upsdebugx(3, "%s: STARTTLS not established, failed to send request: %s",
			__func__, upscli_strerror(ups));
		return -1;
	}

	if (upscli_readline(ups, buf, sizeof(buf)) != 0) {
		upsdebugx(3, "%s: STARTTLS not established, failed to read response: %s",
			__func__, upscli_strerror(ups));
		return -1;
	}

	if (strncmp(buf, "ERR ", 4) == 0) {
		upsdebugx(3, "%s: STARTTLS not supported or init error: %s", __func__, buf);
		return 0;		/* not supported */
	}

	if (strncmp(buf, "OK STARTTLS", 11) != 0) {
		upsdebugx(3, "%s: STARTTLS not supported, unexpected response: %s", __func__, buf);
		return 0;		/* not supported */
	}

	/* upsd is happy and said OK, so let's crank up the client */

# ifdef WITH_OPENSSL

	if (!ssl_ctx) {
		upsdebugx(3, "%s: SSL context is not available", __func__);
		return 0;
	}

	ups->ssl = SSL_new(ssl_ctx);
	if (!ups->ssl) {
		upsdebugx(3, "%s: Can not create SSL socket", __func__);
		return 0;
	}

	if (SSL_set_fd(ups->ssl, ups->fd) != 1) {
		upsdebugx(3, "%s: Can not bind file descriptor to SSL socket", __func__);
		return -1;
	}

	if (verifycert != 0) {
		SSL_set_verify(ups->ssl, SSL_VERIFY_PEER, NULL);
	} else {
		SSL_set_verify(ups->ssl, SSL_VERIFY_NONE, NULL);
	}

	{	/* scoping */
		HOST_CERT_t	*cert = upscli_find_host_cert(ups->host);

		if (cert != NULL && cert->certname != NULL) {
			/* We have a setting like upsmon CERTHOST - to pin the certificate
			 * and other security properties for a host, e.g.:
			 * CERTHOST <hostname> <certificate name> <certverify> <forcessl>
			 */
# if OPENSSL_VERSION_NUMBER >= 0x10100000L
			/* hostname verification - OpenSSL 1.1.0+ */
			const char	*verif_host = (cert && cert->certname) ? cert->certname : ups->host;
			X509_VERIFY_PARAM	*vpm = SSL_get0_param(ups->ssl);

			X509_VERIFY_PARAM_set1_host(vpm, verif_host, 0);

			upslogx(LOG_INFO, "Connecting in SSL to '%s' and looking at certificate called '%s'",
				ups->host, cert->certname);
# else
			upslogx(cert->certverify ? LOG_ERR : LOG_WARNING,
				"Connecting in SSL to '%s' and was asked to look at certificate "
				"called '%s', but the OpenSSL library in this build is too old for that. "
				"Please disable the CERTHOST setting or update the library used by NUT. %s",
				ups->host, cert->certname,
				cert->certverify
				? "Refusing connection attempt now because certificate verification was required."
				: "Proceeding without certificate verification as it was not required.");

			if (cert->certverify)
				return -1;
# endif
		} else {
			upslogx(LOG_NOTICE, "Connecting in SSL to '%s' (no certificate name specified)", ups->host);
		}
	}

	/* SSL_connect() on a non-blocking socket requires a retry loop.
	 * When SSL_connect() returns -1 with SSL_ERROR_WANT_READ or
	 * SSL_ERROR_WANT_WRITE it is signalling a non-fatal "not done yet"
	 * condition: the TLS handshake needs more I/O turns to complete.
	 * The correct response is to wait for the fd to become ready in the
	 * indicated direction and call SSL_connect() again with the SAME ssl
	 * object (per OpenSSL docs for all versions >= 0.9.x).
	 *
	 * On Linux the loopback is fast enough that the handshake nearly always
	 * completes in a single call, masking this requirement.  On BSD, macOS,
	 * illumos/OmniOS/OpenIndiana and other non-Linux platforms the loopback
	 * socket behaviour differs enough that WANT_READ/WANT_WRITE are returned
	 * regularly, causing the previous single-shot code to treat a transient
	 * condition as a fatal error and tear down the connection.
	 *
	 * The retry behaviour and the SSL_ERROR_WANT_* codes are identical
	 * across all supported OpenSSL versions (0.9.x, 1.0.x, 1.1.x, 3.x):
	 * the API contract has never changed in this regard.
	 */
	{
		int	ssl_err;
		int	ssl_retries = 0;
		/* Cap retries to avoid spinning forever on a broken socket.
		 * 250 * 20 ms = 5 s maximum wait, which is generous for a
		 * local handshake while being safe for CI timeouts.
		 */
		const int	SSL_IO_MAX_RETRIES = 250;
		fd_set	fds;
		struct timeval	tv;

		res = -1;
		while (ssl_retries < SSL_IO_MAX_RETRIES) {
			res = SSL_connect(ups->ssl);

			if (res == 1) {
				upsdebugx(3, "%s: SSL connected (%s)",
					__func__, SSL_get_version(ups->ssl));
				break;
			}

			ssl_err = SSL_get_error(ups->ssl, res);

			if (ssl_err == SSL_ERROR_WANT_READ
			 || ssl_err == SSL_ERROR_WANT_WRITE
			) {
				/* Non-fatal: handshake needs another I/O turn.
				 * Wait up to 20 ms for the fd to be ready, then
				 * retry SSL_connect() with the same ssl object. */
				FD_ZERO(&fds);
				FD_SET(ups->fd, &fds);
				tv.tv_sec  = 0;
				tv.tv_usec = 20000;	/* 20 ms */

				upsdebugx(4,
					"%s: SSL_connect WANT_%s, retry %d/%d",
					__func__,
					(ssl_err == SSL_ERROR_WANT_READ)
						? "READ" : "WRITE",
					ssl_retries + 1,
					SSL_IO_MAX_RETRIES);

				if (select(ups->fd + 1,
					(ssl_err == SSL_ERROR_WANT_READ)  ? &fds : NULL,
					(ssl_err == SSL_ERROR_WANT_WRITE) ? &fds : NULL,
					NULL, &tv) < 0
				) {
					upsdebug_with_errno(1,
						"%s: select() failed during SSL_connect",
						__func__);
					/* Returns 0 on non-fatal WANT_READ/WRITE;
					 * we stop retrying even if non-fatal because
					 * select() itself failed. */
					ssl_error(ups->ssl, res);
					return -1;
				}
				ssl_retries++;
				continue;
			}

			/* Any other error is fatal */
			if (res == 0) {
				upsdebug_with_errno(1,
					"%s: SSL_connect did not accept handshake"
					" (SSL_ERROR %d)",
					__func__, ssl_err);
			} else {
				upsdebug_with_errno(1,
					"%s: SSL_connect failed"
					" (SSL_ERROR %d)",
					__func__, ssl_err);
			}
			ssl_error(ups->ssl, res);
			return -1;
		}

		if (ssl_retries >= SSL_IO_MAX_RETRIES) {
			upslogx(LOG_ERR,
				"%s: SSL_connect timed out after %d retries"
				" (non-blocking handshake never completed)",
				__func__, ssl_retries);
			ssl_error(ups->ssl, res);
			return -1;
		}
	}

	upsdebugx(3, "%s: Succeeded to STARTTLS (OpenSSL)", __func__);

# elif defined(WITH_NSS) /* WITH_OPENSSL */

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

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_FUNCTION_TYPE_STRICT)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#endif
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
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_FUNCTION_TYPE_STRICT)
#pragma GCC diagnostic pop
#endif

	cert = upscli_find_host_cert(ups->host);
	if (cert != NULL && cert->certname != NULL) {
		upslogx(LOG_INFO, "Connecting in SSL to '%s' and look at certificate called '%s'",
			ups->host, cert->certname);

		status = SSL_SetURL(ups->ssl, cert->certname);
		if (status != SECSuccess) {
			if (!(cert->certverify)) {
				nss_error("upscli_sslinit / SSL_SetURL");
				upslogx(LOG_ERR, "Certificate verification failed for '%s', but was not required, proceeding", ups->host);
				status = SSL_SetURL(ups->ssl, ups->host);
			}
		}
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

	upsdebugx(3, "%s: Succeeded to STARTTLS (NSS)", __func__);

# endif /* WITH_OPENSSL | WITH_NSS */

	/* Make sure handshake succeeded or abort early
	 * (there is currently no way for the server to
	 * report its fault to the client when connection
	 * is half-way secure):
	 */
	if (!upscli_is_valid_protocol_version(ups, NULL)) {
		upslogx(LOG_WARNING, "%s: STARTTLS setup claimed to succeed, but protocol version check in the secured session failed, and SSL is required", __func__);
		ups->ssl = NULL;
		/* Reaction to forceSSL etc. is up to the caller */
		/* TODO: Should caller drop SSL context or restart the connection as plaintext if SSL is not required? */
		return -2;
	}

	return 1;
#endif /* WITH_SSL */
}

int upscli_tryconnect(UPSCONN_t *ups, const char *host, uint16_t port, int flags, struct timeval * timeout)
{
	int				sock_fd;
	struct addrinfo	hints, *res, *ai;
	char			sport[NI_MAXSERV];
	int				v, certverify, tryssl, forcessl, ret;
	HOST_CERT_t*	hostcert;
	fd_set 			wfds;
	int			error;
	socklen_t		error_size;

#ifndef WIN32
	long			fd_flags;
#else	/* WIN32 */
	HANDLE event = NULL;
	unsigned long argp;

	/* Required ritual before calling any socket functions */
	static WSADATA	WSAdata;
	static int	WSA_Started = 0;
	if (!WSA_Started) {
		WSAStartup(2, &WSAdata);
		atexit((void(*)(void))WSACleanup);
		WSA_Started = 1;
	}
#endif	/* WIN32 */

	if (!ups) {
		return -1;
	}

	/* clear out any lingering junk */
	memset(ups, 0, sizeof(*ups));
	ups->upsclient_magic = UPSCLIENT_MAGIC;
	ups->fd = -1;

	if (!host) {
		upslogx(LOG_WARNING, "%s: Host not specified", __func__);
		ups->upserror = UPSCLI_ERR_NOSUCHHOST;
		return -1;
	}

	snprintf(sport, sizeof(sport), "%" PRIuMAX, (uintmax_t)port);

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
			upslogx(LOG_WARNING, "%s: Host not found: '%s'", __func__, NUT_STRARG(host));
			ups->upserror = UPSCLI_ERR_NOSUCHHOST;
			return -1;
		case EAI_MEMORY:
			upslogx(LOG_WARNING, "%s: Insufficient memory", __func__);
			ups->upserror = UPSCLI_ERR_NOMEM;
			return -1;
		case EAI_SYSTEM:
			ups->syserrno = errno;
			break;
		default:
			break;
		}

		upslog_with_errno(LOG_WARNING, "%s: Unknown error happened during getaddrinfo()", __func__);
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
#ifndef WIN32
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags |= O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
#else	/* WIN32 */
			event = CreateEvent(NULL, /* Security */
					FALSE, /* auto-reset */
					FALSE, /* initial state */
					NULL); /* no name */

			/* Associate socket event to the socket via its Event object */
			WSAEventSelect( sock_fd, event, FD_CONNECT );
			CloseHandle(event);
#endif	/* WIN32 */
		}

		while ((v = connect(sock_fd, ai->ai_addr, ai->ai_addrlen)) < 0) {
#ifndef WIN32
			if(errno == EINPROGRESS || SOLARIS_i386_NBCONNECT_ENOENT(errno) || AIX_NBCONNECT_0(errno)) {
#else	/* WIN32 */
			if(errno == WSAEWOULDBLOCK) {
#endif	/* WIN32 */
				FD_ZERO(&wfds);
				FD_SET(sock_fd, &wfds);
				select(sock_fd+1,NULL,&wfds,NULL,
						timeout);
				if (FD_ISSET(sock_fd, &wfds)) {
					error_size = sizeof(error);
					getsockopt(sock_fd, SOL_SOCKET, SO_ERROR,
							SOCK_OPT_CAST &error, &error_size);
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
					ups->upserror = UPSCLI_ERR_CONNFAILURE;
					ups->syserrno = ETIMEDOUT;
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
			/* if timeout, break out so client can continue */
			/* match Linux behavior that updates timeout struct */
			if (timeout != NULL
			 && ups->upserror == UPSCLI_ERR_CONNFAILURE
			 && ups->syserrno == ETIMEDOUT
			) {
				const char	*addrstr = xinet_ntopAI(ai);
				upslogx(LOG_WARNING, "%s: Connection to host timed out: '%s'",
					__func__, (addrstr && *addrstr) ? addrstr : NUT_STRARG(host));
				if (addrstr)
					free((char*)addrstr);
				break;
			}
			continue;
		}

		/* switch back to blocking operation */
		if (timeout != NULL) {
#ifndef WIN32
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags &= ~O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
#else	/* WIN32 */
			argp = 0;
			ioctlsocket(sock_fd, FIONBIO, &argp);
#endif	/* WIN32 */
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

	ups->host = xstrdup(host);

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
			upslogx(LOG_ERR, "Can not connect to NUT server %s in SSL, disconnect", host);
			ups->upserror = UPSCLI_ERR_SSLFAIL;
			upscli_disconnect(ups);
			return -1;
		} else if (tryssl && ret < 0) {
			/* TODO: (ret == -2) Drop SSL context or restart the connection as plaintext if SSL is not required? */
			upslogx(LOG_NOTICE, "Error while connecting to NUT server %s, disconnect", host);
			upscli_disconnect(ups);
			return -1;
		} else if (tryssl && ret == 0) {
			if (certverify != 0) {
				upslogx(LOG_NOTICE, "Can not connect to NUT server %s in SSL and "
					"certificate is needed, disconnect", host);
				upscli_disconnect(ups);
				return -1;
			}
			upsdebugx(3, "Can not connect to NUT server %s in SSL, continue unencrypted", host);
		} else {
			upslogx(LOG_INFO, "Connected to NUT server %s in SSL", host);
			if (certverify == 0) {
				/* you REALLY should set CERTVERIFY to 1 if using SSL... */
				upslogx(LOG_WARNING, "Certificate verification (by client) is disabled");
			} else {
				upsdebugx(1, "Certificate verification (by client) is enabled, and apparently succeeded");
			}
		}
	}

	return 0;
}

int upscli_connect(UPSCONN_t *ups, const char *host, uint16_t port, int flags)
{
	struct timeval tv = {0, 0}, *ptv = NULL;

	if (upscli_default_connect_timeout_initialized == 0) {
		/* There may be an envvar waiting to be parsed */
		upscli_init_default_connect_timeout(NULL, NULL, NULL);

		/* Failed or not (bad envvar), avoid looping messages
		 * about bad value parsing for every upscli_connect() */
		upscli_default_connect_timeout_initialized = 1;
	}

	tv = upscli_default_connect_timeout;
	if (tv.tv_sec != 0 || tv.tv_usec != 0) {
		/* By default, ptv==NULL for a blocking upscli_tryconnect() */
		ptv = &tv;
	}
	return upscli_tryconnect(ups, host, port, flags, ptv);
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
	size_t numarg, const char **arg)
{
	size_t	i;
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

		snprintfcat_dynamic(
			buf, bufsize, format,
			"%s", pconf_encode(arg[i], enc, sizeof(enc)));
	}

	len = strlen(buf);
	snprintf(buf + len, bufsize - len, "\n");
}

/* make sure upsd is giving us what we asked for */
static int verify_resp(size_t num, const char **q, char **a)
{
	size_t	i;

	for (i = 0; i < num; i++) {
		if (strcasecmp(q[i], a[i]) != 0) {

			/* FUTURE: handle -/+ options here */
			return 0;	/* mismatch */
		}
	}

	return 1;	/* OK */
}

int upscli_get(UPSCONN_t *ups, size_t numq, const char **query,
		size_t *numa, char ***answer)
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

int upscli_list_start(UPSCONN_t *ups, size_t numq, const char **query)
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

int upscli_list_next(UPSCONN_t *ups, size_t numq, const char **query,
		size_t *numa, char ***answer)
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

ssize_t upscli_sendline_timeout_may_disconnect(UPSCONN_t *ups, const char *buf, size_t buflen, const time_t timeout, int may_disconnect)
{
	ssize_t	ret;

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
		if (may_disconnect) {
			upsdebugx(3, "%s: net_write() returned %" PRIiSIZE ", disconnecting", __func__, ret);
			upscli_disconnect(ups);
		} else {
			upsdebugx(3, "%s: net_write() returned %" PRIiSIZE ", keeping connection open as caller wants it", __func__, ret);
		}
		return -1;
	}

	return 0;
}

ssize_t upscli_sendline_timeout(UPSCONN_t *ups, const char *buf, size_t buflen, const time_t timeout)
{
	return upscli_sendline_timeout_may_disconnect(ups, buf, buflen, timeout, 1);
}

ssize_t upscli_sendline(UPSCONN_t *ups, const char *buf, size_t buflen)
{
	return upscli_sendline_timeout(ups, buf, buflen, 0);
}

ssize_t upscli_readline_timeout_may_disconnect(UPSCONN_t *ups, char *buf, size_t buflen, const time_t timeout, int may_disconnect)
{
	ssize_t	ret;
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
				if (may_disconnect) {
					upsdebugx(3, "%s: net_read() returned %" PRIiSIZE ", disconnecting", __func__, ret);
					upscli_disconnect(ups);
				} else {
					upsdebugx(3, "%s: net_read() returned %" PRIiSIZE ", keeping connection open as caller wants it", __func__, ret);
				}
				return -1;
			}

			/* Here ret is safe to cast since it is >=1 and certainly
			 * fits under SIZE_MAX being it signed sibling
			 */
			ups->readlen = (size_t)ret;
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

ssize_t upscli_readline_timeout(UPSCONN_t *ups, char *buf, size_t buflen, const time_t timeout)
{
	return upscli_readline_timeout_may_disconnect(ups, buf, buflen, timeout, 1);
}

ssize_t upscli_readline(UPSCONN_t *ups, char *buf, size_t buflen)
{
	return upscli_readline_timeout(ups, buf, buflen, DEFAULT_NETWORK_TIMEOUT);
}

/* split upsname[@hostname[:port]] into separate components */
int upscli_splitname(const char *buf, char **upsname, char **hostname, uint16_t *port)
{
	char	*sat, *ssc, tmp[SMALLBUF], *last = NULL;

	/* paranoia */
	if ((!buf) || (!upsname) || (!hostname) || (!port)) {
		return -1;
	}

	if (snprintf(tmp, sizeof(tmp), "%s", buf) < 1) {
		fprintf(stderr, "upscli_splitname: can't parse empty string\n");
		return -1;
	}

	sat = strchr(tmp, '@');
	ssc = strchr(tmp, ':');

	/* someone passed a "@hostname" string? */
	if (sat == tmp) {
		fprintf(stderr, "upscli_splitname: got empty upsname string\n");
		return -1;
	}

	if ((*upsname = xstrdup(strtok_r(tmp, "@", &last))) == NULL) {
		fprintf(stderr, "upscli_splitname: xstrdup failed\n");
		return -1;
	}

	/* someone passed a "@hostname" string (take two)? */
	if (!**upsname) {
		fprintf(stderr, "upscli_splitname: got empty upsname string\n");
		return -1;
	}

/*
	fprintf(stderr, "upscli_splitname3: got buf='%s', tmp='%s', upsname='%s', possible hostname:port='%s'\n",
		NUT_STRARG(buf), NUT_STRARG(tmp), NUT_STRARG(*upsname), NUT_STRARG((sat ? sat+1 : sat)));
 */

	/* only a upsname is specified, fill in defaults */
	if (sat == NULL) {
		if (ssc) {
			/* TOTHINK: Consult isdigit(ssc+1) to shortcut
			 *  `upsname:port` into `upsname@localhost:port`? */
			fprintf(stderr, "upscli_splitname: port specified, but not a hostname\n");
			return -1;
		}

		if ((*hostname = xstrdup("localhost")) == NULL) {
			fprintf(stderr, "upscli_splitname: xstrdup failed\n");
			return -1;
		}

		*port = NUT_PORT;
		return 0;
	}

	/* someone passed a "upsname@" string? */
	if (!(*(sat+1))) {
		fprintf(stderr, "upscli_splitname: got the @ separator and then an empty hostname[:port] string\n");
		return -1;
	}

	return upscli_splitaddr(sat+1, hostname, port);
}

/* split hostname[:port] into separate components */
int upscli_splitaddr(const char *buf, char **hostname, uint16_t *port)
{
	char	*s, tmp[SMALLBUF], *last = NULL;
	long	l;

	/* paranoia */
	if ((!buf) || (!hostname) || (!port)) {
		return -1;
	}

	if (snprintf(tmp, sizeof(tmp), "%s", buf) < 1) {
		fprintf(stderr, "upscli_splitaddr: can't parse empty string\n");
		return -1;
	}

	s = strchr(tmp, '@');

	/* someone passed a "@hostname" string? */
	if (s) {
		fprintf(stderr, "upscli_splitaddr: wrong call? "
			"Got upsname@hostname[:port] string where "
			"only hostname[:port] was expected: %s\n", buf);
		/* let it pass, but probably fail later */
	}

	if (*tmp == '[') {
		/* NOTE: Brackets are required for colon-separated IPv6
		 * addresses, to differentiate from a port number. For
		 * example, `[1234:5678]:3493` would seem right.
		 */
		if (strchr(tmp, ']') == NULL) {
			fprintf(stderr, "upscli_splitaddr: missing closing bracket in [domain literal]\n");
			return -1;
		}

		if ((*hostname = xstrdup(strtok_r(tmp+1, "]", &last))) == NULL) {
			fprintf(stderr, "upscli_splitaddr: xstrdup failed\n");
			return -1;
		}

		/* no port specified, use default */
		if (((s = strtok_r(NULL, "\0", &last)) == NULL) || (*s != ':')) {
			*port = NUT_PORT;
			return 0;
		}
	} else {
		s = strchr(tmp, ':');

		if ((*hostname = xstrdup(strtok_r(tmp, ":", &last))) == NULL) {
			fprintf(stderr, "upscli_splitaddr: xstrdup failed\n");
			return -1;
		}

		/* no port specified, use default */
		if (s == NULL) {
			*port = NUT_PORT;
			return 0;
		}
	}

	/* Check that "long" port fits in an "uint16_t" so is in IP range
	 * (under 65536).
	 * FIXME: If it is a non-numeric string, try to resolve via
	 *  "services" naming database, with a C equivalent of:
	 *  :;  getent services ssh
	 *      ssh                   22/tcp
	 */
	if ((*(++s) == '\0') || ((l = strtol(s, NULL, 10)) < 1 ) || (l > 65535)) {
		fprintf(stderr, "upscli_splitaddr: no port number specified after ':' separator\n");
		return -1;
	}
	*port = (uint16_t)l;

	return 0;
}

int upscli_is_valid_protocol_version(UPSCONN_t *ups, const char *version_re)
{
	char	version[UPSCLI_NETBUF_LEN];
	size_t	len;

	if (!ups) {
		return -1;
	}

	net_write(ups, "PROTVER\n", 8, 0);
	memset(version, 0, sizeof(version));
	if (net_read(ups, version, sizeof(version), DEFAULT_NETWORK_TIMEOUT) > 0) {
		if (!strncmp(version, "ERR", 3)) {
			version[0] = '\0';
		}
	}

	if (!version[0]) {
		/* Deprecated and hidden, but may be what ancient NUT servers say
		 * May throw if the error is due to (non-)connection */
		net_write(ups, "NETVER\n", 8, 0);
		memset(version, 0, sizeof(version));
		if (net_read(ups, version, sizeof(version), DEFAULT_NETWORK_TIMEOUT) > 0) {
			if (!strncmp(version, "ERR", 3)) {
				version[0] = '\0';
			}
		}
	}

	if (!version[0]) {
		upsdebugx(3, "%s: PROTVER and NETVER queries returned an error, assuming disconnection or non-compliant NUT server", __func__);
		return -1;
	}

	len = strlen(version);
	if (len > 0 && version[len-1] == '\n') {
		version[len-1] = '\0';
	}

	upsdebugx(3, "%s: PROTVER or NETVER returned '%s', matching against '%s'",
		__func__, version, NUT_STRARG(version_re));

	if (!version_re) {
		/* Basic check for 1.0 through 1.3, as of NUT v2.8.2 */
		return (
			!strcmp(version, "1.0") || !strcmp(version, "1.1") ||
			!strcmp(version, "1.2") || !strcmp(version, "1.3")
			);
	}

	// TODO: Regex
	return (!strcmp(version_re, version));
}

int upscli_disconnect(UPSCONN_t *ups)
{
	char	tmp[UPSCLI_NETBUF_LEN];

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

	/* Give it a bit of time to gracefully close connections,
	 * drain the buffer and avoid noise in logs of upsd like:
	 *   write() failed for 127.0.0.1: Transport endpoint is not connected
	 */
	memset(tmp, 0, sizeof(tmp));
	if (net_read(ups, tmp, sizeof(tmp), DEFAULT_NETWORK_TIMEOUT) > 0) {
		if (!strcmp(tmp, "OK Goodbye\n")) {
			/* There may be trailing garbage from the buffer after the newline, not sure why */
			upsdebugx(1, "%s: We logged out, and server said '%s' nicely, as expected", __func__, tmp);
		} else if (!strncmp(tmp, "OK", 2)) {
			upsdebugx(1, "%s: We logged out, and server said '%s' nicely, good enough", __func__, tmp);
		} else {
			upsdebugx(1, "%s: We logged out, and server said '%s', not OK but oh well", __func__, tmp);
		}
	} else {
		upsdebugx(1, "%s: We logged out, and server did not reply in a short time frame", __func__);
	}

#ifdef WITH_OPENSSL
	if (ups->ssl) {
		SSL_shutdown(ups->ssl);
		SSL_free(ups->ssl);
		ups->ssl = NULL;
	}
#elif defined(WITH_NSS) /* !WITH_OPENSSL */
	if (ups->ssl) {
		PR_Shutdown(ups->ssl, PR_SHUTDOWN_BOTH);
		PR_Close(ups->ssl);
		ups->ssl = NULL;
	}
#endif	/* WITH_OPENSSL | WITH_NSS */

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
#endif	/* WITH_SSL */

	return 0;
}

/* Return a bitmap of the abilities for the current libupsclient build */
int upscli_ssl_caps(void)
{
	int	ret = UPSCLI_SSL_CAPS_NONE;

#ifdef WITH_SSL
# ifdef WITH_OPENSSL
	ret |= UPSCLI_SSL_CAPS_OPENSSL;
# endif
# ifdef WITH_NSS
	ret |= UPSCLI_SSL_CAPS_NSS;
# endif
#endif	/* WITH_SSL */

	return ret;
}

/* String version (English) for program help banners etc. */
const char *upscli_ssl_caps_descr(void)
{
	static const char	*ret = "with"
#ifndef WITH_SSL
		"out SSL support";
#else	/* WITH_SSL */
		" SSL support: "
# ifdef WITH_OPENSSL
		"OpenSSL"
#  ifdef WITH_NSS
	/* Not likely we'd get here, but... */
		" and "
#  endif
# endif
# ifdef WITH_NSS
		"Mozilla NSS"
# endif
# if !(defined WITH_NSS) && !(defined WITH_OPENSSL)
		"oddly undefined"
# endif
		;
#endif	/* WITH_SSL */

	return ret;
}

void upscli_report_build_details(void)
{
	upsdebugx(1, "Using NUT libupsclient library version %s built %s", UPSCLI_VERSION, upscli_ssl_caps_descr());
}

int upscli_set_default_connect_timeout(const char *secs) {
	double fsecs;

	if (secs) {
		if (str_to_double(secs, &fsecs, 10) < 1) {
			return -1;
		}
		if (d_equal(fsecs, 0.0)) {
			upscli_default_connect_timeout.tv_sec = 0;
			upscli_default_connect_timeout.tv_usec = 0;
			return 0;
		}
		if (fsecs < 0.0) {
			return -1;
		}
		upscli_default_connect_timeout.tv_sec = (time_t)fsecs;
		fsecs *= 1000000;
		upscli_default_connect_timeout.tv_usec =
			(suseconds_t)((int)fsecs % 1000000);
	}
	else {
		upscli_default_connect_timeout.tv_sec = 0;
		upscli_default_connect_timeout.tv_usec = 0;
	}
	return 0;
}

void upscli_get_default_connect_timeout(struct timeval *ptv) {
	if (ptv) {
		*ptv = upscli_default_connect_timeout;
	}
}

int upscli_init_default_connect_timeout(const char *cli_secs, const char *config_secs, const char *default_secs) {
	const char	*envvar_secs, *cause = "built-in(blocking)";
	int	failed = 0, applied = 0;

	/* First the very default: blocking connections as we always had */
	upscli_default_connect_timeout.tv_sec = 0;
	upscli_default_connect_timeout.tv_usec = 0;

	/* Then try a program's built-in default, if any */
	if (default_secs) {
		if (upscli_set_default_connect_timeout(default_secs) < 0) {
			upslogx(LOG_WARNING, "%s: default_secs='%s' value was not recognized, ignored",
				__func__, default_secs);
			failed++;
		} else {
			cause = "default_secs";
			applied++;
		}
	}

	/* Then override with envvar setting, if any (and if its value is valid) */
	envvar_secs = getenv("NUT_DEFAULT_CONNECT_TIMEOUT");
	if (envvar_secs) {
		if (upscli_set_default_connect_timeout(envvar_secs) < 0) {
			upslogx(LOG_WARNING, "%s: NUT_DEFAULT_CONNECT_TIMEOUT='%s' value was not recognized, ignored",
				__func__, envvar_secs);
			failed++;
		} else {
			cause = "envvar_secs";
			applied++;
		}
	}

	/* Then override with config-file setting, if any (and if its value is valid) */
	if (config_secs) {
		if (upscli_set_default_connect_timeout(config_secs) < 0) {
			upslogx(LOG_WARNING, "%s: config_secs='%s' value was not recognized, ignored",
				__func__, config_secs);
			failed++;
		} else {
			cause = "config_secs";
			applied++;
		}
	}

	/* Then override with command-line setting, if any (and if its value is valid) */
	if (cli_secs) {
		if (upscli_set_default_connect_timeout(cli_secs) < 0) {
			upslogx(LOG_WARNING, "%s: cli_secs='%s' value was not recognized, ignored",
				__func__, cli_secs);
			failed++;
		} else {
			cause = "cli_secs";
			applied++;
		}
	}

	upsdebugx(1, "%s: upscli_default_connect_timeout=%" PRIiMAX
		".%06" PRIiMAX " sec assigned from: %s",
		__func__, (intmax_t)upscli_default_connect_timeout.tv_sec,
		(intmax_t)upscli_default_connect_timeout.tv_usec, cause);

	/* Some non-built-in value was OK */
	if (applied) {
		upscli_default_connect_timeout_initialized++;
		return 0;
	}

	/* None of provided non-built-in values was OK */
	if (failed)
		return -1;

	/* At least we have the built-in default and nothing failed */
	upscli_default_connect_timeout_initialized++;
	return 0;
}

/* Pick up the methods below from libcommon and expose in the NUT client API */
int	upscli_str_contains_token(const char *string, const char *token)
{
	return str_contains_token(string, token);
}

int	upscli_str_add_unique_token(char *tgt, size_t tgtsize, const char *token,
				int (*callback_always)(char *, size_t, const char *),
				int (*callback_unique)(char *, size_t, const char *)
) {
	return str_add_unique_token(tgt, tgtsize, token, callback_always, callback_unique);
}

/* On some platforms, libupsclient builds tend to get a built-in copy
 * of the internal code from NUT libcommon library, so for NUT client
 * programs using both libraries as dynamically-linked shared code,
 * the nut_debug_level setting is backed by independent variables in
 * active memory, and upsdebugx() calls suffer if the library's copy
 * is never changed from zero.
 */

/* privately exported from common.c for internal libs */
const char *setproctag_lib_once(const char *val);

const void *upscli_upslog_cookie(void)
{
	return nut_common_cookie();
}

void upscli_upslog_set_debug_level(int lvl, const void *cookie)
{
	nut_debug_level = lvl;

	if (cookie == upscli_upslog_cookie())
		return;

	setproctag_lib_once("libupsclient");
}

int upscli_upslog_get_debug_level(void)
{
	return nut_debug_level;
}

/* Avoid re-querying /proc or equivalent and logging about it,
 * if the caller is a NUT program that already knows its name:
 * see getmyprocname() in NUT common library */
void upscli_upslog_setprocname(const char *pn, const void *cookie)
{
	if (cookie != upscli_upslog_cookie())
		setproctag_lib_once("libupsclient");

	setmyprocname(pn);
}

void upscli_upslog_setproctag(const char *tag, const void *cookie)
{
	if (cookie != upscli_upslog_cookie())
		setproctag_lib_once("libupsclient");

	setproctag(tag);
}

const char *upscli_upslog_getproctag(void)
{
	return getproctag();
}

struct timeval *upscli_upslog_start_sync(struct timeval *tv, const void *cookie)
{
	if (cookie != upscli_upslog_cookie())
		setproctag_lib_once("libupsclient");

	/* No-op if internal tv equals passed tv */
	return upslog_start_sync(tv);
}
