dnl Check for various features required for IPv6 support. Define a
dnl preprocessor symbol for each individual feature (HAVE_GETADDRINFO,
dnl HAVE_FREEADDRINFO, HAVE_STRUCT_ADDRINFO, HAVE_SOCKADDR_STORAGE,
dnl SOCKADDR_IN6, IN6_ADDR, HAVE_STRUCT_IN6_ADDR_S6_ADDR32, 
dnl HAVE_AI_ADDRCONFIG). Also set the shell variable nut_have_ipv6=yes 
dnl if all the required features are present. Set nut_have_ipv6=no otherwise.

AC_DEFUN([NUT_CHECK_IPV6], 
[
if test -z "${nut_check_ipv6_seen}"; then
   nut_check_ipv6_seen=yes

   nut_have_ipv6=yes

   AC_CHECK_FUNCS([getaddrinfo freeaddrinfo], 
                  [:],
                  [nut_have_ipv6=no])
   
   AC_CHECK_TYPES([struct addrinfo, 
	           struct sockaddr_storage, 
		   struct sockaddr_in6,
		   struct in6_addr],
                  [:],
                  [nut_have_ipv6=no],
		  [#include <netdb.h>])

   AC_CHECK_MEMBERS([struct in6_addr.s6_addr32],
                  [:],
                  [nut_have_ipv6=no],
		  [#include <netdb.h>])

   AC_MSG_CHECKING([for AI_ADDRCONFIG])
   AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
	   [[#include <netdb.h>]],
	   [[int flag = AI_ADDRCONFIG]]
        )], 
       [AC_DEFINE(HAVE_AI_ADDRCONFIG, 1, [Define if `addrinfo' structure allows AI_ADDRCONFIG flag])
        AC_MSG_RESULT(yes)],
       [AC_MSG_RESULT(no)
        nut_have_ipv6=no]
   )

fi
])

