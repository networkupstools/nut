dnl By default, AC_RUN_IFELSE() fails if it detects cross-compilation
dnl but it provides the fourth argument to customize the handling.
dnl In our case, we would fall back to trying just to link then -
dnl the result would not be as relevant regarding run-time behavior
dnl of the code, but at least we would know that API and ABI are ok.

dnl Per original /usr/share/autoconf/autoconf/general.m4 which makes
dnl a similar wrapper:
dnl # AC_TRY_RUN(PROGRAM,
dnl #            [ACTION-IF-TRUE], [ACTION-IF-FALSE],
dnl #            [ACTION-IF-CROSS-COMPILING = RUNTIME-ERROR])
dnl # -------------------------------------------------------
AC_DEFUN([AX_RUN_OR_LINK_IFELSE],
[
	AC_RUN_IFELSE([$1], [$2], [$3],
		[
		AC_MSG_WARN([Current build is a cross-build, so not running test binaries, just linking them])
		AC_LINK_IFELSE([$1], [$2], [$3])
		]
	)
])
