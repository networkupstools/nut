dnl Check for LIBHAL compiler flags. On success, set nut_have_libhal="yes"
dnl and set LIBHAL_CFLAGS and LIBHAL_LDFLAGS. On failure, set
dnl nut_have_libhal="no". This macro can be run multiple times, but will
dnl do the checking only once. 
dnl NUT requires HAL version 0.5.8 at least

AC_DEFUN([NUT_CHECK_LIBHAL], 
[
if test -z "${nut_have_libhal_seen}"; then
	nut_have_libhal_seen=yes

	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	AC_MSG_CHECKING(for libhal version via pkg-config (0.5.8 minimum required))
	HAL_VERSION="`pkg-config --silence-errors --modversion hal`"
	if test "$?" != "0"; then
		AC_MSG_RESULT(not found)
	elif pkg-config --silence-errors --atleast-version=0.5.8 hal; then
 		AC_MSG_RESULT(${HAL_VERSION})
	else
		AC_MSG_WARN(${HAL_VERSION} is too old)
	fi

	AC_MSG_CHECKING(for libhal cflags)
	AC_ARG_WITH(hal-includes, [
		AS_HELP_STRING([--with-hal-includes=CFLAGS], [include flags for the HAL library])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [
		dnl also get cflags from glib-2.0 to workaround a bug in dbus-glib
		CFLAGS="`pkg-config --silence-errors --cflags hal dbus-glib-1`"
		if test "$?" != "0"; then
			CFLAGS="-DDBUS_API_SUBJECT_TO_CHANGE -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include"
		fi
	])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libhal ldflags)
	AC_ARG_WITH(hal-libs, [
		AS_HELP_STRING([--with-hal-libs=LDFLAGS], [linker flags for the HAL library])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-libs - see docs/configure.txt)
			;;
		*)
			LDFLAGS="${withval}"
			;;
		esac
	], [
		dnl also get libs from glib-2.0 to workaround a bug in dbus-glib
		LDFLAGS="`pkg-config --silence-errors --libs hal dbus-glib-1`"
		if test "$?" != "0"; then
			LDFLAGS="-lhal -ldbus-1 -lpthread"
		fi
	])
	AC_MSG_RESULT([${LDFLAGS}])

	AC_MSG_CHECKING(for libhal user)
	AC_ARG_WITH(hal-user, [
		AS_HELP_STRING([--with-hal-user=USER], [addons run as user])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-user - see docs/configure.txt)
			;;
		*)
			HAL_USER="${withval}"
			;;
		esac
	], [
		dnl this will only work as of HAL 0.5.9
		HAL_USER="`pkg-config --silence-errors --variable=haluser hal`"
		if test "$?" != "0"; then
			HAL_USER="haldaemon"
		fi
	])
	AC_MSG_RESULT(${HAL_USER})
	AC_DEFINE_UNQUOTED(HAL_USER, "${HAL_USER}", [addons run as user])

	AC_MSG_CHECKING(for libhal device match key)
	AC_ARG_WITH(hal-device-match-key, [
		AS_HELP_STRING([--with-hal-device-match-key=KEY], [device match key])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-device-match-key - see docs/configure.txt)
			;;
		*)
			HAL_DEVICE_MATCH_KEY="${withval}"
			;;
		esac
	], [
		dnl the device match key changed with HAL 0.5.11
		if pkg-config --silence-errors --atleast-version=0.5.11 hal; then
			HAL_DEVICE_MATCH_KEY="info.bus"
		else
			HAL_DEVICE_MATCH_KEY="info.subsystem"
		fi
	])
	AC_MSG_RESULT(${HAL_DEVICE_MATCH_KEY})

	AC_MSG_CHECKING(for libhal Callouts path)
	AC_ARG_WITH(hal-callouts-path, [
		AS_HELP_STRING([--with-hal-callouts-path=PATH], [installation path for callouts])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-callouts-path - see docs/configure.txt)
			;;
		*)
			HAL_CALLOUTS_PATH="${withval}"
			;;
		esac
	], [
		dnl Determine installation path for callouts
		dnl As per HAL spec, §5 Callouts addon install path: $libdir/hal
		HAL_CALLOUTS_PATH="`pkg-config --silence-errors --variable=libexecdir hal`"
		if test "$?" != "0"; then
			HAL_CALLOUTS_PATH="${libdir}/hal"
		fi
	])
	AC_MSG_RESULT(${HAL_CALLOUTS_PATH})

	AC_MSG_CHECKING(for libhal Device Information path)
	AC_ARG_WITH(hal-fdi-path, [
		AS_HELP_STRING([--with-hal-fdi-path=PATH], [installation path for device information files])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-fdi-path - see docs/configure.txt)
			;;
		*)
			HAL_FDI_PATH="${withval}"
			;;
		esac
	], [
		dnl Determine installation path for .fdi
		dnl As per HAL spec, §2 Device Information Files
		dnl fdi install path: $datarootdir/hal/fdi/information/20thirdparty
		HAL_FDI_PATH="`pkg-config --silence-errors --variable=hal_fdidir hal`"
		if test "$?" != "0"; then
			HAL_FDI_PATH="${datarootdir}/hal/fdi/information/20thirdparty"
		fi
	])
	AC_MSG_RESULT(${HAL_FDI_PATH})

	dnl check if HAL is usable
	AC_CHECK_HEADERS(libhal.h, [nut_have_libhal=yes], [nut_have_libhal=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(libhal_device_new_changeset, [], [nut_have_libhal=no])

	if test "${nut_have_libhal}" = "yes"; then
		LIBHAL_CFLAGS="${CFLAGS}"
		LIBHAL_LDFLAGS="${LDFLAGS}"
	fi

	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"

	dnl - test for g_timeout_add_seconds availability
	AC_MSG_CHECKING([if GLib is version 2.14.0 or newer])
	if pkg-config --silence-errors --atleast-version=2.14.0 glib-2.0; then
		AC_DEFINE(HAVE_GLIB_2_14, 1, [Define to 1 if GLib is version 2.14 or newer])
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
fi
])
