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
dllldd() {
	# Traverse an EXE or DLL file for DLLs it needs directly,
	# which are provided in the cross-build env (not system ones).
	# Assume no whitespaces in paths and filenames of interest.

	# if `ldd` handles Windows PE (e.g. on MSYS2), we are lucky:
	#         libiconv-2.dll => /mingw64/bin/libiconv-2.dll (0x7ffd26c90000)
	OUT="`ldd "$1" 2>/dev/null | grep -Ei '\.dll' | grep -E '/(bin|lib)/' | sed "s,^${REGEX_WS}*\(${REGEX_NOT_WS}${REGEX_NOT_WS}*\)${REGEX_WS}${REGEX_WS}*=>${REGEX_WS}${REGEX_WS}*\(${REGEX_NOT_WS}${REGEX_NOT_WS}*\)${REGEX_WS}.*\$,\2,"`" \
	&& [ -n "$OUT" ] && { echo "$OUT" ; return 0 ; }

	# Otherwise try objdump, if ARCH is known (linux+mingw builds) or not (MSYS2 builds)
	if [ -n "${ARCH-}${MINGW_PREFIX-}${MSYSTEM_PREFIX-}" ] ; then
		for OD in objdump "$ARCH-objdump" ; do
			(command -v "$OD" >/dev/null 2>/dev/null) || continue

			ODOUT="`$OD -x "$1" 2>/dev/null | grep -Ei "DLL Name:" | awk '{print $NF}'`" \
			&& [ -n "$ODOUT" ] || continue

			if [ -n "$ARCH" ] ; then
				OUT="`for F in $ODOUT ; do ls -1 "/usr/${ARCH}/"{bin,lib}/"$F" 2>/dev/null || true ; done`" \
				&& [ -n "$OUT" ] && { echo "$OUT" ; return 0 ; }
			fi
			if [ -n "$MSYSTEM_PREFIX" ] ; then
				OUT="`for F in $ODOUT ; do ls -1 "${MSYSTEM_PREFIX}/"{bin,lib}/"$F" 2>/dev/null || true ; done`" \
				&& [ -n "$OUT" ] && { echo "$OUT" ; return 0 ; }
			fi
			if [ -n "$MINGW_PREFIX" ] && [ "$MINGW_PREFIX" != "$MSYSTEM_PREFIX" ] ; then
				OUT="`for F in $ODOUT ; do ls -1 "${MINGW_PREFIX}/"{bin,lib}/"$F" 2>/dev/null || true ; done`" \
				&& [ -n "$OUT" ] && { echo "$OUT" ; return 0 ; }
			fi
		done
	fi

	return 1
}

dlllddrec() (
	# Recurse to find the (mingw-provided) tree of dependencies
	dllldd "$1" | while read D ; do
		echo "$D"
		dlllddrec "$D"
	done | sort | uniq
)

if [ x"${DLLLDD_SOURCED-}" != xtrue ] ; then
	# Work like a command-line tool:
	case "$1" in
		""|-h|-help|--help)
			cat << EOF
Tool to find DLLs needed by an EXE or another DLL

Directly used libraries:
	$0 dllldd FILE.EXE

Recursively used libraries:
	$0 dlllddrec FILE.EXE
EOF
			;;
		dlllddrec|dllldd) "$@" ;;
		*) dlllddrec "$1" ;;
	esac

	exit 0
fi

# Caller said DLLLDD_SOURCED=true
echo "SOURCED dllldd methods"
