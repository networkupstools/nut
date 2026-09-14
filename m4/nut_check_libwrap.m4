dnl Check for LIBWRAP compiler flags. On success, set nut_have_libwrap="yes"
dnl and set LIBWRAP_CFLAGS and LIBWRAP_LIBS. On failure, set
dnl nut_have_libwrap="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBWRAP],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libwrap_seen}"], [
        nut_have_libwrap_seen=yes

        nut_noncv_checked_libwrap_now=no
        AC_CACHE_VAL([nut_cv_checked_libwrap], [
            nut_noncv_checked_libwrap_now=yes

            dnl save LIBS, no known CFLAGS here
            LIBS_ORIG="${LIBS}"
            dnl Do not neuter LIBS here, we check it below right away
            depLIBS=""

            AC_CHECK_HEADERS(tcpd.h, [nut_cv_have_libwrap=yes], [nut_cv_have_libwrap=no], [AC_INCLUDES_DEFAULT])
            AC_SEARCH_LIBS(yp_get_default_domain, nsl, [], [nut_cv_have_libwrap=no])

            dnl The line below does not work on Solaris 10.
            dnl AC_SEARCH_LIBS(request_init, wrap, [], [nut_cv_have_libwrap=no])

            dnl Collect possibly updated dependencies after AC SEARCH LIBS:
            AS_IF([test x"${LIBS}" != x"${LIBS_ORIG}"], [
                AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
                    depLIBS="`echo \"$LIBS\" | sed -e 's|'\"${LIBS_ORIG}\"'| |' -e 's|^ *||' -e 's| *$||'`"
                ])
            ])

            AC_MSG_CHECKING(for library containing request_init)
            AC_LANG_PUSH([C])
            AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <tcpd.h>
int allow_severity = 0, deny_severity = 0;
            ]], [[ request_init(0); ]])], [
                AC_MSG_RESULT(none required)
            ], [
                depLIBS="${depLIBS} -lwrap"
                LIBS="${LIBS_ORIG} ${depLIBS}"
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <tcpd.h>
int allow_severity = 0, deny_severity = 0;
                ]], [[ request_init(0); ]])], [
                    AC_MSG_RESULT(-lwrap)
                ], [
                    AC_MSG_RESULT(no)
                    nut_cv_have_libwrap=no
                ])
            ])
            AC_LANG_POP([C])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBWRAP_CFLAGS=""
            nut_cv_LIBWRAP_LIBS=""
            AS_IF([test "${nut_cv_have_libwrap}" = "yes"], [
                nut_cv_LIBWRAP_LIBS="${depLIBS}"
            ])

            AC_CACHE_VAL([nut_cv_have_libwrap], [])
            AC_CACHE_VAL([nut_cv_LIBWRAP_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBWRAP_LIBS], [])

            unset depLIBS

            dnl restore original LIBS
            LIBS="${LIBS_ORIG}"

            unset LIBS_ORIG

            dnl Complete the cache ritual
            nut_cv_checked_libwrap="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libwrap}" = xyes], [
            nut_have_libwrap="${nut_cv_have_libwrap}"

            AS_IF([test "${nut_noncv_checked_libwrap_now}" = no], [
                dnl Should restore the cached value and be done with it
                dnl (CFLAGS assumed empty, LIBS irrelevant for header check)
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(tcpd.h, [], [nut_have_libwrap=no], [AC_INCLUDES_DEFAULT])
                AC_LANG_POP([C])
            ])

            AS_IF([test "${nut_have_libwrap}" = "yes"], [
                AC_DEFINE(HAVE_WRAP, 1, [Define to enable libwrap support])
            ])

            LIBWRAP_CFLAGS="${nut_cv_LIBWRAP_CFLAGS}"
            LIBWRAP_LIBS="${nut_cv_LIBWRAP_LIBS}"

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libwrap_now}" = no], [
                AC_MSG_NOTICE([libwrap (cached): ${nut_have_libwrap}])
                AC_MSG_NOTICE([libwrap (cached): LIBWRAP_CFLAGS='${LIBWRAP_CFLAGS}'])
                AC_MSG_NOTICE([libwrap (cached): LIBWRAP_LIBS='${LIBWRAP_LIBS}'])
            ])
        ])
    ])
])
