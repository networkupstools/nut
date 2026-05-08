dnl Check for LIBGPIO compiler flags. On success, set nut_have_gpio="yes"
dnl and set LIBGPIO_CFLAGS and LIBGPIO_LIBS. On failure, set
dnl nut_have_gpio="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBGPIO],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_gpio_seen}"], [
        nut_have_gpio_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([gpio], [auto], [gpiod])
        NUT_ARG_WITH_LIBOPTS_LIBS([gpio], [auto], [gpiod])

        nut_noncv_checked_libgpio_now=no
        AC_CACHE_VAL([nut_cv_checked_libgpio], [
            nut_noncv_checked_libgpio_now=yes

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
            nut_cv_gpio_lib=""

            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                dnl See which version of the gpiod library (if any) is installed
                dnl FIXME : Support detection of cflags/ldflags below by legacy
                dnl discovery if pkgconfig is not there
                AC_MSG_CHECKING(for libgpiod version via pkg-config (1.0.0 minimum required))
                nut_cv_GPIO_VERSION="`$PKG_CONFIG --silence-errors --modversion libgpiod 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_GPIO_VERSION}"], [
                    nut_cv_GPIO_VERSION="none"
                ], [
                    nut_cv_gpio_lib="libgpiod"
                ])
                AC_MSG_RESULT(${nut_cv_GPIO_VERSION} found)
            ], [
                nut_cv_GPIO_VERSION="none"
                AC_MSG_NOTICE([can not check libgpiod settings via pkg-config])
            ])

            AC_MSG_CHECKING(for libgpiod cflags)
            AS_CASE([${nut_with_gpio_includes}],
                [auto], [
                    AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libgpiod 2>/dev/null`" \
                              && depCFLAGS_SOURCE="pkg-config" ; } \
                         || { depCFLAGS="-I/usr/include -I/usr/local/include" \
                              && depCFLAGS_SOURCE="default" ; }
                        ], [
                            depCFLAGS="-I/usr/include -I/usr/local/include"
                            depCFLAGS_SOURCE="default"
                        ])
                    ],
                    [depCFLAGS="${nut_with_gpio_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for libgpiod ldflags)
            AS_CASE([${nut_with_gpio_libs}],
                [auto], [
                    AS_IF([test x"$have_PKG_CONFIG" = xyes],
                        [   { depLIBS="`$PKG_CONFIG --silence-errors --libs libgpiod 2>/dev/null`" \
                              && depLIBS_SOURCE="pkg-config" ; } \
                         || { depLIBS="-lgpiod" \
                              && depLIBS_SOURCE="default" ; }
                        ], [
                            depLIBS="-lgpiod"
                            depLIBS_SOURCE="default"
                        ])
                    ],
                    [depLIBS="${nut_with_gpio_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if gpiod is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(gpiod.h, [nut_cv_have_gpio=yes], [nut_cv_have_gpio=no], [AC_INCLUDES_DEFAULT])
            AS_IF([test x"${nut_cv_have_gpio}" = xyes], [AC_CHECK_FUNCS(gpiod_chip_close, [], [nut_cv_have_gpio=no])])
            AS_IF([test x"${nut_cv_have_gpio}" = xyes], [
                AS_CASE(["${nut_cv_GPIO_VERSION}"],
                    [2.*], [AC_CHECK_FUNCS(gpiod_chip_open, [nut_cv_gpio_lib="libgpiod"], [nut_cv_have_gpio=no])],
                    [1.*], [AC_CHECK_FUNCS(gpiod_chip_open_by_name, [nut_cv_gpio_lib="libgpiod"], [nut_cv_have_gpio=no])],
                        [AC_CHECK_FUNCS(gpiod_chip_open_by_name, [
                            nut_cv_gpio_lib="libgpiod"
                            AS_IF([test x"${nut_cv_GPIO_VERSION}" = xnone], [nut_cv_GPIO_VERSION="1.x"])
                         ], [
                            AC_CHECK_FUNCS(gpiod_chip_open, [
                                nut_cv_gpio_lib="libgpiod"
                                AS_IF([test x"${nut_cv_GPIO_VERSION}" = xnone], [nut_cv_GPIO_VERSION="2.x"])
                            ])]
                         )]
                )
            ])

            nut_cv_LIBGPIO_CFLAGS=""
            nut_cv_LIBGPIO_LIBS=""
            AS_IF([test "${nut_cv_have_gpio}" = "yes"], [
                nut_cv_LIBGPIO_CFLAGS="${depCFLAGS}"
                nut_cv_LIBGPIO_LIBS="${depLIBS}"
            ], [
                dnl FIXME: Report "none" here?
                nut_cv_gpio_lib=""
            ])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBGPIO_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBGPIO_LIBS_SOURCE="${depLIBS_SOURCE}"

            AC_CACHE_VAL([nut_cv_have_gpio], [])
            AC_CACHE_VAL([nut_cv_gpio_lib], [])
            AC_CACHE_VAL([nut_cv_GPIO_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBGPIO_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBGPIO_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBGPIO_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBGPIO_LIBS_SOURCE], [])

            unset depCFLAGS
            unset depLIBS
            unset depCFLAGS_SOURCE
            unset depLIBS_SOURCE

            dnl restore original CFLAGS and LIBS
            CFLAGS="${CFLAGS_ORIG}"
            LIBS="${LIBS_ORIG}"

            unset CFLAGS_ORIG
            unset LIBS_ORIG

            dnl Complete the cache ritual
            nut_cv_checked_libgpio="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libgpio}" = xyes], [
            nut_have_gpio="${nut_cv_have_gpio}"
            nut_gpio_lib="${nut_cv_gpio_lib}"

            LIBGPIO_CFLAGS="${nut_cv_LIBGPIO_CFLAGS}"
            LIBGPIO_LIBS="${nut_cv_LIBGPIO_LIBS}"

            dnl For troubleshooting of re-runs, mostly:
            LIBGPIO_CFLAGS_SOURCE="${nut_cv_LIBGPIO_CFLAGS_SOURCE}"
            LIBGPIO_LIBS_SOURCE="${nut_cv_LIBGPIO_LIBS_SOURCE}"
            GPIO_VERSION="${nut_cv_GPIO_VERSION}"

            AS_IF([test "${nut_noncv_checked_libgpio_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                CFLAGS="${nut_cv_LIBGPIO_CFLAGS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(gpiod.h, [], [nut_have_gpio=no], [AC_INCLUDES_DEFAULT])
                AS_IF([test x"${nut_have_gpio}" = xyes], [AC_CHECK_FUNCS(gpiod_chip_close, [], [nut_have_gpio=no])])
                AS_IF([test x"${nut_have_gpio}" = xyes], [
                    AS_CASE(["${nut_cv_GPIO_VERSION}"],
                        [2.*], [AC_CHECK_FUNCS(gpiod_chip_open, [], [nut_have_gpio=no])],
                        [1.*], [AC_CHECK_FUNCS(gpiod_chip_open_by_name, [], [nut_have_gpio=no])],
                            [AC_CHECK_FUNCS(gpiod_chip_open_by_name, [], [
                                AC_CHECK_FUNCS(gpiod_chip_open, [])
                             ])]
                    )
                ])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                unset CFLAGS_ORIG
            ])

            AS_IF([test "${nut_have_gpio}" = "yes"], [
                dnl Normally this would be in library headers, but they do not seem forthcoming
                AS_CASE([${nut_cv_GPIO_VERSION}],
                    [2.*], [
                        AC_DEFINE(WITH_LIBGPIO_VERSION, 0x00020000, [Define libgpio C API version generation])
                        AC_DEFINE_UNQUOTED(WITH_LIBGPIO_VERSION_STR, ["0x00020000"], [Define libgpio C API version generation as string])
                        ],
                    [1.*], [
                        AC_DEFINE(WITH_LIBGPIO_VERSION, 0x00010000, [Define libgpio C API version generation])
                        AC_DEFINE_UNQUOTED(WITH_LIBGPIO_VERSION_STR, ["0x00010000"], [Define libgpio C API version generation as string])
                        ]
                )
            ], [
                AC_DEFINE(WITH_LIBGPIO_VERSION, 0x00000000, [Define libgpio C API version generation])
                AC_DEFINE_UNQUOTED(WITH_LIBGPIO_VERSION_STR, ["0x00000000"], [Define libgpio C API version generation as string])
            ])

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libgpio_now}" = no], [
                AC_MSG_NOTICE([libgpio (cached): ${nut_have_gpio} (${nut_gpio_lib} version:${GPIO_VERSION})])
                AC_MSG_NOTICE([libgpio (cached): cflags_source='${LIBGPIO_CFLAGS_SOURCE}' libs_source='${LIBGPIO_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libgpio (cached): LIBGPIO_CFLAGS='${LIBGPIO_CFLAGS}'])
                AC_MSG_NOTICE([libgpio (cached): LIBGPIO_LIBS='${LIBGPIO_LIBS}'])
            ])
        ])
    ])
])
