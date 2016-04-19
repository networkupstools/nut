dnl Check for LIBNETSNMP compiler flags. On success, set
dnl nut_have_libnetsnmp="yes" and set LIBNETSNMP_CFLAGS and
dnl LIBNETSNMP_LIBS. On failure, set nut_have_libnetsnmp="no".
dnl This macro can be run multiple times, but will do the checking only
dnl once.

AC_DEFUN([NUT_CHECK_LIBNETSNMP],
[
if test -z "${nut_have_libnetsnmp_seen}"; then
	nut_have_libnetsnmp_seen=yes

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	dnl By default seek in PATH
	NET_SNMP_CONFIG=net-snmp-config
	AC_ARG_WITH(net-snmp-config,
		AS_HELP_STRING([@<:@--with-net-snmp-config=/path/to/net-snmp-config@:>@],
			[path to program that reports Net-SNMP configuration]),
	[
		case "${withval}" in
		"") ;;
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-net-snmp-config - see docs/configure.txt)
			;;
		*)
			NET_SNMP_CONFIG="${withval}"
			;;
		esac
	])

	dnl See which version of the Net-SNMP library (if any) is installed
	AC_MSG_CHECKING(for Net-SNMP version via ${NET_SNMP_CONFIG})
	SNMP_VERSION=`${NET_SNMP_CONFIG} --version 2>/dev/null`
	if test "$?" != "0" -o -z "${SNMP_VERSION}"; then
		SNMP_VERSION="none"
	fi
	AC_MSG_RESULT(${SNMP_VERSION} found)

	AC_MSG_CHECKING(for Net-SNMP cflags)
	AC_ARG_WITH(snmp-includes,
		AS_HELP_STRING([@<:@--with-snmp-includes=CFLAGS@:>@], [include flags for the Net-SNMP library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-snmp-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [CFLAGS="`${NET_SNMP_CONFIG} --base-cflags 2>/dev/null`"])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for Net-SNMP libs)
	AC_ARG_WITH(snmp-libs,
		AS_HELP_STRING([@<:@--with-snmp-libs=LIBS@:>@], [linker flags for the Net-SNMP library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-snmp-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [LIBS="`${NET_SNMP_CONFIG} --libs 2>/dev/null`"])
	AC_MSG_RESULT([${LIBS}])

	dnl Check if the Net-SNMP library is usable
	AC_CHECK_HEADERS(net-snmp/net-snmp-config.h, [nut_have_libnetsnmp=yes], [nut_have_libnetsnmp=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(init_snmp, [], [nut_have_libnetsnmp=no])

	if test "${nut_have_libnetsnmp}" = "yes"; then
		LIBNETSNMP_CFLAGS="${CFLAGS}"
		LIBNETSNMP_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
