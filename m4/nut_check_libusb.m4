dnl Check for LIBUSB 1.0 or 0.1 (and, if found, fill 'nut_usb_lib' with its
dnl approximate version) and its compiler flags. On success, set
dnl nut_have_libusb="yes" and set LIBUSB_CFLAGS and LIBUSB_LIBS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.
dnl
dnl By default, if both libusb 1.0 and libusb 0.1 are available and appear to be
dnl usable, libusb 1.0 takes precedence.
dnl An optional argument with value 'libusb-1.0' or 'libusb-0.1' can be used to
dnl restrict checks to a specific version.

AC_DEFUN([NUT_CHECK_LIBUSB],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libusb_seen}"], [
        nut_have_libusb_seen=yes

        dnl Determine an optional LIBUSB_CONFIG (likely only for 0.1)
        NUT_ARG_WITH_LIBOPTS_CONFIGSCRIPT([libusb], [], [], [], [LibUSB(-0.1)])
        NUT_ARG_WITH_LIBOPTS_INCLUDES([usb], [auto], [libusb])
        NUT_ARG_WITH_LIBOPTS_LIBS([usb], [auto], [libusb])

        nut_noncv_checked_libusb_now=no
        AC_CACHE_VAL([nut_cv_checked_libusb], [
            nut_noncv_checked_libusb_now=yes

            AC_REQUIRE([NUT_CHECK_PKGCONFIG])

            dnl Our USB matching relies on regex abilities
            AC_REQUIRE([NUT_CHECK_LIBREGEX])

            dnl save CFLAGS and LIBS
            CFLAGS_ORIG="${CFLAGS}"
            LIBS_ORIG="${LIBS}"
            CFLAGS=""
            LIBS=""
            depCFLAGS=""
            depCFLAGS_SOURCE=""
            depLIBS=""
            depLIBS_SOURCE=""

            dnl Magic-format string to hold chosen libusb version and its config-source
            nut_cv_usb_lib=""

            dnl Start with what the build args said:
            nut_cv_with_usb="${nut_with_usb}"

            dnl TOTHINK: What if there are more than 0.1 and 1.0 to juggle?
            dnl TODO? Add libusb-compat (1.0 code with 0.1's API) to the mix?
            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                AC_MSG_CHECKING([for libusb-1.0 version via pkg-config])
                nut_cv_LIBUSB_1_0_VERSION="`$PKG_CONFIG --silence-errors --modversion libusb-1.0 2>/dev/null`" \
                    && test -n "${nut_cv_LIBUSB_1_0_VERSION}" \
                    || nut_cv_LIBUSB_1_0_VERSION="none"
                AC_MSG_RESULT([${nut_cv_LIBUSB_1_0_VERSION} found])

                AC_MSG_CHECKING([for libusb(-0.1) version via pkg-config])
                nut_cv_LIBUSB_0_1_VERSION="`$PKG_CONFIG --silence-errors --modversion libusb 2>/dev/null`" \
                    && test -n "${nut_cv_LIBUSB_0_1_VERSION}" \
                    || nut_cv_LIBUSB_0_1_VERSION="none"
                AC_MSG_RESULT([${nut_cv_LIBUSB_0_1_VERSION} found])
            ], [
                nut_cv_LIBUSB_0_1_VERSION="none"
                nut_cv_LIBUSB_1_0_VERSION="none"
                AC_MSG_NOTICE([can not check libusb settings via pkg-config])
            ])

            dnl Note: it seems the script was only shipped for libusb-0.1
            dnl So we don't separate into LIBUSB_0_1_CONFIG and LIBUSB_1_0_CONFIG
            dnl MAYBE bump preference of a found script over pkg-config?
            AS_IF([test x"${LIBUSB_CONFIG}" != xnone], [
                AC_MSG_CHECKING([via ${LIBUSB_CONFIG}])
                nut_cv_LIBUSB_CONFIG_VERSION="`$LIBUSB_CONFIG --version 2>/dev/null`" \
                    && test -n "${nut_cv_LIBUSB_CONFIG_VERSION}" \
                    || nut_cv_LIBUSB_CONFIG_VERSION="none"
                AC_MSG_RESULT([${nut_cv_LIBUSB_CONFIG_VERSION} found])
            ], [
                nut_cv_LIBUSB_CONFIG_VERSION="none"
            ])

            dnl By default, prefer newest available, and if anything is known
            dnl to pkg-config, prefer that. Otherwise, fall back to script data:
            AS_IF([test x"${nut_cv_LIBUSB_1_0_VERSION}" != xnone], [
                nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_1_0_VERSION}"
                nut_cv_usb_lib="(libusb-1.0)"
                dnl On Windows (mingw/MSYS2 native and cross builds) only
                dnl libusb-1.0 is supported (with the WinUSB driver), see
                dnl https://github.com/networkupstools/nut/issues/1507
            ], [
                AS_IF([test x"${nut_cv_LIBUSB_0_1_VERSION}" != xnone], [
                    nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_0_1_VERSION}"
                    nut_cv_usb_lib="(libusb-0.1)"
                ], [
                    nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_CONFIG_VERSION}"
                    AS_IF([test x"${nut_cv_LIBUSB_CONFIG_VERSION}" != xnone], [
                        dnl TODO: This assumes 0.1; check for 1.0+ somehow?
                        nut_cv_usb_lib="(libusb-0.1-config)"
                    ], [
                        nut_cv_usb_lib=""
                    ])
                ])
            ])

            dnl Pick up the default or caller-provided choice here from
            dnl NUT-ARG-WITH(usb, ...) in the main configure.ac script
            AC_MSG_CHECKING([for libusb preferred version])
            AS_CASE(["${nut_cv_with_usb}"],
                [auto], [], dnl Use preference picked above
                [yes],  [], dnl Use preference from above, fail in the end if none found
                [no],   [], dnl Try to find, report in the end if that is discarded; TODO: not waste time?
                [libusb-1.0|1.0], [
                    dnl NOTE: Assuming there is no libusb-config-1.0 or similar script, never saw one
                    AS_IF([test x"${nut_cv_LIBUSB_1_0_VERSION}" = xnone],
                        [AC_MSG_ERROR([option --with-usb=${nut_cv_with_usb} was required, but this library version was not detected])
                        ])
                    nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_1_0_VERSION}"
                    nut_cv_usb_lib="(libusb-1.0)"
                    ],
                [libusb-0.1|0.1], [
                    AS_CASE(["${target_os}"],
                        [*mingw*], [AC_MSG_ERROR([option --with-usb=${nut_cv_with_usb} is not supported for Windows builds, please use libusb-1.0 (with the WinUSB driver), see https://github.com/networkupstools/nut/issues/1507])]
                    )
                    AS_IF([test x"${nut_cv_LIBUSB_0_1_VERSION}" = xnone \
                        && test x"${nut_cv_LIBUSB_CONFIG_VERSION}" = xnone],
                        [AC_MSG_ERROR([option --with-usb=${nut_cv_with_usb} was required, but this library version was not detected])
                        ])
                    AS_IF([test x"${nut_cv_LIBUSB_0_1_VERSION}" != xnone],
                        [nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_0_1_VERSION}"
                         nut_cv_usb_lib="(libusb-0.1)"
                        ],
                        [nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_CONFIG_VERSION}"
                         nut_cv_usb_lib="(libusb-0.1-config)"
                        ])
                    ],
                [dnl default
                    AC_MSG_ERROR([invalid option value --with-usb=${nut_cv_with_usb} - see docs/configure.txt])
                    ]
            )
            AC_MSG_RESULT([${nut_cv_LIBUSB_VERSION} ${nut_cv_usb_lib}])

            dnl A libusb-1.0 picked by preference (not explicitly requested)
            dnl may be too old for NUT: libusb_error_name() (1.0.9+) is the
            dnl minimum, standing in for libusb_strerror() (1.0.16+, see
            dnl https://github.com/networkupstools/nut/issues/509) e.g. with
            dnl the libusb1-1.0.9 of CentOS 6. If a libusb-0.1 is also available,
            dnl fall back to it rather than end up with no USB drivers, see
            dnl https://github.com/networkupstools/nut/issues/3781
            dnl Custom cflags/ldflags are taken as an explicit choice too.
            AS_IF([test x"${nut_cv_usb_lib}" = x"(libusb-1.0)" \
                && test x"${nut_with_usb_includes}" = xauto \
                && test x"${nut_with_usb_libs}" = xauto \
                && test x"${nut_cv_LIBUSB_0_1_VERSION}" != xnone -o x"${nut_cv_LIBUSB_CONFIG_VERSION}" != xnone],
            [
                AS_CASE(["${nut_cv_with_usb}"],
                    [auto|yes], [
                        AC_MSG_CHECKING([whether libusb-1.0 ${nut_cv_LIBUSB_1_0_VERSION} provides libusb_init() and libusb_error_name()])
                        CFLAGS="${CFLAGS_ORIG} `$PKG_CONFIG --silence-errors --cflags libusb-1.0 2>/dev/null`"
                        LIBS="${LIBS_ORIG} `$PKG_CONFIG --silence-errors --libs libusb-1.0 2>/dev/null`"
                        AC_LANG_PUSH([C])
                        AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <libusb.h>]],
                            [[libusb_init(NULL); (void)libusb_error_name(LIBUSB_SUCCESS);]])],
                            [AC_MSG_RESULT([yes])],
                            [AC_MSG_RESULT([no])
                             AS_IF([test x"${nut_cv_LIBUSB_0_1_VERSION}" != xnone],
                                [nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_0_1_VERSION}"
                                 nut_cv_usb_lib="(libusb-0.1)"
                                ],
                                [nut_cv_LIBUSB_VERSION="${nut_cv_LIBUSB_CONFIG_VERSION}"
                                 nut_cv_usb_lib="(libusb-0.1-config)"
                                ])
                             AC_MSG_NOTICE([libusb-1.0 ${nut_cv_LIBUSB_1_0_VERSION} is not usable by NUT (libusb 1.0.9+ needed), falling back to ${nut_cv_LIBUSB_VERSION} ${nut_cv_usb_lib}])
                            ])
                        AC_LANG_POP([C])
                        CFLAGS=""
                        LIBS=""
                    ]
                )
            ])

            AS_IF([test x"${nut_cv_LIBUSB_1_0_VERSION}" != xnone && test x"${nut_cv_usb_lib}" != x"(libusb-1.0)" ],
                [AC_MSG_NOTICE([libusb-1.0 support was detected, but another was chosen ${nut_cv_usb_lib}])]
            )

            dnl FIXME? Detect and report all CFLAGS/LIBS that we can,
            dnl and *then* pick one set of values to use?
            AS_CASE([${nut_cv_usb_lib}],
                ["(libusb-1.0)"], [
                    depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libusb-1.0 2>/dev/null`"
                    depLIBS="`$PKG_CONFIG --silence-errors --libs libusb-1.0 2>/dev/null`"
                    depCFLAGS_SOURCE="pkg-config(libusb-1.0)"
                    depLIBS_SOURCE="pkg-config(libusb-1.0)"
                    ],
                ["(libusb-0.1)"], [
                    depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libusb 2>/dev/null`"
                    depLIBS="`$PKG_CONFIG --silence-errors --libs libusb 2>/dev/null`"
                    depCFLAGS_SOURCE="pkg-config(libusb-0.1)"
                    depLIBS_SOURCE="pkg-config(libusb-0.1)"
                    ],
                ["(libusb-0.1-config)"], [
                    depCFLAGS="`$LIBUSB_CONFIG --cflags 2>/dev/null`"
                    depLIBS="`$LIBUSB_CONFIG --libs 2>/dev/null`"
                    depCFLAGS_SOURCE="${LIBUSB_CONFIG} program (libusb-0.1)"
                    depLIBS_SOURCE="${LIBUSB_CONFIG} program (libusb-0.1)"
                    ],
                [dnl default, for other versions or "none"
                    AC_MSG_WARN([Defaulting libusb configuration])
                    nut_cv_LIBUSB_VERSION="none"
                    depCFLAGS=""
                    depLIBS="-lusb"
                    depCFLAGS_SOURCE="default"
                    depLIBS_SOURCE="default"
                ]
            )

            dnl check optional user-provided values for cflags/ldflags
            dnl and publish what we end up using
            AC_MSG_CHECKING(for libusb cflags)
            AS_CASE([${nut_with_usb_includes}],
                [auto], [],	dnl Keep what we had found above
                    [depCFLAGS="${nut_with_usb_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for libusb ldflags)
            AS_CASE([${nut_with_usb_libs}],
                [auto], [],	dnl Keep what we had found above
                    [depLIBS="${nut_with_usb_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl Check if libusb is usable (a 1.0 too old for NUT was already
            dnl swapped for an available 0.1 above, when not chosen explicitly)
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            nut_cv_have_libusb_strerror=""
            AC_LANG_PUSH([C])
            AS_IF([test -n "${nut_cv_LIBUSB_VERSION}"], [
                dnl Test specifically for libusb-1.0 via pkg-config, else fall back below
                test -n "$PKG_CONFIG" \
                    && test x"${nut_cv_usb_lib}" = x"(libusb-1.0)" \
                    && $PKG_CONFIG --silence-errors --atleast-version=1.0 libusb-1.0 2>/dev/null
                AS_IF([test "$?" = "0"], [
                    dnl libusb 1.0: libusb_set_auto_detach_kernel_driver
                    AC_CHECK_HEADERS(libusb.h, [nut_cv_have_libusb=yes], [nut_cv_have_libusb=no], [AC_INCLUDES_DEFAULT])
                    AC_CHECK_FUNCS(libusb_init, [], [nut_cv_have_libusb=no])
                    dnl libusb_strerror() appeared in 1.0.16; older releases
                    dnl from 1.0.9 on have libusb_error_name() which we can
                    dnl use instead (see usb-common.h):
                    AC_CHECK_FUNCS(libusb_strerror, [], [nut_cv_have_libusb_strerror=no])
                    AS_IF([test "${nut_cv_have_libusb_strerror}" = "no"], [
                        AC_CHECK_FUNCS(libusb_error_name, [
                            AC_MSG_NOTICE([libusb_strerror() not found (libusb older than 1.0.16), error messages will show libusb_error_name() codes instead])
                        ], [
                            nut_cv_have_libusb=no
                            AC_MSG_WARN([neither libusb_strerror() nor libusb_error_name() found; libusb 1.0.9 or newer is needed to use libusb 1.0 API. See https://github.com/networkupstools/nut/issues/509])
                        ])
                    ])
                    AS_IF([test "${nut_cv_have_libusb}" = "yes"], [
                        dnl Absent before libusb 1.0.10 or so, not fatal:
                        AC_CHECK_FUNCS(libusb_get_version)

                        dnl This function is fairly old, but check for it anyway:
                        AC_CHECK_FUNCS(libusb_kernel_driver_active)
                        dnl Check for libusb "force driver unbind" availability
                        AC_CHECK_FUNCS(libusb_set_auto_detach_kernel_driver)
                        dnl libusb 1.0: libusb_detach_kernel_driver
                        dnl FreeBSD 10.1-10.3 have this, but not libusb_set_auto_detach_kernel_driver
                        AC_CHECK_FUNCS(libusb_detach_kernel_driver)
                        AC_CHECK_FUNCS(libusb_detach_kernel_driver_np)

                        dnl From libusb-0.1 - check these to have valid config.h definitions
                        dnl Note: confusingly, FreeBSD does find both as defined
                        dnl (despite being spread across usb.h and libusb.h),
                        dnl so our source code has to care :\
                        AC_CHECK_FUNCS(usb_detach_kernel_driver_np)
                    ])
                ], [
                    dnl libusb 0.1, or missing pkg-config :
                    AC_CHECK_HEADERS(usb.h, [nut_cv_have_libusb=yes], [nut_cv_have_libusb=no], [AC_INCLUDES_DEFAULT])
                    AC_CHECK_FUNCS(usb_init, [], [
                        dnl Some systems may just have libusb in their standard
                        dnl paths, but not the pkg-config or libusb-config data
                        AS_IF([test "${nut_cv_have_libusb}" = "yes" && test "$LIBUSB_VERSION" = "none" && test -z "$LIBS" -o x"$LIBS" = x"-lusb" ], [
                            AC_MSG_CHECKING([if libusb is just present in path])
                            depLIBS="-L/usr/lib -L/usr/local/lib -lusb"
                            unset ac_cv_func_usb_init || true
                            LIBS="${LIBS_ORIG} ${depLIBS}"
                            AC_CHECK_FUNCS(usb_init, [], [
                                AC_MSG_CHECKING([if libusb0 is just present in path])
                                depLIBS="$depLIBS"0
                                unset ac_cv_func_usb_init || true
                                LIBS="${LIBS_ORIG} ${depLIBS}"
                                AC_CHECK_FUNCS(usb_init, [nut_cv_usb_lib="(libusb-0.1)"], [nut_cv_have_libusb=no])
                                ])
                            AC_MSG_RESULT([${nut_cv_have_libusb}])
                        ], [
                            nut_cv_have_libusb=no
                        ])
                    ])
                    dnl Check for libusb "force driver unbind" availability
                    AS_IF([test "${nut_cv_have_libusb}" = "yes"], [
                        AC_CHECK_FUNCS(usb_detach_kernel_driver_np)

                        dnl From libusb-1.0 - check these to have valid config.h definitions
                        AC_CHECK_FUNCS(libusb_kernel_driver_active)
                        AC_CHECK_FUNCS(libusb_set_auto_detach_kernel_driver)
                        AC_CHECK_FUNCS(libusb_detach_kernel_driver)
                        AC_CHECK_FUNCS(libusb_detach_kernel_driver_np)
                    ])
                ])
            ], [
                nut_cv_have_libusb=no
            ])

            dnl NOTE: Currently not a configurable "--with-usb-busport" option,
            dnl but we may want to add it later. For now, auto-detected only.
            nut_cv_with_usb_busport=no
            nut_cv_SUN_LIBUSB=""
            AS_IF([test "${nut_cv_have_libusb}" = "yes"], [
                dnl ----------------------------------------------------------------------
                dnl additional USB-related checks

                dnl Solaris 10/11 USB handling (need librt and libusb runtime path)
                dnl Should we check for `uname -o == illumos` to avoid legacy here?
                dnl Or better yet, perform some active capability tests for need of
                dnl workarounds or not? e.g. OpenIndiana should include a capable
                dnl version of libusb-1.0.23+ tailored with NUT tests in mind...
                dnl HPUX, since v11, needs an explicit activation of pthreads
                dnl TODO: There are reports about FreeBSD error-code
                dnl handling in libusb-0.1 port returning "-1" always,
                dnl instead of differing codes like on other systems.
                dnl Should we check for that below?..
                dnl https://github.com/networkupstools/nut/issues/490
                AS_CASE(["${target_os}"],
                    [solaris2.1*], [
                        AC_MSG_CHECKING([for Solaris 10 / 11 specific configuration for usb drivers])
                        AC_SEARCH_LIBS(nanosleep, rt)
                        dnl Collect possibly updated dependencies after AC SEARCH LIBS:
                        AS_IF([test x"${LIBS}" != x"${LIBS_ORIG} ${depLIBS}"], [
                            AS_IF([test x = x"${LIBS_ORIG}"], [depLIBS="$LIBS"], [
                                depLIBS="`echo \"$LIBS\" | sed -e 's|'\"${LIBS_ORIG}\"'| |' -e 's|^ *||' -e 's| *$||'`"
                            ])
                        ])
                        depLIBS="-R/usr/sfw/lib ${depLIBS}"
                        dnl FIXME: Sun's libusb doesn't support timeout (so blocks notification)
                        dnl and need to call libusb close upon reconnection
                        dnl TODO: Somehow test for susceptible versions?
                        nut_cv_SUN_LIBUSB=1
                        AC_MSG_RESULT([${depLIBS}])
                        ],
                    [hpux11*], [
                        depCFLAGS="${depCFLAGS} -lpthread"
                        ]
                )

                CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
                LIBS="${LIBS_ORIG} ${depLIBS}"

                dnl AC_MSG_CHECKING([for libusb bus port support])
                dnl Per https://github.com/networkupstools/nut/issues/2043#issuecomment-1721856494 :
                dnl #if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000102)
                dnl DEFINE WITH_USB_BUSPORT
                dnl #endif
                AC_CHECK_FUNCS(libusb_get_port_number, [nut_cv_with_usb_busport=yes])
            ])
            AC_LANG_POP([C])

            dnl libusb-0.1 (libusb-win32 or libusb-compat) is not supported
            dnl for Windows builds, see https://github.com/networkupstools/nut/issues/1507
            AS_IF([test "${nut_cv_have_libusb}" = "yes"], [
                AS_CASE(["${target_os}"],
                    [*mingw*], [
                        AS_CASE(["${nut_cv_usb_lib}"],
                            ["(libusb-0.1)"|"(libusb-0.1-config)"], [
                                AC_MSG_WARN([libusb-0.1 ${nut_cv_usb_lib} is not supported for Windows builds, please use libusb-1.0 (with the WinUSB driver), see https://github.com/networkupstools/nut/issues/1507])
                                nut_cv_have_libusb=no
                                nut_cv_usb_lib=""
                            ]
                        )
                    ]
                )
            ])

            nut_cv_LIBUSB_CFLAGS=""
            nut_cv_LIBUSB_LIBS=""
            AS_IF([test "${nut_cv_have_libusb}" = "yes"], [
                nut_cv_LIBUSB_CFLAGS="${depCFLAGS}"
                nut_cv_LIBUSB_LIBS="${depLIBS}"
            ], [
                AS_CASE(["${nut_cv_with_usb}"],
                    [no|auto], [],
                    [yes|1.0|0.1|libusb-1.0|libusb-0.1],
                        [dnl Explicitly choosing a library implies 'yes' (i.e. fail if not found), not 'auto'.
                         AC_MSG_ERROR([USB drivers requested, but libusb not found.])
                        ]
                )
            ])

            AS_IF([test "${nut_cv_with_usb}" = "no"], [
                AS_IF([test -n "${nut_cv_usb_lib}" && test "${nut_cv_usb_lib}" != none], [
                    AC_MSG_NOTICE([libusb was detected ${nut_cv_usb_lib}, but a build without USB drivers was requested])
                ])
                nut_cv_usb_lib=""
            ], [
                nut_cv_with_usb="${nut_cv_have_libusb}"
            ])

            AS_IF([test x"${nut_enable_configure_debug}" = xyes], [
                AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) nut_cv_have_libusb='${nut_cv_have_libusb}'])
                AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) nut_cv_with_usb='${nut_cv_with_usb}'])
                AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) nut_cv_usb_lib='${nut_cv_usb_lib}'])
            ])

            dnl Note: AC DEFINE specifies a verbatim "value" so we pre-calculate it!
            dnl Source code should be careful to use "#if" and not "#ifdef" when
            dnl checking these values during the build. And both must be defined
            dnl with some value.
            nut_cv_SOPATH_LIBUSB0=""
            nut_cv_SOFILE_LIBUSB0=""
            nut_cv_SONAME_LIBUSB0=""
            nut_cv_SOPATH_LIBUSB1=""
            nut_cv_SOFILE_LIBUSB1=""
            nut_cv_SONAME_LIBUSB1=""
            AS_IF([test "${nut_cv_with_usb}" = "yes" && test "${nut_cv_usb_lib}" = "(libusb-1.0)"], [
                dnl Help usb if we can (nut-scanner etc.)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*usb*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBUSB1], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBUSB1}" && test -s "${nut_cv_SOPATH_LIBUSB1}"], [
                                nut_cv_SOFILE_LIBUSB1="`basename \"${nut_cv_SOPATH_LIBUSB1}\"`"
                                AX_SONAME_LIB([${nut_cv_SOPATH_LIBUSB1}], [nut_cv_SONAME_LIBUSB1], [])
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])

            AS_IF([test "${nut_cv_with_usb}" = "yes" && test "${nut_cv_usb_lib}" = "(libusb-0.1)" -o "${nut_cv_usb_lib}" = "(libusb-0.1-config)"], [
                dnl Help usb if we can (nut-scanner etc.)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*usb*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBUSB0], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBUSB0}" && test -s "${nut_cv_SOPATH_LIBUSB0}"], [
                                nut_cv_SOFILE_LIBUSB0="`basename \"${nut_cv_SOPATH_LIBUSB0}\"`"
                                AX_SONAME_LIB([${nut_cv_SOPATH_LIBUSB0}], [nut_cv_SONAME_LIBUSB0], [])
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBUSB_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBUSB_LIBS_SOURCE="${depLIBS_SOURCE}"

            dnl NOTE: This one may differ from initial build argument (e.g. "auto")
            AC_CACHE_VAL([nut_cv_with_usb], [])
            AC_CACHE_VAL([nut_cv_have_libusb], [])
            AC_CACHE_VAL([nut_cv_usb_lib], [])
            AC_CACHE_VAL([nut_cv_with_usb_busport], [])
            AC_CACHE_VAL([nut_cv_have_libusb_strerror], [])
            AC_CACHE_VAL([nut_cv_SUN_LIBUSB], [])

            AC_CACHE_VAL([nut_cv_LIBUSB_1_0_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBUSB_0_1_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBUSB_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBUSB_CONFIG_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBUSB_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBUSB_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBUSB_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBUSB_LIBS_SOURCE], [])

            AC_CACHE_VAL([nut_cv_SOPATH_LIBUSB0], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBUSB0], [])
            AC_CACHE_VAL([nut_cv_SOPATH_LIBUSB1], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBUSB1], [])

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
            nut_cv_checked_libusb="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libusb}" = xyes], [
            dnl NOTE: This one may differ from initial build argument (e.g. "auto")
            nut_with_usb="${nut_cv_with_usb}"

            nut_have_libusb="${nut_cv_have_libusb}"
            nut_with_usb_busport="${nut_cv_with_usb_busport}"
            nut_have_libusb_strerror="${nut_cv_have_libusb_strerror}"

            dnl Magic-format string to hold chosen libusb version and its config-source
            nut_usb_lib="${nut_cv_usb_lib}"
            SUN_LIBUSB="${nut_cv_SUN_LIBUSB}"

            LIBUSB_CFLAGS="${nut_cv_LIBUSB_CFLAGS}"
            LIBUSB_LIBS="${nut_cv_LIBUSB_LIBS}"

            dnl For nut-scanner style autoloading:
            SOPATH_LIBUSB0="${nut_cv_SOPATH_LIBUSB0}"
            SOFILE_LIBUSB0="${nut_cv_SOFILE_LIBUSB0}"
            SONAME_LIBUSB0="${nut_cv_SONAME_LIBUSB0}"
            SOPATH_LIBUSB1="${nut_cv_SOPATH_LIBUSB1}"
            SOFILE_LIBUSB1="${nut_cv_SOFILE_LIBUSB1}"
            SONAME_LIBUSB1="${nut_cv_SONAME_LIBUSB1}"

            dnl For troubleshooting of re-runs, mostly:
            LIBUSB_CFLAGS_SOURCE="${nut_cv_LIBUSB_CFLAGS_SOURCE}"
            LIBUSB_LIBS_SOURCE="${nut_cv_LIBUSB_LIBS_SOURCE}"

            LIBUSB_1_0_VERSION="${nut_cv_LIBUSB_1_0_VERSION}"
            LIBUSB_0_1_VERSION="${nut_cv_LIBUSB_0_1_VERSION}"
            LIBUSB_CONFIG_VERSION="${nut_cv_LIBUSB_CONFIG_VERSION}"
            LIBUSB_VERSION="${nut_cv_LIBUSB_VERSION}"

            AS_IF([test "${SUN_LIBUSB}" = "1"], [
                dnl See comments above
                AC_DEFINE(SUN_LIBUSB, 1, [Define to 1 for Sun version of the libusb.])
            ])

            AS_IF([test x"${nut_with_usb_busport}" = xyes], [
                AC_DEFINE(WITH_USB_BUSPORT, 1,
                    [Define to 1 for libusb versions where we can support "busport" USB matching value.])
            ], [
                AC_DEFINE(WITH_USB_BUSPORT, 0,
                    [Define to 1 for libusb versions where we can support "busport" USB matching value.])
            ])

            AS_IF([test "${nut_noncv_checked_libusb_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBUSB_CFLAGS}"
                LIBS="${nut_cv_LIBUSB_LIBS}"

                dnl Should restore the cached value and be done with it
                dnl For more details, see comments above
                AC_LANG_PUSH([C])
                AS_IF([test "${nut_with_usb}" = "yes" && test "${nut_usb_lib}" = "(libusb-1.0)"], [
                    dnl TOTHINK: test x"${LIBUSB_CFLAGS_SOURCE}" = x"pkg-config(libusb-1.0)" ?
                    dnl libusb 1.0 detected via pkg-config:
                    AC_CHECK_HEADERS(libusb.h, [], [nut_have_libusb=no], [AC_INCLUDES_DEFAULT])
                    AC_CHECK_FUNCS(libusb_init, [], [nut_have_libusb=no])
                    AC_CHECK_FUNCS(libusb_strerror, [], [nut_have_libusb_strerror=no])
                    AS_IF([test "${nut_have_libusb_strerror}" = "no"], [
                        AC_CHECK_FUNCS(libusb_error_name, [], [
                            nut_have_libusb=no
                            AC_MSG_WARN([neither libusb_strerror() nor libusb_error_name() found; libusb 1.0.9 or newer is needed to use libusb 1.0 API. See https://github.com/networkupstools/nut/issues/509])
                        ])
                    ])
                    AC_CHECK_FUNCS(libusb_get_version)
                ], [
                    dnl libusb 0.1, or missing pkg-config:
                    AC_CHECK_HEADERS(usb.h, [], [nut_cv_have_libusb=no], [AC_INCLUDES_DEFAULT])

                    dnl No elaborate re-detection like that done above.
                    dnl Just set the vars about the method with LIBS already chosen.
                    AC_CHECK_FUNCS(usb_init, [], [nut_cv_have_libusb=no])
                ])

                AS_IF([test "${nut_with_usb}" = "yes"], [
                    dnl Different platforms, different methods (absence not fatal):

                    dnl From libusb-0.1 - check these to have valid config.h definitions
                    dnl Note: confusingly, FreeBSD does find both as defined
                    dnl (despite being spread across usb.h and libusb.h),
                    dnl so our source code has to care :\
                    AC_CHECK_FUNCS(usb_detach_kernel_driver_np)

                    dnl From libusb-1.0 - check these to have valid config.h definitions
                    AC_CHECK_FUNCS(libusb_kernel_driver_active libusb_set_auto_detach_kernel_driver libusb_detach_kernel_driver libusb_detach_kernel_driver_np)

                    AC_CHECK_FUNCS(libusb_get_port_number)
                ])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                LIBS="${LIBS_ORIG}"
                unset CFLAGS_ORIG
                unset LIBS_ORIG
            ])

            AS_IF([test "${nut_with_usb}" = "yes" && test "${nut_usb_lib}" = "(libusb-1.0)"], [
                AC_DEFINE([WITH_LIBUSB_1_0], [1],
                    [Define to 1 for version 1.0 of the libusb (via pkg-config).])
                AS_IF([test -n "${SOPATH_LIBUSB1}" && test -s "${SOPATH_LIBUSB1}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBUSB1], ["${SOPATH_LIBUSB1}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBUSB1], ["${SOFILE_LIBUSB1}"], [Base file name of dynamic library on build system])
                    AS_IF([test -n "${SONAME_LIBUSB1}"], [
                        AC_DEFINE_UNQUOTED([SONAME_LIBUSB1], ["${SONAME_LIBUSB1}"], [SONAME (run-time linker name) of dynamic library on build system])
                    ])
                ])
            ], [
                AC_DEFINE([WITH_LIBUSB_1_0], [0],
                    [Define to 1 for version 1.0 of the libusb (via pkg-config).])
            ])

            AS_IF([test "${nut_with_usb}" = "yes" && test "${nut_usb_lib}" = "(libusb-0.1)" -o "${nut_usb_lib}" = "(libusb-0.1-config)"], [
                AC_DEFINE([WITH_LIBUSB_0_1], [1],
                    [Define to 1 for version 0.1 of the libusb (via pkg-config or libusb-config).])
                AS_IF([test -n "${SOPATH_LIBUSB0}" && test -s "${SOPATH_LIBUSB0}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBUSB0], ["${SOPATH_LIBUSB0}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBUSB0], ["${SOFILE_LIBUSB0}"], [Base file name of dynamic library on build system])
                    AS_IF([test -n "${SONAME_LIBUSB0}"], [
                        AC_DEFINE_UNQUOTED([SONAME_LIBUSB0], ["${SONAME_LIBUSB0}"], [SONAME (run-time linker name) of dynamic library on build system])
                    ])
                ])
            ], [
                AC_DEFINE([WITH_LIBUSB_0_1], [0],
                    [Define to 1 for version 0.1 of the libusb (via pkg-config or libusb-config).])
            ])

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libusb_now}" = no], [
                AC_MSG_NOTICE([libusb (cached): ${nut_have_libusb} (${nut_usb_lib})])
                AC_MSG_NOTICE([libusb (cached): cflags_source='${LIBUSB_CFLAGS_SOURCE}' libs_source='${LIBUSB_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libusb (cached): LIBUSB_CFLAGS='${LIBUSB_CFLAGS}'])
                AC_MSG_NOTICE([libusb (cached): LIBUSB_LIBS='${LIBUSB_LIBS}'])
                AC_MSG_NOTICE([libusb (cached) misc: SUN_LIBUSB='${SUN_LIBUSB}', version:'${LIBUSB_VERSION}', busport:'${nut_with_usb_busport}', strerror:'${nut_have_libusb_strerror}'])
                AC_MSG_NOTICE([libusb (cached) 0.1: version:'${LIBUSB_0_1_VERSION}', LIBUSB_CONFIG_VERSION:'${LIBUSB_CONFIG_VERSION}'])
                AC_MSG_NOTICE([libusb (cached) 0.1: SOFILE:'${nut_cv_SOFILE_LIBUSB0}', SOPATH:'${nut_cv_SOPATH_LIBUSB0}'])
                AC_MSG_NOTICE([libusb (cached) 1.0: version:'${LIBUSB_1_0_VERSION}'])
                AC_MSG_NOTICE([libusb (cached) 1.0: SOFILE:'${nut_cv_SOFILE_LIBUSB1}', SOPATH:'${nut_cv_SOPATH_LIBUSB1}'])
            ])
        ])
    ])
])
