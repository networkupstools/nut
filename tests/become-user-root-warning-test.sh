#!/bin/sh

if test "`id -u`" != 0; then
	echo "SKIP: root privileges are required to test become_user(root)" >&2
	exit 0
fi

testdir="`mktemp -d "${TMPDIR-/tmp}/nut-become-user-root-warning.XXXXXX"`" || exit
trap 'rm -rf "$testdir"' EXIT HUP INT TERM

upslog=${UPSLOG-../clients/upslog}
warning='Warning: running as root (UID=0 EUID=0)'

if output="`"$upslog" -F -u root -p "$testdir/upslog" -W 1 -d 1 -i 1 \
	-s dummy@127.0.0.1:1 -l - 2>&1`"; then
	count="`printf '%s\n' "$output" | grep -F "$warning" | wc -l`"
	if test "$count" -eq 1; then
		exit 0
	fi
fi

printf '%s\n' "$output" >&2
exit 1
