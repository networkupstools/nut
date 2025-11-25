dnl Check for LIBGPIO compiler flags. On success, set nut_have_gpio="yes"
dnl and set LIBGPIO_CFLAGS and LIBGPIO_LIBS. On failure, set
dnl nut_have_gpio="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBGPIO],
[
if test -z "${nut_have_gpio_seen}"; then
	nut_have_gpio_seen=yes
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

	# Store implementation (if any) to be reported by configure.ac:
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
	NUT_ARG_WITH_LIBOPTS_INCLUDES([gpio], [auto], [gpiod])
	AS_CASE([${nut_with_gpio_includes}],
		[auto], [
			AS_IF([test x"$have_PKG_CONFIG" = xyes],
				[   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libgpiod 2>/dev/null`" \
				      && depCFLAGS_SOURCE="pkg-config" ; } \
				 || { depCFLAGS="-I/usr/include -I/usr/local/include" \
				      && depCFLAGS_SOURCE="default" ; }],
				[depCFLAGS="-I/usr/include -I/usr/local/include"
				 depCFLAGS_SOURCE="default"]
			)],
			[depCFLAGS="${nut_with_gpio_includes}"
			 depCFLAGS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

	AC_MSG_CHECKING(for libgpiod ldflags)
	NUT_ARG_WITH_LIBOPTS_LIBS([gpio], [auto], [gpiod])
	AS_CASE([${nut_with_gpio_libs}],
		[auto], [
			AS_IF([test x"$have_PKG_CONFIG" = xyes],
				[   { depLIBS="`$PKG_CONFIG --silence-errors --libs libgpiod 2>/dev/null`" \
				      && depLIBS_SOURCE="pkg-config" ; } \
				 || { depLIBS="-lgpiod" \
				      && depLIBS_SOURCE="default" ; }],
				[depLIBS="-lgpiod"
				 depLIBS_SOURCE="default"]
			)],
			[depLIBS="${nut_with_gpio_libs}"
			 depLIBS_SOURCE="confarg"]
	)
	AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

	dnl check if gpiod is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_CHECK_HEADERS(gpiod.h, [nut_have_gpio=yes], [nut_have_gpio=no], [AC_INCLUDES_DEFAULT])
	AS_IF([test x"${nut_have_gpio}" = xyes], [AC_CHECK_FUNCS(gpiod_chip_close, [], [nut_have_gpio=no])])
	AS_IF([test x"${nut_have_gpio}" = xyes], [
		AS_CASE(["${GPIO_VERSION}"],
			[2.*], [AC_CHECK_FUNCS(gpiod_chip_open, [nut_gpio_lib="libgpiod"], [nut_have_gpio=no])],
			[1.*], [AC_CHECK_FUNCS(gpiod_chip_open_by_name, [nut_gpio_lib="libgpiod"], [nut_have_gpio=no])],
				[AC_CHECK_FUNCS(gpiod_chip_open_by_name, [
					nut_gpio_lib="libgpiod"
					AS_IF([test x"${GPIO_VERSION}" = xnone], [GPIO_VERSION="1.x"])
				 ], [
					AC_CHECK_FUNCS(gpiod_chip_open, [
						nut_gpio_lib="libgpiod"
						AS_IF([test x"${GPIO_VERSION}" = xnone], [GPIO_VERSION="2.x"])
					])]
				 )]
		)
	])

	if test "${nut_have_gpio}" = "yes"; then
		LIBGPIO_CFLAGS="${depCFLAGS}"
		LIBGPIO_LIBS="${depLIBS}"

		dnl Normally this would be in library headers, but they do not seem forthcoming
		AS_CASE([${GPIO_VERSION}],
			[2.*], [
				AC_DEFINE(WITH_LIBGPIO_VERSION, 0x00020000, [Define libgpio C API version generation])
				AC_DEFINE_UNQUOTED(WITH_LIBGPIO_VERSION_STR, ["0x00020000"], [Define libgpio C API version generation as string])
				],
			[1.*], [
				AC_DEFINE(WITH_LIBGPIO_VERSION, 0x00010000, [Define libgpio C API version generation])
				AC_DEFINE_UNQUOTED(WITH_LIBGPIO_VERSION_STR, ["0x00010000"], [Define libgpio C API version generation as string])
				]
		)
	else
		dnl FIXME: Report "none" here?
		nut_gpio_lib=""

		AC_DEFINE(WITH_LIBGPIO_VERSION, 0x00000000, [Define libgpio C API version generation])
		AC_DEFINE_UNQUOTED(WITH_LIBGPIO_VERSION_STR, ["0x00000000"], [Define libgpio C API version generation as string])
	fi

	unset depCFLAGS
	unset depLIBS
	unset depCFLAGS_SOURCE
	unset depLIBS_SOURCE

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
