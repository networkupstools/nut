dnl Check for LIBLTDL compiler flags. On success, set nut_have_libltdl="yes"
dnl and set LIBLTDL_CFLAGS and LIBLTDL_LIBS. On failure, set
dnl nut_have_libltdl="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBLTDL],
[
if test -z "${nut_have_libltdl_seen}"; then
	nut_have_libltdl_seen=yes
	dnl No NUT_CHECK_PKGCONFIG here: (lib)ltdl.pc was not seen on any OS

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	LIBS=""
	CFLAGS=""
	dnl For fallback below:
	myCFLAGS=""

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
	], [dnl Best-Effort Fallback (LDFLAGS might make more sense for -L...,
		dnl but other m4's have it so) to use if probe below fails:
		myCFLAGS="-I/usr/local/include -I/usr/include -L/usr/local/lib -L/usr/lib"
	])
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
	], [])dnl No fallback here - we probe suitable libs below
	AC_MSG_RESULT([${LIBS}])

	AC_CHECK_HEADERS(ltdl.h, [nut_have_libltdl=yes], [
		dnl Double-check if we stashed include paths to try above
		AS_IF([test -n "$myCFLAGS"], [
			CFLAGS="$myCFLAGS"
			AS_UNSET([ac_cv_header_ltdl_h])
			AC_CHECK_HEADERS(ltdl.h, [nut_have_libltdl=yes], [nut_have_libltdl=no], [AC_INCLUDES_DEFAULT])
			],[nut_have_libltdl=no]
		)], [AC_INCLUDES_DEFAULT])
	AS_IF([test x"$nut_have_libltdl" = xyes], [
		dnl ltdl-number may help find it for MingW DLLs naming
		AC_SEARCH_LIBS(lt_dlinit, ltdl ltdl-7, [], [
			nut_have_libltdl=no
			AS_IF([test -n "$myCFLAGS" -a x"$myCFLAGS" != x"$CFLAGS"], [
				CFLAGS="$myCFLAGS"
				dnl No ltdl-7 here, this codepath is unlikely on Windows where that matters:
				AC_SEARCH_LIBS(lt_dlinit, ltdl, [nut_have_libltdl=yes], [])
			])
		])
	])

	AS_IF([test "${nut_have_libltdl}" = "yes"], [
		AC_DEFINE(HAVE_LIBLTDL, 1, [Define to enable libltdl support])
		LIBLTDL_CFLAGS="${CFLAGS}"
		LIBLTDL_LIBS="${LIBS}"
	])
	unset myCFLAGS

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
