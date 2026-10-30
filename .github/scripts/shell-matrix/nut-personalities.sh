#!/usr/bin/env bash

# Build NUT under other OSes' userland on one Linux machine.
#
# Usage:
#
#     nut-personalities.sh GITDIR COMMIT [PERSONALITY...]
#
# A personality is a directory of symlinks put first in PATH, so configure,
# the Makefile recipes and NUT's scripts all meet that OS's sh, make, sed,
# grep and awk (tools a personality does not replace stay GNU, from /usr/bin):
#
#   freebsd    FreeBSD 15 userland (chimerautils: sh, sed, grep, awk, find,
#              xargs, sort, tr, ...) + FreeBSD's bmake 20251111
#   netbsd9    dash (ash, like NetBSD sh) + bmake 20200710 (pre-2020-rewrite,
#              like NetBSD 9) + one-true-awk; sed/grep: FreeBSD's as proxy
#   netbsd11   same with bmake 20260912
#   openbsd    oksh as sh + OpenBSD make + one-true-awk; sed/grep: FreeBSD's
#   busybox    busybox for every applet it has (Alpine-like) + GNU make
#   mawk       dash + mawk (the awk on minimal Debian/Ubuntu) + GNU tools
#
# Without PERSONALITY arguments all of them run, one after another. The BSD
# tools come from build-bsd-tools.sh, in $BSD_PREFIX (default /opt/bsd);
# netbsd9 uses /usr/bin/bmake (Debian/Ubuntu's bmake package), and the awks
# are Debian/Ubuntu's original-awk and mawk packages. autogen.sh runs with
# the autotools in PATH, or those in $AUTOTOOLS_BIN if set.
#
# Per personality, on its own in-tree copy of one autogen'd tree (BSD makes
# and VPATH builds are a separate question): configure with that sh, make
# -j all with that make, then tests/nut-driver-enumerator-test.sh with that
# sh and PATH, since that script is where NUT leans on sed/grep/awk at run
# time. Set NUT_MONITOR=yes to configure with --with-nut_monitor=yes (needs
# Python 3, PyQt5 and msgfmt).
#
# Work trees and logs go to $WORKDIR/pers-<commit> (default $HOME/shm).
# Exit code is 1 if any step failed. In GitHub Actions (GITHUB_ACTIONS=true)
# failures are also printed as error annotations (OpenBSD make's dependency
# cycles on the rule in Makefile.am) and written to the job summary; see
# report.sh.

set -uo pipefail

if [ $# -lt 2 ]; then
  sed -n 's/^#     //p' "$0" >&2; exit 2
fi
GITDIR=$1 COMMIT=$2; shift 2
ALL=(freebsd netbsd9 netbsd11 openbsd busybox mawk)
PERS=("$@"); [ ${#PERS[@]} -gt 0 ] || PERS=("${ALL[@]}")
BSD=${BSD_PREFIX:-/opt/bsd}
FB=$BSD/freebsd/bin
WORK=${WORKDIR:-$HOME/shm}/pers-$(git -C "$GITDIR" rev-parse --short "$COMMIT")
PD=$WORK/pers                       # personality PATH dirs
JOBS=$(nproc)
# NUT-Monitor on: its .mo rules are where BSD makes differ (OpenBSD make
# finds a dependency cycle there), and it is "auto" otherwise, so off
# wherever Python 3 + PyQt5 + msgfmt are missing (needs all three)
CONFIG_OPTS=(--with-all=no --with-doc=no --with-libltdl=no --disable-silent-rules
             ${NUT_MONITOR:+--with-nut_monitor=$NUT_MONITOR})

# (ShellCheck checks report.sh on its own; GHA-07 runs it without -x)
# shellcheck source-path=SCRIPTDIR source=report.sh disable=SC1091
. "$(dirname "$0")/report.sh"

# annotate_cycles <build dir> <make log>: put each "Cycle found" of OpenBSD
# make on the rule in the Makefile.am of the directory it happened in
annotate_cycles() {
  local b=$1 line tgt path dir rel am n
  while IFS= read -r line; do
    # "Cycle found: all-am -> <target> -> (<abs path> -> <abs path>)"
    tgt=$(sed -n 's/^Cycle found: [^ ]* -> \([^ ]*\) -> .*/\1/p' <<<"$line")
    path=$(sed -n 's/.* -> (\([^ ]*\) -> .*/\1/p' <<<"$line")
    rel=${path#"$b"/}
    if [ -z "$tgt" ] || [ "$rel" = "$path" ]; then continue; fi
    dir=${rel%/"$tgt"}
    am=$dir/Makefile.am
    [ -f "$WORK/src/$am" ] || continue
    n=$(grep -n -m1 -F "$tgt:" "$WORK/src/$am" | cut -d: -f1)
    annotate "$am" "${n:-1}" "OpenBSD make: ${line//"$b"\//}"
  done < <(grep '^Cycle found' "$2" | sort -u)
}

mkpers() {  # mkpers <name>: (re)create its PATH dir
  local d=$PD/$1/bin; rm -rf "${PD:?}/$1"; mkdir -p "$d"
  ln_() { ln -sf "$1" "$d/$2"; }
  case $1 in
    freebsd)
      for f in "$FB"/*; do ln_ "$f" "$(basename "$f")"; done
      ln_ "$BSD/bmake-20251111/bin/bmake" make ;;
    netbsd9|netbsd11)
      ln_ /usr/bin/dash sh; ln_ /usr/bin/original-awk awk
      ln_ "$FB/sed" sed; ln_ "$FB/grep" grep
      if [ "$1" = netbsd9 ]; then ln_ /usr/bin/bmake make
      else local bm=("$BSD"/bmake-2026*/bin/bmake); ln_ "${bm[-1]}" make; fi ;;
    openbsd)
      ln_ "$BSD/oksh/bin/oksh" sh; ln_ "$BSD/obsdmake/bin/omake" make
      ln_ /usr/bin/original-awk awk; ln_ "$FB/sed" sed; ln_ "$FB/grep" grep ;;
    busybox)
      # applets busybox has, minus the ones that would replace the C
      # toolchain's own helpers or the package tools
      for a in $(busybox --list); do
        case $a in ar|strings|patch|make|xz|unxz|lzma|tar|install|busybox) continue;; esac
        printf '#!/bin/sh\nexec busybox %s "$@"\n' "$a" > "$d/$a"; chmod +x "$d/$a"
      done
      ln_ /usr/bin/make make ;;
    mawk)
      ln_ /usr/bin/dash sh; ln_ /usr/bin/mawk awk; ln_ /usr/bin/make make ;;
    *)
      echo "unknown personality: $1" >&2; return 1 ;;
  esac
  # egrep/fgrep as POSIX spells them, on whichever grep is in place
  for g in egrep:-E fgrep:-F; do
    [ -e "$d/${g%%:*}" ] || printf '#!/bin/sh\nexec grep %s "$@"\n' "${g#*:}" > "$d/${g%%:*}"
    chmod +x "$d/${g%%:*}"
  done
  # every link must point at an installed tool
  local l; for l in "$d"/*; do [ -e "$l" ] || { echo "missing tool for $1: $l -> $(readlink "$l")" >&2; return 1; }; done
  # and the make and sh must run on this machine
  printf 'all:\n\t@true\n' | "$d/make" -f - >/dev/null 2>&1 || { echo "$1: make does not run: $(readlink -f "$d/make")" >&2; return 1; }
  "$d/sh" -c true || { echo "$1: sh does not run: $(readlink -f "$d/sh")" >&2; return 1; }
}

mkver() {  # bmake's MAKE_VERSION, GNU make's --version, else the binary's path
  local v; v=$("$1" -V MAKE_VERSION -f /dev/null 2>/dev/null)
  [ -n "$v" ] || v=$("$1" --version 2>/dev/null | head -1)
  [ -n "$v" ] || v=$(readlink -f "$1")
  echo "$v"
}

ver() {  # one-line identity of the personality's tools
  local d=$PD/$1/bin
  printf 'sh=%s make=%s awk=%s sed=%s grep=%s' \
    "$(readlink -f "$d/sh" 2>/dev/null || echo busybox)" \
    "$(mkver "$d/make")" \
    "$(basename "$(readlink -f "$d/awk" 2>/dev/null || echo busybox)")" \
    "$(readlink -f "$d/sed" 2>/dev/null | sed "s|$FB/sed|freebsd-sed|" || echo busybox)" \
    "$("$d/grep" --version 2>/dev/null | head -1 | cut -c1-40)"
}

report_init "Userland personality ${PERS[*]}"

rm -rf "$WORK"; mkdir -p "$WORK"; git -C "$GITDIR" worktree prune

# one autogen'd tree; each personality gets a plain copy of it
git -C "$GITDIR" worktree add -f -q --detach "$WORK/src" "$COMMIT"
step "autogen ($(${AUTOTOOLS_BIN:+$AUTOTOOLS_BIN/}autoconf --version | sed -n '1s/.* //p'))"
if (cd "$WORK/src" && PATH=${AUTOTOOLS_BIN:+$AUTOTOOLS_BIN:}$PATH ./autogen.sh) > "$WORK/autogen.log" 2>&1; then ok
else bad "$(grep -m1 "^FAILED" "$WORK/autogen.log" || tail -1 "$WORK/autogen.log")"; detail "$(tail -20 "$WORK/autogen.log")"; exit 1; fi

for p in "${PERS[@]}"; do
  step "[$p] tools"
  if out=$(mkpers "$p" 2>&1); then ok; echo "    $(ver "$p")"
  else bad "$out"; continue; fi

  B=$WORK/$p PP=$PD/$p/bin:$PATH
  cp -a "$WORK/src" "$B"

  step "[$p] configure"
  # the personality's own tools, named explicitly too: configure's
  # AC_PROG_AWK etc. would otherwise go looking for gawk first
  if (cd "$B" && PATH=$PP CONFIG_SHELL=$PD/$p/bin/sh AWK=awk SED=sed GREP=grep EGREP="grep -E" \
        "$PD/$p/bin/sh" ./configure "${CONFIG_OPTS[@]}") > "$B.configure.log" 2>&1; then ok
  else
    bad "$(tail -1 "$B.configure.log" | cut -c1-100)"
    detail "$(tail -15 "$B.configure.log")"; continue
  fi

  step "[$p] make -j$JOBS all"
  if (cd "$B" && PATH=$PP make -j"$JOBS" all) > "$B.make.log" 2>&1; then ok
  else
    bad "$( (grep -m1 -E 'Cycle found|\*\*\* |Stop\.|: error:' "$B.make.log" || tail -1 "$B.make.log") | sed "s|$B/||g" | cut -c1-200)"
    annotate_cycles "$B" "$B.make.log"
    detail "$(grep -E 'Cycle found|\*\*\* |Stop\.|: error:' "$B.make.log" | sed "s|$B/||g" | head -20)"
  fi

  step "[$p] nut-driver-enumerator-test"
  if (cd "$B/tests" && PATH=$PP SERVICE_FRAMEWORK=selftest SHELL_PROGS=$PD/$p/bin/sh \
        NDE="$B/scripts/upsdrvsvcctl/nut-driver-enumerator.sh" ./nut-driver-enumerator-test.sh) > "$B.nde.log" 2>&1; then ok
  else
    bad "$(sed -n 's/.*completed with \([0-9]* failed cases\).*/\1/p' "$B.nde.log")"
    detail "$(grep '^Testing' "$B.nde.log" | grep -v "STATUS_CODE='MATCHED'" | head -20)"
  fi
done

report_exit
