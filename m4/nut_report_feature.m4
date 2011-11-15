dnl automated feature report at the end of configure script.
dnl it also AC_DEFINE() and AM_CONDITIONAL the matching variable.
dnl for example, "usb" (--with-usb) will give
dnl nut_with_usb and WITH_USB (both macros, and
dnl AM_CONDITIONAL)

AC_DEFUN([NUT_REPORT],
[  if test -z "${nut_report_feature_flag}"; then
      nut_report_feature_flag="1"
      ac_clean_files="${ac_clean_files} conf_nut_report_feature"
      echo > conf_nut_report_feature
      echo "Configuration summary:" >> conf_nut_report_feature
      echo "======================" >> conf_nut_report_feature
   fi
   echo "$1: $2" >> conf_nut_report_feature
])

AC_DEFUN([NUT_REPORT_FEATURE],
[
   AC_MSG_CHECKING([whether to $1])
   AC_MSG_RESULT([$2 $3])
   NUT_REPORT([$1], [$2 $3])

   AM_CONDITIONAL([$4], test "$2" = "yes")
   if test "$2" = "yes"; then
      AC_DEFINE_UNQUOTED($4, 1, $5)
   fi
])

AC_DEFUN([NUT_PRINT_FEATURE_REPORT],
[
   cat conf_nut_report_feature
])
