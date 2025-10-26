dnl Check for LIBREGEX (and, if found, fill 'nut_usb_lib' with its
dnl approximate version) and its compiler flags. On success, set
dnl nut_have_libusb="yes" and set LIBREGEX_CFLAGS and LIBREGEX_LIBS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBREGEX],
[
if test -z "${nut_have_libregex_seen}"; then
	nut_have_libregex_seen=yes
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

	dnl Actually did not see it in any systems' pkg-config info...
	dnl Part of standard footprint?
	LIBREGEX_MODULE=""
	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING(for libregex version via pkg-config)
		 LIBREGEX_VERSION="`$PKG_CONFIG --silence-errors --modversion regex 2>/dev/null`"
		 if test "$?" != "0" -o -z "${LIBREGEX_VERSION}"; then
		    LIBREGEX_VERSION="`$PKG_CONFIG --silence-errors --modversion libregex 2>/dev/null`"
		    if test "$?" != "0" -o -z "${LIBREGEX_VERSION}"; then
		        LIBREGEX_VERSION="none"
		    else
		        LIBREGEX_MODULE="libregex"
		    fi
		 else
		    LIBREGEX_MODULE="regex"
		 fi
		 AC_MSG_RESULT(${LIBREGEX_VERSION} found)
		],
		[LIBREGEX_VERSION="none"
			 AC_MSG_NOTICE([can not check libregex settings via pkg-config])
		]
	)

	AS_IF([test x"$LIBREGEX_VERSION" != xnone && test x"$LIBREGEX_MODULE" != x],
		[depCFLAGS="`$PKG_CONFIG --silence-errors --cflags "${LIBREGEX_MODULE}" 2>/dev/null`"
		 depLIBS="`$PKG_CONFIG --silence-errors --libs "${LIBREGEX_MODULE}" 2>/dev/null`"
		 depREQUIRES="${LIBREGEX_MODULE}"
		 depCFLAGS_SOURCE="pkg-config"
		 depLIBS_SOURCE="pkg-config"
		 depREQUIRES_SOURCE="pkg-config"
		],
		[depCFLAGS=""
		 depLIBS=""
		 depREQUIRES=""
		 depCFLAGS_SOURCE="default"
		 depLIBS_SOURCE="default"
		 depREQUIRES_SOURCE="default"
		]
	)

	dnl allow overriding libregex settings if the user knows best
	AC_MSG_CHECKING(for libregex cflags)
	NUT_ARG_WITH_LIBOPTS_INCLUDES([regex], [auto])
	AS_CASE([${nut_with_regex_includes}],
		[auto], [],	dnl Keep what we had found above
			[depCFLAGS="${nut_with_regex_includes}"
			 depCFLAGS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

	AC_MSG_CHECKING(for libregex ldflags)
	NUT_ARG_WITH_LIBOPTS_LIBS([regex], [auto])
	AS_CASE([${nut_with_regex_libs}],
		[auto], [],	dnl Keep what we had found above
			[depLIBS="${nut_with_regex_libs}"
			 depLIBS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

	dnl Check if libregex is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	REQUIRES="${REQUIRES_ORIG} ${depREQUIRES}"

	AC_LANG_PUSH([C])
	dnl # With USB we can match desired devices by regex
	dnl # (and currently have no other use for the library);
	dnl # however we may have some general regex helper
	dnl # methods built into libcommon (may become useful
	dnl # elsewhere) - so need to know if we may and should.
	dnl # Maybe already involved in NUT for Windows builds...
	nut_have_regex=no
	AC_CHECK_HEADER([regex.h],
		[AC_DEFINE([HAVE_REGEX_H], [1],
			[Define to 1 if you have <regex.h>.])])

	AC_CHECK_DECLS([regexec, regcomp], [nut_have_regex=yes], [],
[#ifdef HAVE_REGEX_H
# include <regex.h>
#endif
])

	AS_IF([test x"${nut_have_regex}" = xyes], [
		nut_have_regex=no
		AC_SEARCH_LIBS([regcomp], [regex], [
			AC_SEARCH_LIBS([regcomp], [regex], [nut_have_regex=yes])
		])
	])

	dnl Collect possibly updated dependencies after AC SEARCH LIBS:
	AS_IF([test x"${LIBS}" != x"${LIBS_ORIG} ${depLIBS}"], [
		AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
			depLIBS="`echo "$LIBS" | sed -e 's|'"${LIBS_ORIG}"'| |' -e 's|^ *||' -e 's| *$||'`"
		])
	])

	AS_IF([test x"${nut_have_regex}" = xyes], [
		LIBREGEX_CFLAGS="${depCFLAGS}"
		LIBREGEX_LIBS="${depLIBS}"
		AC_DEFINE(HAVE_LIBREGEX, 1,
			[Define to 1 for build where we can support general regex matching.])
		], [
		LIBREGEX_CFLAGS=""
		LIBREGEX_LIBS=""
		AC_DEFINE(HAVE_LIBREGEX, 0,
			[Define to 1 for build where we can support general regex matching.])
		])
	AM_CONDITIONAL(HAVE_LIBREGEX, test x"${nut_have_regex}" = xyes)

	AC_LANG_POP([C])

	unset depCFLAGS
	unset depLIBS
	unset depREQUIRES
	unset depCFLAGS_SOURCE
	unset depLIBS_SOURCE
	unset depREQUIRES_SOURCE

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"

	dnl FIXME? We did not restore this value, is this needed by libs checked later?
	dnl REQUIRES="${REQUIRES_ORIG}"
fi
])
