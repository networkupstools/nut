dnl Check for LIBLTDL compiler flags. On success, set nut_have_libltdl="yes"
dnl and set LIBLTDL_CFLAGS and LIBLTDL_LIBS. On failure, set
dnl nut_have_libltdl="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBLTDL], 
[
if test -z "${nut_have_libltdl_seen}"; then
	nut_have_libltdl_seen=yes

	dnl save LIBS
	LIBS_ORIG="${LIBS}"
	LIBS=""

	AC_MSG_CHECKING(for libltdl cflags)
	AC_ARG_WITH(libltdl-includes,
		AS_HELP_STRING([@<:@--with-libltdl-includes=CFLAGS@:>@], [include flags for the libltdl library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libltdl-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libltdl ldflags)
	AC_ARG_WITH(libltdl-libs,
		AS_HELP_STRING([@<:@--with-libltdl-libs=LIBS@:>@], [linker flags for the libltdl library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-libltdl-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LIBS}])

	AC_CHECK_HEADERS(ltdl.h, [nut_have_libltdl=yes], [nut_have_libltdl=no], [AC_INCLUDES_DEFAULT])
	dnl ltdl-number may help find it for MingW DLLs naming
	AC_SEARCH_LIBS(lt_dlinit, ltdl ltdl-7, [], [nut_have_libltdl=no])

	if test "${nut_have_libltdl}" = "yes"; then
		AC_DEFINE(HAVE_LIBLTDL, 1, [Define to enable libltdl support])
		LIBLTDL_CFLAGS=""
		LIBLTDL_LIBS="${LIBS}"
	fi

	dnl restore original LIBS
	LIBS="${LIBS_ORIG}"
fi
])
