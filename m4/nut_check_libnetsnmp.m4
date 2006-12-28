dnl Check for LIBNETSNMP compiler flags. On success, set
dnl nut_have_libnetsnmp="yes" and set LIBNETSNMP_CFLAGS and
dnl LIBNETSNMP_LDFLAGS. On failure, set nut_have_libnetsnmp="no".
dnl This macro can be run multiple times, but will do the checking only
dnl once.

AC_DEFUN([NUT_CHECK_LIBNETSNMP], 
[
if test -z "${nut_have_libnetsnmp_seen}"; then
   nut_have_libnetsnmp_seen=yes

   dnl innocent until proven guilty
   nut_have_libnetsnmp=yes
   AC_MSG_CHECKING(for Net-SNMP cflags)
   LIBNETSNMP_CFLAGS=`net-snmp-config --cflags 2>/dev/null`

   if (test "$?" != "0")
   then
	AC_MSG_RESULT([not found])
	nut_have_libnetsnmp=no
   else
	AC_MSG_RESULT([${LIBNETSNMP_CFLAGS}])
   fi

   AC_MSG_CHECKING(for Net-SNMP libs)
   LIBNETSNMP_LDFLAGS=`net-snmp-config --libs 2>/dev/null`

   if (test "$?" != "0")
   then
	AC_MSG_RESULT([not found])
	nut_have_libnetsnmp=no
   else
	AC_MSG_RESULT([${LIBNETSNMP_LDFLAGS}])
   fi

fi
])

