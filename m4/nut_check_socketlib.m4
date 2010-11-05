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
  else
    dnl Unix API.
    dnl Solaris has most socket functions in libsocket and libnsl.
    dnl Haiku has most socket functions in libnetwork.
    dnl BeOS has most socket functions in libnet.
    AC_CACHE_CHECK([for library containing setsockopt], [nut_cv_lib_socket], [
      nut_cv_lib_socket=
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
        [],
        [nut_save_LIBS="$LIBS"
         LIBS="$nut_save_LIBS -lsocket"
         AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
           [nut_cv_lib_socket="-lsocket -lnsl"])
         if test -z "$nut_cv_lib_socket"; then
           LIBS="$nut_save_LIBS -lnetwork"
           AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
             [nut_cv_lib_socket="-lnetwork"])
           if test -z "$nut_cv_lib_socket"; then
             LIBS="$nut_save_LIBS -lnet"
             AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
               [nut_cv_lib_socket="-lnet"])
           fi
         fi
         LIBS="$nut_save_LIBS"
        ])
      if test -z "$nut_cv_lib_socket"; then
        nut_cv_lib_socket="none needed"
      fi
    ])
    if test "$nut_cv_lib_socket" != "none needed"; then
      NETLIBS="$nut_cv_lib_socket"
    fi
  fi
  AC_SUBST([NETLIBS])
])
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
  else
    dnl Unix API.
    dnl Solaris has most socket functions in libsocket and libnsl.
    dnl Haiku has most socket functions in libnetwork.
    dnl BeOS has most socket functions in libnet.
    AC_CACHE_CHECK([for library containing setsockopt], [nut_cv_lib_socket], [
      nut_cv_lib_socket=
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
        [],
        [nut_save_LIBS="$LIBS"
         LIBS="$nut_save_LIBS -lsocket"
         AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
           [nut_cv_lib_socket="-lsocket -lnsl"])
         if test -z "$nut_cv_lib_socket"; then
           LIBS="$nut_save_LIBS -lnetwork"
           AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
             [nut_cv_lib_socket="-lnetwork"])
           if test -z "$nut_cv_lib_socket"; then
             LIBS="$nut_save_LIBS -lnet"
             AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern
#ifdef __cplusplus
"C"
#endif
char setsockopt();]], [[setsockopt();]])],
               [nut_cv_lib_socket="-lnet"])
           fi
         fi
         LIBS="$nut_save_LIBS"
        ])
      if test -z "$nut_cv_lib_socket"; then
        nut_cv_lib_socket="none needed"
      fi
    ])
    if test "$nut_cv_lib_socket" != "none needed"; then
      NETLIBS="$nut_cv_lib_socket"
    fi
  fi
  AC_SUBST([NETLIBS])
])
