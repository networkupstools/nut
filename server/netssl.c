/* netssl.c - Interface to OpenSSL for upsd

   Copyright (C)
	2002	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2020 - 2026	Jim Klimov <jimklimov+nut@gmail.com>

   based on the original implementation:

   Copyright (C) 2002  Technorama Ltd. <oss-list-ups@technorama.net>

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

#include "config.h" /* must be the first header */
#include "common.h" /* for upsdebugx() etc */

#include <sys/types.h>
#ifndef WIN32
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <sys/select.h>	/* fd_set and select(); (or sys/time.h on older BSDs) */
#else	/* WIN32 */
#	include "wincompat.h"
#endif	/* WIN32 */

#include "upsd.h"
#include "neterr.h"
#include "netssl.h"
#include "nut_stdint.h"

#ifdef WITH_OPENSSL
# include <openssl/x509v3.h>
#endif

#ifdef WITH_NSS
#	include <pk11pub.h>
#	include <prinit.h>
#	include <private/pprio.h>
# if defined(NSS_VMAJOR) && (NSS_VMAJOR > 3 || (NSS_VMAJOR == 3 && defined(NSS_VMINOR) && NSS_VMINOR >= 39))
#	include <keyhi.h>
#	include <keythi.h>
# else	/* older NSS */
#	include <key.h>
#	include <keyt.h>
# endif	/* NSS before 3.39 */
#	include <secerr.h>
#	include <sslerr.h>
#	include <sslproto.h>
#endif	/* WITH_NSS */

char	*certfile = NULL;
char	*certname = NULL;
char	*certpasswd = NULL;
char	*certpath = NULL;

/* Warning: in this release of NUT, this feature is disabled by default
 * in order to retain compatibility with "least surprise" for earlier
 * existing deployments. Over time it can become enabled by default.
 * See upsd.conf option DISABLE_WEAK_SSL to toggle this in-vivo.
 */
int	disable_weak_ssl = 0;

#ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
int	certrequest = 0;
#endif /* WITH_CLIENT_CERTIFICATE_VALIDATION */

static int	ssl_initialized = 0;

/* Similar to upscli_ssl_caps_descr() for client library,
 * but with more bells and whistles */
const char *net_ssl_caps_descr(void)
{
	static const char	*ret = "with"
#ifndef WITH_SSL
		"out SSL support";
#else
		" SSL support: "

# ifdef WITH_OPENSSL
		"OpenSSL"
#  if !( ( (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_SET_DEFAULT_PASSWD_CB) ) && (defined(HAVE_SSL_CTX_GET0_CERTIFICATE) && HAVE_SSL_CTX_GET0_CERTIFICATE) && (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST) && (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC) && (defined(HAVE_X509_NAME_ONELINE) && HAVE_X509_NAME_ONELINE) )
		" sans CERTIDENT"
#  else
#   if !( (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_SET_DEFAULT_PASSWD_CB) )
		" sans CERTIDENT(pass)"
#   endif
#   if !( (defined(HAVE_SSL_CTX_GET0_CERTIFICATE) && HAVE_SSL_CTX_GET0_CERTIFICATE) && (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST) && (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC) && (defined(HAVE_X509_NAME_ONELINE) && HAVE_X509_NAME_ONELINE) )
		" sans CERTIDENT(name)"
#   endif
#  endif
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

		"; with"
# ifndef WITH_CLIENT_CERTIFICATE_VALIDATION
		"out"
# endif /* WITH_CLIENT_CERTIFICATE_VALIDATION */
		" client certificate validation";
#endif

	return ret;
}

#ifndef WITH_SSL

/* stubs for non-ssl compiles */
void net_starttls(nut_ctype_t *client, size_t numarg, const char **arg)
{
	NUT_UNUSED_VARIABLE(client);
	NUT_UNUSED_VARIABLE(numarg);
	NUT_UNUSED_VARIABLE(arg);

	send_err(client, NUT_ERR_FEATURE_NOT_SUPPORTED);
	return;
}

ssize_t ssl_write(nut_ctype_t *client, const char *buf, size_t buflen)
{
	NUT_UNUSED_VARIABLE(client);
	NUT_UNUSED_VARIABLE(buf);
	NUT_UNUSED_VARIABLE(buflen);

	upslogx(LOG_ERR, "ssl_write called but SSL wasn't compiled in");
	return -1;
}

ssize_t ssl_read(nut_ctype_t *client, char *buf, size_t buflen)
{
	NUT_UNUSED_VARIABLE(client);
	NUT_UNUSED_VARIABLE(buf);
	NUT_UNUSED_VARIABLE(buflen);

	upslogx(LOG_ERR, "ssl_read called but SSL wasn't compiled in");
	return -1;
}

void ssl_init(void)
{
	ssl_initialized = 0;	/* keep gcc quiet */
}

void ssl_finish(nut_ctype_t *client)
{
	if (client->ssl) {
		upslogx(LOG_ERR, "ssl_finish found active SSL connection but SSL wasn't compiled in");
	}
}

void ssl_cleanup(void)
{
}

#else	/* ifdef WITH_SSL: */

# ifdef WITH_OPENSSL

static SSL_CTX	*ssl_ctx = NULL;

#  ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
/* Adapted from https://stackoverflow.com/a/42477707 with references to
 * https://wiki.openssl.org/index.php/SSL/TLS_Client and further cURL,
 * and https://www.zedwood.com/article/c-openssl-parse-x509-certificate-pem
 * FIXME: dedup something with upsclient.c and nutclient.cpp?
 */
static int openssl_cert_verify_san_name(const char* label, X509* const cert, const char *hostname)
{
	int	san_seen = 0, ok = 0;
	GENERAL_NAMES*	names = NULL;
	unsigned char*	utf8 = NULL;

	do
	{
		int	i, count;

		if (!cert) break; /* failed */

		names = (GENERAL_NAMES *)X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0);
		if (!names) break;

		count = sk_GENERAL_NAME_num(names);
		if (!count) break; /* failed */

		for (i = 0; i < count; ++i) {
			GENERAL_NAME* entry = sk_GENERAL_NAME_value(names, i);
			if (!entry) continue;

			if (GEN_DNS == entry->type) {
				int	len1 = 0, len2 = -1;

				len1 = ASN1_STRING_to_UTF8(&utf8, entry->d.dNSName);
				if (utf8) {
					len2 = (int)strlen((const char*)utf8);
				}

				if (len1 != len2) {
					upsdebugx(5, "%s: %s: strlen and ASN1_STRING size "
						"do not match (embedded null?): %d vs %d",
						__func__, label, len2, len1);
				}

				/* If there's a problem with string lengths, then
				 * we skip the candidate and move on to the next.
				 * Another policy would be to fail, since it probably
				 * indicates the server is under attack by a corrupt
				 * client. Or "just" a loosely configured one?
				 */
				if (utf8 && len1 && len2 && (len1 == len2)) {
					san_seen = 1;
					if (hostname && *hostname && !strcasecmp((const char*)utf8, hostname)) {
						upsdebugx(5, "%s: %s: [DNS]\t%s\t: MATCHED '%s'",
							__func__, label, utf8, hostname);
						ok = 1;
					} else {
						/* TOTHINK: Wildcard certs, with respect to TLD constraints
						 *  (do not accept *.com) etc. if we !HAVE_X509_CHECK_HOST ? */
						upsdebugx(5, "%s: %s: [DNS]\t%s\t: DID NOT MATCH '%s'",
							__func__, label, utf8, NUT_STRARG(hostname));
					}
				} else {
					upsdebugx(4, "%s: WARNING: there is some mismatch about "
						"a SAN entry in %s: [DNS]\t%s (len1=%d len2=%d)",
						__func__, label, NUT_STRARG((char*)utf8), len1, len2);
				}

				if (utf8) {
					OPENSSL_free(utf8);
					utf8 = NULL;
				}
			} else if (GEN_IPADD == entry->type) {
				/* https://datatracker.ietf.org/doc/html/rfc5280#section-4.2.1.6:
				 * When the subjectAltName extension contains an iPAddress,
				 * the address MUST be stored in the octet string in "network
				 * byte order", as specified in [RFC791].  The least significant
				 * bit (LSB) of each octet is the LSB of the corresponding byte
				 * in the network address.  For IP version 4, as specified in
				 * [RFC791], the octet string MUST contain exactly four octets.
				 * For IP version 6, as specified in [RFC2460], the octet string
				 * MUST contain exactly sixteen octets.
				 */
				char	ip_addr_buf[128], *p = ip_addr_buf, *pMax = ip_addr_buf + sizeof(ip_addr_buf) - 5;
				const unsigned char	*ip_addr_raw =
# ifdef HAVE_ASN1_STRING_GET0_DATA
					ASN1_STRING_get0_data(entry->d.iPAddress)
# elif defined(HAVE_ASN1_STRING_DATA)
					ASN1_STRING_data(entry->d.iPAddress)
# else
					(const unsigned char *)entry->d.iPAddress->data
# endif
					;
				int	ip_addr_raw_len =
# ifdef HAVE_ASN1_STRING_LENGTH
					ASN1_STRING_length(entry->d.iPAddress)
# else
					(int)entry->d.iPAddress->length
# endif
					, j;

				memset(ip_addr_buf, 0, sizeof(ip_addr_buf));
				switch (ip_addr_raw_len) {
					case 4:
						for (j = 0; j < ip_addr_raw_len && p < pMax; j++) {
							p += snprintf(p,
								sizeof(ip_addr_buf) - (p - ip_addr_buf) - 1,
								"%u%s",
								ip_addr_raw[j],
								(j == ip_addr_raw_len - 1) ? "" : ".");
						}
						break;

					case 16:
						/* TOTHINK: There are many ways to print an IPv6 address;
						 *  maybe we should rather convert the expected address
						 *  into an array of 16 chars and compare that?
						 *  For reporting, however, this is good enough, even if
						 *  a bit wasteful. */
						for (j = 0; j < ip_addr_raw_len && p < pMax; j++) {
							p += snprintf(p,
								sizeof(ip_addr_buf) - (p - ip_addr_buf) - 1,
								"%02x%s",
								ip_addr_raw[j],
								(j == ip_addr_raw_len - 1) ? "" : ":");
						}
						break;

					default:
						upsdebugx(5, "%s: %s: invalid IP address length: %d",
							__func__, label, ip_addr_raw_len);
						continue;
				}

				san_seen = 1;
				if (hostname && *hostname && !strcasecmp((const char*)ip_addr_buf, hostname)) {
					upsdebugx(5, "%s: %s: [%s]\t%s\t: MATCHED '%s'",
						__func__, label,
						(ip_addr_raw_len == 4 ? "IPv4" : "IPv6"),
						ip_addr_buf, hostname);
					ok = 1;
				} else {
					/* TOTHINK: invert the check as commented above, if we !HAVE_X509_CHECK_IP_ASC ? */
					upsdebugx(5, "%s: %s: [%s]\t%s\t: DID NOT MATCH '%s'",
						__func__, label,
						(ip_addr_raw_len == 4 ? "IPv4" : "IPv6"),
						ip_addr_buf, NUT_STRARG(hostname));
				}
			} else
			{
				/* GEN_URI, RID, email, etc. - not something we
				 * care about for network server/client certs */
				upsdebugx(5, "%s: Unknown GENERAL_NAME type, or irrelevant for certificate vs. hostname validation: %d", __func__, entry->type);
			}
		}
	} while (0);

	if (!ok && hostname && *hostname && (0
# if (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST)
	 || (X509_check_host(cert, (const char *)hostname, 0, 0, NULL) == 1)
# endif
# if (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC)
	 || (X509_check_ip_asc(cert, (const char *)hostname, 0) == 1)
# endif
	)) {
		upsdebugx(5, "%s: %s: MATCHED '%s' using OpenSSL-provided methods",
			__func__, label, hostname);
		ok = 1;
	}

	if (names)
		GENERAL_NAMES_free(names);

	if (utf8)
		OPENSSL_free(utf8);

	if (!san_seen) {
		upsdebugx(4, "%s: %s: subjAltNames not available", __func__, label);
	} else {
		if (!ok) {
			upsdebugx(4, "%s: %s: subjAltNames available, but did not match '%s'", __func__, label, hostname);
		} else {
			upsdebugx(4, "%s: %s: subjAltNames available and at least one matched '%s'", __func__, label, hostname);
		}
	}

	return ok;
}

/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example
 * FIXME: dedup something with upsclient.c and nutclient.cpp?
 */
static int	openssl_cert_verify_data_index;
static int	verify_depth = 9;	/* openssl default */

static int openssl_cert_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	char	buf[SMALLBUF];
	X509	*err_cert;
	int	err, depth;
	SSL	*ssl;
	openssl_cert_verify_data_t	*openssl_cert_verify_data;

	err_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	/* Retrieve the pointer to the SSL of the connection currently treated
	 * and the application-specific data stored into the SSL object.
	 */
	ssl = (SSL *)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	openssl_cert_verify_data = (openssl_cert_verify_data_t *)SSL_get_ex_data(ssl, openssl_cert_verify_data_index);

	X509_NAME_oneline(X509_get_subject_name(err_cert), buf, sizeof(buf));

	/* Sanity-check */
	if (!openssl_cert_verify_data) {
		upsdebugx(4, "%s: openssl_cert_verify_data settings not passed, return ok=%d provided by caller: depth=%d:%s",
			__func__, preverify_ok, depth, buf);
		return preverify_ok;
	}

	/* This is the counterpart's own cert */
	if (depth == 0 && !preverify_ok) {
		/* Call this in any err case, to print debug logs about
		 *  presence and value(s) of subjAltNames in that cert */
		int	san_ok = openssl_cert_verify_san_name(buf, err_cert, openssl_cert_verify_data->hostname);
		if (san_ok && err == X509_V_ERR_HOSTNAME_MISMATCH) {
			/* Caller had some problem with it, did SAN match fix it? */
			upsdebugx(5, "%s: originally called with verify error:num=%d:%s:depth=%d:%s "
				"probably by CN, but SAN matched - reporting ok=%d and clearing error state",
				__func__, err,
				X509_verify_cert_error_string(err),
				depth, buf, san_ok);
			err = 0;
			X509_STORE_CTX_set_error(ctx, err);
			return san_ok;
		}
	}

	/* Catch a too long certificate chain. The depth limit set using
	 * SSL_CTX_set_verify_depth() is by purpose set to "limit+1" so
	 * that whenever the "depth>verify_depth" condition is met, we
	 * have violated the limit and want to log this error condition.
	 * We must do it here, because the CHAIN_TOO_LONG error would not
	 * be found explicitly; only errors introduced by cutting off the
	 * additional certificates would be logged.
	 */
	if (depth > openssl_cert_verify_data->verify_depth) {
		preverify_ok = 0;
		err = X509_V_ERR_CERT_CHAIN_TOO_LONG;
		X509_STORE_CTX_set_error(ctx, err);
	}

	if (!preverify_ok) {
		upsdebugx(5, "%s: called with verify error:num=%d:%s:depth=%d:%s",
			__func__, err,
			X509_verify_cert_error_string(err),
			depth, buf);
	}
	else if (openssl_cert_verify_data->verbose_mode)
	{
		upsdebugx(5, "%s: called with depth=%d:%s", __func__, depth, buf);
	}

	/* At this point, err contains the last verification error.
	 * We can use it for something special, like a report: */
	if (!preverify_ok && (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT))
	{
		char	bufCA[SMALLBUF];
		/* In older versions maybe: X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, sizeof(buf)); */
		X509_NAME_oneline(X509_get_issuer_name(X509_STORE_CTX_get_current_cert(ctx)), bufCA, sizeof(bufCA));
		upsdebugx(5, "%s: issuer=%s", __func__, bufCA);
	}

	if (openssl_cert_verify_data->always_continue) {
		upsdebugx(4, "%s: requested to always continue, return ok=1 (not %d provided by caller): depth=%d:%s", __func__, preverify_ok, depth, buf);
		return 1;
	}

	upsdebugx(4, "%s: return ok=%d provided by caller: depth=%d:%s", __func__, preverify_ok, depth, buf);
	return preverify_ok;
}
#  endif	/* WITH_CLIENT_CERTIFICATE_VALIDATION */

#  if (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_SET_DEFAULT_PASSWD_CB)
static int openssl_password_callback(char *buf, int size, int rwflag, void *userdata)
{
	/* See https://docs.openssl.org/1.0.2/man3/SSL_CTX_set_default_passwd_cb */
	/* is callback used for reading/decryption (rwflag=0) or writing/encryption (rwflag=1)? */
	NUT_UNUSED_VARIABLE(rwflag);
	/* "userdata" is generally the user-provided password, possibly cached
	 * from an earlier loop (e.g. to check interactively typing it twice,
	 * or to probe several items in a loop). For us, it should be certpasswd
	 * via SSL_CTX_set_default_passwd_cb_userdata(), but most programs out
	 * there do not have just one variable with one password to think about. */

	if (!buf || size < 1) {
		/* Can not even set buf[0] */
		return 0;
	}

	if (!userdata || !*((char*)userdata)) {
#   if (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB_USERDATA) && HAVE_SSL_SET_DEFAULT_PASSWD_CB_USERDATA)
		/* Use what we were told to use (or not), do not surprise
		 * anyone by some hard-coded fallback to certpasswd here! */
		buf[0] = '\0';
		return 0;
#   else
		userdata = (void*)certpasswd;
#   endif
	}

	if (strlen((char*)userdata) >= (size_t)size) {
		/* Do not return truncated trash, just say we could not do it */
		return 0;
	}

	strncpy(buf, (char*)userdata, (size_t)size);
	buf[size - 1] = '\0';
	return (int)strlen(buf);
}
#  endif	/* ...SET_DEFAULT_PASSWD_CB */

static void ssl_debug(void)
{
	unsigned long	e;
	char	errmsg[SMALLBUF];

	while ((e = ERR_get_error()) != 0) {
		ERR_error_string_n(e, errmsg, sizeof(errmsg));
		upsdebugx(1, "ssl_debug: %s", errmsg);
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
			upsdebugx(1, "ssl_error() EOF from client");
		} else {
			upsdebugx(1, "ssl_error() ret=%" PRIiSIZE " SSL_ERROR_SYSCALL", ret);
		}
		break;

	default:
		upsdebugx(1, "ssl_error() ret=%" PRIiSIZE " SSL_ERROR %d", ret, e);
		ssl_debug();
	}

	return -1;
}

# elif defined(WITH_NSS) /* not WITH_OPENSSL */

static CERTCertificate	*cert;
static SECKEYPrivateKey	*privKey;

static char *nss_password_callback(PK11SlotInfo *slot, PRBool retry,
		void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	if (retry) {
		/* Force not inted to retrieve password many times. */
		return NULL;
	}
	upslogx(LOG_INFO, "Intend to retrieve password for %s / %s: password %sconfigured",
		PK11_GetSlotName(slot), PK11_GetTokenName(slot),
		certpasswd ? "" : "not ");
	return certpasswd ? PL_strdup(certpasswd) : NULL;
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

static int ssl_error(PRFileDesc *ssl, ssize_t ret)
{
	char	buffer[256], err_name_buf[SMALLBUF];
	PRErrorCode	err_num = PR_GetError();
	const char	*err_name = PR_ErrorToName(err_num);
	PRInt32	err_len = PR_GetErrorTextLength();
	NUT_UNUSED_VARIABLE(ssl);
	NUT_UNUSED_VARIABLE(ret);

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
		if ((size_t)err_len < sizeof(buffer)) {
			PRInt32	length = PR_GetErrorText(buffer);
			upsdebugx(1, "ssl_error %ld%s : %*s", (long)err_num, err_name_buf, length, buffer);
		} else {
			upsdebugx(1, "ssl_error %ld%s : Internal error buffer too small, needs %ld bytes", (long)err_num, err_name_buf, (long)err_len);
		}
	} else {
		/* The code above may be obsolete or not ubiquitous, try another way */
		const char	*err_text = PR_ErrorToString(err_num, PR_LANGUAGE_I_DEFAULT);
		if (err_text && *err_text) {
			upsdebugx(1, "ssl_error %ld%s : %s", (long)err_num, err_name_buf, err_text);
		} else {
			upsdebugx(1, "ssl_error %ld%s", (long)err_num, err_name_buf);
		}
	}

	return -1;
}

static SECStatus AuthCertificate(CERTCertDBHandle *arg, PRFileDesc *fd,
	PRBool checksig, PRBool isServer)
{
	nut_ctype_t	*client = (nut_ctype_t *)SSL_RevealPinArg(fd);
	SECStatus	status  = SSL_AuthCertificate(arg, fd, checksig, isServer);

	upslogx(LOG_INFO, "Intend to authenticate client %s : %s.",
		client?client->addr:"<unnamed>",
		status==SECSuccess?"SUCCESS":"FAILED");

	if (status != SECSuccess) {
		nss_error(isServer ? "SSL_AuthCertificate(server)" : "SSL_AuthCertificate(client)");
	}

	return status;
}

static SECStatus BadCertHandler(nut_ctype_t *arg, PRFileDesc *fd)
{
	NUT_UNUSED_VARIABLE(fd);

	upslogx(LOG_WARNING, "Certificate validation failed for %s",
		(arg&&arg->addr)?arg->addr:"<unnamed>");

#  ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
	/* BadCertHandler is called when the NSS certificate validation is failed.
	 * If the certificate verification (user conf) is mandatory, reject authentication
	 * else accept it.
	 */
	return (certrequest == NETSSL_CERTREQ_REQUIRE) ? SECFailure : SECSuccess;
#  else	/* not WITH_CLIENT_CERTIFICATE_VALIDATION */
	/* Always accept clients. */
	return SECSuccess;
#  endif	/* WITH_CLIENT_CERTIFICATE_VALIDATION */
}

static void HandshakeCallback(PRFileDesc *fd, nut_ctype_t *client_data)
{
	NUT_UNUSED_VARIABLE(fd);

	upslogx(LOG_INFO, "SSL handshake done successfully with client %s",
		client_data->addr);
}


# endif	/* WITH_OPENSSL | WITH_NSS */

void net_starttls(nut_ctype_t *client, size_t numarg, const char **arg)
{
# ifdef WITH_OPENSSL
	int	ret;
# elif defined(WITH_NSS)	/* WITH_OPENSSL */
	SECStatus	status;
	PRFileDesc	*socket;
# endif	/* WITH_OPENSSL | WITH_NSS */

	static char	msg_id_ssl[] =
# ifdef WITH_OPENSSL
		" OpenSSL"
# elif defined(WITH_NSS)	/* WITH_OPENSSL */
		" NSS"
# else	/* neither */
		"out"
# endif	/* WITH_OPENSSL | WITH_NSS */
		;

	NUT_UNUSED_VARIABLE(numarg);
	NUT_UNUSED_VARIABLE(arg);

	upsdebugx(2, "%s: handling a connection upgrade request, server side built with%s SSL support", __func__, msg_id_ssl);

	if (client->ssl) {
		upsdebugx(2, "%s: NUT_ERR_ALREADY_SSL_MODE because this connection is already initialized as SSL",
			__func__);
		send_err(client, NUT_ERR_ALREADY_SSL_MODE);
		return;
	}

	client->ssl_connected = 0;

	if (
# ifdef WITH_OPENSSL
		(!certfile) ||
# elif defined(WITH_NSS)	/* not WITH_OPENSSL */
		(!certpath) ||
# endif
		(!ssl_initialized)
	) {
		upsdebugx(2, "%s: NUT_ERR_FEATURE_NOT_CONFIGURED due to one or more of: "
			"certfile='%s' certpath='%s' certname='%s' ssl_initialized=%d",
			__func__, NUT_STRARG(certfile), NUT_STRARG(certpath),
			NUT_STRARG(certname), ssl_initialized);
		send_err(client, NUT_ERR_FEATURE_NOT_CONFIGURED);
		return;
	}

# ifdef WITH_OPENSSL
	if (!ssl_ctx)
# elif defined(WITH_NSS)	/* not WITH_OPENSSL */
	if (!NSS_IsInitialized())
# endif	/* WITH_OPENSSL | WITH_NSS */
	{
		upsdebugx(2, "%s: NUT_ERR_FEATURE_NOT_CONFIGURED due to lack of initialized context",
			__func__);
		send_err(client, NUT_ERR_FEATURE_NOT_CONFIGURED);
		ssl_initialized = 0;
		return;
	}

	/* Note that after this message, the client assumes that communications
	 * are encrypted (no more plaintext, even if we wanted to report that
	 * some further SSL setup failed, e.g. due to untrusted certificates
	 * as seen during handshake).
	 */
	if (!sendback(client, "OK STARTTLS\n")) {
		upsdebug_with_errno(2, "%s: could not confirm the beginning of SSL ritual to prospective SSL client", __func__);
		return;
	}

# ifdef WITH_OPENSSL

	client->ssl = SSL_new(ssl_ctx);

	if (!client->ssl) {
		upslog_with_errno(LOG_ERR, "SSL_new failed\n");
		ssl_debug();
		return;
	}

	if (SSL_set_fd(client->ssl, client->sock_fd) != 1) {
		upslog_with_errno(LOG_ERR, "SSL_set_fd failed\n");
		ssl_debug();
		return;
	}

# ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
	upsdebugx(4, "%s: certrequest=%d", __func__, certrequest);
	if (certrequest == NETSSL_CERTREQ_REQUEST
	 || certrequest == NETSSL_CERTREQ_REQUIRE
	) {
		/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example:
		 * Set up the SSL specific data into "openssl_cert_verify_data"
		 * and store it into the SSL structure. */
		if (client->openssl_cert_verify_data.hostname_allocated
		 && client->openssl_cert_verify_data.hostname)
			free((void *)client->openssl_cert_verify_data.hostname);
		memset(&(client->openssl_cert_verify_data), 0, sizeof(client->openssl_cert_verify_data));

		client->openssl_cert_verify_data.verify_depth = verify_depth;
		client->openssl_cert_verify_data.hostname = client->addr;	/* not allocated, not to be freed with the structure */
		SSL_set_ex_data(client->ssl, openssl_cert_verify_data_index, &(client->openssl_cert_verify_data));

		SSL_set_verify(client->ssl,
			(certrequest == NETSSL_CERTREQ_REQUIRE
				? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT
				: SSL_VERIFY_PEER),
			openssl_cert_verify_callback);
	}
# endif

	/* SSL_accept() on a non-blocking socket (which upsd uses) requires a
	 * retry loop. When SSL_accept() returns -1 with SSL_ERROR_WANT_READ or
	 * SSL_ERROR_WANT_WRITE it is signalling a non-fatal "not done yet"
	 * condition: the TLS handshake needs more I/O turns to complete.
	 * The correct response is to wait for the fd to become ready in the
	 * indicated direction and call SSL_accept() again with the SAME ssl
	 * object and arguments (per OpenSSL docs for all versions >= 0.9.x).
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

		ret = -1;
		while (ssl_retries < SSL_IO_MAX_RETRIES) {
			ret = SSL_accept(client->ssl);

			if (ret == 1) {
				client->ssl_connected = 1;
				upsdebugx(3, "SSL_accept succeeded (%s)",
					SSL_get_version(client->ssl));

				/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example */
				if (SSL_get_peer_certificate(client->ssl)) {
					if (SSL_get_verify_result(client->ssl) == X509_V_OK) {
						upsdebugx(3, "%s: The client sent a certificate which verified OK", __func__);
					} else {
						upsdebugx(3, "%s: The client sent a certificate which did not verify OK", __func__);
					}
				} else {
					upsdebugx(3, "%s: The client did not send a certificate", __func__);
				}
				break;
			}

			ssl_err = SSL_get_error(client->ssl, ret);

			if (ssl_err == SSL_ERROR_WANT_READ
			 || ssl_err == SSL_ERROR_WANT_WRITE
			) {
				/* Non-fatal: handshake needs another I/O turn.
				 * Wait up to 20 ms for the fd to be ready, then
				 * retry SSL_accept() with the same ssl object.  */
				FD_ZERO(&fds);
				FD_SET(client->sock_fd, &fds);
				tv.tv_sec  = 0;
				tv.tv_usec = 20000;	/* 20 ms */

				upsdebugx(4,
					"%s: SSL_accept WANT_%s, retry %d/%d",
					__func__,
					(ssl_err == SSL_ERROR_WANT_READ)
						? "READ" : "WRITE",
					ssl_retries + 1,
					SSL_IO_MAX_RETRIES);

				if (select(client->sock_fd + 1,
					(ssl_err == SSL_ERROR_WANT_READ)  ? &fds : NULL,
					(ssl_err == SSL_ERROR_WANT_WRITE) ? &fds : NULL,
					NULL, &tv) < 0
				) {
					upslog_with_errno(LOG_ERR,
						"%s: select() failed during SSL_accept",
						__func__);
					/* Returns 0 on non-fatal WANT_READ/WRITE;
					 * we stop retrying even if non-fatal because
					 * select() itself failed. */
					ssl_error(client->ssl, ret);
					return;
				}
				ssl_retries++;
				continue;
			}

			/* Any other error is fatal */
			if (ret == 0) {
				upslog_with_errno(LOG_ERR,
					"%s: SSL_accept did not accept handshake"
					" (SSL_ERROR %d)",
					__func__, ssl_err);
			} else {
				upslog_with_errno(LOG_ERR,
					"%s: SSL_accept failed"
					" (SSL_ERROR %d)",
					__func__, ssl_err);
			}
			ssl_error(client->ssl, ret);
			return;
		}

		if (ssl_retries >= SSL_IO_MAX_RETRIES) {
			upslogx(LOG_ERR,
				"%s: SSL_accept timed out after %d retries"
				" (non-blocking handshake never completed)",
				__func__, ssl_retries);
			ssl_error(client->ssl, ret);
			return;
		}
	}

# elif defined(WITH_NSS)	/* not WITH_OPENSSL */

	upsdebugx(4, "%s: calling PR_ImportTCPSocket()", __func__);
	socket = PR_ImportTCPSocket(client->sock_fd);
	if (socket == NULL) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / PR_ImportTCPSocket");
		return;
	}

	upsdebugx(4, "%s: calling SSL_ImportFD()", __func__);
	client->ssl = SSL_ImportFD(NULL, socket);
	if (client->ssl == NULL) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / SSL_ImportFD");
		return;
	}

	upsdebugx(4, "%s: calling SSL_SetPKCS11PinArg()", __func__);
	if (SSL_SetPKCS11PinArg(client->ssl, client) == -1) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / SSL_SetPKCS11PinArg");
		return;
	}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_FUNCTION_TYPE_STRICT)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#endif
	/* Note cast to SSLAuthCertificate to prevent warning due to
	 * bad function prototype in NSS.
	 */
	upsdebugx(4, "%s: calling SSL_AuthCertificateHook()", __func__);
	status = SSL_AuthCertificateHook(client->ssl, (SSLAuthCertificate)AuthCertificate, CERT_GetDefaultCertDB());
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / SSL_AuthCertificateHook");
		return;
	}

	upsdebugx(4, "%s: calling SSL_BadCertHook()", __func__);
	status = SSL_BadCertHook(client->ssl, (SSLBadCertHandler)BadCertHandler, client);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / SSL_BadCertHook");
		return;
	}

	upsdebugx(4, "%s: calling SSL_HandshakeCallback()", __func__);
	status = SSL_HandshakeCallback(client->ssl, (SSLHandshakeCallback)HandshakeCallback, client);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / SSL_HandshakeCallback");
		return;
	}

	upsdebugx(4, "%s: calling SSL_ConfigSecureServer()", __func__);
	status = SSL_ConfigSecureServer(client->ssl, cert, privKey, NSS_FindCertKEAType(cert));
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / SSL_ConfigSecureServer");
		return;
	}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_FUNCTION_TYPE_STRICT)
#pragma GCC diagnostic pop
#endif

	upsdebugx(4, "%s: calling SSL_ResetHandshake()", __func__);
	status = SSL_ResetHandshake(client->ssl, PR_TRUE);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL connection");
		nss_error("net_starttls / SSL_ResetHandshake");
		return;
	}

	/* Note: this call can generate memory leaks not resolvable
	 * by any release function.
	 * Probably SSL session key object allocation.
	 *
	 * It also seems to block indefinitely (tested a minute),
	 * until it decides that the handshake succeeded or failed.
	 * A malicious or broken client could DoS the server here.
	 * TOTHINK: Maybe we want to set an alarm and time-limit
	 *  this attempt?
	 * TOTHINK: Process such connections or generally dialogs
	 *  in threads?
	 *
	 * In case of certificate expectation mismatches, it can
	 * also block the server until the caller closes the socket
	 * (we rely on the client continuing the crypto-dialog
	 * after receiving OK STARTTLS posted above) :-\
	 */
	upsdebugx(4, "%s: calling SSL_ForceHandshake()", __func__);
	status = SSL_ForceHandshake(client->ssl);
	if (status != SECSuccess) {
		PRErrorCode code = PR_GetError();
		if (code==SSL_ERROR_NO_CERTIFICATE) {
# ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
			if (certrequest == NETSSL_CERTREQ_REQUEST
			 || certrequest == NETSSL_CERTREQ_REQUIRE
			) {
				upslogx(LOG_ERR, "Client %s did not provide any certificate while we %s one.",
					client->addr,
					(certrequest == NETSSL_CERTREQ_REQUIRE ? "require" : "request")
					);
				nss_error("net_starttls / SSL_ForceHandshake");
				return;
			}
# endif
			upslogx(LOG_WARNING, "Client %s did not provide any certificate.",
				client->addr);
		} else {
			nss_error("net_starttls / SSL_ForceHandshake");
			/* TODO : Close the connection. */
			return;
		}
	}
	client->ssl_connected = 1;
# endif /* WITH_OPENSSL | WITH_NSS */
}

void ssl_init(void)
{
# ifdef WITH_NSS
	SECStatus status;
#  if defined(NSS_VMAJOR) && (NSS_VMAJOR > 3 || (NSS_VMAJOR == 3 && defined(NSS_VMINOR) && NSS_VMINOR >= 14))
	SSLVersionRange range;
#  endif
# endif /* WITH_NSS */

# ifdef WITH_OPENSSL
	if (!certfile) {
		upsdebugx(2, "%s: OPENSSL: no certfile", __func__);
		return;
	}
	check_perms(certfile);
# elif defined WITH_NSS
	if (!certpath) {
		upsdebugx(2, "%s: NSS: no certpath", __func__);
		return;
	}
	check_perms(certpath);
# else
	upsdebugx(2, "%s: Non-SSL build: should not have gotten here", __func__);
	return;
# endif

	if (!disable_weak_ssl)
		upslogx(LOG_WARNING, "Warning: DISABLE_WEAK_SSL is not enabled. Please consider enabling to improve network security.");

# ifdef WITH_OPENSSL

#  if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_load_error_strings();
	SSL_library_init();

	ssl_ctx = SSL_CTX_new(SSLv23_server_method());
#  else	/* newer OPENSSL_VERSION_NUMBER */
	ssl_ctx = SSL_CTX_new(TLS_server_method());
#  endif	/* OPENSSL_VERSION_NUMBER */

	if (!ssl_ctx) {
		ssl_debug();
		fatalx(EXIT_FAILURE, "SSL_CTX_new failed");
	}

	SSL_CTX_set_options(ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
#  if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* set minimum protocol TLSv1 */
	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
	if (disable_weak_ssl) {
#   if defined(SSL_OP_NO_TLSv1_2)
		SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
#   elif defined(SSL_OP_NO_TLSv1_1)
		SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_TLSv1);
#   endif
	}
#  else	/* newer OPENSSL_VERSION_NUMBER */
	if (SSL_CTX_set_min_proto_version(ssl_ctx, disable_weak_ssl ? TLS1_2_VERSION : TLS1_VERSION) != 1) {
		ssl_debug();
		fatalx(EXIT_FAILURE, "SSL_CTX_set_min_proto_version(TLS1_VERSION)");
	}
#  endif	/* OPENSSL_VERSION_NUMBER */

	if (certpasswd) {
#  if defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB
		/* Roughly OpenSSL 1.1.0+ or 1.0.2+ with patched distros */
		SSL_CTX_set_default_passwd_cb(ssl_ctx, openssl_password_callback);
#   if defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA
		SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, (void*)certpasswd);
#   endif	/* else callback uses global variable */
#  else	/* Not SSL_CTX_* methods */
		/* Per https://docs.openssl.org/3.5/man3/SSL_CTX_set_default_passwd_cb,
		 * the `SSL_CTX*` variants were added in 1.1.
		 * The SSL_set_default_passwd_cb() and SSL_set_default_passwd_cb_userdata()
		 * for `SSL*` argument were around since the turn of millennium, approx 0.9.6+
		 * per https://github.com/openssl/openssl/commit/66ebbb6a56bc1688fa37878e4feec985b0c260d7
		 *
		 * But to use those, we would need to get that SSL* (connection-oriented,
		 * maybe from socket FD or dummy SSL_new() with subsequent SSL_shutdown()
		 * and SSL_free(?); that would also unlock us using the ssl_error() elsewhere.
		 *
		 * Alternately load PEM "manually", see e.g. Apache httpd sources before 2015.
		 */
#   if defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_SET_DEFAULT_PASSWD_CB
		/* Theoretical solution - didn't find a build system where such methods
		 * would actually be available, so this could be tested and used */
		SSL	*ssl_tmp = SSL_new(ssl_ctx);
		/* OpenSSL 0.9.6+ at least? */
		SSL_set_default_passwd_cb(ssl_tmp, openssl_password_callback);
#    if defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB_USERDATA) && HAVE_SSL_SET_DEFAULT_PASSWD_CB_USERDATA
		SSL_set_default_passwd_cb_userdata(ssl_tmp, (void*)certpasswd);
#    endif
		SSL_free(ssl_tmp);

#   else	/* Not SSL_* methods either */

		upslogx(LOG_ERR, "Private key password support not implemented for OpenSSL < ~0.9.6..~1.1 yet");
		return;
#   endif
#  endif	/* ...SET_DEFAULT_PASSWD_CB */
	}	/* else: CERTIDENT did not pass a password, nothing to check */

	if (SSL_CTX_use_certificate_chain_file(ssl_ctx, certfile) != 1) {
		ssl_debug();
		fatalx(EXIT_FAILURE, "SSL_CTX_use_certificate_chain_file(%s) failed", certfile);
	}

	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, certfile, SSL_FILETYPE_PEM) != 1) {
		ssl_debug();
		fatalx(EXIT_FAILURE, "SSL_CTX_use_PrivateKey_file(%s) failed", certfile);
	}

	if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
		ssl_debug();
		fatalx(EXIT_FAILURE, "SSL_CTX_check_private_key(%s) failed", certfile);
	}

	if (certname && certname[0] != '\0') {
#  if (defined(HAVE_SSL_CTX_GET0_CERTIFICATE) && HAVE_SSL_CTX_GET0_CERTIFICATE) && (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST) && (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC) && (defined(HAVE_X509_NAME_ONELINE) && HAVE_X509_NAME_ONELINE)
		/* Roughly OpenSSL 1.0.2+ */
		X509	*x509 = SSL_CTX_get0_certificate(ssl_ctx);
		if (x509) {
			/* Check if certname matches the host (CN or SAN) */
			if (X509_check_host(x509, (const char *)certname, 0, 0, NULL) != 1
			 && X509_check_ip_asc(x509, (const char *)certname, 0) != 1
			) {
				char	*subject = X509_NAME_oneline(X509_get_subject_name(x509), NULL, 0);
				char	*subject_CN = (subject ? (char*)strstr(subject, "CN=") + 3 : NULL);
				size_t	certname_len = strlen(certname);

				upsdebugx(4, "%s: My certificate subject: '%s'; CN: '%s'; CERTIDENT: [%" PRIuSIZE "]'%s'",
					__func__, NUT_STRARG(subject), NUT_STRARG(subject_CN),
					certname_len, NUT_STRARG(certname));

				/* Check if certname matches the whole subject or just .../CN=.../ part as a string */
				if (!subject || !(
				    strcmp(subject, certname) == 0
				    || (subject_CN && !strncmp(subject_CN, certname, certname_len)
						&& (subject_CN[certname_len] == '\0'
							|| subject_CN[certname_len] == '/'
							|| subject_CN[certname_len] == ','
							|| (subject_CN[certname_len] == '\\' && subject_CN[certname_len + 1] == '/')) )
				)) {
					/* This way or that, the names differ */
					upslogx(LOG_ERR, "Certificate subject (%s) does not match CERTIDENT name (%s)",
						subject ? subject : "unknown", certname);
					if (subject) {
						OPENSSL_free(subject);
					}
					fatalx(EXIT_FAILURE, "Unexpected certificate provided");
				} else {
					upsdebugx(2, "Certificate subject verified against CERTIDENT subject name (%s)", certname);
				}
			} else {
				upsdebugx(2, "Certificate subject verified against CERTIDENT host name (%s)", certname);
			}
		}
#  else	/* Missing X509 methods wanted above */
		fatalx(EXIT_FAILURE, "CERTIDENT name verification is not supported in this OpenSSL build (too old)");
#  endif	/* Got ways to check CERTIDENT? */
	}/* else: CERTIDENT did not pass a name, nothing to check */

	upsdebugx(2, "%s: initialized with OpenSSL and certfile='%s'", __func__, certfile);

	if (SSL_CTX_set_cipher_list(ssl_ctx, "HIGH:@STRENGTH") != 1) {
		ssl_debug();
		fatalx(EXIT_FAILURE, "SSL_CTX_set_cipher_list failed");
	}

# ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
	if (certrequest < NETSSL_CERTREQ_NO || certrequest > NETSSL_CERTREQ_REQUIRE) {
		fatalx(EXIT_FAILURE, "Invalid certificate requirement");
	}

	if (certpath) {
		if (SSL_CTX_load_verify_locations(ssl_ctx, NULL, certpath) != 1) {
			ssl_debug();
			upsdebugx(1, "%s: Failed to load CA certificate(s) from directory %s", __func__, certpath);

			/* Can it be a specific PEM file? */
			if (SSL_CTX_load_verify_locations(ssl_ctx, certpath, NULL) != 1) {
				ssl_debug();
				upslogx(LOG_ERR, "Failed to load CA certificate(s) from directory or file %s", certpath);
				/* return? fatal? */
			} else {
				upsdebugx(1, "%s: ...but succeeded to load CA certificate(s) from file %s", __func__, certpath);
			}
		} else {
			upsdebugx(1, "%s: Succeeded to load CA certificate(s) from directory %s", __func__, certpath);
		}
	}

	if (certrequest == NETSSL_CERTREQ_REQUEST
	 || certrequest == NETSSL_CERTREQ_REQUIRE
	) {
		/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example */
		openssl_cert_verify_data_index = SSL_get_ex_new_index(0,
			"openssl_cert_verify_data index (server)",
			NULL, NULL, NULL);

		SSL_CTX_set_verify(ssl_ctx,
			(certrequest == NETSSL_CERTREQ_REQUIRE
				? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT
				: SSL_VERIFY_PEER),
			openssl_cert_verify_callback);

		/* Let the openssl_cert_verify_callback() catch any verify_depth
		 * error, so that we get an appropriate error in the logfile;
		 * see more around SSL_accept(). */
		SSL_CTX_set_verify_depth(ssl_ctx, verify_depth + 1);
	} else {
		SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
	}
# else
	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
# endif

	ssl_initialized = 1;

# elif defined(WITH_NSS)	/* not WITH_OPENSSL */

	if (!certname || certname[0]==0 ) {
		upslogx(LOG_ERR, "The SSL certificate name is not specified.");
		return;
	}

	PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

	PK11_SetPasswordFunc(nss_password_callback);

	/* Note: this call can generate memory leaks not resolvable
	 * by any release function.
	 * Probably NSS key module object allocation and
	 * probably NSS key db object allocation too. */
	status = NSS_Init(certpath);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL context");
		nss_error("ssl_init / NSS_Init");
		return;
	}

	status = NSS_SetDomesticPolicy();
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL policy");
		nss_error("ssl_init / NSS_SetDomesticPolicy");
		return;
	}

	/* Default server cache config */
	status = SSL_ConfigServerSessionIDCache(0, 0, 0, NULL);
	if (status != SECSuccess) {
		upslogx(LOG_ERR, "Can not initialize SSL server cache");
		nss_error("ssl_init / SSL_ConfigServerSessionIDCache");
		return;
	}

	if (!disable_weak_ssl) {
		status = SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_TRUE);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not enable SSLv3");
			nss_error("ssl_init / SSL_OptionSetDefault(SSL_ENABLE_SSL3)");
			return;
		}
		status = SSL_OptionSetDefault(SSL_ENABLE_TLS, PR_TRUE);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not enable TLSv1");
			nss_error("ssl_init / SSL_OptionSetDefault(SSL_ENABLE_TLS)");
			return;
		}
	} else {
#  if defined(NSS_VMAJOR) && (NSS_VMAJOR > 3 || (NSS_VMAJOR == 3 && defined(NSS_VMINOR) && NSS_VMINOR >= 14))
		status = SSL_VersionRangeGetSupported(ssl_variant_stream, &range);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not get versions supported");
			nss_error("ssl_init / SSL_VersionRangeGetSupported");
			return;
		}
		range.min = SSL_LIBRARY_VERSION_TLS_1_1;
#   ifdef SSL_LIBRARY_VERSION_TLS_1_2
		range.min = SSL_LIBRARY_VERSION_TLS_1_2;
#   endif
		status = SSL_VersionRangeSetDefault(ssl_variant_stream, &range);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not set versions supported");
			nss_error("ssl_init / SSL_VersionRangeSetDefault");
			return;
		}
		/* Disable old/weak ciphers */
		SSL_CipherPrefSetDefault(TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA, PR_FALSE);
		SSL_CipherPrefSetDefault(TLS_RSA_WITH_3DES_EDE_CBC_SHA, PR_FALSE);
		SSL_CipherPrefSetDefault(TLS_RSA_WITH_RC4_128_SHA, PR_FALSE);
		SSL_CipherPrefSetDefault(TLS_RSA_WITH_RC4_128_MD5, PR_FALSE);
#  else	/* older NSS_VMAJOR */
		status = SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_FALSE);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not disable SSLv3");
			nss_error("ssl_init / SSL_OptionSetDefault(SSL_DISABLE_SSL3)");
			return;
		}
		status = SSL_OptionSetDefault(SSL_ENABLE_TLS, PR_TRUE);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not enable TLSv1");
			nss_error("ssl_init / SSL_OptionSetDefault(SSL_ENABLE_TLS)");
			return;
		}
#  endif	/* NSS_VMAJOR */
	}

#ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
	if (certrequest < NETSSL_CERTREQ_NO		/* < 0 */
	 || certrequest > NETSSL_CERTREQ_REQUIRE	/* > 2 */
	) {
		upslogx(LOG_ERR, "Invalid certificate requirement");
		return;
	}

	if (certrequest == NETSSL_CERTREQ_REQUEST	/* 1 */
	 || certrequest == NETSSL_CERTREQ_REQUIRE	/* 2 */
	) {
		status = SSL_OptionSetDefault(SSL_REQUEST_CERTIFICATE, PR_TRUE);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not enable certificate request");
			nss_error("ssl_init / SSL_OptionSetDefault(SSL_REQUEST_CERTIFICATE)");
			return;
		}
	}

	if (certrequest == NETSSL_CERTREQ_REQUIRE) {	/* 2 */
		status = SSL_OptionSetDefault(SSL_REQUIRE_CERTIFICATE, PR_TRUE);
		if (status != SECSuccess) {
			upslogx(LOG_ERR, "Can not enable certificate requirement");
			nss_error("ssl_init / SSL_OptionSetDefault(SSL_REQUIRE_CERTIFICATE)");
			return;
		}
	}
#  endif	/* WITH_CLIENT_CERTIFICATE_VALIDATION */

	cert = PK11_FindCertFromNickname(certname, NULL);
	if (cert == NULL) {
		upslogx(LOG_ERR, "Can not find server certificate");
		nss_error("ssl_init / PK11_FindCertFromNickname");
		return;
	}

	privKey = PK11_FindKeyByAnyCert(cert, NULL);
	if (privKey == NULL) {
		upslogx(LOG_ERR, "Can not find private key associate to server certificate");
		nss_error("ssl_init / PK11_FindKeyByAnyCert");
		return;
	}

	upsdebugx(2, "%s: initialized with NSS and certpath='%s'", __func__, certpath);
	ssl_initialized = 1;
# else /* not (WITH_OPENSSL | WITH_NSS) */
	/* Looking at ifdefs, we should not get here. But just in case... */
	upslogx(LOG_ERR, "ssl_init called but no supported SSL backend wasn compiled in");
# endif /* WITH_OPENSSL | WITH_NSS */
}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC
#pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
ssize_t ssl_read(nut_ctype_t *client, char *buf, size_t buflen)
{
	ssize_t	ret = -1;

# ifdef WITH_OPENSSL
	int iret, ssl_err, ssl_retries = 0;
	/* Cap retries to avoid spinning forever on a broken socket.
	 * 250 * 20 ms = 5 s maximum wait, which is generous for a
	 * local handshake while being safe for CI timeouts.
	 */
	const int	SSL_IO_MAX_RETRIES = 250;
	fd_set	fds;
	struct timeval	tv;
# endif	/* WITH_OPENSSL */

	if (!client->ssl_connected) {
		return -1;
	}

	if (buflen == 0) {
		return 0;
	}

# ifdef WITH_OPENSSL
	/* SSL_* routines deal with int type for return and buflen
	 * We might need to window our I/O if we exceed 2GB (in
	 * 32-bit builds)... Not likely to exceed in 64-bit builds,
	 * but smaller systems with 16-bits might be endangered :)
	 */
	assert(buflen <= INT_MAX);

	while (ssl_retries < SSL_IO_MAX_RETRIES) {
		iret = SSL_read(client->ssl, buf, (int)buflen);

		if (iret > 0) {
			ret = (ssize_t)iret;
			break;
		}

		if (iret == 0) {
			/* Orderly shutdown or actual EOF */
			ret = 0;
			break;
		}

		ssl_err = SSL_get_error(client->ssl, iret);
		if (ssl_err == SSL_ERROR_WANT_READ
		 || ssl_err == SSL_ERROR_WANT_WRITE
		) {
			FD_ZERO(&fds);
			FD_SET(client->sock_fd, &fds);
			tv.tv_sec  = 0;
			tv.tv_usec = 20000;	/* 20 ms */

			if (select(client->sock_fd + 1,
				(ssl_err == SSL_ERROR_WANT_READ)  ? &fds : NULL,
				(ssl_err == SSL_ERROR_WANT_WRITE) ? &fds : NULL,
				NULL, &tv) < 0
			) {
				/* select failure is fatal enough to stop retrying */
				ssl_error(client->ssl, (ssize_t)iret);
				return -1;
			}
			ssl_retries++;
			continue;
		}

		/* Other errors are fatal */
		ssl_error(client->ssl, (ssize_t)iret);
		return -1;
	}

	if (ssl_retries >= SSL_IO_MAX_RETRIES) {
		upslogx(LOG_ERR, "%s: SSL_read timed out after %d retries", __func__, ssl_retries);
		return -1;
	}
# elif defined(WITH_NSS)	/* not WITH_OPENSSL */
	/* PR_* routines deal in PRInt32 type
	 * We might need to window our I/O if we exceed 2GB :) */
	assert(buflen <= PR_INT32_MAX);
	ret = PR_Read(client->ssl, buf, (PRInt32)buflen);
# endif	/* WITH_OPENSSL | WITH_NSS */

	if (ret < 1) {
		ssl_error(client->ssl, ret);
		return -1;
	}

	return ret;
}

ssize_t ssl_write(nut_ctype_t *client, const char *buf, size_t buflen)
{
	ssize_t	ret = -1;

# ifdef WITH_OPENSSL
	int	iret, ssl_err, ssl_retries = 0;
	/* Cap retries to avoid spinning forever on a broken socket.
	 * 250 * 20 ms = 5 s maximum wait, which is generous for a
	 * local handshake while being safe for CI timeouts.
	 */
	const int	SSL_IO_MAX_RETRIES = 250;
	fd_set	fds;
	struct timeval	tv;
# endif	/* WITH_OPENSSL */

	if (!client->ssl_connected) {
		return -1;
	}

	if (buflen == 0) {
		return 0;
	}

# ifdef WITH_OPENSSL
	/* SSL_* routines deal with int type for return and buflen
	 * We might need to window our I/O if we exceed 2GB (in
	 * 32-bit builds)... Not likely to exceed in 64-bit builds,
	 * but smaller systems with 16-bits might be endangered :)
	 */
	assert(buflen <= INT_MAX);

	while (ssl_retries < SSL_IO_MAX_RETRIES) {
		iret = SSL_write(client->ssl, buf, (int)buflen);

		if (iret > 0) {
			ret = (ssize_t)iret;
			break;
		}

		ssl_err = SSL_get_error(client->ssl, iret);
		if (ssl_err == SSL_ERROR_WANT_READ
		 || ssl_err == SSL_ERROR_WANT_WRITE
		) {
			FD_ZERO(&fds);
			FD_SET(client->sock_fd, &fds);
			tv.tv_sec  = 0;
			tv.tv_usec = 20000;	/* 20 ms */

			if (select(client->sock_fd + 1,
				(ssl_err == SSL_ERROR_WANT_READ)  ? &fds : NULL,
				(ssl_err == SSL_ERROR_WANT_WRITE) ? &fds : NULL,
				NULL, &tv) < 0
			) {
				/* select failure is fatal enough to stop retrying */
				ssl_error(client->ssl, (ssize_t)iret);
				return -1;
			}
			ssl_retries++;
			continue;
		}

		/* Other errors (including iret=0) are fatal */
		ssl_error(client->ssl, (ssize_t)iret);
		return -1;
	}

	if (ssl_retries >= SSL_IO_MAX_RETRIES) {
		upslogx(LOG_ERR, "%s: SSL_write timed out after %d retries", __func__, ssl_retries);
		return -1;
	}
# elif defined(WITH_NSS)	/* not WITH_OPENSSL */
	/* PR_* routines deal in PRInt32 type
	 * We might need to window our I/O if we exceed 2GB :) */
	assert(buflen <= PR_INT32_MAX);
	ret = PR_Write(client->ssl, buf, (PRInt32)buflen);
# endif	/* WITH_OPENSSL | WITH_NSS */

	upsdebugx(5, "ssl_write ret=%" PRIiSIZE, ret);

	return ret;
}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
#pragma GCC diagnostic pop
#endif

void ssl_finish(nut_ctype_t *client)
{
	if (client->ssl) {
# ifdef WITH_OPENSSL
		SSL_free(client->ssl);

		if (client->openssl_cert_verify_data.hostname_allocated
		 && client->openssl_cert_verify_data.hostname
		) {
			free((void *)client->openssl_cert_verify_data.hostname);
			client->openssl_cert_verify_data.hostname = NULL;
		}

		memset(&(client->openssl_cert_verify_data), 0, sizeof(client->openssl_cert_verify_data));
# elif defined(WITH_NSS)
		PR_Shutdown(client->ssl, PR_SHUTDOWN_BOTH);
		PR_Close(client->ssl);
# endif /* WITH_OPENSSL | WITH_NSS */
		client->ssl_connected = 0;
		client->ssl = NULL;
	}
}

void ssl_cleanup(void)
{
# ifdef WITH_OPENSSL
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
	}
# elif defined(WITH_NSS)	/* not WITH_OPENSSL */
	CERT_DestroyCertificate(cert);
	SECKEY_DestroyPrivateKey(privKey);
	NSS_Shutdown();
	PR_Cleanup();
	/* Called to release memory arena used by NSS/NSPR.
	 * Prevent to show all PL_ArenaAllocate mem alloc as leaks.
	 * https://developer.mozilla.org/en/NSS_Memory_allocation
	 */
	PL_ArenaFinish();
# endif	/* WITH_OPENSSL | WITH_NSS */
	ssl_initialized = 0;
	upsdebugx(2, "%s: SSL ability un-initialized", __func__);
}

#endif /* WITH_SSL */
