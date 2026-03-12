#!/bin/sh

# Copyright (C) 2025-2026 by Jim Klimov <jimklimov+nut@gmail.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
############################################################################
#
# Extracted from NUT tools/gitlog2version.sh for bits useful to compare SEMVERs.
#
# Checked with bash 3 and 5, dash, ksh, zsh and even busybox sh;
# OpenIndiana, FreeBSD and OpenBSD sh. Not compatible with csh and tcsh.

LANG=C
LC_ALL=C
TZ=UTC
export LANG LC_ALL TZ

if [ -n "${NUT_VERSION_EXTRA_WIDTH-}" -a "${NUT_VERSION_EXTRA_WIDTH-}" -gt 6 ] 2>/dev/null ; then
    :
else
    NUT_VERSION_EXTRA_WIDTH=6
fi

if [ -n "${NUT_VERSION_MIN_COMPONENTS-}" -a "${NUT_VERSION_MIN_COMPONENTS-}" -ge 0 ] 2>/dev/null ; then
    # A number specified by caller is valid (positive integer)
    :
else
    # Default or wrong spec - use NUT default
    NUT_VERSION_MIN_COMPONENTS=-1
fi

# Note we optionally NUT_VERSION_DEFAULT early in the script logic, far below
if [ x"${NUT_VERSION_STRIP_LEADING_ZEROES-}" != xtrue ] ; then
    NUT_VERSION_STRIP_LEADING_ZEROES=false
fi

filter_add_extra_width() {
    # Expand the dot-separated numeric leading part of the version string for
    # relevant alphanumeric comparisons of the result, regardless of digit
    # counts. Above we ensure NUT_VERSION_EXTRA_WIDTH >= 6.
    # NOTE: Not all SEDs allow to substitute a `\n` as a newline in output,
    #  so here we must assume a '|' does not appear in version string values.
    sed -e 's,\.,|\.|,g' -e 's,\([0-9][0-9]*\)\([^.]*\),\1|\2|,g' | tr '|' '\n' | (
        #set -x
        NUMERIC=true
        COMPONENTS=0
        if [ x"${NUT_VERSION_MIN_COMPONENTS}" = x-1 ]; then
            NUT_VERSION_MIN_COMPONENTS=5
        fi
        while read LINE ; do
            #echo "=== '$LINE'" >&2
            case "$LINE" in
                ".") echo "." ;;
                0*|1*|2*|3*|4*|5*|6*|7*|8*|9*)
                    if $NUMERIC && [ x = x"`echo \"$LINE\" | sed 's,[0-9],,g'`" ] ; then
                        # NOTE: Not all shells have `printf '%0.*d'` (variable width)
                        # support, so we embed the number into formatting string:
                        case "$LINE" in
                            0|1*|2*|3*|4*|5*|6*|7*|8*|9*) ;;
                            0*) LINE="`echo \"${LINE}\" | sed -e 's/^00*/0/' -e 's/^0*\([1-9]\)/\1/'`" ;;
                        esac
                        printf "%0.${NUT_VERSION_EXTRA_WIDTH}d" "${LINE}"
                        COMPONENTS="`expr ${COMPONENTS} + 1`"
                    else
                        while [ "${COMPONENTS}" -lt "${NUT_VERSION_MIN_COMPONENTS}" ] ; do
                            printf ".%0.${NUT_VERSION_EXTRA_WIDTH}d" "0"
                            COMPONENTS="`expr ${COMPONENTS} + 1`"
                        done
                        NUMERIC=false
                        echo "$LINE"
                    fi
                    ;;
                "") ;;
                *)
                    while [ "${COMPONENTS}" -lt "${NUT_VERSION_MIN_COMPONENTS}" ] ; do
                        printf ".%0.${NUT_VERSION_EXTRA_WIDTH}d" "0"
                        COMPONENTS="`expr ${COMPONENTS} + 1`"
                    done
                    NUMERIC=false
                    echo "$LINE"
                    ;;
            esac
        done
        # Was the input too short (and with no suffix)?
        while [ "${COMPONENTS}" -lt "${NUT_VERSION_MIN_COMPONENTS}" ] ; do
            printf ".%0.${NUT_VERSION_EXTRA_WIDTH}d" "0"
            COMPONENTS="`expr ${COMPONENTS} + 1`"
        done
        ) | tr -d '\n'
    echo ''
}

filter_away_leading_zeroes() {
    # Chop off leading zeroes in semver part (only impact the numbers-and-dots
    # part of the text), e.g. 02.008.0004-001 => 2.8.4-001
    # Initially this should help convert back values expanded with "extra width"
    # Remain in confines of basic regular expressions (so no alternations with
    # the pipe in parentheses). Order of operations:
    # * Convert repetitive zeroes which ARE the one leading (or only) component
    #   into one zero
    # * Strip away any leading zeroes from start of input (if followed by at
    #   least one other digit)
    # * For string part starting with only zeroes and dots:
    # ** Collapse a trailing all-zeroes component into one zero
    # ** Collapse each intermediate all-zeroes component into one zero
    # ** Strip away any leading zeroes if followed by at least one other digit
    #    and this ends the string
    # ** Strip away any leading zeroes if followed by at least one other digit
    #    and is followed by non-digit-or-dot
    # ** FIXME: The latter three are copy-pasted to match the patterns in
    #    different components, if several are impacted; expecting up to 4
    #    hits with 5-component NUT semver (portable improvements welcome!)
    sed \
        -e 's,^00*$,0,' \
        -e 's,^00*\.,0.,' \
        -e 's,^00*\([1-9][0-9]*\),\1,' \
        -e 's,^\([0-9.]*\)\.00*$,\1.0,' \
        -e 's,^\([0-9.]*\)\.00*\([^0-9]\),\1.0\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([^0-9]\),\1.0\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([^0-9]\),\1.0\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([^0-9]\),\1.0\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)$,\1.\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)$,\1.\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)$,\1.\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)$,\1.\2,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)\([^0-9]\),\1.\2\3,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)\([^0-9]\),\1.\2\3,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)\([^0-9]\),\1.\2\3,g' \
        -e 's,^\([0-9.]*\)\.00*\([1-9][0-9]*\)\([^0-9]\),\1.\2\3,g'
#    | (
#        if [ x"${NUT_VERSION_MIN_COMPONENTS}" = x-1 ]; then
#            NUT_VERSION_MIN_COMPONENTS=3
#        fi
#    )
}

optional_filter_away_leading_zeroes() {
    if [ x"${NUT_VERSION_STRIP_LEADING_ZEROES-}" != xtrue ] ; then
        # Not requested => no-op
        cat
        return
    fi
    filter_away_leading_zeroes
}

while [ $# -gt 0 ] ; do
    case "$1" in
        --help|-h) cat << EOF
Manipulate NUT SEMVER strings so they can be easily compared and sorted with
shell tools.

Common options:

    {--width|-w} NUM
        Pad semver components with leading zeroes to this width (default/min 6)
    {--min-components} NUM
        Ensure there are at least so many components - add/strip .0 in the end
        (NUT SEMVER default 5 for expand, 3 for strip)
        FIXME: strip TBD

Actions:

    $0 [OPTS] {--expand|-x} "SEMVER"
        Pad components in semver start of string with leading zeroes up to
        desired width
    $0 [OPTS] {--strip|+x} "SEMVER"
        Strip leading zeroes from components in semver start of string

    $0 [OPTS] --sortable-expand "SEMVER"
        Print a tab-separated line with expanded variant in first column,
        and original in second
    $0 [OPTS] --sortable-strip "SEMVER"
        Print a tab-separated line with original (presumed expanded) variant
        in first column, and stripped in second

    $0 [OPTS] sort [-r] "SEMVER" "SEMVER"...
        Return sorted (via expansion) list of original semvers (till end of args)
        in their original spelling (with unstripped leading zeroes, if present)

    $0 [OPTS] {test|[|[[} "SEMVER1" {-gt|>|-ge|>=|-lt|<|-le|<=|-eq|=|==|-ne|!=|<>} "SEMVER2" {]|]]}
        Relaxed shellish syntax to compare two semvers, so this script can be
        just prefixed into existing (alphanumeric-based) shell comparisons
EOF
            exit 0
            ;;
        --width|-w)
            if [ -n "$2" -a "$2" -ge 6 ]; then
                NUT_VERSION_EXTRA_WIDTH="$2"
            fi
            # FIXME: "else" and error handling vs number too small and ignored?
            shift
            ;;
        --min-components)
            if [ -n "$2" -a "$2" -ge 0 ]; then
                NUT_VERSION_MIN_COMPONENTS="$2"
            fi
            # FIXME: "else" and error handling vs number too small and ignored?
            shift
            ;;
        --expand|-x|expand) echo "$2" | filter_add_extra_width ; shift ;;
        --strip|+x|strip)  echo "$2" | filter_away_leading_zeroes ; shift ;;
        --sortable-expand) printf '%s\t%s\n' "`echo \"$2\" | filter_add_extra_width`" "$2" ; shift ;;
        --sortable-strip)  printf '%s\t%s\n' "$2" "`echo \"$2\" | filter_away_leading_zeroes`" ; shift ;;
        --sort|sort)
            shift
            SORT_OPTS=""
            if [ x"$1" = x-r ] ; then
                SORT_OPTS="-r"
                shift
            fi
            while [ $# -gt 0 ] ; do
                printf '%s\t%s\n' "`echo \"$1\" | filter_add_extra_width`" "$1"
                shift
            done \
            | sort $SORT_OPTS \
            | awk '{print $2}'
            exit
            ;;
        '['|'[['|test|--test)
            if [ $# -lt 4 ] ; then
                echo "ERROR: Not enough args to 'test' comparison of two semvers" >&2
                exit 2
            fi
            if [ $# -gt 4 ] ; then
                DO_WARN=true
                case "$5" in
                    ']'|']]') if [ $# -eq 5 ] ; then DO_WARN=false ; fi ;;
                esac
                if $DO_WARN ; then
                    echo "WARNING: Extra args ignored after 'test' comparison of two semvers" >&2
                fi
            fi
            SEMVER1="`echo "$2" | filter_add_extra_width`"
            SEMVER2="`echo "$4" | filter_add_extra_width`"
            SEMVER_MIN="`(echo \"$SEMVER1\" ; echo \"$SEMVER2\") | sort | head -1`"
            case "$3" in
                '-eq'|'='|'==')
                    if [ x"${SEMVER1}" = x"${SEMVER2}" ] ; then
                        exit 0
                    else
                        exit 1
                    fi
                    ;;
                '-ne'|'!='|'<>')
                    if [ x"${SEMVER1}" = x"${SEMVER2}" ] ; then
                        exit 1
                    else
                        exit 0
                    fi
                    ;;
                '-gt'|'>')
                    if [ x"${SEMVER_MIN}" = x"${SEMVER2}" ] && [ x"${SEMVER1}" != x"${SEMVER2}" ] ; then
                        exit 0
                    else
                        exit 1
                    fi
                    ;;
                '-ge'|'>=')
                    if [ x"${SEMVER_MIN}" = x"${SEMVER2}" ] ; then
                        exit 0
                    else
                        exit 1
                    fi
                    ;;
                '-lt'|'<')
                    if [ x"${SEMVER_MIN}" = x"${SEMVER1}" ] && [ x"${SEMVER1}" != x"${SEMVER2}" ] ; then
                        exit 0
                    else
                        exit 1
                    fi
                    ;;
                '-le'|'<=')
                    if [ x"${SEMVER_MIN}" = x"${SEMVER1}" ] ; then
                        exit 0
                    else
                        exit 1
                    fi
                    ;;
                *)
                    echo "ERROR: Unsupported operation to 'test' comparison of two semvers: '$3'" >&2
                    exit 2
                    ;;
            esac
            ;;
        *)  echo "ERROR: Unsupported operation/argument (see help): '$1'" >&2
            exit 2
            ;;
    esac
    shift
done
