dnl detect if current compiler is clang or gcc (or...)

AC_DEFUN([NUT_COMPILER_FAMILY],
[
  AC_CACHE_CHECK([if CC compiler family is GCC],
    [nut_cv_GCC],
    [AS_IF([test -n "$CC"],
        [AS_IF([$CC --version 2>&1 | grep 'Free Software Foundation' > /dev/null],
            [nut_cv_GCC=yes],[nut_cv_GCC=no])],
        [AC_MSG_ERROR([CC is not set])]
    )])

  AC_CACHE_CHECK([if CXX compiler family is GCC],
    [nut_cv_GXX],
    [AS_IF([test -n "$CXX"],
        [AS_IF([$CXX --version 2>&1 | grep 'Free Software Foundation' > /dev/null],
            [nut_cv_GXX=yes],[nut_cv_GXX=no])],
        [AC_MSG_ERROR([CXX is not set])]
    )])

  AC_CACHE_CHECK([if CPP preprocessor family is GCC],
    [nut_cv_GPP],
    [AS_IF([test -n "$CPP"],
        [AS_IF([$CPP --version 2>&1 | grep 'Free Software Foundation' > /dev/null],
            [nut_cv_GPP=yes],[nut_cv_GPP=no])],
        [AC_MSG_ERROR([CPP is not set])]
    )])

  AS_IF([test "x$GCC" = "x" && test "$nut_cv_GCC" = yes],   [GCC=yes])
  AS_IF([test "x$GXX" = "x" && test "$nut_cv_GXX" = yes],   [GXX=yes])
  AS_IF([test "x$GPP" = "x" && test "$nut_cv_GPP" = yes],   [GPP=yes])

  AC_CACHE_CHECK([if CC compiler family is clang],
    [nut_cv_CLANGCC],
    [AS_IF([test -n "$CC"],
        [AS_IF([$CC --version 2>&1 | grep -E '(clang version|Apple LLVM version .*clang-)' > /dev/null],
            [nut_cv_CLANGCC=yes],[nut_cv_CLANGCC=no])],
        [AC_MSG_ERROR([CC is not set])]
    )])

  AC_CACHE_CHECK([if CXX compiler family is clang],
    [nut_cv_CLANGXX],
    [AS_IF([test -n "$CXX"],
        [AS_IF([$CXX --version 2>&1 | grep -E '(clang version|Apple LLVM version .*clang-)' > /dev/null],
            [nut_cv_CLANGXX=yes],[nut_cv_CLANGXX=no])],
        [AC_MSG_ERROR([CXX is not set])]
    )])

  AC_CACHE_CHECK([if CPP preprocessor family is clang],
    [nut_cv_CLANGPP],
    [AS_IF([test -n "$CPP"],
        [AS_IF([$CPP --version 2>&1 | grep -E '(clang version|Apple LLVM version .*clang-)' > /dev/null],
            [nut_cv_CLANGPP=yes],[nut_cv_CLANGPP=no])],
        [AC_MSG_ERROR([CPP is not set])]
    )])

  AS_IF([test "x$CLANGCC" = "x" && test "$nut_cv_CLANGCC" = yes],   [CLANGCC=yes])
  AS_IF([test "x$CLANGXX" = "x" && test "$nut_cv_CLANGXX" = yes],   [CLANGXX=yes])
  AS_IF([test "x$CLANGPP" = "x" && test "$nut_cv_CLANGPP" = yes],   [CLANGPP=yes])
])

AC_DEFUN([NUT_CHECK_COMPILE_FLAG],
[
dnl Note: per https://stackoverflow.com/questions/52557417/how-to-check-support-compile-flag-in-autoconf-for-clang
dnl the -Werror below is needed to detect "warnings" about unsupported options
    COMPILERFLAG="$1"
    AC_LANG_PUSH([C])
    AX_CHECK_COMPILE_FLAG([${COMPILERFLAG}],
        [CFLAGS="$CFLAGS ${COMPILERFLAG}"], [], [-Werror])
    AC_LANG_POP([C])

    AC_LANG_PUSH([C++])
    AX_CHECK_COMPILE_FLAG([${COMPILERFLAG}],
        [CXXFLAGS="$CXXFLAGS ${COMPILERFLAG}"], [], [-Werror])
    AC_LANG_POP([C++])
])

AC_DEFUN([NUT_COMPILER_FAMILY_FLAGS],
[
    AC_MSG_NOTICE([Detecting support for additional compiler flags])

dnl -Qunused-arguments:
dnl   Do not die due to `clang: error: argument unused during compilation: '-I .'`
dnl -Wno-unknown-warning-option: Do not die (on older clang releases) due to
dnl   error: unknown warning option '-Wno-double-promotion'; did you mean
dnl          '-Wno-documentation'? [-Werror,-Wunknown-warning-option]
dnl -fcolor-diagnostics: help find where bugs are in the wall of text

    m4_foreach_w([TESTCOMPILERFLAG], [
        -Qunused-arguments
        -Wno-unknown-warning-option
        -fcolor-diagnostics
    ], [
        NUT_CHECK_COMPILE_FLAG([TESTCOMPILERFLAG])
    ])

dnl # Older "brute-forced" settings:
dnl    AS_IF([test "x$CLANGCC" = xyes], [CFLAGS="$CFLAGS -Wno-unknown-warning-option"])
dnl    AS_IF([test "x$CLANGXX" = xyes], [CXXFLAGS="$CXXFLAGS -Wno-unknown-warning-option"])

dnl # Despite the internet lore, practical GCC versions seen so far
dnl # (4.x-10.x) do not know of this CLI option, with varied results
dnl # from "cc1: note: unrecognized command-line option '-Wno-unknown-warning'
dnl # may have been intended to silence earlier diagnostics"
dnl # to "cc1: error: unrecognized command line option '-Wno-unknown-warning'
dnl # [-Werror]"... so we do not pass it by default:
dnl    AS_IF([test "x$GCC" = xyes], [CFLAGS="$CFLAGS -Wno-unknown-warning"])
dnl    AS_IF([test "x$GXX" = xyes], [CXXFLAGS="$CXXFLAGS -Wno-unknown-warning"])

dnl # There should be no need to include standard system paths (and possibly
dnl # confuse the compiler assumptions - along with its provided headers):
dnl #    AS_IF([test "x$CLANGCC" = xyes -o  "x$GCC" = xyes],
dnl #        [CFLAGS="-isystem /usr/include -isystem /usr/local/include $CFLAGS"])
dnl #    AS_IF([test "x$CLANGXX" = xyes -o  "x$GXX" = xyes],
dnl #        [CXXFLAGS="-isystem /usr/include -isystem /usr/local/include $CXXFLAGS"])
])
