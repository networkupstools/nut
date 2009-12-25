dnl Check for LIBUSB compiler flags. On success, set nut_have_libusb="yes"
dnl and set LIBUSB_CFLAGS and LIBUSB_LDFLAGS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBUSB], 
[
if test -z "${nut_have_libusb_seen}"; then
	nut_have_libusb_seen=yes

	dnl save CFLAGS and LDFLAGS
	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	AC_MSG_CHECKING(for libusb version via libusb-config)
	LIBUSB_VERSION=`libusb-config --version 2>/dev/null`
	if (test "$?" != "0"); then  
		AC_MSG_RESULT(not found)
		nut_have_libusb=no
	else
		AC_MSG_RESULT(${LIBUSB_VERSION} found)
		nut_have_libusb=yes
	fi

	AC_MSG_CHECKING(for libusb cflags via libusb-config)
	CFLAGS=`libusb-config --cflags 2>/dev/null`
	if (test "$?" != "0"); then
		AC_MSG_RESULT(not found)
		nut_have_libusb=no
	else
		AC_MSG_RESULT(${CFLAGS})
	fi

	AC_MSG_CHECKING(for libusb libs via libusb-config)
	LDFLAGS=`libusb-config --libs 2>/dev/null`
	if (test "$?" != "0"); then
		AC_MSG_RESULT(not found)
		nut_have_libusb=no
	else
		AC_MSG_RESULT(${LDFLAGS})
	fi

	if test "${nut_have_libusb}" != "yes"; then
		dnl try again using defaults
		CFLAGS=""
		LDFLAGS="-lusb"

		AC_CHECK_HEADERS(usb.h, [nut_have_libusb=yes], [nut_have_libusb=no], [AC_INCLUDES_DEFAULT])
		AC_CHECK_FUNCS(usb_init, [], [nut_have_libusb=no])
	fi

	if test "${nut_have_libusb}" = "yes"; then
		dnl Check for libusb "force driver unbind" availability
		AC_CHECK_FUNCS(usb_detach_kernel_driver_np)
		LIBUSB_CFLAGS="${CFLAGS}"
		LIBUSB_LDFLAGS="${LDFLAGS}"
	fi

	dnl restore original CFLAGS and LDFLAGS
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
fi
])

