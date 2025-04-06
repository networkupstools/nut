dnl Check for LIBLUA compiler flags. On success, set nut_have_lua="yes"
dnl and set LIBLUA_CFLAGS and LIBLUA_LIBS. On failure, set
dnl nut_have_lua="no". This macro can be run multiple times, but will
dnl do the checking only once.
dnl For substantial checks it delegates work to GNU ax_lua.m4 which so
dnl far lacks an ability to check pkg-config information. As such, this
dnl script sets LUA_INCLUDE and LUA_LIB instead of directly CFLAGS and
dnl LIBS (ax_lua uses these inputs, or hazards a guess if they are empty);
dnl these variables can be passed by caller to `configure` script or set
dnl --with-lua-includes/--with-lua-libs options to this m4 script.
dnl Finding the "lua" or "luaX.Y" interpreter program front-end is a bonus
dnl but not a goal (the tool itself is not used by NUT at the moment).

AC_DEFUN([NUT_CHECK_LIBLUA],
[
dnl TODO: Pass minver/maxver as args?
if test -z "${nut_have_lua_seen}"; then
	nut_have_lua_seen=yes
	NUT_CHECK_PKGCONFIG

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"
	CFLAGS=""
	LIBS=""

	LUA_PKGNAME=""

	AS_IF([test x"$have_PKG_CONFIG" = xyes],
		[dnl See which version of the lua library (if any) is installed
		 dnl FIXME : Support detection of cflags/ldflags below by legacy
		 dnl discovery if pkgconfig is not there
		 AC_MSG_CHECKING(for liblua version via pkg-config (5.1 minimum required))
		 for V in lua lua5 lua-5 \
		    lua51 lua5.1 lua-51 lua-5.1 \
		    lua52 lua5.2 lua-52 lua-5.2 \
		    lua53 lua5.3 lua-53 lua-5.3 \
		    lua54 lua5.4 lua-54 lua-5.4 \
		    lua55 lua5.5 lua-55 lua-5.5 \
		 ; do
			LUA_VERSION="`$PKG_CONFIG --silence-errors --modversion "$V" 2>/dev/null`"
			if test "$?" != "0" -o -z "${LUA_VERSION}"; then
				LUA_VERSION="none"
			else
				LUA_PKGNAME="$V"
				break
			fi
		 done
		 AC_MSG_RESULT(${LUA_VERSION} found)
		],
		[LUA_VERSION="none"
		 AC_MSG_NOTICE([can not check liblua settings via pkg-config])
		]
	)

	AC_MSG_CHECKING(for liblua cflags)
	AC_ARG_WITH(lua-includes,
		AS_HELP_STRING([@<:@--with-lua-includes=CFLAGS@:>@], [include flags for the lua library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-lua-includes - see docs/configure.txt)
			;;
		*)
			LUA_INCLUDE="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[LUA_INCLUDE="`$PKG_CONFIG --silence-errors --cflags ${LUA_PKGNAME} 2>/dev/null`" || LUA_INCLUDE=""]
		)]
	)
	AC_MSG_RESULT([${LUA_INCLUDE}])

	AC_MSG_CHECKING(for liblua ldflags)
	AC_ARG_WITH(lua-libs,
		AS_HELP_STRING([@<:@--with-lua-libs=LIBS@:>@], [linker flags for the lua library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-lua-libs - see docs/configure.txt)
			;;
		*)
			LUA_LIB="${withval}"
			;;
		esac
	], [
		AS_IF([test x"$have_PKG_CONFIG" = xyes],
			[LUA_LIB="`$PKG_CONFIG --silence-errors --libs ${LUA_PKGNAME} 2>/dev/null`" || LUA_LIB=""]
		)]
	)
	AC_MSG_RESULT([${LUA_LIB}])

	dnl check if lua is usable
	dnl TODO: Not sure the PROG is really used, so maybe configure test should change later on
	AX_PROG_LUA([5.1], [], [nut_have_lua=yes], [nut_have_lua=no])
	if test "${nut_have_lua}" = "yes"; then
		AX_LUA_HEADERS([nut_have_lua=yes], [nut_have_lua=no])
	fi
	if test "${nut_have_lua}" = "yes"; then
		AX_LUA_LIBS([nut_have_lua=yes], [nut_have_lua=no])
	fi
	dnl FIXME: If we do not have a suitable version while pieces
	dnl  seem to exist, it may be because we found one version
	dnl  in pkg-config and another as the interpreter, so headers
	dnl  check fails.

	if test "${nut_have_lua}" = "yes"; then
		dnl AC_MSG_NOTICE([DEBUG: LUA:      CFLAGS='${CFLAGS}'])
		dnl AC_MSG_NOTICE([DEBUG: LUA: LUA_INCLUDE='${LUA_INCLUDE}'])
		dnl AC_MSG_NOTICE([DEBUG: LUA:        LIBS='${LIBS}'])
		dnl AC_MSG_NOTICE([DEBUG: LUA:     LUA_LIB='${LUA_LIB}'])
		AS_IF([test x"${CFLAGS}" != x], [LIBLUA_CFLAGS="${CFLAGS}"], [LIBLUA_CFLAGS="${LUA_INCLUDE}"])
		AS_IF([test x"${LIBS}" != x], [LIBLUA_LIBS="${LIBS}"], [LIBLUA_LIBS="${LUA_LIB}"])

		dnl Help ltdl if we can (nut-scanner etc.)
		for TOKEN in $LIBS ; do
			AS_CASE(["${TOKEN}"],
				[-l*lua*], [
					AX_REALPATH_LIB([${TOKEN}], [SOPATH_LIBLUA], [])
					AS_IF([test -n "${SOPATH_LIBLUA}" && test -s "${SOPATH_LIBLUA}"], [
						AC_DEFINE_UNQUOTED([SOPATH_LIBLUA],["${SOPATH_LIBLUA}"],[Path to dynamic library on build system])
						SOFILE_LIBLUA="`basename "$SOPATH_LIBLUA"`"
						AC_DEFINE_UNQUOTED([SOFILE_LIBLUA],["${SOFILE_LIBLUA}"],[Base file name of dynamic library on build system])
						break
					])
				]
			)
		done
		unset TOKEN
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
