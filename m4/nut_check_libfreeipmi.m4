dnl Check for FreeIPMI (LIBFREEIPMI) compiler flags. On success, set 
dnl nut_have_freeipmi="yes" and nut_ipmi_lib="FreeIPMI", and define WITH_IPMI,
dnl WITH_FREEIPMI, LIBIPMI_CFLAGS and LIBIPMI_LIBS. On failure, set
dnl nut_have_freeipmi="no".
dnl This macro can be run multiple times, but will do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBFREEIPMI],
[
if test -z "${nut_have_libfreeipmi_seen}"; then
	nut_have_libfreeipmi_seen=yes
	AC_REQUIRE([NUT_CHECK_PKGCONFIG])

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LIBS=""
	depCFLAGS=""
	depLIBS=""

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl pkg-config support requires Freeipmi 1.0.5, released on Thu Jun 30 2011
		 dnl but NUT should only require 0.8.5 (for nut-scanner) and 1.0.1 (for
		 dnl nut-ipmipsu) (comment from upstream Al Chu)
		 AC_MSG_CHECKING(for FreeIPMI version via pkg-config)
		 FREEIPMI_VERSION="`$PKG_CONFIG --silence-errors --modversion libfreeipmi 2>/dev/null`"
		 if test "$?" != "0" -o -z "${FREEIPMI_VERSION}"; then
		    FREEIPMI_VERSION="none"
		 fi
		 AC_MSG_RESULT(${FREEIPMI_VERSION} found)
		],
		[FREEIPMI_VERSION="none"
		 AC_MSG_NOTICE([can not check FreeIPMI settings via pkg-config])
		]
	)

	AS_IF([test x"$FREEIPMI_VERSION" != xnone],
		[depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libfreeipmi libipmimonitoring 2>/dev/null`"
		 depLIBS="`$PKG_CONFIG --silence-errors --libs libfreeipmi libipmimonitoring 2>/dev/null`"
		],
		[depCFLAGS=""
		 depLIBS="-lfreeipmi -lipmimonitoring"
		]
	)

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
			depCFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${depCFLAGS}])

	AC_MSG_CHECKING(for FreeIPMI ldflags)
	AC_ARG_WITH(freeipmi-libs,
		AS_HELP_STRING([@<:@--with-freeipmi-libs=LIBS@:>@], [linker flags for the FreeIPMI library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-freeipmi-libs - see docs/configure.txt)
			;;
		*)
			depLIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${depLIBS}])

	dnl check if freeipmi is usable with our current flags
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_CHECK_HEADERS(freeipmi/freeipmi.h, [nut_have_freeipmi=yes], [nut_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_HEADERS(ipmi_monitoring.h, [], [nut_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
	AC_SEARCH_LIBS([ipmi_ctx_create], [freeipmi], [], [nut_have_freeipmi=no])
	dnl when version cannot be tested (prior to 1.0.5, with no pkg-config)
	dnl we have to check for some specific functions
	AC_SEARCH_LIBS([ipmi_ctx_find_inband], [freeipmi], [], [nut_have_freeipmi=no])

	AC_SEARCH_LIBS([ipmi_monitoring_init], [ipmimonitoring], [nut_have_freeipmi_monitoring=yes], [nut_have_freeipmi_monitoring=no])
	AC_SEARCH_LIBS([ipmi_monitoring_sensor_read_record_id], [ipmimonitoring], [], [nut_have_freeipmi_monitoring=no])

	dnl Check for FreeIPMI 1.1.X / 1.2.X which implies API changes!
	AC_SEARCH_LIBS([ipmi_sdr_cache_ctx_destroy], [freeipmi], [nut_have_freeipmi_11x_12x=no], [])
	AC_SEARCH_LIBS([ipmi_sdr_ctx_destroy], [freeipmi], [nut_have_freeipmi_11x_12x=yes], [nut_have_freeipmi_11x_12x=no])

	dnl Collect possibly updated dependencies after AC SEARCH LIBS:
	AS_IF([test x"${LIBS}" != x"${LIBS_ORIG} ${depLIBS}"], [
		AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
			depLIBS="`echo "$LIBS" | sed -e 's|'"${LIBS_ORIG}"'| |' -e 's|^ *||' -e 's| *$||'`"
		])
	])

	if test "${nut_have_freeipmi}" = "yes"; then
		nut_with_ipmi="yes"
		nut_ipmi_lib="(FreeIPMI)"
		nut_have_libipmi="yes"
		AC_DEFINE(HAVE_FREEIPMI, 1, [Define if FreeIPMI support is available])
		LIBIPMI_CFLAGS="${depCFLAGS}"
		LIBIPMI_LIBS="${depLIBS}"

		dnl Help ltdl if we can (nut-scanner etc.)
		for TOKEN in $depLIBS ; do
			AS_CASE(["${TOKEN}"],
				[-l*ipmi*], [
					AX_REALPATH_LIB([${TOKEN}], [SOPATH_LIBFREEIPMI], [])
					AS_IF([test -n "${SOPATH_LIBFREEIPMI}" && test -s "${SOPATH_LIBFREEIPMI}"], [
						AC_DEFINE_UNQUOTED([SOPATH_LIBFREEIPMI],["${SOPATH_LIBFREEIPMI}"],[Path to dynamic library on build system])
						SOFILE_LIBFREEIPMI="`basename "$SOPATH_LIBFREEIPMI"`"
						AC_DEFINE_UNQUOTED([SOFILE_LIBFREEIPMI],["${SOFILE_LIBFREEIPMI}"],[Base file name of dynamic library on build system])
						break
					])
				]
			)
		done
		unset TOKEN
	fi

	if test "${nut_have_freeipmi_11x_12x}" = "yes"; then
		AC_DEFINE(HAVE_FREEIPMI_11X_12X, 1, [Define if FreeIPMI 1.1.X / 1.2.X support is available])
	fi

	if test "${nut_have_freeipmi_monitoring}" = "yes"; then
		AC_DEFINE(HAVE_FREEIPMI_MONITORING, 1, [Define if FreeIPMI monitoring support is available])
	fi

	unset depCFLAGS
	unset depLIBS

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
