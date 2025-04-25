dnl Check for LIBGD compiler flags. On success, set nut_have_libgd="yes"
dnl and set LIBGD_CFLAGS and LIBGD_LDFLAGS. On failure, set
dnl nut_have_libgd="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBGD],
[
if test -z "${nut_have_libgd_seen}"; then
	nut_have_libgd_seen=yes
	AC_REQUIRE([NUT_CHECK_PKGCONFIG])

	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LDFLAGS=""
	LIBS=""
	depCFLAGS=""
	depLDFLAGS=""
	depLIBS=""

	AS_IF([test x"${nut_enable_configure_debug}" = xyes], [
		AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) LIBGD (before): CFLAGS_ORIG="${CFLAGS_ORIG}" CXXFLAGS_ORIG="${CXXFLAGS_ORIG}" CPPFLAGS_ORIG="${CPPFLAGS_ORIG}" LDFLAGS_ORIG="${LDFLAGS_ORIG}" LIBS_ORIG="${LIBS_ORIG}"])
	])

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING(for gd version via pkg-config)
		 GD_VERSION="`$PKG_CONFIG --silence-errors --modversion gdlib 2>/dev/null`"
		 if test "$?" != "0" -o -z "${GD_VERSION}"; then
		    GD_VERSION="none"
		 fi
		 AC_MSG_RESULT(${GD_VERSION} found)
		],
		[GD_VERSION="none"
		 AC_MSG_NOTICE([can not check libgd settings via pkg-config])
		]
	)

	AS_IF([test x"$GD_VERSION" != xnone],
		[depCFLAGS="`$PKG_CONFIG --silence-errors --cflags gdlib 2>/dev/null`"
		 depLIBS="`$PKG_CONFIG --silence-errors --libs gdlib 2>/dev/null`"
		],
		[dnl Initial defaults. These are only used if gdlib-config is
		 dnl unusable and the user fails to pass better values in --with
		 dnl arguments
		 depCFLAGS=""
		 depLDFLAGS="-L/usr/X11R6/lib"
		 depLIBS="-lgd -lpng -lz -ljpeg -lfreetype -lm -lXpm -lX11"

		 dnl By default seek in PATH
		 AC_PATH_PROGS([GDLIB_CONFIG], [gdlib-config], [none])
		 AC_ARG_WITH(gdlib-config,
			AS_HELP_STRING([@<:@--with-gdlib-config=/path/to/gdlib-config@:>@],
				[path to program that reports GDLIB configuration]),
		 [
			case "${withval}" in
			"") ;;
			yes|no)
				AC_MSG_ERROR(invalid option --with(out)-gdlib-config - see docs/configure.txt)
				;;
			*)
				GDLIB_CONFIG="${withval}"
				;;
			esac
		 ])

		 AS_IF([test x"$GDLIB_CONFIG" != xnone],
			[AC_MSG_CHECKING(for gd version via ${GDLIB_CONFIG})
			 GD_VERSION="`${GDLIB_CONFIG} --version 2>/dev/null`"
			 if test "$?" != "0" -o -z "${GD_VERSION}"; then
				GD_VERSION="none"
			 fi
			 AC_MSG_RESULT(${GD_VERSION} found)
			], [GD_VERSION="none"]
		 )

		 case "${GD_VERSION}" in
		 none)
			;;
		 2.0.5 | 2.0.6 | 2.0.7)
			AC_MSG_WARN([[gd ${GD_VERSION} detected, unable to use ${GDLIB_CONFIG} script]])
			AC_MSG_WARN([[If gd detection fails, upgrade gd or use --with-gd-includes and --with-gd-libs]])
			;;
		 *)
			depCFLAGS="`${GDLIB_CONFIG} --includes 2>/dev/null`"
			depLDFLAGS="`${GDLIB_CONFIG} --ldflags 2>/dev/null`"
			depLIBS="`${GDLIB_CONFIG} --libs 2>/dev/null`"
			;;
		 esac
		]
	)

	dnl Now allow overriding gd settings if the user knows best
	AC_MSG_CHECKING(for gd include flags)
	AC_ARG_WITH(gd-includes,
		AS_HELP_STRING([@<:@--with-gd-includes=CFLAGS@:>@], [include flags for the gd library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-gd-includes - see docs/configure.txt)
			;;
		*)
			depCFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${depCFLAGS}])

	AC_MSG_CHECKING(for gd library flags)
	AC_ARG_WITH(gd-libs,
		AS_HELP_STRING([@<:@--with-gd-libs=LDFLAGS@:>@], [linker flags for the gd library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-gd-libs - see docs/configure.txt)
			;;
		*)
			depLDFLAGS="${withval}"
			depLIBS=""
			;;
		esac
	], [])
	AC_MSG_RESULT([${depLDFLAGS} ${depLIBS}])

	dnl check if gd is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LDFLAGS="${LDFLAGS_ORIG} ${depLDFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_CHECK_HEADERS(gd.h gdfontmb.h, [nut_have_libgd=yes], [nut_have_libgd=no], [AC_INCLUDES_DEFAULT])
	AC_SEARCH_LIBS(gdImagePng, gd, [], [
		dnl If using pkg-config, query additionally for Libs.private
		dnl to pull -L/usr/X11R6/lib or whatever current OS wants
		AC_MSG_CHECKING([for more gd library flags])
		AS_IF([test -n "${with_gd_libs}" || test x"$have_PKG_CONFIG" != xyes], [nut_have_libgd=no], [
			depLIBS_PRIVATE="`$PKG_CONFIG --silence-errors --libs gdlib --static 2>/dev/null`"
			AS_IF([test -z "${depLIBS_PRIVATE}"], [nut_have_libgd=no], [
				AC_MSG_CHECKING([with gdlib.pc Libs.private])
				depLDFLAGS="$depLDFLAGS $depLIBS_PRIVATE"
				unset ac_cv_search_gdImagePng
				LDFLAGS="${LDFLAGS_ORIG} ${depLDFLAGS}"
				AC_SEARCH_LIBS(gdImagePng, gd, [nut_have_libgd=yes], [nut_have_libgd=no])
			])
			unset depLIBS_PRIVATE
			dnl At least mingw 32-bit builds of the DLL seem to not
			dnl tell the linker how to get from GD to PNG lib
			AS_IF([test x"$nut_have_libgd" = xno], [
				AC_MSG_CHECKING([with explicit -lpng in the loop])
				depLDFLAGS="$depLDFLAGS -lgd"
				unset ac_cv_search_gdImagePng
				LDFLAGS="${LDFLAGS_ORIG} ${depLDFLAGS}"
				AC_SEARCH_LIBS(gdImagePng, png png16, [nut_have_libgd=yes], [nut_have_libgd=no])
			])
		])
	])

	dnl Collect possibly updated dependencies after AC SEARCH LIBS:
	AS_IF([test x"${LIBS}" != x"${LIBS_ORIG} ${depLIBS}"], [
		AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
			depLIBS="`echo "$LIBS" | sed -e 's|'"${LIBS_ORIG}"'| |' -e 's|^ *||' -e 's| *$||'`"
		])
	])

	if test "${nut_have_libgd}" = "yes"; then
		AC_MSG_CHECKING([whether we can build, link and/or run a program with libgd])
		AC_LANG_PUSH([C])
		AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([
#include <gd.h>
#include <gdfontmb.h>
#include <stdio.h>
],
[
FILE *tmpf = tmpfile();
gdImagePtr im = gdImageCreate(64, 128);
int back_color = gdImageColorAllocate(im, 255, 128, 32);
int scale_num_color = gdImageColorAllocate(im, 0, 128, 128);
gdImageFilledRectangle(im, 0, 0, 64, 128, back_color);
gdImageColorTransparent(im, back_color);
/* this may invoke fontconfig/freetype or equivalen dependencies of libgd: */
gdImageString(im, gdFontMediumBold, 4, 16, (unsigned char *)"Test Label", scale_num_color);
gdImagePng(im, tmpf ? tmpf : stderr);
gdImageDestroy(im);
]
		)], [], [nut_have_libgd=no])
		AC_LANG_POP([C])
		AC_MSG_RESULT([${nut_have_libgd}])
	fi

	if test "${nut_have_libgd}" = "yes"; then
		AC_DEFINE(HAVE_LIBGD, 1, [Define if you have Boutell's libgd installed])
		LIBGD_CFLAGS="${depCFLAGS}"
		LIBGD_LDFLAGS="${depLDFLAGS} ${depLIBS}"
	fi

	unset depCFLAGS
	unset depLDFLAGS
	unset depLIBS

	dnl put back the original versions
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
