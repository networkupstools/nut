dnl Check for LIBNETSNMP compiler flags. On success, set
dnl nut_have_libnetsnmp="yes" and set NUT_LIBNETSNMP_CFLAGS and
dnl NUT_LIBNETSNMP_LDFLAGS. On failure, set nut_have_libnetsnmp="no".
dnl This macro can be run multiple times, but will do the checking only
dnl once.

AC_DEFUN([NUT_CHECK_LIBNETSNMP], 
[
if test -z "${nut_have_libnetsnmp_seen}"; then
   nut_have_libnetsnmp_seen=yes

   dnl innocent until proven guilty
   nut_have_libnetsnmp=yes
   AC_MSG_CHECKING(for Net-SNMP cflags)
   NUT_LIBNETSNMP_CFLAGS=`net-snmp-config --cflags 2>/dev/null`

   if (test "$?" != "0")
   then
	AC_MSG_RESULT([not found])
	nut_have_libnetsnmp=no
   else
	AC_MSG_RESULT([${NUT_LIBNETSNMP_CFLAGS}])
   fi

   AC_MSG_CHECKING(for Net-SNMP libs)
   NUT_LIBNETSNMP_LDFLAGS=`net-snmp-config --libs 2>/dev/null`

   if (test "$?" != "0")
   then
	AC_MSG_RESULT([not found])
	nut_have_libnetsnmp=no
   else
	AC_MSG_RESULT([${NUT_LIBNETSNMP_LDFLAGS}])
   fi

fi
])

