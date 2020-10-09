dnl Check for LIBGD compiler flags. On success, set nut_have_libgd="yes"
dnl and set LIBGD_CFLAGS and LIBGD_LDFLAGS. On failure, set
dnl nut_have_libgd="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBGD], 
[
if test -z "${nut_have_libgd_seen}"; then
	nut_have_libgd_seen=yes

	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"
	LIBS_ORIG="${LIBS}"

	AC_MSG_CHECKING(for gd version via pkg-config)
	GD_VERSION="`pkg-config --silence-errors --modversion gdlib 2>/dev/null`"
	if test "$?" = "0" -a -n "${GD_VERSION}"; then
		CFLAGS="`pkg-config --silence-errors --cflags gdlib 2>/dev/null`"
		LIBS="`pkg-config --silence-errors --libs gdlib 2>/dev/null`"
	else
		GD_VERSION="none"
	fi
	AC_MSG_RESULT(${GD_VERSION} found)

	if test "${GD_VERSION}" = "none"; then
		dnl Initial defaults. These are only used if gdlib-config is
		dnl unusable and the user fails to pass better values in --with
		dnl arguments
		CFLAGS=""
		LDFLAGS="-L/usr/X11R6/lib"
		LIBS="-lgd -lpng -lz -ljpeg -lfreetype -lm -lXpm -lX11"

		dnl By default seek in PATH
		GDLIB_CONFIG=gdlib-config
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

		AC_MSG_CHECKING(for gd version via ${GDLIB_CONFIG})
		GD_VERSION=`${GDLIB_CONFIG} --version 2>/dev/null`
		if test "$?" != "0" -o -z "${GD_VERSION}"; then
			GD_VERSION="none"
		fi
		AC_MSG_RESULT(${GD_VERSION} found)

		case "${GD_VERSION}" in
		none)
			;;
		2.0.5 | 2.0.6 | 2.0.7)
			AC_MSG_WARN([[gd ${GD_VERSION} detected, unable to use ${GDLIB_CONFIG} script]])
			AC_MSG_WARN([[If gd detection fails, upgrade gd or use --with-gd-includes and --with-gd-libs]])
			;;
		*)
			CFLAGS="`${GDLIB_CONFIG} --includes 2>/dev/null`"
			LDFLAGS="`${GDLIB_CONFIG} --ldflags 2>/dev/null`"
			LIBS="`${GDLIB_CONFIG} --libs 2>/dev/null`"
			;;
		esac
	fi

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
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for gd library flags)
	AC_ARG_WITH(gd-libs,
		AS_HELP_STRING([@<:@--with-gd-libs=LDFLAGS@:>@], [linker flags for the gd library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-gd-libs - see docs/configure.txt)
			;;
		*)
			LDFLAGS="${withval}"
			LIBS=""
			;;
		esac
	], [])
	AC_MSG_RESULT([${LDFLAGS} ${LIBS}])

	dnl check if gd is usable
	AC_CHECK_HEADERS(gd.h gdfontmb.h, [nut_have_libgd=yes], [nut_have_libgd=no], [AC_INCLUDES_DEFAULT])
	AC_SEARCH_LIBS(gdImagePng, gd, [], [nut_have_libgd=no])

	if test "${nut_have_libgd}" = "yes"; then
		AC_DEFINE(HAVE_LIBGD, 1, [Define if you have Boutell's libgd installed])
		LIBGD_CFLAGS="${CFLAGS}"
		LIBGD_LDFLAGS="${LDFLAGS} ${LIBS}"
	fi

	dnl put back the original versions
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
