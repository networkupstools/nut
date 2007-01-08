dnl automated feature report at the end of configure script

AC_DEFUN([NUT_REPORT],
[  if test -z "${nut_report_feature_flag}"; then
      nut_report_feature_flag="1"
      ac_clean_files="${ac_clean_files} conf_nut_report_feature"
      echo -n > conf_nut_report_feature
   fi
   echo "$1: $2" >> conf_nut_report_feature
])

AC_DEFUN([NUT_REPORT_FEATURE],
[  
   AC_MSG_CHECKING([whether to $1])
   AC_MSG_RESULT([$2])
   NUT_REPORT([$1], [$2])
])

AC_DEFUN([NUT_PRINT_FEATURE_REPORT],
[  echo 
   echo Configuration summary:
   cat conf_nut_report_feature
])
