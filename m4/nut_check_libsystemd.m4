dnl Check for LIBSYSTEMD compiler flags. On success, set nut_have_libsystemd="yes"
dnl and set LIBSYSTEMD_CFLAGS and LIBSYSTEMD_LIBS. On failure, set
dnl nut_have_libsystemd="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBSYSTEMD],
[
if test -z "${nut_have_libsystemd_seen}"; then
	nut_have_libsystemd_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the systemd library (if any) is installed
		 dnl FIXME : Support detection of cflags/ldflags below by legacy
		 dnl discovery if pkgconfig is not there
		 AC_MSG_CHECKING(for libsystemd version via pkg-config)
		 SYSTEMD_VERSION="`$PKG_CONFIG --silence-errors --modversion libsystemd 2>/dev/null`"
		 if test "$?" != "0" -o -z "${SYSTEMD_VERSION}"; then
		    SYSTEMD_VERSION="none"
		 fi
		 AC_MSG_RESULT(${SYSTEMD_VERSION} found)
		],
		[SYSTEMD_VERSION="none"
		 AC_MSG_NOTICE([can not check libsystemd settings via pkg-config])
		]
	)

	AC_MSG_CHECKING(for libsystemd cflags)
	AC_ARG_WITH(libsystemd-includes,
		AS_HELP_STRING([@<:@--with-libsystemd-includes=CFLAGS@:>@], [include flags for the systemd library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libsystemd-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [
		dnl Not specifying a default include path here,
		dnl headers are referenced by relative directory
		dnl and these should be in OS location usually.
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[CFLAGS="`$PKG_CONFIG --silence-errors --cflags libsystemd 2>/dev/null`" || CFLAGS=""],
			[CFLAGS=""]
		)]
	)
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libsystemd ldflags)
	AC_ARG_WITH(libsystemd-libs,
		AS_HELP_STRING([@<:@--with-libsystemd-libs=LIBS@:>@], [linker flags for the systemd library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libsystemd-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[LIBS="`$PKG_CONFIG --silence-errors --libs libsystemd 2>/dev/null`" || LIBS="-lsystemd"],
			[LIBS="-lsystemd"]
		)]
	)
	AC_MSG_RESULT([${LIBS}])

	dnl check if libsystemd is usable
	AC_CHECK_HEADERS(systemd/sd-daemon.h, [nut_have_libsystemd=yes], [nut_have_libsystemd=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(sd_notify, [], [nut_have_libsystemd=no])

	AS_IF([test x"${nut_have_libsystemd}" = x"yes"], [
		dnl Check for additional feature support in library (optional)
		AC_CHECK_FUNCS(sd_booted sd_watchdog_enabled sd_notify_barrier)
		LIBSYSTEMD_CFLAGS="${CFLAGS}"
		LIBSYSTEMD_LIBS="${LIBS}"
	])

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
