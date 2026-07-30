dnl Check for LIBSYSTEMD compiler flags. On success, set nut_have_libsystemd="yes"
dnl and set LIBSYSTEMD_CFLAGS and LIBSYSTEMD_LIBS. On failure, set
dnl nut_have_libsystemd="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBSYSTEMD],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libsystemd_seen}"], [
        nut_have_libsystemd_seen=yes

        AC_CHECK_TOOL(SYSTEMCTL, systemctl, none)
        NUT_ARG_WITH_LIBOPTS_INCLUDES([libsystemd], [auto], [systemd])
        NUT_ARG_WITH_LIBOPTS_LIBS([libsystemd], [auto], [systemd])

        nut_noncv_checked_libsystemd_now=no
        AC_CACHE_VAL([nut_cv_checked_libsystemd], [
            nut_noncv_checked_libsystemd_now=yes

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

            nut_cv_SYSTEMD_VERSION="none"

            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                dnl See which version of the systemd library (if any) is installed
                dnl FIXME : Support detection of cflags/ldflags below by legacy
                dnl discovery if pkgconfig is not there
                AC_MSG_CHECKING(for libsystemd version via pkg-config)
                nut_cv_SYSTEMD_VERSION="`$PKG_CONFIG --silence-errors --modversion libsystemd 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_SYSTEMD_VERSION}"], [
                    nut_cv_SYSTEMD_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_SYSTEMD_VERSION} found)
            ])

            AS_IF([test x"${nut_cv_SYSTEMD_VERSION}" = xnone], [
                 AS_IF([test x"${SYSTEMCTL}" != xnone], [
                     AC_MSG_CHECKING(for libsystemd version via systemctl)
                     dnl NOTE: Unlike the configure.ac file, in a "pure"
                     dnl m4 script like this one, we have to escape the
                     dnl dollar-number references (in awk below) lest they
                     dnl get seen as m4 function positional parameters.
                     nut_cv_SYSTEMD_VERSION="`LANG=C LC_ALL=C ${SYSTEMCTL} --version | ${EGREP} '^systemd@<:@ \t@:>@*@<:@0-9@:>@@<:@0-9@:>@*' | awk '{print ''$''2}'`" \
                     && test -n "${nut_cv_SYSTEMD_VERSION}" \
                     || nut_cv_SYSTEMD_VERSION="none"
                     AC_MSG_RESULT(${nut_cv_SYSTEMD_VERSION} found)
                 ])
            ])

            AS_IF([test x"${nut_cv_SYSTEMD_VERSION}" = xnone], [
                AC_MSG_NOTICE([can not check libsystemd settings via pkg-config nor systemctl])
            ])

            AC_MSG_CHECKING(for libsystemd cflags)
            AS_CASE([${nut_with_libsystemd_includes}],
                [auto], [
                    dnl Not specifying a default include path here,
                    dnl headers are referenced by relative directory
                    dnl and these should be in OS location usually.
                    AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libsystemd 2>/dev/null`" \
                              && depCFLAGS_SOURCE="pkg-config" ; } \
                         || { depCFLAGS="" \
                              && depCFLAGS_SOURCE="default" ; }
                        ], [
                            depCFLAGS=""
                            depCFLAGS_SOURCE="default"
                        ])
                    ],
                    [depCFLAGS="${nut_with_libsystemd_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for libsystemd ldflags)
            AS_CASE([${nut_with_libsystemd_libs}],
                [auto], [
                    AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depLIBS="`$PKG_CONFIG --silence-errors --libs libsystemd 2>/dev/null`" \
                              && depLIBS_SOURCE="pkg-config" ; } \
                         || { depLIBS="-lsystemd" \
                              && depLIBS_SOURCE="default" ; }
                        ], [
                            depLIBS="-lsystemd"
                            depLIBS_SOURCE="default"
                       ])
                    ],
                    [depLIBS="${nut_with_libsystemd_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if libsystemd is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(systemd/sd-daemon.h, [nut_cv_have_libsystemd=yes], [nut_cv_have_libsystemd=no], [AC_INCLUDES_DEFAULT])
            AC_CHECK_FUNCS(sd_notify, [], [nut_cv_have_libsystemd=no])

            nut_cv_have_libsystemd_inhibitor=no
            nut_cv_LIBSYSTEMD_CFLAGS=""
            nut_cv_LIBSYSTEMD_LIBS=""
            AS_IF([test x"${nut_cv_have_libsystemd}" = x"yes"], [
                dnl Check for additional feature support in library (optional)
                AC_CHECK_FUNCS(sd_booted sd_watchdog_enabled sd_notify_barrier)
                nut_cv_LIBSYSTEMD_CFLAGS="${depCFLAGS}"
                nut_cv_LIBSYSTEMD_LIBS="${depLIBS}"

                dnl Since systemd 183: https://systemd.io/INHIBITOR_LOCKS/
                dnl ...or 221: https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html
                dnl and some bits even later (e.g. message container reading)
                AS_IF([test "$nut_cv_SYSTEMD_VERSION" -ge 221], [
                    nut_cv_have_libsystemd_inhibitor=yes
                    AC_CHECK_HEADERS(systemd/sd-bus.h, [], [nut_cv_have_libsystemd_inhibitor=no], [AC_INCLUDES_DEFAULT])
                    AC_CHECK_FUNCS([sd_bus_call_method sd_bus_message_read_basic sd_bus_open_system sd_bus_default_system sd_bus_get_property_trivial], [], [nut_cv_have_libsystemd_inhibitor=no])
                    dnl NOTE: In practice we use "p"-suffixed sd_bus_flush_close_unrefp
                    dnl  and sd_bus_message_unrefp methods prepared by a macro in sd-bus.h
                    AC_CHECK_FUNCS([sd_bus_flush_close_unref sd_bus_message_unref sd_bus_error_free], [], [nut_cv_have_libsystemd_inhibitor=no])
                    dnl Optional methods: nicer with them, can do without
                    AC_CHECK_FUNCS([sd_bus_open_system_with_description sd_bus_set_description])
                    dnl For inhibitor per se, we do not have to read containers:
                    dnl AC_CHECK_FUNCS([sd_bus_message_enter_container sd_bus_message_exit_container])
                ])

                AC_MSG_CHECKING(for libsystemd inhibitor interface support)
                AC_MSG_RESULT([${nut_cv_have_libsystemd_inhibitor}])
            ])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBSYSTEMD_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBSYSTEMD_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_have_libsystemd], [])
            AC_CACHE_VAL([nut_cv_have_libsystemd_inhibitor], [])
            AC_CACHE_VAL([nut_cv_SYSTEMD_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBSYSTEMD_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBSYSTEMD_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBSYSTEMD_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBSYSTEMD_LIBS_SOURCE], [])

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
            nut_cv_checked_libsystemd="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libsystemd}" = xyes], [
            nut_have_libsystemd="${nut_cv_have_libsystemd}"
            nut_have_libsystemd_inhibitor="${nut_cv_have_libsystemd_inhibitor}"

            AS_IF([test "${nut_noncv_checked_libsystemd_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBSYSTEMD_CFLAGS}"
                LIBS="${nut_cv_LIBSYSTEMD_LIBS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(systemd/sd-daemon.h, [], [nut_have_libsystemd=no], [AC_INCLUDES_DEFAULT])
                AC_CHECK_FUNCS(sd_notify sd_booted sd_watchdog_enabled sd_notify_barrier, [], [nut_cv_have_libsystemd=no])
                AS_IF([test "$nut_cv_SYSTEMD_VERSION" -ge 221], [
                    AC_CHECK_HEADERS(systemd/sd-bus.h, [], [nut_have_libsystemd_inhibitor=no], [AC_INCLUDES_DEFAULT])
                    AC_CHECK_FUNCS([sd_bus_call_method sd_bus_message_read_basic sd_bus_open_system sd_bus_default_system sd_bus_get_property_trivial sd_bus_flush_close_unref sd_bus_message_unref sd_bus_error_free], [], [nut_cv_have_libsystemd_inhibitor=no])
                    AC_CHECK_FUNCS([sd_bus_open_system_with_description sd_bus_set_description])
                    dnl AC_CHECK_FUNCS([sd_bus_message_enter_container sd_bus_message_exit_container])
                ])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                LIBS="${LIBS_ORIG}"
                unset CFLAGS_ORIG
                unset LIBS_ORIG
            ])

            dnl ### AS_IF([test "${nut_have_libsystemd}" = "yes"], [
            dnl ###     AC_DEFINE(HAVE_LIBSYSTEMD, 1, [Define to enable libsystemd support])
            dnl ### ])

            LIBSYSTEMD_CFLAGS="${nut_cv_LIBSYSTEMD_CFLAGS}"
            LIBSYSTEMD_LIBS="${nut_cv_LIBSYSTEMD_LIBS}"

            dnl For troubleshooting of re-runs, mostly:
            SYSTEMD_VERSION="${nut_cv_SYSTEMD_VERSION}"
            LIBSYSTEMD_CFLAGS_SOURCE="${nut_cv_LIBSYSTEMD_CFLAGS_SOURCE}"
            LIBSYSTEMD_LIBS_SOURCE="${nut_cv_LIBSYSTEMD_LIBS_SOURCE}"

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libsystemd_now}" = no], [
                AC_MSG_NOTICE([libsystemd (cached): ${nut_have_libsystemd} (inhibitor:${nut_have_libsystemd_inhibitor})])
                AC_MSG_NOTICE([libsystemd (cached): version='${SYSTEMD_VERSION}' cflags_source='${LIBSYSTEMD_CFLAGS_SOURCE}' libs_source='${LIBSYSTEMD_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libsystemd (cached): LIBSYSTEMD_CFLAGS='${LIBSYSTEMD_CFLAGS}'])
                AC_MSG_NOTICE([libsystemd (cached): LIBSYSTEMD_LIBS='${LIBSYSTEMD_LIBS}'])
            ])
        ])
    ])
])
