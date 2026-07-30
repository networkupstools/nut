dnl Check for Mozilla NSS (LIBNSS) compiler flags. On success, set
dnl nut_have_libnss="yes" and nut_ssl_lib="Mozilla NSS", and define WITH_SSL,
dnl WITH_NSS, LIBSSL_CFLAGS and LIBSSL_LIBS. On failure, set nut_have_libnss="no".
dnl This macro can be run multiple times, but will do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBNSS],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libnss_seen}"], [
        nut_have_libnss_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([nss], [auto], [Mozilla NSS])
        NUT_ARG_WITH_LIBOPTS_LIBS([nss], [auto], [Mozilla NSS])

        nut_noncv_checked_libnss_now=no
        AC_CACHE_VAL([nut_cv_checked_libnss], [
            nut_noncv_checked_libnss_now=yes

            AC_REQUIRE([NUT_CHECK_PKGCONFIG])

            dnl save CFLAGS and LIBS
            CFLAGS_ORIG="${CFLAGS}"
            LIBS_ORIG="${LIBS}"
            REQUIRES_ORIG="${REQUIRES}"
            CFLAGS=""
            LIBS=""
            REQUIRES=""
            depCFLAGS=""
            depCFLAGS_SOURCE=""
            depLIBS=""
            depLIBS_SOURCE=""
            depREQUIRES=""
            depREQUIRES_SOURCE=""

            AS_IF([test x"$have_PKG_CONFIG" = xyes], [
                AC_MSG_CHECKING(for Mozilla NSS version via pkg-config)
                nut_cv_NSS_VERSION="`$PKG_CONFIG --silence-errors --modversion nss 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_NSS_VERSION}"], [
                    nut_cv_NSS_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_NSS_VERSION} found)
            ], [
                nut_cv_NSS_VERSION="none"
                AC_MSG_NOTICE([can not check libnss settings via pkg-config])
            ])

            AS_IF([test x"${nut_cv_NSS_VERSION}" != xnone], [
                depCFLAGS="`$PKG_CONFIG --silence-errors --cflags nss 2>/dev/null`"
                depLIBS="`$PKG_CONFIG --silence-errors --libs nss 2>/dev/null`"
                depREQUIRES="nss"
                depCFLAGS_SOURCE="pkg-config"
                depLIBS_SOURCE="pkg-config"
                depREQUIRES_SOURCE="default(pkg-config)"
            ], [
                depCFLAGS=""
                depLIBS="-lnss3 -lnssutil3 -lsmime3 -lssl3 -lplds4 -lplc4 -lnspr4"
                depREQUIRES="nss"
                depCFLAGS_SOURCE="default"
                depLIBS_SOURCE="default"
                depREQUIRES_SOURCE="default"
            ])

            dnl allow overriding NSS settings if the user knows best
            AC_MSG_CHECKING(for Mozilla NSS cflags)
            AS_CASE([${nut_with_nss_includes}],
                [auto], [],	dnl Keep what we had found above
                    [depCFLAGS="${nut_with_nss_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for Mozilla NSS ldflags)
            AS_CASE([${nut_with_nss_libs}],
                [auto], [],	dnl Keep what we had found above
                    [depLIBS="${nut_with_nss_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if NSS is usable: we need both the runtime and headers
            dnl NOTE that caller may have to specify PKG_CONFIG_PATH including
            dnl their bitness variant if it is not prioritized in their default
            dnl setting built in by OS distribution; the .../pkgconfig/nss.pc
            dnl tends to specify the libdir which is CPU Arch dependent.
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            REQUIRES="${REQUIRES_ORIG} ${depREQUIRES}"
            AC_CHECK_FUNCS(NSS_Init, [nut_cv_have_libnss=yes], [nut_cv_have_libnss=no])
            dnl libc6 also provides an nss.h file, so also check for ssl.h
            AC_CHECK_HEADERS([nss.h ssl.h], [], [nut_cv_have_libnss=no], [AC_INCLUDES_DEFAULT])

            AS_IF([test "${nut_cv_have_libnss}" = "yes"], [
                nut_cv_with_ssl="yes"
                nut_cv_ssl_lib="(Mozilla NSS)"

                dnl # Repeat some tricks from nut_compiler_family.m4
                AS_IF([test "x$cross_compiling" != xyes], [
                    AS_IF([test "x$CLANGCC" = xyes -o "x$GCC" = xyes], [
                        dnl # CLANG dislikes MPS headers for use of reserved
                        dnl # identifiers (starting with underscores, some
                        dnl # with upper-case letters afterwards - oh, the
                        dnl # blasphemers!)
                        addCFLAGS=""
                        for TOKEN in ${depCFLAGS} ; do
                            case "${TOKEN}" in
                                -I*)	TOKENDIR="`echo \"$TOKEN\" | sed 's,^-I,,'`"
                                    case " ${CFLAGS} ${addCFLAGS} " in
                                        *" -isystem $TOKENDIR "*) ;;
                                        *) addCFLAGS="${addCFLAGS} -isystem $TOKENDIR" ;;
                                    esac ;;
                            esac
                        done
                        test -z "${addCFLAGS}" || depCFLAGS="${depCFLAGS} ${addCFLAGS}"
                        unset addCFLAGS
                        unset TOKEN
                        unset TOKENDIR
                    ])
                ])

                dnl WARNING: Do not pre-initialize these values as empty,
                dnl nor override in an "else" -- there may have been (cached
                dnl or live) results from other SSL backends!
                nut_cv_LIBSSL_CFLAGS="${depCFLAGS}"
                nut_cv_LIBSSL_LIBS="${depLIBS}"
                nut_cv_LIBSSL_REQUIRES="${depREQUIRES}"

                dnl # See tools/nut-scanner/Makefile.am
                dnl # FIXME: Handle "-R /path" tokens, are they anywhere?
                nut_cv_LIBSSL_LDFLAGS_RPATH=""
                for TOKEN in ${nut_cv_LIBSSL_LIBS} ; do
                    AS_CASE([$TOKEN],
                        [-R*], [
                            nut_cv_LIBSSL_LDFLAGS_RPATH="${nut_cv_LIBSSL_LDFLAGS_RPATH} $TOKEN"
                            dnl ### nut_cv_LIBSSL_LDFLAGS_RPATH="${nut_cv_LIBSSL_LDFLAGS_RPATH} -Wl,runpath,`echo $TOKEN | sed 's,^-R *,,'`"
                            nut_cv_LIBSSL_LDFLAGS_RPATH="${nut_cv_LIBSSL_LDFLAGS_RPATH} -Wl,-rpath,`echo $TOKEN | sed 's,^-R *,,'`"
                            ]
                    )
                done
                unset TOKEN

                dnl ### AS_IF([test x"${nut_cv_LIBSSL_LDFLAGS_RPATH}" != x], [
                dnl	###    nut_cv_LIBSSL_LDFLAGS_RPATH="--enable-new-dtags ${nut_cv_LIBSSL_LDFLAGS_RPATH}"
                dnl	### ])

                nut_cv_LIBSSL_REQUIRES_SOURCE="${depREQUIRES_SOURCE}"
                nut_cv_LIBSSL_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
                nut_cv_LIBSSL_LIBS_SOURCE="${depLIBS_SOURCE}"
            ])

            dnl Make sure the values cached/updated are the ones we discovered
            dnl now (or did not modify from other SSL backend variant results):
            AC_CACHE_VAL([nut_cv_have_nss], [])
            AC_CACHE_VAL([nut_cv_NSS_VERSION], [])

            AC_CACHE_VAL([nut_cv_with_ssl], [])
            AC_CACHE_VAL([nut_cv_ssl_lib], [])
            AC_CACHE_VAL([nut_cv_LIBSSL_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBSSL_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBSSL_LDFLAGS_RPATH], [])
            AC_CACHE_VAL([nut_cv_LIBSSL_REQUIRES], [])
            AC_CACHE_VAL([nut_cv_LIBSSL_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBSSL_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBSSL_REQUIRES_SOURCE], [])

            unset depCFLAGS
            unset depLIBS
            unset depREQUIRES
            unset depCFLAGS_SOURCE
            unset depLIBS_SOURCE
            unset depREQUIRES_SOURCE

            dnl restore original CFLAGS and LIBS
            CFLAGS="${CFLAGS_ORIG}"
            LIBS="${LIBS_ORIG}"
            REQUIRES="${REQUIRES_ORIG}"

            unset CFLAGS_ORIG
            unset LIBS_ORIG
            unset REQUIRES_ORIG

            dnl Complete the cache ritual
            nut_cv_checked_libnss="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libnss}" = xyes], [
            nut_have_libnss="${nut_cv_have_libnss}"

            dnl NOTE: This one may differ from initial build argument (e.g. "auto")
            nut_with_ssl="${nut_cv_with_ssl}"
            nut_ssl_lib="${nut_cv_ssl_lib}"

            AS_IF([test "${nut_noncv_checked_libnss_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBSSL_CFLAGS}"
                LIBS="${nut_cv_LIBSSL_LIBS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_FUNCS(NSS_Init, [], [nut_have_libnss=no])
                dnl libc6 also provides an nss.h file, so also check for ssl.h
                AC_CHECK_HEADERS([nss.h ssl.h], [], [nut_have_libnss=no], [AC_INCLUDES_DEFAULT])

                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                LIBS="${LIBS_ORIG}"
                unset CFLAGS_ORIG
                unset LIBS_ORIG
            ])

            AS_IF([test "${nut_with_ssl}" = "yes"], [
                AC_DEFINE(WITH_SSL, 1, [Define to enable SSL support])

                dnl NOTE: Technically these may come from another backend variant!
                LIBSSL_CFLAGS="${nut_cv_LIBSSL_CFLAGS}"
                LIBSSL_LIBS="${nut_cv_LIBSSL_LIBS}"
                LIBSSL_REQUIRES="${nut_cv_LIBSSL_REQUIRES}"
                LIBSSL_LDFLAGS_RPATH="${nut_cv_LIBSSL_LDFLAGS_RPATH}"

                dnl For troubleshooting of re-runs, mostly:
                LIBSSL_CFLAGS_SOURCE="${nut_cv_LIBSSL_CFLAGS_SOURCE}"
                LIBSSL_LIBS_SOURCE="${nut_cv_LIBSSL_LIBS_SOURCE}"
                LIBSSL_REQUIRES_SOURCE="${nut_cv_LIBSSL_REQUIRES_SOURCE}"
            ])
            AS_IF([test "${nut_have_libnss}" = "yes"], [
                AC_DEFINE(WITH_NSS, 1, [Define to enable SSL support using Mozilla NSS])
            ])

            dnl For troubleshooting of re-runs, mostly:
            LIBNSS_VERSION="${nut_cv_LIBNSS_VERSION}"
            LIBNSS_MODULE="${nut_cv_LIBNSS_MODULE}"
            LIBNSS_CFLAGS_SOURCE="${nut_cv_LIBNSS_CFLAGS_SOURCE}"
            LIBNSS_LIBS_SOURCE="${nut_cv_LIBNSS_LIBS_SOURCE}"
            LIBNSS_REQUIRES_SOURCE="${nut_cv_LIBNSS_REQUIRES_SOURCE}"

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libnss_now}" = no], [
                AC_MSG_NOTICE([libnss (cached): ${nut_have_libnss} (version=${NSS_VERSION})])
                dnl Others, for "Some LIBSSL (cached)", are reported in configure.ac
            ])
        ])
    ])
])
