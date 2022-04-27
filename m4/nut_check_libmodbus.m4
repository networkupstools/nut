dnl Check for LIBMODBUS compiler flags. On success, set nut_have_libmodbus="yes"
dnl and set LIBMODBUS_CFLAGS and LIBMODBUS_LIBS. On failure, set
dnl nut_have_libmodbus="no". This macro can be run multiple times, but will
dnl do the checking only once.

AC_DEFUN([NUT_CHECK_LIBMODBUS],
[
if test -z "${nut_have_libmodbus_seen}"; then
	nut_have_libmodbus_seen=yes

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	NUT_CHECK_PKGCONFIG

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[AC_MSG_CHECKING(for libmodbus version via pkg-config)
		 LIBMODBUS_VERSION="`$PKG_CONFIG --silence-errors --modversion libmodbus 2>/dev/null`"
		 if test "$?" != "0" -o -z "${LIBMODBUS_VERSION}"; then
		    LIBMODBUS_VERSION="none"
		 fi
		 AC_MSG_RESULT(${LIBMODBUS_VERSION} found)
		],
		[LIBMODBUS_VERSION="none"
		 AC_MSG_NOTICE([can not check libmodbus settings via pkg-config])
		]
	)

	AS_IF([test x"$LIBMODBUS_VERSION" != xnone],
		[CFLAGS="`$PKG_CONFIG --silence-errors --cflags libmodbus 2>/dev/null`"
		 LIBS="`$PKG_CONFIG --silence-errors --libs libmodbus 2>/dev/null`"
		],
		[CFLAGS="-I/usr/include/modbus"
		 LIBS="-lmodbus"
		]
	)

	AC_MSG_CHECKING(for libmodbus cflags)
	AC_ARG_WITH(modbus-includes,
		AS_HELP_STRING([@<:@--with-modbus-includes=CFLAGS@:>@], [include flags for the libmodbus library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-modbus-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libmodbus ldflags)
	AC_ARG_WITH(modbus-libs,
		AS_HELP_STRING([@<:@--with-modbus-libs=LIBS@:>@], [linker flags for the libmodbus library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-modbus-libs - see docs/configure.txt)
			;;
		*)
			LIBS="${withval}"
			;;
		esac
	], [])
	AC_MSG_RESULT([${LIBS}])

	dnl check if libmodbus is usable
	AC_CHECK_HEADERS(modbus.h, [nut_have_libmodbus=yes], [nut_have_libmodbus=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(modbus_new_rtu, [], [nut_have_libmodbus=no])
	AC_CHECK_FUNCS(modbus_new_tcp, [], [nut_have_libmodbus=no])
	AC_CHECK_FUNCS(modbus_set_byte_timeout, [], [nut_have_libmodbus=no])
	AC_CHECK_FUNCS(modbus_set_response_timeout, [], [nut_have_libmodbus=no])

	dnl modbus_set_byte_timeout() and modbus_set_response_timeout()
	dnl in 3.0.x and 3.1.x have different args (since ~2013): the
	dnl older version used to accept timeout as a struct timeval
	dnl instead of seconds and microseconds. Detect which we use?..
	AS_IF([test x"$nut_have_libmodbus" = xyes],
		[dnl Do not rely on versions if we can test actual API
		 AX_C_PRAGMAS
		 AC_LANG_PUSH([C])
		 AC_CACHE_CHECK([types of arguments for modbus_set_byte_timeout],
			[nut_cv_func_modbus_set_byte_timeout_args],
			[nut_cv_func_modbus_set_byte_timeout_args="unknown"
			 AC_COMPILE_IFELSE(
				[dnl Try purely the old API (timeval)
				 AC_LANG_PROGRAM([
#include <time.h>
#include <modbus.h>
], [modbus_t *ctx; struct timeval to = (struct timeval){0};
modbus_set_byte_timeout(ctx, &to);])
				], [nut_cv_func_modbus_set_byte_timeout_args="timeval"
				dnl Try the old API in more detail: check
				dnl if we can just assign uint32's for new
				dnl code into timeval fields (exist+numeric)?
				 AC_COMPILE_IFELSE(
					[AC_LANG_PROGRAM([
#include <time.h>
#include <stdint.h>
#include <modbus.h>
], [modbus_t *ctx; uint32_t to_sec = 10, to_usec = 50;
struct timeval to = (struct timeval){0};
/* TODO: Clarify and detect warning names and
 * so pragmas for signed/unsigned assignment (e.g.
 * for timeval definitions that have "long" fields)
 */
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SIGN_COMPARE
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SIGN_CONVERSION
# pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
to.tv_sec = to_sec;
to.tv_usec = to_usec;
modbus_set_byte_timeout(ctx, &to);
])
					], [nut_cv_func_modbus_set_byte_timeout_args="timeval_numeric_fields"])
				],

				[dnl Try another API variant: new API with
				 dnl fields of struct timeval as numbers
				 dnl (checks they exist, and are compatible
				 dnl numeric types so compiler can convert)
				 AC_COMPILE_IFELSE(
					[AC_LANG_PROGRAM([
#include <time.h>
#include <stdint.h>
#include <modbus.h>
], [modbus_t *ctx; struct timeval to = (struct timeval){0};
/* TODO: Clarify and detect warning names and
 * so pragmas for signed/unsigned assignment (e.g.
 * for timeval definitions that have "long" fields)
 */
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SIGN_COMPARE
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SIGN_CONVERSION
# pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
uint32_t to_sec = to.tv_sec, to_usec = to.tv_usec;
modbus_set_byte_timeout(ctx, to_sec, to_usec);
])
					], [nut_cv_func_modbus_set_byte_timeout_args="sec_usec_uint32_cast_timeval_fields"],
					[dnl Try another API variant: new API purely (two uint32's)
					 AC_COMPILE_IFELSE(
						[AC_LANG_PROGRAM([
#include <stdint.h>
#include <modbus.h>
], [modbus_t *ctx; uint32_t to_sec = 0, to_usec = 0;
modbus_set_byte_timeout(ctx, to_sec, to_usec);])
						], [nut_cv_func_modbus_set_byte_timeout_args="sec_usec_uint32"])
					])
				])
			])

		dnl NOTE: We could add similar tests to above for
		dnl other time-related methods, but for now keep
		dnl it simple -- and assume some same approach
		dnl applies to the same generation of the library.
		 AC_LANG_POP([C])
		 AC_MSG_RESULT([Found types to use for modbus_set_byte_timeout: ${nut_cv_func_modbus_set_byte_timeout_args}])
		 dnl NOTE: code should check for having a token name defined e.g.:
		 dnl   #ifdef NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32
		 dnl Alas, we can't pass variables as macro name to AC_DEFINE
		 COMMENT="Define to specify timeout method args approach for libmodbus"
		 AS_CASE(["${nut_cv_func_modbus_set_byte_timeout_args}"],
			[timeval_numeric_fields], [AC_DEFINE([NUT_MODBUS_TIMEOUT_ARG_timeval_numeric_fields], 1, [${COMMENT}])],
			[timeval], [AC_DEFINE([NUT_MODBUS_TIMEOUT_ARG_timeval], 1, [${COMMENT}])],
			[sec_usec_uint32_cast_timeval_fields], [AC_DEFINE([NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32_cast_timeval_fields], 1, [${COMMENT}])],
			[sec_usec_uint32], [AC_DEFINE([NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32], 1, [${COMMENT}])],
			[dnl default
			 AC_MSG_WARN([Cannot find proper types to use for modbus_set_byte_timeout])
			 nut_have_libmodbus=no]
			)
	])

	AS_IF([test x"${nut_have_libmodbus}" = x"yes"],
		[LIBMODBUS_CFLAGS="${CFLAGS}"
		 LIBMODBUS_LIBS="${LIBS}"]
	)

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
