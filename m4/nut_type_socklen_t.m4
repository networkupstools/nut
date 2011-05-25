dnl Check for socklen_t: historically on BSD it is an int, and in
dnl POSIX 1g it is a type of its own, but some platforms use different
dnl types for the argument to getsockopt, getpeername, etc.:
dnl HP-UX 10.20, IRIX 6.5, Interix 3.5, BeOS.
dnl So we have to test to find something that will work.

dnl Copyright (C) 2005, 2006, 2007, 2009, 2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Albert Chin, Windows fixes from Simon Josefsson.

dnl On mingw32, socklen_t is in ws2tcpip.h ('int'), so we try to find
dnl it there first.  That file is included by gnulib sys_socket.in.h, which
dnl all users of this module should include.  Cygwin must not include
dnl ws2tcpip.h.

dnl This code gets around.  This instance came from gnulib trunk (21 Oct. 2010).

AC_DEFUN([NUT_TYPE_SOCKLEN_T],
  [
   NUT_PREREQ_SYS_H_SOCKET
   AC_CHECK_TYPE([socklen_t], ,
     [AC_MSG_CHECKING([for socklen_t equivalent])
      AC_CACHE_VAL([nut_cv_socklen_t_equiv],
        [# Systems have either "struct sockaddr *" or
         # "void *" as the second argument to getpeername
         nut_cv_socklen_t_equiv=
         for arg2 in "struct sockaddr" void; do
           for t in int size_t "unsigned int" "long int" "unsigned long int"; do
             AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
                 [[#include <sys/types.h>
                   #ifdef HAVE_SYS_SOCKET_H
                   #include <sys/socket.h>
                   #endif
                   #ifdef HAVE_WS2TCPIP_H
                   #include <ws2tcpip.h>
                   #endif

                   int getpeername (int, $arg2 *, $t *);]],
                 [[$t len;
                  getpeername (0, 0, &len);]])],
               [nut_cv_socklen_t_equiv="$t"])
             test "$nut_cv_socklen_t_equiv" != "" && break
           done
           test "$nut_cv_socklen_t_equiv" != "" && break
         done
      ])
      if test "$nut_cv_socklen_t_equiv" = ""; then
        AC_MSG_ERROR([Cannot find a type to use in place of socklen_t])
      fi
      AC_MSG_RESULT([$nut_cv_socklen_t_equiv])
      AC_DEFINE_UNQUOTED([socklen_t], [$nut_cv_socklen_t_equiv],
        [type to use in place of socklen_t if not defined])],
     [#include <sys/types.h>
      #ifdef HAVE_SYS_SOCKET_H
      #include <sys/socket.h>
      #endif
      #ifdef HAVE_WS2TCPIP_H
      #include <ws2tcpip.h>
      #endif])
])

AC_DEFUN([NUT_PREREQ_SYS_H_SOCKET],
[
  dnl Check prerequisites of the <sys/socket.h> replacement.
  AC_CHECK_HEADERS_ONCE([sys/socket.h])
  if test $ac_cv_header_sys_socket_h = yes; then
    HAVE_SYS_SOCKET_H=1
    HAVE_WS2TCPIP_H=0
    HAVE_WINSOCK2_H=0
  else
    HAVE_SYS_SOCKET_H=0
    dnl We cannot use AC_CHECK_HEADERS_ONCE here, because that would make
    dnl the check for those headers unconditional; yet cygwin reports
    dnl that the headers are present but cannot be compiled (since on
    dnl cygwin, all socket information should come from sys/socket.h).
    AC_CHECK_HEADERS([ws2tcpip.h])
    if test $ac_cv_header_ws2tcpip_h = yes; then
      HAVE_WS2TCPIP_H=1
    else
      HAVE_WS2TCPIP_H=0
    fi
    AC_CHECK_HEADERS([winsock2.h])
    if test "$ac_cv_header_winsock2_h" = yes; then
      HAVE_WINSOCK2_H=1
    else
      HAVE_WINSOCK2_H=0
    fi
  fi
  AC_SUBST([HAVE_WINSOCK2_H])
  AC_SUBST([HAVE_SYS_SOCKET_H])
  AC_SUBST([HAVE_WS2TCPIP_H])
])
