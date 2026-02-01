dnl Check for LIBGLIB (used by nut-upower driver) and related LIBGIO (optionally
dnl used by nut-scanner) compiler and linker flags.
dnl On success, set nut_have_libglib="yes" and set LIBGLIB_CFLAGS and LIBGLIB_LIBS.
dnl On failure, set nut_have_libglib="no".
dnl Similarly for LIBGIO_FLAGS, LIBGIO_LIBS and nut_have_libgio.
dnl This macro can be run multiple times, but will do the checking only once.

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
		 AC_MSG_CHECKING(for glib-2.0 version via pkg-config (2.26.0 minimum required))
		 LIBGLIB_VERSION="`$PKG_CONFIG --silence-errors --modversion glib-2.0 2>/dev/null`"
		 if test "$?" != "0" -o -z "${LIBGLIB_VERSION}"; then
		    LIBGLIB_VERSION="none"
		 fi
		 AC_MSG_RESULT(${LIBGLIB_VERSION} found)
		], [AC_MSG_NOTICE([can not check glib-2.0 settings via pkg-config])]
	)

	AC_MSG_CHECKING(for glib-2.0 cflags)
	NUT_ARG_WITH_LIBOPTS_INCLUDES([glib], [auto])
	AS_CASE([${nut_with_glib_includes}],
		[auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
				[   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags glib-2.0 2>/dev/null`" \
				      && depCFLAGS_SOURCE="pkg-config" ; } \
				 || { depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include" \
				      && depCFLAGS_SOURCE="default" ; }],
				[depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include"
				 depCFLAGS_SOURCE="default"]
			)],
				[depCFLAGS="${nut_with_glib_includes}"
				 depCFLAGS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

	AC_MSG_CHECKING(for glib-2.0 ldflags)
	NUT_ARG_WITH_LIBOPTS_LIBS([glib], [auto])
	AS_CASE([${nut_with_glib_libs}],
		[auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
				[   { depLIBS="`$PKG_CONFIG --silence-errors --libs glib-2.0 2>/dev/null`" \
				      && depLIBS_SOURCE="pkg-config" ; } \
				 || { depLIBS="-lgobject-2.0 -lglib-2.0" \
				      && depLIBS_SOURCE="default" ; }],
				[depLIBS="-lgobject-2.0 -lglib-2.0"
				 depLIBS_SOURCE="default"]
			)],
				[depLIBS="${nut_with_glib_libs}"
				 depLIBS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

	dnl check if glib-2.0 is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_CHECK_HEADERS(glib.h, [nut_have_libglib=yes], [nut_have_libglib=no], [AC_INCLUDES_DEFAULT])

	if test "${nut_have_libglib}" = "yes"; then
		LIBGLIB_CFLAGS="${depCFLAGS}"
		LIBGLIB_LIBS="${depLIBS}"

		dnl GLib headers seem incorrect and offensive to many compilers
		dnl (starting names with underscores and capital characters,
		dnl varying support for attributes, method pointer mismatches).
		dnl There is nothing NUT can do about it, except telling the
		dnl compiler that we take these headers from the system as they
		dnl are, so strict checks should not apply to them.
		dnl On newer releases (2025+) the headers and CLANG seem to work
		dnl together out of the box, but during the decade before this is
		dnl troublesome.
		AS_IF([test "${CLANGCC}" = "yes" || test "${GCC}" = "yes"], [
			myGLIB_CFLAGS=""
			for TOKEN in ${LIBGLIB_CFLAGS} ; do
				AS_CASE(["${TOKEN}"],
					[-I/*], [
						_IDIR="`echo \"${TOKEN}\" | sed 's/^-I//'`"
						AS_IF([echo " ${LIBGLIB_CFLAGS}" | ${EGREP} " -isystem *${_IDIR}" >/dev/null],
							[myGLIB_CFLAGS="${myGLIB_CFLAGS} ${TOKEN}"],
							[myGLIB_CFLAGS="${myGLIB_CFLAGS} -isystem ${_IDIR} ${TOKEN}"]
						)],
						[myGLIB_CFLAGS="${myGLIB_CFLAGS} ${TOKEN}"]
				)
			done
			unset TOKEN
			unset _IDIR
			myGLIB_CFLAGS="`echo \"${myGLIB_CFLAGS}\" | sed 's/^ *//'`"

			AS_IF([test x"${LIBGLIB_CFLAGS}" != x -a x"${LIBGLIB_CFLAGS}" != x"${myGLIB_CFLAGS}"], [
				AC_MSG_NOTICE([Patched libglib CFLAGS to declare -isystem])
				AS_IF([test x"${nut_enable_configure_debug}" = xyes], [
					AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) old: ${LIBGLIB_CFLAGS}])
					AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) new: ${myGLIB_CFLAGS}])
				])
				LIBGLIB_CFLAGS="${myGLIB_CFLAGS}"
			])
			unset myGLIB_CFLAGS
		])

		dnl Help ltdl if we can (nut-scanner etc.)
		for TOKEN in $depLIBS ; do
			AS_CASE(["${TOKEN}"],
				[-l*glib*], [
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

	dnl ///////////////////////////////////////
	dnl //          Same for libgio          //
	dnl ///////////////////////////////////////

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the glib/gio library (if any) is installed
		 AC_MSG_CHECKING(for gio-2.0 version via pkg-config (2.26.0 minimum required))
		 LIBGIO_VERSION="`$PKG_CONFIG --silence-errors --modversion gio-2.0 2>/dev/null`"
		 if test "$?" != "0" -o -z "${LIBGIO_VERSION}"; then
			LIBGIO_VERSION="none"
		 fi
		 AC_MSG_RESULT(${LIBGIO_VERSION} found)
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
	AC_CHECK_HEADERS(gio/gio.h, [nut_have_libgio=yes], [nut_have_libgio=no], [AC_INCLUDES_DEFAULT])
	dnl AC_CHECK_FUNCS(g_bus_get_sync, [], [nut_have_libgio=no])

	if test "${nut_have_libglib}" = "yes"; then

		dnl GLib headers seem incorrect and offensive to many compilers
		dnl (starting names with underscores and capital characters,
		dnl varying support for attributes, method pointer mismatches).
		dnl There is nothing NUT can do about it, except telling the
		dnl compiler that we take these headers from the system as they
		dnl are, so strict checks should not apply to them.
		dnl On newer releases (2025+) the headers and CLANG seem to work
		dnl together out of the box, but during the decade before this is
		dnl troublesome.
		AS_IF([test "${CLANGCC}" = "yes" || test "${GCC}" = "yes"], [
			myGLIB_CFLAGS=""
			for TOKEN in ${depCFLAGS} ; do
				AS_CASE(["${TOKEN}"],
					[-I/*], [
						_IDIR="`echo \"${TOKEN}\" | sed 's/^-I//'`"
						AS_IF([echo " ${depCFLAGS}" | ${EGREP} " -isystem *${_IDIR}" >/dev/null],
							[myGLIB_CFLAGS="${myGLIB_CFLAGS} ${TOKEN}"],
							[myGLIB_CFLAGS="${myGLIB_CFLAGS} -isystem ${_IDIR} ${TOKEN}"]
						)],
						[myGLIB_CFLAGS="${myGLIB_CFLAGS} ${TOKEN}"]
				)
			done
			unset TOKEN
			unset _IDIR
			myGLIB_CFLAGS="`echo \"${myGLIB_CFLAGS}\" | sed 's/^ *//'`"

			AS_IF([test x"${depCFLAGS}" != x -a x"${depCFLAGS}" != x"${myGLIB_CFLAGS}"], [
				AC_MSG_NOTICE([Patched libglib/libgio CFLAGS to declare -isystem])
				AS_IF([test x"${nut_enable_configure_debug}" = xyes], [
					AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) old: ${depCFLAGS}])
					AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) new: ${myGLIB_CFLAGS}])
				])
				depCFLAGS="${myGLIB_CFLAGS}"
			])
			unset myGLIB_CFLAGS
		])

		LIBGLIB_CFLAGS="${depCFLAGS}"
		LIBGLIB_LIBS="${depLIBS}"

		dnl Help ltdl if we can (nut-scanner etc.)
		for TOKEN in $depLIBS ; do
			AS_CASE(["${TOKEN}"],
				[-l*gio*], [
					AX_REALPATH_LIB([${TOKEN}], [SOPATH_LIBGIO], [])
					AS_IF([test -n "${SOPATH_LIBGIO}" && test -s "${SOPATH_LIBGIO}"], [
						AC_DEFINE_UNQUOTED([SOPATH_LIBGIO],["${SOPATH_LIBGIO}"],[Path to dynamic library on build system])
						SOFILE_LIBGIO="`basename \"$SOPATH_LIBGIO\"`"
						AC_DEFINE_UNQUOTED([SOFILE_LIBGIO],["${SOFILE_LIBGIO}"],[Base file name of dynamic library on build system])
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

	AC_SUBST([LIBGLIB_CFLAGS])
	AC_SUBST([LIBGLIB_LIBS])

	AC_SUBST([LIBGIO_CFLAGS])
	AC_SUBST([LIBGIO_LIBS])
fi
])
