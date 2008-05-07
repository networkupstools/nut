dnl Check for LIBHAL compiler flags. On success, set nut_have_libhal="yes"
dnl and set LIBHAL_CFLAGS and LIBHAL_LDFLAGS. On failure, set
dnl nut_have_libhal="no". This macro can be run multiple times, but will
dnl do the checking only once. 
dnl NUT requires HAL version 0.5.8 at least

AC_DEFUN([NUT_CHECK_LIBHAL], 
[
if test -z "${nut_have_libhal_seen}"; then
   nut_have_libhal_seen=yes

   CFLAGS_ORIG="${CFLAGS}"
   CPPFLAGS_ORIG="${CPPFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   nut_have_libhal=yes
   AC_MSG_CHECKING(for libhal version via pkg-config (0.5.8 minimum required))
   HAL_VERSION=`pkg-config --silence-errors  --modversion hal`
   HAL_MIN_VERSION=`pkg-config --silence-errors  --atleast-version=0.5.8 hal`
   if (test "$?" != "0")
   then     
	   AC_MSG_RESULT(${HAL_VERSION} found)
	   nut_have_libhal=no
   else
	   AC_MSG_RESULT(${HAL_VERSION} found)
   fi
   
   dnl also get cflags from glib-2.0 to workaround a bug in dbus-glib
   AC_MSG_CHECKING(for libhal cflags via pkg-config)
     CFLAGS=`pkg-config --silence-errors --cflags hal dbus-glib-1`
   if (test "$?" != "0")
   then
     AC_MSG_RESULT(not found)
     nut_have_libhal=no
   else
     AC_MSG_RESULT(${CFLAGS})
   fi

   dnl also get libs from glib-2.0 to workaround a bug in dbus-glib
   AC_MSG_CHECKING(for libhal ldflags via pkg-config)
   LDFLAGS=`pkg-config --silence-errors --libs hal dbus-glib-1`
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

   dnl Determine installation paths for callout and .fdi
   dnl As per HAL spec, §5 Callouts and §2 Device Information Files
   dnl - addon install path: $libdir/hal
   AC_MSG_CHECKING(for libhal Callouts path)
   if (test -d "/usr/lib/hal" || test -d "/usr/lib64/hal")
   then
     # For Debian
     HAL_CALLOUTS_PATH="${libdir}/hal"
	   AC_MSG_RESULT(${HAL_CALLOUTS_PATH})
   else # For RedHat
     if (test -d "/usr/libexec")
     then
       HAL_CALLOUTS_PATH="${libexecdir}"
       AC_MSG_RESULT(${HAL_CALLOUTS_PATH})
     else
       # FIXME
       HAL_CALLOUTS_PATH=""
	     AC_MSG_RESULT(not found)
     fi
   fi

   dnl - fdi install path: $datarootdir/hal/fdi/information/20thirdparty
   AC_MSG_CHECKING(for libhal Device Information path)
   if (test -d "/usr/share/hal/fdi/information/20thirdparty")
   then
     # seems supported everywhere
     HAL_FDI_PATH="${datarootdir}/hal/fdi/information/20thirdparty"
	   AC_MSG_RESULT(${HAL_FDI_PATH})
   else
     # FIXME
     HAL_FDI_PATH=""
	   AC_MSG_RESULT(not found)
   fi

   if test "${nut_have_libhal}" != "yes"; then
      dnl try again

      CFLAGS="-DDBUS_API_SUBJECT_TO_CHANGE -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include"
      CPPFLAGS="${CFLAGS}"
      LDFLAGS="-lhal -ldbus-1 -lpthread"
      
      AC_CHECK_HEADER(libhal.h, , [nut_have_libhal=no])
      AC_CHECK_LIB(hal, libhal_device_new_changeset, [nut_have_libhal=yes], 
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

