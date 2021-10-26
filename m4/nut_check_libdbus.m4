dnl Check for LIBDBUS compiler flags. On success, set nut_have_dbus="yes"
dnl and set LIBDBUS_CFLAGS and LIBDBUS_LIBS. On failure, set
dnl nut_have_dbus="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBDBUS],
[
if test -z "${nut_have_dbus_seen}"; then
	nut_have_dbus_seen=yes

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	dnl See which version of the dbus library (if any) is installed
	AC_MSG_CHECKING(for libdbus version via pkg-config)
	DBUS_VERSION="`pkg-config --silence-errors --modversion dbus-1 2>/dev/null`"
	if test "$?" != "0" -o -z "${DBUS_VERSION}"; then
		DBUS_VERSION="none"
	fi
	AC_MSG_RESULT(${DBUS_VERSION} found)

	AC_MSG_CHECKING(for libdbus cflags)
	AC_ARG_WITH(dbus-includes,
		AS_HELP_STRING([@<:@--with-dbus-includes=CFLAGS@:>@], [include flags for the dbus library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-dbus-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [CFLAGS="`pkg-config --silence-errors --cflags dbus-1 2>/dev/null`"])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libdbus ldflags)
	AC_ARG_WITH(dbus-libs,
		AS_HELP_STRING([@<:@--with-dbus-libs=LIBS@:>@], [linker flags for the dbus library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-dbus-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [LIBS="`pkg-config --silence-errors --libs dbus-1 2>/dev/null`"])
	AC_MSG_RESULT([${LIBS}])

	dnl check if dbus is usable
	AC_CHECK_HEADERS(dbus/dbus.h, [nut_have_dbus=yes], [nut_have_dbus=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(dbus_bus_get, [], [nut_have_dbus=no])

	if test "${nut_have_dbus}" = "yes"; then
		AC_DEFINE(HAVE_DBUS, 1, [Define if you have Freedesktop libdbus installed])
		LIBDBUS_CFLAGS="${CFLAGS}"
		LIBDBUS_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
