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
	CFLAGS=""
	LIBS=""

	LIBUSB_CONFIG="none"
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
		[AC_PATH_PROGS([LIBUSB_CONFIG], [libusb-config], [none])
		 AC_ARG_WITH(libusb-config,
			AS_HELP_STRING([@<:@--with-libusb-config=/path/to/libusb-config@:>@],
				[path to program that reports LibUSB configuration]), dnl ...for LibUSB-0.1
			[
				case "${withval}" in
				"") ;;
				yes|no)
					AC_MSG_ERROR(invalid option --with(out)-libusb-config - see docs/configure.txt)
					;;
				*)
					LIBUSB_CONFIG="${withval}"
					;;
				esac
			]
		 )

		 AS_IF([test x"$LIBUSB_CONFIG" != xnone],
			[AC_MSG_CHECKING([for libusb version via ${LIBUSB_CONFIG}])
			 LIBUSB_VERSION="`"${LIBUSB_CONFIG}" --version 2>/dev/null`"
			 if test "$?" = "0" -a -n "${LIBUSB_VERSION}"; then
				CFLAGS="`"${LIBUSB_CONFIG}" --cflags 2>/dev/null`"
				LIBS="`"${LIBUSB_CONFIG}" --libs 2>/dev/null`"
			 else
				LIBUSB_VERSION="none"
				CFLAGS=""
				LIBS="-lusb"
			 fi
			 AC_MSG_RESULT(${LIBUSB_VERSION} found)
			]
		 )
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
	AC_CHECK_FUNCS(usb_init, [], [
		dnl Some systems may just have libusb in their standard
		dnl paths, but not the pkg-config or libusb-config data
		AS_IF([test "${nut_have_libusb}" = "yes" && test "$LIBUSB_VERSION" = "none" && test -z "$LIBS"],
			[AC_MSG_CHECKING([if libusb is just present in path])
			 LIBS="-L/usr/lib -L/usr/local/lib -lusb"
			 unset ac_cv_func_usb_init || true
			 AC_CHECK_FUNCS(usb_init, [], [nut_have_libusb=no])
			 AC_MSG_RESULT([${nut_have_libusb}])
			], [nut_have_libusb=no]
		)]
	)

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
