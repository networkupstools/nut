dnl Check for LIBNEON compiler flags. On success, set nut_have_neon="yes"
dnl and set LIBNEON_CFLAGS and LIBNEON_LIBS. On failure, set
dnl nut_have_neon="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBNEON],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_neon_seen}"], [
        nut_have_neon_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([neon], [auto])
        NUT_ARG_WITH_LIBOPTS_LIBS([neon], [auto])

        nut_noncv_checked_libneon_now=no
        AC_CACHE_VAL([nut_cv_checked_libneon], [
            nut_noncv_checked_libneon_now=yes

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
                dnl See which version of the neon library (if any) is installed
                dnl FIXME : Support detection of cflags/ldflags below by legacy
                dnl discovery if pkgconfig is not there
                AC_MSG_CHECKING(for libneon version via pkg-config (0.25.0 minimum required))
                nut_cv_NEON_VERSION="`$PKG_CONFIG --silence-errors --modversion neon 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_NEON_VERSION}"], [
                    nut_cv_NEON_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_NEON_VERSION} found)
            ], [
                nut_cv_NEON_VERSION="none"
                AC_MSG_NOTICE([can not check libneon settings via pkg-config])
            ])

            AC_MSG_CHECKING(for libneon cflags)
            AS_CASE([${nut_with_neon_includes}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags neon 2>/dev/null`" \
                              && depCFLAGS_SOURCE="pkg-config" ; } \
                         || { depCFLAGS="-I/usr/include/neon -I/usr/local/include/neon" \
                              && depCFLAGS_SOURCE="default" ; }
                        ], [
                            depCFLAGS="-I/usr/include/neon -I/usr/local/include/neon"
                            depCFLAGS_SOURCE="default"
                        ])
                    ],
                        [depCFLAGS="${nut_with_neon_includes}"
                         depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for libneon ldflags)
            AS_CASE([${nut_with_neon_libs}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depLIBS="`$PKG_CONFIG --silence-errors --libs neon 2>/dev/null`" \
                              && depLIBS_SOURCE="pkg-config" ; } \
                         || { depLIBS="-lneon" \
                              && depLIBS_SOURCE="default" ; }
                        ], [
                            depLIBS="-lneon"
                            depLIBS_SOURCE="default"
                        ])
                    ],
                        [depLIBS="${nut_with_neon_libs}"
                         depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if neon is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(ne_xmlreq.h, [nut_cv_have_neon=yes], [nut_cv_have_neon=no], [AC_INCLUDES_DEFAULT])
            AC_CHECK_FUNCS(ne_xml_dispatch_request, [], [nut_cv_have_neon=no])

            nut_cv_LIBNEON_CFLAGS=""
            nut_cv_LIBNEON_LIBS=""
            nut_cv_SOFILE_LIBNEON=""
            nut_cv_SOPATH_LIBNEON=""
            AS_IF([test "${nut_cv_have_neon}" = "yes"], [
                dnl Check for connect timeout support in library (optional)
                AC_CHECK_FUNCS(ne_set_connect_timeout ne_sock_connect_timeout)
                nut_cv_LIBNEON_CFLAGS="${depCFLAGS}"
                nut_cv_LIBNEON_LIBS="${depLIBS}"

                dnl Help neon if we can (nut-scanner etc.)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*neon*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBNEON], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBNEON}" && test -s "${nut_cv_SOPATH_LIBNEON}"], [
                                nut_cv_SOFILE_LIBNEON="`basename \"${nut_cv_SOPATH_LIBNEON}\"`"
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBNEON_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBNEON_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_have_libneon], [])
            AC_CACHE_VAL([nut_cv_NEON_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBNEON_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBNEON_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBNEON_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBNEON_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBNEON], [])
            AC_CACHE_VAL([nut_cv_SOPATH_LIBNEON], [])

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
            nut_cv_checked_libneon="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libneon}" = xyes], [
            nut_have_neon="${nut_cv_have_neon}"

            AS_IF([test "${nut_noncv_checked_libneon_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                CFLAGS="${nut_cv_LIBNEON_CFLAGS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(ne_xmlreq.h, [], [nut_have_neon=no], [AC_INCLUDES_DEFAULT])
                AC_CHECK_FUNCS(ne_xml_dispatch_request, [], [nut_have_neon=no])
                AS_IF([test "${nut_have_neon}" = "yes"], [
                    AC_CHECK_FUNCS(ne_set_connect_timeout ne_sock_connect_timeout)
                ])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                unset CFLAGS_ORIG
            ])

            LIBNEON_CFLAGS="${nut_cv_LIBNEON_CFLAGS}"
            LIBNEON_LIBS="${nut_cv_LIBNEON_LIBS}"

            dnl For nut-scanner style autoloading:
            SOPATH_LIBNEON="${nut_cv_SOPATH_LIBNEON}"
            SOFILE_LIBNEON="${nut_cv_SOFILE_LIBNEON}"

            dnl For troubleshooting of re-runs, mostly:
            LIBNEON_CFLAGS_SOURCE="${nut_cv_LIBNEON_CFLAGS_SOURCE}"
            LIBNEON_LIBS_SOURCE="${nut_cv_LIBNEON_LIBS_SOURCE}"
            NEON_VERSION="${nut_cv_NEON_VERSION}"

            AS_IF([test "${nut_have_neon}" = "yes"], [
                AC_DEFINE(HAVE_NEON, 1, [Define to enable libneon support])
                AS_IF([test -n "${SOPATH_LIBNEON}" && test -s "${SOPATH_LIBNEON}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBNEON], ["${SOPATH_LIBNEON}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBNEON], ["${SOFILE_LIBNEON}"], [Base file name of dynamic library on build system])
                ])
            ])

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libneon_now}" = no], [
                AC_MSG_NOTICE([libneon (cached): ${nut_have_neon} (${NEON_VERSION})])
                AC_MSG_NOTICE([libneon (cached): cflags_source='${LIBNEON_CFLAGS_SOURCE}' libs_source='${LIBNEON_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libneon (cached): LIBNEON_CFLAGS='${LIBNEON_CFLAGS}'])
                AC_MSG_NOTICE([libneon (cached): LIBNEON_LIBS='${LIBNEON_LIBS}'])
                AC_MSG_NOTICE([libneon (cached): SOFILE:'${SOFILE_LIBNEON}', SOPATH:'${SOPATH_LIBNEON}'])
            ])
        ])
    ])
])
