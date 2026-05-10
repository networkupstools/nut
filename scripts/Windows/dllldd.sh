#!/bin/sh

# This is a helper script to find Windows DLL library files used by another
# Portable Executable (DLL or EXE) file, restricted to paths in the MinGW
# environment. It can do so recursively, to facilitate installation of NUT
# for Windows, bundled with open-source dependencies.
#
# Copyright (C)
#   2022-2026  Jim Klimov <jimklimov+nut@gmail.com>

# tools
[ -n "${GREP}" ] || { GREP="`command -v grep`" && [ x"${GREP}" != x ] || { echo "$0: FAILED to locate GREP tool" >&2 ; exit 1 ; } ; }
[ -n "${EGREP}" ] || { if ( [ x"`echo a | $GREP -E '(a|b)'`" = xa ] ) 2>/dev/null ; then EGREP="$GREP -E" ; else EGREP="`command -v egrep`" ; fi && [ x"${EGREP}" != x ] || { echo "$0: FAILED to locate EGREP tool" >&2 ; exit 1 ; } ; }

REGEX_WS="`printf '[\t ]'`"
REGEX_NOT_WS="`printf '[^\t ]'`"

cherrypick_MSYS_DLL_PATH() {
	echo "${PATH}:${LD_LIBRARY_PATH}" | tr ':' '\n' | \
	while read D ; do
		case "$D" in
			""|/?/*|*/Windows*|*/System*|*/Progra*) continue ;;
			"${MSYS_HOME}"/*|/mingw*/*|/usr/*|/clang*/*|/?bin/*|/ucrt*/*|/opt/*|/home/*|/var/*|/tmp/*)
				[ -d "$D" ] && printf '%s:' "$D"
				;;
		esac
	done | sed 's/:*$//'
}
SEARCH_DLL_PATH="`cherrypick_MSYS_DLL_PATH`"

discover_COMPILER_PATHS() {
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
		COMPILER_PATHS="`$CC --print-search-dirs | ${GREP} libraries: | sed 's,^libraries: *=/,/,'`"
	else
		# FIXME: Try to look up in config.log first?
		if [ -n "$ARCH" ] && (command -v "${ARCH}-gcc") 2>/dev/null >/dev/null ; then
			COMPILER_PATHS="`\"${ARCH}-gcc\" --print-search-dirs | ${GREP} libraries: | sed 's,^libraries: *=/,/,'`"
		fi
	fi
	if [ -n "$CXX" ] ; then
		# g++ and clang support this option:
		COMPILER_PATHS="`$CXX --print-search-dirs | ${GREP} libraries: | sed 's,^libraries: *=/,/,'`:${COMPILER_PATHS}"
	else
		# FIXME: Try to look up in config.log first?
		if [ -n "$ARCH" ] && (command -v "${ARCH}-g++") 2>/dev/null >/dev/null ; then
			COMPILER_PATHS="`\"${ARCH}-g++\" --print-search-dirs | ${GREP} libraries: | sed 's,^libraries: *=/,/,'`"
		fi
	fi
}
discover_COMPILER_PATHS

filter_away_system_DLLs() {
	${EGREP} -v -i '^(/.*/)?(msvcrt|userenv|bcrypt|rpcrt4|usp10|ntdll|api-ms-win-[^ ]*|(advapi|kernel|user|wsock|ws2_|gdi|ole|shell)(32|64))\.dll$'
}

filter_away_NUT_DLLs() {
	# Only use this in search via `strings|grep` (and if coupled with
	# a tools-based search)
	${EGREP} -v -i '^(/.*/)?lib(nut|ups)[^ ]*\.dll$'
}

dllldd_with_tools() (
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

			ODOUT="`$OD -x \"$@\" 2>/dev/null | ${EGREP} -i \"DLL Name:\" | awk '{print $NF}' | sort | uniq | filter_away_system_DLLs`" \
			&& [ -n "$ODOUT" ] || continue

			for F in $ODOUT ; do
				if [ -n "$DESTDIR" -a -d "${DESTDIR}" ] ; then
					OUT="`find \"$DESTDIR\" -type f -name \"$F\" \! -size 0 2>/dev/null | head -1`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi
				if [ -n "$ARCH" -a -d "/usr/${ARCH}" ] ; then
					OUT="`ls -1 \"/usr/${ARCH}/bin/$F\" \"/usr/${ARCH}/lib/$F\" 2>/dev/null || true`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi
				if [ -n "$MSYSTEM_PREFIX" -a -d "$MSYSTEM_PREFIX" ] ; then
					OUT="`ls -1 \"${MSYSTEM_PREFIX}/bin/$F\" \"${MSYSTEM_PREFIX}/lib/$F\" 2>/dev/null || true`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi
				if [ -n "$MINGW_PREFIX" -a "$MINGW_PREFIX" != "$MSYSTEM_PREFIX" -a -d "$MINGW_PREFIX" ] ; then
					OUT="`ls -1 \"${MINGW_PREFIX}/bin/$F\" \"${MINGW_PREFIX}/lib/$F\" 2>/dev/null || true`" \
					&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue ; }
				fi

				if [ -n "$COMPILER_PATHS" ] ; then
					COMPILER_PATHS="`echo \"$COMPILER_PATHS\" | tr ':' '\n'`"
					for P in $COMPILER_PATHS ; do
						OUT="`ls -1 \"${P}/$F\" 2>/dev/null || true`" \
						&& [ -n "$OUT" ] && { echo "$OUT" ; SEEN="`expr $SEEN + 1`" ; continue 2 ; }
					done
				fi

				echo "WARNING: '$F' was not found in searched locations (system paths) by tools matcher ($OD)!" >&2
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
	OUT="`ldd \"$@\" 2>/dev/null | ${EGREP} -i '\.dll' | ${EGREP} '/(bin|lib)/' | sed \"s,^${REGEX_WS}*\(${REGEX_NOT_WS}${REGEX_NOT_WS}*\)${REGEX_WS}${REGEX_WS}*=>${REGEX_WS}${REGEX_WS}*\(${REGEX_NOT_WS}${REGEX_NOT_WS}*\)${REGEX_WS}.*\$,\2,\" | sort | uniq | ${EGREP} -i '\.dll$'`" \
	&& [ -n "$OUT" ] && { echo "$OUT" ; return 0 ; }

	echo "WARNING: no suitable DLLs were found in $@ by tools matcher (ldd)!" >&2
	return 1
)

dllldd_with_strings() (
	strings "$@" | ${EGREP} -i '..*\.dll$' | sort | uniq | filter_away_system_DLLs | \
	while read DLL ; do (
		# Avoid looping on at least self-reference in a file
		for S in "$@" ; do
			# echo "=== Compare '$DLL' to '$S'" >&2
			case "$DLL" in
				"$S"|*/"$S") exit ;;
			esac
			case "$S" in
				*/"$DLL") exit ;;
			esac
		done
		# echo "=== '$DLL' not in '$@'" >&2
		echo "$DLL"
	) ; done
)

dllldd() (
	# Did at least one method not-fail and return something?
	RES=0
	OUT_TOOLS="`dllldd_with_tools \"$@\"`" && [ -n "${OUT_TOOLS}" ] || RES=$?
	OUT_STRINGS="`dllldd_with_strings \"$@\" | filter_away_NUT_DLLs`" && [ -n "${OUT_STRINGS}" ] && RES=0
	( # Subshell to sort results in the end
	if [ -n "${OUT_TOOLS}" ] ; then
		echo "${OUT_TOOLS}"
	fi
	if [ -n "${OUT_STRINGS}" ] ; then
		OUT_STRINGS_FULL="`echo \"${OUT_STRINGS}\" | ${EGREP} '[/\\]'`" || OUT_STRINGS_FULL=""
		if [ -n "${OUT_STRINGS_FULL}" ] ; then
			echo "${OUT_STRINGS_FULL}"
			OUT_STRINGS="`echo \"${OUT_STRINGS}\" | ${EGREP} -v '[/\\]'`"
		fi

		for S in ${OUT_STRINGS} ; do
			if (echo "${OUT_STRINGS_FULL}"; echo "${OUT_TOOLS}") | ${GREP} -i '[/\\]'"$S"'$' ; then
				# Already a full path name (reported via grep to stdout above)
				continue
			fi

			# Something new (e.g. something listed for dynamic loading)...

			# Is it simply in PATH (and deemed executable)?
			# WARNING: Can return things in system path
			# command -v "$S" && continue

			echo "${SEARCH_DLL_PATH}" | tr ':' '\n' | {
				while read D ; do
					if [ -s "$D/$S" ]; then
						echo "$D/$S"
						exit
					fi
				done

				echo "WARNING: '$S' was not found in searched locations (system paths) by strings matcher!" >&2
			}
		done
	fi
	) | sort | uniq
	return $RES
)

do_dlllddrec() (
	# Skip out if we already reported this file
	if [ -n "$TEMPFILE_REC" ] ; then
		if ${EGREP} '^'"$1"'$' "$TEMPFILE_REC" >/dev/null 2>/dev/null ; then
			exit
		fi
	fi

	# Recurse to find the (mingw-provided) tree of dependencies - implem
	echo "=== Recursing into '$1'..." >&2
	echo "$1" >> "$TEMPFILE_REC"
	dllldd "$1" | while read DLL_HIT ; do
		[ -n "$DLL_HIT" ] || continue
		echo "$DLL_HIT"
		do_dlllddrec "$DLL_HIT"
	done
)

dlllddrec() {
	TEMPFILE_REC="`mktemp`" || TEMPFILE_REC=""
	if [ -n "$TEMPFILE_REC" ] ; then
		trap 'rm -f "$TEMPFILE_REC"' 0 1 2 3 15
	fi

	# Recurse to find the (mingw-provided) tree of dependencies for one file
	do_dlllddrec "$1" | sort | uniq

	rm -f "$TEMPFILE_REC"
	trap - 0 1 2 3 15
}

# Alas, can't rely on having BASH, and dash fails to parse its syntax
# even if hidden by conditionals or separate method like this (might
# optionally source it from another file though?)
#diffvars_bash() {
#	diff -bu <(echo "$1") <(echo "$2") | ${EGREP} '^\+[^+]' | sed 's,^\+,,'
#}

dllldddir() (
	# Recurse the current (or specified) directory, find all EXE/DLL here,
	# and locate their dependency DLLs, and produce a unique-item list
	if [ $# = 0 ]; then
		dllldddir .
		return
	fi

	# Assume no whitespace in built/MSYS/MinGW paths...
	ORIGFILES="`find \"$@\" -type f | ${EGREP} -i '\.(exe|dll)$'`" || return

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
		#	NEXTDLLS="`diffvars_bash \"$SEENDLLS\" \"$MOREDLLS\"`"
		#else
			echo "$SEENDLLS" > "$TMP1"
			echo "$MOREDLLS" > "$TMP2"
			NEXTDLLS="`diff -bu \"$TMP1\" \"$TMP2\" | ${EGREP} '^\+[^+]' | sed 's,^\+,,'`"
		#fi

		if [ -n "$NEXTDLLS" ] ; then
			SEENDLLS="`( echo \"$SEENDLLS\" ; echo \"$NEXTDLLS\" ) | sort | uniq`"
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
	find "$@" -type f | ${EGREP} -i '\.(exe|dll)$' \
	| while read E ; do dllldd "$E" ; done | sort | uniq \
	| while read D ; do echo "$D"; dlllddrec "$D" ; done | sort | uniq
)

if [ x"${DLLLDD_SOURCED-}" != xtrue ] ; then
	# Work like a command-line tool:
	case "$1" in
		""|-h|-help|--help)
			cat << EOF
Tool to find DLLs needed by an EXE or another DLL

Directly used libraries, search with "proper tools":
	$0 dllldd_with_tools ONEFILE.EXE

Directly used libraries, search with "strings" and "grep":
	$0 dllldd_with_strings ONEFILE.EXE

Directly used libraries, combine the two methods above (default):
	$0 dllldd ONEFILE.EXE

Recursively used libraries:
	$0 dlllddrec ONEFILE.EXE

Find all EXE/DLL files under specified (or current) dir,
and list their set of required DLLs
	$0 dllldddir [DIRNAME...]
	$0 dllldddir_pedantic [DIRNAME...]
EOF
			;;
		dlllddrec|dllldd|dllldd_with_tools|dllldd_with_strings|dllldddir|dllldddir_pedantic) "$@" ;;
		*) dlllddrec "$1" ;;
	esac

	exit 0
fi

# Caller said DLLLDD_SOURCED=true
echo "SOURCED dllldd methods"
