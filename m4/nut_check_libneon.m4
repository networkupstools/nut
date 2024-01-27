dnl Check for LIBNEON compiler flags. On success, set nut_have_neon="yes"
dnl and set LIBNEON_CFLAGS and LIBNEON_LIBS. On failure, set
dnl nut_have_neon="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBNEON],
[
if test -z "${nut_have_neon_seen}"; then
	nut_have_neon_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the neon library (if any) is installed
		 dnl FIXME : Support detection of cflags/ldflags below by legacy
		 dnl discovery if pkgconfig is not there
		 AC_MSG_CHECKING(for libneon version via pkg-config (0.25.0 minimum required))
		 NEON_VERSION="`$PKG_CONFIG --silence-errors --modversion neon 2>/dev/null`"
		 if test "$?" != "0" -o -z "${NEON_VERSION}"; then
		    NEON_VERSION="none"
		 fi
		 AC_MSG_RESULT(${NEON_VERSION} found)
		],
		[NEON_VERSION="none"
		 AC_MSG_NOTICE([can not check libneon settings via pkg-config])
		]
	)

	AC_MSG_CHECKING(for libneon cflags)
	AC_ARG_WITH(neon-includes,
		AS_HELP_STRING([@<:@--with-neon-includes=CFLAGS@:>@], [include flags for the neon library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-neon-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[CFLAGS="`$PKG_CONFIG --silence-errors --cflags neon 2>/dev/null`" || CFLAGS="-I/usr/include/neon -I/usr/local/include/neon"],
			[CFLAGS="-I/usr/include/neon -I/usr/local/include/neon"]
		)]
	)
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libneon ldflags)
	AC_ARG_WITH(neon-libs,
		AS_HELP_STRING([@<:@--with-neon-libs=LIBS@:>@], [linker flags for the neon library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-neon-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[LIBS="`$PKG_CONFIG --silence-errors --libs neon 2>/dev/null`" || LIBS="-lneon"],
			[LIBS="-lneon"]
		)]
	)
	AC_MSG_RESULT([${LIBS}])

	dnl check if neon is usable
	AC_CHECK_HEADERS(ne_xmlreq.h, [nut_have_neon=yes], [nut_have_neon=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(ne_xml_dispatch_request, [], [nut_have_neon=no])

	if test "${nut_have_neon}" = "yes"; then
		dnl Check for connect timeout support in library (optional)
		AC_CHECK_FUNCS(ne_set_connect_timeout ne_sock_connect_timeout)
		LIBNEON_CFLAGS="${CFLAGS}"
		LIBNEON_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
