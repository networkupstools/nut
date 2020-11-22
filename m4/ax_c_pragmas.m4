dnl Check for current compiler support of specific pragmas we use,
dnl e.g. diagnostics management to keep warnings quiet sometimes

AC_DEFUN([AX_C_PRAGMAS], [
  CFLAGS_SAVED="${CFLAGS}"

  dnl # This is expected to fail builds with unknown pragma names on GCC or CLANG at least
  AS_IF([test "${CLANG}" = "yes"],
    [CFLAGS="${CFLAGS_SAVED} -Werror=pragmas -Werror=unknown-warning-option"],
    [AS_IF([test "${GCC}" = "yes"],
dnl ### Despite the docs, this dies with lack of (apparently) support for
dnl ### -Wunknown-warning(-options) on all GCC versions I tried (v5-v10)
dnl ###        [CFLAGS="${CFLAGS_SAVED} -Werror=pragmas -Werror=unknown-warning"],
        [CFLAGS="${CFLAGS_SAVED} -Wall -Wextra -Werror"],
        [CFLAGS="${CFLAGS_SAVED} -Wall -Wextra -Werror"])
    ])

  AC_CACHE_CHECK([for pragma GCC diagnostic push and pop],
    [ax_cv__pragma__gcc__diags_push_pop],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[
#pragma GCC diagnostic push
#pragma GCC diagnostic pop
        ]], [])],
      [ax_cv__pragma__gcc__diags_push_pop=yes],
      [ax_cv__pragma__gcc__diags_push_pop=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_push_pop" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP], 1, [define if your compiler has #pragma GCC diagnostic push and pop])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-nonliteral"],
    [ax_cv__pragma__gcc__diags_ignored_format_nonliteral],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wformat-nonliteral"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_nonliteral=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_nonliteral=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_nonliteral" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-nonliteral"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-security"],
    [ax_cv__pragma__gcc__diags_ignored_format_security],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wformat-security"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_security=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_security=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_security" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-security"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wunreachable-code-break"],
    [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wunreachable-code-break"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break=yes],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code_break" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BREAK], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wunreachable-code-break"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wunreachable-code"],
    [ax_cv__pragma__gcc__diags_ignored_unreachable_code],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wunreachable-code"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code=yes],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wunreachable-code"])
  ])

  dnl # Meta-macros for simpler use-cases where we pick
  dnl # equivalent-effect macros for different compiler versions
  AS_IF([test "$ax_cv__pragma__gcc__diags_push_pop" = "yes"],[
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_security" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_format_nonliteral" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wformat-nonliteral" or "-Wformat-security" and for push-pop support])
    ])
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code_break" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wunreachable-code(-break)" and for push-pop support])
    ])
  ])

  AC_CACHE_CHECK([for pragma BOGUSforTest],
    [ax_cv__pragma__bogus],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma BOGUSforTest]], [])],
      [ax_cv__pragma__bogus=yes],
      [ax_cv__pragma__bogus=no]
    )]
  )

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-WBOGUSforTest"],
    [ax_cv__pragma__bogus_diag],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-WBOGUSforTest"]], [])],
      [ax_cv__pragma__bogus_diag=yes],
      [ax_cv__pragma__bogus_diag=no]
    )]
  )

  CFLAGS="${CFLAGS_SAVED}"
])
