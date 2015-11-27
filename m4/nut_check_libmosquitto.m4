dnl Check for LIBMOSQUITTO compiler flags. On success, set nut_have_libmosquitto="yes"
dnl and set LIBMOSQUITTO_CFLAGS and LIBMOSQUITTO_LIBS. On failure, set
dnl nut_have_libmosquitto="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBMOSQUITTO],
[
if test -z "${nut_have_libmosquitto_seen}"; then
	nut_have_libmosquitto_seen=yes

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AC_MSG_CHECKING(for libmosquitto cflags)
	AC_ARG_WITH(mosquitto-includes,
		AS_HELP_STRING([@<:@--with-mosquitto-includes=CFLAGS@:>@], [include flags for the libmosquitto library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-mosquitto-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libmosquitto libs)
	AC_ARG_WITH(mosquitto-libs,
		AS_HELP_STRING([@<:@--with-mosquitto-libs=LIBS@:>@], [linker flags for the libmosquitto library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-mosquitto-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [LIBS="-lmosquitto"])
	AC_MSG_RESULT([${LIBS}])

	dnl check if libmosquitto is usable
	AC_CHECK_HEADERS(mosquitto.h, [nut_have_libmosquitto=yes], [nut_have_libmosquitto=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(mosquitto_lib_init, [], [nut_have_libmosquitto=no])

	if test "${nut_have_libmosquitto}" = "yes"; then
		LIBMOSQUITTO_CFLAGS="${CFLAGS}"
		LIBMOSQUITTO_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"

fi
])
