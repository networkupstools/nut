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
   AC_REQUIRE([NUT_CHECK_HEADER_WS2TCPIP])dnl
   dnl # TOTHINK: Are <sys/socket.h> and <ws2tcpip.h> mutually exclusive?
   dnl # Some projects test for both, based on ifdef results; we declare
   dnl # those in NUT_PREREQ_SYS_H_SOCKET however as either one or another
   HEADERS_SOCKLEN_T='
#undef inline
#ifdef HAVE_WINDOWS_H
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# if (!defined(_WIN32_WINNT)) || (_WIN32_WINNT < 0x0501)
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif
# include <windows.h>
# ifdef HAVE_WINSOCK2_H
#  include <winsock2.h>
#  ifdef HAVE_WS2TCPIP_H
#   include <ws2tcpip.h>
#  endif
# endif
# define GNICALLCONV WSAAPI
# define GNICALLLINK WINSOCK_API_LINKAGE
#else
# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif
# ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
# endif
# ifdef HAVE_NETDB_H
#  include <netdb.h>
# endif
# define GNICALLCONV
# define GNICALLLINK
#endif
'
   AC_CHECK_TYPE([socklen_t], ,[
      AC_MSG_CHECKING([for socklen_t equivalent])
      AC_CACHE_VAL([nut_cv_socklen_t_equiv],
        [# Systems have either "struct sockaddr *" or
         # "void *" as the second argument to getpeername
         AC_LANG_PUSH([C])
         nut_cv_socklen_t_equiv=
         for arg1 in "int" "SOCKET"; do
          for arg2 in "struct sockaddr" void; do
            for arg3 in int size_t unsigned long "unsigned long" "unsigned int" "long int" "unsigned long int"; do
               AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${HEADERS_SOCKLEN_T}
                  GNICALLLINK int GNICALLCONV getpeername ($arg1, $arg2 *, $arg3 *);
                   ],[
                  $arg3 len;
                  getpeername(0, 0, &len);
                   ])],
               [
                  nut_cv_socklen_t_equiv="$arg3"
                  break
               ])
             if test x"$nut_cv_socklen_t_equiv" != "x" ; then break ; fi
            done
            if test x"$nut_cv_socklen_t_equiv" != "x" ; then break ; fi
          done
          if test x"$nut_cv_socklen_t_equiv" != "x" ; then break ; fi
         done
         AC_LANG_POP([C])

         if test "x$nut_cv_socklen_t_equiv" = x; then
            AC_MSG_ERROR([Cannot find a type to use in place of socklen_t])
         fi
      ])
      AC_MSG_RESULT($nut_cv_socklen_t_equiv)
      AC_DEFINE_UNQUOTED(socklen_t, $nut_cv_socklen_t_equiv,
			[type to use in place of socklen_t if not defined])],
      [${HEADERS_SOCKLEN_T}])
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
