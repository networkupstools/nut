dnl This code was lifted and adapted for NUT from cURL project:
dnl   https://github.com/curl/curl/blob/83245d9ff352b742753cff1216781ff041e4e987/acinclude.m4#L172
dnl   https://github.com/curl/curl/blob/83245d9ff352b742753cff1216781ff041e4e987/acinclude.m4#L207
dnl   https://github.com/curl/curl/blob/83245d9ff352b742753cff1216781ff041e4e987/acinclude.m4#L238
dnl   https://github.com/curl/curl/blob/83245d9ff352b742753cff1216781ff041e4e987/acinclude.m4#L275
dnl   https://github.com/curl/curl/blob/83245d9ff352b742753cff1216781ff041e4e987/acinclude.m4#L312

dnl NUT_CHECK_HEADER_WINDOWS
dnl -------------------------------------------------
dnl Check for compilable and valid windows.h header

AC_DEFUN([NUT_CHECK_HEADER_WINDOWS], [
  AC_CACHE_CHECK([for windows.h], [nut_cv_header_windows_h], [
    AC_LANG_PUSH([C])
    TESTPROG_H='
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
'
    TESTPROG_C='
#if defined(__CYGWIN__) || defined(__CEGCC__)
        HAVE_WINDOWS_H shall not be defined.
#else
        int dummy=2*WINVER;
#endif
'
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([$TESTPROG_H], [$TESTPROG_C])
    ],[
      nut_cv_header_windows_h="yes"
    ],[
      dnl Try extending default search path as relevant for e.g. MSYS2
      dnl (though there this should be compiler built-in default):
      SAVED_CFLAGS="$CFLAGS"
      CFLAGS="-I/usr/include/w32api $CFLAGS"
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([$TESTPROG_H], [$TESTPROG_C])
      ],[
        nut_cv_header_windows_h="yes"
      ],[
        nut_cv_header_windows_h="no"
        CFLAGS="$SAVED_CFLAGS"
      ])
    ])
    AC_LANG_POP([C])
  ])
  case "$nut_cv_header_windows_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WINDOWS_H, 1,
        [Define to 1 if you have the windows.h header file.])
      ;;
  esac
  AM_CONDITIONAL(HAVE_WINDOWS_H, test "x$nut_cv_header_windows_h" = xyes)
])


dnl NUT_CHECK_NATIVE_WINDOWS
dnl -------------------------------------------------
dnl Check if building a native Windows target

AC_DEFUN([NUT_CHECK_NATIVE_WINDOWS], [
  AC_REQUIRE([NUT_CHECK_HEADER_WINDOWS])dnl
  AC_CACHE_CHECK([whether build target is a native Windows one], [nut_cv_native_windows], [
    if test "$nut_cv_header_windows_h" = "no"; then
      nut_cv_native_windows="no"
    else
      AC_LANG_PUSH([C])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
        ]],[[
#if defined(__MINGW32__) || defined(__MINGW32CE__) || \
   (defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64)))
          int dummy=1;
#else
          Not a native Windows build target.
#endif
        ]])
      ],[
        nut_cv_native_windows="yes"
      ],[
        nut_cv_native_windows="no"
      ])
      AC_LANG_POP([C])
    fi
  ])
  AM_CONDITIONAL(DOING_NATIVE_WINDOWS, test "x$nut_cv_native_windows" = xyes)
])


dnl NUT_CHECK_HEADER_WINSOCK
dnl -------------------------------------------------
dnl Check for compilable and valid winsock.h header

AC_DEFUN([NUT_CHECK_HEADER_WINSOCK], [
  AC_REQUIRE([NUT_CHECK_HEADER_WINDOWS])dnl
  AC_CACHE_CHECK([for winsock.h], [nut_cv_header_winsock_h], [
    AC_LANG_PUSH([C])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock.h>
      ]],[[
#if defined(__CYGWIN__) || defined(__CEGCC__)
        HAVE_WINSOCK_H shall not be defined.
#else
        int dummy=WSACleanup();
#endif
      ]])
    ],[
      nut_cv_header_winsock_h="yes"
    ],[
      nut_cv_header_winsock_h="no"
    ])
    AC_LANG_POP([C])
  ])
  case "$nut_cv_header_winsock_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WINSOCK_H, 1,
        [Define to 1 if you have the winsock.h header file.])
      ;;
  esac
  AM_CONDITIONAL(HAVE_WINSOCK_H, test "x$nut_cv_header_winsock_h" = xyes)
])


dnl NUT_CHECK_HEADER_WINSOCK2
dnl -------------------------------------------------
dnl Check for compilable and valid winsock2.h header

AC_DEFUN([NUT_CHECK_HEADER_WINSOCK2], [
  AC_REQUIRE([NUT_CHECK_HEADER_WINDOWS])dnl
  AC_CACHE_CHECK([for winsock2.h], [nut_cv_header_winsock2_h], [
    AC_LANG_PUSH([C])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
      ]],[[
#if defined(__CYGWIN__) || defined(__CEGCC__) || defined(__MINGW32CE__)
        HAVE_WINSOCK2_H shall not be defined.
#else
        int dummy=2*IPPROTO_ESP;
#endif
      ]])
    ],[
      nut_cv_header_winsock2_h="yes"
    ],[
      nut_cv_header_winsock2_h="no"
    ])
    AC_LANG_POP([C])
  ])
  case "$nut_cv_header_winsock2_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WINSOCK2_H, 1,
        [Define to 1 if you have the winsock2.h header file.])
      ;;
  esac
  AM_CONDITIONAL(HAVE_WINSOCK2_H, test "x$nut_cv_header_winsock2_h" = xyes)
])


dnl NUT_CHECK_HEADER_WS2TCPIP
dnl -------------------------------------------------
dnl Check for compilable and valid ws2tcpip.h header

AC_DEFUN([NUT_CHECK_HEADER_WS2TCPIP], [
  AC_REQUIRE([NUT_CHECK_HEADER_WINSOCK2])dnl
  AC_CACHE_CHECK([for ws2tcpip.h], [nut_cv_header_ws2tcpip_h], [
    AC_LANG_PUSH([C])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
      ]],[[
#if defined(__CYGWIN__) || defined(__CEGCC__) || defined(__MINGW32CE__)
        HAVE_WS2TCPIP_H shall not be defined.
#else
        int dummy=2*IP_PKTINFO;
#endif
      ]])
    ],[
      nut_cv_header_ws2tcpip_h="yes"
    ],[
      nut_cv_header_ws2tcpip_h="no"
    ])
    AC_LANG_POP([C])
  ])
  case "$nut_cv_header_ws2tcpip_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WS2TCPIP_H, 1,
        [Define to 1 if you have the ws2tcpip.h header file.])
      ;;
  esac
  AM_CONDITIONAL(HAVE_WS2TCPIP_H, test "x$nut_cv_header_ws2tcpip_h" = xyes)
])

dnl NUT_CHECK_HEADER_IPHLPAPI
dnl -------------------------------------------------
dnl Check for compilable and valid iphlpapi.h header

AC_DEFUN([NUT_CHECK_HEADER_IPHLPAPI], [
  AC_REQUIRE([NUT_CHECK_HEADER_WINSOCK2])dnl
  AC_CACHE_CHECK([for iphlpapi.h], [nut_cv_header_iphlpapi_h], [
    AC_LANG_PUSH([C])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
      ]],[[
        PIP_ADAPTER_ADDRESSES pAddresses = NULL;
        IP_ADAPTER_PREFIX *pPrefix = NULL;
        PIP_ADAPTER_INFO pAdapter = NULL;
      ]])
    ],[
      nut_cv_header_iphlpapi_h="yes"
    ],[
      nut_cv_header_iphlpapi_h="no"
    ])
    AC_LANG_POP([C])
  ])
  AS_CASE([$nut_cv_header_iphlpapi_h],
    [yes], [
      AC_DEFINE_UNQUOTED(HAVE_IPHLPAPI_H, 1,
        [Define to 1 if you have the iphlpapi.h header file.])
      ]
  )
  AM_CONDITIONAL(HAVE_IPHLPAPI_H, test "x$nut_cv_header_iphlpapi_h" = xyes)
])
