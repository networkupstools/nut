dnl Check for LIBREGEX (and, if found, fill 'nut_usb_lib' with its
dnl approximate version) and its compiler flags. On success, set
dnl nut_have_libregex="yes" and set LIBREGEX_CFLAGS and LIBREGEX_LIBS. On failure, set
dnl nut_have_libregex="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBREGEX],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libregex_seen}"], [
        nut_have_libregex_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([regex], [auto])
        NUT_ARG_WITH_LIBOPTS_LIBS([regex], [auto])

        nut_noncv_checked_libregex_now=no
        AC_CACHE_VAL([nut_cv_checked_libregex], [
            nut_noncv_checked_libregex_now=yes

            AC_REQUIRE([NUT_CHECK_PKGCONFIG])

            dnl save CFLAGS and LIBS
            CFLAGS_ORIG="${CFLAGS}"
            LIBS_ORIG="${LIBS}"
            REQUIRES_ORIG="${REQUIRES}"
            CFLAGS=""
            LIBS=""
            REQUIRES=""
            depCFLAGS=""
            depCFLAGS_SOURCE=""
            depLIBS=""
            depLIBS_SOURCE=""
            depREQUIRES=""
            depREQUIRES_SOURCE=""

            dnl Actually did not see it in any systems' pkg-config info...
            dnl Part of standard footprint?
            nut_cv_LIBREGEX_MODULE=""
            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                AC_MSG_CHECKING(for libregex version via pkg-config)
                nut_cv_LIBREGEX_VERSION="`$PKG_CONFIG --silence-errors --modversion regex 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_LIBREGEX_VERSION}"], [
                    nut_cv_LIBREGEX_VERSION="`$PKG_CONFIG --silence-errors --modversion libregex 2>/dev/null`"
                    AS_IF([test "$?" != "0" -o -z "${nut_cv_LIBREGEX_VERSION}"], [
                        nut_cv_LIBREGEX_VERSION="none"
                    ], [
                        nut_cv_LIBREGEX_MODULE="libregex"
                    ])
                ], [
                    nut_cv_LIBREGEX_MODULE="regex"
                ])
                AC_MSG_RESULT(${nut_cv_LIBREGEX_VERSION} found)
            ], [
                nut_cv_LIBREGEX_VERSION="none"
                AC_MSG_NOTICE([can not check libregex settings via pkg-config])
            ])

            AS_IF([test x"${nut_cv_LIBREGEX_VERSION}" != xnone && test x"${nut_cv_LIBREGEX_MODULE}" != x], [
                depCFLAGS="`$PKG_CONFIG --silence-errors --cflags \"${nut_cv_LIBREGEX_MODULE}\" 2>/dev/null`"
                depLIBS="`$PKG_CONFIG --silence-errors --libs \"${nut_cv_LIBREGEX_MODULE}\" 2>/dev/null`"
                depREQUIRES="${nut_cv_LIBREGEX_MODULE}"
                depCFLAGS_SOURCE="pkg-config"
                depLIBS_SOURCE="pkg-config"
                depREQUIRES_SOURCE="pkg-config"
            ], [
                depCFLAGS=""
                depLIBS=""
                depREQUIRES=""
                depCFLAGS_SOURCE="default"
                depLIBS_SOURCE="default"
                depREQUIRES_SOURCE="default"
            ])

            dnl allow overriding libregex settings if the user knows best
            AC_MSG_CHECKING(for libregex cflags)
            AS_CASE([${nut_with_regex_includes}],
                [auto], [],	dnl Keep what we had found above
                    [depCFLAGS="${nut_with_regex_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for libregex ldflags)
            AS_CASE([${nut_with_regex_libs}],
                [auto], [],	dnl Keep what we had found above
                    [depLIBS="${nut_with_regex_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl Check if libregex is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            REQUIRES="${REQUIRES_ORIG} ${depREQUIRES}"

            AC_LANG_PUSH([C])
            dnl # With USB we can match desired devices by regex
            dnl # (and currently have no other use for the library);
            dnl # however we may have some general regex helper
            dnl # methods built into libcommon (may become useful
            dnl # elsewhere) - so need to know if we may and should.
            dnl # Maybe already involved in NUT for Windows builds...
            nut_cv_have_regex=no
            AC_CHECK_HEADER([regex.h], [
                AC_CHECK_DECLS([regexec, regcomp], [nut_cv_have_regex=yes], [], [#include <regex.h>])
            ], [
                AC_CHECK_DECLS([regexec, regcomp], [nut_cv_have_regex=yes], [], [])
            ])

            AS_IF([test x"${nut_cv_have_regex}" = xyes], [
                nut_cv_have_regex=no
                AC_SEARCH_LIBS([regexec], [regex], [
                    AC_SEARCH_LIBS([regcomp], [regex], [nut_cv_have_regex=yes])
                ])
            ])

            dnl Collect possibly updated dependencies after AC SEARCH LIBS:
            AS_IF([test x"${LIBS}" != x"${LIBS_ORIG} ${depLIBS}"], [
                AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
                    depLIBS="`echo \"$LIBS\" | sed -e 's|'\"${LIBS_ORIG}\"'| |' -e 's|^ *||' -e 's| *$||'`"
                ])
            ])

            AC_LANG_POP([C])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBREGEX_CFLAGS=""
            nut_cv_LIBREGEX_LIBS=""
            AS_IF([test x"${nut_cv_have_regex}" = xyes], [
                nut_cv_LIBREGEX_CFLAGS="${depCFLAGS}"
                nut_cv_LIBREGEX_LIBS="${depLIBS}"
            ])

            nut_cv_LIBREGEX_REQUIRES="${depREQUIRES}"
            nut_cv_LIBREGEX_REQUIRES_SOURCE="${depREQUIRES_SOURCE}"
            nut_cv_LIBREGEX_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBREGEX_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_have_regex], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_MODULE], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_REQUIRES], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBREGEX_REQUIRES_SOURCE], [])

            unset depCFLAGS
            unset depLIBS
            unset depREQUIRES
            unset depCFLAGS_SOURCE
            unset depLIBS_SOURCE
            unset depREQUIRES_SOURCE

            dnl restore original CFLAGS and LIBS
            CFLAGS="${CFLAGS_ORIG}"
            LIBS="${LIBS_ORIG}"

            unset CFLAGS_ORIG
            unset LIBS_ORIG

            dnl FIXME? We did not restore this value, is this needed by libs checked later?
            dnl REQUIRES="${REQUIRES_ORIG}"
            dnl unset REQUIRES_ORIG

            dnl Complete the cache ritual
            nut_cv_checked_libregex="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libregex}" = xyes], [
            nut_have_regex="${nut_cv_have_regex}"

            dnl Note that on some platforms we can have the feature
            dnl without the header, so not neutering nut_have_regex
            dnl here in case of failure. Also, due to header+define
            dnl we run this part always, not just for cached re-runs.
            CFLAGS_ORIG="${CFLAGS}"
            CFLAGS="${nut_cv_LIBREGEX_CFLAGS}"
            AC_LANG_PUSH([C])
            AC_CHECK_HEADER([regex.h], [
                AC_DEFINE([HAVE_REGEX_H], [1],
                    [Define to 1 if you have <regex.h>.])
                AS_IF([test "${nut_noncv_checked_libregex_now}" = no], [
                    AC_CHECK_DECLS([regexec, regcomp], [], [nut_have_regex=no], [#include <regex.h>])
                ])
            ], [
                AS_IF([test "${nut_noncv_checked_libregex_now}" = no], [
                    AC_CHECK_DECLS([regexec, regcomp], [], [nut_have_regex=no], [])
                ])
            ])
            AC_LANG_POP([C])
            CFLAGS="${CFLAGS_ORIG}"
            unset CFLAGS_ORIG

            AS_IF([test x"${nut_have_regex}" = xyes], [
                AC_DEFINE(HAVE_LIBREGEX, 1,
                    [Define to 1 for build where we can support general regex matching.])
            ], [
                AC_DEFINE(HAVE_LIBREGEX, 0,
                    [Define to 1 for build where we can support general regex matching.])
            ])
            AM_CONDITIONAL(HAVE_LIBREGEX, [test x"${nut_have_regex}" = xyes])

            LIBREGEX_CFLAGS="${nut_cv_LIBREGEX_CFLAGS}"
            LIBREGEX_LIBS="${nut_cv_LIBREGEX_LIBS}"
            LIBREGEX_REQUIRES="${nut_cv_LIBREGEX_REQUIRES}"

            dnl For troubleshooting of re-runs, mostly:
            LIBREGEX_VERSION="${nut_cv_LIBREGEX_VERSION}"
            LIBREGEX_MODULE="${nut_cv_LIBREGEX_MODULE}"
            LIBREGEX_CFLAGS_SOURCE="${nut_cv_LIBREGEX_CFLAGS_SOURCE}"
            LIBREGEX_LIBS_SOURCE="${nut_cv_LIBREGEX_LIBS_SOURCE}"
            LIBREGEX_REQUIRES_SOURCE="${nut_cv_LIBREGEX_REQUIRES_SOURCE}"

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libregex_now}" = no], [
                AC_MSG_NOTICE([libregex (cached): ${nut_have_regex}])
                AC_MSG_NOTICE([libregex (cached): version=${LIBREGEX_VERSION} cflags_source=${LIBREGEX_CFLAGS_SOURCE} libs_source=${LIBREGEX_LIBS_SOURCE}])
                AC_MSG_NOTICE([libregex (cached): LIBREGEX_CFLAGS=${LIBREGEX_CFLAGS}])
                AC_MSG_NOTICE([libregex (cached): LIBREGEX_LIBS=${LIBREGEX_LIBS}])
                AC_MSG_NOTICE([libregex (cached): module=${LIBREGEX_MODULE} requires_source=${LIBREGEX_REQUIRES_SOURCE} LIBREGEX_REQUIRES=${LIBREGEX_REQUIRES}])
            ])
        ])
    ])
])
