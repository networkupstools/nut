dnl Check for LIBXML2 compiler flags. On success, set nut_have_libxml2="yes"
dnl and set LIBXML2_CFLAGS and LIBXML2_LIBS. On failure, set
dnl nut_have_libxml2="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBXML2],
[
if test -z "${nut_have_libxml2_seen}"; then
	nut_have_libxml2_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the xml2 library (if any) is installed
		 dnl FIXME : Support detection of cflags/ldflags below by legacy
		 dnl discovery if pkgconfig is not there
		 AC_MSG_CHECKING(for libxml2 version via pkg-config)
		 XML2_VERSION="`$PKG_CONFIG --silence-errors --modversion xml2 2>/dev/null`"
		 if test "$?" != "0" -o -z "${XML2_VERSION}"; then
		    XML2_VERSION="none"
		 fi
		 AC_MSG_RESULT(${XML2_VERSION} found)
		],
		[XML2_VERSION="none"
		 AC_MSG_NOTICE([can not check libxml2 settings via pkg-config])
		]
	)

	AC_MSG_CHECKING(for libxml2 cflags)
	AC_ARG_WITH(libxml2-includes,
		AS_HELP_STRING([@<:@--with-libxml2-includes=CFLAGS@:>@], [include flags for the libxml2 library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libxml2-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[CFLAGS="`$PKG_CONFIG --silence-errors --cflags xml2 2>/dev/null`" || CFLAGS="-I/usr/include/xml2 -I/usr/local/include/xml2"],
			[CFLAGS="`xml2-config --cflags`" || CFLAGS="-I/usr/include/xml2 -I/usr/local/include/xml2"]
		)]
	)
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libxml2 ldflags)
	AC_ARG_WITH(xml2-libs,
		AS_HELP_STRING([@<:@--with-libxml2-libs=LIBS@:>@], [linker flags for the libxml2 library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libxml2-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[LIBS="`$PKG_CONFIG --silence-errors --libs xml2 2>/dev/null`" || LIBS="-lxml2"],
			[LIBS="`xml2-config --libs`" || LIBS="-lxml2"]
		)]
	)
	AC_MSG_RESULT([${LIBS}])

	dnl check if libxml2 is usable
	AC_CHECK_HEADERS(libxml/encoding.h libxml/parser.h,
		[nut_have_libxml2=yes],
		[nut_have_libxml2=no],
		[AC_INCLUDES_DEFAULT])

	AS_IF([test "${nut_have_libxml2}" = "yes"],
		[AC_CHECK_FUNCS(xmlInitParser xmlCleanupCharEncodingHandlers xmlCleanupParser,
			[],
			[nut_have_libxml2=no])
		]
	)

	if test "${nut_have_libxml2}" = "yes"; then
		LIBXML2_CFLAGS="${CFLAGS}"
		LIBXML2_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
