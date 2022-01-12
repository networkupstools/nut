dnl Check for "cppcheck" analysis tools

AC_DEFUN([NUT_CHECK_CPPCHECK],
[
if test -z "${nut_have_cppcheck_seen}"; then
	dnl NOTE: Did not really investigate suitable CPPCHECK_MIN_VERSION
	dnl values, just using current available as the baseline; maybe
	dnl older releases are also okay (if someone proves so).
	dnl The oldest available on the NUT CI build farm was 1.6 that worked.
	nut_have_cppcheck_seen=yes
	CPPCHECK_MIN_VERSION="1.0"

	AC_PATH_PROGS([CPPCHECK], [cppcheck])
	if test -n "${CPPCHECK}"; then
		AC_MSG_CHECKING([for cppcheck version])
		CPPCHECK_VERSION="`${CPPCHECK} --version 2>/dev/null`"
		dnl strip 'cppcheck ' from version string
		CPPCHECK_VERSION="${CPPCHECK_VERSION##* }"
		AC_MSG_RESULT(${CPPCHECK_VERSION} found)
	fi

	AC_MSG_CHECKING([if cppcheck version is okay (minimum required ${CPPCHECK_MIN_VERSION})])
	AX_COMPARE_VERSION([${CPPCHECK_VERSION}], [ge], [${CPPCHECK_MIN_VERSION}], [
		AC_MSG_RESULT(yes)
		nut_have_cppcheck="yes"
	], [
		AC_MSG_RESULT(no)
		nut_have_cppcheck="no"
	])

	dnl Notes: we also keep HAVE_CPPCHECK for implicit targets
	AM_CONDITIONAL([HAVE_CPPCHECK], [test "${nut_have_cppcheck}" = "yes"])
fi
])
