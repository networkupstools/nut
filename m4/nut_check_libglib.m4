dnl Check for LIBGLIB compiler flags. On success, set nut_have_libglib="yes"
dnl and set LIBGLIB_CFLAGS and LIBGLIB_LIBS. On failure, set
dnl nut_have_libglib="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBGLIB],
[
if test -z "${nut_have_libglib_seen}"; then
	nut_have_libglib_seen=yes
	AC_REQUIRE([NUT_CHECK_PKGCONFIG])

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LIBS=""
	depCFLAGS=""
	depCFLAGS_SOURCE=""
	depLIBS=""
	depLIBS_SOURCE=""

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the glib/gio library (if any) is installed
		 AC_MSG_CHECKING(for gio-2.0 version via pkg-config (2.26.0 minimum required))
		 LIBGLIB_VERSION="`$PKG_CONFIG --silence-errors --modversion gio-2.0 2>/dev/null`"
		 if test "$?" != "0" -o -z "${LIBGLIB_VERSION}"; then
		    LIBGLIB_VERSION="none"
		 fi
		 AC_MSG_RESULT(${LIBGLIB_VERSION} found)
		], [AC_MSG_NOTICE([can not check gio-2.0 settings via pkg-config])]
	)

	AC_MSG_CHECKING(for gio-2.0 cflags)
	NUT_ARG_WITH_LIBOPTS_INCLUDES([gio], [auto])
	AS_CASE([${nut_with_gio_includes}],
		[auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
				[   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags gio-2.0 2>/dev/null`" \
				      && depCFLAGS_SOURCE="pkg-config" ; } \
				 || { depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include" \
				      && depCFLAGS_SOURCE="default" ; }],
				[depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include"
				 depCFLAGS_SOURCE="default"]
			)],
				[depCFLAGS="${nut_with_gio_includes}"
				 depCFLAGS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

	AC_MSG_CHECKING(for gio-2.0 ldflags)
	NUT_ARG_WITH_LIBOPTS_LIBS([gio], [auto])
	AS_CASE([${nut_with_gio_libs}],
		[auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
				[   { depLIBS="`$PKG_CONFIG --silence-errors --libs gio-2.0 2>/dev/null`" \
				      && depLIBS_SOURCE="pkg-config" ; } \
				 || { depLIBS="-lgio-2.0 -lgobject-2.0 -lglib-2.0" \
				      && depLIBS_SOURCE="default" ; }],
				[depLIBS="-lgio-2.0 -lgobject-2.0 -lglib-2.0"
				 depLIBS_SOURCE="default"]
			)],
				[depLIBS="${nut_with_gio_libs}"
				 depLIBS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

	dnl check if gio-2.0 is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_CHECK_HEADERS(gio/gio.h, [nut_have_libglib=yes], [nut_have_libglib=no], [AC_INCLUDES_DEFAULT])
	dnl AC_CHECK_FUNCS(g_bus_get_sync, [], [nut_have_libglib=no])

	if test "${nut_have_libglib}" = "yes"; then
		LIBGLIB_CFLAGS="${depCFLAGS}"
		LIBGLIB_LIBS="${depLIBS}"

		dnl Help ltdl if we can (nut-scanner etc.)
		for TOKEN in $depLIBS ; do
			AS_CASE(["${TOKEN}"],
				[-l*gio*], [
					AX_REALPATH_LIB([${TOKEN}], [SOPATH_LIBGLIB], [])
					AS_IF([test -n "${SOPATH_LIBGLIB}" && test -s "${SOPATH_LIBGLIB}"], [
						AC_DEFINE_UNQUOTED([SOPATH_LIBGLIB],["${SOPATH_LIBGLIB}"],[Path to dynamic library on build system])
						SOFILE_LIBGLIB="`basename \"$SOPATH_LIBGLIB\"`"
						AC_DEFINE_UNQUOTED([SOFILE_LIBGLIB],["${SOFILE_LIBGLIB}"],[Base file name of dynamic library on build system])
						break
					])
				]
			)
		done
		unset TOKEN
	fi

	unset depCFLAGS
	unset depLIBS
	unset depCFLAGS_SOURCE
	unset depLIBS_SOURCE

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi

AC_SUBST([LIBGLIB_CFLAGS])
AC_SUBST([LIBGLIB_LIBS])
])
