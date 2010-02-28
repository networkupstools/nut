dnl Check for LIBPOWERMAN compiler flags. On success, set nut_have_libpowerman="yes"
dnl and set LIBPOWERMAN_CFLAGS and LIBPOWERMAN_LDFLAGS. On failure, set
dnl nut_have_libpowerman="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBPOWERMAN],
[
if test -z "${nut_have_libpowerman_seen}"; then
	nut_have_libpowerman_seen=yes

	dnl save CFLAGS and LDFLAGS
	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	AC_MSG_CHECKING(for libpowerman cflags)
	AC_ARG_WITH(powerman-includes, [
		AS_HELP_STRING([--with-powerman-includes=CFLAGS], [include flags for the libpowerman library])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-powerman-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [CFLAGS="`pkg-config --silence-errors --cflags libpowerman 2>/dev/null`"])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libpowerman libs)
	AC_ARG_WITH(powerman-libs, [
		AS_HELP_STRING([--with-powerman-libs=LDFLAGS], [linker flags for the libpowerman library])
	], [
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-powerman-libs - see docs/configure.txt)
			;;
		*)
			LDFLAGS="${withval}"
			;;
		esac
	], [LDFLAGS="`pkg-config --silence-errors --libs libpowerman 2>/dev/null`"])
	AC_MSG_RESULT([${LDFLAGS}])

	dnl check if libpowerman is usable
	AC_CHECK_HEADERS(libpowerman.h, [nut_have_libpowerman=yes], [nut_have_libpowerman=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(pm_connect, [], [nut_have_libpowerman=no])

	if test "${nut_have_libpowerman}" = "yes"; then
		LIBPOWERMAN_CFLAGS="${CFLAGS}"
		LIBPOWERMAN_LDFLAGS="${LDFLAGS}"
	fi

	dnl restore original CFLAGS and LDFLAGS
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"

fi
])
