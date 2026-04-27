/* nutclient.cpp - nutclient C++ library implementation

    Copyright (C) 2012 Eaton

        Author: Emilien Kia <emilien.kia@gmail.com>

    Copyright (C) 2024-2026 NUT Community

        Author: Jim Klimov  <jimklimov+nut@gmail.com>

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

#include "config.h"
#include "nutclient.h"

#include <sstream>
#include <chrono>
#include <thread>

/* TODO: Make it a run-time option like upsdebugx(),
 * probably with a verbosity level variable in each
 * class instance */
#include <iostream>	/* std::cerr debugging */
#include <cstdint>
#include <cstdlib>
#include <stdlib.h>

#ifndef WIN32
# ifdef HAVE_PTHREAD
/* this include is needed on AIX to have errno stored in thread local storage */
#  include <pthread.h>
# endif
#endif	/* !WIN32 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
#  include <openssl/x509v3.h>
# endif

# ifdef WITH_NSS
#  include <prerror.h>
#  include <prinit.h>
#  include <pk11func.h>
#  include <prtypes.h>
#  include <ssl.h>
#  include <private/pprio.h>
# endif	/* WITH_NSS */
#endif	/* WITH_SSL_CXX */

/* Windows/Linux Socket compatibility layer: */
/* Thanks to Benjamin Roux (http://broux.developpez.com/articles/c/sockets/) */
#ifdef WIN32
#  include <winsock2.h>
#  define SOCK_OPT_CAST (char *)

/* equivalent of W32_NETWORK_CALL_OVERRIDE
 * invoked by wincompat.h in upsclient.c:
 */
static inline int sktconnect(int fh, struct sockaddr * name, int len)
{
	int ret = connect(fh,name,len);
	errno = WSAGetLastError();
	return ret;
}
static inline int sktread(int fh, void *buf, int size)
{
	int ret = recv(fh,(char*)buf,size,0);
	errno = WSAGetLastError();
	return ret;
}
static inline int sktwrite(int fh, const void*buf, int size)
{
	int ret = send(fh,(char*)buf,size,0);
	errno = WSAGetLastError();
	return ret;
}
static inline int sktclose(int fh)
{
	int ret = closesocket((SOCKET)fh);
	errno = WSAGetLastError();
	return ret;
}

#else /* not WIN32 */

#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h> /* close */
#  include <netdb.h> /* gethostbyname */
#  include <fcntl.h>
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET -1
#  endif
#  ifndef SOCKET_ERROR
#    define SOCKET_ERROR -1
#  endif
#  ifndef closesocket
#    define closesocket(s) close(s)
#  endif
#  define SOCK_OPT_CAST
   typedef int SOCKET;
   typedef struct sockaddr_in SOCKADDR_IN;
   typedef struct sockaddr SOCKADDR;
   typedef struct in_addr IN_ADDR;

#  define sktconnect(h,n,l)	::connect(h,n,l)
#  define sktread(h,b,s) 	::read(h,b,s)
#  define sktwrite(h,b,s) 	::write(h,b,s)
#  define sktclose(h)		::close(h)

/* WA for Solaris/i386 bug: non-blocking connect sets errno to ENOENT */
#  if (defined NUT_PLATFORM_SOLARIS)
#    define SOLARIS_i386_NBCONNECT_ENOENT(status) ( (!strcmp("i386", CPU_TYPE)) ? (ENOENT == (status)) : 0 )
#  else
#    define SOLARIS_i386_NBCONNECT_ENOENT(status) (0)
#  endif  /* end of Solaris/i386 WA for non-blocking connect */

/* WA for AIX bug: non-blocking connect sets errno to 0 */
#  if (defined NUT_PLATFORM_AIX)
#    define AIX_NBCONNECT_0(status) (0 == (status))
#  else
#    define AIX_NBCONNECT_0(status) (0)
#  endif  /* end of AIX WA for non-blocking connect */

#endif /* !WIN32 */
/* End of Windows/Linux Socket compatibility layer */


/* Include nut common utility functions or define simple ones if not */
#ifdef HAVE_NUTCOMMON
/* For C++ code below, we do not actually use the fallback time methods
 * (on mingw mostly), but in C++ context they happen to conflict with
 * time.h or ctime headers, while native-C does not. Just disable fallback:
 */
# ifndef HAVE_GMTIME_R
#  define HAVE_GMTIME_R 111
# endif
# ifndef HAVE_LOCALTIME_R
#  define HAVE_LOCALTIME_R 111
# endif
#include "common.h"
#else /* not HAVE_NUTCOMMON */
#include <stdlib.h>
#include <string.h>
static inline void *xmalloc(size_t size){return malloc(size);}
static inline void *xcalloc(size_t number, size_t size){return calloc(number, size);}
static inline void *xrealloc(void *ptr, size_t size){return realloc(ptr, size);}
static inline char *xstrdup(const char *string){return strdup(string);}
#endif /* not HAVE_NUTCOMMON */

#include "nut_stdint.h" /* PRIuMAX etc. */

/* To stay in line with modern C++, we use nullptr (not numeric NULL
 * or shim __null on some systems) which was defined after C++98.
 * The NUT C++ interface is intended for C++11 and newer, so we
 * quiesce these warnigns if possible.
 * An idea might be to detect if we do build with old C++ standard versions
 * and define a nullptr like https://stackoverflow.com/a/44517878/4715872
 * but again - currently we do not intend to support that officially.
 */
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT_PEDANTIC
#pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT
#pragma GCC diagnostic ignored "-Wc++98-compat"
#endif

namespace nut
{

SystemException::SystemException():
NutException(err())
{
}

std::string SystemException::err()
{
	if(errno==0)
		return "Undefined system error";
	else
	{
		std::stringstream str;
		str << "System error " << errno << ": " << strerror(errno);
		return str.str();
	}
}

/* Implemented out-of-line to avoid "Weak vtables" warnings and related overheads
 * But now with clang-9 C++11 linter (though not C++17) they complain with
 *   error: definition of implicit copy constructor for 'NutException'
 *          is deprecated because it has a user-declared destructor
 * This is fixed in header with declarations like:
 *   NutException(const NutException&) = default;
 * and assignment operator to accompany the copy constructor, per
 * https://lgtm.com/rules/2165180572/ like:
 *   NutException& operator=(NutException& rhs) = default;
 */
NutException::~NutException() noexcept {}
SystemException::~SystemException() noexcept {}
IOException::~IOException() noexcept {}
SSLException::~SSLException() noexcept {}
SSLException_OpenSSL::~SSLException_OpenSSL() noexcept {}
SSLException_NSS::~SSLException_NSS() noexcept {}
UnknownHostException::~UnknownHostException() noexcept {}
NotConnectedException::~NotConnectedException() noexcept {}
TimeoutException::~TimeoutException() noexcept {}


namespace internal
{

#ifdef WITH_OPENSSL
/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example */
typedef struct {
	int	verbose_mode;
	int	verify_depth;
	int	always_continue;
	
	/* In this context, hostname is by default a copy of Socket->_host.c_str(),
	 * which should be freed (we should set hostname_allocated!=0) */
	const char	*hostname;
	int	hostname_allocated;
} openssl_cert_verify_data_t;
#endif

/**
 * Internal socket wrapper.
 * Provides only client socket functions.
 *
 * Implemented as separate internal class to easily hide platform specificities.
 */
class Socket
{
public:
	Socket();
	~Socket();

	void connect(const std::string& host, uint16_t port);
	void disconnect();
	bool isConnected()const;
	void setDebugConnect(bool d);

	void setSSLConfig_OpenSSL(const SSLConfig_OpenSSL& config);
	void setSSLConfig_NSS(const SSLConfig_NSS& config);

	void startTLS();
	bool isSSL()const;

	void setTimeout(time_t timeout);
	bool hasTimeout()const{return _tv.tv_sec>=0;}

	size_t read(void* buf, size_t sz);
	size_t write(const void* buf, size_t sz);

	std::string read();
	void write(const std::string& str);


private:
	SOCKET _sock;
#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
	SSL* _ssl;
	openssl_cert_verify_data_t	openssl_cert_verify_data;
	int _verify_depth;
	static int _openssl_cert_verify_data_index;
	static SSL_CTX* _ssl_ctx;
# elif defined(WITH_NSS)
	PRFileDesc* _ssl;
# endif
#endif
	bool _debugConnect;
	struct timeval	_tv;
	std::string _buffer; /* Received buffer, string because data should be text only. */
	std::string _host;
	uint16_t _port;
	int _ssl_configured;
	int _forcessl;	/* Always known, so even non-SSL builds can fail if security is required */
#ifdef WITH_SSL_CXX
# if defined(WITH_OPENSSL)
	const SSLConfig_OpenSSL* _ssl_config;
# elif defined(WITH_NSS)
	const SSLConfig_NSS* _ssl_config;
# endif

# if defined(WITH_OPENSSL)
	/* Callbacks, syntax dictated by OpenSSL */
	static int openssl_password_callback(char *buf, int size, int rwflag, void *userdata);	/* pem_passwd_cb, 1.1.0+ */
	static int openssl_cert_verify_callback(int preverify_ok, X509_STORE_CTX *ctx);

	/* Helper for callback above */
	static int openssl_cert_verify_san_name(const char* label, X509* const cert, const char *hostname);
# elif defined(WITH_NSS)
	/* Callbacks, syntax dictated by NSS */
	static char *nss_password_callback(PK11SlotInfo *slot, PRBool retry, void *arg);
	static SECStatus AuthCertificate(CERTCertDBHandle *arg, PRFileDesc *fd,
		PRBool checksig, PRBool isServer);
	static SECStatus AuthCertificateDontVerify(CERTCertDBHandle *arg,
		PRFileDesc *fd, PRBool checksig, PRBool isServer);
	static SECStatus BadCertHandler(void *arg, PRFileDesc *fd);
	static SECStatus GetClientAuthData(void *arg, PRFileDesc *fd,
		CERTDistNames *caNames, CERTCertificate **pRetCert,
		SECKEYPrivateKey **pRetKey);
	static void HandshakeCallback(PRFileDesc *fd, void *arg);
# endif
#endif	/* WITH_SSL_CXX */
};

#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
SSL_CTX* Socket::_ssl_ctx = nullptr;
int Socket::_openssl_cert_verify_data_index = 0;

/* Adapted from https://stackoverflow.com/a/42477707 with references to
 * https://wiki.openssl.org/index.php/SSL/TLS_Client and further cURL,
 * and https://www.zedwood.com/article/c-openssl-parse-x509-certificate-pem */
/*static*/ int Socket::openssl_cert_verify_san_name(const char* label, X509* const cert, const char *hostname)
{
	int	san_seen = 0, ok = 0;
	GENERAL_NAMES*	names = nullptr;
	unsigned char*	utf8 = nullptr;

	do
	{
		int	i, count;

		if (!cert) break; /* failed */

		names = static_cast<GENERAL_NAMES *>(X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
		if (!names) break;

		/* OpenSSL macros may have an unreachable effect like this */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
#endif
		count = sk_GENERAL_NAME_num(names);
		if (!count) break; /* failed */

		for (i = 0; i < count; ++i) {
			GENERAL_NAME* entry = sk_GENERAL_NAME_value(names, i);
			if (!entry) continue;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic pop
#endif

			if (GEN_DNS == entry->type) {
				int	len1 = 0, len2 = -1;

				len1 = ASN1_STRING_to_UTF8(&utf8, entry->d.dNSName);
				if (utf8) {
					len2 = static_cast<int>(strlen(reinterpret_cast<const char*>(utf8)));
				}

				if (len1 != len2) {
					upsdebugx(5, "%s: %s: strlen and ASN1_STRING size "
						"do not match (embedded null?): %d vs %d",
						__func__, label, len2, len1);
				}

				/* If there's a problem with string lengths, then
				 * we skip the candidate and move on to the next.
				 * Another policy would be to fail, since it probably
				 * indicates the client is under attack by a corrupt
				 * server.
				 */
				if (utf8 && len1 && len2 && (len1 == len2)) {
					san_seen = 1;
					if (hostname && *hostname && !strcasecmp(reinterpret_cast<const char*>(utf8), hostname)) {
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
						__func__, label, NUT_STRARG(reinterpret_cast<const char*>(utf8)), len1, len2);
				}

				if (utf8) {
					OPENSSL_free(utf8);
					utf8 = nullptr;
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
					static_cast<const unsigned char *>(entry->d.iPAddress->data)
# endif
					;
				int	ip_addr_raw_len =
# ifdef HAVE_ASN1_STRING_LENGTH
					ASN1_STRING_length(entry->d.iPAddress)
# else
					static_cast<int>(entry->d.iPAddress->length)
# endif
					, j;

				memset(ip_addr_buf, 0, sizeof(ip_addr_buf));
				switch (ip_addr_raw_len) {
					case 4:
						for (j = 0; j < ip_addr_raw_len && p < pMax; j++) {
							p += snprintf(p,
								sizeof(ip_addr_buf) - static_cast<size_t>(p - ip_addr_buf) - 1,
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
								sizeof(ip_addr_buf) - static_cast<size_t>(p - ip_addr_buf) - 1,
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
				if (hostname && *hostname && !strcasecmp(static_cast<const char*>(ip_addr_buf), hostname)) {
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

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
	/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	if (!ok && hostname && *hostname && (0
#  if (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST)
	 || (X509_check_host(cert, static_cast<const char *>(hostname), 0, 0, nullptr) == 1)
#  endif
#  if (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC)
	 || (X509_check_ip_asc(cert, static_cast<const char *>(hostname), 0) == 1)
#  endif
	)) {
		upsdebugx(5, "%s: %s: MATCHED '%s' using OpenSSL-provided methods",
			__func__, label, hostname);
		ok = 1;
	}
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic pop
#endif

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

/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example */
/*static*/ int Socket::openssl_cert_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
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
	ssl = static_cast<SSL *>(X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
	openssl_cert_verify_data = static_cast<openssl_cert_verify_data_t *>(SSL_get_ex_data(ssl, _openssl_cert_verify_data_index));

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
		if (san_ok
# ifdef X509_V_ERR_HOSTNAME_MISMATCH
			&& err == X509_V_ERR_HOSTNAME_MISMATCH
# else
			&& err != 0
# endif
		) {
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

#  if (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_SET_DEFAULT_PASSWD_CB)
/* Note: availability of these methods seems to predate C++11, but still... */
/*static*/ int Socket::openssl_password_callback(char *buf, int size, int rwflag, void *userdata)	/* pem_passwd_cb, cca OpenSSL 1.1.0+ */
{
	/* See https://docs.openssl.org/1.0.2/man3/SSL_CTX_set_default_passwd_cb */
	/* is callback used for reading/decryption (rwflag=0) or writing/encryption (rwflag=1)? */
	NUT_UNUSED_VARIABLE(rwflag);
	/* "userdata" is generally the user-provided password, possibly cached
	 * from an earlier loop (e.g. to check interactively typing it twice,
	 * or to probe several items in a loop). For us, it should be this->_key_pass
	 * via SSL_CTX_set_default_passwd_cb_userdata(), but most programs out
	 * there do not have just one variable with one password to think about. */

	if (!buf || size < 1) {
		/* Can not even set buf[0] */
		return 0;
	}

	if (!userdata || !*(static_cast<char *>(userdata))) {
#  if (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB_USERDATA) && HAVE_SSL_SET_DEFAULT_PASSWD_CB_USERDATA)
		/* Use what we were told to use (or not), do not surprise
		 * anyone by some hard-coded fallback to key_pass from
		 * our _ssl_config here! */
		buf[0] = '\0';
		return 0;
#  else
		userdata = const_cast<void *>(static_cast<const void *>(this->_ssl_config ? this->_ssl_config->getKeyPass().c_str() : ""));
#  endif
	}

	if (strlen(static_cast<char *>(userdata)) >= static_cast<size_t>(size)) {
		/* Do not return truncated trash, just say we could not do it */
		return 0;
	}

	strncpy(buf, static_cast<char *>(userdata), static_cast<size_t>(size));
	buf[size - 1] = '\0';
	return static_cast<int>(strlen(buf));
}
#  endif	/* ...SET_DEFAULT_PASSWD_CB */
# endif	/* WITH_OPENSSL */

# ifdef WITH_NSS
/** Detail the currently raised NSS error code if possible, and debug-log
 *  it with caller-provided text (typically the calling function name). */
static void nss_error(const char* text)
{
	std::string	err_name_buf;
	PRErrorCode	err_num = PR_GetError();
	const char	*err_name = PR_ErrorToName(err_num);
	PRInt32	err_len = PR_GetErrorTextLength();

	if (err_name) {
		err_name_buf  = " (";
		err_name_buf += err_name;
		err_name_buf += ")";
	}

	if (err_len > 0) {
		char	*buffer = static_cast<char *>(calloc(static_cast<size_t>(err_len) + 1, sizeof(char)));
		if (buffer) {
			PR_GetErrorText(buffer);
			std::cerr << "nss_error "
				<< static_cast<long>(err_num)
				<< err_name_buf
				<< " in " << text
				<< " : "
				<< buffer
				<< std::endl;
			free(buffer);
		} else {
			std::cerr << "nss_error "
				<< static_cast<long>(err_num)
				<< err_name_buf
				<< " in " << text
				<< " : "
				<< "Failed to allocate internal error buffer "
				<< "for detailed error text, needs "
				<< static_cast<long>(err_len) << " bytes"
				<< std::endl;
		}
	} else {
		/* The code above may be obsolete or not ubiquitous, try another way */
		const char	*err_text = PR_ErrorToString(err_num, PR_LANGUAGE_I_DEFAULT);
		if (err_text && *err_text) {
			std::cerr << "nss_error "
				<< static_cast<long>(err_num)
				<< err_name_buf
				<< " in " << text
				<< " : "
				<< err_text
				<< std::endl;
		} else {
			std::cerr << "nss_error "
				<< static_cast<long>(err_num)
				<< err_name_buf
				<< " in " << text
				<< std::endl;
		}
	}
}

/*static*/ char *Socket::nss_password_callback(PK11SlotInfo *slot, PRBool retry,
		void *arg)
{
	NUT_UNUSED_VARIABLE(slot);
	NUT_UNUSED_VARIABLE(retry);
	Socket* sock = static_cast<Socket*>(arg);

	if (!sock || !sock->_ssl_config || sock->_ssl_config->getCertStorePass().empty()) {
		return nullptr;
	}

	return PL_strdup(sock->_ssl_config->getCertStorePass().c_str());
}

/*static*/ SECStatus Socket::AuthCertificate(CERTCertDBHandle *arg, PRFileDesc *fd,
	PRBool checksig, PRBool isServer)
{
	Socket	*sock = static_cast<Socket*>(SSL_RevealPinArg(fd));
	SECStatus	status = SSL_AuthCertificate(arg, fd, checksig, isServer);

	if (sock && sock->_debugConnect) {
		std::cerr << "Intend to authenticate server "
            << (sock->_host.empty() ? "<unnamed>" : sock->_host)
            << " : " << (status==SECSuccess ? "SUCCESS" : "FAILED")
            << std::endl;
	}

	if (status != SECSuccess) {
		nss_error((std::string("SSL_AuthCertificate") + (isServer ? "(server)" : "(client)")).c_str());
	}

	return status;
}

/*static*/ SECStatus Socket::AuthCertificateDontVerify(CERTCertDBHandle *arg, PRFileDesc *fd,
	PRBool checksig, PRBool isServer)
{
	NUT_UNUSED_VARIABLE(arg);
	NUT_UNUSED_VARIABLE(fd);
	NUT_UNUSED_VARIABLE(checksig);
	NUT_UNUSED_VARIABLE(isServer);

	return SECSuccess;
}

/*static*/ SECStatus Socket::BadCertHandler(void *arg, PRFileDesc *fd)
{
	Socket* sock = static_cast<Socket*>(arg);
	NUT_UNUSED_VARIABLE(fd);

	if (sock && sock->_ssl_config && sock->_ssl_config->getCertVerify() == 0) {
		return SECSuccess;
	}

	return SECFailure;
}

/*static*/ SECStatus Socket::GetClientAuthData(void *arg, PRFileDesc *fd,
	CERTDistNames *caNames, CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey)
{
	Socket* sock = static_cast<Socket*>(arg);
	CERTCertificate *cert;
	SECKEYPrivateKey *privKey;
	SECStatus status = NSS_GetClientAuthData(arg, fd, caNames, pRetCert, pRetKey);

	if (status == SECFailure) {
		if (sock && sock->_ssl_config && !sock->_ssl_config->getCertIdentName().empty()) {
			cert = PK11_FindCertFromNickname(sock->_ssl_config->getCertIdentName().c_str(), nullptr);
			if (cert == nullptr) {
				nss_error("GetClientAuthData / PK11_FindCertFromNickname");
			} else {
				privKey = PK11_FindKeyByAnyCert(cert, nullptr);
				if (privKey == nullptr) {
					nss_error("GetClientAuthData / PK11_FindKeyByAnyCert");
					CERT_DestroyCertificate(cert);
				} else {
					*pRetCert = cert;
					*pRetKey = privKey;
					status = SECSuccess;
				}
			}
		}
	}

	return status;
}

/*static*/ void Socket::HandshakeCallback(PRFileDesc *fd, void *arg)
{
	NUT_UNUSED_VARIABLE(fd);
	Socket* sock = static_cast<Socket*>(arg);

	if (sock && sock->_debugConnect) {
		std::cerr << "SSL handshake done successfully with server " << sock->_host << std::endl;
	}
}
# endif	/* WITH_NSS */
#endif	/* WITH_SSL_CXX */

Socket::Socket():
	_sock(INVALID_SOCKET),
#ifdef WITH_SSL_CXX
# if defined(WITH_OPENSSL) || defined(WITH_NSS)
	_ssl(nullptr),
# endif
# if defined(WITH_OPENSSL)
	_verify_depth(9),	/* openssl default */
# endif
#endif
	_debugConnect(false),
	_tv(),
	_port(NUT_PORT),
	_forcessl(0)
#ifdef WITH_SSL_CXX
# if defined(WITH_OPENSSL) || defined(WITH_NSS)
	,_ssl_config(nullptr)
# endif
#endif
{
	/* Initialize timeout from envvar NUT_DEFAULT_CONNECT_TIMEOUT if present */
	char	*s = getenv("NUT_DEFAULT_CONNECT_TIMEOUT");

	_tv.tv_sec = -1;
	if (s) {
		long	l = atol(s);
		if (l > 0)
			_tv.tv_sec = l;
	}

	_tv.tv_usec = 0;

#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
	memset(&openssl_cert_verify_data, 0, sizeof(openssl_cert_verify_data));
# endif	/* WITH_OPENSSL */
#endif	/* WITH_SSL_CXX */
}

Socket::~Socket()
{
	disconnect();
}

void Socket::setTimeout(time_t timeout)
{
	_tv.tv_sec = timeout;
}

void Socket::setDebugConnect(bool d)
{
	_debugConnect = d;
}

void Socket::connect(const std::string& host, uint16_t port)
{
	int	sock_fd;
	struct addrinfo	hints, *res, *ai;
	char			sport[NI_MAXSERV];
	int			v;
	fd_set 			wfds;
	int			error;
	socklen_t		error_size;

	_host = host;
	_port = port;

#ifndef WIN32
	long			fd_flags;
#else	/* WIN32 */
	HANDLE event = NULL;	/* TOTHINK: nullptr for C++? */
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

	_sock = INVALID_SOCKET;

	if (host.empty()) {
		if (_debugConnect) std::cerr <<
			"[D2] Socket::connect(): host.empty()" <<
			std::endl << std::flush;
		throw nut::UnknownHostException();
	}

	snprintf(sport, sizeof(sport), "%" PRIuMAX, static_cast<uintmax_t>(port));

	memset(&hints, 0, sizeof(hints));
	/* TODO? Port IPv4 vs. IPv6 detail from upsclient.c */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (_debugConnect) std::cerr <<
		"[D2] Socket::connect(): getaddrinfo(" <<
		host << ", " <<
		sport << ", " <<
		"...)" << std::endl << std::flush;

	while ((v = getaddrinfo(host.c_str(), sport, &hints, &res)) != 0) {
		switch (v)
		{
		case EAI_AGAIN:
			continue;
		case EAI_NONAME:
			if (_debugConnect) std::cerr <<
				"[D2] Socket::connect(): " <<
				"connect not successful: " <<
				"UnknownHostException" <<
				std::endl << std::flush;
			throw nut::UnknownHostException();
		case EAI_MEMORY:
			if (_debugConnect) std::cerr <<
				"[D2] Socket::connect(): " <<
				"connect not successful: " <<
				"Out of memory" <<
				std::endl << std::flush;
			throw nut::NutException("Out of memory");
#ifndef WIN32
		case EAI_SYSTEM:
#else	/* WIN32 */
		case WSANO_RECOVERY:
#endif	/* WIN32 */
			if (_debugConnect) std::cerr <<
				"[D2] Socket::connect(): " <<
				"connect not successful: " <<
				"SystemException" <<
				std::endl << std::flush;
			throw nut::SystemException();
		default:
			if (_debugConnect) std::cerr <<
				"[D2] Socket::connect(): " <<
				"connect not successful: " <<
				"Unknown error" <<
				std::endl << std::flush;
			throw nut::NutException("Unknown error");
		}
	}

	for (ai = res; ai != nullptr; ai = ai->ai_next) {

		if (_debugConnect) std::cerr <<
			"[D2] Socket::connect(): socket(" <<
			ai->ai_family << ", " <<
			ai->ai_socktype << ", " <<
			ai->ai_protocol << ")" <<
			std::endl << std::flush;

		sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (_debugConnect) std::cerr <<
			"[D2] Socket::connect(): socket(): " <<
			"sock_fd = " << sock_fd <<
			std::endl << std::flush;

		if (sock_fd < 0) {
			switch (errno)
			{
			case EAFNOSUPPORT:
			case EINVAL:
				break;
			default:
				if (_debugConnect) std::cerr <<
					"[D2] Socket::connect(): " <<
					"connect not successful: " <<
					"SystemException" <<
					std::endl << std::flush;
				throw nut::SystemException();
			}
			continue;
		}

		/* non blocking connect */
		if (hasTimeout()) {
#ifndef WIN32
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags |= O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
#else	/* WIN32 */
			/* TOTHINK: nullptr instead of NULL for C++? */
			event = CreateEvent(NULL, /* Security */
					FALSE, /* auto-reset */
					FALSE, /* initial state */
					NULL); /* no name */

			/* Associate socket event to the socket via its Event object */
			WSAEventSelect( sock_fd, event, FD_CONNECT );
			CloseHandle(event);
#endif	/* WIN32 */
		}

		if (_debugConnect) std::cerr <<
			"[D2] Socket::connect(): sktconnect(" <<
			sock_fd << ", " <<
			ai->ai_addr << ", " <<
			ai->ai_addrlen << ")" <<
			std::endl << std::flush;

		while ((v = sktconnect(sock_fd, ai->ai_addr, ai->ai_addrlen)) < 0) {
			if (_debugConnect) std::cerr <<
				"[D2] Socket::connect(): " <<
				"sktconnect() < 0" <<
				"; errno = " << errno <<
				"; v = " << v <<
				std::endl << std::flush;

#ifndef WIN32
			if(errno == EINPROGRESS || SOLARIS_i386_NBCONNECT_ENOENT(errno) || AIX_NBCONNECT_0(errno)) {
#else	/* WIN32 */
			if(errno == WSAEWOULDBLOCK) {
#endif	/* WIN32 */
				FD_ZERO(&wfds);
				FD_SET(sock_fd, &wfds);
				select(sock_fd+1, nullptr, &wfds, nullptr,
					hasTimeout() ? &_tv : nullptr);
				if (FD_ISSET(sock_fd, &wfds)) {
					error_size = sizeof(error);
					getsockopt(sock_fd, SOL_SOCKET, SO_ERROR,
							SOCK_OPT_CAST &error, &error_size);
					if( error == 0) {
						/* connect successful */
						if (_debugConnect) std::cerr <<
							"[D2] Socket::connect(): " <<
							"connect-select successful" <<
							std::endl << std::flush;
						v = 0;
						break;
					}
					errno = error;
					if (_debugConnect) std::cerr <<
						"[D2] Socket::connect(): " <<
						"connect-select not successful: " <<
						"errno = " << errno <<
						std::endl << std::flush;
				}
				else {
					/* Timeout */
					v = -1;
					if (_debugConnect) std::cerr <<
						"[D2] Socket::connect(): " <<
						"connect-select not successful: timeout" <<
						std::endl << std::flush;
					break;
				}
			} else {
				/* WIN32: errno=10061 is actively refusing connection */
				if (_debugConnect) std::cerr <<
					"[D2] Socket::connect(): " <<
					"connect not successful: " <<
					"errno = " << errno <<
					std::endl << std::flush;
			}

			switch (errno)
			{
			case EAFNOSUPPORT:
				break;
			case EINTR:
			case EAGAIN:
				continue;
			default:
//				ups->upserror = UPSCLI_ERR_CONNFAILURE;
//				ups->syserrno = errno;
				break;
			}
			break;
		}

		if (v < 0) {
			if (_debugConnect) std::cerr <<
				"[D2] Socket::connect(): " <<
				"sktconnect() remains < 0 => sktclose()" <<
				std::endl << std::flush;
			sktclose(sock_fd);
			continue;
		}
		if (_debugConnect) std::cerr <<
			"[D2] Socket::connect(): " <<
			"sktconnect() > 0, looks promising" <<
			std::endl << std::flush;

		/* switch back to blocking operation */
		if (hasTimeout()) {
#ifndef WIN32
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags &= ~O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
#else	/* WIN32 */
			argp = 0;
			ioctlsocket(sock_fd, FIONBIO, &argp);
#endif	/* WIN32 */
		}

		if (_debugConnect) std::cerr <<
			"[D2] Socket::connect(): " <<
			"saving sock_fd = " << sock_fd <<
			std::endl << std::flush;
		_sock = sock_fd;
//		ups->upserror = 0;
//		ups->syserrno = 0;
		break;
	}

	freeaddrinfo(res);

#ifndef WIN32
	if (_sock < 0) {
#else	/* WIN32 */
	if (_sock == INVALID_SOCKET) {
		/* In tracing one may see 18446744073709551615 = "-1" after
		 * conversion from 'long long unsigned int' to 'int'
		 * 64-bit WINSOCK API with UINT_PTR , see gory details at e.g.
		 * https://github.com/openssl/openssl/issues/7282#issuecomment-430633656
		 */
#endif	/* WIN32 */
		if (_debugConnect) std::cerr <<
			"[D2] Socket::connect(): " <<
			"invalid _sock = " << _sock <<
			std::endl << std::flush;
		throw nut::IOException("Cannot connect to host");
	}

#ifdef OLD
	struct hostent *hostinfo = nullptr;
	SOCKADDR_IN sin = { 0 };
	hostinfo = ::gethostbyname(host.c_str());
	if(hostinfo == nullptr) /* Host doesnt exist */
	{
		throw nut::UnknownHostException();
	}

	// Create socket
	_sock = ::socket(PF_INET, SOCK_STREAM, 0);
	if(_sock == INVALID_SOCKET)
	{
		throw nut::IOException("Cannot create socket");
	}

	// Connect
	sin.sin_addr = *(IN_ADDR *) hostinfo->h_addr;
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	if(sktconnect(_sock,(SOCKADDR *) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		_sock = INVALID_SOCKET;
		throw nut::IOException("Cannot connect to host");
	}
#endif // OLD
}

void Socket::disconnect()
{
#ifdef WITH_SSL_CXX
# if defined(WITH_OPENSSL) || defined(WITH_NSS)
	if (_ssl) {
#  ifdef WITH_OPENSSL
		SSL_shutdown(_ssl);
		SSL_free(_ssl);
#  elif defined(WITH_NSS)
		PR_Close(_ssl);
#  endif
		_ssl = nullptr;
	}

#  ifdef WITH_OPENSSL
	if (openssl_cert_verify_data.hostname_allocated
	 && openssl_cert_verify_data.hostname
	) {
		free(const_cast<void *>(static_cast<const void *>((openssl_cert_verify_data.hostname))));
		openssl_cert_verify_data.hostname = nullptr;
	}

	memset(&openssl_cert_verify_data, 0, sizeof(openssl_cert_verify_data));
#  endif
# endif
#endif	/* WITH_SSL_CXX */

	if(_sock != INVALID_SOCKET)
	{
		::closesocket(_sock);
		_sock = INVALID_SOCKET;
	}
	_buffer.clear();
}

bool Socket::isSSL()const
{
#if defined (WITH_SSL_CXX) && (defined(WITH_OPENSSL) || defined(WITH_NSS))
	return _ssl != nullptr;
#else
	return false;
#endif
}

void Socket::setSSLConfig_OpenSSL(const SSLConfig_OpenSSL& config)
{
	_forcessl = config.getForceSsl();

	/* CERTHOST can override the global setting for this connection */
	const SSLConfig_CERTHOST *ch = config.getCertHostByAddrOrSubj(_host);
	if (ch) {
		if (ch->getForceSsl() != -1) {
			_forcessl = ch->getForceSsl();
		}
	}

#if defined(WITH_SSL_CXX) && defined(WITH_OPENSSL)
	_ssl_config = &config;

	if (_debugConnect) std::cerr <<
		"[D2] config OpenSSL" << std::endl;

	/* TOTHINK: Add any config checks for a viable run? (e.g. have cert but no key) */
	_ssl_configured |= UPSCLI_SSL_CAPS_OPENSSL;

	/* Got something to check, and ability to do so */
	if (!(_ssl_config->getKeyPass().empty() || _ssl_config->getKeyFile().empty())
	 && !(_ssl_config->getCertFile().empty() || _ssl_config->getCertIdentName().empty())
	) {
# if (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_SET_DEFAULT_PASSWD_CB)
		_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_PASS;
# endif
# if (defined(HAVE_SSL_CTX_GET0_CERTIFICATE) && HAVE_SSL_CTX_GET0_CERTIFICATE) && (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST) && (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC) && (defined(HAVE_X509_NAME_ONELINE) && HAVE_X509_NAME_ONELINE)
		_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_NAME;
# endif
	}

#else
	if (_debugConnect) std::cerr <<
		"[D2] NOT config OpenSSL" << std::endl;

	_ssl_configured &= ~UPSCLI_SSL_CAPS_OPENSSL;
	_ssl_configured &= ~UPSCLI_SSL_CAPS_CERTIDENT_PASS;
	_ssl_configured &= ~UPSCLI_SSL_CAPS_CERTIDENT_NAME;
#endif
}

void Socket::setSSLConfig_NSS(const SSLConfig_NSS& config)
{
	_forcessl = config.getForceSsl();

	/* CERTHOST can override the global setting for this connection */
	const SSLConfig_CERTHOST *ch = config.getCertHostByAddrOrSubj(_host);
	if (ch) {
		if (ch->getForceSsl() != -1) {
			_forcessl = ch->getForceSsl();
		}
	}

#if defined(WITH_SSL_CXX) && defined(WITH_NSS)
	_ssl_config = &config;

	if (_debugConnect) std::cerr <<
		"[D2] config NSS" << std::endl;

	/* TOTHINK: Add any config checks for a viable run? (e.g. have cert but no key) */
	_ssl_configured |= UPSCLI_SSL_CAPS_NSS;

	/* Got something to check, and ability to do so */
	if (!(_ssl_config->getCertStorePass().empty() || _ssl_config->getCertStorePath().empty())
	 && !(_ssl_config->getCertIdentName().empty())
	) {
		_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_PASS;
		_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_NAME;
	}
#else
	if (_debugConnect) std::cerr <<
		"[D2] NOT config NSS" << std::endl;

	_ssl_configured &= ~UPSCLI_SSL_CAPS_NSS;
	_ssl_configured &= ~UPSCLI_SSL_CAPS_CERTIDENT_PASS;
	_ssl_configured &= ~UPSCLI_SSL_CAPS_CERTIDENT_NAME;
#endif
}

void Socket::startTLS()
{
	if (!isConnected()) {
		throw nut::NotConnectedException();
	}

#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
	if (!(_ssl_config && _ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) {
		if (_debugConnect) std::cerr <<
			"[D2] Socket::startTLS(): Not configured for OpenSSL" <<
			" and forcessl is " << _forcessl <<
			std::endl << std::flush;
		if (_forcessl) {
			disconnect();
			throw nut::SSLException_OpenSSL("Not configured for OpenSSL");
		}
		return;
	}
# elif defined(WITH_NSS)
	if (!(_ssl_config && _ssl_configured & UPSCLI_SSL_CAPS_NSS)) {
		if (_debugConnect) std::cerr <<
			"[D2] Socket::startTLS(): Not configured for NSS" <<
			" and forcessl is " << _forcessl <<
			std::endl << std::flush;
		if (_forcessl) {
			disconnect();
			throw nut::SSLException_NSS("Not configured for NSS");
		}
		return;
	}
# endif	/* WITH_OPENSSL || WITH_NSS */

# if defined(WITH_OPENSSL) || defined(WITH_NSS)
	write("STARTTLS");
	std::string res = read();
	if (res.substr(0, 11) != "OK STARTTLS") {
		if (_forcessl) {
			disconnect();
			throw nut::SSLException("STARTTLS failed: " + res);
		}
		return;
	}
# endif	/* WITH_OPENSSL || WITH_NSS */

# ifdef WITH_OPENSSL
	if (!_ssl_ctx) {
#  if OPENSSL_VERSION_NUMBER < 0x10100000L
		SSL_load_error_strings();
		SSL_library_init();
		_ssl_ctx = SSL_CTX_new(SSLv23_client_method());
#  else
		_ssl_ctx = SSL_CTX_new(TLS_client_method());
#  endif
		if (!_ssl_ctx) {
			throw nut::SSLException_OpenSSL("Cannot create SSL context");
		}
	}

	const std::string& ca_file_str = _ssl_config->getCAFile();
	const std::string& ca_path_str = _ssl_config->getCAPath();
	const char *ca_file = ca_file_str.empty() ? nullptr : ca_file_str.c_str();
	const char *ca_path = ca_path_str.empty() ? nullptr : ca_path_str.c_str();

	if (ca_file || ca_path) {
		if (SSL_CTX_load_verify_locations(_ssl_ctx, ca_file, ca_path) != 1) {
			if (!(!ca_file && ca_path
				/* Retry in case CERTPATH actually pointed to a big PEM file */
				&& SSL_CTX_load_verify_locations(_ssl_ctx, ca_path, nullptr) == 1)
			) {
				throw nut::SSLException_OpenSSL("Failed to load CA verify locations");
			}
		}
	}

	int certverify = _ssl_config->getCertVerify();
	/* CERTHOST can override the global setting for this connection */
	const SSLConfig_CERTHOST *ch = _ssl_config->getCertHostByAddrOrSubj(_host);
	if (ch) {
		if (ch->getCertVerify() != -1) {
			certverify = ch->getCertVerify();
		}
	}

	if (certverify != -1) {
		/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example */
		_openssl_cert_verify_data_index = SSL_get_ex_new_index(0,
			const_cast<void*>(static_cast<const void *>("openssl_cert_verify_data index (client)")),
			nullptr, nullptr, nullptr);

		SSL_CTX_set_verify(_ssl_ctx,
			certverify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
			openssl_cert_verify_callback);

		/* Let the openssl_cert_verify_callback() catch any verify_depth
		 * error, so that we get an appropriate error in the logfile;
		 * see more around SSL_connect(). */
		SSL_CTX_set_verify_depth(_ssl_ctx, _verify_depth + 1);
	}

	const std::string& cert_file_str = _ssl_config->getCertFile();
	const char *cert_file = cert_file_str.empty() ? nullptr : cert_file_str.c_str();
	if (cert_file) {
		if (SSL_CTX_use_certificate_chain_file(_ssl_ctx, cert_file) != 1) {
			throw nut::SSLException_OpenSSL("Failed to load client certificate file");
		}

		const std::string& key_pass_str = _ssl_config->getKeyPass();
		const char *key_pass = key_pass_str.empty() ? nullptr : key_pass_str.c_str();
		if (key_pass) {
			/* Note: availability of these methods seems to predate C++11, but still... */
#  if defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB
			/* Roughly OpenSSL 1.1.0+ or 1.0.2+ with patched distros
			 * https://docs.openssl.org/3.5/man3/SSL_CTX_set_default_passwd_cb/#return-values
			 */
			/* 1. Set the callback function */
			SSL_CTX_set_default_passwd_cb(_ssl_ctx, openssl_password_callback);
#   if defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB_USERDATA
			/* 2. Set the userdata to the password string */
			SSL_CTX_set_default_passwd_cb_userdata(_ssl_ctx, const_cast<void *>(static_cast<const void *>(key_pass)));
#   endif	/* else callback uses class instance field */
#  else	/* Not SSL_CTX_* methods */
			/* Per https://docs.openssl.org/3.5/man3/SSL_CTX_set_default_passwd_cb,
			 * the `SSL_CTX*` variants were added in 1.1.
			 * The SSL_set_default_passwd_cb() and SSL_set_default_passwd_cb_userdata()
			 * for `SSL*` argument were around since the turn of millennium, approx 0.9.6+
			 * per https://github.com/openssl/openssl/commit/66ebbb6a56bc1688fa37878e4feec985b0c260d7
			 *
			 * But to use those, we would need to get that SSL* (maybe from socket FD?);
			 * that would also unlock us using the ssl_error() elsewhere.
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
			SSL_set_default_passwd_cb_userdata(ssl_tmp, const_cast<void *>(static_cast<const void *>(key_pass)));
#    endif
			SSL_free(ssl_tmp);

#   else	/* Not SSL_* methods either */

			throw nut::SSLException_OpenSSL("Private key password support not implemented for OpenSSL < 1.1 yet");
#   endif
#  endif	/* ...SET_DEFAULT_PASSWD_CB */
		}

		const std::string& key_file_str = _ssl_config->getKeyFile();
		const char *key_file = key_file_str.empty() ? cert_file : key_file_str.c_str();
		if (SSL_CTX_use_PrivateKey_file(_ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1) {
			throw nut::SSLException_OpenSSL("Failed to load client private key file");
		}

		const std::string& certident_name_str = _ssl_config->getCertIdentName();
		const char *certident_name = certident_name_str.empty() ? nullptr : certident_name_str.c_str();
		if (certident_name) {
#  if (defined(HAVE_SSL_CTX_GET0_CERTIFICATE) && HAVE_SSL_CTX_GET0_CERTIFICATE) && (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST) && (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC) && (defined(HAVE_X509_NAME_ONELINE) && HAVE_X509_NAME_ONELINE)
			/* Roughly OpenSSL 1.0.2+ */
			X509	*x509 = SSL_CTX_get0_certificate(_ssl_ctx);
			if (x509) {
				/* Check if certident_name matches the host (CN or SAN) */
				if (X509_check_host(x509, certident_name, 0, 0, nullptr) != 1
				 && X509_check_ip_asc(x509, certident_name, 0) != 1
				) {
					char	*subject = X509_NAME_oneline(X509_get_subject_name(x509), nullptr, 0);
					char	*subject_CN = (subject ? static_cast<char*>(strstr(subject, "CN=")) + 3 : nullptr);
					size_t	certident_len = certident_name_str.length();

					if (_debugConnect) std::cerr <<
						"[D4] Socket::startTLS(): My certificate subject: '" << (subject ? subject : "unknown") <<
						"'; CN: '" << (subject_CN ? subject_CN : "unknown") <<
						"'; CERTIDENT: [" << certident_len << "]'" << certident_name_str << "'" <<
						std::endl << std::flush;

					/* Check if _certident_name matches the whole subject or just .../CN=.../ part as a string */
					if (!subject || !(
						strcmp(subject, certident_name) == 0
						|| (subject_CN && !strncmp(subject_CN, certident_name, certident_len)
							&& (subject_CN[certident_len] == '\0'
								|| subject_CN[certident_len] == '/'
								|| subject_CN[certident_len] == ','
								|| (subject_CN[certident_len] == '\\' && subject_CN[certident_len + 1] == '/')) )
					)) {
						/* This way or that, the names differ */
						std::string err = "Certificate subject (" + std::string(subject ? subject : "unknown") + ") does not match CERTIDENT name (" + certident_name_str + ")";
						if (subject) {
							OPENSSL_free(subject);
						}
						throw nut::SSLException_OpenSSL(err);
					} else {
						if (_debugConnect) std::cerr <<
							"[D2] Socket::startTLS(): Certificate subject verified against CERTIDENT subject name (" << certident_name_str << ")" <<
							std::endl << std::flush;
					}
					if (subject) {
						OPENSSL_free(subject);
					}
				} else {
					if (_debugConnect) std::cerr <<
						"[D2] Socket::startTLS(): Certificate subject verified against CERTIDENT host name (" << certident_name_str << ")" <<
						std::endl << std::flush;
				}
			}
#  else	/* Missing X509 methods wanted above */
			throw nut::SSLException_OpenSSL("Can not verify CERTIDENT '" + certident_name_str + "': not supported in this OpenSSL build (too old)");
#  endif	/* Got ways to check CERTIDENT? */
		}
	} else {
		const std::string& certident_name_str = _ssl_config->getCertIdentName();
		if (!certident_name_str.empty()) {
			throw nut::SSLException_OpenSSL("Can not verify CERTIDENT '" + certident_name_str + "': no cert_file was provided");
		}
	}

	_ssl = SSL_new(_ssl_ctx);
	if (!_ssl) {
		throw nut::SSLException_OpenSSL("Cannot create SSL object");
	}
	if (SSL_set_fd(_ssl, static_cast<int>(_sock)) != 1) {
		throw nut::SSLException_OpenSSL("Can not bind file descriptor to SSL socket");
	}

	if (certverify > 0) {	/* assume SSL_VERIFY_PEER */
		/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example:
		 * Set up the SSL specific data into "openssl_cert_verify_data"
		 * and store it into the SSL structure. */
		if (openssl_cert_verify_data.hostname_allocated
		 && openssl_cert_verify_data.hostname)
			free(const_cast<void *>(static_cast<const void *>((openssl_cert_verify_data.hostname))));
		memset(&openssl_cert_verify_data, 0, sizeof(openssl_cert_verify_data));

		openssl_cert_verify_data.verify_depth = _verify_depth;
		openssl_cert_verify_data.hostname = xstrdup(_host.c_str());	/* to be freed with the structure */
		openssl_cert_verify_data.hostname_allocated = 1;
		SSL_set_ex_data(_ssl, _openssl_cert_verify_data_index, &openssl_cert_verify_data);

		SSL_set_verify(_ssl, SSL_VERIFY_PEER, openssl_cert_verify_callback);
	}

	{	/* scoping */
		const std::string& certhost_name_str = _ssl_config->getCertHostName();
		const char *certhost_name = certhost_name_str.empty() ? nullptr : certhost_name_str.c_str();

		if (certhost_name) {
			/* We have a setting like upsmon CERTHOST - to pin the certificate
			 * and other security properties for a host.
			 */
# if OPENSSL_VERSION_NUMBER >= 0x10100000L
			/* hostname verification - OpenSSL 1.1.0+ */
			const char	*verif_host = certhost_name ? certhost_name : _host.c_str();
			X509_VERIFY_PARAM	*vpm = SSL_get0_param(_ssl);

			X509_VERIFY_PARAM_set1_host(vpm, verif_host, 0);

			if (_debugConnect) std::cerr <<
				"[D2] Socket::startTLS(): Connecting in SSL to '" << _host <<
				"' and looking at certificate called '" << certhost_name << "'" <<
				std::endl << std::flush;
# else
			int certverify_val = _ssl_config->getCertVerify();
			if (_debugConnect) std::cerr <<
				"[D2] Socket::startTLS(): Connecting in SSL to '" << _host <<
				"' and was asked to look at certificate called '" << certhost_name <<
				"', but the OpenSSL library in this build is too old for that. " <<
				"Please disable the CERTHOST setting or update the library used by NUT. " <<
				(certverify_val > 0
				? "Refusing connection attempt now because certificate verification was required."
				: "Proceeding without certificate verification as it was not required.") <<
				std::endl << std::flush;

			if (certverify_val > 0) {
				SSL_free(_ssl);
				_ssl = nullptr;
				disconnect();
				throw nut::SSLException_OpenSSL("OpenSSL library too old for CERTHOST verification");
			}
# endif
		} else {
			if (_debugConnect) std::cerr <<
				"[D2] Socket::startTLS(): Connecting in SSL to '" << _host <<
				"' (no certificate name specified)" <<
				std::endl << std::flush;
		}
	}
	/* FIXME: port further upsclient/upsmon features like
	 *  upscli_find_host_cert() and respective CERTHOST-like
	 *  server expected hostname (double-checking) validation? */

	if (SSL_connect(_ssl) != 1) {
		unsigned long err = ERR_get_error();
		char errbuf[256];
		ERR_error_string_n(err, errbuf, sizeof(errbuf));
		SSL_free(_ssl);
		_ssl = nullptr;
		disconnect();
		throw nut::SSLException_OpenSSL(std::string("SSL connection failed: ") + errbuf);
	}

	/* Adapted from https://linux.die.net/man/3/ssl_set_verify man page example */
	if (SSL_get_peer_certificate(_ssl)) {
		if (SSL_get_verify_result(_ssl) == X509_V_OK) {
			upsdebugx(3, "%s: The server sent a certificate which verified OK", __func__);
		} else {
			upsdebugx(3, "%s: The server sent a certificate which did not verify OK", __func__);
		}
	} else {
		upsdebugx(3, "%s: Odd, the server did not send a certificate", __func__);
	}

# elif defined(WITH_NSS)
	/* NSS implementation following upsclient.c logic */
	static bool nss_initialized = false;

	// FIXME: Support several NSS databases, use prefix parameters?
	if (!nss_initialized) {
		PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
		PK11_SetPasswordFunc(nss_password_callback);

		SECStatus status;
		const std::string& certstore_path_str = _ssl_config->getCertStorePath();
		const std::string& certstore_prefix_str = _ssl_config->getCertStorePrefix();
		const char *certstore_path = certstore_path_str.empty() ? nullptr : certstore_path_str.c_str();
		const char *certstore_prefix = certstore_prefix_str.empty() ? nullptr : certstore_prefix_str.c_str();
		if (certstore_path) {
			if (certstore_prefix)
				throw nut::SSLException_NSS("NSS database prefix support not implemented yet");
			// FIXME: Use certstore_prefix
			status = NSS_Init(certstore_path);
		} else {
			status = NSS_NoDB_Init(nullptr);
		}

		if (status != SECSuccess) {
			throw nut::SSLException_NSS("NSS initialization failed");
		}
		nss_initialized = true;
	}

	PRFileDesc *socket = PR_ImportTCPSocket(static_cast<int>(_sock));
	if (!socket) {
		throw nut::SSLException_NSS("Cannot import socket FD");
	}

	_ssl = SSL_ImportFD(nullptr, socket);
	if (!_ssl) {
		throw nut::SSLException_NSS("Cannot import SSL FD");
	}

	if (SSL_SetPKCS11PinArg(_ssl, this) != SECSuccess) {
		nss_error("SSL_SetPKCS11PinArg");
	}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_FUNCTION_TYPE_STRICT)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#endif
	if (_ssl_config) {
		int certverify = _ssl_config->getCertVerify();
		/* CERTHOST can override the global setting for this connection */
		const SSLConfig_CERTHOST *ch = _ssl_config->getCertHostByAddrOrSubj(_host);
		if (ch) {
			if (ch->getCertVerify() != -1) {
				certverify = ch->getCertVerify();
			}
		}

		if (certverify != -1) {
			if (certverify) {
				SSL_AuthCertificateHook(_ssl, reinterpret_cast<SSLAuthCertificate>(AuthCertificate), CERT_GetDefaultCertDB());
			} else {
				SSL_AuthCertificateHook(_ssl, reinterpret_cast<SSLAuthCertificate>(AuthCertificateDontVerify), CERT_GetDefaultCertDB());
			}
		}
	}
	SSL_BadCertHook(_ssl, reinterpret_cast<SSLBadCertHandler>(BadCertHandler), this);

	if (SSL_GetClientAuthDataHook(_ssl, reinterpret_cast<SSLGetClientAuthData>(GetClientAuthData), this) != SECSuccess) {
		nss_error("SSL_GetClientAuthDataHook");
	}

	if (SSL_HandshakeCallback(_ssl, reinterpret_cast<SSLHandshakeCallback>(HandshakeCallback), this) != SECSuccess) {
		nss_error("SSL_HandshakeCallback");
	}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_FUNCTION_TYPE_STRICT)
#pragma GCC diagnostic pop
#endif

	const char	*ssl_url = nullptr;
	const std::string& certhost_addr = _ssl_config->getCertHostAddr();
	if (certhost_addr.empty()) {
		ssl_url = _host.c_str();
	} else {
		ssl_url = certhost_addr.c_str();
	}
	SSL_SetURL(_ssl, ssl_url);

	if (SSL_ResetHandshake(_ssl, PR_FALSE) != SECSuccess
	 || SSL_ForceHandshake(_ssl) != SECSuccess
	) {
		PR_Close(_ssl);
		_ssl = nullptr;
		disconnect();
		throw nut::SSLException_NSS((std::string("Handshake failed for ") + ssl_url).c_str());
	}
# endif	/* WITH_NSS */
#else
	if (_debugConnect) std::cerr <<
		"[D2] Socket::startTLS(): SSL support not compiled in" <<
		" and forcessl is " << _forcessl <<
		std::endl << std::flush;
	if (_forcessl) {
		disconnect();
		throw nut::SSLException("SSL support not compiled in");
	}
#endif	/* WITH_SSL_CXX */
}

bool Socket::isConnected()const
{
	return _sock!=INVALID_SOCKET;
}

size_t Socket::read(void* buf, size_t sz)
{
	if(!isConnected())
	{
		throw nut::NotConnectedException();
	}

	if(_tv.tv_sec>=0)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(_sock, &fds);
		int ret = select(_sock+1, &fds, nullptr, nullptr, &_tv);
		if (ret < 1) {
			throw nut::TimeoutException();
		}
	}

	ssize_t res;
#if defined(WITH_SSL_CXX) && (defined(WITH_OPENSSL) || defined(WITH_NSS))
	if (_ssl) {
# ifdef WITH_OPENSSL
		res = SSL_read(_ssl, buf, static_cast<int>(sz));
# elif defined(WITH_NSS)
		res = PR_Read(_ssl, buf, static_cast<int>(sz));
# endif
	} else {
		res = sktread(_sock, buf, sz);
	}
#else
	res = sktread(_sock, buf, sz);
#endif

	if(res==-1)
	{
		disconnect();
		throw nut::IOException("Error while reading on socket");
	}
	return static_cast<size_t>(res);
}

size_t Socket::write(const void* buf, size_t sz)
{
	if(!isConnected())
	{
		throw nut::NotConnectedException();
	}

	if(_tv.tv_sec>=0)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(_sock, &fds);
		int ret = select(_sock+1, nullptr, &fds, nullptr, &_tv);
		if (ret < 1) {
			throw nut::TimeoutException();
		}
	}

	ssize_t res;
#if defined(WITH_SSL_CXX) && (defined(WITH_OPENSSL) || defined(WITH_NSS))
	if (_ssl) {
# ifdef WITH_OPENSSL
		res = SSL_write(_ssl, buf, static_cast<int>(sz));
# elif defined(WITH_NSS)
		res = PR_Write(_ssl, buf, static_cast<int>(sz));
# endif
	} else {
		res = sktwrite(_sock, buf, sz);
	}
#else
	res = sktwrite(_sock, buf, sz);
#endif

	if(res==-1)
	{
		disconnect();
		throw nut::IOException("Error while writing on socket");
	}
	return static_cast<size_t>(res);
}

std::string Socket::read()
{
	std::string res;
	char buff[256];

	while(true)
	{
		// Look at already read data in _buffer
		if(!_buffer.empty())
		{
			size_t idx = _buffer.find('\n');
			if(idx!=std::string::npos)
			{
				res += _buffer.substr(0, idx);
				_buffer.erase(0, idx+1);
				return res;
			}
			res += _buffer;
		}

		// Read new buffer
		size_t sz = read(&buff, 256);
		if(sz==0)
		{
			disconnect();
			throw nut::IOException("Server closed connection unexpectedly");
		}
		_buffer.assign(buff, sz);
	}
}

void Socket::write(const std::string& str)
{
//	write(str.c_str(), str.size());
//	write("\n", 1);
	std::string buff = str + "\n";
	write(buff.c_str(), buff.size());
}

}/* namespace internal */


/*
 *
 * Client implementation
 *
 */

/* Pedantic builds complain about the static variable below...
 * It is assumed safe to ignore since it is a std::string with
 * no complex teardown at program exit.
 */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic push
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS
#  pragma GCC diagnostic ignored "-Wglobal-constructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS
#  pragma GCC diagnostic ignored "-Wexit-time-destructors"
# endif
#endif
const Feature Client::TRACKING = "TRACKING";
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic pop
#endif

Client::Client()
{
}

Client::~Client()
{
}

bool Client::hasDevice(const std::string& dev)
{
	std::set<std::string> devs = getDeviceNames();
	return devs.find(dev) != devs.end();
}

Device Client::getDevice(const std::string& name)
{
	if(hasDevice(name))
		return Device(this, name);
	else
		return Device(nullptr, "");
}

std::set<Device> Client::getDevices()
{
	std::set<Device> res;

	std::set<std::string> devs = getDeviceNames();
	for(std::set<std::string>::iterator it=devs.begin(); it!=devs.end(); ++it)
	{
		res.insert(Device(this, *it));
	}

	return res;
}

bool Client::hasDeviceVariable(const std::string& dev, const std::string& name)
{
	std::set<std::string> names = getDeviceVariableNames(dev);
	return names.find(name) != names.end();
}

std::map<std::string,std::vector<std::string> > Client::getDeviceVariableValues(const std::string& dev)
{
	std::map<std::string,std::vector<std::string> > res;

	std::set<std::string> names = getDeviceVariableNames(dev);
	for(std::set<std::string>::iterator it=names.begin(); it!=names.end(); ++it)
	{
		const std::string& name = *it;
		res[name] = getDeviceVariableValue(dev, name);
	}

	return res;
}

std::map<std::string,std::map<std::string,std::vector<std::string> > > Client::getDevicesVariableValues(const std::set<std::string>& devs)
{
	std::map<std::string,std::map<std::string,std::vector<std::string> > > res;

	for(std::set<std::string>::const_iterator it=devs.cbegin(); it!=devs.cend(); ++it)
	{
		res[*it] = getDeviceVariableValues(*it);
	}

	return res;
}

bool Client::hasDeviceCommand(const std::string& dev, const std::string& name)
{
	std::set<std::string> names = getDeviceCommandNames(dev);
	return names.find(name) != names.end();
}

bool Client::hasFeature(const Feature& feature)
{
	try
	{
		// If feature is known, querying it won't throw an exception.
		isFeatureEnabled(feature);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

/*
 *
 * TCP Client implementation
 *
 */

TcpClient::TcpClient():
Client(),
_host("localhost"),
_port(NUT_PORT),
_tryssl(false),
_ssl_config_openssl(nullptr),
_ssl_config_nss(nullptr),
_ssl_configured(UPSCLI_SSL_CAPS_NONE),
_timeout(0),
_socket(new internal::Socket)
{
	// Do not connect now
}

TcpClient::TcpClient(const std::string& host, uint16_t port)
	: Client(), _host(host), _port(port), _tryssl(true), _ssl_config_openssl(nullptr), _ssl_config_nss(nullptr), _ssl_configured(UPSCLI_SSL_CAPS_NONE), _timeout(-1), _socket(new internal::Socket)
{
	// No SSL settings, so just plaintext protocol
	connect();
}

TcpClient::TcpClient(const std::string& host, uint16_t port, const SSLConfig& config)
	: Client(), _host(host), _port(port), _tryssl(true), _ssl_config_openssl(nullptr), _ssl_config_nss(nullptr), _ssl_configured(UPSCLI_SSL_CAPS_NONE), _timeout(-1), _socket(new internal::Socket)
{
	setSSLConfig(config);
	connect();
}

TcpClient::~TcpClient()
{
	delete _ssl_config_openssl;
	delete _ssl_config_nss;
	delete _socket;
}

/* Return a bitmap of the abilities for the current libupsclient build */
/*static*/ int TcpClient::getSslCaps(void)
{
	int	ret = UPSCLI_SSL_CAPS_NONE;

#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
	ret |= UPSCLI_SSL_CAPS_OPENSSL;
# endif
# ifdef WITH_NSS
	ret |= UPSCLI_SSL_CAPS_NSS;
# endif
#endif	/* WITH_SSL_CXX */

	return ret;
}

int TcpClient::getSslConfigured() const
{
	return _ssl_configured;
}

void TcpClient::updateSslConfigured()
{
	_ssl_configured = UPSCLI_SSL_CAPS_NONE;

#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
	if (_ssl_config_openssl) {
		_ssl_configured |= UPSCLI_SSL_CAPS_OPENSSL;

		/* Got something to check, and ability to do so */
		if (!(_ssl_config_openssl->getKeyPass().empty() || _ssl_config_openssl->getKeyFile().empty())
		 && !(_ssl_config_openssl->getCertFile().empty() || _ssl_config_openssl->getCertIdentName().empty())
		) {
#  if (defined(HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_CTX_SET_DEFAULT_PASSWD_CB) || (defined(HAVE_SSL_SET_DEFAULT_PASSWD_CB) && HAVE_SSL_SET_DEFAULT_PASSWD_CB)
			_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_PASS;
#  endif
#  if (defined(HAVE_SSL_CTX_GET0_CERTIFICATE) && HAVE_SSL_CTX_GET0_CERTIFICATE) && (defined(HAVE_X509_CHECK_HOST) && HAVE_X509_CHECK_HOST) && (defined(HAVE_X509_CHECK_IP_ASC) && HAVE_X509_CHECK_IP_ASC) && (defined(HAVE_X509_NAME_ONELINE) && HAVE_X509_NAME_ONELINE)
			_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_NAME;
#  endif
		}
	}
# endif
# ifdef WITH_NSS
	if (_ssl_config_nss) {
		_ssl_configured |= UPSCLI_SSL_CAPS_NSS;

		/* Got something to check, and ability to do so */
		if (!(_ssl_config_nss->getCertStorePass().empty() || _ssl_config_nss->getCertStorePath().empty())
		 && !(_ssl_config_nss->getCertIdentName().empty())
		) {
			_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_PASS;
			_ssl_configured |= UPSCLI_SSL_CAPS_CERTIDENT_NAME;
		}
	}
# endif
#endif /* WITH_SSL_CXX */
}

SSLConfig_CERTSTORE::SSLConfig_CERTSTORE()
{
}

bool SSLConfig_CERTSTORE::hasCALocation() const
{
	return false;
}

bool SSLConfig_CERTSTORE::hasCertIdentity() const
{
	return false;
}

bool SSLConfig_CERTSTORE::operator < (const SSLConfig_CERTSTORE& other) const
{
	NUT_UNUSED_VARIABLE(other);
	return true;
}

SSLConfig_CERTSTORE* SSLConfig_CERTSTORE::clone() const
{
	return new SSLConfig_CERTSTORE(*this);
}

/* Some destructors compiled below to avoid "empty vtable" warnings when all the simple code was inlined to header */
SSLConfig_CERTSTORE::~SSLConfig_CERTSTORE()
{
}

SSLConfig_CERTSTORE_OpenSSL::SSLConfig_CERTSTORE_OpenSSL(
	const std::string& ca_path,
	const std::string& ca_file,
	const std::string& cert_file,
	const std::string& key_file)
	: _ca_path(ca_path),
	  _ca_file(ca_file),
	  _cert_file(cert_file),
	  _key_file(key_file)
{
}

SSLConfig_CERTSTORE_OpenSSL::SSLConfig_CERTSTORE_OpenSSL(
	const char *ca_path,
	const char *ca_file,
	const char *cert_file,
	const char *key_file)
	: _ca_path(ca_path ? ca_path : ""),
	  _ca_file(ca_file ? ca_file : ""),
	  _cert_file(cert_file ? cert_file : ""),
	  _key_file(key_file ? key_file : "")
{
}

SSLConfig_CERTSTORE_OpenSSL* SSLConfig_CERTSTORE_OpenSSL::clone() const
{
	return new SSLConfig_CERTSTORE_OpenSSL(*this);
}

SSLConfig_CERTSTORE_OpenSSL::~SSLConfig_CERTSTORE_OpenSSL()
{
}

const std::string& SSLConfig_CERTSTORE_OpenSSL::getCAFile() const
{
	return _ca_file;
}

const char *SSLConfig_CERTSTORE_OpenSSL::getCAFile_c_str() const
{
	return _ca_file.empty() ? nullptr : _ca_file.c_str();
}

const std::string& SSLConfig_CERTSTORE_OpenSSL::getCAPath() const
{
	return _ca_path;
}

const char *SSLConfig_CERTSTORE_OpenSSL::getCAPath_c_str() const
{
	return _ca_path.empty() ? nullptr : _ca_path.c_str();
}

const std::string& SSLConfig_CERTSTORE_OpenSSL::getCALocation() const
{
	if (_ca_path.empty()) return _ca_file;
	return _ca_file;
}

const char *SSLConfig_CERTSTORE_OpenSSL::getCALocation_c_str() const
{
	if (_ca_path.empty()) return _ca_file.empty() ? nullptr : _ca_file.c_str();
	return _ca_file.c_str();
}

bool SSLConfig_CERTSTORE_OpenSSL::hasCALocation() const
{
	return !_ca_path.empty() || !_ca_file.empty();
}

const std::string& SSLConfig_CERTSTORE_OpenSSL::getCertFile() const
{
	return _cert_file;
}

const char *SSLConfig_CERTSTORE_OpenSSL::getCertFile_c_str() const
{
	return _cert_file.empty() ? nullptr : _cert_file.c_str();
}

bool SSLConfig_CERTSTORE_OpenSSL::hasCertIdentity() const
{
	return !_cert_file.empty();
}

const std::string& SSLConfig_CERTSTORE_OpenSSL::getKeyFile() const
{
	return _key_file;
}

const char *SSLConfig_CERTSTORE_OpenSSL::getKeyFile_c_str() const
{
	return _key_file.empty() ? nullptr : _key_file.c_str();
}

const std::string& SSLConfig_CERTSTORE_OpenSSL::getKeyOrCertFile() const
{
	return _key_file.empty() ? _cert_file : _key_file;
}

const char *SSLConfig_CERTSTORE_OpenSSL::getKeyOrCertFile_c_str() const
{
	return getKeyOrCertFile().empty() ? nullptr : getKeyOrCertFile().c_str();
}

bool SSLConfig_CERTSTORE_OpenSSL::operator < (const SSLConfig_CERTSTORE_OpenSSL& other) const
{
	if (!_cert_file.empty() && !other._cert_file.empty()) return _cert_file < other._cert_file;
	if (!_ca_path.empty() && !other._ca_path.empty()) return _ca_path < other._ca_path;
	if (!_ca_file.empty() && !other._ca_file.empty()) return _ca_file < other._ca_file;
	return true;	/* Nothing set, this object is not more useful than the other */
}

bool SSLConfig_CERTSTORE_OpenSSL::operator < (const SSLConfig_CERTSTORE& other) const
{
	NUT_UNUSED_VARIABLE(other);
	return false;	/* we are better than arbitrary sibling/parent class instance */
}

SSLConfig_CERTSTORE_NSS::SSLConfig_CERTSTORE_NSS(
	const std::string& certstore_path,
	const std::string& certstore_pass,
	const std::string& certstore_prefix)
	: _certstore_path(certstore_path),
	  _certstore_pass(certstore_pass),
	  _certstore_prefix(certstore_prefix)
{
}

SSLConfig_CERTSTORE_NSS::SSLConfig_CERTSTORE_NSS(
	const char *certstore_path,
	const char *certstore_pass,
	const char *certstore_prefix)
	: _certstore_path(certstore_path ? certstore_path : ""),
	  _certstore_pass(certstore_pass ? certstore_pass : ""),
	  _certstore_prefix(certstore_prefix ? certstore_prefix : "")
{
}

SSLConfig_CERTSTORE_NSS* SSLConfig_CERTSTORE_NSS::clone() const
{
	return new SSLConfig_CERTSTORE_NSS(*this);
}

SSLConfig_CERTSTORE_NSS::~SSLConfig_CERTSTORE_NSS()
{
}

const std::string& SSLConfig_CERTSTORE_NSS::getCertStorePath() const
{
	return _certstore_path;
}

const char *SSLConfig_CERTSTORE_NSS::getCertStorePath_c_str() const
{
	return _certstore_path.empty() ? nullptr : _certstore_path.c_str();
}

const std::string& SSLConfig_CERTSTORE_NSS::getCertStorePass() const
{
	return _certstore_pass;
}

const char *SSLConfig_CERTSTORE_NSS::getCertStorePass_c_str() const
{
	return _certstore_pass.empty() ? nullptr : _certstore_pass.c_str();
}

const std::string& SSLConfig_CERTSTORE_NSS::getCertStorePrefix() const
{
	return _certstore_prefix;
}

const char *SSLConfig_CERTSTORE_NSS::getCertStorePrefix_c_str() const
{
	return _certstore_prefix.empty() ? nullptr : _certstore_prefix.c_str();
}

bool SSLConfig_CERTSTORE_NSS::hasCALocation() const
{
	return !_certstore_path.empty();
}

bool SSLConfig_CERTSTORE_NSS::hasCertIdentity() const
{
	return !_certstore_path.empty();
}

bool SSLConfig_CERTSTORE_NSS::operator < (const SSLConfig_CERTSTORE_NSS& other) const
{
	if (!_certstore_prefix.empty() && !other._certstore_prefix.empty()
	 && !_certstore_path.empty() && !other._certstore_path.empty())
		return _certstore_path + _certstore_prefix < other._certstore_path + other._certstore_prefix;

	if (!_certstore_path.empty() && !other._certstore_path.empty()) return _certstore_path < other._certstore_path;
	if (!_certstore_prefix.empty() && !other._certstore_prefix.empty()) return _certstore_prefix < other._certstore_prefix;

	return true;	/* Nothing set, this object is not more useful than the other */
}

bool SSLConfig_CERTSTORE_NSS::operator < (const SSLConfig_CERTSTORE& other) const
{
	NUT_UNUSED_VARIABLE(other);
	return false;	/* we are better than arbitrary sibling/parent class instance */
}

SSLConfig_CERTIDENT::SSLConfig_CERTIDENT(
	const std::string& cert_subj,
	const std::string& key_pass)
	: _cert_subj(cert_subj),
	  _key_pass(key_pass),
	  _certstore(new SSLConfig_CERTSTORE())
{
}

SSLConfig_CERTIDENT::SSLConfig_CERTIDENT(
	const std::string& cert_subj,
	const std::string& key_pass,
	const SSLConfig_CERTSTORE& certstore)
	: _cert_subj(cert_subj),
	  _key_pass(key_pass),
	  _certstore(certstore.clone())
{
}

SSLConfig_CERTIDENT::SSLConfig_CERTIDENT(
	const char *cert_subj,
	const char *key_pass)
	: _cert_subj(cert_subj ? cert_subj : ""),
	  _key_pass(key_pass ? key_pass : ""),
	  _certstore(new SSLConfig_CERTSTORE())
{
}

SSLConfig_CERTIDENT::SSLConfig_CERTIDENT(
	const char *cert_subj,
	const char *key_pass,
	const SSLConfig_CERTSTORE& certstore)
	: _cert_subj(cert_subj ? cert_subj : ""),
	  _key_pass(key_pass ? key_pass : ""),
	  _certstore(certstore.clone())
{
}

SSLConfig_CERTIDENT::SSLConfig_CERTIDENT(const SSLConfig_CERTIDENT& other)
	: _cert_subj(other._cert_subj),
	  _key_pass(other._key_pass),
	  _certstore(other._certstore ? other._certstore->clone() : nullptr)
{
}

SSLConfig_CERTIDENT& SSLConfig_CERTIDENT::operator=(const SSLConfig_CERTIDENT& other)
{
	if (this != &other) {
		_cert_subj = other._cert_subj;
		_key_pass = other._key_pass;
		delete _certstore;
		_certstore = other._certstore ? other._certstore->clone() : nullptr;
	}
	return *this;
}

SSLConfig_CERTIDENT* SSLConfig_CERTIDENT::clone() const
{
	return new SSLConfig_CERTIDENT(*this);
}

SSLConfig_CERTIDENT::~SSLConfig_CERTIDENT()
{
	delete _certstore;
}

const std::string& SSLConfig_CERTIDENT::getCertSubj() const
{
	return _cert_subj;
}

const char *SSLConfig_CERTIDENT::getCertSubj_c_str() const
{
	return _cert_subj.empty() ? nullptr : _cert_subj.c_str();
}

const std::string& SSLConfig_CERTIDENT::getKeyPass() const
{
	return _key_pass;
}

const char *SSLConfig_CERTIDENT::getKeyPass_c_str() const
{
	return _key_pass.empty() ? nullptr : _key_pass.c_str();
}

const SSLConfig_CERTSTORE& SSLConfig_CERTIDENT::getCertstore() const
{
	return *_certstore;
}

bool SSLConfig_CERTIDENT::operator < (const SSLConfig_CERTIDENT& other) const
{
	if (_cert_subj.empty() && other._cert_subj.empty()) return _key_pass < other._key_pass;
	return _cert_subj < other._cert_subj;
}

SSLConfig_CERTIDENT_OpenSSL::SSLConfig_CERTIDENT_OpenSSL(
	const std::string& cert_subj,
	const std::string& key_pass,
	const std::string& cert_file,
	const std::string& key_file)
	: SSLConfig_CERTIDENT(cert_subj, key_pass,
		SSLConfig_CERTSTORE_OpenSSL(SSLConfig::_empty_str, SSLConfig::_empty_str, cert_file, key_file))
{
}

SSLConfig_CERTIDENT_OpenSSL::SSLConfig_CERTIDENT_OpenSSL(
	const char *cert_subj,
	const char *key_pass,
	const char *cert_file,
	const char *key_file)
	: SSLConfig_CERTIDENT(
		cert_subj ? cert_subj : SSLConfig::_empty_str,
		key_pass ? key_pass : SSLConfig::_empty_str,
		SSLConfig_CERTSTORE_OpenSSL(SSLConfig::_empty_str, SSLConfig::_empty_str,
			cert_file ? cert_file : SSLConfig::_empty_str,
			key_file ? key_file : SSLConfig::_empty_str))
{
}

SSLConfig_CERTIDENT_OpenSSL* SSLConfig_CERTIDENT_OpenSSL::clone() const
{
	return new SSLConfig_CERTIDENT_OpenSSL(*this);
}

SSLConfig_CERTIDENT_OpenSSL::~SSLConfig_CERTIDENT_OpenSSL()
{
}

const std::string& SSLConfig_CERTIDENT_OpenSSL::getCertFile() const
{
	return static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(_certstore)->getCertFile();
}

const char *SSLConfig_CERTIDENT_OpenSSL::getCertFile_c_str() const
{
	return static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(_certstore)->getCertFile_c_str();
}

const std::string& SSLConfig_CERTIDENT_OpenSSL::getKeyFile() const
{
	return static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(_certstore)->getKeyFile();
}

const char *SSLConfig_CERTIDENT_OpenSSL::getKeyFile_c_str() const
{
	return static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(_certstore)->getKeyFile_c_str();
}

const std::string& SSLConfig_CERTIDENT_OpenSSL::getKeyOrCertFile() const
{
	return static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(_certstore)->getKeyOrCertFile();
}

const char *SSLConfig_CERTIDENT_OpenSSL::getKeyOrCertFile_c_str() const
{
	return static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(_certstore)->getKeyOrCertFile_c_str();
}

SSLConfig_CERTIDENT_NSS::SSLConfig_CERTIDENT_NSS(
	const std::string& cert_subj,
	const std::string& key_pass,	/* For NSS this is the private key database password, shared by all keys in it */
	const std::string& certstore_path,
	const std::string& certstore_prefix)
	: SSLConfig_CERTIDENT(cert_subj, key_pass,
		SSLConfig_CERTSTORE_NSS(certstore_path, key_pass, certstore_prefix))
{
}

SSLConfig_CERTIDENT_NSS::SSLConfig_CERTIDENT_NSS(
	const char *cert_subj,
	const char *key_pass,	/* For NSS this is the private key database password, shared by all keys in it */
	const char *certstore_path,
	const char *certstore_prefix)
	: SSLConfig_CERTIDENT(
		cert_subj ? cert_subj : SSLConfig::_empty_str,
		key_pass ? key_pass : SSLConfig::_empty_str,
		SSLConfig_CERTSTORE_NSS(certstore_path ? certstore_path : SSLConfig::_empty_str,
			key_pass ? key_pass : SSLConfig::_empty_str,
			certstore_prefix ? certstore_prefix : SSLConfig::_empty_str))
{
}

SSLConfig_CERTIDENT_NSS* SSLConfig_CERTIDENT_NSS::clone() const
{
	return new SSLConfig_CERTIDENT_NSS(*this);
}

SSLConfig_CERTIDENT_NSS::~SSLConfig_CERTIDENT_NSS()
{
}

const std::string& SSLConfig_CERTIDENT_NSS::getCertStorePath() const
{
	return static_cast<const SSLConfig_CERTSTORE_NSS*>(_certstore)->getCertStorePath();
}

const char *SSLConfig_CERTIDENT_NSS::getCertStorePath_c_str() const
{
	return static_cast<const SSLConfig_CERTSTORE_NSS*>(_certstore)->getCertStorePath_c_str();
}

const std::string& SSLConfig_CERTIDENT_NSS::getCertStorePass() const
{
	return static_cast<const SSLConfig_CERTSTORE_NSS*>(_certstore)->getCertStorePass();
}

const char *SSLConfig_CERTIDENT_NSS::getCertStorePass_c_str() const
{
	return static_cast<const SSLConfig_CERTSTORE_NSS*>(_certstore)->getCertStorePass_c_str();
}

const std::string& SSLConfig_CERTIDENT_NSS::getCertStorePrefix() const
{
	return static_cast<const SSLConfig_CERTSTORE_NSS*>(_certstore)->getCertStorePrefix();
}

const char *SSLConfig_CERTIDENT_NSS::getCertStorePrefix_c_str() const
{
	return static_cast<const SSLConfig_CERTSTORE_NSS*>(_certstore)->getCertStorePrefix_c_str();
}

SSLConfig_CERTHOST::SSLConfig_CERTHOST(
	const std::string& host_addr,
	const std::string& cert_subj,
	int forcessl,
	int certverify)
	: _host_addr(host_addr),
	  _cert_subj(cert_subj),
	  _forcessl(forcessl),
	  _certverify(certverify)
{
}

SSLConfig_CERTHOST::SSLConfig_CERTHOST(
	const char *host_addr,
	const char *cert_subj,
	int forcessl,
	int certverify)
	: _host_addr(host_addr ? host_addr : ""),
	  _cert_subj(cert_subj ? cert_subj : ""),
	  _forcessl(forcessl),
	  _certverify(certverify)
{
}

SSLConfig_CERTHOST* SSLConfig_CERTHOST::clone() const
{
	return new SSLConfig_CERTHOST(*this);
}

SSLConfig_CERTHOST::~SSLConfig_CERTHOST()
{
}

const std::string& SSLConfig_CERTHOST::getHostAddr() const
{
	return _host_addr;
}

const char *SSLConfig_CERTHOST::getHostAddr_c_str() const
{
	return _host_addr.empty() ? nullptr : _host_addr.c_str();
}

const std::string& SSLConfig_CERTHOST::getCertSubj() const
{
	return _cert_subj;
}

const char *SSLConfig_CERTHOST::getCertSubj_c_str() const
{
	return _cert_subj.empty() ? nullptr : _cert_subj.c_str();
}

int SSLConfig_CERTHOST::getForceSsl() const
{
	return _forcessl;
}

int SSLConfig_CERTHOST::getCertVerify() const
{
	return _certverify;
}

bool SSLConfig_CERTHOST::operator < (const SSLConfig_CERTHOST& other) const
{
	if (_cert_subj.empty() && other._cert_subj.empty()) return _host_addr < other._host_addr;
	return _cert_subj < other._cert_subj;
}

SSLConfig::SSLConfig(
	bool forcessl,
	int certverify)
	: _forcessl(forcessl),
	  _certverify(certverify)
{
}

SSLConfig::SSLConfig(
	const SSLConfig_CERTIDENT& certident,
	bool forcessl,
	int certverify)
	: _forcessl(forcessl),
	  _certverify(certverify)
{
	setCertIdent(certident);
}

SSLConfig::SSLConfig(
	const SSLConfig_CERTSTORE& certstore,
	bool forcessl,
	int certverify)
	: _forcessl(forcessl),
	  _certverify(certverify)
{
	setCertStore(certstore);
}

SSLConfig::SSLConfig(
	const SSLConfig_CERTSTORE& certstore,
	const SSLConfig_CERTIDENT& certident,
	bool forcessl,
	int certverify)
	: _forcessl(forcessl),
	  _certverify(certverify)
{
	setCertStore(certstore);
	setCertIdent(certident);
}

SSLConfig::SSLConfig(const SSLConfig& other)
	: _forcessl(other._forcessl),
	  _certverify(other._certverify)
{
	for (auto* item : other._certidents) {
		_certidents.insert(item->clone());
	}
	for (auto* item : other._certstores) {
		_certstores.insert(item->clone());
	}
	for (auto* item : other._certhosts) {
		_certhosts.insert(item->clone());
	}
}

SSLConfig& SSLConfig::operator=(const SSLConfig& other)
{
	if (this != &other) {
		_forcessl = other._forcessl;
		_certverify = other._certverify;
		unsetCertIdent();
		unsetCertStore();
		unsetCertHost();
		for (auto* item : other._certidents) {
			_certidents.insert(item->clone());
		}
		for (auto* item : other._certstores) {
			_certstores.insert(item->clone());
		}
		for (auto* item : other._certhosts) {
			_certhosts.insert(item->clone());
		}
	}
	return *this;
}

SSLConfig::~SSLConfig()
{
	unsetCertIdent();
	unsetCertStore();
	unsetCertHost();
}

bool SSLConfig::getForceSsl() const
{
	return _forcessl;
}

void SSLConfig::setForceSsl(bool forcessl)
{
	_forcessl = forcessl;
}

int SSLConfig::getCertVerify() const
{
	return _certverify;
}

void SSLConfig::setCertVerify(int certverify)
{
	_certverify = certverify;
}

void SSLConfig::setCertIdent(const SSLConfig_CERTIDENT& certident)
{
	unsetCertIdent();
	_certidents.insert(certident.clone());
}

void SSLConfig::unsetCertIdent()
{
	for (auto* item : _certidents) {
		delete item;
	}
	_certidents.clear();
}

const SSLConfig_CERTIDENT* SSLConfig::getCertIdent() const
{
	if (_certidents.empty()) return nullptr;
	return *(_certidents.begin());
}

void SSLConfig::setCertStore(const SSLConfig_CERTSTORE& certident)
{
	unsetCertStore();
	_certstores.insert(certident.clone());
}

void SSLConfig::unsetCertStore()
{
	for (auto* item : _certstores) {
		delete item;
	}
	_certstores.clear();
}

const SSLConfig_CERTSTORE* SSLConfig::getCertStore() const
{
	return _certstores.empty() ? nullptr : *(_certstores.begin());
}

void SSLConfig::addCertHost(const SSLConfig_CERTHOST& certhost)
{
	if (certhost.getHostAddr().empty()) return;
	if (certhost.getCertSubj().empty()) return;
	_certhosts.insert(certhost.clone());
}

void SSLConfig::unsetCertHost()
{
	for (auto* item : _certhosts) {
		delete item;
	}
	_certhosts.clear();
}

const std::set<const SSLConfig_CERTHOST*> SSLConfig::getCertHosts() const
{
	return _certhosts;
}

const SSLConfig_CERTHOST *SSLConfig::getFirstCertHost() const
{
	return _certhosts.empty() ? nullptr : *(_certhosts.begin());
}

const SSLConfig_CERTHOST *SSLConfig::getCertHostByAddr(std::string &s) const
{
	for (const auto* item : _certhosts) {
		if (item->getHostAddr() == s) {
			return item;
		}
	}
	return nullptr;
}

const SSLConfig_CERTHOST *SSLConfig::getCertHostBySubj(std::string &s) const
{
	for (const auto* item : _certhosts) {
		if (item->getCertSubj() == s) {
			return item;
		}
	}
	return nullptr;
}

const SSLConfig_CERTHOST *SSLConfig::getCertHostByAddrOrSubj(std::string &s) const
{
	for (const auto* item : _certhosts) {
		if (item->getHostAddr() == s || item->getCertSubj() == s) {
			return item;
		}
	}
	return nullptr;
}

const std::string SSLConfig::_empty_str = "";

void SSLConfig::apply(TcpClient& client) const
{
	client.setSslForce(_forcessl);
	client.setSslCertVerify(_certverify);
}

SSLConfig_OpenSSL::SSLConfig_OpenSSL(
	bool forcessl,
	int certverify,
	const std::string& ca_path,
	const std::string& ca_file,
	const std::string& cert_file,
	const std::string& key_file,
	const std::string& key_pass,
	const std::string& certident_name,
	const std::string& certhost_addr,
	const std::string& certhost_name)
	: SSLConfig(
		SSLConfig_CERTSTORE_OpenSSL(ca_path, ca_file),
		SSLConfig_CERTIDENT_OpenSSL(certident_name, key_pass, cert_file, key_file),
		forcessl, certverify)
{
	if (!(certhost_addr.empty()) && !(certhost_name.empty())) {
		addCertHost(SSLConfig_CERTHOST(certhost_addr, certhost_name));
	}
}

SSLConfig_OpenSSL::SSLConfig_OpenSSL(
	bool forcessl,
	int certverify,
	const char *ca_path,
	const char *ca_file,
	const char *cert_file,
	const char *key_file,
	const char *key_pass,
	const char *certident_name,
	const char *certhost_addr,
	const char *certhost_name)
	: SSLConfig(
		SSLConfig_CERTSTORE_OpenSSL(
			ca_path ? ca_path : SSLConfig::_empty_str,
			ca_file ? ca_file : SSLConfig::_empty_str),
		SSLConfig_CERTIDENT_OpenSSL(certident_name ? certident_name : SSLConfig::_empty_str,
			key_pass ? key_pass : SSLConfig::_empty_str,
			cert_file ? cert_file : SSLConfig::_empty_str,
			key_file ? key_file : SSLConfig::_empty_str),
		forcessl, certverify)
{
	if (certhost_addr && *certhost_addr && certhost_name && *certhost_name) {
		addCertHost(SSLConfig_CERTHOST(certhost_addr, certhost_name));
	}
}

const std::string& SSLConfig_OpenSSL::getCAPath() const
{
	const SSLConfig_CERTSTORE_OpenSSL *cs = static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(getCertStore());
	if (cs) return cs->getCAPath();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getCAPath_c_str() const
{
	const SSLConfig_CERTSTORE_OpenSSL *cs = static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(getCertStore());
	if (cs) return cs->getCAPath_c_str();
	return nullptr;
}

const std::string& SSLConfig_OpenSSL::getCAFile() const
{
	const SSLConfig_CERTSTORE_OpenSSL *cs = static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(getCertStore());
	if (cs) return cs->getCAFile();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getCAFile_c_str() const
{
	const SSLConfig_CERTSTORE_OpenSSL *cs = static_cast<const SSLConfig_CERTSTORE_OpenSSL*>(getCertStore());
	if (cs) return cs->getCAFile_c_str();
	return nullptr;
}

const std::string& SSLConfig_OpenSSL::getCertFile() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getCertFile();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getCertFile_c_str() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getCertFile_c_str();
	return nullptr;
}

const std::string& SSLConfig_OpenSSL::getKeyFile() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getKeyFile();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getKeyFile_c_str() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getKeyFile_c_str();
	return nullptr;
}

const std::string& SSLConfig_OpenSSL::getKeyPass() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getKeyPass();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getKeyPass_c_str() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getKeyPass_c_str();
	return nullptr;
}

const std::string& SSLConfig_OpenSSL::getCertIdentName() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getCertSubj();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getCertIdentName_c_str() const
{
	const SSLConfig_CERTIDENT_OpenSSL *ci = static_cast<const SSLConfig_CERTIDENT_OpenSSL*>(getCertIdent());
	if (ci) return ci->getCertSubj_c_str();
	return nullptr;
}

const std::string& SSLConfig_OpenSSL::getCertHostAddr() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getHostAddr();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getCertHostAddr_c_str() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getHostAddr_c_str();
	return nullptr;
}

const std::string& SSLConfig_OpenSSL::getCertHostName() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getCertSubj();
	return _empty_str;
}

const char *SSLConfig_OpenSSL::getCertHostName_c_str() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getCertSubj_c_str();
	return nullptr;
}

void SSLConfig_OpenSSL::apply(TcpClient& client) const
{
#ifdef DEBUG
	/* Line by line, so we know which accessor crashes, if any */
	std::cerr << "SSLConfig_OpenSSL::apply(): ";
	std::cerr << "forceSSL=" << getForceSsl();
	std::cerr << ", certVerify=" << getCertVerify();
	std::cerr << ", getCAPath='" << getCAPath();
	std::cerr << "', getCAFile='" << getCAFile();
	std::cerr << "', getCertFile='" << getCertFile();
	std::cerr << "', getKeyFile='" << getKeyFile();
	std::cerr << "', getKeyPass='" << getKeyPass();
	std::cerr << "', certHostAddr='" << getCertHostAddr();
	std::cerr << "', certHostName='" << getCertHostName();
	std::cerr << "', certIdentName='" << getCertIdentName() << "'";
	std::cerr << std::endl;
#endif

	client.setSSLConfig_OpenSSL(getForceSsl(), getCertVerify(),
		getCAPath(), getCAFile(),
		getCertFile(), getKeyFile(), getKeyPass(),
		getCertIdentName(),
		getCertHostAddr(), getCertHostName());
}

SSLConfig_NSS::SSLConfig_NSS(bool forcessl, int certverify,
	const std::string& certstore_path, const std::string& certstore_pass,
	const std::string& certstore_prefix,
	const std::string& certhost_addr, const std::string& certhost_name,
	const std::string& certident_name)
	: SSLConfig(
		SSLConfig_CERTIDENT_NSS(certident_name, certstore_pass, certstore_path, certstore_prefix),
		forcessl, certverify)
{
	if (!(certhost_addr.empty()) && !(certhost_name.empty())) {
		addCertHost(SSLConfig_CERTHOST(certhost_addr, certhost_name));
	}
}

SSLConfig_NSS::SSLConfig_NSS(bool forcessl, int certverify,
	const char *certstore_path, const char *certstore_pass,
	const char *certstore_prefix,
	const char *certhost_addr, const char *certhost_name,
	const char *certident_name)
	: SSLConfig(
		SSLConfig_CERTIDENT_NSS(
			certident_name ? certident_name : SSLConfig::_empty_str,
			certstore_pass ? certstore_pass : SSLConfig::_empty_str,
			certstore_path ? certstore_path : SSLConfig::_empty_str,
			certstore_prefix ? certstore_prefix : SSLConfig::_empty_str),
		forcessl, certverify)
{
	if (certhost_addr && *certhost_addr && certhost_name && *certhost_name) {
		addCertHost(SSLConfig_CERTHOST(certhost_addr, certhost_name));
	}
}

const std::string& SSLConfig_NSS::getCertStorePath() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertStorePath();
	return _empty_str;
}

const char *SSLConfig_NSS::getCertStorePath_c_str() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertStorePath_c_str();
	return nullptr;
}

const std::string& SSLConfig_NSS::getCertStorePass() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertStorePass();
	return _empty_str;
}

const char *SSLConfig_NSS::getCertStorePass_c_str() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertStorePass_c_str();
	return nullptr;
}

const std::string& SSLConfig_NSS::getCertStorePrefix() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertStorePrefix();
	return _empty_str;
}

const char *SSLConfig_NSS::getCertStorePrefix_c_str() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertStorePrefix_c_str();
	return nullptr;
}

const std::string& SSLConfig_NSS::getCertIdentName() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertSubj();
	return _empty_str;
}

const char *SSLConfig_NSS::getCertIdentName_c_str() const
{
	const SSLConfig_CERTIDENT_NSS *ci = static_cast<const SSLConfig_CERTIDENT_NSS*>(getCertIdent());
	if (ci) return ci->getCertSubj_c_str();
	return nullptr;
}

const std::string& SSLConfig_NSS::getCertHostAddr() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getHostAddr();
	return _empty_str;
}

const char *SSLConfig_NSS::getCertHostAddr_c_str() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getHostAddr_c_str();
	return nullptr;
}

const std::string& SSLConfig_NSS::getCertHostName() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getCertSubj();
	return _empty_str;
}

const char *SSLConfig_NSS::getCertHostName_c_str() const
{
	const SSLConfig_CERTHOST *ch = getFirstCertHost();
	if (ch) return ch->getCertSubj_c_str();
	return nullptr;
}

void SSLConfig_NSS::apply(TcpClient& client) const
{
#ifdef DEBUG
	/* Line by line, so we know which accessor crashes, if any */
	std::cerr << "SSLConfig_NSS::apply(): ";
	std::cerr << "forceSSL=" << getForceSsl();
	std::cerr << ", certVerify=" << getCertVerify();
	std::cerr << ", certStorePath='" << getCertStorePath();
	std::cerr << "', certStorePass='" << getCertStorePass();
	std::cerr << "', certStorePrefix='" << getCertStorePrefix();
	std::cerr << "', certHostAddr='" << getCertHostAddr();
	std::cerr << "', certHostName='" << getCertHostName();
	std::cerr << "', certIdentName='" << getCertIdentName() << "'";
	std::cerr << std::endl;
#endif

	client.setSSLConfig_NSS(getForceSsl(), getCertVerify(),
		getCertStorePath(), getCertStorePass(), getCertStorePrefix(),
		getCertHostAddr(), getCertHostName(),
		getCertIdentName());
}

void TcpClient::setSSLConfig(const SSLConfig& config)
{
	config.apply(*this);
}

void TcpClient::setSSLConfig_OpenSSL(int forcessl, int certverify, const char *ca_path, const char *ca_file, const char *cert_file, const char *key_file, const char *key_pass, const char *certident_name, const char *certhost_addr, const char *certhost_name)
{
	delete _ssl_config_openssl;
	_ssl_config_openssl = new SSLConfig_OpenSSL(forcessl, certverify, ca_path, ca_file, cert_file, key_file, key_pass, certident_name, certhost_addr, certhost_name);
	updateSslConfigured();
}

void TcpClient::setSSLConfig_OpenSSL(int forcessl, int certverify, const std::string& ca_path, const std::string& ca_file, const std::string& cert_file, const std::string& key_file, const std::string& key_pass, const std::string& certident_name, const std::string& certhost_addr, const std::string& certhost_name)
{
	delete _ssl_config_openssl;
	_ssl_config_openssl = new SSLConfig_OpenSSL(forcessl, certverify, ca_path, ca_file, cert_file, key_file, key_pass, certident_name, certhost_addr, certhost_name);
	updateSslConfigured();
}

void TcpClient::setSSLConfig_NSS(int forcessl, int certverify, const char *certstore_path, const char *certstore_pass, const char *certstore_prefix, const char *certhost_addr, const char *certhost_name, const char *certident_name)
{
	delete _ssl_config_nss;
	_ssl_config_nss = new SSLConfig_NSS(forcessl, certverify, certstore_path, certstore_pass, certstore_prefix, certhost_addr, certhost_name, certident_name);
	updateSslConfigured();
}

void TcpClient::setSSLConfig_NSS(int forcessl, int certverify, const std::string& certstore_path, const std::string& certstore_pass, const std::string& certstore_prefix, const std::string& certhost_addr, const std::string& certhost_name, const std::string& certident_name)
{
	delete _ssl_config_nss;
	_ssl_config_nss = new SSLConfig_NSS(forcessl, certverify, certstore_path, certstore_pass, certstore_prefix, certhost_addr, certhost_name, certident_name);
	updateSslConfigured();
}

void TcpClient::connect(const std::string& host, uint16_t port)
{
	_host = host;
	_port = port;
	connect();
}

void TcpClient::connect(const std::string& host, uint16_t port, bool tryssl)
{
	_tryssl = tryssl;
	connect(host, port);
}

void TcpClient::connect()
{
	_socket->connect(_host, _port);
	bool forcessl = getSslForce();
	if (_tryssl || forcessl) {
		/* We can actually set both types of config, most points
		 *  are shared and the rest depends on build abilities */
		if (_ssl_config_openssl) {
			_socket->setSSLConfig_OpenSSL(*_ssl_config_openssl);
		}
		if (_ssl_config_nss) {
			_socket->setSSLConfig_NSS(*_ssl_config_nss);
		}

		/* May throw in case of low-level problems */
		_socket->startTLS();

		/* Make sure handshake succeeded or abort early
		 * (there is currently no way for the server to
		 * report its fault to the client when connection
		 * is half-way secure):
		 */
		if (!isValidProtocolVersion()) {
			if (forcessl) {
				disconnect();
				throw nut::SSLException("STARTTLS setup claimed to succeed, but protocol version check in the secured session failed, and SSL is required");
			}
			/* TODO: Drop SSL context or restart the connection as plaintext if SSL is not required? */
		}
	}
}

bool TcpClient::isSSL() const
{
	return _socket->isSSL();
}

bool TcpClient::getSslTry() const
{
	return _tryssl;
}

void TcpClient::setSslTry(bool tryssl)
{
	_tryssl = tryssl;
}

bool TcpClient::getSslForce() const
{
	/* Whichever is enabled (if any) wins */
	return (
		(_ssl_config_openssl && _ssl_config_openssl->getForceSsl())
	 || (_ssl_config_nss && _ssl_config_nss->getForceSsl())
	);
}

void TcpClient::setSslForce(bool forcessl)
{
	if (_ssl_config_openssl) _ssl_config_openssl->setForceSsl(forcessl);
	if (_ssl_config_nss) _ssl_config_nss->setForceSsl(forcessl);
}

int TcpClient::getSslCertVerify() const
{
	if (_ssl_config_openssl) return _ssl_config_openssl->getCertVerify();
	if (_ssl_config_nss) return _ssl_config_nss->getCertVerify();
	return -1;
}

void TcpClient::setSslCertVerify(int certverify)
{
	if (_ssl_config_openssl) _ssl_config_openssl->setCertVerify(certverify);
	if (_ssl_config_nss) _ssl_config_nss->setCertVerify(certverify);
}

const std::string& TcpClient::getSslCAPath() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getCAPath();
	return SSLConfig::_empty_str;
}

/* Here and below, individual (re-)setters are tricky because we can't easily
 * change one field of an SSLConfig_* class without recreating it or adding
 * setters to it. For now, let's assume we should recreate it with new value
 * if it exists. */
void TcpClient::setSslCAPath(const std::string& ca_path)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), ca_path, getSslCAFile(), getSslCertFile(), getSslKeyFile(), getSslKeyPass(), getSslCertIdentName(), getSslCertHostAddr(), getSslCertHostName());
	}
}

void TcpClient::setSslCAPath(const char* ca_path)
{
	setSslCAPath(ca_path ? ca_path : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslCAFile() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getCAFile();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslCAFile(const std::string& ca_file)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), getSslCAPath(), ca_file, getSslCertFile(), getSslKeyFile(), getSslKeyPass(), getSslCertIdentName(), getSslCertHostAddr(), getSslCertHostName());
	}
}

void TcpClient::setSslCAFile(const char* ca_file)
{
	setSslCAFile(ca_file ? ca_file : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslCertFile() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getCertFile();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslCertFile(const std::string& cert_file)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), getSslCAPath(), getSslCAFile(), cert_file, getSslKeyFile(), getSslKeyPass(), getSslCertIdentName(), getSslCertHostAddr(), getSslCertHostName());
	}
}

void TcpClient::setSslCertFile(const char* cert_file)
{
	setSslCertFile(cert_file ? cert_file : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslKeyFile() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getKeyFile();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslKeyFile(const std::string& key_file)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), getSslCAPath(), getSslCAFile(), getSslCertFile(), key_file, getSslKeyPass(), getSslCertIdentName(), getSslCertHostAddr(), getSslCertHostName());
	}
}

void TcpClient::setSslKeyFile(const char* key_file)
{
	setSslKeyFile(key_file ? key_file : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslKeyPass() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getKeyPass();
	if (_ssl_config_nss && (_ssl_configured & UPSCLI_SSL_CAPS_NSS)) return _ssl_config_nss->getCertStorePass();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslKeyPass(const std::string& key_pass)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), getSslCAPath(), getSslCAFile(), getSslCertFile(), getSslKeyFile(), key_pass, getSslCertIdentName(), getSslCertHostAddr(), getSslCertHostName());
	}
	if (_ssl_config_nss) {
		setSSLConfig_NSS(getSslForce(), getSslCertVerify(), getSslCertstorePath(), key_pass, getSslCertstorePrefix(), getSslCertHostAddr(), getSslCertHostName(), getSslCertIdentName());
	}
}

void TcpClient::setSslKeyPass(const char* key_pass)
{
	setSslKeyPass(key_pass ? key_pass : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslCertstorePath() const
{
	if (_ssl_config_nss && (_ssl_configured & UPSCLI_SSL_CAPS_NSS)) return _ssl_config_nss->getCertStorePath();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslCertstorePath(const std::string& certstore_path)
{
	if (_ssl_config_nss) {
		setSSLConfig_NSS(getSslForce(), getSslCertVerify(), certstore_path, getSslKeyPass(), getSslCertstorePrefix(), getSslCertHostAddr(), getSslCertHostName(), getSslCertIdentName());
	}
}

void TcpClient::setSslCertstorePath(const char* certstore_path)
{
	setSslCertstorePath(certstore_path ? certstore_path : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslCertstorePrefix() const
{
	if (_ssl_config_nss && (_ssl_configured & UPSCLI_SSL_CAPS_NSS)) return _ssl_config_nss->getCertStorePrefix();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslCertstorePrefix(const std::string& certstore_prefix)
{
	if (_ssl_config_nss) {
		setSSLConfig_NSS(getSslForce(), getSslCertVerify(), getSslCertstorePath(), getSslKeyPass(), certstore_prefix, getSslCertHostAddr(), getSslCertHostName(), getSslCertIdentName());
	}
}

void TcpClient::setSslCertstorePrefix(const char* certstore_prefix)
{
	setSslCertstorePrefix(certstore_prefix ? certstore_prefix : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslCertIdentName() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getCertIdentName();
	if (_ssl_config_nss && (_ssl_configured & UPSCLI_SSL_CAPS_NSS)) return _ssl_config_nss->getCertIdentName();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslCertIdentName(const std::string& certident_name)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), getSslCAPath(), getSslCAFile(), getSslCertFile(), getSslKeyFile(), getSslKeyPass(), certident_name, getSslCertHostAddr(), getSslCertHostName());
	}
	if (_ssl_config_nss) {
		setSSLConfig_NSS(getSslForce(), getSslCertVerify(), getSslCertstorePath(), getSslKeyPass(), getSslCertstorePrefix(), getSslCertHostAddr(), getSslCertHostName(), certident_name);
	}
}

void TcpClient::setSslCertIdentName(const char* certident_name)
{
	setSslCertIdentName(certident_name ? certident_name : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslCertHostAddr() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getCertHostAddr();
	if (_ssl_config_nss && (_ssl_configured & UPSCLI_SSL_CAPS_NSS)) return _ssl_config_nss->getCertHostAddr();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslCertHostAddr(const std::string& certhost_addr)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), getSslCAPath(), getSslCAFile(), getSslCertFile(), getSslKeyFile(), getSslKeyPass(), getSslCertIdentName(), certhost_addr, getSslCertHostName());
	}
	if (_ssl_config_nss) {
		setSSLConfig_NSS(getSslForce(), getSslCertVerify(), getSslCertstorePath(), getSslKeyPass(), getSslCertstorePrefix(), certhost_addr, getSslCertHostName(), getSslCertIdentName());
	}
}

void TcpClient::setSslCertHostAddr(const char* certhost_addr)
{
	setSslCertHostAddr(certhost_addr ? certhost_addr : SSLConfig::_empty_str);
}

const std::string& TcpClient::getSslCertHostName() const
{
	if (_ssl_config_openssl && (_ssl_configured & UPSCLI_SSL_CAPS_OPENSSL)) return _ssl_config_openssl->getCertHostName();
	if (_ssl_config_nss && (_ssl_configured & UPSCLI_SSL_CAPS_NSS)) return _ssl_config_nss->getCertHostName();
	return SSLConfig::_empty_str;
}

void TcpClient::setSslCertHostName(const std::string& certhost_name)
{
	if (_ssl_config_openssl) {
		setSSLConfig_OpenSSL(getSslForce(), getSslCertVerify(), getSslCAPath(), getSslCAFile(), getSslCertFile(), getSslKeyFile(), getSslKeyPass(), getSslCertIdentName(), getSslCertHostAddr(), certhost_name);
	}
	if (_ssl_config_nss) {
		setSSLConfig_NSS(getSslForce(), getSslCertVerify(), getSslCertstorePath(), getSslKeyPass(), getSslCertstorePrefix(), getSslCertHostAddr(), certhost_name, getSslCertIdentName());
	}
}

void TcpClient::setSslCertHostName(const char* certhost_name)
{
	setSslCertHostName(certhost_name ? certhost_name : SSLConfig::_empty_str);
}

void TcpClient::setDebugConnect(bool d)
{
	_socket->setDebugConnect(d);
}

std::string TcpClient::getHost()const
{
	return _host;
}

uint16_t TcpClient::getPort()const
{
	return _port;
}

bool TcpClient::isConnected()const
{
	return _socket->isConnected();
}

void TcpClient::disconnect()
{
	_socket->disconnect();
}

void TcpClient::setTimeout(time_t timeout)
{
	_timeout = timeout;
}

time_t TcpClient::getTimeout()const
{
	return _timeout;
}

void TcpClient::authenticate(const std::string& user, const std::string& passwd)
{
	detectError(sendQuery("USERNAME " + user));
	detectError(sendQuery("PASSWORD " + passwd));
}

void TcpClient::logout()
{
	detectError(sendQuery("LOGOUT"));
	_socket->disconnect();
}

bool TcpClient::isValidProtocolVersion(const std::string& version_re)
{
	std::string version;
	try {
		version = sendQuery("PROTVER");
	} catch (NutException &ignored) {
		NUT_UNUSED_VARIABLE(ignored);
		/* Deprecated and hidden, but may be what ancient NUT servers say
		 * May throw if the error is due to (non-)connection */
		version = sendQuery("NETVER");
	}

	if (version_re.empty()) {
		// Basic check for 1.0 through 1.3, as of NUT v2.8.2
		if (version == "1.0" || version == "1.1" || version == "1.2" || version == "1.3") {
			return true;
		}
	} else {
		// TODO: Regex
		return (version_re == version);
	}

	return false;
}

Device TcpClient::getDevice(const std::string& name)
{
	try
	{
		get("UPSDESC", name);
	}
	catch(NutException& ex)
	{
		if(ex.str()=="UNKNOWN-UPS")
			return Device(nullptr, "");
		else
			throw;
	}
	return Device(this, name);
}

std::set<std::string> TcpClient::getDeviceNames()
{
	std::set<std::string> res;

	std::vector<std::vector<std::string> > devs = list("UPS");
	for(std::vector<std::vector<std::string> >::iterator it=devs.begin();
		it!=devs.end(); ++it)
	{
		std::string id = (*it)[0];
		if(!id.empty())
			res.insert(id);
	}

	return res;
}

std::string TcpClient::getDeviceDescription(const std::string& name)
{
	return get("UPSDESC", name)[0];
}

std::set<std::string> TcpClient::getDeviceVariableNames(const std::string& dev)
{
	std::set<std::string> set;

	std::vector<std::vector<std::string> > res = list("VAR", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(res[n][0]);
	}

	return set;
}

std::set<std::string> TcpClient::getDeviceRWVariableNames(const std::string& dev)
{
	std::set<std::string> set;

	std::vector<std::vector<std::string> > res = list("RW", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(res[n][0]);
	}

	return set;
}

std::string TcpClient::getDeviceVariableDescription(const std::string& dev, const std::string& name)
{
	return get("DESC", dev + " " + name)[0];
}

std::vector<std::string> TcpClient::getDeviceVariableValue(const std::string& dev, const std::string& name)
{
	return get("VAR", dev + " " + name);
}

std::map<std::string,std::vector<std::string> > TcpClient::getDeviceVariableValues(const std::string& dev)
{

	std::map<std::string,std::vector<std::string> >  map;

	std::vector<std::vector<std::string> > res = list("VAR", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		std::vector<std::string>& vals = res[n];
		std::string var = vals[0];
		vals.erase(vals.begin());
		map[var] = vals;
	}

	return map;
}

std::map<std::string,std::map<std::string,std::vector<std::string> > > TcpClient::getDevicesVariableValues(const std::set<std::string>& devs)
{
	std::map<std::string,std::map<std::string,std::vector<std::string> > > map;

	if (devs.empty())
	{
		// This request might come from processing the empty valid
		// response of an upsd server which was allowed to start
		// with no device sections in its ups.conf
		return map;
	}

	std::vector<std::string> queries;
	for (std::set<std::string>::const_iterator it=devs.cbegin(); it!=devs.cend(); ++it)
	{
		queries.push_back("LIST VAR " + *it);
	}
	sendAsyncQueries(queries);

	for (std::set<std::string>::const_iterator it=devs.cbegin(); it!=devs.cend(); ++it)
	{
		try
		{
			std::map<std::string,std::vector<std::string> > map2;
			std::vector<std::vector<std::string> > res = parseList("VAR " + *it);
			for (std::vector<std::vector<std::string> >::iterator it2=res.begin(); it2!=res.end(); ++it2)
			{
				std::vector<std::string>& vals = *it2;
				std::string var = vals[0];
				vals.erase(vals.begin());
				map2[var] = vals;
			}
			map[*it] = map2;
		}
		catch (NutException&)
		{
			// We sent a bunch of queries, we need to process them all to clear up the backlog.
		}
	}

	if (map.empty())
	{
		// We may fail on some devices, but not on ALL devices.
		throw NutException("Invalid device");
	}

	return map;
}

TrackingID TcpClient::setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value, int waitIntervalSec, int waitMaxCount)
{
	std::string query = "SET VAR " + dev + " " + name + " " + escape(value);
	return sendTrackingQuery(query, waitIntervalSec, waitMaxCount);
}

TrackingID TcpClient::setDeviceVariable(const std::string& dev, const std::string& name, const std::vector<std::string>& values, int waitIntervalSec, int waitMaxCount)
{
	std::string query = "SET VAR " + dev + " " + name;
	for(size_t n=0; n<values.size(); ++n)
	{
		query += " " + escape(values[n]);
	}
	return sendTrackingQuery(query, waitIntervalSec, waitMaxCount);
}

std::set<std::string> TcpClient::getDeviceCommandNames(const std::string& dev)
{
	std::set<std::string> cmds;

	std::vector<std::vector<std::string> > res = list("CMD", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		cmds.insert(res[n][0]);
	}

	return cmds;
}

std::string TcpClient::getDeviceCommandDescription(const std::string& dev, const std::string& name)
{
	return get("CMDDESC", dev + " " + name)[0];
}

TrackingID TcpClient::executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param, int waitIntervalSec, int waitMaxCount)
{
	return sendTrackingQuery("INSTCMD " + dev + " " + name + " " + param, waitIntervalSec, waitMaxCount);
}

std::map<std::string, std::set<std::string>> TcpClient::listDeviceClients(void)
{
	/* Lists all clients of all devices (which have at least one client) */
	std::map<std::string, std::set<std::string>> deviceClientsMap;

	std::set<std::string> devs = getDeviceNames();
	for(std::set<std::string>::iterator it=devs.begin(); it!=devs.end(); ++it)
	{
		std::string dev = *it;
		std::set<std::string> deviceClients = deviceGetClients(dev);
		if (!deviceClients.empty()) {
			deviceClientsMap[dev] = deviceClients;
		}
	}

	return deviceClientsMap;
}

std::set<std::string> TcpClient::deviceGetClients(const std::string& dev)
{
	/* Who did a deviceLogin() to this dev? */
	std::set<std::string> clients;

	std::vector<std::vector<std::string> > res = list("CLIENT", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		clients.insert(res[n][0]);
	}

	return clients;
}

void TcpClient::deviceLogin(const std::string& dev)
{
	/* Requires that current session is already logged in with
	 * an account which has one of "upsmon" roles in `upsd.users` */
	detectError(sendQuery("LOGIN " + dev));
}

/* NOTE: "master" is deprecated since NUT v2.8.0 in favor of "primary".
 * For the sake of old/new server/client interoperability,
 * practical implementations should try to use one and fall
 * back to the other, and only fail if both return "ERR".
 */
void TcpClient::deviceMaster(const std::string& dev)
{
	try {
		detectError(sendQuery("MASTER " + dev));
	} catch (NutException &exOrig) {
		try {
			detectError(sendQuery("PRIMARY " + dev));
		} catch (NutException &exRetry) {
			NUT_UNUSED_VARIABLE(exRetry);
			throw exOrig;
		}
	}
}

void TcpClient::devicePrimary(const std::string& dev)
{
	try {
		detectError(sendQuery("PRIMARY " + dev));
	} catch (NutException &exOrig) {
		try {
			detectError(sendQuery("MASTER " + dev));
		} catch (NutException &exRetry) {
			NUT_UNUSED_VARIABLE(exRetry);
			throw exOrig;
		}
	}
}

void TcpClient::deviceForcedShutdown(const std::string& dev)
{
	detectError(sendQuery("FSD " + dev));
}

int TcpClient::deviceGetNumLogins(const std::string& dev)
{
	std::string num = get("NUMLOGINS", dev)[0];
	return atoi(num.c_str());
}

TrackingResult TcpClient::getTrackingResult(const TrackingID& id)
{
	if (id.empty())
	{
		return TrackingResult::UNSET;
		/* TOTHINK // return TrackingResult::SUCCESS;*/
	}

	std::string result = sendQuery("GET TRACKING " + id.id());

	/* TOTHINK: Update id.setStatus() ? */
	if (result == "PENDING")
	{
		return TrackingResult::PENDING;
	}
	else if (result == "SUCCESS")
	{
		return TrackingResult::SUCCESS;
	}
	else if (result == "ERR UNKNOWN")
	{
		return TrackingResult::UNKNOWN;
	}
	else if (result == "ERR INVALID-ARGUMENT")
	{
		return TrackingResult::INVALID_ARGUMENT;
	}
	else
	{
		return TrackingResult::FAILURE;
	}
}

void TcpClient::enableTrackingModeOnce(void)
{
	if (_tracking != "ON")
	{
		setFeature(TRACKING, true);
		_tracking = sendQuery("GET " + TRACKING);
	}
}

bool TcpClient::isTrackingModeEnabled(void)
{
	if (_tracking != "ON")
	{
		_tracking = sendQuery("GET " + TRACKING);
	}
	return (_tracking == "ON");
}

TrackingResult TcpClient::waitTrackingResult(const TrackingID& id, int waitIntervalSec, int waitMaxCount)
{
	if (id.empty())
	{
		return TrackingResult::SUCCESS;
	}

	int count = 0;
	while (true)
	{
		TrackingResult res = getTrackingResult(id);
		if (res != TrackingResult::PENDING)
		{
			return res;
		}

		if (waitMaxCount > 0 && ++count >= waitMaxCount)
		{
			return TrackingResult::PENDING;
		}

		if (waitIntervalSec > 0)
		{
			// https://stackoverflow.com/questions/37358856/does-mingw-w64-support-stdthread-out-of-the-box-when-using-the-win32-threading
			// Does not work on some, not all, mingw versions (headers lack threads):
#ifndef WIN32
			std::this_thread::sleep_for(std::chrono::seconds(waitIntervalSec));
#else
			Sleep(waitIntervalSec * 1000L);
#endif
		}
		else
		{
			// Default to some small sleep if not specified but we are waiting?
			// Actually Perl/Python just loop or use the interval.
			// If interval is 0, we might busy loop, which is bad.
			// But let's follow the provided parameters.
			if (waitIntervalSec == 0) break;
		}
	}
	return TrackingResult::PENDING;
}

bool TcpClient::isFeatureEnabled(const Feature& feature)
{
	std::string result = sendQuery("GET " + feature);
	detectError(result);

	if (result == "ON")
	{
		return true;
	}
	else if (result == "OFF")
	{
		return false;
	}
	else
	{
		throw NutException("Unknown feature result " + result);
	}
}
void TcpClient::setFeature(const Feature& feature, bool status)
{
	std::string result = sendQuery("SET " + feature + " " + (status ? "ON" : "OFF"));
	detectError(result);
}

std::vector<std::string> TcpClient::get
	(const std::string& subcmd, const std::string& params)
{
	std::string req = subcmd;
	if(!params.empty())
	{
		req += " " + params;
	}
	std::string res = sendQuery("GET " + req);
	detectError(res);
	if(res.substr(0, req.size()) != req)
	{
		throw NutException("Invalid response");
	}

	return explode(res, req.size());
}

std::vector<std::vector<std::string> > TcpClient::list
	(const std::string& subcmd, const std::string& params)
{
	std::string req = subcmd;
	if(!params.empty())
	{
		req += " " + params;
	}
	std::vector<std::string> query;
	query.push_back("LIST " + req);
	sendAsyncQueries(query);
	return parseList(req);
}

std::vector<std::vector<std::string> > TcpClient::parseList
	(const std::string& req)
{
	std::string res = _socket->read();
	detectError(res);
	if(res != ("BEGIN LIST " + req))
	{
		throw NutException("Invalid response");
	}

	std::vector<std::vector<std::string> > arr;
	while(true)
	{
		res = _socket->read();
		detectError(res);
		if(res == ("END LIST " + req))
		{
			return arr;
		}
		if(res.substr(0, req.size()) == req)
		{
			arr.push_back(explode(res, req.size()));
		}
		else
		{
			throw NutException("Invalid response");
		}
	}
}

std::string TcpClient::sendQuery(const std::string& req)
{
	_socket->write(req);
	return _socket->read();
}

void TcpClient::sendAsyncQueries(const std::vector<std::string>& req)
{
	for (std::vector<std::string>::const_iterator it = req.cbegin(); it != req.cend(); ++it)
	{
		_socket->write(*it);
	}
}

void TcpClient::detectError(const std::string& req)
{
	if(req.substr(0,3)=="ERR")
	{
		throw NutException(req.substr(4));
	}
}

std::vector<std::string> TcpClient::explode(const std::string& str, size_t begin)
{
	std::vector<std::string> res;
	std::string temp;

	enum STATE {
		INIT,
		SIMPLE_STRING,
		QUOTED_STRING,
		SIMPLE_ESCAPE,
		QUOTED_ESCAPE
	} state = INIT;

	for(size_t idx=begin; idx<str.size(); ++idx)
	{
		char c = str[idx];
		switch(state)
		{
		case INIT:
			if(c==' ' /* || c=='\t' */)
			{ /* Do nothing */ }
			else if(c=='"')
			{
				state = QUOTED_STRING;
			}
			else if(c=='\\')
			{
				state = SIMPLE_ESCAPE;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
				state = SIMPLE_STRING;
			}
			break;
		case SIMPLE_STRING:
			if(c==' ' /* || c=='\t' */)
			{
				/* if(!temp.empty()) : Must not occur */
					res.push_back(temp);
				temp.clear();
				state = INIT;
			}
			else if(c=='\\')
			{
				state = SIMPLE_ESCAPE;
			}
			else if(c=='"')
			{
				/* if(!temp.empty()) : Must not occur */
					res.push_back(temp);
				temp.clear();
				state = QUOTED_STRING;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
			}
			break;
		case QUOTED_STRING:
			if(c=='\\')
			{
				state = QUOTED_ESCAPE;
			}
			else if(c=='"')
			{
				res.push_back(temp);
				temp.clear();
				state = INIT;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
			}
			break;
		case SIMPLE_ESCAPE:
			if(c=='\\' || c=='"' || c==' ' /* || c=='\t'*/)
			{
				temp += c;
			}
			else
			{
				temp += '\\' + c; // Really do this ?
			}
			state = SIMPLE_STRING;
			break;
		case QUOTED_ESCAPE:
			if(c=='\\' || c=='"')
			{
				temp += c;
			}
			else
			{
				temp += '\\' + c; // Really do this ?
			}
			state = QUOTED_STRING;
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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		default:
			/* Must not occur. */
			break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}
	}

	if(!temp.empty())
	{
		res.push_back(temp);
	}

	return res;
}

std::string TcpClient::escape(const std::string& str)
{
	std::string res = "\"";

	for(size_t n=0; n<str.size(); n++)
	{
		char c = str[n];
		if(c=='"')
			res += "\\\"";
		else if(c=='\\')
			res += "\\\\";
		else
			res += c;
	}

	res += '"';
	return res;
}

TrackingID TcpClient::sendTrackingQuery(const std::string& req, int waitIntervalSec, int waitMaxCount)
{
	if (waitIntervalSec > 0)
	{
		enableTrackingModeOnce();
	}

	std::string reply = sendQuery(req);
	detectError(reply);
	std::vector<std::string> res = explode(reply);

	if (res.size() == 1 && res[0] == "OK")
	{
		return TrackingID("");
	}
	else if (res.size() == 3 && res[0] == "OK" && res[1] == "TRACKING")
	{
		TrackingID	id = TrackingID(res[2]);
		if (waitIntervalSec > 0 && !id.empty())
		{
			TrackingResult	wtres = waitTrackingResult(id, waitIntervalSec, waitMaxCount);
			switch (wtres) {
				case TrackingResult::SUCCESS:
				case TrackingResult::UNSET:
				case TrackingResult::PENDING:
					id.setStatus(wtres);
					break;

				case TrackingResult::UNKNOWN:
				case TrackingResult::FAILURE:
				case TrackingResult::INVALID_ARGUMENT:
					id.setStatus(wtres);
					throw NutException("TRACKING query failed: " + reply);

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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
				default:
					/* Must not occur. */
					throw NutException("TRACKING query failed: " + reply);
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
			}
		}
		return id;
	}
	else
	{
		throw NutException("Unknown query result");
	}
}

/*
 *
 * Device implementation
 *
 */

Device::Device(Client* client, const std::string& name):
_client(client),
_name(name)
{
}

Device::Device(const Device& dev):
_client(dev._client),
_name(dev._name)
{
}

Device& Device::operator=(const Device& dev)
{
	// Self assignment?
	if (this==&dev)
		return *this;

	_client = dev._client;
	_name = dev._name;
	return *this;
}

Device::~Device()
{
}

std::string Device::getName()const
{
	return _name;
}

const Client* Device::getClient()const
{
	return _client;
}

Client* Device::getClient()
{
	return _client;
}

bool Device::isOk()const
{
	return _client!=nullptr && !_name.empty();
}

Device::operator bool()const
{
	return isOk();
}

bool Device::operator!()const
{
	return !isOk();
}

bool Device::operator==(const Device& dev)const
{
	return dev._client==_client && dev._name==_name;
}

bool Device::operator<(const Device& dev)const
{
	return getName()<dev.getName();
}

std::string Device::getDescription()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceDescription(getName());
}

std::vector<std::string> Device::getVariableValue(const std::string& name)
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceVariableValue(getName(), name);
}

std::map<std::string,std::vector<std::string> > Device::getVariableValues()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceVariableValues(getName());
}

std::set<std::string> Device::getVariableNames()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceVariableNames(getName());
}

std::set<std::string> Device::getRWVariableNames()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceRWVariableNames(getName());
}

TrackingID Device::setVariable(const std::string& name, const std::string& value, int waitIntervalSec, int waitMaxCount)
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->setDeviceVariable(getName(), name, value, waitIntervalSec, waitMaxCount);
}

TrackingID Device::setVariable(const std::string& name, const std::vector<std::string>& values, int waitIntervalSec, int waitMaxCount)
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->setDeviceVariable(getName(), name, values, waitIntervalSec, waitMaxCount);
}

Variable Device::getVariable(const std::string& name)
{
	if (!isOk()) throw NutException("Invalid device");
	if(getClient()->hasDeviceVariable(getName(), name))
		return Variable(this, name);
	else
		return Variable(nullptr, "");
}

std::set<Variable> Device::getVariables()
{
	std::set<Variable> set;
	if (!isOk()) throw NutException("Invalid device");

	std::set<std::string> names = getClient()->getDeviceVariableNames(getName());
	for(std::set<std::string>::iterator it=names.begin(); it!=names.end(); ++it)
	{
		set.insert(Variable(this, *it));
	}

	return set;
}

std::set<Variable> Device::getRWVariables()
{
	std::set<Variable> set;
	if (!isOk()) throw NutException("Invalid device");

	std::set<std::string> names = getClient()->getDeviceRWVariableNames(getName());
	for(std::set<std::string>::iterator it=names.begin(); it!=names.end(); ++it)
	{
		set.insert(Variable(this, *it));
	}

	return set;
}

std::set<std::string> Device::getCommandNames()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceCommandNames(getName());
}

std::set<Command> Device::getCommands()
{
	std::set<Command> cmds;

	std::set<std::string> res = getCommandNames();
	for(std::set<std::string>::iterator it=res.begin(); it!=res.end(); ++it)
	{
		cmds.insert(Command(this, *it));
	}

	return cmds;
}

Command Device::getCommand(const std::string& name)
{
	if (!isOk()) throw NutException("Invalid device");
	if(getClient()->hasDeviceCommand(getName(), name))
		return Command(this, name);
	else
		return Command(nullptr, "");
}

TrackingID Device::executeCommand(const std::string& name, const std::string& param, int waitIntervalSec, int waitMaxCount)
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->executeDeviceCommand(getName(), name, param, waitIntervalSec, waitMaxCount);
}

std::set<std::string> Device::getClients()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->deviceGetClients(getName());
}

void Device::login()
{
	if (!isOk()) throw NutException("Invalid device");
	getClient()->deviceLogin(getName());
}

/* Note: "master" is deprecated, but supported
 * for mixing old/new client/server combos: */
void Device::master()
{
	if (!isOk()) throw NutException("Invalid device");

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	getClient()->deviceMaster(getName());
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#pragma GCC diagnostic pop
#endif
}

void Device::becomePrimary()
{
	if (!isOk()) throw NutException("Invalid device");
	getClient()->devicePrimary(getName());
}

void Device::forcedShutdown()
{
	if (!isOk()) throw NutException("Invalid device");
	getClient()->deviceForcedShutdown(getName());
}

int Device::getNumLogins()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->deviceGetNumLogins(getName());
}

/*
 *
 * Variable implementation
 *
 */

Variable::Variable(Device* dev, const std::string& name):
_device(dev),
_name(name)
{
}

Variable::Variable(const Variable& var):
_device(var._device),
_name(var._name)
{
}

Variable& Variable::operator=(const Variable& var)
{
	// Self assignment?
	if (this==&var)
		return *this;

	_device = var._device;
	_name = var._name;
	return *this;
}

Variable::~Variable()
{
}

std::string Variable::getName()const
{
	return _name;
}

const Device* Variable::getDevice()const
{
	return _device;
}

Device* Variable::getDevice()
{
	return _device;
}

bool Variable::isOk()const
{
	return _device!=nullptr && !_name.empty();

}

Variable::operator bool()const
{
	return isOk();
}

bool Variable::operator!()const
{
	return !isOk();
}

bool Variable::operator==(const Variable& var)const
{
	return var._device==_device && var._name==_name;
}

bool Variable::operator<(const Variable& var)const
{
	return getName()<var.getName();
}

std::vector<std::string> Variable::getValue()
{
	return getDevice()->getClient()->getDeviceVariableValue(getDevice()->getName(), getName());
}

std::string Variable::getDescription()
{
	return getDevice()->getClient()->getDeviceVariableDescription(getDevice()->getName(), getName());
}

TrackingID Variable::setValue(const std::string& value, int waitIntervalSec, int waitMaxCount)
{
	return getDevice()->setVariable(getName(), value, waitIntervalSec, waitMaxCount);
}

TrackingID Variable::setValues(const std::vector<std::string>& values, int waitIntervalSec, int waitMaxCount)
{
	return getDevice()->setVariable(getName(), values, waitIntervalSec, waitMaxCount);
}


/*
 *
 * Command implementation
 *
 */

Command::Command(Device* dev, const std::string& name):
_device(dev),
_name(name)
{
}

Command::Command(const Command& cmd):
_device(cmd._device),
_name(cmd._name)
{
}

Command& Command::operator=(const Command& cmd)
{
	// Self assignment?
	if (this==&cmd)
		return *this;

	_device = cmd._device;
	_name = cmd._name;
	return *this;
}

Command::~Command()
{
}

std::string Command::getName()const
{
	return _name;
}

const Device* Command::getDevice()const
{
	return _device;
}

Device* Command::getDevice()
{
	return _device;
}

bool Command::isOk()const
{
	return _device!=nullptr && !_name.empty();
}

Command::operator bool()const
{
	return isOk();
}

bool Command::operator!()const
{
	return !isOk();
}

bool Command::operator==(const Command& cmd)const
{
	return cmd._device==_device && cmd._name==_name;
}

bool Command::operator<(const Command& cmd)const
{
	return getName()<cmd.getName();
}

std::string Command::getDescription()
{
	return getDevice()->getClient()->getDeviceCommandDescription(getDevice()->getName(), getName());
}

TrackingID Command::execute(const std::string& param, int waitIntervalSec, int waitMaxCount)
{
	return getDevice()->executeCommand(getName(), param, waitIntervalSec, waitMaxCount);
}

} /* namespace nut */


/**
 * C nutclient API.
 */
extern "C" {


strarr strarr_alloc(size_t count)
{
	strarr arr = static_cast<strarr>(xcalloc(count+1, sizeof(char*)));

	if (arr == nullptr) {
		throw nut::NutException("Out of memory");
	}

	arr[count] = nullptr;
	return arr;
}

void strarr_free(strarr arr)
{
	char** pstr = arr;
	while(*pstr!=nullptr)
	{
		free(*pstr);
		++pstr;
	}
	free(arr);
}

strarr stringset_to_strarr(const std::set<std::string>& strset)
{
	strarr arr = strarr_alloc(strset.size());
	strarr pstr = arr;
	for(std::set<std::string>::const_iterator it=strset.begin(); it!=strset.end(); ++it)
	{
		*pstr = xstrdup(it->c_str());
		pstr++;
	}
	return arr;
}

strarr stringvector_to_strarr(const std::vector<std::string>& strset)
{
	strarr arr = strarr_alloc(strset.size());
	strarr pstr = arr;
	for(std::vector<std::string>::const_iterator it=strset.begin(); it!=strset.end(); ++it)
	{
		*pstr = xstrdup(it->c_str());
		pstr++;
	}
	return arr;
}

NUTCLIENT_TCP_t nutclient_tcp_create_client(const char* host, uint16_t port)
{
	nut::TcpClient* client = new nut::TcpClient;
	try
	{
		client->connect(host, port);
		return static_cast<NUTCLIENT_TCP_t>(client);
	}
	catch(nut::NutException& ex)
	{
		// TODO really catch it
		NUT_UNUSED_VARIABLE(ex);
		delete client;
		return nullptr;
	}

}

int nutclient_tcp_get_ssl_caps(void) { return nut::TcpClient::getSslCaps(); }

NUTCLIENT_TCP_t nutclient_tcp_create_client_ssl_OpenSSL(const char* host, uint16_t port, int tryssl, int forcessl, int certverify, const char *ca_path, const char *ca_file, const char *cert_file, const char *key_file, const char *key_pass, const char *certident_name, const char *certhost_addr, const char *certhost_name)
{
	nut::TcpClient* client = new nut::TcpClient;
	try
	{
		client->setSSLConfig(nut::SSLConfig_OpenSSL(
			(forcessl > 0), certverify, ca_path, ca_file, cert_file, key_file, key_pass, certident_name, certhost_addr, certhost_name
		));
		client->connect(host, port, tryssl != 0);
		return static_cast<NUTCLIENT_TCP_t>(client);
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		delete client;
		return nullptr;
	}
}

void nutclient_tcp_set_ssl_config_OpenSSL(NUTCLIENT_TCP_t client, int forcessl, int certverify, const char *ca_path, const char *ca_file, const char *cert_file, const char *key_file, const char *key_pass, const char *certident_name, const char *certhost_addr, const char *certhost_name)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSSLConfig(nut::SSLConfig_OpenSSL((
				forcessl > 0), certverify, ca_path, ca_file, cert_file, key_file, key_pass, certident_name, certhost_addr, certhost_name
			));
		}
	}
}

NUTCLIENT_TCP_t nutclient_tcp_create_client_ssl_NSS(const char* host, uint16_t port, int tryssl, int forcessl, int certverify, const char *certstore_path, const char *certstore_pass, const char *certstore_prefix, const char *certhost_addr, const char *certhost_name, const char *certident_name)
{
	nut::TcpClient* client = new nut::TcpClient;
	try
	{
		client->setSSLConfig(nut::SSLConfig_NSS(
			(forcessl > 0), certverify, certstore_path, certstore_pass, certstore_prefix, certhost_addr, certhost_name, certident_name
		));
		client->connect(host, port, tryssl != 0);
		return static_cast<NUTCLIENT_TCP_t>(client);
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		delete client;
		return nullptr;
	}
}

void nutclient_tcp_set_ssl_config_NSS(NUTCLIENT_TCP_t client, int forcessl, int certverify, const char *certstore_path, const char *certstore_pass, const char *certstore_prefix, const char *certhost_addr, const char *certhost_name, const char *certident_name)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSSLConfig(nut::SSLConfig_NSS(
				(forcessl > 0), certverify, certstore_path, certstore_pass, certstore_prefix, certhost_addr, certhost_name, certident_name
			));
		}
	}
}

void nutclient_destroy(NUTCLIENT_t client)
{
	if(client)
	{
		delete static_cast<nut::Client*>(client);
	}
}

int nutclient_tcp_is_connected(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->isConnected() ? 1 : 0;
		}
	}
	return 0;
}

void nutclient_tcp_disconnect(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->disconnect();
		}
	}
}

int nutclient_tcp_reconnect(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			try
			{
				cl->connect();
				return 0;
			}
			catch(...){}
		}
	}
	return -1;
}

int nutclient_tcp_is_ssl(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->isSSL() ? 1 : 0;
		}
	}
	return 0;
}

void nutclient_tcp_set_ssl_try(NUTCLIENT_TCP_t client, int tryssl)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslTry(tryssl ? true : false);
		}
	}
}

int nutclient_tcp_get_ssl_try(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslTry() ? 1 : 0;
		}
	}
	return 0;
}

void nutclient_tcp_set_ssl_force(NUTCLIENT_TCP_t client, int forcessl)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslForce(forcessl ? true : false);
		}
	}
}

int nutclient_tcp_get_ssl_force(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslForce() ? 1 : 0;
		}
	}
	return 0;
}

void nutclient_tcp_set_ssl_certverify(NUTCLIENT_TCP_t client, int certverify)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCertVerify(certverify);
		}
	}
}

int nutclient_tcp_get_ssl_certverify(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCertVerify();
		}
	}
	return -1;
}

void nutclient_tcp_set_ssl_capath(NUTCLIENT_TCP_t client, const char* ca_path)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCAPath(ca_path);
		}
	}
}

const char* nutclient_tcp_get_ssl_capath(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCAPath().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_cafile(NUTCLIENT_TCP_t client, const char* ca_file)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCAFile(ca_file);
		}
	}
}

const char* nutclient_tcp_get_ssl_cafile(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCAFile().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_certfile(NUTCLIENT_TCP_t client, const char* cert_file)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCertFile(cert_file);
		}
	}
}

const char* nutclient_tcp_get_ssl_certfile(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCertFile().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_keyfile(NUTCLIENT_TCP_t client, const char* key_file)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslKeyFile(key_file);
		}
	}
}

const char* nutclient_tcp_get_ssl_keyfile(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslKeyFile().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_keypass(NUTCLIENT_TCP_t client, const char* key_pass)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslKeyPass(key_pass);
		}
	}
}

const char* nutclient_tcp_get_ssl_keypass(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslKeyPass().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_certstore_path(NUTCLIENT_TCP_t client, const char* certstore_path)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCertstorePath(certstore_path);
		}
	}
}

const char* nutclient_tcp_get_ssl_certstore_path(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCertstorePath().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_certstore_prefix(NUTCLIENT_TCP_t client, const char* certstore_prefix)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCertstorePrefix(certstore_prefix);
		}
	}
}

const char* nutclient_tcp_get_ssl_certstore_prefix(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCertstorePrefix().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_certident_name(NUTCLIENT_TCP_t client, const char* certident_name)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCertIdentName(certident_name);
		}
	}
}

const char* nutclient_tcp_get_ssl_certident_name(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCertIdentName().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_certhost_addr(NUTCLIENT_TCP_t client, const char* certhost_addr)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCertHostAddr(certhost_addr);
		}
	}
}

const char* nutclient_tcp_get_ssl_certhost_addr(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCertHostAddr().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_ssl_certhost_name(NUTCLIENT_TCP_t client, const char* certhost_name)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setSslCertHostName(certhost_name);
		}
	}
}

const char* nutclient_tcp_get_ssl_certhost_name(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getSslCertHostName().c_str();
		}
	}
	return nullptr;
}

void nutclient_tcp_set_timeout(NUTCLIENT_TCP_t client, time_t timeout)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setTimeout(timeout);
		}
	}
}

time_t nutclient_tcp_get_timeout(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getTimeout();
		}
	}
	return -1;
}

void nutclient_authenticate(NUTCLIENT_t client, const char* login, const char* passwd)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->authenticate(login, passwd);
			}
			catch(...){}
		}
	}
}

void nutclient_logout(NUTCLIENT_t client)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->logout();
			}
			catch(...){}
		}
	}
}

void nutclient_device_login(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->deviceLogin(dev);
			}
			catch(...){}
		}
	}
}

int nutclient_get_device_num_logins(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->deviceGetNumLogins(dev);
			}
			catch(...){}
		}
	}
	return -1;
}

/* Note: "master" is deprecated, but supported
 * for mixing old/new client/server combos: */
void nutclient_device_master(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
				cl->deviceMaster(dev);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#pragma GCC diagnostic pop
#endif
			}
			catch(...){}
		}
	}
}

void nutclient_device_primary(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->devicePrimary(dev);
			}
			catch(...){}
		}
	}
}

void nutclient_device_forced_shutdown(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->deviceForcedShutdown(dev);
			}
			catch(...){}
		}
	}
}

strarr nutclient_get_devices(NUTCLIENT_t client)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceNames());
			}
			catch(...){}
		}
	}
	return nullptr;
}

int nutclient_has_device(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->hasDevice(dev)?1:0;
			}
			catch(...){}
		}
	}
	return 0;
}

char* nutclient_get_device_description(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return xstrdup(cl->getDeviceDescription(dev).c_str());
			}
			catch(...){}
		}
	}
	return nullptr;
}

strarr nutclient_get_device_variables(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceVariableNames(dev));
			}
			catch(...){}
		}
	}
	return nullptr;
}

strarr nutclient_get_device_rw_variables(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceRWVariableNames(dev));
			}
			catch(...){}
		}
	}
	return nullptr;
}

int nutclient_has_device_variable(NUTCLIENT_t client, const char* dev, const char* var)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->hasDeviceVariable(dev, var)?1:0;
			}
			catch(...){}
		}
	}
	return 0;
}

char* nutclient_get_device_variable_description(NUTCLIENT_t client, const char* dev, const char* var)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return xstrdup(cl->getDeviceVariableDescription(dev, var).c_str());
			}
			catch(...){}
		}
	}
	return nullptr;
}

strarr nutclient_get_device_variable_values(NUTCLIENT_t client, const char* dev, const char* var)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringvector_to_strarr(cl->getDeviceVariableValue(dev, var));
			}
			catch(...){}
		}
	}
	return nullptr;
}

void nutclient_set_device_variable_value(NUTCLIENT_t client, const char* dev, const char* var, const char* value)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->setDeviceVariable(dev, var, value, -1, -1);
			}
			catch(...){}
		}
	}
}

void nutclient_set_device_variable_value_wait(NUTCLIENT_t client, const char* dev, const char* var, const char* value, int waitIntervalSec, int waitMaxCount)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->setDeviceVariable(dev, var, value, waitIntervalSec, waitMaxCount);
			}
			catch(...){}
		}
	}
}

void nutclient_set_device_variable_values(NUTCLIENT_t client, const char* dev, const char* var, const strarr values)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				std::vector<std::string> vals;
				strarr pstr = static_cast<strarr>(values);
				while(*pstr)
				{
					vals.push_back(std::string(*pstr));
					++pstr;
				}

				cl->setDeviceVariable(dev, var, vals);
			}
			catch(...){}
		}
	}
}

void nutclient_set_device_variable_values_wait(NUTCLIENT_t client, const char* dev, const char* var, const strarr values, int waitIntervalSec, int waitMaxCount)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				std::vector<std::string> vals;
				strarr pstr = static_cast<strarr>(values);
				while(*pstr)
				{
					vals.push_back(std::string(*pstr));
					++pstr;
				}

				cl->setDeviceVariable(dev, var, vals, waitIntervalSec, waitMaxCount);
			}
			catch(...){}
		}
	}
}

strarr nutclient_get_device_commands(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceCommandNames(dev));
			}
			catch(...){}
		}
	}
	return nullptr;
}

int nutclient_has_device_command(NUTCLIENT_t client, const char* dev, const char* cmd)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->hasDeviceCommand(dev, cmd)?1:0;
			}
			catch(...){}
		}
	}
	return 0;
}

char* nutclient_get_device_command_description(NUTCLIENT_t client, const char* dev, const char* cmd)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return xstrdup(cl->getDeviceCommandDescription(dev, cmd).c_str());
			}
			catch(...){}
		}
	}
	return nullptr;
}

void nutclient_execute_device_command(NUTCLIENT_t client, const char* dev, const char* cmd, const char* param)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->executeDeviceCommand(dev, cmd, param, -1, -1);
			}
			catch(...){}
		}
	}
}

void nutclient_execute_device_command_wait(NUTCLIENT_t client, const char* dev, const char* cmd, const char* param, int waitIntervalSec, int waitMaxCount)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->executeDeviceCommand(dev, cmd, param, waitIntervalSec, waitMaxCount);
			}
			catch(...){}
		}
	}
}

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT
#pragma GCC diagnostic pop
#endif

} /* extern "C" */
