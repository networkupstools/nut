dnl Check for LIBLTDL compiler flags. On success, set nut_have_libltdl="yes"
dnl and set LIBLTDL_CFLAGS and LIBLTDL_LIBS. On failure, set
dnl nut_have_libltdl="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBLTDL],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libltdl_seen}"], [
        nut_have_libltdl_seen=yes
        dnl No NUT_CHECK_PKGCONFIG here: (lib)ltdl.pc was not seen on any OS

        nut_noncv_checked_libltdl_now=no
        AC_CACHE_VAL([nut_cv_checked_libltdl], [
            nut_noncv_checked_libltdl_now=yes

            dnl save CFLAGS and LIBS
            CFLAGS_ORIG="${CFLAGS}"
            LIBS_ORIG="${LIBS}"
            CFLAGS=""
            LIBS=""
            depCFLAGS=""
            depCFLAGS_SOURCE=""
            depLIBS=""
            depLIBS_SOURCE=""
            dnl For fallback below:
            myCFLAGS=""

            AC_MSG_CHECKING(for libltdl cflags)
            NUT_ARG_WITH_LIBOPTS_INCLUDES([libltdl], [auto])
            AS_CASE([${nut_with_libltdl_includes}],
                [auto], [
                     dnl Best-Effort Fallback (LDFLAGS might make more sense for -L...,
                     dnl but other m4 files have it so) to use if probe below fails:
                     myCFLAGS="-I/usr/local/include -I/usr/include -L/usr/local/lib -L/usr/lib"
                     depCFLAGS_SOURCE="default"
                    ],
                    [depCFLAGS="${nut_with_libltdl_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for libltdl ldflags)
            NUT_ARG_WITH_LIBOPTS_LIBS([libltdl], [auto])
            AS_CASE([${nut_with_libltdl_libs}],
                [auto], [depLIBS_SOURCE="default (probe later)"],	dnl No fallback here - we probe suitable libs below
                    [depLIBS="${nut_with_libltdl_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(ltdl.h, [nut_have_libltdl=yes], [
                dnl Double-check if we stashed include paths to try above
                AS_IF([test -n "$myCFLAGS"], [
                    depCFLAGS="$myCFLAGS"
                    AS_UNSET([ac_cv_header_ltdl_h])
                    CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
                    AC_CHECK_HEADERS(ltdl.h, [nut_have_libltdl=yes], [nut_have_libltdl=no], [AC_INCLUDES_DEFAULT])
                    ],[nut_have_libltdl=no]
                )], [AC_INCLUDES_DEFAULT])
            AS_IF([test x"$nut_have_libltdl" = xyes], [
                dnl ltdl-number may help find it for MingW DLLs naming
                AC_SEARCH_LIBS(lt_dlinit, ltdl ltdl-7, [], [
                    nut_have_libltdl=no
                    AS_IF([test -n "$myCFLAGS" -a x"$myCFLAGS" != x"$CFLAGS"], [
                        depCFLAGS="$myCFLAGS"
                        dnl No ltdl-7 here, this codepath is unlikely on Windows where that matters:
                        CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
                        unset ac_cv_search_lt_dlinit
                        AC_SEARCH_LIBS(lt_dlinit, ltdl, [nut_have_libltdl=yes], [])
                    ])
                ])
            ])

            dnl Collect possibly updated dependencies after AC SEARCH LIBS:
            AS_IF([test x"${LIBS}" != x"${LIBS_ORIG} ${depLIBS}"], [
                AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
                    depLIBS="`echo \"$LIBS\" | sed -e 's|'\"${LIBS_ORIG}\"'| |' -e 's|^ *||' -e 's| *$||'`"
                ])
            ])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_have_libltdl="${nut_have_libltdl}"
            AS_IF([test "${nut_have_libltdl}" = "yes"], [
                nut_cv_LIBLTDL_CFLAGS="${depCFLAGS}"
                nut_cv_LIBLTDL_LIBS="${depLIBS}"
            ], [
                nut_cv_LIBLTDL_CFLAGS=""
                nut_cv_LIBLTDL_LIBS=""
            ])
            nut_cv_LIBLTDL_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBLTDL_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_have_libltdl], [])
            AC_CACHE_VAL([nut_cv_LIBLTDL_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBLTDL_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBLTDL_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBLTDL_LIBS_SOURCE], [])

            unset myCFLAGS
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
            nut_cv_checked_libltdl="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libltdl}" = xyes], [
            nut_have_libltdl="${nut_cv_have_libltdl}"

            AS_IF([test "${nut_noncv_checked_libltdl_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                CFLAGS="${nut_cv_LIBLTDL_CFLAGS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(ltdl.h, [], [nut_have_libltdl=no])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                unset CFLAGS_ORIG
            ])

            AS_IF([test "${nut_have_libltdl}" = "yes"], [
                AC_DEFINE(HAVE_LIBLTDL, 1, [Define to enable libltdl support])
            ])

            LIBLTDL_CFLAGS="${nut_cv_LIBLTDL_CFLAGS}"
            LIBLTDL_LIBS="${nut_cv_LIBLTDL_LIBS}"

            dnl For troubleshooting of re-runs, mostly:
            LIBLTDL_CFLAGS_SOURCE="${nut_cv_LIBLTDL_CFLAGS_SOURCE}"
            LIBLTDL_LIBS_SOURCE="${nut_cv_LIBLTDL_LIBS_SOURCE}"

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libltdl_now}" = no], [
                AC_MSG_NOTICE([libltdl (cached): ${nut_have_libltdl}])
                AC_MSG_NOTICE([libltdl (cached): cflags_source='${LIBLTDL_CFLAGS_SOURCE}' libs_source='${LIBLTDL_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libltdl (cached): LIBLTDL_CFLAGS='${LIBLTDL_CFLAGS}'])
                AC_MSG_NOTICE([libltdl (cached): LIBLTDL_LIBS='${LIBLTDL_LIBS}'])
            ])
        ])
    ])
])
