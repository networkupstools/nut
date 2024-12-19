dnl Check for LIBSYSTEMD compiler flags. On success, set nut_have_libsystemd="yes"
dnl and set LIBSYSTEMD_CFLAGS and LIBSYSTEMD_LIBS. On failure, set
dnl nut_have_libsystemd="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBSYSTEMD],
[
if test -z "${nut_have_libsystemd_seen}"; then
	nut_have_libsystemd_seen=yes
	AC_REQUIRE([NUT_CHECK_PKGCONFIG])

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LIBS=""
	depCFLAGS=""
	depLIBS=""

	SYSTEMD_VERSION="none"

	AC_CHECK_TOOL(SYSTEMCTL, systemctl, none)

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the systemd library (if any) is installed
		 dnl FIXME : Support detection of cflags/ldflags below by legacy
		 dnl discovery if pkgconfig is not there
		 AC_MSG_CHECKING(for libsystemd version via pkg-config)
		 SYSTEMD_VERSION="`$PKG_CONFIG --silence-errors --modversion libsystemd 2>/dev/null`"
		 AS_IF([test "$?" != "0" -o -z "${SYSTEMD_VERSION}"], [
			SYSTEMD_VERSION="none"
		 ])
		 AC_MSG_RESULT(${SYSTEMD_VERSION} found)
		]
	)

	AS_IF([test x"${SYSTEMD_VERSION}" = xnone], [
		 AS_IF([test x"${SYSTEMCTL}" != xnone], [
			AC_MSG_CHECKING(for libsystemd version via systemctl)
			dnl NOTE: Unlike the configure.ac file, in a "pure"
			dnl m4 script like this one, we have to escape the
			dnl dollar-number references (in awk below) lest they
			dnl get seen as m4 function positional parameters.
			SYSTEMD_VERSION="`LANG=C LC_ALL=C ${SYSTEMCTL} --version | grep -E '^systemd@<:@ \t@:>@*@<:@0-9@:>@@<:@0-9@:>@*' | awk '{print ''$''2}'`" \
			&& test -n "${SYSTEMD_VERSION}" \
			|| SYSTEMD_VERSION="none"
			AC_MSG_RESULT(${SYSTEMD_VERSION} found)
		 ])
		]
	)

	AS_IF([test x"${SYSTEMD_VERSION}" = xnone], [
		AC_MSG_NOTICE([can not check libsystemd settings via pkg-config nor systemctl])
	])

	AC_MSG_CHECKING(for libsystemd cflags)
	AC_ARG_WITH(libsystemd-includes,
		AS_HELP_STRING([@<:@--with-libsystemd-includes=CFLAGS@:>@], [include flags for the systemd library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libsystemd-includes - see docs/configure.txt)
			;;
		*)
			depCFLAGS="${withval}"
			;;
		esac
	], [
		dnl Not specifying a default include path here,
		dnl headers are referenced by relative directory
		dnl and these should be in OS location usually.
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libsystemd 2>/dev/null`" \
			 || depCFLAGS=""],
			[depCFLAGS=""]
		)]
	)
	AC_MSG_RESULT([${depCFLAGS}])

	AC_MSG_CHECKING(for libsystemd ldflags)
	AC_ARG_WITH(libsystemd-libs,
		AS_HELP_STRING([@<:@--with-libsystemd-libs=LIBS@:>@], [linker flags for the systemd library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libsystemd-libs - see docs/configure.txt)
			;;
		*)
			depLIBS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[depLIBS="`$PKG_CONFIG --silence-errors --libs libsystemd 2>/dev/null`" \
			 || depLIBS="-lsystemd"],
			[depLIBS="-lsystemd"]
		)]
	)
	AC_MSG_RESULT([${depLIBS}])

	dnl check if libsystemd is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_CHECK_HEADERS(systemd/sd-daemon.h, [nut_have_libsystemd=yes], [nut_have_libsystemd=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(sd_notify, [], [nut_have_libsystemd=no])

	nut_have_libsystemd_inhibitor=no
	AS_IF([test x"${nut_have_libsystemd}" = x"yes"], [
		dnl Check for additional feature support in library (optional)
		AC_CHECK_FUNCS(sd_booted sd_watchdog_enabled sd_notify_barrier)
		LIBSYSTEMD_CFLAGS="${depCFLAGS}"
		LIBSYSTEMD_LIBS="${depLIBS}"

		dnl Since systemd 183: https://systemd.io/INHIBITOR_LOCKS/
		dnl ...or 221: https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html
		dnl and some bits even later (e.g. message container reading)
		AS_IF([test "$SYSTEMD_VERSION" -ge 221], [
			nut_have_libsystemd_inhibitor=yes
			AC_CHECK_HEADERS(systemd/sd-bus.h, [], [nut_have_libsystemd_inhibitor=no], [AC_INCLUDES_DEFAULT])
			AC_CHECK_FUNCS([sd_bus_call_method sd_bus_message_read_basic sd_bus_open_system sd_bus_default_system sd_bus_get_property_trivial], [], [nut_have_libsystemd_inhibitor=no])
			dnl NOTE: In practice we use "p"-suffixed sd_bus_flush_close_unrefp
			dnl  and sd_bus_message_unrefp methods prepared by a macro in sd-bus.h
			AC_CHECK_FUNCS([sd_bus_flush_close_unref sd_bus_message_unref sd_bus_error_free], [], [nut_have_libsystemd_inhibitor=no])
			dnl Optional methods: nicer with them, can do without
			AC_CHECK_FUNCS([sd_bus_open_system_with_description sd_bus_set_description])
			dnl For inhibitor per se, we do not have to read containers:
			dnl AC_CHECK_FUNCS([sd_bus_message_enter_container sd_bus_message_exit_container])
		])

		AC_MSG_CHECKING(for libsystemd inhibitor interface support)
		AC_MSG_RESULT([${nut_have_libsystemd_inhibitor}])
	])

	unset depCFLAGS
	unset depLIBS

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
