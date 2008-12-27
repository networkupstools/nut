dnl Check for LIBWRAP compiler flags. On success, set nut_have_libwrap="yes"
dnl and set LIBWRAP_CFLAGS and LIBWRAP_LDFLAGS. On failure, set
dnl nut_have_libwrap="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBWRAP], 
[
if test -z "${nut_have_libwrap_seen}"; then
   nut_have_libwrap_seen=yes

   nut_have_libwrap=yes

   dnl save LIBS
   LIBS_ORIG="${LIBS}"

   LIBS=""

   AC_CHECK_HEADER(tcpd.h, [], nut_have_libwrap=no)
   AC_CHECK_LIB(wrap, request_init, [], nut_have_libwrap=no)
   AC_SEARCH_LIBS(yp_get_default_domain, nsl, [], nut_have_libwrap=no)

   if test "${nut_have_libwrap}" = "yes"; then
	LIBWRAP_CFLAGS=""
	LIBWRAP_LDFLAGS="${LIBS}"
   fi

   dnl restore original LIBS
   LIBS="${LIBS_ORIG}"
fi
])
