dnl Check for LIBSSL compiler flags. On success, set nut_have_libssl="yes"
dnl and set LIBSSL_CFLAGS and LIBSSL_LDFLAGS. On failure, set
dnl nut_have_libssl="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBSSL], 
[
if test -z "${nut_have_libssl_seen}"; then
	nut_have_libssl_seen=yes

	dnl save CFLAGS and LDFLAGS
	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	AC_MSG_CHECKING(for openssl version via pkg-config)
	OPENSSL_VERSION=`pkg-config --silence-errors --modversion openssl`
	if test "$?" = "0"; then
		AC_MSG_RESULT(${OPENSSL_VERSION} found)
		CFLAGS="`pkg-config --silence-errors --cflags openssl`"
		LDFLAGS="`pkg-config --silence-errors --libs openssl`"
	else
		AC_MSG_RESULT(not found)
		CFLAGS=""
		LDFLAGS="-lssl -lcrypto"
	fi

	dnl allow overriding openssl settings if the user knows best
	AC_MSG_CHECKING(for openssl cflags)
	AC_ARG_WITH(ssl-includes, [
		AS_HELP_STRING([--with-ssl-includes=CFLAGS], [include flags for the OpenSSL library])
	], [
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
	AC_ARG_WITH(ssl-libs, [
		AS_HELP_STRING([--with-ssl-libs=LDFLAGS], [linker flags for the OpenSSL library])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-ssl-libs - see docs/configure.txt)
			;;
		*)
			LDFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LDFLAGS}])

	dnl check if openssl is usable
	AC_CHECK_HEADERS(openssl/ssl.h, [nut_have_libssl=yes], [nut_have_libssl=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(SSL_library_init, [], [nut_have_libssl=no])

	if test "${nut_have_libssl}" = "yes"; then
		AC_DEFINE(HAVE_SSL, 1, [Define to enable SSL development code])
		LIBSSL_CFLAGS="${CFLAGS}"
		LIBSSL_LDFLAGS="${LDFLAGS}"
	fi

	dnl restore original CFLAGS and LDFLAGS
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
fi
])
