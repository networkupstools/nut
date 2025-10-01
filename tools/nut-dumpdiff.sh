#!/usr/bin/env bash

# This script is intended to simplify comparison of NUT data dumps,
# such as those collected by drivers with `-d 1` argument, or by
# the `upsc` client, ignoring irrelevant variations (e.g. numbers).
#
# TODO: Make more portable than bash and GNU toolkits
#
# Subject to same license as the NUT Project.
#
# Copyright (C)
#	2022-2025 Jim Klimov <jimklimov+nut@gmail.com>
#

if [ $# = 2 ] && [ -s "$1" ] && [ -s "$2" ]; then
    echo "=== $0: comparing '$1' (-) vs. '$2' (+)" >&2
else
    echo "=== $0: aborting: requires two filenames to compare as arguments" >&2
    exit 1
fi

# tools
[ -n "${GREP}" ] || { GREP="`command -v grep`" && [ x"${GREP}" != x ] || { echo "$0: FAILED to locate GREP tool" >&2 ; exit 1 ; } ; }
[ -n "${EGREP}" ] || { if ( [ x"`echo a | $GREP -E '(a|b)'`" = xa ] ) 2>/dev/null ; then EGREP="$GREP -E" ; else EGREP="`command -v egrep`" ; fi && [ x"${EGREP}" != x ] || { echo "$0: FAILED to locate EGREP tool" >&2 ; exit 1 ; } ; }

# Pre-sort just in case:
DATA1="`sort -n < "$1"`"
DATA2="`sort -n < "$2"`"

# Strip away same-context lines,
# and lines with measurements that are either decimal numbers
# or multi-digit numbers without a decimal point (assuming
# differences in shorter numbers or counters may be important)
diff -bu <(echo "$DATA1") <(echo "$DATA2") \
| ${EGREP} '^[+-][^+-]' \
| ${EGREP} -v '^[^:]*(power|load|voltage|current|frequency|temperature|humidity): ([0-9][0-9]*|[0-9][0-9]*\.[0-9][0-9]*)$'

# Note: up to user to post-filter, "^driver.version.*:"
# may be deemed irrelevant as well
