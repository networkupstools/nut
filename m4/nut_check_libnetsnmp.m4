dnl Check for LIBNETSNMP compiler flags. On success, set
dnl nut_have_libnetsnmp="yes" and set LIBNETSNMP_CFLAGS and
dnl LIBNETSNMP_LIBS. On failure, set nut_have_libnetsnmp="no".
dnl This macro can be run multiple times, but will do the
dnl checking only once.

AC_DEFUN([NUT_CHECK_LIBNETSNMP],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libnetsnmp_seen}"], [
        nut_have_libnetsnmp_seen=yes

        dnl Choice of net-snmp-config script variant (if we have to) depends on bitness
        AC_CHECK_SIZEOF([void *])
        NUT_ARG_WITH_LIBOPTS([net-snmp-config], [/path/to/net-snmp-config],
                [Path to program that reports Net-SNMP configuration], [auto])
        NUT_ARG_WITH_LIBOPTS_INCLUDES([snmp], [auto], [Net-SNMP])
        NUT_ARG_WITH_LIBOPTS_LIBS([snmp], [auto], [Net-SNMP])

        nut_noncv_checked_libnetsnmp_now=no
        AC_CACHE_VAL([nut_cv_checked_libnetsnmp], [
            nut_noncv_checked_libnetsnmp_now=yes

            AC_REQUIRE([NUT_CHECK_PKGCONFIG])
            AC_LANG_PUSH([C])

            dnl save CFLAGS and LIBS
            CFLAGS_ORIG="${CFLAGS}"
            LIBS_ORIG="${LIBS}"
            CFLAGS=""
            LIBS=""
            depCFLAGS=""
            depLIBS=""
            depCFLAGS_SOURCE=""
            depLIBS_SOURCE=""

            dnl We prefer to get info from pkg-config (for suitable arch/bitness as
            dnl specified in args for that mechanism), unless (legacy) a particular
            dnl --with-net-snmp-config=... was requested. If there is no pkg-config
            dnl info, we fall back to detecting and running a NET_SNMP_CONFIG as well.

            dnl By default seek in PATH, but which variant (if several are provided)?
            NET_SNMP_CONFIG="none"
            AS_CASE(["${ac_cv_sizeof_void_p}"],
                [4],[AC_PATH_PROGS([NET_SNMP_CONFIG], [net-snmp-config-32 net-snmp-config], [none])],
                [8],[AC_PATH_PROGS([NET_SNMP_CONFIG], [net-snmp-config-64 net-snmp-config], [none])],
                    [AC_PATH_PROGS([NET_SNMP_CONFIG], [net-snmp-config], [none])]
            )

            prefer_NET_SNMP_CONFIG=false
            AS_CASE([${nut_with_net_snmp_config}],
                [""|yes],	[prefer_NET_SNMP_CONFIG=true],
                [auto],	[prefer_NET_SNMP_CONFIG=auto],
                [no], [
                    dnl AC_MSG_ERROR(invalid option --with(out)-net-snmp-config - see docs/configure.txt)
                    prefer_NET_SNMP_CONFIG=false
                    ],
                    [NET_SNMP_CONFIG="${nut_with_net_snmp_config}"
                     prefer_NET_SNMP_CONFIG=true
                    ]
            )

            AS_IF([test x"$have_PKG_CONFIG" = xyes -a x"${prefer_NET_SNMP_CONFIG}" != xtrue], [
                AC_MSG_CHECKING(for Net-SNMP version via pkg-config)
                dnl TODO? Loop over possible/historic pkg names, like
                dnl netsnmp, net-snmp, ucd-snmp, libsnmp, snmp...
                nut_cv_SNMP_VERSION="`$PKG_CONFIG --silence-errors --modversion netsnmp 2>/dev/null`"
                AS_IF([test "$?" = "0" -a -n "${nut_cv_SNMP_VERSION}"], [
                    AC_MSG_RESULT(${nut_cv_SNMP_VERSION} found)
                    AS_IF([test x"${prefer_NET_SNMP_CONFIG}" = xauto], [
                        prefer_NET_SNMP_CONFIG=false
                    ])
                ], [
                    AC_MSG_RESULT(none found)
                    prefer_NET_SNMP_CONFIG=true
                ])
            ])

            AS_IF([test "$NET_SNMP_CONFIG" = none], [
                prefer_NET_SNMP_CONFIG=false
            ])

            AS_IF(["${prefer_NET_SNMP_CONFIG}"], [
                dnl See which version of the Net-SNMP library (if any) is installed
                AC_MSG_CHECKING(for Net-SNMP version via ${NET_SNMP_CONFIG})
                nut_cv_SNMP_VERSION="`${NET_SNMP_CONFIG} --version 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_SNMP_VERSION}"], [
                    nut_cv_SNMP_VERSION="none"
                    prefer_NET_SNMP_CONFIG=false
                ])
                AC_MSG_RESULT(${nut_cv_SNMP_VERSION} found)
            ])

            AS_IF([test x"$have_PKG_CONFIG" != xyes -a x"${prefer_NET_SNMP_CONFIG}" = xfalse], [
                AC_MSG_WARN([did not find either net-snmp-config or pkg-config for net-snmp])
            ])

            AC_MSG_CHECKING(for Net-SNMP cflags)
            AS_CASE([${nut_with_snmp_includes}],
                [auto], [AS_IF(["${prefer_NET_SNMP_CONFIG}"],
                        [depCFLAGS="`${NET_SNMP_CONFIG} --base-cflags 2>/dev/null`"
                         depCFLAGS_SOURCE="${NET_SNMP_CONFIG} program"],
                        [AS_IF([test x"$have_PKG_CONFIG" = xyes],
                            [depCFLAGS="`$PKG_CONFIG --silence-errors --cflags netsnmp 2>/dev/null`"
                             depCFLAGS_SOURCE="pkg-config"],
                            [depCFLAGS_SOURCE="default"]
                        )]
                    )],
                    [depCFLAGS_SOURCE="confarg"
                     depCFLAGS="${nut_with_snmp_includes}"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            dnl Note: below we check for specifically `if depLIBS_SOURCE == "pkg-config"`
            AC_MSG_CHECKING(for Net-SNMP libs)
            AS_CASE([${nut_with_snmp_libs}],
                [auto], [AS_IF(["${prefer_NET_SNMP_CONFIG}"],
                        [depLIBS="`${NET_SNMP_CONFIG} --libs 2>/dev/null`"
                         depLIBS_SOURCE="${NET_SNMP_CONFIG} program"],
                        [AS_IF([test x"$have_PKG_CONFIG" = xyes],
                            [depLIBS="`$PKG_CONFIG --silence-errors --libs netsnmp 2>/dev/null`"
                             depLIBS_SOURCE="pkg-config"],
                            [depLIBS="-lnetsnmp"
                             depLIBS_SOURCE="default"]
                         )]
                    )],
                    [depLIBS_SOURCE="confarg"
                     depLIBS="${nut_with_snmp_libs}"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl Check if the Net-SNMP library is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            nut_cv_have_libnetsnmp_static=no
            nut_cv_have_libnetsnmp=no
            AC_CHECK_HEADERS([net-snmp/net-snmp-config.h],
                dnl The second header requires the first to be included
                [AC_CHECK_HEADERS([net-snmp/net-snmp-includes.h], [nut_cv_have_libnetsnmp=yes], [],
                 [AC_INCLUDES_DEFAULT
                 #include <net-snmp/net-snmp-config.h>
                 ])
                ], [], [AC_INCLUDES_DEFAULT])

            AC_CHECK_FUNCS(init_snmp, [], [
                dnl Probably is dysfunctional, except one case...
                nut_cv_have_libnetsnmp=no
                AS_IF([test x"$depLIBS_SOURCE" = x"pkg-config"], [
                    AS_CASE(["${target_os}"],
                        [*mingw*], [
                            AC_MSG_NOTICE([mingw builds of net-snmp might provide only a static library - retrying for that])
                            depLIBS="`$PKG_CONFIG --silence-errors --libs --static netsnmp 2>/dev/null`"
                            dnl # Some workarouds here, to avoid libtool bailing out like this:
                            dnl # *** Warning: This system cannot link to static lib archive /usr/x86_64-w64-mingw32/lib//libnetsnmp.la.
                            dnl # *** I have the capability to make that library automatically link in when
                            dnl # *** you link to this library.  But I can only do this if you have a
                            dnl # *** shared version of the library, which you do not appear to have.
                            dnl # In Makefiles be sure to use _LDFLAGS (not _LIBADD) to smuggle linker
                            dnl # arguments when building "if WITH_SNMP_STATIC" recipe blocks!
                            dnl # For a practical example, see tools/nut-scanner/Makefile.am.
                            depLIBS="`echo \" $depLIBS\" | sed 's/ -l/ -Wl,-l/g'`"
                            LIBS="${LIBS_ORIG} ${depLIBS}"
                            AS_UNSET([ac_cv_func_init_snmp])
                            AC_CHECK_FUNCS(init_snmp, [
                                nut_cv_have_libnetsnmp=yes
                                nut_cv_have_libnetsnmp_static=yes
                                dnl Get into this code path upon re-runs even with config.cache:
                                AS_UNSET([ac_cv_func_init_snmp])
                            ])
                        ]
                    )
                ])
            ])

            nut_cv_LIBNETSNMP_CFLAGS=""
            nut_cv_LIBNETSNMP_LIBS=""
            nut_cv_SOFILE_LIBNETSNMP=""
            nut_cv_SOPATH_LIBNETSNMP=""
            AS_IF([test "${nut_cv_have_libnetsnmp}" = "yes"], [
                nut_cv_LIBNETSNMP_CFLAGS="${depCFLAGS}"
                nut_cv_LIBNETSNMP_LIBS="${depLIBS}"

                AC_CACHE_CHECK([for defined usmAESPrivProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAESPrivProtocol;
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmAES128PrivProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAES128PrivProtocol;
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmDESPrivProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmDESPrivProtocol;
#ifdef NETSNMP_DISABLE_DES
#error "NETSNMP_DISABLE_DES is defined"
#endif
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmHMAC256SHA384AuthProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMAC256SHA384AuthProtocol;
#ifndef HAVE_EVP_SHA384
#error "HAVE_EVP_SHA384 is NOT defined"
#endif
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmHMAC384SHA512AuthProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMAC384SHA512AuthProtocol;
#ifndef HAVE_EVP_SHA384
#error "HAVE_EVP_SHA384 is NOT defined"
#endif
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmHMAC192SHA256AuthProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMAC192SHA256AuthProtocol;
#ifndef HAVE_EVP_SHA224
#error "HAVE_EVP_SHA224 is NOT defined"
#endif
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmAES192PrivProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAES192PrivProtocol;
#ifndef NETSNMP_DRAFT_BLUMENTHAL_AES_04
#error "NETSNMP_DRAFT_BLUMENTHAL_AES_04 is NOT defined"
#endif
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmAES256PrivProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAES256PrivProtocol;
#ifndef NETSNMP_DRAFT_BLUMENTHAL_AES_04
#error "NETSNMP_DRAFT_BLUMENTHAL_AES_04 is NOT defined"
#endif
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmHMACMD5AuthProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMACMD5AuthProtocol;
#ifdef NETSNMP_DISABLE_MD5
#error "NETSNMP_DISABLE_MD5 is defined"
#endif
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined usmHMACSHA1AuthProtocol],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMACSHA1AuthProtocol;
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol=no]
                    )])

                AC_CACHE_CHECK([for defined NETSNMP_DRAFT_BLUMENTHAL_AES_04],
                    [nut_cv_NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
int num = NETSNMP_DRAFT_BLUMENTHAL_AES_04 + 1; /* if defined, NETSNMP_DRAFT_BLUMENTHAL_AES_04 is 1 */
],
[]
                        )],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04=yes],
                        [nut_cv_NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04=no]
                    )])

                dnl Help ltdl if we can (nut-scanner etc.)
                for TOKEN in $depLIBS ; do
                    AS_CASE(["${TOKEN}"],
                        [-l*snmp*], [
                            AX_REALPATH_LIB([${TOKEN}], [nut_cv_SOPATH_LIBNETSNMP], [])
                            AS_IF([test -n "${nut_cv_SOPATH_LIBNETSNMP}" && test -s "${nut_cv_SOPATH_LIBNETSNMP}"], [
                                nut_cv_SOFILE_LIBNETSNMP="`basename \"${nut_cv_SOPATH_LIBNETSNMP}\"`"
                                break
                            ])
                        ]
                    )
                done
                unset TOKEN
            ])

            AC_LANG_POP([C])

            dnl Make sure the values cached/updated are the ones we discovered now:
            nut_cv_LIBNETSNMP_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
            nut_cv_LIBNETSNMP_LIBS_SOURCE="${depLIBS_SOURCE}"

            dnl Whichever config script we found or selected to user's bidding above:
            nut_cv_NET_SNMP_CONFIG="${NET_SNMP_CONFIG}"
            nut_cv_prefer_NET_SNMP_CONFIG="${prefer_NET_SNMP_CONFIG}"

            AC_CACHE_VAL([nut_cv_have_libnetsnmp], [])
            AC_CACHE_VAL([nut_cv_have_libnetsnmp_static], [])
            AC_CACHE_VAL([nut_cv_SNMP_VERSION], [])
            AC_CACHE_VAL([nut_cv_NET_SNMP_CONFIG], [])
            AC_CACHE_VAL([nut_cv_prefer_NET_SNMP_CONFIG], [])
            AC_CACHE_VAL([nut_cv_LIBNETSNMP_CFLAGS], [])
            AC_CACHE_VAL([nut_cv_LIBNETSNMP_LIBS], [])
            AC_CACHE_VAL([nut_cv_LIBNETSNMP_CFLAGS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_LIBNETSNMP_LIBS_SOURCE], [])
            AC_CACHE_VAL([nut_cv_SOFILE_LIBNETSNMP], [])
            AC_CACHE_VAL([nut_cv_SOPATH_LIBNETSNMP], [])

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
            nut_cv_checked_libnetsnmp="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libnetsnmp}" = xyes], [
            nut_have_libnetsnmp="${nut_cv_have_libnetsnmp}"
            nut_have_libnetsnmp_static="${nut_cv_have_libnetsnmp_static}"

            AS_IF([test "${nut_noncv_checked_libnetsnmp_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBNETSNMP_CFLAGS}"
                LIBS="${nut_cv_LIBNETSNMP_LIBS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS([net-snmp/net-snmp-config.h], [], [nut_have_libnetsnmp=no], [AC_INCLUDES_DEFAULT])
                dnl The second header requires the first to be included
                AC_CHECK_HEADERS([net-snmp/net-snmp-includes.h], [], [nut_have_libnetsnmp=no],
                    [AC_INCLUDES_DEFAULT
                    #include <net-snmp/net-snmp-config.h>
                    ])
                AS_IF([test "${nut_have_libnetsnmp}" = "yes"], [
                    AC_CHECK_FUNCS(init_snmp, [], [nut_have_libnetsnmp=no])
                ])
                AC_LANG_POP([C])

                CFLAGS="${CFLAGS_ORIG}"
                unset CFLAGS_ORIG
                LIBS="${LIBS_ORIG}"
                unset LIBS_ORIG
            ])

            LIBNETSNMP_CFLAGS="${nut_cv_LIBNETSNMP_CFLAGS}"
            LIBNETSNMP_LIBS="${nut_cv_LIBNETSNMP_LIBS}"

            dnl For nut-scanner style autoloading:
            SOPATH_LIBNETSNMP="${nut_cv_SOPATH_LIBNETSNMP}"
            SOFILE_LIBNETSNMP="${nut_cv_SOFILE_LIBNETSNMP}"

            dnl For troubleshooting of re-runs, mostly:
            LIBNETSNMP_CFLAGS_SOURCE="${nut_cv_LIBNETSNMP_CFLAGS_SOURCE}"
            LIBNETSNMP_LIBS_SOURCE="${nut_cv_LIBNETSNMP_LIBS_SOURCE}"
            SNMP_VERSION="${nut_cv_SNMP_VERSION}"
            NET_SNMP_CONFIG="${nut_cv_NET_SNMP_CONFIG}"
            prefer_NET_SNMP_CONFIG="${nut_cv_prefer_NET_SNMP_CONFIG}"

            AS_IF([test "${nut_have_libnetsnmp}" = "yes"], [
                AC_DEFINE(HAVE_LIBNETSNMP, 1, [Define to enable libnetsnmp support])
                AS_IF([test -n "${SOPATH_LIBNETSNMP}" && test -s "${SOPATH_LIBNETSNMP}"], [
                    AC_DEFINE_UNQUOTED([SOPATH_LIBNETSNMP], ["${SOPATH_LIBNETSNMP}"], [Path to dynamic library on build system])
                    AC_DEFINE_UNQUOTED([SOFILE_LIBNETSNMP], ["${SOFILE_LIBNETSNMP}"], [Base file name of dynamic library on build system])
                ])
            ])

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol, 0, [Variable or macro by this name is not resolvable])]
            )

            AS_IF([test "${nut_cv_NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04}" = "yes"],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04, 1, [Variable or macro by this name is resolvable])],
                [AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04, 0, [Variable or macro by this name is not resolvable])]
            )

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libnetsnmp_now}" = no], [
                AC_MSG_NOTICE([libnetsnmp (cached): ${nut_have_libnetsnmp} (${SNMP_VERSION})])
                AC_MSG_NOTICE([libnetsnmp (cached): cflags_source='${LIBNETSNMP_CFLAGS_SOURCE}' libs_source='${LIBNETSNMP_LIBS_SOURCE}'])
                AC_MSG_NOTICE([libnetsnmp (cached): LIBNETSNMP_CFLAGS='${LIBNETSNMP_CFLAGS}'])
                AC_MSG_NOTICE([libnetsnmp (cached): LIBNETSNMP_LIBS='${LIBNETSNMP_LIBS}'])
                AC_MSG_NOTICE([libnetsnmp (cached): SOFILE:'${SOFILE_LIBNETSNMP}', SOPATH:'${SOPATH_LIBNETSNMP}'])
            ])
        ])
    ])
])
