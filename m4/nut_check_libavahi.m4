dnl Check for LIBAVAHI compiler flags. On success, set nut_have_avahi="yes"
dnl and set LIBAVAHI_CFLAGS and LIBAVAHI_LIBS. On failure, set
dnl nut_have_avahi="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBAVAHI],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_avahi_seen}"], [
        nut_have_avahi_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([avahi], [auto])
        NUT_ARG_WITH_LIBOPTS_LIBS([avahi], [auto])

        nut_noncv_checked_libavahi_now=no
        AC_CACHE_VAL([nut_cv_checked_libavahi], [
            nut_noncv_checked_libavahi_now=yes

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

            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                dnl See which version of the avahi library (if any) is installed
                AC_MSG_CHECKING(for avahi-core version via pkg-config (0.6.30 minimum required))
                nut_cv_AVAHI_CORE_VERSION="`$PKG_CONFIG --silence-errors --modversion avahi-core 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_AVAHI_CORE_VERSION}"], [
                    nut_cv_AVAHI_CORE_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_AVAHI_CORE_VERSION} found)

                AC_MSG_CHECKING(for avahi-client version via pkg-config (0.6.30 minimum required))
                nut_cv_AVAHI_CLIENT_VERSION="`$PKG_CONFIG --silence-errors --modversion avahi-client 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_AVAHI_CLIENT_VERSION}"], [
                    nut_cv_AVAHI_CLIENT_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_AVAHI_CLIENT_VERSION} found)
            ], [
                nut_cv_AVAHI_CORE_VERSION="none"
                nut_cv_AVAHI_CLIENT_VERSION="none"
                AC_MSG_NOTICE([can not check avahi settings via pkg-config])
            ])

            AC_MSG_CHECKING(for avahi cflags)
            AS_CASE([${nut_with_avahi_includes}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags avahi-core avahi-client 2>/dev/null`" \
                              && depCFLAGS_SOURCE="pkg-config" ; } \
                         || { depCFLAGS="-I/usr/local/include -I/usr/include -L/usr/local/lib -L/usr/lib" \
                              && depCFLAGS_SOURCE="default" ; }],
                        [depCFLAGS="-I/usr/local/include -I/usr/include -L/usr/local/lib -L/usr/lib"
                         depCFLAGS_SOURCE="default"]
                    )],
                        [depCFLAGS="${nut_with_avahi_includes}"
                         depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for avahi ldflags)
            AS_CASE([${nut_with_avahi_libs}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depLIBS="`$PKG_CONFIG --silence-errors --libs avahi-core avahi-client 2>/dev/null`" \
                              && depLIBS_SOURCE="pkg-config" ; } \
                         || { depLIBS="-lavahi-core -lavahi-client" \
                              && depLIBS_SOURCE="default" ; }],
                        [depLIBS="-lavahi-core -lavahi-client"
                         depLIBS_SOURCE="default"]
                    )],
                        [depLIBS="${nut_with_avahi_libs}"
                         depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if avahi-core is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(avahi-common/malloc.h, [nut_cv_have_avahi=yes], [nut_cv_have_avahi=no], [AC_INCLUDES_DEFAULT])
            AC_CHECK_FUNCS(avahi_free, [], [nut_cv_have_avahi=no])

            nut_cv_LIBAVAHI_CFLAGS=""
            nut_cv_LIBAVAHI_LIBS=""
            nut_cv_SOFILE_LIBAVAHI=""
            nut_cv_SOPATH_LIBAVAHI=""
            AS_IF([test "${nut_cv_have_avahi}" = "yes"], [
                dnl check if avahi-client is usable
                AC_CHECK_HEADERS(avahi-client/client.h, [nut_cv_have_avahi=yes], [nut_cv_have_avahi=no], [AC_INCLUDES_DEFAULT])
                AC_CHECK_FUNCS(avahi_client_new, [], [nut_cv_have_avahi=no])
                AS_IF([test "${nut_cv_have_avahi}" = "yes"], [
                    nut_cv_LIBAVAHI_CFLAGS="${depCFLAGS}"
                    nut_cv_LIBAVAHI_LIBS="${depLIBS}"
                ])

                dnl Help ltdl if we can (nut-scanner etc.)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*avahi*client*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBAVAHI], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBAVAHI}" && test -s "${nut_cv_SOPATH_LIBAVAHI}"], [
                                nut_cv_SOFILE_LIBAVAHI="`basename \"${nut_cv_SOPATH_LIBAVAHI}\"`"
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBAVAHI_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBAVAHI_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_have_libavahi], [])
            AC_CACHE_VAL([nut_cv_AVAHI_CORE_VERSION], [])
            AC_CACHE_VAL([nut_cv_AVAHI_CLIENT_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBAVAHI_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBAVAHI_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBAVAHI_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBAVAHI_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBAVAHI], [])
            AC_CACHE_VAL([nut_cv_SOPATH_LIBAVAHI], [])

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
            nut_cv_checked_libavahi="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libavahi}" = xyes], [
            nut_have_avahi="${nut_cv_have_avahi}"

            AS_IF([test "${nut_noncv_checked_libavahi_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBAVAHI_CFLAGS}"
                LIBS="${nut_cv_LIBAVAHI_LIBS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(avahi-common/malloc.h, [], [nut_have_avahi=no], [AC_INCLUDES_DEFAULT])
                AC_CHECK_FUNCS(avahi_free, [], [nut_have_avahi=no])
                AS_IF([test "${nut_have_avahi}" = "yes"], [
                    AC_CHECK_HEADERS(avahi-client/client.h, [], [nut_have_avahi=no], [AC_INCLUDES_DEFAULT])
                    AC_CHECK_FUNCS(avahi_client_new, [], [nut_have_avahi=no])
                ])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                unset CFLAGS_ORIG
                LIBS="${LIBS_ORIG}"
                unset LIBS_ORIG
            ])

            LIBAVAHI_CFLAGS="${nut_cv_LIBAVAHI_CFLAGS}"
            LIBAVAHI_LIBS="${nut_cv_LIBAVAHI_LIBS}"

            dnl For nut-scanner style autoloading:
            SOPATH_LIBAVAHI="${nut_cv_SOPATH_LIBAVAHI}"
            SOFILE_LIBAVAHI="${nut_cv_SOFILE_LIBAVAHI}"

            dnl For troubleshooting of re-runs, mostly:
            LIBAVAHI_CFLAGS_SOURCE="${nut_cv_LIBAVAHI_CFLAGS_SOURCE}"
            LIBAVAHI_LIBS_SOURCE="${nut_cv_LIBAVAHI_LIBS_SOURCE}"
            AVAHI_CORE_VERSION="${nut_cv_AVAHI_CORE_VERSION}"
            AVAHI_CLIENT_VERSION="${nut_cv_AVAHI_CLIENT_VERSION}"

            AS_IF([test "${nut_have_avahi}" = "yes"], [
                AC_DEFINE(HAVE_AVAHI, 1, [Define to enable libavahi (core and client) support])
                AS_IF([test -n "${SOPATH_LIBAVAHI}" && test -s "${SOPATH_LIBAVAHI}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBAVAHI], ["${SOPATH_LIBAVAHI}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBAVAHI], ["${SOFILE_LIBAVAHI}"], [Base file name of dynamic library on build system])
                ])
            ])

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libavahi_now}" = no], [
                AC_MSG_NOTICE([libavahi (cached): ${nut_have_avahi} (core:${AVAHI_CORE_VERSION} client:${AVAHI_CLIENT_VERSION})])
                AC_MSG_NOTICE([libavahi (cached): cflags_source='${LIBAVAHI_CFLAGS_SOURCE}' libs_source='${LIBAVAHI_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libavahi (cached): LIBAVAHI_CFLAGS='${LIBAVAHI_CFLAGS}'])
                AC_MSG_NOTICE([libavahi (cached): LIBAVAHI_LIBS='${LIBAVAHI_LIBS}'])
                AC_MSG_NOTICE([libavahi (cached): SOFILE:'${SOFILE_LIBAVAHI}', SOPATH:'${SOPATH_LIBAVAHI}'])
            ])
        ])
    ])
])
