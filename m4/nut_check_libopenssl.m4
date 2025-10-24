dnl Check for OpenSSL (LIBOPENSSL) compiler flags. On success, set 
dnl nut_have_openssl="yes" and nut_ssl_lib="OpenSSL", and define WITH_SSL,
dnl WITH_OPENSSL, LIBSSL_CFLAGS and LIBSSL_LIBS. On failure, set
dnl nut_have_libssl="no".
dnl This macro can be run multiple times, but will do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBOPENSSL],
[
if test -z "${nut_have_libopenssl_seen}"; then
	nut_have_libopenssl_seen=yes
	AC_REQUIRE([NUT_CHECK_PKGCONFIG])

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	REQUIRES_ORIG="${REQUIRES}"
	CFLAGS=""
	LIBS=""
	REQUIRES=""
	depCFLAGS=""
	depCFLAGS_SOURCE=""
	depLIBS=""
	depLIBS_SOURCE=""
	depREQUIRES=""
	depREQUIRES_SOURCE=""

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
		[depCFLAGS="`$PKG_CONFIG --silence-errors --cflags openssl 2>/dev/null`"
		 depLIBS="`$PKG_CONFIG --silence-errors --libs openssl 2>/dev/null`"
		 depREQUIRES="openssl"
		 depCFLAGS_SOURCE="pkg-config"
		 depLIBS_SOURCE="pkg-config"
		 depREQUIRES_SOURCE="default(pkg-config)"
		],
		[depCFLAGS=""
		 depLIBS="-lssl -lcrypto"
		 depREQUIRES="openssl"
		 depCFLAGS_SOURCE="default"
		 depLIBS_SOURCE="default"
		 depREQUIRES_SOURCE="default"
		]
	)

	dnl allow overriding OpenSSL settings if the user knows best
	AC_MSG_CHECKING(for OpenSSL cflags)
	NUT_ARG_WITH_LIBOPTS_INCLUDES([OpenSSL], [auto])
	AS_CASE([${nut_with_openssl_includes}],
		[auto], [],	dnl Keep what we had found above
			[depCFLAGS="${nut_with_openssl_includes}"
			 depCFLAGS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

	AC_MSG_CHECKING(for OpenSSL ldflags)
	NUT_ARG_WITH_LIBOPTS_LIBS([OpenSSL], [auto])
	AS_CASE([${nut_with_openssl_libs}],
		[auto], [],	dnl Keep what we had found above
			[depLIBS="${nut_with_openssl_libs}"
			 depLIBS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

	dnl check if openssl is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	REQUIRES="${REQUIRES_ORIG} ${depREQUIRES}"
	AC_CHECK_HEADERS(openssl/ssl.h, [nut_have_openssl=yes], [nut_have_openssl=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(SSL_CTX_new, [], [nut_have_openssl=no])

	if test "${nut_have_openssl}" = "yes"; then
		nut_with_ssl="yes"
		nut_ssl_lib="(OpenSSL)"
		AC_DEFINE(WITH_SSL, 1, [Define to enable SSL support])
		AC_DEFINE(WITH_OPENSSL, 1, [Define to enable SSL support using OpenSSL])

		dnl # Repeat some tricks from nut_compiler_family.m4
		AS_IF([test "x$cross_compiling" != xyes], [
			AS_IF([test "x$CLANGCC" = xyes -o "x$GCC" = xyes], [
				dnl # This was hit on OmniOS Extra packaging repo
				dnl   /usr/ssl-3/include/openssl/safestack.h:205:1:
				dnl   error: cast from 'sk_OPENSSL_STRING_freefunc'
				dnl   (aka 'void (*)(char *)') to 'OPENSSL_sk_freefunc'
				dnl   (aka 'void (*)(void *)') converts to incompatible
				dnl   function type [-Werror,-Wcast-function-type-strict]
				dnl # Below: Pick out -I... args of the depCFLAGS
				dnl # to check locations that actually matter for
				dnl # the build
				addCFLAGS=""
				for TOKEN in ${depCFLAGS} ; do
					case "${TOKEN}" in
						-I*)	TOKENDIR="`echo "$TOKEN" | sed 's,^-I,,'`"
							case " ${CFLAGS} ${addCFLAGS} " in
								*" -isystem $TOKENDIR "*) ;;
								*) addCFLAGS="${addCFLAGS} -isystem $TOKENDIR" ;;
							esac ;;
					esac
				done
				test -z "${addCFLAGS}" || depCFLAGS="${depCFLAGS} ${addCFLAGS}"
				unset addCFLAGS
			])
		])

		LIBSSL_CFLAGS="${depCFLAGS}"
		LIBSSL_LIBS="${depLIBS}"
		LIBSSL_REQUIRES="${depREQUIRES}"

		dnl # See tools/nut-scanner/Makefile.am and also
		dnl # nut_check_libnss.m4 if there are custom RPATHs
		LIBSSL_LDFLAGS_RPATH=""
	fi

	unset depCFLAGS
	unset depLIBS
	unset depREQUIRES
	unset depCFLAGS_SOURCE
	unset depLIBS_SOURCE
	unset depREQUIRES_SOURCE

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
	REQUIRES="${REQUIRES_ORIG}"
fi
])
