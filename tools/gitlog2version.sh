#!/bin/sh

# Copyright (C) 2016-2026 by Jim Klimov <jimklimov+nut@gmail.com>
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
# For setup check NUT_VERSION* in script source.
# For more info see docs/nut-versioning.adoc
#
# The NUT SEMVER definition mostly follows https://semver.org/ standard
# except that for development iterations the base version may have up to
# five dot-separated numeric components (SEMVER triplet for base release,
# and additional data described below). Unlike standard semver provisions
# for "pre-release versions" (separated by a minus sign after the triplet),
# which are "less than" that release for comparisons, the fourth and fifth
# components (if present) are "greater than" that release and any preceding
# development iterations made after it.
#
# Helper script to determine the project version in a manner similar to what
# `git describe` produces, but with added numbers after the common triplet
# of semantically versioned numbers: X.Y.Z.T.B(-C+H(+R)) or X.Y.Z.T.B(-R)
#   * X: MAJOR - incompatible API changes
#   * Y: MINOR - new features and/or API
#   * Z: PATCH - small bug fixes
#   * T: Commits on trunk since previous release tag
#   * B: Commits on branch since nearest ancestor which is on trunk
# The optional suffix (only for commits which are not release tags themselves)
# is provided by `git describe`:
#   * C: Commits on branch since previous release tag
#   * H: (Short) Git hash (prefixed by "g" character) of the described commit
# The pre-release information (if provided/known) would either follow the
# optional suffix detailed above, or it would be the suffix itself:
#   * R: If this commit has a non-release tag, it can be optionally reported
#        so we know that the commit 1234 iterations after release N is also
#        a release candidate for N+1. Note that any dash in that tag value will
#        be replaced by a plus, e.g. 2.8.2.2878.1-2879+g882dd4b00+v2.8.3+rc6
#
# Note that historically NUT did not diligently follow the semver triplet,
# primarily because a snapshot of trunk is tested and released, and work
# moves on with the PATCH part (rarely MINOR one) incremented; no actual
# patches are released to some sustaining track of an older release lineage.
# There were large re-designs that got MAJOR up to 2, though.
#
# Occasionally there may be tagged pre-releases, which follow the standard
# semver markup, like `v2.8.0-rc3` (in git), however they would be converted
# to NUT SEMVER here.
#
############################################################################
# Checked with bash 3 and 5, dash, ksh, zsh and even busybox sh;
# OpenIndiana, FreeBSD and OpenBSD sh. Not compatible with csh and tcsh.
# See some examples in https://github.com/networkupstools/nut/issues/1949

LANG=C
LC_ALL=C
TZ=UTC
export LANG LC_ALL TZ

SCRIPT_DIR="`dirname \"$0\"`"
SCRIPT_DIR="`cd \"${SCRIPT_DIR}\" && pwd`"

if [ x"${abs_top_srcdir}" = x ]; then
    abs_top_srcdir="${SCRIPT_DIR}/.."
fi
if [ x"${abs_top_builddir}" = x ]; then
    abs_top_builddir="${abs_top_srcdir}"
fi

SRC_IS_GIT=false
if ( [ -e "${abs_top_srcdir}/.git" ] ) 2>/dev/null || [ -d "${abs_top_srcdir}/.git" ] || [ -f "${abs_top_srcdir}/.git" ] || [ -h "${abs_top_srcdir}/.git" ] ; then
    SRC_IS_GIT=true
fi

[ -n "${GREP}" ] || { GREP="`command -v grep`" && [ x"${GREP}" != x ] || { echo "$0: FAILED to locate GREP tool" >&2 ; exit 1 ; } ; }
[ -n "${EGREP}" ] || { if ( [ x"`echo a | $GREP -E '(a|b)'`" = xa ] ) 2>/dev/null ; then EGREP="$GREP -E" ; else EGREP="`command -v egrep`" ; fi && [ x"${EGREP}" != x ] || { echo "$0: FAILED to locate EGREP tool" >&2 ; exit 1 ; } ; }
[ -n "${SEMVER_COMPARE}" ] || { SEMVER_COMPARE="${SCRIPT_DIR}/semver-compare.sh" ; }
[ -x "${SEMVER_COMPARE}" ] || { echo "$0: FAILED to locate semver-compare.sh helper" >&2 ; exit 1 ; }

############################################################################
# Numeric-only default version, for AC_INIT and similar consumers
# in case we build without a Git workspace (from tarball, etc.)
# By legacy convention, 3-digit "semver" was for NUT releases, and
# a nominal "semver.1" for any development snapshots afterwards.

# The VERSION_DEFAULT files are absent in Git, but should be provided
# in tarballs. It may be re-generated by NUT autogen.sh script forcibly,
# but is otherwise preferred if present and NUT source dir is not a git
# workspace itself (e.g. when we build from release tarballs in
# a git-tracked repository of distro recipes, do not use that
# distro's own versions for NUT).
# Embedded distros that hack a NUT version are not encouraged to, but
# can, use a NUT_VERSION_FORCED variable or a VERSION_FORCED file with
# higher priority than auto-detection attempts. Unfortunately, some
# appliances tag all software the same with their firmware version;
# if this is required, a NUT_VERSION_FORCED(_SEMVER) envvar from the
# caller environment, or a file setting it reproducibly, can help
# identify the actual NUT release version triplet used on the box.
# Please use it, it immensely helps with community troubleshooting!

# These *FORCED files can be (re-)populated with UPDATE_FILE_GIT_RELEASE:
if [ x"${NUT_VERSION_QUERY-}" = x"UPDATE_FILE_GIT_RELEASE" ] ; then
    if [ -s "${abs_top_srcdir}/VERSION_FORCED" ] ; then
        echo "NOTE: Ignoring '${abs_top_srcdir}/VERSION_FORCED', will replace with git info" >&2
    fi
    if [ -s "${abs_top_srcdir}/VERSION_FORCED_SEMVER" ] ; then
        echo "NOTE: Ignoring '${abs_top_srcdir}/VERSION_FORCED_SEMVER', will replace with git info" >&2
    fi
else
    # If envvar is passed by caller, ignore the files
    if [ x"${NUT_VERSION_FORCED-}${NUT_VERSION_FORCED_SEMVER-}" = x ] ; then
        if [ -s "${abs_top_srcdir}/VERSION_FORCED" ] ; then
            # Should set NUT_VERSION_FORCED=X.Y.Z(.a.b...)
            . "${abs_top_srcdir}/VERSION_FORCED" || exit
        fi
        if [ -s "${abs_top_srcdir}/VERSION_FORCED_SEMVER" ] ; then
            # Should set NUT_VERSION_FORCED_SEMVER=X.Y.Z
            . "${abs_top_srcdir}/VERSION_FORCED_SEMVER" || exit
        fi
    else
        if [ -s "${abs_top_srcdir}/VERSION_FORCED" ] ; then
            echo "NOTE: Ignoring '${abs_top_srcdir}/VERSION_FORCED', because envvars were passed" >&2
        fi
        if [ -s "${abs_top_srcdir}/VERSION_FORCED_SEMVER" ] ; then
            echo "NOTE: Ignoring '${abs_top_srcdir}/VERSION_FORCED_SEMVER', because envvars were passed" >&2
        fi
    fi
fi
if [ -n "${NUT_VERSION_FORCED-}" ] ; then
    NUT_VERSION_DEFAULT="${NUT_VERSION_FORCED-}"
    NUT_VERSION_PREFER_GIT=false
fi

# The VERSION_DEFAULT file can be (re-)populated with UPDATE_FILE:
if [ -z "${NUT_VERSION_DEFAULT-}" -a -s "${abs_top_builddir}/VERSION_DEFAULT" ] ; then
    # Should set NUT_VERSION_DEFAULT=X.Y.Z(.a.b...)
    # Not practically used if NUT_VERSION_FORCED is set
    . "${abs_top_builddir}/VERSION_DEFAULT" || exit
    [ x"${NUT_VERSION_PREFER_GIT-}" = xtrue ] || { [ x"${SRC_IS_GIT}" = xtrue ] || NUT_VERSION_PREFER_GIT=false ; }
fi

if [ -z "${NUT_VERSION_DEFAULT-}" -a -s "${abs_top_srcdir}/VERSION_DEFAULT" ] ; then
    . "${abs_top_srcdir}/VERSION_DEFAULT" || exit
    [ x"${NUT_VERSION_PREFER_GIT-}" = xtrue ] || { [ x"${SRC_IS_GIT}" = xtrue ] || NUT_VERSION_PREFER_GIT=false ; }
fi

# Fallback default, to be updated only during release cycle
[ -n "${NUT_VERSION_DEFAULT-}" ] || NUT_VERSION_DEFAULT='2.8.5'

# Default website paths, extended for historic sub-sites for a release
[ -n "${NUT_WEBSITE-}" ] || NUT_WEBSITE="https://www.networkupstools.org/"

# Must be "true" or "false" exactly, interpreted as such below:
[ x"${NUT_VERSION_PREFER_GIT-}" = xfalse ] || { [ x"${SRC_IS_GIT}" = xtrue ] && NUT_VERSION_PREFER_GIT=true || NUT_VERSION_PREFER_GIT=false ; }

##############################################################################
# For tools/semver-compare.sh ($SEMVER_COMPARE):
if [ -n "${NUT_VERSION_EXTRA_WIDTH-}" -a "${NUT_VERSION_EXTRA_WIDTH-}" -gt 6 ] 2>/dev/null ; then
    :
else
    NUT_VERSION_EXTRA_WIDTH=6
fi

# Note we optionally NUT_VERSION_DEFAULT early in the script logic, far below
if [ x"${NUT_VERSION_STRIP_LEADING_ZEROES-}" != xtrue ] ; then
    NUT_VERSION_STRIP_LEADING_ZEROES=false
fi

# When padding extra width for e.g. comparisons, is 1.2.3 equal to 1.2.3.0.0?
# Should we add those ".0" in the end?
if [ -n "${NUT_VERSION_MIN_COMPONENTS-}" -a "${NUT_VERSION_MIN_COMPONENTS-}" -ge 0 ] 2>/dev/null ; then
    # A number specified by caller is valid (positive integer)
    :
else
    # Default or wrong spec - do not add components (might use NUT default 5)
    NUT_VERSION_MIN_COMPONENTS=0
fi

export NUT_VERSION_EXTRA_WIDTH
export NUT_VERSION_STRIP_LEADING_ZEROES
export NUT_VERSION_MIN_COMPONENTS
##############################################################################

check_shallow_git() {
    if git log --oneline --decorate=short | tail -1 | $GREP -w grafted >&2 || [ 10 -gt `git log --oneline | wc -l` ] ; then
        echo "$0: $1" >&2
    fi
}

getver_git() {
    # NOTE: The chosen trunk branch must be up to date (may be "origin/master"
    # or "upstream/master", etc.) for resulting version discovery to make sense.
    if [ x"${NUT_VERSION_GIT_TRUNK-}" = x ] ; then
        # Find the newest info, it may be in a fetched branch
        # not yet checked out locally (or long not updated).
        # Currently we repeat the likely branch names in the
        # end, so that if they exist and are still newest -
        # those are the names to report.
        for T in master `git branch -a 2>/dev/null | ${EGREP} '^ *remotes/[^ ]*/master$'` origin/master upstream/master master ; do
            git log -1 "$T" 2>/dev/null >/dev/null || continue
            if [ x"${NUT_VERSION_GIT_TRUNK-}" = x ] ; then
                NUT_VERSION_GIT_TRUNK="$T"
            else
                # T is strictly same or newer
                # Assume no deviations from the one true path in a master branch
                git merge-base --is-ancestor "${NUT_VERSION_GIT_TRUNK}" "${T}" 2>/dev/null >/dev/null \
                && NUT_VERSION_GIT_TRUNK="$T"
            fi
        done
        if [ x"${NUT_VERSION_GIT_TRUNK-}" = x ] ; then
            echo "$0: FAILED to discover a NUT_VERSION_GIT_TRUNK in this workspace" >&2
            check_shallow_git "NOTE: Current checkout is shallow, the workspace may not include enough context to describe it"
            return 1
        fi
    fi

    # By default, only annotated tags are considered
    ALL_TAGS_ARG=""
    if [ x"${NUT_VERSION_GIT_ALL_TAGS-}" = xtrue ] ; then ALL_TAGS_ARG="--tags" ; fi

    # NOTE: "--always" should not be needed in NUT repos normally,
    # but may help other projects who accept such scheme and script:
    # it tells git to return a commit hash if no tag is matched.
    # It may still be needed on CI systems which only fetch the
    # commit they build and no branch names or tags (minimizing the
    # traffic, storage and general potential for creative errors).
    ALWAYS_DESC_ARG=""
    if [ x"${NUT_VERSION_GIT_ALWAYS_DESC-}" = xtrue ] ; then ALWAYS_DESC_ARG="--always" ; fi

    # Praises to old gits and the new, who may --exclude;
    # NOTE: match/exclude by shell glob expressions, not regex!
    DESC="`git describe $ALL_TAGS_ARG $ALWAYS_DESC_ARG --match 'v[0-9]*.[0-9]*.[0-9]' --exclude '*-signed' --exclude '*rc*' --exclude '*alpha*' --exclude '*beta*' --exclude '*Windows*' --exclude '*IPM*' 2>/dev/null`" \
    && [ -n "${DESC}" ] \
    || DESC="`git describe $ALL_TAGS_ARG $ALWAYS_DESC_ARG | ${EGREP} -v '(rc|-signed|alpha|beta|Windows|IPM)' | ${EGREP} 'v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*'`"
    # Old stripper (also for possible refspec parts like "tags/"):
    #   echo "${DESC}" | sed -e 's/^v\([0-9]\)/\1/' -e 's,^.*/,,'
    # Follow https://semver.org/#spec-item-10 about build metadata:
    # it is (a dot-separated list) separated by a plus sign from preceding
    DESC="`echo \"${DESC}\" | sed 's/\(-[0-9][0-9]*\)-\(g[0-9a-fA-F][0-9a-fA-F]*\)$/\1+\2/'`"
    if [ x"${DESC}" = x ] ; then
        echo "$0: FAILED to 'git describe' this codebase" >&2
        check_shallow_git "NOTE: Current checkout is shallow, may not include enough history to describe it"
        return 1
    fi

    # Does the current commit correspond to an `(alpha|beta|rc)NUM` git tag
    # (may be un-annotated)? Note that `git describe` picks the value to
    # report in case several tags are attached; as of git-v2.34.1 it seems
    # to go for first alphanumeric hit (picking same or older non-suffixed
    # string over longer ones if available, or older RC over newer release
    # like "v2.8.2-rc8" preferred over "v2.8.3" if they happen to be tagging
    # the same commit):
    DESC_PRERELEASE="`git describe --tags | ${EGREP} '^v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*([0-9]*|[-](rc|alpha|beta)[-]*[0-9][0-9]*)$'`" \
    || DESC_PRERELEASE=""

    # How much of the known trunk history is in current HEAD?
    # e.g. all of it when we are on that branch or PR made from its tip,
    # some of it if looking at a historic snapshot, or nothing if looking
    # at the tagged commit (it is the merge base for itself and any of
    # its descendants):
    BASE="`git merge-base HEAD \"${NUT_VERSION_GIT_TRUNK}\"`" || BASE=""
    if [ x"${BASE}" = x ] ; then
        echo "$0: FAILED to get a git merge-base of this codebase vs. '${NUT_VERSION_GIT_TRUNK}'" >&2
        check_shallow_git "NOTE: Current checkout is shallow, may not include enough history to describe it or find intersections with other trees"
        DESC=""
        return 1
    fi

    # Nearest (annotated by default) tag preceding the HEAD in history:
    TAG="`echo \"${DESC}\" | sed 's/-[0-9][0-9]*[+-]g[0-9a-fA-F][0-9a-fA-F]*$//'`"
    TAG_PRERELEASE=""
    if [ -n "${DESC_PRERELEASE}" ] ; then
        TAG_PRERELEASE="`echo \"${DESC_PRERELEASE}\" | sed 's/-[0-9][0-9]*[+-]g[0-9a-fA-F][0-9a-fA-F]*$//'`"
        if [ x"${DESC_PRERELEASE}" != x"${TAG_PRERELEASE}" ] ; then
            # We did chop off something, so `git describe` above did not hit
            # exactly the tagged commit, but something later - not interesting
            TAG_PRERELEASE=""
        fi
        if [ x"${TAG}" = x"${TAG_PRERELEASE}" ] ; then
            # Nothing new
            TAG_PRERELEASE=""
        fi
    fi

    # Commit count since the tag, and hash, of the current HEAD commit;
    # empty e.g. when HEAD is the release-tagged commit:
    SUFFIX="`echo \"${DESC}\" | sed 's/^.*\(-[0-9][0-9]*[+-]g[0-9a-fA-F][0-9a-fA-F]*\)$/\1/'`" \
    && [ x"${SUFFIX}" != x"${TAG}" ] || SUFFIX=""

    # Tack on "this commit is a pre-release!" info, if known
    SUFFIX_PRERELEASE=""
    if [ -n "${TAG_PRERELEASE}" ] ; then
        SUFFIX_PRERELEASE="`echo \"${TAG_PRERELEASE}\" | tr '-' '+'`"
        if [ -n "${SUFFIX}" ] ; then
            SUFFIX="${SUFFIX}+${SUFFIX_PRERELEASE}"
        else
            SUFFIX="-${SUFFIX_PRERELEASE}"
        fi
    fi

    # 5-digit version, note we strip leading "v" from the expected TAG value
    # Note the commit count will be non-trivial even if this is commit tagged
    # as a final release but it is not (yet?) on the BASE branch!
    VER5="`echo \"${TAG}\" | sed 's,^v,,'`.`git log --oneline \"${TAG}..${BASE}\" | wc -l | tr -d ' '`.`git log --oneline \"${NUT_VERSION_GIT_TRUNK}..HEAD\" | wc -l | tr -d ' '`"
    DESC5="${VER5}${SUFFIX}"

    # Strip up to two trailing zeroes for trunk snapshots and releases
    VER50="`echo \"${VER5}\" | sed -e 's/\.0$//' -e 's/\.0$//'`"
    DESC50="${VER50}${SUFFIX}"

    # Leave exactly 3 components
    if [ -n "${NUT_VERSION_FORCED_SEMVER-}" ] ; then
        if [ x"${NUT_VERSION_STRIP_LEADING_ZEROES-}" != xtrue ] ; then
            SEMVER="${NUT_VERSION_FORCED_SEMVER-}"
        else
            SEMVER="`\"${SEMVER_COMPARE}\" --strip \"${NUT_VERSION_FORCED_SEMVER-}\"`"
        fi
    else
        if [ -n "${TAG_PRERELEASE}" ] ; then
            # Actually report as SEMVER the version of (next) release
            # for which this commit is candidate
            SEMVER="`echo \"${TAG_PRERELEASE}\" | sed -e 's/^v*//' -e 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)[^0-9].*$/\1/'`"
        else
            SEMVER="`echo \"${VER5}\" | sed -e 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\..*$/\1/'`"
        fi
    fi
    # FIXME? Add ".0" up to 3 components?
}

getver_default() {
    # We will collect this value as we go
    SEMVER=""
    if [ -n "${NUT_VERSION_FORCED_SEMVER-}" ] ; then
        if [ x"${NUT_VERSION_STRIP_LEADING_ZEROES-}" != xtrue ] ; then
            SEMVER="${NUT_VERSION_FORCED_SEMVER-}"
        else
            SEMVER="`\"${SEMVER_COMPARE}\" --strip \"${NUT_VERSION_FORCED_SEMVER-}\"`"
        fi
    fi

    # Similar to DESC_PRERELEASE filtering above, should yield non-trivial
    # "-rc6" given a "v2.8.3-rc6" as input. Unlike git-based knowledge,
    # we can not say that this is a build based off old release N which
    # is a candidate for N+1, probably.
    SUFFIX=""
    SUFFIX_DESC=""
    SUFFIX_PRERELEASE=""
    TAG=""
    DESC=""

    # NOTE: We can not rely on SED supporting extended regular
    # expressions, and those with BRE only/by default do not
    # support alternations ("one of..." pipes in parentheses).
    KEYWORD_PRERELEASE=""
    case "${NUT_VERSION_DEFAULT}" in
        *-rc*|*+rc*)        KEYWORD_PRERELEASE="rc" ;;
        *-alpha*|*+alpha*)  KEYWORD_PRERELEASE="alpha" ;;
        *-beta*|*+beta*)    KEYWORD_PRERELEASE="beta" ;;
    esac

    case "${NUT_VERSION_DEFAULT}" in
        *-rc*|*-alpha*|*-beta*)
            # Assume triplet (possibly prefixed with `v`) + suffix
            # like `v2.8.3-rc6` or `2.8.2-beta-1`
            # FIXME: Check the assumption better!
            SUFFIX="`echo \"${NUT_VERSION_DEFAULT}\" | ${EGREP} '^v*[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*([0-9]*|[-](rc|alpha|beta)[-]*[0-9][0-9]*)$' | sed -e 's/^v*//' -e 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\([^0-9].*\)$/\2/'`" \
            && [ -n "${SUFFIX}" ] \
            && SUFFIX_DESC="`echo \"${SUFFIX}\" | sed -e 's/[-]\('\"${KEYWORD_PRERELEASE}\"'\).*$//'`" \
            && SUFFIX_PRERELEASE="`echo \"${SUFFIX}\" | sed 's/^-*//'`" \
            && NUT_VERSION_DEFAULT="`echo \"${NUT_VERSION_DEFAULT}\" | sed -e 's/'\"${SUFFIX}\"'$//'`"
            ;;
        *+rc*|*+alpha*|*+beta*)
            # Consider forced `2.8.2.2878.3-2881+g45029249f+v2.8.3+rc6` values

            # We remove up to 5 dot-separated leading numbers, so
            # for the example above, `-2881+g45029249f` remains:
            tmpSUFFIX="`echo \"${NUT_VERSION_DEFAULT}\" | ${EGREP} '^v*[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*(.*\+(rc|alpha|beta)[+-]*[0-9][0-9]*)$' | sed -e 's/^v*//' -e 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\([^0-9].*\)$/\2/' -e 's/^\(\.[0-9][0-9]*\)//' -e 's/^\(\.[0-9][0-9]*\)//'`" \
            || tmpSUFFIX=""
            if [ -n "${tmpSUFFIX}" ] && [ x"${tmpSUFFIX}" != x"${NUT_VERSION_DEFAULT}" ] ; then
                # Extract tagged NUT version from that suffix
                SUFFIX="${tmpSUFFIX}"
                # for the example above, `v2.8.3+rc6` remains
                tmpTAG_PRERELEASE="`echo \"${tmpSUFFIX}\" | sed 's/^.*[^0-9]\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*[+]\('\"${KEYWORD_PRERELEASE}\"'\)[+-]*[0-9][0-9]*\)$/\1/'`" \
                || tmpTAG_PRERELEASE=""
                SUFFIX_DESC="`echo \"${SUFFIX}\" | sed -e 's/[+]v[0-9.][0-9.]*[+]\('\"${KEYWORD_PRERELEASE}\"'\).*$//'`"
                if [ -n "${tmpTAG_PRERELEASE}" ] && [ x"${tmpSUFFIX}" != x"${tmpTAG_PRERELEASE}" ] ; then
                    # Replace back pluses to dashes for the tag
                    TAG_PRERELEASE="v`echo \"${tmpTAG_PRERELEASE}\" | sed -e 's/[+]\('\"${KEYWORD_PRERELEASE}\"'\)/-\1/' -e 's/\('\"${KEYWORD_PRERELEASE}\"'\)[+]/\1-/'`"
                    if [ -z "${SEMVER}" ] ; then
                        # for the example above, `2.8.3` remains:
                        SEMVER="`echo \"${tmpTAG_PRERELEASE}\" | sed -e 's/[-+].*$//'`"
                        if [ x"${NUT_VERSION_STRIP_LEADING_ZEROES-}" = xtrue ] ; then
                            SEMVER="`\"${SEMVER_COMPARE}\" --strip \"${SEMVER}\"`"
                        fi
                    fi
                    # for the example above, `rc6` remains:
                    SUFFIX_PRERELEASE="`echo \"${tmpTAG_PRERELEASE}\" | sed 's/^[^+-]*[+-]//'`"
                    # for the example above, `2.8.2.2878.3-2881+g45029249f` remains:
                    NUT_VERSION_DEFAULT="`echo \"${NUT_VERSION_DEFAULT}\" | sed -e 's/'\"${SUFFIX}\"'$//'`"
                fi
            fi
            ;;
        #*{0,1,2,3,4,5,6,7,8,9}-{0,1,2,3,4,5,6,7,8,9}*+g{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f,A,B,C,D,E,F}*)
        *-*+g*)
            # Assume a saved/forced non-RC value like `2.8.3.786-786+gadfdbe3ab`
            tmpSUFFIX="`echo \"${NUT_VERSION_DEFAULT}\" | ${EGREP} '^v*[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*(\.[0-9][0-9]*)*(-[0-9][0-9]*\+g*[0-9a-fA-F][0-9a-fA-F]*)$' | sed -e 's/^v*//' -e 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\([^0-9].*\)$/\2/' -e 's/^\(\.[0-9][0-9]*\)//' -e 's/^\(\.[0-9][0-9]*\)//'`" \
            || tmpSUFFIX=""
            if [ -n "${tmpSUFFIX}" ] && [ x"${tmpSUFFIX}" != x"${NUT_VERSION_DEFAULT}" ] ; then
                # Extract tagged NUT version from that suffix
                SUFFIX="${tmpSUFFIX}"
                NUT_VERSION_DEFAULT="`echo \"${NUT_VERSION_DEFAULT}\" | sed -e 's/'\"${SUFFIX}\"'$//'`"
            fi
            ;;
        *+g*)
            # Assume a saved/forced non-RC (buggy?) value like `2.8.3.786+gadfdbe3ab`
            # and assume it meant but missed a "-0" for no commits since tag
            tmpSUFFIX="`echo \"${NUT_VERSION_DEFAULT}\" | ${EGREP} '^v*[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*(\.[0-9][0-9]*)*(\+g*[0-9a-fA-F][0-9a-fA-F]*)$' | sed -e 's/^v*//' -e 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\([^0-9].*\)$/\2/' -e 's/^\(\.[0-9][0-9]*\)//' -e 's/^\(\.[0-9][0-9]*\)//'`" \
            || tmpSUFFIX=""
            if [ -n "${tmpSUFFIX}" ] && [ x"${tmpSUFFIX}" != x"${NUT_VERSION_DEFAULT}" ] ; then
                # Extract tagged NUT version from that suffix
                NUT_VERSION_DEFAULT="`echo \"${NUT_VERSION_DEFAULT}\" | sed -e 's/'\"${tmpSUFFIX}\"'$//'`"

                # Sum up commits after tag (assuming 3 components are it) as the DESC offset
                OFFSET="`echo \"${NUT_VERSION_DEFAULT}\" | sed 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\.//'`"
                if [ -n "${OFFSET}" -a x"${OFFSET}" != x"${NUT_VERSION_DEFAULT}" ] ; then
                    OFFSET="`echo \"${OFFSET}\" | sed 's/\./ \+ /g'`"
                    OFFSET="`expr $OFFSET`" && [ "${OFFSET}" -gt 0 ] || OFFSET=0
                else
                    OFFSET=0
                fi

                SUFFIX="-${OFFSET}${tmpSUFFIX}"
            fi
            ;;
    esac

    NUT_VERSION_DEFAULT_DOTS="`echo \"${NUT_VERSION_DEFAULT}\" | sed 's/[^.]*//g' | tr -d '\n' | wc -c`"

    # Ensure at least 4 dots (5 presumed-numeric components)
    NUT_VERSION_DEFAULT5_DOTS="${NUT_VERSION_DEFAULT_DOTS}"
    NUT_VERSION_DEFAULT5="${NUT_VERSION_DEFAULT}"
    while [ "${NUT_VERSION_DEFAULT5_DOTS}" -lt 4 ] ; do
        NUT_VERSION_DEFAULT5="${NUT_VERSION_DEFAULT5}.0"
        NUT_VERSION_DEFAULT5_DOTS="`expr $NUT_VERSION_DEFAULT5_DOTS + 1`"
    done

    # Truncate/extend to exactly 2 dots (3 presumed-numeric components)
    NUT_VERSION_DEFAULT3_DOTS="${NUT_VERSION_DEFAULT_DOTS}"
    NUT_VERSION_DEFAULT3="${NUT_VERSION_DEFAULT}"
    while [ "${NUT_VERSION_DEFAULT3_DOTS}" -lt 2 ] ; do
        NUT_VERSION_DEFAULT3="${NUT_VERSION_DEFAULT3}.0"
        NUT_VERSION_DEFAULT3_DOTS="`expr $NUT_VERSION_DEFAULT3_DOTS + 1`"
    done
    while [ "${NUT_VERSION_DEFAULT3_DOTS}" -gt 2 ] ; do
        NUT_VERSION_DEFAULT3="`echo \"${NUT_VERSION_DEFAULT3}\" | sed 's,\.[0-9][0-9]*[^.]*$,,'`"
        NUT_VERSION_DEFAULT3_DOTS="`expr $NUT_VERSION_DEFAULT3_DOTS - 1`"
    done

    DESC5="${NUT_VERSION_DEFAULT5}${SUFFIX}"
    DESC50="${NUT_VERSION_DEFAULT}${SUFFIX}"
    VER5="${NUT_VERSION_DEFAULT5}"
    VER50="${NUT_VERSION_DEFAULT}"
    BASE=""
    if [ -z "${SEMVER}" ] ; then
        SEMVER="${NUT_VERSION_DEFAULT3}"
    fi
    if [ -z "${TAG}" ] ; then
        TAG="v${NUT_VERSION_DEFAULT3}"
    fi
    if [ -z "${DESC}" ] ; then
        if [ -z "${SUFFIX_DESC}" ] ; then
            DESC="v${NUT_VERSION_DEFAULT3}${SUFFIX}"
        else
            DESC="v${NUT_VERSION_DEFAULT3}${SUFFIX_DESC}"
        fi
    fi
    if [ x"${TAG_PRERELEASE-}" = x ] ; then
        if [ x"${SUFFIX_PRERELEASE}" != x ] ; then
            TAG_PRERELEASE="v${NUT_VERSION_DEFAULT3}-${SUFFIX_PRERELEASE}"
        else
            TAG_PRERELEASE=""
        fi
    fi
}

report_debug() {
    # Debug
    echo "SEMVER=${SEMVER}; TRUNK='${NUT_VERSION_GIT_TRUNK-}'; BASE='${BASE}'; DESC='${DESC}' => TAG='${TAG}' + SUFFIX='${SUFFIX}' => VER5='${VER5}' => DESC5='${DESC5}' => VER50='${VER50}' => DESC50='${DESC50}'" >&2
}

report_output() {
    case "${NUT_VERSION_QUERY-}" in
        "DESC5")	echo "${DESC5}" ;;
        "DESC5x"|"DESC5X")	"${SEMVER_COMPARE}" --expand "${DESC5}" ;;
        "DESC50")	echo "${DESC50}" ;;
        "DESC50x"|"DESC50X")	"${SEMVER_COMPARE}" --expand "${DESC50}" ;;
        "VER5") 	echo "${VER5}" ;;
        "VER5x"|"VER5X")	"${SEMVER_COMPARE}" --expand "${VER5}" ;;
        "VER50")	echo "${VER50}" ;;
        "VER50x"|"VER50X")	"${SEMVER_COMPARE}" --expand "${VER50}" ;;
        "SEMVER")	echo "${SEMVER}" ;;
        "IS_RELEASE")	[ x"${SEMVER}" = x"${VER50}" ] && echo true || echo false ;;
        "IS_PRERELEASE")	[ x"${SUFFIX_PRERELEASE}" != x ] && echo true || echo false ;;
        "TAG")  	echo "${TAG}" ;;
        "TAG_PRERELEASE") echo "${TAG_PRERELEASE}" ;;
        "TRUNK")  	echo "${NUT_VERSION_GIT_TRUNK-}" ;;
        "SUFFIX")	echo "${SUFFIX}" ;;
        "SUFFIX_PRERELEASE") echo "${SUFFIX_PRERELEASE}" ;;
        "BASE") 	echo "${BASE}" ;;
        "URL")
            # Clarify the project website URL - particularly historically
            # frozen snapshots made for releases
            if [ x"${SEMVER}" = x"${VER50}" ] ; then
                echo "${NUT_WEBSITE}historic/v${SEMVER}/index.html"
            else
                echo "${NUT_WEBSITE}"
            fi
            ;;
        "UPDATE_FILE_GIT_RELEASE")
            # NOTE: For maintainers, changes SRCDIR not BUILDDIR; requires GIT
            # Do not "mv" here because maintainer files may be hard-linked from elsewhere
            echo "NUT_VERSION_FORCED='${DESC50}'" > "${abs_top_srcdir}/VERSION_FORCED.tmp" || exit
            if cmp "${abs_top_srcdir}/VERSION_FORCED.tmp" "${abs_top_srcdir}/VERSION_FORCED" >/dev/null 2>/dev/null ; then
                true
            else
                cat "${abs_top_srcdir}/VERSION_FORCED.tmp" > "${abs_top_srcdir}/VERSION_FORCED" || exit
            fi
            rm -f "${abs_top_srcdir}/VERSION_FORCED.tmp"

            echo "NUT_VERSION_FORCED_SEMVER='${SEMVER}'" > "${abs_top_srcdir}/VERSION_FORCED_SEMVER.tmp" || exit
            if cmp "${abs_top_srcdir}/VERSION_FORCED_SEMVER.tmp" "${abs_top_srcdir}/VERSION_FORCED_SEMVER" >/dev/null 2>/dev/null ; then
                true
            else
                cat "${abs_top_srcdir}/VERSION_FORCED_SEMVER.tmp" > "${abs_top_srcdir}/VERSION_FORCED_SEMVER" || exit
            fi
            rm -f "${abs_top_srcdir}/VERSION_FORCED_SEMVER.tmp"

            $GREP . "${abs_top_srcdir}/VERSION_FORCED" "${abs_top_srcdir}/VERSION_FORCED_SEMVER"
            ;;
        "UPDATE_FILE")
            if [ x"${abs_top_builddir}" != x"${abs_top_srcdir}" ] \
            && [ -s "${abs_top_srcdir}/VERSION_DEFAULT" ] \
            && [ ! -s "${abs_top_builddir}/VERSION_DEFAULT" ] \
            ; then
                cp -f "${abs_top_srcdir}/VERSION_DEFAULT" "${abs_top_builddir}/VERSION_DEFAULT" || exit
            fi

            echo "NUT_VERSION_DEFAULT='${DESC50}'" > "${abs_top_builddir}/VERSION_DEFAULT.tmp" || exit
            if cmp "${abs_top_builddir}/VERSION_DEFAULT.tmp" "${abs_top_builddir}/VERSION_DEFAULT" >/dev/null 2>/dev/null ; then
                rm -f "${abs_top_builddir}/VERSION_DEFAULT.tmp"
            else
                mv -f "${abs_top_builddir}/VERSION_DEFAULT.tmp" "${abs_top_builddir}/VERSION_DEFAULT" || exit
            fi
            cat "${abs_top_builddir}/VERSION_DEFAULT"
            ;;
        *)		echo "${DESC50}" ;;
    esac
}

if [ x"${NUT_VERSION_STRIP_LEADING_ZEROES-}" = xtrue ] ; then
    NUT_VERSION_DEFAULT="`\"${SEMVER_COMPARE}\" --strip \"${NUT_VERSION_DEFAULT-}\"`"
fi

DESC=""
NUT_VERSION_TRIED_GIT=false
if $NUT_VERSION_PREFER_GIT ; then
    if (command -v git && git rev-parse --show-toplevel) >/dev/null 2>/dev/null ; then
        getver_git || { echo "$0: Fall back to pre-set default version information" >&2 ; DESC=""; }
        NUT_VERSION_TRIED_GIT=true
    else
        NUT_VERSION_PREFER_GIT=false
    fi
fi

if [ x"$DESC" = x ]; then
    getver_default
    if $NUT_VERSION_TRIED_GIT ; then
        if CURRENT_COMMIT="`git log -1 --format='%h'`" && [ -n "${CURRENT_COMMIT}" ] ; then
            # Cases help rule out values populated via fallbacks from files;
            # try to not add inconsistencies though (only add if nobody has it)
            CAN_TACK=true
            case "$VER5" in
                *"+g"*) CAN_TACK=false ;;
            esac
            case "$VER50" in
                *"+g"*) CAN_TACK=false ;;
            esac
            case "$DESC5" in
                *"+g"*) CAN_TACK=false ;;
            esac
            case "$DESC50" in
                *"+g"*) CAN_TACK=false ;;
            esac

            if ${CAN_TACK} ; then
                echo "$0: Git failed originally, but current commit '${CURRENT_COMMIT}' is known (shallow checkout?); tack it to values that lack it" >&2
                case "$VER5" in
                    *"+g"*) ;;
                    *-{0,1,2,3,4,5,6,7,8,9}*)
                        VER5="${VER5}+g${CURRENT_COMMIT}" ;;
                    *)  VER5="${VER5}-0+g${CURRENT_COMMIT}" ;;
                esac
                case "$VER50" in
                    *"+g"*) ;;
                    *-{0,1,2,3,4,5,6,7,8,9}*)
                        VER50="${VER50}+g${CURRENT_COMMIT}" ;;
                    *)  VER50="${VER50}-0+g${CURRENT_COMMIT}" ;;
                esac
                case "$DESC5" in
                    *"+g"*) ;;
                    *-{0,1,2,3,4,5,6,7,8,9}*)
                        DESC5="${DESC5}+g${CURRENT_COMMIT}" ;;
                    *)  DESC5="${DESC5}-0+g${CURRENT_COMMIT}" ;;
                esac
                case "$DESC50" in
                    *"+g"*) ;;
                    *-{0,1,2,3,4,5,6,7,8,9}*)
                        DESC50="${DESC50}+g${CURRENT_COMMIT}" ;;
                    *)  DESC50="${DESC50}-0+g${CURRENT_COMMIT}" ;;
                esac
            fi
        fi
    fi
fi

report_debug
report_output

# Set exit code based on availability of default version info data point
# Not empty means good
# TOTHINK: consider the stdout of report_output() instead?
[ x"${DESC50}" != x ]
