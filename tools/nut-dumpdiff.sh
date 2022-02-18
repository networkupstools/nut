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
#	2022 Jim Klimov <jimklimov+nut@gmail.com>
#

if [ $# = 2 ] && [ -s "$1" ] && [ -s "$2" ]; then
    echo "=== $0: comparing '$1' (-) vs '$2' (+)" >&2
else
    echo "=== $0: aborting: requires two filenames to compare as arguments" >&2
    exit 1
fi

# Pre-sort just in case:
DATA1="`sort -n < "$1"`"
DATA2="`sort -n < "$2"`"

diff -bu <(echo "$DATA1") <(echo "$DATA2") \
| grep -E '^[+-][^+-]' \
| grep -vE '^[^:]*: [0-9][0-9.]*$'
