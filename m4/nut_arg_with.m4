dnl simplified declaration of some feature options

AC_DEFUN([NUT_ARG_WITH],
[  AC_ARG_WITH($1,
      AC_HELP_STRING([--with-$1], [$2 ($3)]),
      [nut_with_$1="${withval}"],
      [nut_with_$1="$3"]
   )
])
