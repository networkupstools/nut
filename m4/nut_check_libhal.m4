dnl Check for LIBHAL compiler flags. On success, set nut_have_libhal="yes"
dnl and set LIBHAL_CFLAGS and LIBHAL_LDFLAGS. On failure, set
dnl nut_have_libhal="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBHAL], 
[
if test -z "${nut_have_libhal_seen}"; then
   nut_have_libhal_seen=yes

   nut_have_libhal=yes
   AC_MSG_CHECKING(for libhal cflags via pkg-config)
	LIBHAL_CFLAGS=`pkg-config --silence-errors --cflags hal`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libhal=no
   else
	AC_MSG_RESULT(${LIBHAL_CFLAGS})
   fi

   AC_MSG_CHECKING(for libhal ldflags via pkg-config)
   LIBHAL_LDFLAGS=`pkg-config --silence-errors --libs hal`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libhal=no
   else
	AC_MSG_RESULT(${LIBHAL_LDFLAGS})
   fi

   dnl if this didn't work, try some standard places. For example,
   dnl HAL 0.5.8 and 0.5.8.1 contain pkg-config bugs.

   if test "${nut_have_libhal}" != "yes"; then

      dnl try again
      nut_have_libhal=yes

      CFLAGS_ORIG="${CFLAGS}"
      CPPFLAGS_ORIG="${CPPFLAGS}"
      LDFLAGS_ORIG="${LDFLAGS}"

      LIBHAL_CFLAGS="-DDBUS_API_SUBJECT_TO_CHANGE -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include"
      LIBHAL_LDFLAGS="-lhal -ldbus-1 -lpthread"
      
      CFLAGS="${LIBHAL_CFLAGS}"
      CPPFLAGS="${LIBHAL_CFLAGS}"
      LDFLAGS="${LIBHAL_LDFLAGS}"
      AC_CHECK_HEADER(libhal.h, , [nut_have_libhal=no])
      AC_CHECK_LIB(hal, libhal_ctx_init, [:], [nut_have_libhal=no], 
	${LIBHAL_LDFLAGS})

      CFLAGS="${CFLAGS_ORIG}"
      CPPFLAGS="${CPPFLAGS_ORIG}"
      LDFLAGS="${LDFLAGS_ORIG}"
   fi

fi
])

