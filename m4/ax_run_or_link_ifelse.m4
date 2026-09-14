dnl By default, AC_RUN_IFELSE() fails if it detects cross-compilation
dnl but it provides the fourth argument to customize the handling.
dnl In our case, we would fall back to trying just to link then -
dnl the result would not be as relevant regarding run-time behavior
dnl of the code, but at least we would know that API and ABI are ok.
dnl Here the fourth and fifth arguments can be used to pass custom
dnl CFLAGS and CXXFLAGS options (e.g. warnings to cover or ignore),
dnl respectively. For the test to succeed, the build/link must pass
dnl without warnings.

dnl Per original /usr/share/autoconf/autoconf/general.m4 which makes
dnl a similar wrapper:
dnl # AC_TRY_RUN(PROGRAM,
dnl #            [ACTION-IF-TRUE], [ACTION-IF-FALSE],
dnl #            [ACTION-IF-CROSS-COMPILING = RUNTIME-ERROR])
dnl # -------------------------------------------------------
AC_DEFUN([AX_RUN_OR_LINK_IFELSE],
[
	myCFLAGS="$CFLAGS"
	myCXXFLAGS="$CXXFLAGS"
	AS_IF([test "${GCC}" = "yes" || test "${CLANGCC}" = "yes"], [
		CFLAGS="$myCFLAGS -Werror -Werror=implicit-function-declaration $4"
		dnl # cc1plus: error: '-Werror=' argument '-Werror=implicit-function-declaration' is not valid for C++ [-Werror]
		CXXFLAGS="$myCXXFLAGS -Werror $5"
	], [
		dnl # Don't know what to complain about for unknown compilers
		dnl # FIXME: Presume here they have at least a "-Werror" option
		CFLAGS="$myCFLAGS -Werror $4"
		CXXFLAGS="$myCXXFLAGS -Werror $5"
	])
	AC_RUN_IFELSE([$1], [$2], [$3],
		[
		AC_MSG_WARN([Current build is a cross-build, so not running test binaries, just linking them])
		AC_LINK_IFELSE([$1], [$2], [$3])
		]
	)
	CFLAGS="$myCFLAGS"
	CXXFLAGS="$myCXXFLAGS"
])
