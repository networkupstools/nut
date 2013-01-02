dnl Check for LIBDBUSCPP compiler flags and DBUS_SYSBUS_CONF_PATH config prefix.
dnl
dnl On success, set nut_have_libdbus="yes"
dnl and set LIBDBUSCPP_CFLAGS and LIBDBUSCPP_LIBS. On failure, set
dnl nut_have_libdbus="no". This macro can be run multiple times, but will
dnl do the checking only once. 
dnl
dnl NUT requires DBUS and DBUS-CPP

AC_DEFUN([NUT_CHECK_DBUS], 
[
if test -z "${nut_have_libdbus_seen}"; then
	nut_have_libdbus_seen=yes

	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"


	dnl See which version of dbus is installed
	AC_MSG_CHECKING(for dbus-1 version via pkg-config)
	DBUS_VERSION="`pkg-config --silence-errors --modversion dbus-1 2>/dev/null`"
	if test "$?" != "0" -o -z "${DBUS_VERSION}"; then
		DBUS_VERSION="none"
	fi
	AC_MSG_RESULT(${DBUS_VERSION} found)

	dnl See where to install system bus configuration files.
	AC_MSG_CHECKING(for dbus-1 system configuration)
	AC_ARG_WITH(dbus-sysbus-conf, AS_HELP_STRING([@<:@--with-dbus-sysbus-conf=DIR@:>@],
		 [directory where install system bus configuration files]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-dbus-sysbus-conf - see docs/configure.txt)
			;;
		*)
			DBUS_SYSBUS_CONF_PATH="${withval}"
			;;
		esac
	], [DBUS_SYSBUS_CONF_PATH="`pkg-config --silence-errors --variable=sysconfdir dbus-1 2>/dev/null`/dbus-1/system.d"])
	AC_MSG_RESULT([${DBUS_SYSBUS_CONF_PATH}])


	dnl Look at dbus-c++ CFLAGS
	AC_MSG_CHECKING(for libdbus-c++ cflags)
	AC_ARG_WITH(dbusc++-includes,
		AS_HELP_STRING([@<:@--with-dbusc++-includes=CFLAGS@:>@], [include flags for the dbus-c++ library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-dbusc++-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [CFLAGS="`pkg-config --silence-errors --cflags dbus-c++-1 2>/dev/null`"])
	AC_MSG_RESULT([${CFLAGS}])

	dnl Look at dbus-c++ LDFLAGS
	AC_MSG_CHECKING(for libdbus-c++ ldflags)
	AC_ARG_WITH(dbus-c++-libs,
		AS_HELP_STRING([@<:@--with-dbus-c++-libs=LIBS@:>@], [linker flags for the dbus-c++ library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-dbus-c++-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [LIBS="`pkg-config --silence-errors --libs dbus-c++-1 2>/dev/null`"])
	AC_MSG_RESULT([${LIBS}])

	dnl check if dbus-c++ is usable
	AC_CHECK_HEADERS(dbus-c++/dbus.h, [nut_have_libdbuscpp=yes], [nut_have_libdbuscpp=no], [AC_INCLUDES_DEFAULT])
	dnl TODO Complete it

	if test "${nut_have_libdbuscpp}" = "yes"; then
		LIBDBUSCPP_CFLAGS="${CFLAGS}"
		LIBDBUSCPP_LIBS="${LIBS}"
	fi

	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
