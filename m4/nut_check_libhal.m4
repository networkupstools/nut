dnl Check for LIBHAL compiler flags. On success, set nut_have_libhal="yes"
dnl and set LIBHAL_CFLAGS and LIBHAL_LDFLAGS. On failure, set
dnl nut_have_libhal="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBHAL], 
[
if test -z "${nut_have_libhal_seen}"; then
   nut_have_libhal_seen=yes

   CFLAGS_ORIG="${CFLAGS}"
   CPPFLAGS_ORIG="${CPPFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   nut_have_libhal=yes
   AC_MSG_CHECKING(for libhal cflags via pkg-config)
	CFLAGS=`pkg-config --silence-errors --cflags hal`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libhal=no
   else
	AC_MSG_RESULT(${CFLAGS})
   fi

   AC_MSG_CHECKING(for libhal ldflags via pkg-config)
   LDFLAGS=`pkg-config --silence-errors --libs hal`
   if (test "$?" != "0")
   then
	AC_MSG_RESULT(not found)
	nut_have_libhal=no
   else
	AC_MSG_RESULT(${LDFLAGS})
   fi

   dnl this will only work as of HAL 0.5.9
   AC_MSG_CHECKING(for libhal user via pkg-config)
   HAL_USER=`pkg-config --silence-errors  --variable=haluser hal`
   if (test -z "$HAL_USER")
   then
	HAL_USER="haldaemon"
	AC_MSG_RESULT(using default (${HAL_USER}))
   else
	AC_MSG_RESULT(${HAL_USER})
   fi
   AC_DEFINE_UNQUOTED(HAL_USER, "${HAL_USER}", [HAL user])

   dnl A request has been made to get variables for:
   dnl - addon install path
   dnl - fdi install path

   dnl if this didn't work, try some standard places. For example,
   dnl HAL 0.5.8 and 0.5.8.1 contain pkg-config bugs.

   if test "${nut_have_libhal}" != "yes"; then
      dnl try again

      CFLAGS="-DDBUS_API_SUBJECT_TO_CHANGE -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include"
      CPPFLAGS="${CFLAGS}"
      LDFLAGS="-lhal -ldbus-1 -lpthread"
      
      AC_CHECK_HEADER(libhal.h, , [nut_have_libhal=no])
      AC_CHECK_LIB(hal, libhal_ctx_init, [nut_have_libhal=yes], 
	[nut_have_libhal=no], 
	${LDFLAGS})
   fi

   if test "${nut_have_libhal}" = "yes"; then
        LIBHAL_CFLAGS="${CFLAGS}"
        LIBHAL_LDFLAGS="${LDFLAGS}"
   fi

   CFLAGS="${CFLAGS_ORIG}"
   CPPFLAGS="${CPPFLAGS_ORIG}"
   LDFLAGS="${LDFLAGS_ORIG}"

fi
])

