dnl Check for LIBUSB 1.0 or 0.1 (and, if found, fill 'nut_usb_lib' with its
dnl approximate version) and its compiler flags. On success, set
dnl nut_have_libusb="yes" and set LIBUSB_CFLAGS and LIBUSB_LIBS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl By default, if both libusb 1.0 and libusb 0.1 are available and appear to be
dnl usable, libusb 1.0 takes precedence.
dnl An optional argument with value 'libusb-1.0' or 'libusb-0.1' can be used to
dnl restrict checks to a specific version.

AC_DEFUN([NUT_CHECK_LIBUSB],
[
if test -z "${nut_have_libusb_seen}"; then
	nut_have_libusb_seen=yes
	AC_REQUIRE([NUT_CHECK_PKGCONFIG])

	dnl Our USB matching relies on regex abilities
	AC_REQUIRE([NUT_CHECK_LIBREGEX])

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LIBS=""
	depCFLAGS=""
	depLIBS=""

	dnl Magic-format string to hold chosen libusb version and its config-source
	nut_usb_lib=""

	dnl TOTHINK: What if there are more than 0.1 and 1.0 to juggle?
	dnl TODO? Add libusb-compat (1.0 code with 0.1's API) to the mix?
	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING([for libusb-1.0 version via pkg-config])
		 LIBUSB_1_0_VERSION="`$PKG_CONFIG --silence-errors --modversion libusb-1.0 2>/dev/null`" \
		    && test -n "${LIBUSB_1_0_VERSION}" \
		    || LIBUSB_1_0_VERSION="none"
		 AC_MSG_RESULT([${LIBUSB_1_0_VERSION} found])

		 AC_MSG_CHECKING([for libusb(-0.1) version via pkg-config])
		 LIBUSB_0_1_VERSION="`$PKG_CONFIG --silence-errors --modversion libusb 2>/dev/null`" \
		    && test -n "${LIBUSB_0_1_VERSION}" \
		    || LIBUSB_0_1_VERSION="none"
		 AC_MSG_RESULT([${LIBUSB_0_1_VERSION} found])
		],
		[LIBUSB_0_1_VERSION="none"
		 LIBUSB_1_0_VERSION="none"
		 AC_MSG_NOTICE([can not check libusb settings via pkg-config])
		]
	)

	dnl Note: it seems the script was only shipped for libusb-0.1
	dnl So we don't separate into LIBUSB_0_1_CONFIG and LIBUSB_1_0_CONFIG
	AC_PATH_PROGS([LIBUSB_CONFIG], [libusb-config], [none])

	AC_ARG_WITH(libusb-config,
		AS_HELP_STRING([@<:@--with-libusb-config=/path/to/libusb-config@:>@],
			[path to program that reports LibUSB configuration]), dnl ...for LibUSB-0.1
		[
			AS_CASE(["${withval}"],
				[""], [], dnl empty arg
				[yes|no], [
					dnl MAYBE bump preference of script over pkg-config?
					AC_MSG_ERROR([invalid option --with(out)-libusb-config - see docs/configure.txt])
				],
				[dnl default
				LIBUSB_CONFIG="${withval}"
				]
			)
		]
	)

	AS_IF([test x"${LIBUSB_CONFIG}" != xnone],
		[AC_MSG_CHECKING([via ${LIBUSB_CONFIG}])
		 LIBUSB_CONFIG_VERSION="`$LIBUSB_CONFIG --version 2>/dev/null`" \
			&& test -n "${LIBUSB_CONFIG_VERSION}" \
			|| LIBUSB_CONFIG_VERSION="none"
		 AC_MSG_RESULT([${LIBUSB_CONFIG_VERSION} found])
		], [LIBUSB_CONFIG_VERSION="none"]
	)

	dnl By default, prefer newest available, and if anything is known
	dnl to pkg-config, prefer that. Otherwise, fall back to script data:
	AS_IF([test x"${LIBUSB_1_0_VERSION}" != xnone],
		[LIBUSB_VERSION="${LIBUSB_1_0_VERSION}"
		 nut_usb_lib="(libusb-1.0)"
		 dnl ...except on Windows, where we support libusb-0.1(-compat)
		 dnl better so far (allow manual specification though, to let
		 dnl someone finally develop the on-par support), see also
		 dnl https://github.com/networkupstools/nut/issues/1507
		 dnl Note this may upset detection of libmodbus RTU USB support.
		 AS_IF([test x"${LIBUSB_0_1_VERSION}" != xnone], [
			AS_CASE(["${target_os}"],
				[*mingw*], [
					AS_IF([test x"$build" = x"$host"], [
						AS_IF([test "${nut_with_modbus_and_usb}" = "yes"], [
							AC_MSG_NOTICE(["Normally" mingw/MSYS2 native builds prefer libusb-0.1(-compat) over libusb-1.0 if both are available, but you requested --with-modbus+usb so preferring libusb-1.0 in this build])
						], [
							AC_MSG_NOTICE([mingw/MSYS2 native builds prefer libusb-0.1(-compat) over libusb-1.0 if both are available until https://github.com/networkupstools/nut/issues/1507 is solved])
							LIBUSB_VERSION="${LIBUSB_0_1_VERSION}"
							nut_usb_lib="(libusb-0.1)"
						])
						],[
						AC_MSG_NOTICE([mingw cross-builds prefer libusb-1.0 over libusb-0.1(-compat) if both are available])
						])
					])
				]
			)
		],
		[AS_IF([test x"${LIBUSB_0_1_VERSION}" != xnone],
			[LIBUSB_VERSION="${LIBUSB_0_1_VERSION}"
			 nut_usb_lib="(libusb-0.1)"
			],
			[LIBUSB_VERSION="${LIBUSB_CONFIG_VERSION}"
			 AS_IF([test x"${LIBUSB_CONFIG_VERSION}" != xnone],
				[dnl TODO: This assumes 0.1; check for 1.0+ somehow?
				 nut_usb_lib="(libusb-0.1-config)"],
				[nut_usb_lib=""]
			)]
		)]
	)

	dnl Pick up the default or caller-provided choice here from
	dnl NUT_ARG_WITH(usb, ...) in the main configure.ac script
	AC_MSG_CHECKING([for libusb preferred version])
	AS_CASE(["${nut_with_usb}"],
		[auto], [], dnl Use preference picked above
		[yes],  [], dnl Use preference from above, fail in the end if none found
		[no],   [], dnl Try to find, report in the end if that is discarded; TODO: not waste time?
		[libusb-1.0|1.0], [
			dnl NOTE: Assuming there is no libusb-config-1.0 or similar script, never saw one
			AS_IF([test x"${LIBUSB_1_0_VERSION}" = xnone],
				[AC_MSG_ERROR([option --with-usb=${withval} was required, but this library version was not detected])
				])
			LIBUSB_VERSION="${LIBUSB_1_0_VERSION}"
			nut_usb_lib="(libusb-1.0)"
			],
		[libusb-0.1|0.1], [
			AS_IF([test x"${LIBUSB_0_1_VERSION}" = xnone \
				&& test x"${LIBUSB_CONFIG_VERSION}" = xnone],
				[AC_MSG_ERROR([option --with-usb=${withval} was required, but this library version was not detected])
				])
			AS_IF([test x"${LIBUSB_0_1_VERSION}" != xnone],
				[LIBUSB_VERSION="${LIBUSB_0_1_VERSION}"
				 nut_usb_lib="(libusb-0.1)"
				],
				[LIBUSB_VERSION="${LIBUSB_CONFIG_VERSION}"
				 nut_usb_lib="(libusb-0.1-config)"
				])
			],
		[dnl default
			AC_MSG_ERROR([invalid option value --with-usb=${withval} - see docs/configure.txt])
			]
	)
	AC_MSG_RESULT([${LIBUSB_VERSION} ${nut_usb_lib}])

	AS_IF([test x"${LIBUSB_1_0_VERSION}" != xnone && test x"${nut_usb_lib}" != x"(libusb-1.0)" ],
		[AC_MSG_NOTICE([libusb-1.0 support was detected, but another was chosen ${nut_usb_lib}])]
	)

	dnl FIXME? Detect and report all CFLAGS/LIBS that we can,
	dnl and *then* pick one set of values to use?
	AS_CASE([${nut_usb_lib}],
		["(libusb-1.0)"], [
			depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libusb-1.0 2>/dev/null`"
			depLIBS="`$PKG_CONFIG --silence-errors --libs libusb-1.0 2>/dev/null`"
			],
		["(libusb-0.1)"], [
			depCFLAGS="`$PKG_CONFIG --silence-errors --cflags libusb 2>/dev/null`"
			depLIBS="`$PKG_CONFIG --silence-errors --libs libusb 2>/dev/null`"
			],
		["(libusb-0.1-config)"], [
			depCFLAGS="`$LIBUSB_CONFIG --cflags 2>/dev/null`"
			depLIBS="`$LIBUSB_CONFIG --libs 2>/dev/null`"
			],
		[dnl default, for other versions or "none"
			AC_MSG_WARN([Defaulting libusb configuration])
			LIBUSB_VERSION="none"
			depCFLAGS=""
			depLIBS="-lusb"
		]
	)

	dnl check optional user-provided values for cflags/ldflags
	dnl and publish what we end up using
	AC_MSG_CHECKING(for libusb cflags)
	AC_ARG_WITH(usb-includes,
		AS_HELP_STRING([@<:@--with-usb-includes=CFLAGS@:>@], [include flags for the libusb library]),
	[
		AS_CASE(["${withval}"],
			[yes|no], [
				AC_MSG_ERROR(invalid option --with(out)-usb-includes - see docs/configure.txt)
			],
			[dnl default
				depCFLAGS="${withval}"
			]
		)
	], [])
	AC_MSG_RESULT([${depCFLAGS}])

	AC_MSG_CHECKING(for libusb ldflags)
	AC_ARG_WITH(usb-libs,
		AS_HELP_STRING([@<:@--with-usb-libs=LIBS@:>@], [linker flags for the libusb library]),
	[
		AS_CASE(["${withval}"],
			[yes|no], [
				AC_MSG_ERROR(invalid option --with(out)-usb-libs - see docs/configure.txt)
			],
			[dnl default
				depLIBS="${withval}"
			]
		)
	], [])
	AC_MSG_RESULT([${depLIBS}])

	dnl TODO: Consult chosen nut_usb_lib value and/or nut_with_usb argument
	dnl (with "auto" we may use a 0.1 if present and working while a 1.0 is
	dnl present but useless)
	dnl Check if libusb is usable
	CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
	LIBS="${LIBS_ORIG} ${depLIBS}"
	AC_LANG_PUSH([C])
	if test -n "${LIBUSB_VERSION}"; then
		dnl Test specifically for libusb-1.0 via pkg-config, else fall back below
		test -n "$PKG_CONFIG" \
			&& test x"${nut_usb_lib}" = x"(libusb-1.0)" \
			&& $PKG_CONFIG --silence-errors --atleast-version=1.0 libusb-1.0 2>/dev/null
		if test "$?" = "0"; then
			dnl libusb 1.0: libusb_set_auto_detach_kernel_driver
			AC_CHECK_HEADERS(libusb.h, [nut_have_libusb=yes], [nut_have_libusb=no], [AC_INCLUDES_DEFAULT])
			AC_CHECK_FUNCS(libusb_init, [], [nut_have_libusb=no])
			AC_CHECK_FUNCS(libusb_strerror, [], [nut_have_libusb=no; nut_have_libusb_strerror=no])
			if test "${nut_have_libusb_strerror}" = "no"; then
				AC_MSG_WARN([libusb_strerror() not found; install libusbx to use libusb 1.0 API. See https://github.com/networkupstools/nut/issues/509])
			fi
			if test "${nut_have_libusb}" = "yes"; then
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
			fi
		else
			dnl libusb 0.1, or missing pkg-config :
			AC_CHECK_HEADERS(usb.h, [nut_have_libusb=yes], [
				nut_have_libusb=no
				dnl Per https://sourceforge.net/projects/libusb-win32/files/libusb-win32-releases/1.2.6.0/
				dnl this project (used among alternatives in MSYS2/MinGW builds)
				dnl uses a different include filename to avoid conflict with
				dnl a WDK header:
				AS_CASE(["${target_os}"],
					[*mingw*], [
						AC_MSG_NOTICE([try alternate header name for mingw builds with libusb-win32])
						AC_CHECK_HEADERS(lusb0_usb.h, [
							nut_usb_lib="(libusb-0.1)"
							nut_have_libusb=yes
						], [], [AC_INCLUDES_DEFAULT])
					])
				],
				[AC_INCLUDES_DEFAULT])
			AC_CHECK_FUNCS(usb_init, [], [
				dnl Some systems may just have libusb in their standard
				dnl paths, but not the pkg-config or libusb-config data
				AS_IF([test "${nut_have_libusb}" = "yes" && test "$LIBUSB_VERSION" = "none" && test -z "$LIBS" -o x"$LIBS" = x"-lusb" ],
					[AC_MSG_CHECKING([if libusb is just present in path])
					 depLIBS="-L/usr/lib -L/usr/local/lib -lusb"
					 dnl TODO: Detect bitness for trying /mingw32 or /usr/$ARCH as well?
					 dnl This currently caters to mingw-w64-x86_64-libusb-win32 of MSYS2:
					 AS_CASE(["${target_os}"],
						[*mingw*], [depLIBS="-L/mingw64/lib $depLIBS"])
					 unset ac_cv_func_usb_init || true
					 LIBS="${LIBS_ORIG} ${depLIBS}"
					 AC_CHECK_FUNCS(usb_init, [], [
						AC_MSG_CHECKING([if libusb0 is just present in path])
						depLIBS="$depLIBS"0
						unset ac_cv_func_usb_init || true
						LIBS="${LIBS_ORIG} ${depLIBS}"
						AC_CHECK_FUNCS(usb_init, [nut_usb_lib="(libusb-0.1)"], [nut_have_libusb=no])
						])
					 AC_MSG_RESULT([${nut_have_libusb}])
					], [nut_have_libusb=no]
				)]
			)
			dnl Check for libusb "force driver unbind" availability
			if test "${nut_have_libusb}" = "yes"; then
				AC_CHECK_FUNCS(usb_detach_kernel_driver_np)

				dnl From libusb-1.0 - check these to have valid config.h definitions
				AC_CHECK_FUNCS(libusb_kernel_driver_active)
				AC_CHECK_FUNCS(libusb_set_auto_detach_kernel_driver)
				AC_CHECK_FUNCS(libusb_detach_kernel_driver)
				AC_CHECK_FUNCS(libusb_detach_kernel_driver_np)
			fi
		fi
	else
		nut_have_libusb=no
	fi

	nut_with_usb_busport=no
	AS_IF([test "${nut_have_libusb}" = "yes"], [
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
						depLIBS="`echo "$LIBS" | sed -e 's|'"${LIBS_ORIG}"'| |' -e 's|^ *||' -e 's| *$||'`"
					])
				])
				depLIBS="-R/usr/sfw/lib ${depLIBS}"
				dnl FIXME: Sun's libusb doesn't support timeout (so blocks notification)
				dnl and need to call libusb close upon reconnection
				dnl TODO: Somehow test for susceptible versions?
				AC_DEFINE(SUN_LIBUSB, 1, [Define to 1 for Sun version of the libusb.])
				SUN_LIBUSB=1
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
		AC_CHECK_FUNCS(libusb_get_port_number, [nut_with_usb_busport=yes])
	])
	AC_LANG_POP([C])

	AS_IF([test x"${nut_with_usb_busport}" = xyes], [
		AC_DEFINE(WITH_USB_BUSPORT, 1,
			[Define to 1 for libusb versions where we can support "busport" USB matching value.])
	], [
		AC_DEFINE(WITH_USB_BUSPORT, 0,
			[Define to 1 for libusb versions where we can support "busport" USB matching value.])
	])

	AS_IF([test "${nut_have_libusb}" = "yes"], [
		LIBUSB_CFLAGS="${depCFLAGS}"
		LIBUSB_LIBS="${depLIBS}"
	], [
		AS_CASE(["${nut_with_usb}"],
			[no|auto], [],
			[yes|1.0|0.1|libusb-1.0|libusb-0.1],
				[dnl Explicitly choosing a library implies 'yes' (i.e. fail if not found), not 'auto'.
				 AC_MSG_ERROR([USB drivers requested, but libusb not found.])
				]
		)
	])

	if test "${nut_with_usb}" = "no"; then
		if test -n "${nut_usb_lib}" && test "${nut_usb_lib}" != none ; then
			AC_MSG_NOTICE([libusb was detected ${nut_usb_lib}, but a build without USB drivers was requested])
		fi
		nut_usb_lib=""
	else
		nut_with_usb="${nut_have_libusb}"
	fi

	AS_IF([test x"${nut_enable_configure_debug}" = xyes], [
		AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) nut_have_libusb='${nut_have_libusb}'])
		AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) nut_with_usb='${nut_with_usb}'])
		AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) nut_usb_lib='${nut_usb_lib}'])
	])

	dnl Note: AC_DEFINE specifies a verbatim "value" so we pre-calculate it!
	dnl Source code should be careful to use "#if" and not "#ifdef" when
	dnl checking these values during the build. And both must be defined
	dnl with some value.
	AS_IF([test "${nut_with_usb}" = "yes" && test "${nut_usb_lib}" = "(libusb-1.0)"],
		[AC_DEFINE([WITH_LIBUSB_1_0], [1],
			[Define to 1 for version 1.0 of the libusb (via pkg-config).])

		 dnl Help ltdl if we can (nut-scanner etc.)
		 for TOKEN in $depLIBS ; do
			AS_CASE(["${TOKEN}"],
				[-l*usb*], [
					AX_REALPATH_LIB([${TOKEN}], [SOPATH_LIBUSB1], [])
					AS_IF([test -n "${SOPATH_LIBUSB1}" && test -s "${SOPATH_LIBUSB1}"], [
						AC_DEFINE_UNQUOTED([SOPATH_LIBUSB1],["${SOPATH_LIBUSB1}"],[Path to dynamic library on build system])
						SOFILE_LIBUSB1="`basename "$SOPATH_LIBUSB1"`"
						AC_DEFINE_UNQUOTED([SOFILE_LIBUSB1],["${SOFILE_LIBUSB1}"],[Base file name of dynamic library on build system])
						break
					])
				]
			)
		 done
		 unset TOKEN
		],
		[AC_DEFINE([WITH_LIBUSB_1_0], [0],
			[Define to 1 for version 1.0 of the libusb (via pkg-config).])]
	)

	AS_IF([test "${nut_with_usb}" = "yes" && test "${nut_usb_lib}" = "(libusb-0.1)" -o "${nut_usb_lib}" = "(libusb-0.1-config)"],
		[AC_DEFINE([WITH_LIBUSB_0_1], [1],
			[Define to 1 for version 0.1 of the libusb (via pkg-config or libusb-config).])

		 dnl Help ltdl if we can (nut-scanner etc.)
		 for TOKEN in $depLIBS ; do
			AS_CASE(["${TOKEN}"],
				[-l*usb*], [
					AX_REALPATH_LIB([${TOKEN}], [SOPATH_LIBUSB0], [])
					AS_IF([test -n "${SOPATH_LIBUSB0}" && test -s "${SOPATH_LIBUSB0}"], [
						AC_DEFINE_UNQUOTED([SOPATH_LIBUSB0],["${SOPATH_LIBUSB0}"],[Path to dynamic library on build system])
						SOFILE_LIBUSB0="`basename "$SOPATH_LIBUSB0"`"
						AC_DEFINE_UNQUOTED([SOFILE_LIBUSB0],["${SOFILE_LIBUSB0}"],[Base file name of dynamic library on build system])
						break
					])
				]
			)
		 done
		 unset TOKEN
		],
		[AC_DEFINE([WITH_LIBUSB_0_1], [0],
			[Define to 1 for version 0.1 of the libusb (via pkg-config or libusb-config).])]
	)

	unset depCFLAGS
	unset depLIBS

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
