#!/usr/bin/env bash

# Build several autoconf/automake/libtool generations side by side, each
# set in its own prefix ($AT_PREFIX/<set>, default /opt/at/<set>), so its
# aclocal sees only its own libtool.m4. Tarballs are verified against the
# GNU keyring.
#
# Usage:
#
#     build-autotools.sh [SET...]
#
# Without arguments all sets are built. Sets already built are skipped.
# Used by the "GHA-10: Shell portability" workflow, see nut-sh-matrix.sh.

set -euo pipefail

SETS=(
  # name  autoconf automake libtool   (what ships it)
  "c7     2.69 1.13.4 2.4.2"    # CentOS 7
  "u18    2.69 1.15.1 2.4.6"    # Ubuntu 18.04 / Debian 10
  "u22    2.71 1.16.5 2.4.7"    # Ubuntu 22.04 / Debian 12
  "u26    2.72 1.18.1 2.5.4"    # Ubuntu 26.04 / Debian 13+
)
# any mirror will do: every tarball is checked against the GNU keyring.
# ftpmirror.gnu.org redirects to a nearby mirror, which can be down;
# ftp.gnu.org itself is the fallback.
GNU_SITES=(https://ftpmirror.gnu.org https://ftp.gnu.org/gnu)
AT_PREFIX=${AT_PREFIX:-/opt/at}
SRC=${AT_SRC:-$HOME/at-src}
mkdir -p "$SRC" && cd "$SRC"

[ -f gnu-keyring.gpg ] || curl -fsSL --retry 5 -O https://ftp.gnu.org/gnu/gnu-keyring.gpg

fetch() {  # fetch <pkg> <version>
  local t=$1-$2.tar.gz site
  if [ ! -f "$t.sig" ]; then
    for site in "${GNU_SITES[@]}"; do
      if curl -fsSL --connect-timeout 20 --retry 3 -o "$t" "$site/$1/$t" &&
         curl -fsSL --connect-timeout 20 --retry 3 -o "$t.sig" "$site/$1/$t.sig"; then
        break
      fi
      rm -f "$t" "$t.sig"; echo "download from $site failed: $t" >&2
    done
    [ -f "$t.sig" ] || { echo "download failed: $t" >&2; exit 1; }
  fi
  gpgv --keyring ./gnu-keyring.gpg "$t.sig" "$t" 2>/dev/null || { echo "BAD SIGNATURE: $t" >&2; exit 1; }
  rm -rf "$1-$2"; tar xzf "$t"
}

for s in "${SETS[@]}"; do
  read -r name ac am lt <<<"$s"
  if [ $# -gt 0 ]; then
    found=no; for w in "$@"; do [ "$w" = "$name" ] && found=yes; done
    [ $found = yes ] || continue
  fi
  P=$AT_PREFIX/$name
  [ -x "$P/bin/libtoolize" ] && { echo "== $name already built"; continue; }
  echo "== $name: autoconf $ac, automake $am, libtool $lt"
  sudo mkdir -p "$P" && sudo chown "$(id -u):$(id -g)" "$P"
  (
    export PATH=$P/bin:/usr/bin:/bin

    fetch autoconf "$ac"
    (cd "autoconf-$ac" && ./configure -q --prefix="$P" && make -s && make -s install) >/dev/null

    fetch automake "$am"
    (cd "automake-$am" && ./configure -q --prefix="$P" && make -s && make -s install) >/dev/null
    # automake < 1.15.1 on perl >= 5.26: "Unescaped left brace in regex",
    # upstream's own fix (automake commit 13f00eb4)
    sed -i 's/s\/\\${(\[^ \\t=:+{}\]+)}\//s\/\\$\\{([^ \\t=:+{}]+)\\}\//' "$P/bin/automake"

    fetch libtool "$lt"
    (cd "libtool-$lt" && ./configure -q --prefix="$P" && make -s && make -s install) >/dev/null

    # third-party macros NUT's configure.ac uses, from the distro, not the
    # system aclocal dir as a whole (that one has its own libtool.m4)
    cp /usr/share/aclocal/pkg.m4 "$P/share/aclocal/"
    echo "   $("$P/bin/autoconf" --version | head -1) / $("$P/bin/automake" --version | head -1) / $("$P/bin/libtool" --version | head -1)"
  )
done
