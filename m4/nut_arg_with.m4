dnl simplified declaration of some feature options

dnl Working With External Software (might name a variant or other contextual arg)
dnl https://www.gnu.org/software/autoconf/manual/autoconf-2.66/html_node/External-Software.html#External-Software
AC_DEFUN([NUT_ARG_WITH],
[  AC_ARG_WITH($1,
      AC_HELP_STRING([--with-$1], [$2 ($3)]),
      [nut_with_$1="${withval}"],
      [nut_with_$1="$3"]
   )
])

dnl Enable a feature (might name a variant), or yes/no
dnl https://www.gnu.org/software/autoconf/manual/autoconf-2.66/html_node/Package-Options.html
AC_DEFUN([NUT_ARG_ENABLE],
[  AC_ARG_ENABLE($1,
      AC_HELP_STRING([--enable-$1], [$2 ($3)]),
      [nut_enable_$1="${enableval}"],
      [nut_enable_$1="$3"]
   )
])
