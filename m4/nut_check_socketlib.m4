dnl Copyright (C) 2008-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl NUT_CHECK_SOCKETLIB
dnl Determines the library to use for socket functions.
dnl Sets and AC_SUBSTs NETLIBS.

dnl This code comes from gnulib trunk (4 Nov. 2010).

AC_DEFUN([NUT_CHECK_SOCKETLIB],
[
  NUT_PREREQ_SYS_H_SOCKET dnl for HAVE_WINSOCK2_H
  NETLIBS=
  if test $HAVE_WINSOCK2_H = 1; then
    dnl Native Windows API (not Cygwin).
    AC_CACHE_CHECK([if we need to call WSAStartup in winsock2.h and -lws2_32],
                   [nut_cv_func_wsastartup], [
      nut_save_LIBS="$LIBS"
      LIBS="$LIBS -lws2_32"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
#endif]], [[
          WORD wVersionRequested = MAKEWORD(1, 1);
          WSADATA wsaData;
          int err = WSAStartup(wVersionRequested, &wsaData);
          WSACleanup ();]])],
        nut_cv_func_wsastartup=yes, nut_cv_func_wsastartup=no)
      LIBS="$nut_save_LIBS"
    ])
    if test "$nut_cv_func_wsastartup" = "yes"; then
      AC_DEFINE([WINDOWS_SOCKETS], [1], [Define if WSAStartup is needed.])
      NETLIBS='-lws2_32'
    fi
  fi
  AC_SUBST([NETLIBS])

  AM_CONDITIONAL([HAVE_WINDOWS_SOCKETS], [test  "$nut_cv_func_wsastartup" = "yes"])
])
