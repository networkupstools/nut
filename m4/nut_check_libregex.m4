dnl Check for LIBREGEX (and, if found, fill 'nut_usb_lib' with its
dnl approximate version) and its compiler flags. On success, set
dnl nut_have_libusb="yes" and set LIBREGEX_CFLAGS and LIBREGEX_LIBS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBREGEX],
[
if test -z "${nut_have_libregex_seen}"; then
	nut_have_libregex_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LIBS=""

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
		[CFLAGS="`$PKG_CONFIG --silence-errors --cflags "${LIBREGEX_MODULE}" 2>/dev/null`"
		 LIBS="`$PKG_CONFIG --silence-errors --libs "${LIBREGEX_MODULE}" 2>/dev/null`"
		 REQUIRES="${LIBREGEX_MODULE}"
		],
		[CFLAGS=""
		 LIBS=""
		 REQUIRES=""
		]
	)

	dnl Check if libregex is usable
	AC_LANG_PUSH([C])
	dnl # With USB we can match desired devices by regex
	dnl # (and currently have no other use for the library);
	dnl # however we may have some general regex helper
	dnl # methods built into libcommon (may become useful
	dnl # elsewhere) - so need to know if we may and should.
	dnl # Maybe already involved in NUT for Windows builds...
	nut_have_regex=no
	AC_CHECK_HEADER([regex.h],
		[nut_have_regex=yes
		 AC_DEFINE([HAVE_REGEX_H], [1],
			[Define to 1 if you have <regex.h>.])])
	AC_CHECK_DECLS([regexec, regcomp], [nut_have_regex=yes], [],
[#ifdef HAVE_REGEX_H
# include <regex.h>
#endif
])

	AC_SEARCH_LIBS([regcomp, regexec], [], [nut_have_regex=yes], [
		AS_IF([test x"$LIBS" = x], [
			dnl Avoid using cached reply for the absent library name
			unset ac_cv_search_regcomp__regexec || true
			AC_SEARCH_LIBS([regcomp, regexec], [regex], [
				LIBS="-lregex"
				nut_have_regex=yes
			])
		])
	])

	AS_IF([test x"${nut_have_regex}" = xyes], [
		LIBREGEX_CFLAGS="${CFLAGS}"
		LIBREGEX_LIBS="${LIBS}"
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

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
