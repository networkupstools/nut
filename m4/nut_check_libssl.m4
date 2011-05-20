dnl Check for LIBSSL compiler flags. On success, set nut_have_libssl="yes"
dnl and set LIBSSL_CFLAGS and LIBSSL_LIBS. On failure, set
dnl nut_have_libssl="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBSSL], 
[
if test -z "${nut_have_libssl_seen}"; then
	nut_have_libssl_seen=yes

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AC_MSG_CHECKING(for openssl version via pkg-config)
	OPENSSL_VERSION="`pkg-config --silence-errors --modversion openssl 2>/dev/null`"
	if test "$?" = "0" -a -n "${OPENSSL_VERSION}"; then
		CFLAGS="`pkg-config --silence-errors --cflags openssl 2>/dev/null`"
		LIBS="`pkg-config --silence-errors --libs openssl 2>/dev/null`"
	else
		OPENSSL_VERSION="none"
		CFLAGS=""
		LIBS="-lssl -lcrypto"
	fi
	AC_MSG_RESULT(${OPENSSL_VERSION} found)

	dnl allow overriding openssl settings if the user knows best
	AC_MSG_CHECKING(for openssl cflags)
	AC_ARG_WITH(ssl-includes,
		AS_HELP_STRING([@<:@--with-ssl-includes=CFLAGS@:>@], [include flags for the OpenSSL library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-ssl-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for openssl ldflags)
	AC_ARG_WITH(ssl-libs,
		AS_HELP_STRING([@<:@--with-ssl-libs=LIBS@:>@], [linker flags for the OpenSSL library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-ssl-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LIBS}])

	dnl check if openssl is usable
	AC_CHECK_HEADERS(openssl/ssl.h, [nut_have_libssl=yes], [nut_have_libssl=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(SSL_library_init, [], [nut_have_libssl=no])

	if test "${nut_have_libssl}" = "yes"; then
		AC_DEFINE(HAVE_SSL, 1, [Define to enable SSL development code])
		LIBSSL_CFLAGS="${CFLAGS}"
		LIBSSL_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
