dnl Check for LIBPOWERMAN compiler flags. On success, set nut_have_libpowerman="yes"
dnl and set LIBPOWERMAN_CFLAGS and LIBPOWERMAN_LDFLAGS. On failure, set
dnl nut_have_libpowerman="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBPOWERMAN], 
[
if test -z "${nut_have_libpowerman_seen}"; then
   nut_have_libpowerman_seen=yes

   dnl save CFLAGS and LDFLAGS
   CFLAGS_ORIG="${CFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   dnl innocent until proven guilty
   nut_have_libpowerman=yes

   dnl Check for libpowerman libs and flags
   AC_MSG_CHECKING(for libpowerman cflags)
   CFLAGS=`pkg-config --cflags libpowerman 2>/dev/null`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libpowerman=no
   else
	AC_MSG_RESULT(${CFLAGS})
   fi

   AC_MSG_CHECKING(for libpowerman libs)
   LDFLAGS=`pkg-config --libs libpowerman 2>/dev/null`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libpowerman=no
   else
	AC_MSG_RESULT(${LDFLAGS})
   fi

   if test "${nut_have_libpowerman}" = "yes"; then
	LIBPOWERMAN_CFLAGS="${CFLAGS}"
	LIBPOWERMAN_LDFLAGS="${LDFLAGS}"
   fi

   dnl restore original CFLAGS and LDFLAGS
   CFLAGS="${CFLAGS_ORIG}"
   LDFLAGS="${LDFLAGS_ORIG}"

fi
])

