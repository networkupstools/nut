dnl Check for LIBHAL configuration if support for HAL was found.
dnl This keeps compile and link time options separate from runtime
dnl configuration items. This macro can be run multiple times, but
dnl will do the checking only once.

AC_DEFUN([NUT_CONFIG_LIBHAL],
[
if test -z "${nut_have_config_libhal_seen}" -a "${nut_have_libhal}" = "yes"; then
	nut_have_config_libhal_seen=yes

	AC_REQUIRE([NUT_CHECK_LIBHAL])

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
		HAL_USER="`pkg-config --silence-errors --variable=haluser hal 2>/dev/null`"
		if test -z "${HAL_USER}"; then
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
		if pkg-config --silence-errors --atleast-version=0.5.11 hal 2>/dev/null; then
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
		HAL_CALLOUTS_PATH="`pkg-config --silence-errors --variable=libexecdir hal 2>/dev/null`"
		if test -z "${HAL_CALLOUTS_PATH}"; then
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
		HAL_FDI_PATH="`pkg-config --silence-errors --variable=hal_fdidir hal 2>/dev/null`"
		if test -z "${HAL_FDI_PATH}"; then
			HAL_FDI_PATH="${datarootdir}/hal/fdi/information/20thirdparty"
		fi
	])
	AC_MSG_RESULT(${HAL_FDI_PATH})
fi
])
