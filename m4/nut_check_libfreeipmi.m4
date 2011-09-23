dnl Check for FreeIPMI (LIBFREEIPMI) compiler flags. On success, set 
dnl nut_have_freeipmi="yes" and nut_ipmi_lib="FreeIPMI", and define WITH_IPMI,
dnl WITH_FREEIPMI, LIBIPMI_CFLAGS and LIBIPMI_LIBS. On failure, set
dnl nut_have_freeipmi="no".
dnl This macro can be run multiple times, but will do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBFREEIPMI],
[
if test -z "${nut_have_libfreeipmi_seen}"; then
	nut_have_libfreeipmi_seen=yes

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AC_MSG_CHECKING(for FreeIPMI version via pkg-config)
	dnl pkg-config support requires Freeipmi 1.0.5, released on Thu Jun 30 2011
	dnl but NUT should only require 0.8.5 or 1.0.1 (comment from upstream Al Chu)
	FREEIPMI_VERSION="`pkg-config --silence-errors --modversion libfreeipmi 2>/dev/null`"
	if test "$?" = "0" -a -n "${FREEIPMI_VERSION}"; then
		CFLAGS="`pkg-config --silence-errors --cflags libfreeipmi libipmimonitoring 2>/dev/null`"
		LIBS="`pkg-config --silence-errors --libs libfreeipmi libipmimonitoring 2>/dev/null`"
	else
		FREEIPMI_VERSION="none"
		CFLAGS=""
		LIBS="-lfreeipmi -lipmimonitoring"
	fi
	AC_MSG_RESULT(${FREEIPMI_VERSION} found)

	dnl allow overriding FreeIPMI settings if the user knows best
	AC_MSG_CHECKING(for FreeIPMI cflags)
	AC_ARG_WITH(freeipmi-includes,
		AS_HELP_STRING([@<:@--with-freeipmi-includes=CFLAGS@:>@], [include flags for the FreeIPMI library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-freeipmi-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for FreeIPMI ldflags)
	AC_ARG_WITH(freeipmi-libs,
		AS_HELP_STRING([@<:@--with-freeipmi-libs=LIBS@:>@], [linker flags for the FreeIPMI library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-freeipmi-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LIBS}])

	dnl check if freeipmi is usable with our current flags
	AC_CHECK_HEADERS(freeipmi/freeipmi.h, [nut_have_freeipmi=yes], [nut_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_HEADERS(ipmi_monitoring.h, [], [nut_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
	AC_SEARCH_LIBS([ipmi_ctx_create], [freeipmi], [], [nut_have_freeipmi=no])
	dnl when version cannot be tested (prior to 1.0.5, with no pkg-config)
	dnl we have to check for some specific functions
	AC_SEARCH_LIBS([ipmi_ctx_find_inband], [freeipmi], [], [nut_have_freeipmi=no])
	AC_SEARCH_LIBS([ipmi_fru_parse_ctx_create], [freeipmi], [], [nut_have_freeipmi=no])
	AC_SEARCH_LIBS([ipmi_monitoring_init], [ipmimonitoring], [], [nut_have_freeipmi=no])

	if test "${nut_have_freeipmi}" = "yes"; then
		nut_with_ipmi="yes"
		nut_ipmi_lib="(FreeIPMI)"
		nut_have_libipmi="yes"
		AC_DEFINE(HAVE_FREEIPMI, 1, [Define if FreeIPMI support is available])
		LIBIPMI_CFLAGS="${CFLAGS}"
		LIBIPMI_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
