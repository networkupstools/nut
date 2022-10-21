dnl This code was lifted and adapted for NUT from cURL project:
dnl   https://github.com/curl/curl/blob/e3657644d695373e9cf9ab9b4f1571afda7fd041/acinclude.m4#L228

dnl NUT_FUNC_GETNAMEINFO_ARGTYPES
dnl -------------------------------------------------
dnl Check the type to be passed to five of the arguments
dnl of getnameinfo function, and define those types in
dnl macros GETNAMEINFO_TYPE_ARG1, GETNAMEINFO_TYPE_ARG2,
dnl GETNAMEINFO_TYPE_ARG46 (4 and 6) and GETNAMEINFO_TYPE_ARG7.
dnl As seen from the loop, these vary a lot between OSes...
dnl Order of attempts in the loop below was updated to first
dnl try and quickly match current X/Open definition at
dnl   https://pubs.opengroup.org/onlinepubs/9699919799/functions/getnameinfo.html
dnl for modern conforming OSes.

AC_DEFUN([NUT_FUNC_GETNAMEINFO_ARGTYPES], [
  AC_REQUIRE([NUT_CHECK_HEADER_WS2TCPIP])dnl
  AC_REQUIRE([NUT_TYPE_SOCKLEN_T])dnl
  AC_CHECK_HEADERS(sys/types.h sys/socket.h netdb.h)
  AC_LANG_PUSH([C])
  AC_CACHE_CHECK([types of arguments for getnameinfo],
    [nut_cv_func_getnameinfo_args], [
    nut_cv_func_getnameinfo_args="unknown"
    for gni_arg1 in 'const struct sockaddr *' 'struct sockaddr *' 'void *'; do
      for gni_arg2 in 'socklen_t' 'size_t' 'int'; do
        for gni_arg46 in 'socklen_t' 'size_t' 'int' 'unsigned int' 'DWORD'; do
          for gni_arg7 in 'int' 'unsigned int'; do
            AC_COMPILE_IFELSE([
              AC_LANG_PROGRAM([
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
#endif
                extern int GNICALLCONV getnameinfo($gni_arg1, $gni_arg2,
                                       char *, $gni_arg46,
                                       char *, $gni_arg46,
                                       $gni_arg7);
              ],[
                $gni_arg2 salen=0;
                $gni_arg46 hostlen=0;
                $gni_arg46 servlen=0;
                $gni_arg7 flags=0;
                int res = getnameinfo(0, salen, 0, hostlen, 0, servlen, flags);
              ])
            ],[
               nut_cv_func_getnameinfo_args="$gni_arg1,$gni_arg2,$gni_arg46,$gni_arg7"
               break 4
            ])
          done
        done
      done
    done
  ])
  AC_LANG_POP([C])
  if test "$nut_cv_func_getnameinfo_args" = "unknown"; then
    AC_MSG_WARN([Cannot find proper types to use for getnameinfo args])
  else
    gni_prev_IFS=$IFS; IFS=','
    set dummy `echo "$nut_cv_func_getnameinfo_args" | sed 's/\*/\*/g'`
    IFS=$gni_prev_IFS
    shift
    AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG1, $[1],
      [Define to the type of arg 1 for getnameinfo.])
    AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG2, $[2],
      [Define to the type of arg 2 for getnameinfo.])
    AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG46, $[3],
      [Define to the type of args 4 and 6 for getnameinfo.])
    AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG7, $[4],
      [Define to the type of arg 7 for getnameinfo.])
  fi
])
