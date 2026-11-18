#!/bin/sh

set -eu
upslog=${UPSLOG-../clients/upslog}
testdir=`mktemp -d "${TMPDIR:-/tmp}/nut-upslog-output.XXXXXX"`
trap 'rm -rf "$testdir"' 0
trap 'exit 1' HUP INT TERM
user=`id -un 2>/dev/null || whoami`
system=dummy@127.0.0.1:1

run_upslog() {
	NUT_AUTHCONF_FILE=none "$upslog" -F -u "$user" -p "$testdir/upslog" \
		-W 1 -i 0 -f sample "$@" 2>"$testdir/stderr"
}

printf 'sample\n' >"$testdir/expected"
run_upslog -d 1 -s "$system" -l "$testdir/healthy" >"$testdir/stdout"
# Allow native text-mode line endings.
tr -d '\015' <"$testdir/healthy" >"$testdir/rows"
cmp "$testdir/expected" "$testdir/rows"

# Give two systems a stdout open for reading. A third logs to a healthy file.
if run_upslog -d 3 -m "$system,-" -m "other@127.0.0.1:1,-" \
	-s "$system" -l "$testdir/mixed" 1<"$testdir/expected"; then
	echo "upslog reported success after losing samples" >&2
	exit 1
fi
printf 'sample\nsample\nsample\n' >"$testdir/expected"
tr -d '\015' <"$testdir/mixed" >"$testdir/rows"
cmp "$testdir/expected" "$testdir/rows"
if test "`grep -c 'Could not write sample to stdout' "$testdir/stderr"`" != 1; then
	cat "$testdir/stderr" >&2
	exit 1
fi
