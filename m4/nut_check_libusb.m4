dnl Check for LIBUSB compiler flags. On success, set nut_have_libusb="yes"
dnl and set LIBUSB_CFLAGS and LIBUSB_LDFLAGS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBUSB], 
[
if test -z "${nut_have_libusb_seen}"; then
   nut_have_libusb_seen=yes

   dnl save CFLAGS and LDFLAGS
   CFLAGS_ORIG="${CFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   dnl innocent until proven guilty
   nut_have_libusb=yes

   dnl Check for libusb libs and flags
   AC_MSG_CHECKING(for libusb cflags)
   CFLAGS=`libusb-config --cflags 2>/dev/null`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libusb=no
   else
	AC_MSG_RESULT(${CFLAGS})
   fi

   AC_MSG_CHECKING(for libusb libs)
   LDFLAGS=`libusb-config --libs 2>/dev/null`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libusb=no
   else
	AC_MSG_RESULT(${LDFLAGS})
   fi

   if test "${nut_have_libusb}" = "yes"; then
	LIBUSB_CFLAGS="${CFLAGS}"
	LIBUSB_LDFLAGS="${LDFLAGS}"
   fi

   dnl restore original CFLAGS and LDFLAGS
   CFLAGS="${CFLAGS_ORIG}"
   LDFLAGS="${LDFLAGS_ORIG}"

fi
])

