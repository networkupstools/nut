dnl Check for various features required for IPv6 support. Define a
dnl preprocessor symbol for each individual feature (HAVE_GETADDRINFO,
dnl HAVE_FREEADDRINFO, HAVE_STRUCT_ADDRINFO, HAVE_SOCKADDR_STORAGE,
dnl SOCKADDR_IN6, IN6_ADDR, HAVE_IN6_IS_ADDR_V4MAPPED, 
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

dnl AC_CHECK_MEMBERS([struct in6_addr.s6_addr32],
dnl               [:],
dnl               [nut_have_ipv6=no],
dnl		  [#include <netdb.h>])

dnl AC_MSG_CHECKING([for AI_ADDRCONFIG])
dnl AC_COMPILE_IFELSE(
dnl    [AC_LANG_PROGRAM(
dnl	   [[#include <netdb.h>]],
dnl	   [[int flag = AI_ADDRCONFIG]]
dnl     )], 
dnl    [AC_DEFINE(HAVE_AI_ADDRCONFIG, 1, [Define if `addrinfo' structure allows AI_ADDRCONFIG flag])
dnl     AC_MSG_RESULT(yes)],
dnl    [AC_MSG_RESULT(no)
dnl     nut_have_ipv6=no]
dnl)

dnl AC_MSG_CHECKING([for IN6_IS_ADDR_V4MAPPED])
dnl AC_LINK_IFELSE(
dnl    [AC_LANG_PROGRAM(
dnl	   [[#include <netinet/in.h>]],
dnl	   [[
dnl	    struct in6_addr *i6 = (struct in6_addr *)0;
dnl	    return IN6_IS_ADDR_V4MAPPED(i6);
dnl	   ]]
dnl     )], 
dnl    [AC_DEFINE(HAVE_IN6_IS_ADDR_V4MAPPED, 1, [Define if IN6_IS_ADDR_V4MAPPED is available])
dnl     AC_MSG_RESULT(yes)],
dnl    [AC_MSG_RESULT(no)
dnl     nut_have_ipv6=no]
dnl)

fi
])

