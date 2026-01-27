dnl Check for Mozilla NSS (LIBNSS) compiler flags. On success, set 
dnl nut_have_libnss="yes" and nut_ssl_lib="Mozilla NSS", and define WITH_SSL,
dnl WITH_NSS, LIBSSL_CFLAGS and LIBSSL_LIBS. On failure, set nut_have_libnss="no".
dnl This macro can be run multiple times, but will do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBNSS],
[
if test -z "${nut_have_libnss_seen}"; then
	nut_have_libnss_seen=yes
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
		[AC_MSG_CHECKING(for Mozilla NSS version via pkg-config)
		 NSS_VERSION="`$PKG_CONFIG --silence-errors --modversion nss 2>/dev/null`"
		 if test "$?" != "0" -o -z "${NSS_VERSION}"; then
		    NSS_VERSION="none"
		 fi
		 AC_MSG_RESULT(${NSS_VERSION} found)
		],
		[NSS_VERSION="none"
		 AC_MSG_NOTICE([can not check libnss settings via pkg-config])
		]
	)

	AS_IF([test x"$NSS_VERSION" != xnone],
		[depCFLAGS="`$PKG_CONFIG --silence-errors --cflags nss 2>/dev/null`"
		 depLIBS="`$PKG_CONFIG --silence-errors --libs nss 2>/dev/null`"
		 depREQUIRES="nss"
		 depCFLAGS_SOURCE="pkg-config"
		 depLIBS_SOURCE="pkg-config"
		 depREQUIRES_SOURCE="default(pkg-config)"
		],
		[depCFLAGS=""
		 depLIBS="-lnss3 -lnssutil3 -lsmime3 -lssl3 -lplds4 -lplc4 -lnspr4"
		 depREQUIRES="nss"
		 depCFLAGS_SOURCE="default"
		 depLIBS_SOURCE="default"
		 depREQUIRES_SOURCE="default"
		]
	)

	dnl allow overriding NSS settings if the user knows best
	AC_MSG_CHECKING(for Mozilla NSS cflags)
	NUT_ARG_WITH_LIBOPTS_INCLUDES([nss], [auto], [Mozilla NSS])
	AS_CASE([${nut_with_nss_includes}],
		[auto], [],	dnl Keep what we had found above
			[depCFLAGS="${nut_with_nss_includes}"
			 depCFLAGS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

	AC_MSG_CHECKING(for Mozilla NSS ldflags)
	NUT_ARG_WITH_LIBOPTS_LIBS([nss], [auto], [Mozilla NSS])
	AS_CASE([${nut_with_nss_libs}],
		[auto], [],	dnl Keep what we had found above
			[depLIBS="${nut_with_nss_libs}"
			 depLIBS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

	dnl check if NSS is usable: we need both the runtime and headers
	dnl NOTE that caller may have to specify PKG_CONFIG_PATH including
	dnl their bitness variant if it is not prioritized in their default
	dnl setting built in by OS distribution; the .../pkgconfig/nss.pc
	dnl tends to specify the libdir which is CPU Arch dependent.
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	REQUIRES="${REQUIRES_ORIG} ${depREQUIRES}"
	AC_CHECK_FUNCS(NSS_Init, [nut_have_libnss=yes], [nut_have_libnss=no])
	dnl libc6 also provides an nss.h file, so also check for ssl.h
	AC_CHECK_HEADERS([nss.h ssl.h], [], [nut_have_libnss=no], [AC_INCLUDES_DEFAULT])

	if test "${nut_have_libnss}" = "yes"; then
		nut_with_ssl="yes"
		nut_ssl_lib="(Mozilla NSS)"
		AC_DEFINE(WITH_SSL, 1, [Define to enable SSL support])
		AC_DEFINE(WITH_NSS, 1, [Define to enable SSL support using Mozilla NSS])

		dnl # Repeat some tricks from nut_compiler_family.m4
		AS_IF([test "x$cross_compiling" != xyes], [
			AS_IF([test "x$CLANGCC" = xyes -o "x$GCC" = xyes], [
				dnl # CLANG dislikes MPS headers for use of reserved
				dnl # identifiers (starting with underscores, some
				dnl # with upper-case letters afterwards - oh, the
				dnl # blasphemers!)
				addCFLAGS=""
				for TOKEN in ${depCFLAGS} ; do
					case "${TOKEN}" in
						-I*)	TOKENDIR="`echo \"$TOKEN\" | sed 's,^-I,,'`"
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

		dnl # See tools/nut-scanner/Makefile.am
		dnl # FIXME: Handle "-R /path" tokens, are they anywhere?
		LIBSSL_LDFLAGS_RPATH=""
		for TOKEN in ${LIBSSL_LIBS} ; do
			case "$TOKEN" in
			-R*)
				LIBSSL_LDFLAGS_RPATH="$LIBSSL_LDFLAGS_RPATH $TOKEN"
				dnl ### LIBSSL_LDFLAGS_RPATH="$LIBSSL_LDFLAGS_RPATH -Wl,runpath,`echo $TOKEN | sed 's,^-R *,,'`"
				LIBSSL_LDFLAGS_RPATH="$LIBSSL_LDFLAGS_RPATH -Wl,-rpath,`echo $TOKEN | sed 's,^-R *,,'`"
				;;
			esac
		done
dnl		if test x"$LIBSSL_LDFLAGS_RPATH" != x ; then
dnl			LIBSSL_LDFLAGS_RPATH="--enable-new-dtags $LIBSSL_LDFLAGS_RPATH"
dnl		fi

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
