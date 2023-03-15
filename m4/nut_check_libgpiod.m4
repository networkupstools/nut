dnl Check for LIBGPIO compiler flags. On success, set nut_have_gpio="yes"
dnl and set LIBGPIO_CFLAGS and LIBGPIO_LIBS. On failure, set
dnl nut_have_gpio="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBGPIO],
[
if test -z "${nut_have_gpio_seen}"; then
	nut_have_gpio_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	nut_gpio_lib=""

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the gpiod library (if any) is installed
		 dnl FIXME : Support detection of cflags/ldflags below by legacy
		 dnl discovery if pkgconfig is not there
		 AC_MSG_CHECKING(for libgpiod version via pkg-config (1.0.0 minimum required))
		 GPIO_VERSION="`$PKG_CONFIG --silence-errors --modversion libgpiod 2>/dev/null`"
		 if test "$?" != "0" -o -z "${GPIO_VERSION}"; then
		    GPIO_VERSION="none"
		 else
		    nut_gpio_lib="libgpiod"
		 fi
		 AC_MSG_RESULT(${GPIO_VERSION} found)
		],
		[GPIO_VERSION="none"
		 AC_MSG_NOTICE([can not check libgpiod settings via pkg-config])
		]
	)

	AC_MSG_CHECKING(for libgpiod cflags)
	AC_ARG_WITH(gpio-includes,
		AS_HELP_STRING([@<:@--with-gpio-includes=CFLAGS@:>@], [include flags for the gpiod library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-gpio-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[CFLAGS="`$PKG_CONFIG --silence-errors --cflags libgpiod 2>/dev/null`" || CFLAGS="-I/usr/include -I/usr/local/include"],
			[CFLAGS="-I/usr/include -I/usr/local/include"]
		)]
	)
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libgpiod ldflags)
	AC_ARG_WITH(gpio-libs,
		AS_HELP_STRING([@<:@--with-gpio-libs=LIBS@:>@], [linker flags for the gpiod library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-gpio-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[LIBS="`$PKG_CONFIG --silence-errors --libs libgpiod 2>/dev/null`" || LIBS="-lgpiod"],
			[LIBS="-lgpiod"]
		)]
	)
	AC_MSG_RESULT([${LIBS}])

	dnl check if gpiod is usable
	AC_CHECK_HEADERS(gpiod.h, [nut_have_gpio=yes], [nut_have_gpio=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(gpiod_chip_open_by_name gpiod_chip_close, [nut_gpio_lib="libgpiod"], [nut_have_gpio=no])

	if test "${nut_have_gpio}" = "yes"; then
		LIBGPIO_CFLAGS="${CFLAGS}"
		LIBGPIO_LIBS="${LIBS}"
	else
		nut_gpio_lib=""
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
