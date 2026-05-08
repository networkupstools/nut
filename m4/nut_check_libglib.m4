dnl Check for LIBGLIB (used by nut-upower driver) and related LIBGIO (optionally
dnl used by nut-scanner) compiler and linker flags.
dnl On success, set nut_have_libglib="yes" and set LIBGLIB_CFLAGS and LIBGLIB_LIBS.
dnl On failure, set nut_have_libglib="no".
dnl Similarly for LIBGIO_FLAGS, LIBGIO_LIBS and nut_have_libgio.
dnl This macro can be run multiple times, but will do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBGLIB],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libglib_seen}"], [
        nut_have_libglib_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([glib], [auto])
        NUT_ARG_WITH_LIBOPTS_LIBS([glib], [auto])

        NUT_ARG_WITH_LIBOPTS_INCLUDES([gio], [auto])
        NUT_ARG_WITH_LIBOPTS_LIBS([gio], [auto])

        nut_noncv_checked_libglib_now=no
        AC_CACHE_VAL([nut_cv_checked_libglib], [
            nut_noncv_checked_libglib_now=yes

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

            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                dnl See which version of the glib/gio library (if any) is installed
                AC_MSG_CHECKING(for glib-2.0 version via pkg-config (2.26.0 minimum required))
                nut_cv_LIBGLIB_VERSION="`$PKG_CONFIG --silence-errors --modversion glib-2.0 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_LIBGLIB_VERSION}"], [
                    nut_cv_LIBGLIB_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_LIBGLIB_VERSION} found)
            ], [
                nut_cv_LIBGLIB_VERSION="none"
                AC_MSG_NOTICE([can not check glib-2.0 settings via pkg-config])
            ])

            AC_MSG_CHECKING(for glib-2.0 cflags)
            AS_CASE([${nut_with_glib_includes}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                            [   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags glib-2.0 2>/dev/null`" \
                                  && depCFLAGS_SOURCE="pkg-config" ; } \
                             || { depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include" \
                                  && depCFLAGS_SOURCE="default" ; }
                            ], [
                                depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include"
                                depCFLAGS_SOURCE="default"
                            ])
                    ],
                        [depCFLAGS="${nut_with_glib_includes}"
                         depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for glib-2.0 ldflags)
            AS_CASE([${nut_with_glib_libs}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                            [   { depLIBS="`$PKG_CONFIG --silence-errors --libs glib-2.0 2>/dev/null`" \
                                  && depLIBS_SOURCE="pkg-config" ; } \
                                 || { depLIBS="-lgobject-2.0 -lglib-2.0" \
                                      && depLIBS_SOURCE="default" ; }
                            ], [
                                depLIBS="-lgobject-2.0 -lglib-2.0"
                                depLIBS_SOURCE="default"
                            ])
                    ],
                        [depLIBS="${nut_with_glib_libs}"
                         depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if glib-2.0 is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(glib.h, [nut_cv_have_libglib=yes], [nut_cv_have_libglib=no], [AC_INCLUDES_DEFAULT])

            nut_cv_LIBGLIB_CFLAGS=""
            nut_cv_LIBGLIB_LIBS=""
            nut_cv_SOFILE_LIBGLIB=""
            nut_cv_SOPATH_LIBGLIB=""
            AS_IF([test "${nut_cv_have_libglib}" = "yes"], [
                dnl GLib headers seem incorrect and offensive to many compilers
                dnl (starting names with underscores and capital characters,
                dnl varying support for attributes, method pointer mismatches).
                dnl There is nothing NUT can do about it, except telling the
                dnl compiler that we take these headers from the system as they
                dnl are, so strict checks should not apply to them.
                dnl On newer releases (2025+) the headers and CLANG seem to work
                dnl together out of the box, but during the decade before this is
                dnl troublesome.
                AS_IF([test "${CLANGCC}" = "yes" || test "${GCC}" = "yes"], [
                    myGLIB_CFLAGS=""
                    for TOKEN in ${depCFLAGS} ; do
                        AS_CASE(["${TOKEN}"],
                            [-I/*], [
                                _IDIR="`echo \"${TOKEN}\" | sed 's/^-I//'`"
                                AS_IF([echo " ${depCFLAGS}" | ${EGREP} " -isystem *${_IDIR}" >/dev/null],
                                    [myGLIB_CFLAGS="${myGLIB_CFLAGS} ${TOKEN}"],
                                    [myGLIB_CFLAGS="${myGLIB_CFLAGS} -isystem ${_IDIR} ${TOKEN}"]
                                )],
                                [myGLIB_CFLAGS="${myGLIB_CFLAGS} ${TOKEN}"]
                        )
                    done
                    unset TOKEN
                    unset _IDIR
                    myGLIB_CFLAGS="`echo \"${myGLIB_CFLAGS}\" | sed 's/^ *//'`"

                    AS_IF([test x"${depCFLAGS}" != x -a x"${depCFLAGS}" != x"${myGLIB_CFLAGS}"], [
                        AC_MSG_NOTICE([Patched libglib CFLAGS to declare -isystem])
                        AS_IF([test x"${nut_enable_configure_debug}" = xyes], [
                            AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) old: ${depCFLAGS}])
                            AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) new: ${myGLIB_CFLAGS}])
                        ])
                        depCFLAGS="${myGLIB_CFLAGS}"
                    ])
                    unset myGLIB_CFLAGS
                ])

                nut_cv_LIBGLIB_CFLAGS="${depCFLAGS}"
                nut_cv_LIBGLIB_LIBS="${depLIBS}"

                dnl Help ltdl if we can (nut-scanner etc.)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*glib*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBGLIB], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBGLIB}" && test -s "${nut_cv_SOPATH_LIBGLIB}"], [
                                nut_cv_SOFILE_LIBGLIB="`basename \"${nut_cv_SOPATH_LIBGLIB}\"`"
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])
            nut_cv_LIBGLIB_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBGLIB_LIBS_SOURCE="${depLIBS_SOURCE}"

            dnl ///////////////////////////////////////
            dnl //          Same for libgio          //
            dnl ///////////////////////////////////////

            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                dnl See which version of the glib/gio library (if any) is installed
                AC_MSG_CHECKING(for gio-2.0 version via pkg-config (2.26.0 minimum required))
                nut_cv_LIBGIO_VERSION="`$PKG_CONFIG --silence-errors --modversion gio-2.0 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_LIBGIO_VERSION}"], [
                    nut_cv_LIBGIO_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_LIBGIO_VERSION} found)
            ], [
                nut_cv_LIBGIO_VERSION="none"
                AC_MSG_NOTICE([can not check gio-2.0 settings via pkg-config])
            ])

            AC_MSG_CHECKING(for gio-2.0 cflags)
            AS_CASE([${nut_with_gio_includes}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                            [   { depCFLAGS="`$PKG_CONFIG --silence-errors --cflags gio-2.0 2>/dev/null`" \
                                  && depCFLAGS_SOURCE="pkg-config" ; } \
                             || { depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include" \
                                  && depCFLAGS_SOURCE="default" ; }
                            ], [
                                depCFLAGS="-I/usr/local/include/glib-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include"
                                depCFLAGS_SOURCE="default"
                            ])
                    ],
                        [depCFLAGS="${nut_with_gio_includes}"
                         depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for gio-2.0 ldflags)
            AS_CASE([${nut_with_gio_libs}],
                [auto],	[AS_IF([test x"$have_PKG_CONFIG" = xyes],
                            [   { depLIBS="`$PKG_CONFIG --silence-errors --libs gio-2.0 2>/dev/null`" \
                                  && depLIBS_SOURCE="pkg-config" ; } \
                             || { depLIBS="-lgio-2.0 -lgobject-2.0 -lglib-2.0" \
                                  && depLIBS_SOURCE="default" ; }
                            ], [
                                depLIBS="-lgio-2.0 -lgobject-2.0 -lglib-2.0"
                                depLIBS_SOURCE="default"
                            ])
                    ],
                        [depLIBS="${nut_with_gio_libs}"
                         depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if gio-2.0 is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            AC_CHECK_HEADERS(gio/gio.h, [nut_cv_have_libgio=yes], [nut_cv_have_libgio=no], [AC_INCLUDES_DEFAULT])
            dnl AC_CHECK_FUNCS(g_bus_get_sync, [], [nut_cv_have_libgio=no])

            nut_cv_LIBGIO_CFLAGS=""
            nut_cv_LIBGIO_LIBS=""
            nut_cv_SOFILE_LIBGIO=""
            nut_cv_SOPATH_LIBGIO=""
            AS_IF([test "${nut_cv_have_libgio}" = "yes"], [
                dnl GLib headers seem incorrect and offensive to many compilers
                dnl (see details in big comment above).
                AS_IF([test "${CLANGCC}" = "yes" || test "${GCC}" = "yes"], [
                    myGIO_CFLAGS=""
                    for TOKEN in ${depCFLAGS} ; do
                        AS_CASE(["${TOKEN}"],
                            [-I/*], [
                                _IDIR="`echo \"${TOKEN}\" | sed 's/^-I//'`"
                                AS_IF([echo " ${depCFLAGS}" | ${EGREP} " -isystem *${_IDIR}" >/dev/null],
                                    [myGIO_CFLAGS="${myGIO_CFLAGS} ${TOKEN}"],
                                    [myGIO_CFLAGS="${myGIO_CFLAGS} -isystem ${_IDIR} ${TOKEN}"]
                                )],
                                [myGIO_CFLAGS="${myGIO_CFLAGS} ${TOKEN}"]
                        )
                    done
                    unset TOKEN
                    unset _IDIR
                    myGIO_CFLAGS="`echo \"${myGIO_CFLAGS}\" | sed 's/^ *//'`"

                    AS_IF([test x"${depCFLAGS}" != x -a x"${depCFLAGS}" != x"${myGIO_CFLAGS}"], [
                        AC_MSG_NOTICE([Patched libglib/libgio CFLAGS to declare -isystem])
                        AS_IF([test x"${nut_enable_configure_debug}" = xyes], [
                            AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) old: ${depCFLAGS}])
                            AC_MSG_NOTICE([(CONFIGURE-DEVEL-DEBUG) new: ${myGIO_CFLAGS}])
                        ])
                        depCFLAGS="${myGIO_CFLAGS}"
                    ])
                    unset myGIO_CFLAGS
                ])

                nut_cv_LIBGIO_CFLAGS="${depCFLAGS}"
                nut_cv_LIBGIO_LIBS="${depLIBS}"

                dnl Help ltdl if we can (nut-scanner etc.)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*gio*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBGIO], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBGIO}" && test -s "${nut_cv_SOPATH_LIBGIO}"], [
                                nut_cv_SOFILE_LIBGIO="`basename \"${nut_cv_SOPATH_LIBGIO}\"`"
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])
            nut_cv_LIBGIO_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBGIO_LIBS_SOURCE="${depLIBS_SOURCE}"

            dnl Make sure the values cached/updated are the ones we discovered now:
            AC_CACHE_VAL([nut_cv_have_libglib], [])
            AC_CACHE_VAL([nut_cv_LIBGLIB_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBGLIB_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBGLIB_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBGLIB_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBGLIB_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBGLIB], [])
            AC_CACHE_VAL([nut_cv_SOPATH_LIBGLIB], [])

            AC_CACHE_VAL([nut_cv_have_libgio], [])
            AC_CACHE_VAL([nut_cv_LIBGIO_VERSION], [])
            AC_CACHE_VAL([nut_cv_LIBGIO_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBGIO_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBGIO_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBGIO_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBGIO], [])
            AC_CACHE_VAL([nut_cv_SOPATH_LIBGIO], [])

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
            nut_cv_checked_libglib="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libglib}" = xyes], [
            nut_have_libglib="${nut_cv_have_libglib}"
            nut_have_libgio="${nut_cv_have_libgio}"

            AS_IF([test "${nut_noncv_checked_libglib_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])

                CFLAGS="${nut_cv_LIBGLIB_CFLAGS}"
                AC_CHECK_HEADERS(glib.h, [], [nut_have_libglib=no], [AC_INCLUDES_DEFAULT])

                CFLAGS="${nut_cv_LIBGLIB_CFLAGS}"
                AC_CHECK_HEADERS(gio/gio.h, [], [nut_have_libgio=no], [AC_INCLUDES_DEFAULT])
                dnl AC_CHECK_FUNCS(g_bus_get_sync, [], [nut_have_libgio=no])

                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                unset CFLAGS_ORIG
            ])

            LIBGLIB_CFLAGS="${nut_cv_LIBGLIB_CFLAGS}"
            LIBGLIB_LIBS="${nut_cv_LIBGLIB_LIBS}"
            LIBGIO_CFLAGS="${nut_cv_LIBGIO_CFLAGS}"
            LIBGIO_LIBS="${nut_cv_LIBGIO_LIBS}"

            dnl For nut-scanner style autoloading:
            SOPATH_LIBGLIB="${nut_cv_SOPATH_LIBGLIB}"
            SOFILE_LIBGLIB="${nut_cv_SOFILE_LIBGLIB}"
            SOPATH_LIBGIO="${nut_cv_SOPATH_LIBGIO}"
            SOFILE_LIBGIO="${nut_cv_SOFILE_LIBGIO}"

            dnl For troubleshooting of re-runs, mostly:
            LIBGLIB_CFLAGS_SOURCE="${nut_cv_LIBGLIB_CFLAGS_SOURCE}"
            LIBGLIB_LIBS_SOURCE="${nut_cv_LIBGLIB_LIBS_SOURCE}"
            LIBGLIB_VERSION="${nut_cv_LIBGLIB_VERSION}"
            LIBGIO_CFLAGS_SOURCE="${nut_cv_LIBGIO_CFLAGS_SOURCE}"
            LIBGIO_LIBS_SOURCE="${nut_cv_LIBGIO_LIBS_SOURCE}"
            LIBGIO_VERSION="${nut_cv_LIBGIO_VERSION}"

            AS_IF([test "${nut_have_libglib}" = "yes"], [
                AC_DEFINE(HAVE_LIBGLIB, 1, [Define to enable libglib support])
                AS_IF([test -n "${SOPATH_LIBGLIB}" && test -s "${SOPATH_LIBGLIB}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBGLIB], ["${SOPATH_LIBGLIB}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBGLIB], ["${SOFILE_LIBGLIB}"], [Base file name of dynamic library on build system])
                ])
            ])

            AS_IF([test "${nut_have_libgio}" = "yes"], [
                AC_DEFINE(HAVE_LIBGIO, 1, [Define to enable libgio support])
                AS_IF([test -n "${SOPATH_LIBGIO}" && test -s "${SOPATH_LIBGIO}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBGIO], ["${SOPATH_LIBGIO}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBGIO], ["${SOFILE_LIBGIO}"], [Base file name of dynamic library on build system])
                ])
            ])

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libglib_now}" = no], [
                AC_MSG_NOTICE([libglib (cached): ${nut_have_libglib} (${LIBGLIB_VERSION})])
                AC_MSG_NOTICE([libglib (cached): cflags_source='${LIBGLIB_CFLAGS_SOURCE}' libs_source='${LIBGLIB_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libglib (cached): LIBGLIB_CFLAGS='${LIBGLIB_CFLAGS}'])
                AC_MSG_NOTICE([libglib (cached): LIBGLIB_LIBS='${LIBGLIB_LIBS}'])
                AC_MSG_NOTICE([libglib (cached): SOFILE:'${SOFILE_LIBGLIB}', SOPATH:'${SOPATH_LIBGLIB}'])

                AC_MSG_NOTICE([libgio (cached): ${nut_have_libgio} (${LIBGIO_VERSION})])
                AC_MSG_NOTICE([libgio (cached): cflags_source='${LIBGIO_CFLAGS_SOURCE}' libs_source='${LIBGIO_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libgio (cached): LIBGIO_CFLAGS='${LIBGIO_CFLAGS}'])
                AC_MSG_NOTICE([libgio (cached): LIBGIO_LIBS='${LIBGIO_LIBS}'])
                AC_MSG_NOTICE([libgio (cached): SOFILE:'${SOFILE_LIBGIO}', SOPATH:'${SOPATH_LIBGIO}'])
            ])
        ])
    ])
])
