dnl Check for FreeIPMI (LIBFREEIPMI) compiler flags. On success, set
dnl nut_have_freeipmi="yes" and nut_ipmi_lib="FreeIPMI", and define WITH_IPMI,
dnl WITH_FREEIPMI, LIBIPMI_CFLAGS and LIBIPMI_LIBS. On failure, set
dnl nut_have_freeipmi="no".
dnl This macro can be run multiple times, but will do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBFREEIPMI],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libfreeipmi_seen}"], [
        nut_have_libfreeipmi_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([FreeIPMI], [auto])
        NUT_ARG_WITH_LIBOPTS_LIBS([FreeIPMI], [auto])

        nut_noncv_checked_libfreeipmi_now=no
        AC_CACHE_VAL([nut_cv_checked_libfreeipmi], [
            nut_noncv_checked_libfreeipmi_now=yes

            AC_REQUIRE([NUT_CHECK_PKGCONFIG])

            dnl save CFLAGS and LIBS
            CFLAGS_ORIG="${CFLAGS}"
            LIBS_ORIG="${LIBS}"
            CFLAGS=""
            LIBS=""
            depCFLAGS=""
            depCFLAGS_SOURCE=""
            depLIBS=""
            depLIBS_SOURCE=""

            dnl Magic-format string to hold chosen IPMI library version and its config-source
            nut_cv_ipmi_lib=""
            nut_cv_have_libipmi=""

            dnl Start with what the build args said:
            nut_cv_with_ipmi="${nut_with_ipmi}"

            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                dnl pkg-config support requires Freeipmi 1.0.5, released on Thu Jun 30 2011
                dnl but NUT should only require 0.8.5 (for nut-scanner) and 1.0.1 (for
                dnl nut-ipmipsu) (comment from upstream Al Chu)
                AC_MSG_CHECKING(for FreeIPMI version via pkg-config)
                nut_cv_FREEIPMI_VERSION="`$PKG_CONFIG --silence-errors --modversion libfreeipmi 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_FREEIPMI_VERSION}"], [
                    nut_cv_FREEIPMI_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_FREEIPMI_VERSION} found)
            ], [
                nut_cv_FREEIPMI_VERSION="none"
                AC_MSG_NOTICE([can not check FreeIPMI settings via pkg-config])
            ])

            AS_IF([test x"${nut_cv_FREEIPMI_VERSION}" != xnone], [
                depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libfreeipmi libipmimonitoring 2>/dev/null`"
                depLIBS="`$PKG_CONFIG --silence-errors --libs libfreeipmi libipmimonitoring 2>/dev/null`"
                depCFLAGS_SOURCE="pkg-config"
                depLIBS_SOURCE="pkg-config"
            ], [
                depCFLAGS=""
                depLIBS="-lfreeipmi -lipmimonitoring"
                depCFLAGS_SOURCE="default"
                depLIBS_SOURCE="default"
            ])

            dnl allow overriding FreeIPMI settings if the user knows best
            AC_MSG_CHECKING(for FreeIPMI cflags)
            AS_CASE([${nut_with_freeipmi_includes}],
                [auto], [],	dnl Keep what we had found above
                    [depCFLAGS="${nut_with_freeipmi_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for FreeIPMI ldflags)
            AS_CASE([${nut_with_freeipmi_libs}],
                [auto], [],	dnl Keep what we had found above
                    [depLIBS="${nut_with_freeipmi_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if freeipmi is usable with our current flags
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(freeipmi/freeipmi.h, [nut_cv_have_freeipmi=yes], [nut_cv_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
            AC_CHECK_HEADERS(ipmi_monitoring.h, [], [nut_cv_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
            AC_SEARCH_LIBS([ipmi_ctx_create], [freeipmi], [], [nut_cv_have_freeipmi=no])
            dnl when version cannot be tested (prior to 1.0.5, with no pkg-config)
            dnl we have to check for some specific functions
            AC_SEARCH_LIBS([ipmi_ctx_find_inband], [freeipmi], [], [nut_cv_have_freeipmi=no])

            AC_SEARCH_LIBS([ipmi_monitoring_init], [ipmimonitoring], [nut_cv_have_freeipmi_monitoring=yes], [nut_cv_have_freeipmi_monitoring=no])
            AC_SEARCH_LIBS([ipmi_monitoring_sensor_read_record_id], [ipmimonitoring], [], [nut_cv_have_freeipmi_monitoring=no])

            dnl Check for FreeIPMI 1.1.X / 1.2.X which implies API changes!
            AC_SEARCH_LIBS([ipmi_sdr_cache_ctx_destroy], [freeipmi], [nut_cv_have_freeipmi_11x_12x=no], [])
            AC_SEARCH_LIBS([ipmi_sdr_ctx_destroy], [freeipmi], [nut_cv_have_freeipmi_11x_12x=yes], [nut_cv_have_freeipmi_11x_12x=no])

            dnl Collect possibly updated dependencies after AC SEARCH LIBS:
            AS_IF([test x"${LIBS}" != x"${LIBS_ORIG} ${depLIBS}"], [
                AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
                    depLIBS="`echo \"$LIBS\" | sed -e 's|'\"${LIBS_ORIG}\"'| |' -e 's|^ *||' -e 's| *$||'`"
                ])
            ])

            nut_cv_LIBIPMI_CFLAGS=""
            nut_cv_LIBIPMI_LIBS=""
            nut_cv_SOFILE_LIBFREEIPMI=""
            nut_cv_SOPATH_LIBFREEIPMI=""
            AS_IF([test "${nut_cv_have_freeipmi}" = "yes"], [
                nut_cv_with_ipmi="yes"
                nut_cv_ipmi_lib="(FreeIPMI)"
                nut_cv_have_libipmi="yes"
                nut_cv_LIBIPMI_CFLAGS="${depCFLAGS}"
                nut_cv_LIBIPMI_LIBS="${depLIBS}"

                dnl Help ltdl if we can (nut-scanner etc.)
                dnl Note we can have e.g. `-lfreeipmi -lipmimonitoring` with
                dnl one including the other, so should try to prefer the
                dnl "outer" linked library (libipmimonitoring here). (FIXME!)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*ipmi*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBFREEIPMI], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBFREEIPMI}" && test -s "${nut_cv_SOPATH_LIBFREEIPMI}"], [
                                nut_cv_SOFILE_LIBFREEIPMI="`basename \"${nut_cv_SOPATH_LIBFREEIPMI}\"`"
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBIPMI_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBIPMI_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_with_ipmi], [])
            AC_CACHE_VAL([nut_cv_ipmi_lib], [])
            AC_CACHE_VAL([nut_cv_have_libipmi], [])

            AC_CACHE_VAL([nut_cv_have_freeipmi], [])
            AC_CACHE_VAL([nut_cv_have_freeipmi_monitoring], [])
            AC_CACHE_VAL([nut_cv_have_freeipmi_11x_12x], [])
            AC_CACHE_VAL([nut_cv_FREEIPMI_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBIPMI_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBIPMI_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBIPMI_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBIPMI_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBFREEIPMI], [])
            AC_CACHE_VAL([nut_cv_SOPATH_LIBFREEIPMI], [])

            unset depCFLAGS
            unset depLIBS
            unset depCFLAGS_SOURCE
            unset depLIBS_SOURCE

            dnl restore original CFLAGS and LIBS
            CFLAGS="${CFLAGS_ORIG}"
            LIBS="${LIBS_ORIG}"

            unset CFLAGS_ORIG
            unset LIBS_ORIG

            dnl Complete the cache ritual
            nut_cv_checked_libfreeipmi="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libfreeipmi}" = xyes], [
            dnl NOTE: This one may differ from initial build argument (e.g. "auto")
            nut_with_ipmi="${nut_cv_with_ipmi}"

            nut_ipmi_lib="${nut_cv_ipmi_lib}"
            nut_have_libipmi="${nut_cv_have_libipmi}"

            nut_have_freeipmi="${nut_cv_have_freeipmi}"
            nut_have_freeipmi_monitoring="${nut_cv_have_freeipmi_monitoring}"
            nut_have_freeipmi_11x_12x="${nut_cv_have_freeipmi_11x_12x}"

            AS_IF([test "${nut_noncv_checked_libfreeipmi_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBIPMI_CFLAGS}"
                LIBS="${nut_cv_LIBIPMI_LIBS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(freeipmi/freeipmi.h, [], [nut_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
                AC_CHECK_HEADERS(ipmi_monitoring.h, [], [nut_have_freeipmi=no], [AC_INCLUDES_DEFAULT])
                AS_IF([test "${nut_have_freeipmi}" = "yes"], [
                    AC_SEARCH_LIBS([ipmi_ctx_create], [freeipmi], [], [nut_have_freeipmi=no])
                    AC_SEARCH_LIBS([ipmi_ctx_find_inband], [freeipmi], [], [nut_have_freeipmi=no])

                    AC_SEARCH_LIBS([ipmi_monitoring_init], [ipmimonitoring], [], [nut_have_freeipmi_monitoring=no])
                    AC_SEARCH_LIBS([ipmi_monitoring_sensor_read_record_id], [ipmimonitoring], [], [nut_have_freeipmi_monitoring=no])

                    AC_SEARCH_LIBS([ipmi_sdr_cache_ctx_destroy], [freeipmi], [nut_have_freeipmi_11x_12x=no], [])
                    AC_SEARCH_LIBS([ipmi_sdr_ctx_destroy], [freeipmi], [], [nut_have_freeipmi_11x_12x=no])
                ])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                LIBS="${LIBS_ORIG}"
                unset CFLAGS_ORIG
                unset LIBS_ORIG
            ])

            LIBIPMI_CFLAGS="${nut_cv_LIBIPMI_CFLAGS}"
            LIBIPMI_LIBS="${nut_cv_LIBIPMI_LIBS}"

            dnl For nut-scanner style autoloading:
            SOPATH_LIBFREEIPMI="${nut_cv_SOPATH_LIBFREEIPMI}"
            SOFILE_LIBFREEIPMI="${nut_cv_SOFILE_LIBFREEIPMI}"

            dnl For troubleshooting of re-runs, mostly:
            LIBIPMI_CFLAGS_SOURCE="${nut_cv_LIBIPMI_CFLAGS_SOURCE}"
            LIBIPMI_LIBS_SOURCE="${nut_cv_LIBIPMI_LIBS_SOURCE}"
            FREEIPMI_VERSION="${nut_cv_FREEIPMI_VERSION}"

            AS_IF([test "${nut_have_freeipmi}" = "yes"], [
                AC_DEFINE(HAVE_FREEIPMI, 1, [Define if FreeIPMI support is available])
                AS_IF([test -n "${SOPATH_LIBFREEIPMI}" && test -s "${SOPATH_LIBFREEIPMI}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBFREEIPMI], ["${SOPATH_LIBFREEIPMI}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBFREEIPMI], ["${SOFILE_LIBFREEIPMI}"], [Base file name of dynamic library on build system])
                ])
            ])

            AS_IF([test "${nut_have_freeipmi_11x_12x}" = "yes"], [
                AC_DEFINE(HAVE_FREEIPMI_11X_12X, 1, [Define if FreeIPMI 1.1.X / 1.2.X support is available])
            ])

            AS_IF([test "${nut_have_freeipmi_monitoring}" = "yes"], [
                AC_DEFINE(HAVE_FREEIPMI_MONITORING, 1, [Define if FreeIPMI monitoring support is available])
            ])

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libfreeipmi_now}" = no], [
                AC_MSG_NOTICE([Some IPMI library (cached): ${nut_cv_have_libipmi} (${nut_cv_ipmi_lib})])
                AC_MSG_NOTICE([libfreeipmi (cached): ${nut_have_freeipmi} (${FREEIPMI_VERSION})])
                AC_MSG_NOTICE([libfreeipmi (cached): monitoring:${nut_cv_have_freeipmi_monitoring}, 11x_12x:${nut_cv_have_freeipmi_11x_12x}])
                AC_MSG_NOTICE([libfreeipmi (cached): cflags_source='${LIBIPMI_CFLAGS_SOURCE}' libs_source='${LIBIPMI_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libfreeipmi (cached): LIBIPMI_CFLAGS='${LIBIPMI_CFLAGS}'])
                AC_MSG_NOTICE([libfreeipmi (cached): LIBIPMI_LIBS='${LIBIPMI_LIBS}'])
                AC_MSG_NOTICE([libfreeipmi (cached): SOFILE:'${SOFILE_LIBFREEIPMI}', SOPATH:'${SOPATH_LIBFREEIPMI}'])
            ])
        ])
    ])
])
