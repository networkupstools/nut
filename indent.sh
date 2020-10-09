#!/bin/bash

# Filter NUT C source file style to conform to recommendations of
# http://networkupstools.org/docs/developer-guide.chunked/ar01s03.html#_coding_style
# Note that the sed filter "command does a reasonable job of converting
# most C++ style comments (but not URLs and DOCTYPE strings)" so a manual
# pass may be needed to revise the changes.
#
# Since that the result is not always immediately acceptable, so this script
# is not part of e.g. automated testing - but it helps clean up much of the
# codebase to be up to a common spec.
#
# Script wrapping (C) 2017 by Jim Klimov
# Rules (C) long ago by NUT project team

set -o pipefail

TMPFILE=".style-tmp.$$"
trap 'EXITCODE=$? ; rm -f "$TMPFILE" ; exit $EXITCODE' 0 1 2 3 15

convertFile() {
    SRCFILE="$1"

    # TODO: The indent below does a poor job for C++
    cat "$SRCFILE" \
        | indent -kr -i8 -T FILE -l1000 -nhnl \
        | sed 's#\(^\|[ \t]\)//[ \t]*\(.*\)[ \t]*#/* \2 */#' \
        > "$TMPFILE"
    STEPCODE=$?
    if [ "$STEPCODE" != 0 ]; then
        TOTALCODE="$STEPCODE"
        echo "FAILED to process file: $SRCFILE" >&2
        continue
    fi

    if diff -q "$SRCFILE" "$TMPFILE" ; then
        echo "File was not changed: $SRCFILE" >&2
    else
        echo "File was changed: $SRCFILE - please revise the differences" >&2
        meld "$SRCFILE" "$TMPFILE"
    fi
}

TOTALCODE=0
if [ $# = 0 ] ; then
    git ls-files | egrep '\.(c|h)$' | while read F ; do
        convertFile "$F"
    done
else
    case "$1" in
        -h|--help)
            echo "Usage: $0 [file.c] [header.h] - process listed file(s)"
            echo "Usage: $0 - process all .c and .h files currently tracked in Git repo"
            ;;
        *)
            for F in "$@" ; do
                convertFile "$F"
            done
            ;;
    esac
fi

exit $TOTALCODE
