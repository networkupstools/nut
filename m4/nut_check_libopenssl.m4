dnl Check for OpenSSL (LIBOPENSSL) compiler flags. On success, set 
dnl nut_have_openssl="yes" and nut_ssl_lib="OpenSSL", and define WITH_SSL,
dnl WITH_OPENSSL, LIBSSL_CFLAGS and LIBSSL_LIBS. On failure, set
dnl nut_have_libssl="no".
dnl This macro can be run multiple times, but will do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBOPENSSL], 
[
if test -z "${nut_have_libopenssl_seen}"; then
	nut_have_libopenssl_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	REQUIRES_ORIG="${REQUIRES}"

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING(for OpenSSL version via pkg-config)
		 OPENSSL_VERSION="`$PKG_CONFIG --silence-errors --modversion openssl 2>/dev/null`"
		 if test "$?" != "0" -o -z "${OPENSSL_VERSION}"; then
		    OPENSSL_VERSION="none"
		 fi
		 AC_MSG_RESULT(${OPENSSL_VERSION} found)
		],
		[OPENSSL_VERSION="none"
		 AC_MSG_NOTICE([can not check OpenSSL settings via pkg-config])
		]
	)

	AS_IF([test x"$OPENSSL_VERSION" != xnone],
		[CFLAGS="`$PKG_CONFIG --silence-errors --cflags openssl 2>/dev/null`"
		 LIBS="`$PKG_CONFIG --silence-errors --libs openssl 2>/dev/null`"
		 REQUIRES="openssl"
		],
		[CFLAGS=""
		 LIBS="-lssl -lcrypto"
		 REQUIRES="openssl"
		]
	)

	dnl allow overriding OpenSSL settings if the user knows best
	AC_MSG_CHECKING(for OpenSSL cflags)
	AC_ARG_WITH(openssl-includes,
		AS_HELP_STRING([@<:@--with-openssl-includes=CFLAGS@:>@], [include flags for the OpenSSL library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-openssl-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for OpenSSL ldflags)
	AC_ARG_WITH(openssl-libs,
		AS_HELP_STRING([@<:@--with-openssl-libs=LIBS@:>@], [linker flags for the OpenSSL library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-openssl-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LIBS}])

	dnl check if openssl is usable
	AC_CHECK_HEADERS(openssl/ssl.h, [nut_have_openssl=yes], [nut_have_openssl=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(SSL_CTX_new, [], [nut_have_openssl=no])

	if test "${nut_have_openssl}" = "yes"; then
		nut_with_ssl="yes"
		nut_ssl_lib="(OpenSSL)"
		AC_DEFINE(WITH_SSL, 1, [Define to enable SSL support])
		AC_DEFINE(WITH_OPENSSL, 1, [Define to enable SSL support using OpenSSL])
		LIBSSL_CFLAGS="${CFLAGS}"
		LIBSSL_LIBS="${LIBS}"
		LIBSSL_REQUIRES="${REQUIRES}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
	REQUIRES="${REQUIRES_ORIG}"
fi
])
