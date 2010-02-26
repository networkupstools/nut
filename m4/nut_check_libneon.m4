dnl Check for LIBNEON compiler flags. On success, set nut_have_neon="yes"
dnl and set LIBNEON_CFLAGS and LIBNEON_LDFLAGS. On failure, set
dnl nut_have_neon="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBNEON],
[
if test -z "${nut_have_neon_seen}"; then
	nut_have_neon_seen=yes

	dnl save CFLAGS and LDFLAGS
	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	dnl See which version of the neon library (if any) is installed
	AC_MSG_CHECKING(for libneon version via pkg-config (0.25.0 minimum required))
	NEON_VERSION=`pkg-config --silence-errors --modversion neon`
	if test "$?" = "0"; then
		AC_MSG_RESULT(${NEON_VERSION} found)
	else
		AC_MSG_RESULT(not found)
	fi

	AC_MSG_CHECKING(for libneon cflags)
	AC_ARG_WITH(neon-includes, [
		AS_HELP_STRING([--with-neon-includes=CFLAGS], [include flags for the neon library])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-neon-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [CFLAGS="`pkg-config --silence-errors --cflags neon`"])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libneon ldflags)
	AC_ARG_WITH(neon-libs, [
		AS_HELP_STRING([--with-neon-libs=LDFLAGS], [linker flags for the neon library])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-neon-libs - see docs/configure.txt)
			;;
		*)
			LDFLAGS="${withval}"
			;;
		esac
	], [LDFLAGS="`pkg-config --silence-errors --libs neon`"])
	AC_MSG_RESULT([${LDFLAGS}])

	dnl check if neon is usable
	AC_CHECK_HEADERS(ne_xmlreq.h, [nut_have_neon=yes], [nut_have_neon=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(ne_xml_dispatch_request, [], [nut_have_neon=no])

	if test "${nut_have_neon}" = "yes"; then
		dnl Check for connect timeout support in library (optional)
		AC_CHECK_FUNCS(ne_set_connect_timeout ne_sock_connect_timeout)
		LIBNEON_CFLAGS="${CFLAGS}"
		LIBNEON_LDFLAGS="${LDFLAGS}"
	fi

	dnl restore original CFLAGS and LDFLAGS
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
fi
])
