dnl Check for LIBPOWERMAN compiler flags. On success, set nut_have_libpowerman="yes"
dnl and set LIBPOWERMAN_CFLAGS and LIBPOWERMAN_LIBS. On failure, set
dnl nut_have_libpowerman="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_PKGCONFIG],
[
  AS_IF([test -z "${nut_have_pkg_config_seen}"], [
	nut_have_pkg_config_seen=yes

	dnl Note that PKG_CONFIG may be a filename, path,
	dnl or either with args - so no quoting here
	AC_MSG_CHECKING([whether usable PKG_CONFIG was already detected by autoconf])
	AS_IF([test -n "${PKG_CONFIG-}" && test x"${PKG_CONFIG-}" != x"false" && $PKG_CONFIG --help 2>&1 | grep -E '(--cflags|--libs)' >/dev/null],
		[AC_MSG_RESULT([yes: ${PKG_CONFIG}])
		 have_PKG_CONFIG=yes
		],
		[AC_MSG_RESULT([no])
		 PKG_CONFIG=false
		 have_PKG_CONFIG=no
		]
	)

	AS_IF([test x"${PKG_CONFIG-}" = x"false"],
		[dnl Some systems have older autotools without direct macro support for PKG_CONF*
		have_PKG_CONFIG=yes
		AC_PATH_PROG(dummy_PKG_CONFIG, pkg-config)

		AC_ARG_WITH(pkg-config,
			AS_HELP_STRING([--with-pkg-config=/path/to/pkg-config],
				[path to program that reports development package configuration]),
		[
			case "${withval}" in
			"") ;;
			yes|no)
				AC_MSG_ERROR(invalid option --with(out)-pkg-config - see docs/configure.txt)
				;;
			*)
				dummy_PKG_CONFIG="${withval}"
				;;
			esac
		])

		AC_MSG_CHECKING([whether usable PKG_CONFIG is present in PATH or was set by caller])
		AS_IF([test x"$dummy_PKG_CONFIG" = xno || test -z "$dummy_PKG_CONFIG"],
			[AC_MSG_RESULT([no])
			 PKG_CONFIG=false
			 have_PKG_CONFIG=no
			],
			[AS_IF([$dummy_PKG_CONFIG --help 2>&1 | grep -E '(--cflags|--libs)' >/dev/null],
				[AC_MSG_RESULT([yes: ${dummy_PKG_CONFIG}])
				 have_PKG_CONFIG=yes
				 PKG_CONFIG="$dummy_PKG_CONFIG"
				],
				[AC_MSG_RESULT([no])
				 PKG_CONFIG=false
				 have_PKG_CONFIG=no
				]
			)]
		)]
	)

	have_PKG_CONFIG_MACROS=no
	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_NOTICE([checking for autoconf macro support of pkg-config (${PKG_CONFIG})])
		 dummy_RES=0
		 ifdef([PKG_PROG_PKG_CONFIG], [], [dummy_RES=1])
		 ifdef([PKG_CHECK_MODULES], [], [dummy_RES=2])
		 AS_IF([test "${dummy_RES}" = 0],
			[AC_MSG_NOTICE([checking for autoconf macro support of pkg-config module checker])
			 dnl The m4 macro below may be not defined if pkg-config package is not
			 dnl installed. Use of ifdef (here and below for e.g. CPPUNIT check)
			 dnl allows to avoid shell syntax errors in generated configure script
			 dnl by defining a dummy macro in-place.
			 ifdef([PKG_CHECK_MODULES], [], [AC_DEFUN([PKG_CHECK_MODULES], [false])])
			 PKG_CHECK_MODULES([dummy_PKG_CONFIG], [pkg-config], [have_PKG_CONFIG_MACROS=yes])
			]
		)]
	)

	AS_IF([test x"$have_PKG_CONFIG" = xno],
		[AC_MSG_WARN([pkg-config program is needed to look for further dependencies (will be skipped)])
		 PKG_CONFIG="false"
		],
		[AS_IF([test x"$have_PKG_CONFIG_MACROS" = xno],
			[AC_MSG_WARN([pkg-config macros are needed to look for further dependencies, but in some cases pkg-config program can be used directly])])

		 AC_MSG_CHECKING([for pkg-config envvar PKG_CONFIG_PATH (if passed to configure script or exported in shell session)])
		 AC_MSG_RESULT([${PKG_CONFIG_PATH}])

		 AC_MSG_CHECKING([for pkg-config reported pc_path])
		 myPKG_CONFIG_PC_PATH="`pkg-config --variable pc_path pkg-config`" || myPKG_CONFIG_PC_PATH=""
		 AC_MSG_RESULT([${myPKG_CONFIG_PC_PATH}])

		 dnl # The piece below is inspired by https://github.com/wxWidgets/wxWidgets/commit/7899850496ba2707c41cc534b51d14ac5b9fdbba
		 dnl When cross-compiling for another system from Linux, don't use .pc files on
		 dnl the build system, they are at best useless and can be harmful (e.g. they
		 dnl may define options inappropriate for the cross-build, resulting in the
		 dnl failure of all the subsequent tests).
		 dnl
		 dnl However do use .pc files for the host libraries that can be found by the
		 dnl host-specific pkg-config if it was found by PKG_PROG_PKG_CONFIG above.
		 AS_IF([test x"$build" != x"$host"], [
			AS_CASE(["$PKG_CONFIG"],
				[*/"$host"-pkg-config], [],
				[AC_MSG_WARN([Not using native pkg-config when cross-compiling.])

				 dnl pkg.m4 forbids the use of PKG_XXX, so undo it here
				 dnl to avoid autoconf errors.
				 m4_pattern_allow([PKG_CONFIG_LIBDIR])

				 dnl If pkg-config libdir is already defined, we suppose that
				 dnl callers know what they're doing and leave it alone, but
				 dnl if not, set it to a path in which no .pc files will be found.
				 AS_IF([test x"$PKG_CONFIG_LIBDIR" = x], [
					AC_MSG_WARN([Exporting a hack PKG_CONFIG_LIBDIR to avoid search for .pc files in host paths])
					PKG_CONFIG_LIBDIR=/dev/null
					export PKG_CONFIG_LIBDIR
					])
				])
			])
		]
	)

  ]) dnl if nut_have_pkg_config_seen
])
