dnl Check for LIBPOWERMAN compiler flags. On success, set nut_have_libpowerman="yes"
dnl and set LIBPOWERMAN_CFLAGS and LIBPOWERMAN_LIBS. On failure, set
dnl nut_have_libpowerman="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBPOWERMAN],
[
if test -z "${nut_have_libpowerman_seen}"; then
	nut_have_libpowerman_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING([for LLNC libpowerman version via pkg-config])
		 POWERMAN_VERSION="`$PKG_CONFIG --silence-errors --modversion libpowerman 2>/dev/null`"
		 if test "$?" != "0" -o -z "${POWERMAN_VERSION}"; then
		    POWERMAN_VERSION="none"
		 fi
		 AC_MSG_RESULT([${POWERMAN_VERSION} found])
		],
		[POWERMAN_VERSION="none"
		 AC_MSG_NOTICE([can not check LLNC libpowerman settings via pkg-config])
		]
	)

	AS_IF([test x"$POWERMAN_VERSION" != xnone],
		[CFLAGS="`$PKG_CONFIG --silence-errors --cflags libpowerman 2>/dev/null`"
		 LIBS="`$PKG_CONFIG --silence-errors --libs libpowerman 2>/dev/null`"
		],
		[CFLAGS=""
		 LIBS=""
		]
	)

	AC_MSG_CHECKING([for libpowerman cflags])
	AC_ARG_WITH(powerman-includes,
		AS_HELP_STRING([@<:@--with-powerman-includes=CFLAGS@:>@], [include flags for the libpowerman library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR([invalid option --with(out)-powerman-includes - see docs/configure.txt])
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libpowerman libs)
	AC_ARG_WITH(powerman-libs,
		AS_HELP_STRING([@<:@--with-powerman-libs=LIBS@:>@], [linker flags for the libpowerman library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-powerman-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LIBS}])

	dnl check if libpowerman is usable
	AC_CHECK_HEADERS(libpowerman.h, [nut_have_libpowerman=yes], [nut_have_libpowerman=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(pm_connect, [], [nut_have_libpowerman=no])

	if test "${nut_have_libpowerman}" = "yes"; then
		LIBPOWERMAN_CFLAGS="${CFLAGS}"
		LIBPOWERMAN_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"

fi
])
