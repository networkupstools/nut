dnl Check for LIBUSB compiler flags. On success, set nut_have_libusb="yes"
dnl and set LIBUSB_CFLAGS and LIBUSB_LIBS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBUSB],
[
if test -z "${nut_have_libusb_seen}"; then
	nut_have_libusb_seen=yes

	dnl save CFLAGS, LDFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"
	LIBS_ORIG="${LIBS}"

	AC_MSG_CHECKING(for libusb version via pkg-config)
	LIBUSB_VERSION="`pkg-config --silence-errors --modversion libusb 2>/dev/null`"
	if test "$?" = "0" -a -n "${LIBUSB_VERSION}"; then
		CFLAGS="`pkg-config --silence-errors --cflags libusb 2>/dev/null`"
		LDFLAGS=""
		LIBS="`pkg-config --silence-errors --libs libusb 2>/dev/null`"
	else
		AC_MSG_CHECKING(via libusb-config)
		LIBUSB_VERSION="`libusb-config --version 2>/dev/null`"
		if test "$?" = "0" -a -n "${LIBUSB_VERSION}"; then
			CFLAGS="`libusb-config --cflags 2>/dev/null`"
			LDFLAGS=""
			LIBS="`libusb-config --libs 2>/dev/null`"
		else
			LIBUSB_VERSION="none"
			CFLAGS=""
			LDFLAGS=""
			LIBS="-lusb"
		fi
	fi
	AC_MSG_RESULT(${LIBUSB_VERSION} found)

	AC_MSG_CHECKING(for libusb cflags)
	AC_ARG_WITH(usb-includes,
		AS_HELP_STRING([@<:@--with-usb-includes=CFLAGS@:>@], [include flags for the libusb library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-usb-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libusb ldflags)
	AC_ARG_WITH(usb-libs,
		AS_HELP_STRING([@<:@--with-usb-libs=LIBS@:>@], [linker flags for the libusb library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-usb-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LIBS}])

	AC_SEARCH_LIBS(regcomp, regex)

	dnl check if libusb is usable
	AC_CHECK_HEADERS(usb.h, [nut_have_libusb=yes], [nut_have_libusb=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(usb_init, [], [nut_have_libusb=no])

	if test "${nut_have_libusb}" = "yes"; then
		dnl Check for libusb "force driver unbind" availability
		AC_CHECK_FUNCS(usb_detach_kernel_driver_np)
		LIBUSB_CFLAGS="${CFLAGS}"
		LIBUSB_LDFLAGS="${LDFLAGS} ${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
