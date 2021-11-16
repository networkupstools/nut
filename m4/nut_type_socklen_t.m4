dnl Check for socklen_t: historically on BSD it is an int, and in
dnl POSIX 1g it is a type of its own, but some platforms use different
dnl types for the argument to getsockopt, getpeername, etc.  So we
dnl have to test to find something that will work.

dnl This code gets around.  This instance came from rsync 2.5.6.

AC_DEFUN([NUT_TYPE_SOCKLEN_T],
[
   AC_REQUIRE([NUT_CHECK_HEADER_WS2TCPIP])dnl
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
      [
         # Systems have either "struct sockaddr *" or
         # "void *" as the second argument to getpeername
         nut_cv_socklen_t_equiv=
         for arg1 in "int" "SOCKET"; do
          for arg2 in "struct sockaddr" void; do
            for arg3 in int size_t unsigned long "unsigned long"; do
               AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${HEADERS_SOCKLEN_T}
                  GNICALLLINK int GNICALLCONV getpeername ($arg1, $arg2 *, $arg3 *);
                   ],[
                  $arg3 len;
                  getpeername(0,0,&len);
                   ])],
               [
                  nut_cv_socklen_t_equiv="$arg3"
                  break
               ])
            done
          done
         done

         if test "x$nut_cv_socklen_t_equiv" = x; then
            AC_MSG_ERROR([Cannot find a type to use in place of socklen_t])
         fi
      ])
      AC_MSG_RESULT($nut_cv_socklen_t_equiv)
      AC_DEFINE_UNQUOTED(socklen_t, $nut_cv_socklen_t_equiv,
			[type to use in place of socklen_t if not defined])],
      [${HEADERS_SOCKLEN_T}])
])
