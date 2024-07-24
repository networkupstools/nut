#!/bin/sh

# Copyright (C) 2016-2024 by Jim Klimov <jimklimov+nut@gmail.com>
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
# Helper script to determine the project version in a manner similar to
# what `git describe` produces, but with added numbers after the common
# triplet of semantically versioned numbers:   X.Y.Z.T.B(-C-gHASH)
#   * X: MAJOR - incompatible API changes
#   * Y: MINOR - new features and/or API
#   * Z: PATCH - small bug fixes
#   * T: Commits on trunk since previous release tag
#   * B: Commits on branch since nearest ancestor which is on trunk
# The optional suffix (only for commits which are not tags themselves)
# is provided by `git describe`:
#   * C: Commits on branch since previous release tag
#   * H: Git hash (prefixed by "g" character) of the described commit
# Note that historically NUT did not diligently follow the semver triplet,
# primarily because a snapshot of trunk is tested and released, and work
# moves on with the PATCH part (rarely MINOR one) incremented; no actual
# patches are released to some sustaining track of an older release lineage.
# There were large re-designs that got MAJOR up to 2, though.
#
############################################################################
# Checked with bash 3 and 5, dash, ksh, zsh and even busybox sh;
# OpenIndiana, FreeBSD and OpenBSD sh. Not compatible with csh and tcsh.
# See some examples in https://github.com/networkupstools/nut/issues/1949

############################################################################
# Numeric-only default version, for AC_INIT and similar consumers
# in case we build without a Git workspace (from tarball, etc.)
# By legacy convention, 3-digit "semver" was for NUT releases, and
# a nominal "semver.1" for any development snapshots afterwards.
[ -n "${DEFAULT_VERSION-}" ] || DEFAULT_VERSION='2.8.2.1'

getver() (
    # NOTE: The chosen trunk branch must be up to date (may be "origin/master"
    # or "upstream/master", etc.) for resulting version discovery to make sense.
    [ x"${TRUNK-}" != x ] || TRUNK=master

    # How much of the known trunk history is in current HEAD?
    # e.g. all of it when we are on that branch or PR made from its tip,
    # some of it if looking at a historic snapshot, or nothing if looking
    # at the tagged commit (it is the merge base for itself and any of
    # its descendants):
    BASE="`git merge-base HEAD "${TRUNK}"`"

    # By default, only annotated tags are considered
    ALL_TAGS_ARG=""
    if [ x"${ALL_TAGS-}" = xtrue ] ; then ALL_TAGS_ARG="--tags" ; fi

    DESC="`git describe $ALL_TAGS_ARG --match 'v[0-9]*.[0-9]*.[0-9]' --exclude '*-signed' --exclude '*rc*' --exclude '*alpha*' --exclude '*beta*' --exclude '*Windows*' --exclude '*IPM*' --always`"
    # Old stripper (also for possible refspec parts like "tags/"):
    #   echo "${DESC}" | sed -e 's/^v\([0-9]\)/\1/' -e 's,^.*/,,'

    # Nearest (annotated by default) tag preceding the HEAD in history:
    TAG="`echo "${DESC}" | sed 's/-[0-9][0-9]*-g[0-9a-fA-F][0-9a-fA-F]*$//'`"

    # Commit count since the tag and hash of the HEAD commit;
    # empty e.g. when HEAD is the tagged commit:
    SUFFIX="`echo "${DESC}" | sed 's/^.*\(-[0-9][0-9]*-g[0-9a-fA-F][0-9a-fA-F]*\)$/\1/'`" && [ x"${SUFFIX}" != x"${TAG}" ] || SUFFIX=""

    # 5-digit version, note we strip leading "v" from the expected TAG value
    VER5="${TAG#v}.`git log --oneline "${TAG}..${BASE}" | wc -l | tr -d ' '`.`git log --oneline "${TRUNK}..HEAD" | wc -l | tr -d ' '`"
    DESC5="${VER5}${SUFFIX}"

    # Strip trailing zeroes for trunk snapshots and releases
    VER50="`echo "${VER5}" | sed -e 's/\.0$//' -e 's/\.0$//'`"
    DESC50="${VER50}${SUFFIX}"

    # Debug
    echo "DESC='${DESC}' => TAG='${TAG}' + SUFFIX='${SUFFIX}'; BASE='${BASE}' => VER5='${VER5}' => VER50='${VER50}' => DESC50='${DESC50}'" >&2

    # echo "${DESC5}"
    echo "${DESC50}"
)

if (command -v git && git rev-parse --show-toplevel) >/dev/null 2>/dev/null ; then
    getver
    exit
fi

echo "${DEFAULT_VERSION-}"
