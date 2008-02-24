dnl Check for LIBNEON compiler flags. On success, set nut_have_neon="yes"
dnl and set LIBNEON_CFLAGS and LIBNEON_LDFLAGS. On failure, set
dnl nut_have_neon="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBNEON], 
[
if test -z "${nut_have_neon_seen}"; then
   nut_have_neon_seen=yes

   dnl save CFLAGS and LDFLAGS
   CFLAGS_ORIG="${CFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   dnl innocent until proven guilty
   nut_have_neon=yes

   dnl Check for neon libs and flags
   AC_MSG_CHECKING(for neon cflags)
   CFLAGS=`neon-config --cflags 2>/dev/null`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_neon=no
   else
	AC_MSG_RESULT(${CFLAGS})
   fi

   AC_MSG_CHECKING(for neon libs)
   LDFLAGS=`neon-config --libs 2>/dev/null`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_neon=no
   else
	AC_MSG_RESULT(${LDFLAGS})
   fi

   if test "${nut_have_neon}" = "yes"; then
	LIBNEON_CFLAGS="${CFLAGS}"
	LIBNEON_LDFLAGS="${LDFLAGS}"
   fi

   dnl restore original CFLAGS and LDFLAGS
   CFLAGS="${CFLAGS_ORIG}"
   LDFLAGS="${LDFLAGS_ORIG}"

fi
])
