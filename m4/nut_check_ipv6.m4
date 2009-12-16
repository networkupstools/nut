dnl Check for various features required for IPv6 support. Define a
dnl preprocessor symbol for each individual feature (HAVE_GETADDRINFO,
dnl HAVE_FREEADDRINFO, HAVE_STRUCT_ADDRINFO, HAVE_SOCKADDR_STORAGE,
dnl SOCKADDR_IN6, IN6_ADDR). Also set the shell variable nut_have_ipv6=yes 
dnl if all the required features are present. Set nut_have_ipv6=no otherwise.

AC_DEFUN([NUT_CHECK_IPV6], 
[
if test -z "${nut_check_ipv6_seen}"; then
	nut_check_ipv6_seen=yes

	AC_CHECK_FUNCS([getaddrinfo freeaddrinfo], [nut_have_ipv6=yes], [nut_have_ipv6=no])
   
	AC_CHECK_TYPES([struct addrinfo, 
		struct sockaddr_storage, 
		struct sockaddr_in6,
		struct in6_addr],
		[], [nut_have_ipv6=no], [#include <netdb.h>])

	if test "${nut_with_ipv6}" = "yes"; then
		AC_DEFINE(HAVE_IPV6, 1, [Define to enable IPv6 support])
	fi
fi
])
