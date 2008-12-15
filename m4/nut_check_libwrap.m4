dnl Check for LIBWRAP compiler flags. On success, set nut_have_libwrap="yes"
dnl and set LIBWRAP_CFLAGS and LIBWRAP_LDFLAGS. On failure, set
dnl nut_have_libwrap="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBWRAP], 
[
if test -z "${nut_have_libwrap_seen}"; then
   nut_have_libwrap_seen=yes

   AC_MSG_CHECKING(for tcp-wrappers library availability)

   dnl save CFLAGS and LDFLAGS
   CFLAGS_ORIG="${CFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   CFLAGS=""
   LDFLAGS="-lwrap"

   AC_CHECK_LIB(wrap, request_init, nut_have_libwrap=yes, nut_have_libwrap=no)

   if test "${nut_have_libwrap}" = "yes"; then
	LIBWRAP_CFLAGS="${CFLAGS}"
	LIBWRAP_LDFLAGS="${LDFLAGS}"
   fi

   dnl restore original CFLAGS and LDFLAGS
   CFLAGS="${CFLAGS_ORIG}"
   LDFLAGS="${LDFLAGS_ORIG}"

   AC_MSG_RESULT([${nut_have_libwrap}])

fi
])
