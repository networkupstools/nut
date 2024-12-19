dnl Check for LIBPOWERMAN compiler flags. On success, set nut_have_libpowerman="yes"
dnl and set LIBPOWERMAN_CFLAGS and LIBPOWERMAN_LIBS. On failure, set
dnl nut_have_libpowerman="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBPOWERMAN],
[
if test -z "${nut_have_libpowerman_seen}"; then
	nut_have_libpowerman_seen=yes
	AC_REQUIRE([NUT_CHECK_PKGCONFIG])

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LIBS=""
	depCFLAGS=""
	depLIBS=""

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING([for LLNC libpowerman version via pkg-config])
		 POWERMAN_VERSION="`$PKG_CONFIG --silence-errors --modversion libpowerman 2>/dev/null`"
		 dnl Unlike other pkg-config enabled projects we use,
		 dnl libpowerman (at least on Debian) delivers an empty
		 dnl "Version:" tag in /usr/lib/pkgconfig/libpowerman.pc
		 dnl (and it is the only file in that dir, others going
		 dnl to /usr/lib/x86_64-linux-gnu/pkgconfig/ or similar
		 dnl for other architectures). Empty is not an error here!
		 if test "$?" != "0" ; then # -o -z "${POWERMAN_VERSION}"; then
		    POWERMAN_VERSION="none"
		 fi
		 AC_MSG_RESULT(['${POWERMAN_VERSION}' found])
		],
		[POWERMAN_VERSION="none"
		 AC_MSG_NOTICE([can not check LLNC libpowerman settings via pkg-config])
		]
	)

	AS_IF([test x"$POWERMAN_VERSION" != xnone],
		[depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libpowerman 2>/dev/null`"
		 depLIBS="`$PKG_CONFIG --silence-errors --libs libpowerman 2>/dev/null`"
		],
		[depCFLAGS=""
		 depLIBS=""
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
			depCFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${depCFLAGS}])

	AC_MSG_CHECKING(for libpowerman libs)
	AC_ARG_WITH(powerman-libs,
		AS_HELP_STRING([@<:@--with-powerman-libs=LIBS@:>@], [linker flags for the libpowerman library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-powerman-libs - see docs/configure.txt)
			;;
		*)
			depLIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${depLIBS}])

	dnl check if libpowerman is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_CHECK_HEADERS(libpowerman.h, [nut_have_libpowerman=yes], [nut_have_libpowerman=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(pm_connect, [], [
		dnl Some systems may just have libpowerman in their
		dnl standard paths, but not the pkg-config data
		AS_IF([test "${nut_have_libpowerman}" = "yes" && test "$POWERMAN_VERSION" = "none" && test -z "$LIBS"],
			[AC_MSG_CHECKING([if libpowerman is just present in path])
			 depLIBS="-L/usr/lib -L/usr/local/lib -lpowerman"
			 unset ac_cv_func_pm_connect || true
			 LIBS="${LIBS_ORIG} ${depLIBS}"
			 AC_CHECK_FUNCS(pm_connect, [], [nut_have_libpowerman=no])
			 AC_MSG_RESULT([${nut_have_libpowerman}])
			], [nut_have_libpowerman=no]
		)]
	)

	if test "${nut_have_libpowerman}" = "yes"; then
		LIBPOWERMAN_CFLAGS="${depCFLAGS}"
		LIBPOWERMAN_LIBS="${depLIBS}"
	fi

	unset depCFLAGS
	unset depLIBS

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
