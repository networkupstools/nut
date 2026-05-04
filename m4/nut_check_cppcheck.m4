dnl Check for "cppcheck" analysis toolkit with suitable version; if found
dnl set nut_have_cppcheck="yes" and provide the path in CPPCHECK variable,
dnl otherwise nut_have_cppcheck="no". Also populates HAVE_CPPCHECK automake
dnl variable (whether the check succeeded or failed).
dnl This macro can be run multiple times, but will do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_CPPCHECK],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_cppcheck_seen}"], [
        dnl NOTE: Did not really investigate suitable CPPCHECK_MIN_VERSION
        dnl values, just using current available as the baseline; maybe
        dnl older releases are also okay (if someone proves so).
        dnl The oldest available on the NUT CI build farm was 1.6 that worked.
        nut_have_cppcheck_seen=yes
        CPPCHECK_MIN_VERSION="1.0"

        AC_PATH_PROGS([CPPCHECK], [cppcheck])
        AC_CACHE_VAL([nut_cv_have_cppcheck], [
            AS_IF([test -n "${CPPCHECK}"], [
                AC_MSG_CHECKING([for cppcheck version])
                CPPCHECK_VERSION="`${CPPCHECK} --version 2>/dev/null`"
                dnl strip 'cppcheck ' from version string
                CPPCHECK_VERSION="${CPPCHECK_VERSION##* }"
                AC_MSG_RESULT(${CPPCHECK_VERSION} found)
            ])

            AC_MSG_CHECKING([if cppcheck version is okay (minimum required ${CPPCHECK_MIN_VERSION})])
            AX_COMPARE_VERSION([${CPPCHECK_VERSION}], [ge], [${CPPCHECK_MIN_VERSION}], [
                AC_MSG_RESULT(yes)
                nut_cv_have_cppcheck="yes"
            ], [
                AC_MSG_RESULT(no)
                nut_cv_have_cppcheck="no"
            ])
        ])

        dnl Notes: we also keep HAVE_CPPCHECK for implicit targets
        nut_have_cppcheck="${nut_cv_have_cppcheck}"
        AM_CONDITIONAL([HAVE_CPPCHECK], [test "${nut_have_cppcheck}" = "yes"])
    ])
])
