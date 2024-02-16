dnl detect if current compiler is clang or gcc (or...)

AC_DEFUN([NUT_COMPILER_FAMILY],
[
  CC_VERSION_FULL="`LANG=C LC_ALL=C $CC --version 2>&1`"   || CC_VERSION_FULL=""
  CXX_VERSION_FULL="`LANG=C LC_ALL=C $CXX --version 2>&1`" || CXX_VERSION_FULL=""
  CPP_VERSION_FULL="`LANG=C LC_ALL=C $CPP --version 2>&1`" || CPP_VERSION_FULL=""
  CC_VERSION=""
  CXX_VERSION=""
  CPP_VERSION=""

  AC_CACHE_CHECK([if CC compiler family is GCC],
    [nut_cv_GCC],
    [AS_IF([test -n "$CC" && test -n "$CC_VERSION_FULL"],
        [AS_IF([echo "${CC_VERSION_FULL}" | grep 'Free Software Foundation' > /dev/null],
            [nut_cv_GCC=yes],[nut_cv_GCC=no])],
        [AC_MSG_ERROR([CC is not set])]
    )])

  AC_CACHE_CHECK([if CXX compiler family is GCC],
    [nut_cv_GXX],
    [AS_IF([test -n "$CXX" && test -n "$CXX_VERSION_FULL"],
        [AS_IF([echo "${CXX_VERSION_FULL}" | grep 'Free Software Foundation' > /dev/null],
            [nut_cv_GXX=yes],[nut_cv_GXX=no])],
        [AC_MSG_ERROR([CXX is not set])]
    )])

  AC_CACHE_CHECK([if CPP preprocessor family is GCC],
    [nut_cv_GPP],
    [AS_IF([test -n "$CPP" && test -n "$CPP_VERSION_FULL"],
        [AS_IF([echo "${CPP_VERSION_FULL}" | grep 'Free Software Foundation' > /dev/null],
            [nut_cv_GPP=yes],[nut_cv_GPP=no])],
        [AC_MSG_ERROR([CPP is not set])]
    )])

  AS_IF([test "x$GCC" = "x" && test "$nut_cv_GCC" = yes],   [GCC=yes
    CC_VERSION="`echo "${CC_VERSION_FULL}" | grep -i gcc | head -1`" \
    && test -n "${CC_VERSION}" || CC_VERSION=""
    ])
  AS_IF([test "x$GXX" = "x" && test "$nut_cv_GXX" = yes],   [GXX=yes
    CXX_VERSION="`echo "${CXX_VERSION_FULL}" | grep -i -E 'g++|gcc' | head -1`" \
    && test -n "${CXX_VERSION}" || CXX_VERSION=""
    ])
  AS_IF([test "x$GPP" = "x" && test "$nut_cv_GPP" = yes],   [GPP=yes
    CPP_VERSION="`echo "${CPP_VERSION_FULL}" | grep -i -E 'cpp|gcc' | head -1`" \
    && test -n "${CPP_VERSION}" || CPP_VERSION=""
    ])

  AC_CACHE_CHECK([if CC compiler family is clang],
    [nut_cv_CLANGCC],
    [AS_IF([test -n "$CC" && test -n "$CC_VERSION_FULL"],
        [AS_IF([echo "${CC_VERSION_FULL}" | grep -E '(clang version|Apple LLVM version .*clang-)' > /dev/null],
            [nut_cv_CLANGCC=yes],[nut_cv_CLANGCC=no])],
        [AC_MSG_ERROR([CC is not set])]
    )])

  AC_CACHE_CHECK([if CXX compiler family is clang],
    [nut_cv_CLANGXX],
    [AS_IF([test -n "$CXX" && test -n "$CXX_VERSION_FULL"],
        [AS_IF([echo "${CXX_VERSION_FULL}" | grep -E '(clang version|Apple LLVM version .*clang-)' > /dev/null],
            [nut_cv_CLANGXX=yes],[nut_cv_CLANGXX=no])],
        [AC_MSG_ERROR([CXX is not set])]
    )])

  AC_CACHE_CHECK([if CPP preprocessor family is clang],
    [nut_cv_CLANGPP],
    [AS_IF([test -n "$CPP" && test -n "$CPP_VERSION_FULL"],
        [AS_IF([echo "${CPP_VERSION_FULL}" | grep -E '(clang version|Apple LLVM version .*clang-)' > /dev/null],
            [nut_cv_CLANGPP=yes],[nut_cv_CLANGPP=no])],
        [AC_MSG_ERROR([CPP is not set])]
    )])

  AS_IF([test "x$CLANGCC" = "x" && test "$nut_cv_CLANGCC" = yes],   [CLANGCC=yes
    CC_VERSION="`echo "${CC_VERSION_FULL}" | grep -v "Dir:" | tr '\n' ';' | sed -e 's, *;,;,g' -e 's,;$,,' -e 's,;,; ,g'`" \
    && test -n "${CC_VERSION}" || CC_VERSION=""
    ])
  AS_IF([test "x$CLANGXX" = "x" && test "$nut_cv_CLANGXX" = yes],   [CLANGXX=yes
    CXX_VERSION="`echo "${CXX_VERSION_FULL}" | grep -v "Dir:" | tr '\n' ';' | sed -e 's, *;,;,g' -e 's,;$,,' -e 's,;,; ,g'`" \
    && test -n "${CXX_VERSION}" || CXX_VERSION=""
    ])
  AS_IF([test "x$CLANGPP" = "x" && test "$nut_cv_CLANGPP" = yes],   [CLANGPP=yes
    CPP_VERSION="`echo "${CPP_VERSION_FULL}" | grep -v "Dir:" | tr '\n' ';' | sed -e 's, *;,;,g' -e 's,;$,,' -e 's,;,; ,g'`" \
    && test -n "${CPP_VERSION}" || CPP_VERSION=""
    ])

  AS_IF([test "x$CC_VERSION" = x],  [CC_VERSION="`echo "${CC_VERSION_FULL}" | head -1`"])
  AS_IF([test "x$CXX_VERSION" = x], [CXX_VERSION="`echo "${CXX_VERSION_FULL}" | head -1`"])
  AS_IF([test "x$CPP_VERSION" = x], [CPP_VERSION="`echo "${CPP_VERSION_FULL}" | head -1`"])
])

AC_DEFUN([NUT_CHECK_COMPILE_FLAG],
[
dnl Note: with this line uncommented, builds report
dnl   sed: 0: conftest.c: No such file or directory
dnl so seemingly try to parse the method without args:
    dnl### AC_REQUIRE([AX_RUN_OR_LINK_IFELSE])

dnl Note: per https://stackoverflow.com/questions/52557417/how-to-check-support-compile-flag-in-autoconf-for-clang
dnl the -Werror below is needed to detect "warnings" about unsupported options
    COMPILERFLAG="$1"

dnl We also try to run an actual build since tools called from that might
dnl complain if they are forwarded unknown flags accepted by the front-end.
    SAVED_CFLAGS="$CFLAGS"
    SAVED_CXXFLAGS="$CXXFLAGS"

    AC_LANG_PUSH([C])
    AX_CHECK_COMPILE_FLAG([${COMPILERFLAG}],
        [CFLAGS="$CFLAGS ${COMPILERFLAG}"
         AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
            [], [CFLAGS="$SAVED_CFLAGS"])
        ], [], [-Werror])
    AC_LANG_POP([C])

    AC_LANG_PUSH([C++])
    AX_CHECK_COMPILE_FLAG([${COMPILERFLAG}],
        [CXXFLAGS="$CXXFLAGS ${COMPILERFLAG}"
         AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
            [], [CXXFLAGS="$SAVED_CXXFLAGS"])
        ], [], [-Werror])
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
dnl -fcolor-diagnostics: help find where bugs are in the wall of text (clang)
dnl -fdiagnostics-color=ARG: help find where bugs are in the wall of text (gcc)

    dnl First check for this to avoid failing on unused include paths etc:
    NUT_CHECK_COMPILE_FLAG([-Qunused-arguments])

    m4_foreach_w([TESTCOMPILERFLAG], [
        -Wno-reserved-identifier
    ], [
        NUT_CHECK_COMPILE_FLAG([TESTCOMPILERFLAG])
    ])

    dnl Note: each m4_foreach_w arg must be named uniquely
    dnl Note: Seems -fcolor-diagnostics is clang-only and sometimes
    dnl gcc blindly accepts it in test and fails to use later.
    AS_IF([test x"${nut_enable_Wcolor}" = xyes], [
        m4_foreach_w([TESTCOMPILERFLAG_COLOR], [
            -fdiagnostics-color=always
        ], [
            NUT_CHECK_COMPILE_FLAG([TESTCOMPILERFLAG_COLOR])
        ])
    ], [AC_MSG_NOTICE([NOT checking for options to request colorized compiler output (pass --enable-Wcolor for that)])])

    dnl Last check for this to avoid accepting anything regardless of support.
    dnl NOTE that some toolkit versions accept this option blindly and without
    dnl really supporting it (but not erroring out on it, either):
    dnl    cc1: note: unrecognized command-line option '-Wno-unknown-warning-option'
    dnl         may have been intended to silence earlier diagnostics
    NUT_CHECK_COMPILE_FLAG([-Wno-unknown-warning-option])

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
dnl # confuse the compiler assumptions - along with its provided headers)...
dnl # ideally; in practice however cppunit, net-snmp and some system include
dnl # files do cause grief to picky compiler settings (more so from third
dnl # party packages shipped via /usr/local/... namespace):
    AS_IF([test "x$cross_compiling" != xyes], [
        AS_IF([test "x$CLANGCC" = xyes -o "x$GCC" = xyes], [
dnl #            CFLAGS="-isystem /usr/include $CFLAGS"
            AS_IF([test -d /usr/local/include],
                [CFLAGS="-isystem /usr/local/include $CFLAGS"])
            AS_IF([test -d /usr/pkg/include],
                [CFLAGS="-isystem /usr/pkg/include $CFLAGS"])
        ])
        AS_IF([test "x$CLANGXX" = xyes -o "x$GXX" = xyes], [
dnl #           CXXFLAGS="-isystem /usr/include $CXXFLAGS"
            AS_IF([test -d /usr/local/include],
                [CXXFLAGS="-isystem /usr/local/include $CXXFLAGS"])
            AS_IF([test -d /usr/pkg/include],
                [CXXFLAGS="-isystem /usr/pkg/include $CXXFLAGS"])
        ])
    ])

dnl # Default to avoid noisy warnings on older compilers
dnl # (gcc-4.x, clang-3.x) due to their preference of
dnl # ANSI C (C89/C90) out of the box. While NUT codebase
dnl # currently can build in that mode, reliability of
dnl # results is uncertain - third-party and/or system
dnl # headers and libs seemingly no longer care for C90
dnl # on modern systems, and we have no recent data from
dnl # truly legacy systems which have no choice.
dnl # Some distributions and platforms also have problems
dnl # building in "strict C" mode, so for the GNU-compatible
dnl # compilers we default to the GNU C/C++ dialects.
    AS_IF([test "x$GCC" = xyes -o "x$CLANGCC" = xyes],
        [AS_CASE(["${CFLAGS}"], [*"-std="*|*"-ansi"*], [],
            [AC_LANG_PUSH([C])
             AX_CHECK_COMPILE_FLAG([-std=gnu99],
                [AC_MSG_NOTICE([Defaulting C standard support to GNU C99 on a GCC or CLANG compatible compiler])
                 CFLAGS="$CFLAGS -std=gnu99"
                ], [], [-Werror])
             AC_LANG_POP([C])
         ])
    ])

dnl # Note: this might upset some very old compilers
dnl # but then by default we wouldn't build C++ parts
    AS_IF([test "x$GCC" = xyes -o "x$CLANGCC" = xyes],
        [AS_CASE(["${CXXFLAGS}"], [*"-std="*|*"-ansi"*], [],
            [AC_LANG_PUSH([C++])
             AX_CHECK_COMPILE_FLAG([-std=gnu++11],
                [AC_MSG_NOTICE([Defaulting C++ standard support to GNU C++11 on a GCC or CLANG compatible compiler])
                 CXXFLAGS="$CXXFLAGS -std=gnu++11"
                ], [], [-Werror])
             AC_LANG_POP([C++])
         ])
    ])

])

AC_DEFUN([NUT_COMPILER_FAMILY_FLAGS_DEFAULT_STANDARD],
[
dnl # Default to avoid noisy warnings on older compilers
dnl # (gcc-4.x, clang-3.x) due to their preference of
dnl # ANSI C (C89/C90) out of the box. While NUT codebase
dnl # currently can build in that mode, reliability of
dnl # results is uncertain - third-party and/or system
dnl # headers and libs seemingly no longer care for C90
dnl # on modern systems, and we have no recent data from
dnl # truly legacy systems which have no choice.
dnl # Some distributions and platforms also have problems
dnl # building in "strict C" mode, so for the GNU-compatible
dnl # compilers we default to the GNU C/C++ dialects.
    AS_IF([test "x$GCC" = xyes -o "x$CLANGCC" = xyes],
        [AS_CASE(["${CFLAGS}"], [*"-std="*|*"-ansi"*], [],
            [AC_LANG_PUSH([C])
             AX_CHECK_COMPILE_FLAG([-std=gnu99],
                [AC_MSG_NOTICE([Defaulting C standard support to GNU C99 on a GCC or CLANG compatible compiler])
                 CFLAGS="$CFLAGS -std=gnu99"
                ], [], [-Werror])
             AC_LANG_POP([C])
         ])
    ])

dnl # Note: this might upset some very old compilers
dnl # but then by default we wouldn't build C++ parts
    AS_IF([test "x$GCC" = xyes -o "x$CLANGCC" = xyes],
        [AS_CASE(["${CXXFLAGS}"], [*"-std="*|*"-ansi"*], [],
            [AC_LANG_PUSH([C++])
             AX_CHECK_COMPILE_FLAG([-std=gnu++11],
                [AC_MSG_NOTICE([Defaulting C++ standard support to GNU C++11 on a GCC or CLANG compatible compiler])
                 CXXFLAGS="$CXXFLAGS -std=gnu++11"
                ], [], [-Werror])
             AC_LANG_POP([C++])
         ])
    ])

])
