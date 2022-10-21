dnl Check for LIBWRAP compiler flags. On success, set nut_have_libwrap="yes"
dnl and set LIBWRAP_CFLAGS and LIBWRAP_LIBS. On failure, set
dnl nut_have_libwrap="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBWRAP], 
[
if test -z "${nut_have_libwrap_seen}"; then
	nut_have_libwrap_seen=yes

	dnl save LIBS
	LIBS_ORIG="${LIBS}"
	LIBS=""

	AC_CHECK_HEADERS(tcpd.h, [nut_have_libwrap=yes], [nut_have_libwrap=no], [AC_INCLUDES_DEFAULT])
	AC_SEARCH_LIBS(yp_get_default_domain, nsl, [], [nut_have_libwrap=no])

	dnl The line below does not work on Solaris 10.
	dnl AC_SEARCH_LIBS(request_init, wrap, [], [nut_have_libwrap=no])
	AC_MSG_CHECKING(for library containing request_init)
	AC_LANG_PUSH([C])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <tcpd.h>
int allow_severity = 0, deny_severity = 0;
	]], [[ request_init(0); ]])], [
		AC_MSG_RESULT(none required)
	], [
		LIBS="${LIBS} -lwrap"
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <tcpd.h>
int allow_severity = 0, deny_severity = 0;
		]], [[ request_init(0); ]])], [
			AC_MSG_RESULT(-lwrap)
		], [
			AC_MSG_RESULT(no)
			nut_have_libwrap=no
		])
	])
	AC_LANG_POP([C])

	if test "${nut_have_libwrap}" = "yes"; then
		AC_DEFINE(HAVE_WRAP, 1, [Define to enable libwrap support])
		LIBWRAP_CFLAGS=""
		LIBWRAP_LIBS="${LIBS}"
	fi

	dnl restore original LIBS
	LIBS="${LIBS_ORIG}"
fi
])
