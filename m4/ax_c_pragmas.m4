dnl Check for current compiler support of specific pragmas we use,
dnl e.g. diagnostics management to keep warnings quiet sometimes

AC_DEFUN([AX_C_PRAGMAS], [
  CFLAGS_SAVED="${CFLAGS}"
  CXXFLAGS_SAVED="${CXXFLAGS}"

  dnl ### To be sure, bolt the language
  AC_LANG_PUSH([C])

  dnl # This is expected to fail builds with unknown pragma names on GCC or CLANG at least
  AS_IF([test "${CLANG}" = "yes"],
    [CFLAGS="${CFLAGS_SAVED} -Werror=pragmas -Werror=unknown-warning-option"
     CXXFLAGS="${CXXFLAGS_SAVED} -Werror=pragmas -Werror=unknown-warning-option"],
    [AS_IF([test "${GCC}" = "yes"],
dnl ### Despite the docs, this dies with lack of (apparently) support for
dnl ### -Wunknown-warning(-options) on all GCC versions I tried (v5-v10)
dnl ###        [CFLAGS="${CFLAGS_SAVED} -Werror=pragmas -Werror=unknown-warning"],
        [CFLAGS="${CFLAGS_SAVED} -Wall -Wextra -Werror"
         CXXFLAGS="${CXXFLAGS_SAVED} -Wall -Wextra -Werror"],
        [CFLAGS="${CFLAGS_SAVED} -Wall -Wextra -Werror"
         CXXFLAGS="${CXXFLAGS_SAVED} -Wall -Wextra -Werror"])
    ])

  AC_CACHE_CHECK([for pragma GCC diagnostic push and pop (outside functions)],
    [ax_cv__pragma__gcc__diags_push_pop_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[
#pragma GCC diagnostic push
#pragma GCC diagnostic pop
        ]], [])],
      [ax_cv__pragma__gcc__diags_push_pop_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_push_pop_besidefunc=no]
    )]
  )

  AC_CACHE_CHECK([for pragma clang diagnostic push and pop (outside functions)],
    [ax_cv__pragma__clang__diags_push_pop_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[
#ifdef __clang__
#endif
#pragma clang diagnostic push
#pragma clang diagnostic pop
        ]], [])],
      [ax_cv__pragma__clang__diags_push_pop_besidefunc=yes],
      [ax_cv__pragma__clang__diags_push_pop_besidefunc=no]
    )]
  )

  dnl # Currently our code uses these pragmas as close to lines that cause
  dnl # questions from linters as possible. GCC before 4.5 did not allow
  dnl # for diag pragmas inside function bodies, but also did not complain
  dnl # about messy code as new compilers do. For completeness, we support
  dnl # the possibility of defining larger-scoped pragmas around whole
  dnl # function bodies. In practice, we don't currently do that so in
  dnl # most cases the shorter names (without _INSIDEFUNC) are used with
  dnl # that implied meaning.

  AC_CACHE_CHECK([for pragma GCC diagnostic push and pop (inside functions)],
    [ax_cv__pragma__gcc__diags_push_pop_insidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic push
#pragma GCC diagnostic pop
}
        ]], [])],
      [ax_cv__pragma__gcc__diags_push_pop_insidefunc=yes],
      [ax_cv__pragma__gcc__diags_push_pop_insidefunc=no]
    )]
  )

  AC_CACHE_CHECK([for pragma clang diagnostic push and pop (inside functions)],
    [ax_cv__pragma__clang__diags_push_pop_insidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#ifdef __clang__
#endif
#pragma clang diagnostic push
#pragma clang diagnostic pop
}
        ]], [])],
      [ax_cv__pragma__clang__diags_push_pop_insidefunc=yes],
      [ax_cv__pragma__clang__diags_push_pop_insidefunc=no]
    )]
  )

  AS_IF([test "$ax_cv__pragma__gcc__diags_push_pop_insidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic push and pop inside function bodies])
  ])

  AS_IF([test "$ax_cv__pragma__gcc__diags_push_pop_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic push and pop outside function bodies])
  ])

  AS_IF([test "$ax_cv__pragma__gcc__diags_push_pop_besidefunc" = "yes" && test "$ax_cv__pragma__gcc__diags_push_pop_insidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP], 1, [define if your compiler has #pragma GCC diagnostic push and pop])
  ])

  AS_IF([test "$ax_cv__pragma__clang__diags_push_pop_insidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_CLANG_DIAGNOSTIC_PUSH_POP_INSIDEFUNC], 1, [define if your compiler has #pragma clang diagnostic push and pop inside function bodies])
  ])

  AS_IF([test "$ax_cv__pragma__clang__diags_push_pop_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_CLANG_DIAGNOSTIC_PUSH_POP_BESIDEFUNC], 1, [define if your compiler has #pragma clang diagnostic push and pop outside function bodies])
  ])

  AS_IF([test "$ax_cv__pragma__clang__diags_push_pop_besidefunc" = "yes" && test "$ax_cv__pragma__clang__diags_push_pop_insidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_CLANG_DIAGNOSTIC_PUSH_POP], 1, [define if your compiler has #pragma clang diagnostic push and pop])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wunused-function"],
    [ax_cv__pragma__gcc__diags_ignored_unused_function],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wunused-function"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_unused_function=yes],
      [ax_cv__pragma__gcc__diags_ignored_unused_function=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unused_function" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNUSED_FUNCTION], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wunused-function"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-nonliteral"],
    [ax_cv__pragma__gcc__diags_ignored_format_nonliteral],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_nonliteral=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_nonliteral=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_nonliteral" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-nonliteral"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-nonliteral" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_format_nonliteral_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wformat-nonliteral"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_nonliteral_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_nonliteral_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_nonliteral_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-nonliteral" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-security"],
    [ax_cv__pragma__gcc__diags_ignored_format_security],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wformat-security"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_security=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_security=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_security" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-security"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-security" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_format_security_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wformat-security"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_security_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_security_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_security_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-security" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-truncation"],
    [ax_cv__pragma__gcc__diags_ignored_format_truncation],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wformat-truncation"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_truncation=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_truncation=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_truncation" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-truncation"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-truncation" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_format_truncation_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wformat-truncation"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_truncation_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_truncation_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_truncation_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-truncation" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wstringop-truncation"],
    [ax_cv__pragma__gcc__diags_ignored_stringop_truncation],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wstringop-truncation"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_stringop_truncation=yes],
      [ax_cv__pragma__gcc__diags_ignored_stringop_truncation=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_stringop_truncation" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRINGOP_TRUNCATION], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wstringop-truncation"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wstringop-truncation" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_stringop_truncation_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wstringop-truncation"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_stringop_truncation_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_stringop_truncation_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_stringop_truncation_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRINGOP_TRUNCATION_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wstringop-truncation" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtype-limits"],
    [ax_cv__pragma__gcc__diags_ignored_type_limits],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wtype-limits"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_type_limits=yes],
      [ax_cv__pragma__gcc__diags_ignored_type_limits=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_type_limits" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtype-limits"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtype-limits" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_type_limits_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wtype-limits"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_type_limits_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_type_limits_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_type_limits_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtype-limits" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Warray-bounds"],
    [ax_cv__pragma__gcc__diags_ignored_array_bounds],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Warray-bounds"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_array_bounds=yes],
      [ax_cv__pragma__gcc__diags_ignored_array_bounds=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_array_bounds" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ARRAY_BOUNDS], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Warray-bounds"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Warray-bounds" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_array_bounds_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Warray-bounds"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_array_bounds_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_array_bounds_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_array_bounds_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ARRAY_BOUNDS_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Warray-bounds" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"],
    [ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare=yes],
      [ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_tautological_constant_out_of_range_compare_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"],
    [ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare=yes],
      [ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_tautological_unsigned_zero_compare_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtautological-compare"],
    [ax_cv__pragma__gcc__diags_ignored_tautological_compare],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wtautological-compare"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_tautological_compare=yes],
      [ax_cv__pragma__gcc__diags_ignored_tautological_compare=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_tautological_compare" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_COMPARE], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtautological-compare"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wtautological-compare" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_tautological_compare_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wtautological-compare"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_tautological_compare_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_tautological_compare_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_tautological_compare_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_COMPARE_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wtautological-compare" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wsign-compare"],
    [ax_cv__pragma__gcc__diags_ignored_sign_compare],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wsign-compare"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_sign_compare=yes],
      [ax_cv__pragma__gcc__diags_ignored_sign_compare=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_sign_compare" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SIGN_COMPARE], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wsign-compare"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wsign-compare" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_sign_compare_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wsign-compare"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_sign_compare_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_sign_compare_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_sign_compare_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SIGN_COMPARE_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wsign-compare" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wunreachable-code-break"],
    [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wunreachable-code-break"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break=yes],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code_break" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BREAK], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wunreachable-code-break"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wunreachable-code-break" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wunreachable-code-break"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_break_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code_break_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BREAK_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wunreachable-code-break" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wunreachable-code"],
    [ax_cv__pragma__gcc__diags_ignored_unreachable_code],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wunreachable-code"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code=yes],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wunreachable-code"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wunreachable-code" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_unreachable_code_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wunreachable-code"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_unreachable_code_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wunreachable-code" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-overflow"],
    [ax_cv__pragma__gcc__diags_ignored_format_overflow],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wformat-overflow"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_overflow=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_overflow=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_overflow" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_OVERFLOW], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-overflow"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wformat-overflow" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_format_overflow_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wformat-overflow"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_format_overflow_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_format_overflow_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_overflow_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_OVERFLOW_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wformat-overflow" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wcovered-switch-default"],
    [ax_cv__pragma__gcc__diags_ignored_covered_switch_default],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wcovered-switch-default"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_covered_switch_default=yes],
      [ax_cv__pragma__gcc__diags_ignored_covered_switch_default=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_covered_switch_default" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wcovered-switch-default"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wcovered-switch-default" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_covered_switch_default_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wcovered-switch-default"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_covered_switch_default_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_covered_switch_default_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_covered_switch_default_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wcovered-switch-default" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wcast-align"],
    [ax_cv__pragma__gcc__diags_ignored_cast_align],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wcast-align"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_cast_align=yes],
      [ax_cv__pragma__gcc__diags_ignored_cast_align=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cast_align" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wcast-align"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wcast-align" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_cast_align_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wcast-align"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_cast_align_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_cast_align_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cast_align_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wcast-align" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wstrict-prototypes"],
    [ax_cv__pragma__gcc__diags_ignored_strict_prototypes],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_strict_prototypes=yes],
      [ax_cv__pragma__gcc__diags_ignored_strict_prototypes=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_strict_prototypes" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wstrict-prototypes"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wstrict-prototypes" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_strict_prototypes_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wstrict-prototypes"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_strict_prototypes_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_strict_prototypes_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_strict_prototypes_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wstrict-prototypes" (outside functions)])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wassign-enum"],
    [ax_cv__pragma__gcc__diags_ignored_assign_enum],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wassign-enum"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_assign_enum=yes],
      [ax_cv__pragma__gcc__diags_ignored_assign_enum=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_assign_enum" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ASSIGN_ENUM], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wassign-enum"])
  ])

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-Wassign-enum" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_assign_enum_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wassign-enum"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_assign_enum_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_assign_enum_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_assign_enum_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ASSIGN_ENUM_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wassign-enum" (outside functions)])
  ])

  AC_LANG_POP([C])

  dnl ### Series of tests for C++ specific pragmas
  AC_LANG_PUSH([C++])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"],
    [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic=yes],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT_PEDANTIC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"])
  ])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wc++98-compat-pedantic" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT_PEDANTIC_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wc++98-compat-pedantic" (outside functions)])
  ])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wc++98-compat"],
    [ax_cv__pragma__gcc__diags_ignored_cxx98_compat],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wc++98-compat"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat=yes],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wc++98-compat"])
  ])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wc++98-compat" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wc++98-compat"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_cxx98_compat_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wc++98-compat" (outside functions)])
  ])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wglobal-constructors"],
    [ax_cv__pragma__gcc__diags_ignored_global_constructors],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wglobal-constructors"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_global_constructors=yes],
      [ax_cv__pragma__gcc__diags_ignored_global_constructors=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_global_constructors" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wglobal-constructors"])
  ])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wglobal-constructors" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_global_constructors_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wglobal-constructors"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_global_constructors_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_global_constructors_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_global_constructors_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wglobal-constructors" (outside functions)])
  ])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wexit-time-destructors"],
    [ax_cv__pragma__gcc__diags_ignored_exit_time_destructors],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
}
]], [])],
      [ax_cv__pragma__gcc__diags_ignored_exit_time_destructors=yes],
      [ax_cv__pragma__gcc__diags_ignored_exit_time_destructors=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_exit_time_destructors" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wexit-time-destructors"])
  ])

  AC_CACHE_CHECK([for C++ pragma GCC diagnostic ignored "-Wexit-time-destructors" (outside functions)],
    [ax_cv__pragma__gcc__diags_ignored_exit_time_destructors_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-Wexit-time-destructors"]], [])],
      [ax_cv__pragma__gcc__diags_ignored_exit_time_destructors_besidefunc=yes],
      [ax_cv__pragma__gcc__diags_ignored_exit_time_destructors_besidefunc=no]
    )]
  )
  AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_exit_time_destructors_besidefunc" = "yes"],[
    AC_DEFINE([HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS_BESIDEFUNC], 1, [define if your compiler has #pragma GCC diagnostic ignored "-Wexit-time-destructors" (outside functions)])
  ])

  AC_LANG_POP([C++])

  dnl # Meta-macros for simpler use-cases where we pick
  dnl # equivalent-effect macros for different compiler versions
  AS_IF([test "$ax_cv__pragma__gcc__diags_push_pop_insidefunc" = "yes"],[
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_security" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_format_nonliteral" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wformat-nonliteral" or "-Wformat-security" and for push-pop support])
    ])
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_truncation" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_stringop_truncation" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wformat-truncation" or "-Werror=stringop-truncation" and for push-pop support])
    ])
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code_break" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wunreachable-code(-break)" and for push-pop support])
    ])
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wc++98-compat(-pedantic)" and for push-pop support])
    ])
  ])

  AS_IF([test "$ax_cv__pragma__gcc__diags_push_pop_besidefunc" = "yes"],[
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_security" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_format_nonliteral" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL_BESIDEFUNC], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wformat-nonliteral" or "-Wformat-security" and for push-pop support (outside function bodies)])
    ])
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_format_truncation" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_stringop_truncation" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION_BESIDEFUNC], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wformat-truncation" or "-Werror=stringop-truncation" and for push-pop support (outside function bodies)])
    ])
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_unreachable_code_break" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BESIDEFUNC], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wunreachable-code(-break)" and for push-pop support (outside function bodies)])
    ])
    AS_IF([test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat_pedantic" = "yes" || test "$ax_cv__pragma__gcc__diags_ignored_cxx98_compat" = "yes" ],[
        AC_DEFINE([HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT_BESIDEFUNC], 1, [define if your compiler has pragmas for GCC diagnostic ignored "-Wc++98-compat(-pedantic)" and for push-pop support (outside function bodies)])
    ])
  ])

  dnl ### Sanity check if the CLI options actually work:
  AC_CACHE_CHECK([for pragma BOGUSforTest],
    [ax_cv__pragma__bogus],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma BOGUSforTest
}
]], [])],
      [ax_cv__pragma__bogus=yes],
      [ax_cv__pragma__bogus=no]
    )]
  )

  AC_CACHE_CHECK([for pragma BOGUSforTest (outside functions)],
    [ax_cv__pragma__bogus_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma BOGUSforTest]], [])],
      [ax_cv__pragma__bogus_besidefunc=yes],
      [ax_cv__pragma__bogus_besidefunc=no]
    )]
  )

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-WBOGUSforTest"],
    [ax_cv__pragma__bogus_diag],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[void func(void) {
#pragma GCC diagnostic ignored "-WBOGUSforTest"
}
]], [])],
      [ax_cv__pragma__bogus_diag=yes],
      [ax_cv__pragma__bogus_diag=no]
    )]
  )

  AC_CACHE_CHECK([for pragma GCC diagnostic ignored "-WBOGUSforTest" (outside functions)],
    [ax_cv__pragma__bogus_diag_besidefunc],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#pragma GCC diagnostic ignored "-WBOGUSforTest"]], [])],
      [ax_cv__pragma__bogus_diag_besidefunc=yes],
      [ax_cv__pragma__bogus_diag_besidefunc=no]
    )]
  )

  AS_IF([test "${ax_cv__pragma__bogus}" != "no"],
    [AC_MSG_WARN([A bogus test that was expected to fail did not! ax_cv__pragma__bogus=$ax_cv__pragma__bogus (not 'no')])])

  AS_IF([test "${ax_cv__pragma__bogus_besidefunc}" != "no"],
    [AC_MSG_WARN([A bogus test that was expected to fail did not! ax_cv__pragma__bogus_besidefunc=$ax_cv__pragma__bogus_besidefunc (not 'no')])])

  AS_IF([test "${ax_cv__pragma__bogus_diag}" != "no"],
    [AC_MSG_WARN([A bogus test that was expected to fail did not! ax_cv__pragma__bogus_diag=$ax_cv__pragma__bogus_diag (not 'no')])])

  AS_IF([test "${ax_cv__pragma__bogus_diag_besidefunc}" != "no"],
    [AC_MSG_WARN([A bogus test that was expected to fail did not! ax_cv__pragma__bogus_diag_besidefunc=$ax_cv__pragma__bogus_diag_besidefunc (not 'no')])])

  CFLAGS="${CFLAGS_SAVED}"
  CXXFLAGS="${CXXFLAGS_SAVED}"
])

AC_DEFUN([AX_C_PRINTF_STRING_NULL], [
  dnl ### To be sure, bolt the language
  AC_LANG_PUSH([C])

  AC_CACHE_CHECK([for practical support to pritnf("%s", NULL)],
    [ax_cv__printf_string_null],
    [AC_RUN_IFELSE(
        [AC_LANG_PROGRAM([dnl
#include <stdio.h>
#include <strings.h>
], [dnl
char buf[128];
char *s = NULL;
int res = snprintf(buf, sizeof(buf), "%s", s);
buf[sizeof(buf)-1] = '\0';
if (res < 0) {
    printf(stderr, "FAILED to snprintf() a NULL string argument");
    exit 1;
}
if (buf[0] == '\0')
    printf(stderr, "RETURNED empty string from snprintf() with a NULL string argument");
    exit 0;
}
if (strcasestr(buf, 'null') == NULL)
    printf(stderr, "RETURNED some string from snprintf() with a NULL string argument: '%s'", buf);
    exit 0;
}
printf(stderr, "SUCCESS: RETURNED a string that contains something like 'null' from snprintf() with a NULL string argument: '%s'", buf);
exit 0;
            ])],
        [ax_cv__printf_string_null=yes],
        [ax_cv__printf_string_null=no]
    )]
  )

  AS_IF([test "$ax_cv__printf_string_null" = "yes"],[
    AC_DEFINE([HAVE_PRINTF_STRING_NULL], 1, [define if your libc can printf("%s", NULL) sanely])
  ])

  AC_LANG_POP([C])
])
