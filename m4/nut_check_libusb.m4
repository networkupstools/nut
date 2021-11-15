dnl Check for LIBUSB compiler flags. On success, set nut_have_libusb="yes"
dnl and set LIBUSB_CFLAGS and LIBUSB_LIBS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBUSB],
[
if test -z "${nut_have_libusb_seen}"; then
	nut_have_libusb_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING(for libusb version via pkg-config)
		 LIBUSB_VERSION="`$PKG_CONFIG --silence-errors --modversion libusb 2>/dev/null`"
		 if test "$?" != "0" -o -z "${LIBUSB_VERSION}"; then
		    LIBUSB_VERSION="none"
		 fi
		 AC_MSG_RESULT(${LIBUSB_VERSION} found)
		],
		[LIBUSB_VERSION="none"
		 AC_MSG_NOTICE([can not check libusb settings via pkg-config])
		]
	)

	AS_IF([test x"${LIBUSB_VERSION}" != xnone],
		[CFLAGS="`$PKG_CONFIG --silence-errors --cflags libusb 2>/dev/null`"
		 LIBS="`$PKG_CONFIG --silence-errors --libs libusb 2>/dev/null`"
		],
		[AC_MSG_CHECKING([via libusb-config (if present)])
		 LIBUSB_VERSION="`libusb-config --version 2>/dev/null`"
		 if test "$?" = "0" -a -n "${LIBUSB_VERSION}"; then
			CFLAGS="`libusb-config --cflags 2>/dev/null`"
			LIBS="`libusb-config --libs 2>/dev/null`"
		 else
			LIBUSB_VERSION="none"
			CFLAGS=""
			LIBS="-lusb"
		 fi
		 AC_MSG_RESULT(${LIBUSB_VERSION} found)
		]
	)

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

	dnl check if libusb is usable
	AC_CHECK_HEADERS(usb.h, [nut_have_libusb=yes], [nut_have_libusb=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(usb_init, [], [nut_have_libusb=no])

	if test "${nut_have_libusb}" = "yes"; then
		dnl Check for libusb "force driver unbind" availability
		AC_CHECK_FUNCS(usb_detach_kernel_driver_np)
		LIBUSB_CFLAGS="${CFLAGS}"
		LIBUSB_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
