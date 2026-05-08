dnl Check for OpenSSL (LIBOPENSSL) compiler flags. On success, set
dnl nut_have_openssl="yes" and nut_ssl_lib="OpenSSL", and define WITH_SSL,
dnl WITH_OPENSSL, LIBSSL_CFLAGS and LIBSSL_LIBS. On failure, set
dnl nut_have_libssl="no".
dnl This macro can be run multiple times, but will do the checking only once.
dnl It can use autoconf cache to speed up re-runs, assuming unmodified system
dnl environment and same configuration script arguments.

AC_DEFUN([NUT_CHECK_LIBOPENSSL],
[
    dnl Have we been here in this run?
    AS_IF([test -z "${nut_have_libopenssl_seen}"], [
        nut_have_libopenssl_seen=yes

        NUT_ARG_WITH_LIBOPTS_INCLUDES([OpenSSL], [auto])
        NUT_ARG_WITH_LIBOPTS_LIBS([OpenSSL], [auto])

        nut_noncv_checked_libopenssl_now=no
        AC_CACHE_VAL([nut_cv_checked_libopenssl], [
            nut_noncv_checked_libopenssl_now=yes

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
                AC_MSG_CHECKING(for OpenSSL version via pkg-config)
                nut_cv_OPENSSL_VERSION="`$PKG_CONFIG --silence-errors --modversion openssl 2>/dev/null`"
                AS_IF([test "$?" != "0" -o -z "${nut_cv_OPENSSL_VERSION}"], [
                    nut_cv_OPENSSL_VERSION="none"
                ])
                AC_MSG_RESULT(${nut_cv_OPENSSL_VERSION} found)
            ], [
                nut_cv_OPENSSL_VERSION="none"
                AC_MSG_NOTICE([can not check OpenSSL settings via pkg-config])
            ])

            AS_IF([test x"${nut_cv_OPENSSL_VERSION}" != xnone], [
                depCFLAGS="`$PKG_CONFIG --silence-errors --cflags openssl 2>/dev/null`"
                depLIBS="`$PKG_CONFIG --silence-errors --libs openssl 2>/dev/null`"
                depREQUIRES="openssl"
                depCFLAGS_SOURCE="pkg-config"
                depLIBS_SOURCE="pkg-config"
                depREQUIRES_SOURCE="default(pkg-config)"
            ], [
                depCFLAGS=""
                depLIBS="-lssl -lcrypto"
                depREQUIRES="openssl"
                depCFLAGS_SOURCE="default"
                depLIBS_SOURCE="default"
                depREQUIRES_SOURCE="default"
            ])

            dnl allow overriding OpenSSL settings if the user knows best
            AC_MSG_CHECKING(for OpenSSL cflags)
            AS_CASE([${nut_with_openssl_includes}],
                [auto], [],	dnl Keep what we had found above
                    [depCFLAGS="${nut_with_openssl_includes}"
                     depCFLAGS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depCFLAGS} (source: ${depCFLAGS_SOURCE})])

            AC_MSG_CHECKING(for OpenSSL ldflags)
            AS_CASE([${nut_with_openssl_libs}],
                [auto], [],	dnl Keep what we had found above
                    [depLIBS="${nut_with_openssl_libs}"
                     depLIBS_SOURCE="confarg"]
            )
            AC_MSG_RESULT([${depLIBS} (source: ${depLIBS_SOURCE})])

            dnl check if openssl is usable
            CFLAGS="${CFLAGS_ORIG} ${depCFLAGS}"
            LIBS="${LIBS_ORIG} ${depLIBS}"
            REQUIRES="${REQUIRES_ORIG} ${depREQUIRES}"
            AC_CHECK_HEADERS(openssl/ssl.h, [nut_cv_have_openssl=yes], [nut_cv_have_openssl=no], [AC_INCLUDES_DEFAULT])
            AC_CHECK_FUNCS(SSL_CTX_new SSL_CTX_free, [], [nut_cv_have_openssl=no])

            AS_IF([test "${nut_cv_have_openssl}" = "yes"], [
                nut_cv_with_ssl="yes"
                nut_cv_ssl_lib="(OpenSSL)"

                dnl # Variants for compilation choices (not fatal if absent):
                AC_CHECK_FUNCS(SSL_set_default_passwd_cb SSL_set_default_passwd_cb_userdata SSL_CTX_set_default_passwd_cb SSL_CTX_set_default_passwd_cb_userdata SSL_CTX_get0_certificate X509_check_host X509_check_ip_asc X509_NAME_oneline ASN1_STRING_get0_data ASN1_STRING_data ASN1_STRING_length, [], [])

                dnl # Repeat some tricks from nut_compiler_family.m4
                AS_IF([test "x$cross_compiling" != xyes], [
                    AS_IF([test "x$CLANGCC" = xyes -o "x$GCC" = xyes], [
                        dnl # This was hit on OmniOS Extra packaging repo
                        dnl   /usr/ssl-3/include/openssl/safestack.h:205:1:
                        dnl   error: cast from 'sk_OPENSSL_STRING_freefunc'
                        dnl   (aka 'void (*)(char *)') to 'OPENSSL_sk_freefunc'
                        dnl   (aka 'void (*)(void *)') converts to incompatible
                        dnl   function type [-Werror,-Wcast-function-type-strict]
                        dnl # Below: Pick out -I... args of the depCFLAGS
                        dnl # to check locations that actually matter for
                        dnl # the build
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

                dnl # See tools/nut-scanner/Makefile.am and also
                dnl # nut_check_libnss.m4 if there are custom RPATHs
                nut_cv_LIBSSL_LDFLAGS_RPATH=""

                nut_cv_LIBSSL_REQUIRES_SOURCE="${depREQUIRES_SOURCE}"
                nut_cv_LIBSSL_CFLAGS_SOURCE="${depCFLAGS_SOURCE}"
                nut_cv_LIBSSL_LIBS_SOURCE="${depLIBS_SOURCE}"
            ])

            dnl Make sure the values cached/updated are the ones we discovered
            dnl now (or did not modify from other SSL backend variant results):
            AC_CACHE_VAL([nut_cv_have_openssl], [])
            AC_CACHE_VAL([nut_cv_OPENSSL_VERSION], [])

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

            unset REQUIRES_ORIG
            unset CFLAGS_ORIG
            unset LIBS_ORIG

            dnl Complete the cache ritual
            nut_cv_checked_libopenssl="yes"
        ])

        dnl May be cached from earlier build with same args (in NUTCI_AUTOCONF_CACHE case)
        AS_IF([test x"${nut_cv_checked_libopenssl}" = xyes], [
            nut_have_openssl="${nut_cv_have_openssl}"

            dnl NOTE: This one may differ from initial build argument (e.g. "auto")
            nut_with_ssl="${nut_cv_with_ssl}"
            nut_ssl_lib="${nut_cv_ssl_lib}"

            AS_IF([test "${nut_noncv_checked_libopenssl_now}" = no], [
                CFLAGS_ORIG="${CFLAGS}"
                LIBS_ORIG="${LIBS}"
                CFLAGS="${nut_cv_LIBSSL_CFLAGS}"
                LIBS="${nut_cv_LIBSSL_LIBS}"

                dnl Should restore the cached value and be done with it
                AC_LANG_PUSH([C])
                AC_CHECK_HEADERS(openssl/ssl.h, [], [nut_have_openssl=no], [AC_INCLUDES_DEFAULT])
                AC_CHECK_FUNCS(SSL_CTX_new SSL_CTX_free, [], [nut_have_openssl=no])
                AS_IF([test "${nut_have_openssl}" = "yes"], [
                    dnl # Variants for compilation choices (not fatal if absent):
                    AC_CHECK_FUNCS(SSL_set_default_passwd_cb SSL_set_default_passwd_cb_userdata SSL_CTX_set_default_passwd_cb SSL_CTX_set_default_passwd_cb_userdata SSL_CTX_get0_certificate X509_check_host X509_check_ip_asc X509_NAME_oneline ASN1_STRING_get0_data ASN1_STRING_data ASN1_STRING_length, [], [])
                ])
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
            AS_IF([test "${nut_have_openssl}" = "yes"], [
                AC_DEFINE(WITH_OPENSSL, 1, [Define to enable SSL support using OpenSSL])
            ])

            dnl For troubleshooting of re-runs, mostly:
            OPENSSL_VERSION="${nut_cv_OPENSSL_VERSION}"

            dnl Summary for re-runs:
            AS_IF([test "${nut_noncv_checked_libopenssl_now}" = no], [
                AC_MSG_NOTICE([libopenssl (cached): ${nut_have_openssl} (version=${OPENSSL_VERSION})])
                dnl Others, for "Some LIBSSL (cached)", are reported in configure.ac
            ])
        ])
    ])
])
