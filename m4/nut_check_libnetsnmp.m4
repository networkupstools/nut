dnl Check for LIBNETSNMP compiler flags. On success, set
dnl nut_have_libnetsnmp="yes" and set LIBNETSNMP_CFLAGS and
dnl LIBNETSNMP_LIBS. On failure, set nut_have_libnetsnmp="no".
dnl This macro can be run multiple times, but will do the
dnl checking only once.

AC_DEFUN([NUT_CHECK_LIBNETSNMP],
[
if test -z "${nut_have_libnetsnmp_seen}"; then
	nut_have_libnetsnmp_seen=yes
	NUT_CHECK_PKGCONFIG
	AC_LANG_PUSH([C])

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	dnl We prefer to get info from pkg-config (for suitable arch/bitness as
	dnl specified in args for that mechanism), unless (legacy) a particular
	dnl --with-net-snmp-config=... was requested. If there is no pkg-config
	dnl info, we fall back to detecting and running a NET_SNMP_CONFIG as well.

	dnl By default seek in PATH, but which variant (if several are provided)?
	AC_CHECK_SIZEOF([void *])
	NET_SNMP_CONFIG="none"
	AS_CASE(["${ac_cv_sizeof_void_p}"],
		[4],[AC_PATH_PROGS([NET_SNMP_CONFIG], [net-snmp-config-32 net-snmp-config], [none])],
		[8],[AC_PATH_PROGS([NET_SNMP_CONFIG], [net-snmp-config-64 net-snmp-config], [none])],
		    [AC_PATH_PROGS([NET_SNMP_CONFIG], [net-snmp-config], [none])]
	)

	prefer_NET_SNMP_CONFIG=false
	AC_ARG_WITH(net-snmp-config,
		AS_HELP_STRING([@<:@--with-net-snmp-config=/path/to/net-snmp-config@:>@],
			[path to program that reports Net-SNMP configuration]),
	[
		case "${withval}" in
		""|yes) prefer_NET_SNMP_CONFIG=true ;;
		no)
			dnl AC_MSG_ERROR(invalid option --with(out)-net-snmp-config - see docs/configure.txt)
			prefer_NET_SNMP_CONFIG=false
			;;
		*)
			NET_SNMP_CONFIG="${withval}"
			prefer_NET_SNMP_CONFIG=true
			;;
		esac
	])

	if test x"$have_PKG_CONFIG" = xyes && ! "${prefer_NET_SNMP_CONFIG}" ; then
		AC_MSG_CHECKING(for Net-SNMP version via pkg-config)
		dnl TODO? Loop over possible/historic pkg names, like
		dnl netsnmp, net-snmp, ucd-snmp, libsnmp, snmp...
		SNMP_VERSION="`$PKG_CONFIG --silence-errors --modversion netsnmp 2>/dev/null`"
		if test "$?" = "0" -a -n "${SNMP_VERSION}" ; then
			AC_MSG_RESULT(${SNMP_VERSION} found)
		else
			AC_MSG_RESULT(none found)
			prefer_NET_SNMP_CONFIG=true
		fi
	fi

	if test "$NET_SNMP_CONFIG" = none ; then
		prefer_NET_SNMP_CONFIG=false
	fi

	if "${prefer_NET_SNMP_CONFIG}" ; then
		dnl See which version of the Net-SNMP library (if any) is installed
		AC_MSG_CHECKING(for Net-SNMP version via ${NET_SNMP_CONFIG})
		SNMP_VERSION="`${NET_SNMP_CONFIG} --version 2>/dev/null`"
		if test "$?" != "0" -o -z "${SNMP_VERSION}"; then
			SNMP_VERSION="none"
			prefer_NET_SNMP_CONFIG=false
		fi
		AC_MSG_RESULT(${SNMP_VERSION} found)
	fi

	if test x"$have_PKG_CONFIG" != xyes && ! "${prefer_NET_SNMP_CONFIG}" ; then
		AC_MSG_WARN([did not find either net-snmp-config or pkg-config for net-snmp])
	fi

	AC_MSG_CHECKING(for Net-SNMP cflags)
	AC_ARG_WITH(snmp-includes,
		AS_HELP_STRING([@<:@--with-snmp-includes=CFLAGS@:>@], [include flags for the Net-SNMP library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-snmp-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [AS_IF(["${prefer_NET_SNMP_CONFIG}"],
		[CFLAGS="`${NET_SNMP_CONFIG} --base-cflags 2>/dev/null`"],
		[AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[CFLAGS="`$PKG_CONFIG --silence-errors --cflags netsnmp 2>/dev/null`"]
			)]
		)]
	)
	AC_MSG_RESULT([${CFLAGS}])

	myLIBS_SOURCE=""
	AC_MSG_CHECKING(for Net-SNMP libs)
	AC_ARG_WITH(snmp-libs,
		AS_HELP_STRING([@<:@--with-snmp-libs=LIBS@:>@], [linker flags for the Net-SNMP library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-snmp-libs - see docs/configure.txt)
			;;
		*)
			myLIBS_SOURCE="confarg"
			LIBS="${withval}"
			;;
		esac
	], [AS_IF(["${prefer_NET_SNMP_CONFIG}"],
		[LIBS="`${NET_SNMP_CONFIG} --libs 2>/dev/null`"
		 myLIBS_SOURCE="netsnmp-config"],
		[AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[LIBS="`$PKG_CONFIG --silence-errors --libs netsnmp 2>/dev/null`"
			 myLIBS_SOURCE="pkg-config"],
			[LIBS="-lnetsnmp"
			 myLIBS_SOURCE="default"])]
		)]
	)
	AC_MSG_RESULT([${LIBS}])

	dnl Check if the Net-SNMP library is usable
	nut_have_libnetsnmp_static=no
	AC_CHECK_HEADERS(net-snmp/net-snmp-config.h, [nut_have_libnetsnmp=yes], [nut_have_libnetsnmp=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(init_snmp, [], [
		dnl Probably is dysfunctional, except one case...
		nut_have_libnetsnmp=no
		AS_IF([test x"$myLIBS_SOURCE" = x"pkg-config"], [
			AS_CASE(["${target_os}"],
				[*mingw*], [
					AC_MSG_NOTICE([mingw builds of net-snmp might provide only a static library - retrying for that])
					LIBS="`$PKG_CONFIG --silence-errors --libs --static netsnmp 2>/dev/null`"
					dnl # Some workarouds here, to avoid libtool bailing out like this:
					dnl # *** Warning: This system cannot link to static lib archive /usr/x86_64-w64-mingw32/lib//libnetsnmp.la.
					dnl # *** I have the capability to make that library automatically link in when
					dnl # *** you link to this library.  But I can only do this if you have a
					dnl # *** shared version of the library, which you do not appear to have.
					dnl # In Makefiles be sure to use _LDFLAGS (not _LIBADD) to smuggle linker
					dnl # arguments when building "if WITH_SNMP_STATIC" recipe blocks!
					dnl # For a practical example, see tools/nut-scanner/Makefile.am.
					LIBS="`echo " $LIBS" | sed 's/ -l/ -Wl,-l/g'`"
					AS_UNSET([ac_cv_func_init_snmp])
					AC_CHECK_FUNCS(init_snmp, [
						nut_have_libnetsnmp=yes
						nut_have_libnetsnmp_static=yes
					])
				]
			)
		])
	])
	AS_UNSET([myLIBS_SOURCE])

	AS_IF([test "${nut_have_libnetsnmp}" = "yes"], [
		LIBNETSNMP_CFLAGS="${CFLAGS}"
		LIBNETSNMP_LIBS="${LIBS}"

		AC_MSG_CHECKING([for defined usmAESPrivProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAESPrivProtocol;
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmAES128PrivProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAES128PrivProtocol;
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmDESPrivProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmDESPrivProtocol;
#ifdef NETSNMP_DISABLE_DES
#error "NETSNMP_DISABLE_DES is defined"
#endif
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmHMAC256SHA384AuthProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMAC256SHA384AuthProtocol;
#ifndef HAVE_EVP_SHA384
#error "HAVE_EVP_SHA384 is NOT defined"
#endif
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmHMAC384SHA512AuthProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMAC384SHA512AuthProtocol;
#ifndef HAVE_EVP_SHA384
#error "HAVE_EVP_SHA384 is NOT defined"
#endif
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmHMAC192SHA256AuthProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMAC192SHA256AuthProtocol;
#ifndef HAVE_EVP_SHA224
#error "HAVE_EVP_SHA224 is NOT defined"
#endif
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmAES192PrivProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAES192PrivProtocol;
#ifndef NETSNMP_DRAFT_BLUMENTHAL_AES_04
#error "NETSNMP_DRAFT_BLUMENTHAL_AES_04 is NOT defined"
#endif
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmAES256PrivProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmAES256PrivProtocol;
#ifndef NETSNMP_DRAFT_BLUMENTHAL_AES_04
#error "NETSNMP_DRAFT_BLUMENTHAL_AES_04 is NOT defined"
#endif
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmHMACMD5AuthProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMACMD5AuthProtocol;
#ifdef NETSNMP_DISABLE_MD5
#error "NETSNMP_DISABLE_MD5 is defined"
#endif
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined usmHMACSHA1AuthProtocol])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
oid * pProto = usmHMACSHA1AuthProtocol;
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol, 0, [Variable or macro by this name is not resolvable])
			])

		AC_MSG_CHECKING([for defined NETSNMP_DRAFT_BLUMENTHAL_AES_04])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
int num = NETSNMP_DRAFT_BLUMENTHAL_AES_04 + 1; /* if defined, NETSNMP_DRAFT_BLUMENTHAL_AES_04 is 1 */
],
[]
			)],
			[AC_MSG_RESULT([yes])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04, 1, [Variable or macro by this name is resolvable])
			],
			[AC_MSG_RESULT([no])
			 AC_DEFINE_UNQUOTED(NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04, 0, [Variable or macro by this name is not resolvable])
			])

	])
	AC_LANG_POP([C])

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
