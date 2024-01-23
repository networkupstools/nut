#!/bin/sh

# This is a helper script to find Windows DLL library files used by another
# Portable Executable (DLL or EXE) file, restricted to paths in the MinGW
# environment. It can do so recursively, to facilitate installation of NUT
# for Windows, bundled with open-source dependencies.
#
# Copyright (C)
#   2022  Jim Klimov <jimklimov+nut@gmail.com>

REGEX_WS="`printf '[\t ]'`"
REGEX_NOT_WS="`printf '[^\t ]'`"
dllldd() (
	# Traverse an EXE or DLL file for DLLs it needs directly,
	# which are provided in the cross-build env (not system ones).
	# Assume no whitespaces in paths and filenames of interest.

	# grep for standard-language strings where needed:
	LANG=C
	LC_ALL=C
	export LANG LC_ALL

	# Otherwise try objdump, if ARCH is known (linux+mingw builds) or not (MSYS2 builds)
	SEEN=0
	if [ -n "${ARCH-}${MINGW_PREFIX-}${MSYSTEM_PREFIX-}" ] ; then
		for OD in objdump "$ARCH-objdump" ; do
			(command -v "$OD" >/dev/null 2>/dev/null) || continue

			ODOUT="`$OD -x "$@" 2>/dev/null | grep -Ei "DLL Name:" | awk '{print $NF}' | sort | uniq | grep -vEi '^(|/.*/)(msvcrt|userenv|bcrypt|rpcrt4|usp10|(advapi|kernel|user|wsock|ws2_|gdi|ole||shell)(32|64))\.dll$'`" \
			&& [ -n "$ODOUT" ] || continue

			for F in $ODOUT ; do
				if [ -n "$DESTDIR" -a -d "${DESTDIR}" ] ; then
					OUT="`find "$DESTDIR" -type f -name "$F" \! -size 0 2>/dev/null | head -1`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi
				if [ -n "$ARCH" -a -d "/usr/${ARCH}" ] ; then
					OUT="`ls -1 "/usr/${ARCH}/bin/$F" "/usr/${ARCH}/lib/$F" 2>/dev/null || true`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi
				if [ -n "$MSYSTEM_PREFIX" -a -d "$MSYSTEM_PREFIX" ] ; then
					OUT="`ls -1 "${MSYSTEM_PREFIX}/bin/$F" "${MSYSTEM_PREFIX}/lib/$F" 2>/dev/null || true`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi
				if [ -n "$MINGW_PREFIX" -a "$MINGW_PREFIX" != "$MSYSTEM_PREFIX" -a -d "$MINGW_PREFIX" ] ; then
					OUT="`ls -1 "${MINGW_PREFIX}/bin/$F" "${MINGW_PREFIX}/lib/$F" 2>/dev/null || true`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi

				# Look for compiler-provided libraries, e.g. in cross-builds on linux+mingw
				# we have a selection of such C++ required artifacts as:
				#   /usr/lib/gcc/x86_64-w64-mingw32/9.3-win32/libgcc_s_seh-1.dll
				#   /usr/lib/gcc/x86_64-w64-mingw32/9.3-win32/libstdc++-6.dll
				#   /usr/lib/gcc/x86_64-w64-mingw32/9.3-posix/libgcc_s_seh-1.dll
				#   /usr/lib/gcc/x86_64-w64-mingw32/9.3-posix/libstdc++-6.dll
				#   /usr/lib/gcc/i686-w64-mingw32/9.3-win32/libstdc++-6.dll
				#   /usr/lib/gcc/i686-w64-mingw32/9.3-posix/libstdc++-6.dll
				# while on MSYS2 there is one in standard path matched above:
				#   /mingw64/bin/libstdc++-6.dll
				# A clumsy alternative would be to link deliverable C++ libs/bins
				# statically with "-static-libgcc -static-libstdc++" options.
				COMPILER_PATHS=""
				if [ -n "$CC" ] ; then
					# gcc and clang support this option:
					COMPILER_PATHS="`$CC --print-search-dirs | grep libraries: | sed 's,^libraries: *=/,/,'`"
				else
					# FIXME: Try to look up in config.log first?
					if [ -n "$ARCH" ] && (command -v "${ARCH}-gcc") 2>/dev/null >/dev/null ; then
						COMPILER_PATHS="`"${ARCH}-gcc" --print-search-dirs | grep libraries: | sed 's,^libraries: *=/,/,'`"
					fi
				fi
				if [ -n "$CXX" ] ; then
					# g++ and clang support this option:
					COMPILER_PATHS="`$CXX --print-search-dirs | grep libraries: | sed 's,^libraries: *=/,/,'`:${COMPILER_PATHS}"
				else
					# FIXME: Try to look up in config.log first?
					if [ -n "$ARCH" ] && (command -v "${ARCH}-g++") 2>/dev/null >/dev/null ; then
						COMPILER_PATHS="`"${ARCH}-g++" --print-search-dirs | grep libraries: | sed 's,^libraries: *=/,/,'`"
					fi
				fi
				if [ -n "$COMPILER_PATHS" ] ; then
					COMPILER_PATHS="`echo "$COMPILER_PATHS" | tr ':' '\n'`"
					for P in $COMPILER_PATHS ; do
						OUT="`ls -1 "${P}/$F" 2>/dev/null || true`" \
						&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue 2 ; }
					done
				fi

				echo "WARNING: '$F' was not found in searched locations (system paths)!" >&2
			done
		done
		if [ "$SEEN" != 0 ] ; then
			return 0
		fi
	fi

	# if `ldd` handles Windows PE (e.g. on MSYS2), we are lucky:
	#         libiconv-2.dll => /mingw64/bin/libiconv-2.dll (0x7ffd26c90000)
	# but it tends to say "not a dynamic executable"
	# or that file type is not supported
	OUT="`ldd "$@" 2>/dev/null | grep -Ei '\.dll' | grep -E '/(bin|lib)/' | sed "s,^${REGEX_WS}*\(${REGEX_NOT_WS}${REGEX_NOT_WS}*\)${REGEX_WS}${REGEX_WS}*=>${REGEX_WS}${REGEX_WS}*\(${REGEX_NOT_WS}${REGEX_NOT_WS}*\)${REGEX_WS}.*\$,\2," | sort | uniq | grep -Ei '\.dll$'`" \
	&& [ -n "$OUT" ] && { echo "$OUT" ; return 0 ; }

	return 1
)

dlllddrec() (
	# Recurse to find the (mingw-provided) tree of dependencies
	dllldd "$1" | while read D ; do
		echo "$D"
		dlllddrec "$D"
	done | sort | uniq
)

# Alas, can't rely on having BASH, and dash fails to parse its syntax
# even if hidden by conditionals or separate method like this (might
# optionally source it from another file though?)
#diffvars_bash() {
#	diff -bu <(echo "$1") <(echo "$2") | grep -E '^\+[^+]' | sed 's,^\+,,'
#}

dllldddir() (
	# Recurse the current (or specified) directory, find all EXE/DLL here,
	# and locate their dependency DLLs, and produce a unique-item list
	if [ $# = 0 ]; then
		dllldddir .
		return
	fi

	# Assume no whitespace in built/MSYS/MinGW paths...
	ORIGFILES="`find "$@" -type f | grep -Ei '\.(exe|dll)$'`" || return

	# Quick OK, nothing here?
	[ -n "$ORIGFILES" ] || return 0

	# Loop until we see nothing new:
	SEENDLLS="`dllldd $ORIGFILES | sort | uniq`"
	[ -n "$SEENDLLS" ] || return 0

	#if [ -z "$BASH_VERSION" ] ; then
		TMP1="`mktemp`"
		TMP2="`mktemp`"
		trap "rm -f '$TMP1' '$TMP2'" 0 1 2 3 15
	#fi

	NEXTDLLS="$SEENDLLS"
	while [ -n "$NEXTDLLS" ] ; do
		MOREDLLS="`dllldd $NEXTDLLS | sort | uniq`"

		# Next iteration we drill into those we have not seen yet
		#if [ -n "$BASH_VERSION" ] ; then
		#	NEXTDLLS="`diffvars_bash "$SEENDLLS" "$MOREDLLS"`"
		#else
			echo "$SEENDLLS" > "$TMP1"
			echo "$MOREDLLS" > "$TMP2"
			NEXTDLLS="`diff -bu "$TMP1" "$TMP2" | grep -E '^\+[^+]' | sed 's,^\+,,'`"
		#fi

		if [ -n "$NEXTDLLS" ] ; then
			SEENDLLS="`( echo "$SEENDLLS" ; echo "$NEXTDLLS" ) | sort | uniq`"
		fi
	done

	if [ -z "$BASH_VERSION" ] ; then
		rm -f "$TMP1" "$TMP2"
		trap - 0 1 2 3 15
	fi

	echo "$SEENDLLS"
)

dllldddir_pedantic() (
	# For `ldd` or `objdump` versions that do not act on many files

	# Recurse the current (or specified) directory, find all EXE/DLL here,
	# and locate their dependency DLLs, and produce a unique-item list
	if [ $# = 0 ]; then
		dllldddir_pedantic .
		return
	fi

	# Two passes: one finds direct dependencies of all EXE/DLL under the
	# specified location(s); then trims this list to be unique, and then
	# the second pass recurses those libraries for their dependencies:
	find "$@" -type f | grep -Ei '\.(exe|dll)$' \
	| while read E ; do dllldd "$E" ; done | sort | uniq \
	| while read D ; do echo "$D"; dlllddrec "$D" ; done | sort | uniq
)

if [ x"${DLLLDD_SOURCED-}" != xtrue ] ; then
	# Work like a command-line tool:
	case "$1" in
		""|-h|-help|--help)
			cat << EOF
Tool to find DLLs needed by an EXE or another DLL

Directly used libraries:
	$0 dllldd ONEFILE.EXE

Recursively used libraries:
	$0 dlllddrec ONEFILE.EXE

Find all EXE/DLL files under specified (or current) dir,
and list their set of required DLLs
	$0 dllldddir [DIRNAME...]
	$0 dllldddir_pedantic [DIRNAME...]
EOF
			;;
		dlllddrec|dllldd|dllldddir|dllldddir_pedantic) "$@" ;;
		*) dlllddrec "$1" ;;
	esac

	exit 0
fi

# Caller said DLLLDD_SOURCED=true
echo "SOURCED dllldd methods"
