dnl Check for LIBPOWERMAN compiler flags. On success, set nut_have_libpowerman="yes"
dnl and set LIBPOWERMAN_CFLAGS and LIBPOWERMAN_LIBS. On failure, set
dnl nut_have_libpowerman="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBPOWERMAN],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libpowerman_seen}"], [
        nut_have_libpowerman_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([powerman], [auto], [libpowerman])
        NUT_ARG_WITH_LIBOPTS_LIBS([powerman], [auto], [libpowerman])

        nut_noncv_checked_libpowerman_now=no
        AC_CACHE_VAL([nut_cv_checked_libpowerman], [
            nut_noncv_checked_libpowerman_now=yes

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
                AC_MSG_CHECKING([for LLNC libpowerman version via pkg-config])
                nut_cv_POWERMAN_VERSION="`$PKG_CONFIG --silence-errors --modversion libpowerman 2>/dev/null`"
                dnl Unlike other pkg-config enabled projects we use,
                dnl libpowerman (at least on Debian) delivers an empty
                dnl "Version:" tag in /usr/lib/pkgconfig/libpowerman.pc
                dnl (and it is the only file in that dir, others going
                dnl to /usr/lib/x86_64-linux-gnu/pkgconfig/ or similar
                dnl for other architectures).
                dnl NOTE: Empty value is not an error here!
                AS_IF([test "$?" != "0"], [
                    nut_cv_POWERMAN_VERSION="none"
                ])
                AC_MSG_RESULT(['${nut_cv_POWERMAN_VERSION}' found])
            ], [
                nut_cv_POWERMAN_VERSION="none"
                AC_MSG_NOTICE([can not check LLNC libpowerman settings via pkg-config])
            ])

            AS_IF([test x"${nut_cv_POWERMAN_VERSION}" != xnone], [
                depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libpowerman 2>/dev/null`"
                depLIBS="`$PKG_CONFIG --silence-errors --libs libpowerman 2>/dev/null`"
                depCFLAGS_SOURCE="pkg-config"
                depLIBS_SOURCE="pkg-config"
            ], [
                depCFLAGS=""
                depLIBS=""
                depCFLAGS_SOURCE="default"
                depLIBS_SOURCE="default"
            ])

            AC_MSG_CHECKING([for libpowerman cflags])
            AS_CASE([${nut_with_powerman_includes}],
                [auto], [],	dnl Keep what we had found above
                    [depCFLAGS="${nut_with_powerman_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for libpowerman libs)
            AS_CASE([${nut_with_powerman_libs}],
                [auto], [],	dnl Keep what we had found above
                    [depLIBS="${nut_with_powerman_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if libpowerman is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(libpowerman.h, [nut_cv_have_libpowerman=yes], [nut_cv_have_libpowerman=no], [AC_INCLUDES_DEFAULT])
            AC_CHECK_FUNCS(pm_connect, [], [
                dnl Some systems may just have libpowerman in their
                dnl standard paths, but not the pkg-config data
                AS_IF([test "${nut_cv_have_libpowerman}" = "yes" && test "${nut_cv_POWERMAN_VERSION}" = "none" && test -z "$LIBS"],
                    [AC_MSG_CHECKING([if libpowerman is just present in path])
                     depLIBS="-L/usr/lib -L/usr/local/lib -lpowerman"
                     unset ac_cv_func_pm_connect || true
                     LIBS="${LIBS_ORIG} ${depLIBS}"
                     AC_CHECK_FUNCS(pm_connect, [], [nut_cv_have_libpowerman=no])
                     AC_MSG_RESULT([${nut_cv_have_libpowerman}])
                    ], [nut_cv_have_libpowerman=no]
                )]
            )

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBPOWERMAN_CFLAGS=""
            nut_cv_LIBPOWERMAN_LIBS=""
            AS_IF([test "${nut_cv_have_libpowerman}" = "yes"], [
                nut_cv_LIBPOWERMAN_CFLAGS="${depCFLAGS}"
                nut_cv_LIBPOWERMAN_LIBS="${depLIBS}"
            ])
            nut_cv_LIBPOWERMAN_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBPOWERMAN_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_have_libpowerman], [])
            AC_CACHE_VAL([nut_cv_POWERMAN_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBPOWERMAN_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBPOWERMAN_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBPOWERMAN_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBPOWERMAN_LIBS_SOURCE], [])

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
            nut_cv_checked_libpowerman="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libpowerman}" = xyes], [
            nut_have_libpowerman="${nut_cv_have_libpowerman}"

            AS_IF([test "${nut_noncv_checked_libpowerman_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBPOWERMAN_CFLAGS}"
                LIBS="${nut_cv_LIBPOWERMAN_LIBS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(libpowerman.h, [], [nut_have_libpowerman=no], [AC_INCLUDES_DEFAULT])
                AC_CHECK_FUNCS(pm_connect, [], [nut_have_libpowerman=no])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                LIBS="${LIBS_ORIG}"
                unset CFLAGS_ORIG
                unset LIBS_ORIG
            ])

            AS_IF([test "${nut_have_libpowerman}" = "yes"], [
                AC_DEFINE(HAVE_LIBPOWERMAN, 1, [Define to enable libpowerman support])
            ])

            LIBPOWERMAN_CFLAGS="${nut_cv_LIBPOWERMAN_CFLAGS}"
            LIBPOWERMAN_LIBS="${nut_cv_LIBPOWERMAN_LIBS}"

            dnl For troubleshooting of re-runs, mostly:
            LIBPOWERMAN_CFLAGS_SOURCE="${nut_cv_LIBPOWERMAN_CFLAGS_SOURCE}"
            LIBPOWERMAN_LIBS_SOURCE="${nut_cv_LIBPOWERMAN_LIBS_SOURCE}"
            POWERMAN_VERSION="${nut_cv_POWERMAN_VERSION}"

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libpowerman_now}" = no], [
                AC_MSG_NOTICE([libpowerman (cached): ${nut_have_libpowerman} (${POWERMAN_VERSION})])
                AC_MSG_NOTICE([libpowerman (cached): cflags_source='${LIBPOWERMAN_CFLAGS_SOURCE}' libs_source='${LIBPOWERMAN_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libpowerman (cached): LIBPOWERMAN_CFLAGS='${LIBPOWERMAN_CFLAGS}'])
                AC_MSG_NOTICE([libpowerman (cached): LIBPOWERMAN_LIBS='${LIBPOWERMAN_LIBS}'])
            ])
        ])
    ])
])
