#!/usr/bin/env bash

# Build BSD (and other non-GNU) userland pieces on Linux, each in its own
# prefix under $BSD_PREFIX (default /opt/bsd), so nothing shadows the
# system tools by accident:
#
#   freebsd/    chimerautils: FreeBSD 15's sh, sed, grep, awk, find, xargs...
#   oksh/       OpenBSD's ksh (OpenBSD /bin/sh), portable
#   obsdmake/   OpenBSD make, ibara/make portable (synced to OpenBSD 6.9)
#   bmake-*/    bmake = NetBSD make = FreeBSD make, at the versions shipped:
#               20251111 FreeBSD 14/15 (from FreeBSD's own contrib/bmake),
#               20260912 newest release (~ NetBSD 11/-current). Ubuntu's
#               own bmake 20200710 predates the 2020 rewrite, like NetBSD 9.
#
# Usage:
#
#     build-bsd-tools.sh
#
# Git sources are pinned to commits, tarballs by sha256. Pieces already
# built are skipped. Needs (Debian/Ubuntu package names): meson ninja-build
# flex byacc libedit-dev libbsd-dev libssl-dev libncurses-dev libacl1-dev
# musl-tools pkgconf. Used by the "GHA-10: Shell portability" workflow,
# see nut-personalities.sh.

set -euo pipefail

CHIMERAUTILS_REF=6937121c68e2ae4d3741280671a98d7ebfbf0d56   # tag v15.1.2
OBSDMAKE_REF=fc8b50c6dea7                                   # ibara/make master, 2021-10-28
FREEBSD_REF=6ede1a5a70b6cc50bb614583c7e68817bb12136e        # stable/15, contrib/bmake 20251111
LIBXO_VER=2.3.0
LIBXO_SHA256=f688acfbad07ba14826871437b0431bc0425d5732457013af5aff07236810f65
OKSH_VER=7.9
OKSH_SHA256=51b2d92515950c959dbf24f6fc33336db8c0526c2a50fee4ca598a18a6114a49
BMAKE_NEW=20260912
BMAKE_NEW_SHA256=b6bd32964cbe451be2838822c9d200b7c7e76a2a5947c03feb71dc6bd72988bd

BSD=${BSD_PREFIX:-/opt/bsd}
SRC=${BSD_SRC:-$HOME/bsd-src}; mkdir -p "$SRC"; cd "$SRC"
sudo mkdir -p "$BSD" && sudo chown "$(id -u):$(id -g)" "$BSD"

gitpin() {  # gitpin <url> <dir> <commit>
  [ -d "$2/.git" ] || git clone -q "$1" "$2"
  git -C "$2" checkout -q "$3"
}

fetch() {  # fetch <url> <sha256>: download to the url's basename, verified
  local f; f=$(basename "$1")
  [ -f "$f" ] || curl -fsSL --retry 5 -o "$f" "$1"
  echo "$2  $f" | sha256sum -c --quiet - || { echo "BAD CHECKSUM: $f" >&2; rm -f "$f"; exit 1; }
}

# libxo, needed by chimerautils (not packaged on Ubuntu)
if [ ! -f "$BSD/libxo/lib/libxo.so" ]; then
  echo "== libxo $LIBXO_VER"
  fetch https://github.com/Juniper/libxo/releases/download/$LIBXO_VER/libxo-$LIBXO_VER.tar.gz $LIBXO_SHA256
  rm -rf libxo-$LIBXO_VER && tar xzf libxo-$LIBXO_VER.tar.gz
  (cd libxo-$LIBXO_VER && ./configure -q --prefix="$BSD/libxo" && make -s -j"$(nproc)" && make -s install) >/dev/null 2>&1
fi

if [ ! -x "$BSD/freebsd/bin/sed" ]; then
  echo "== chimerautils (FreeBSD 15 userland)"
  gitpin https://github.com/chimera-linux/chimerautils.git chimerautils $CHIMERAUTILS_REF
  rm -rf chimerautils/build
  (cd chimerautils &&
   PKG_CONFIG_PATH=$BSD/libxo/lib/pkgconfig LDFLAGS=-Wl,-rpath,$BSD/libxo/lib \
     meson setup build --prefix="$BSD/freebsd" -Dpam=disabled -Dselinux=disabled -Dzlib=disabled -Dlzma=disabled -Dbzip2=disabled -Dzstd=disabled) >/dev/null
  # -k 0: ln's one C++ file does not build against GCC 15's libstdc++
  # headers (with clang either), which also stops `ninja install`; the
  # tools needed here are plain C, so take the binaries as built
  ninja -k 0 -C chimerautils/build >/dev/null 2>&1 || true
  mkdir -p "$BSD/freebsd/bin"
  find chimerautils/build/src.freebsd -maxdepth 3 -type f -perm -u+x ! -name '*.so*' \
    -exec cp {} "$BSD/freebsd/bin/" \;
  for t in sh sed grep awk; do [ -x "$BSD/freebsd/bin/$t" ] || { echo "chimerautils: no $t built" >&2; exit 1; }; done
fi

if [ ! -x "$BSD/oksh/bin/oksh" ]; then
  echo "== oksh $OKSH_VER"
  fetch https://github.com/ibara/oksh/releases/download/oksh-$OKSH_VER/oksh-$OKSH_VER.tar.gz $OKSH_SHA256
  rm -rf oksh-$OKSH_VER && tar xzf oksh-$OKSH_VER.tar.gz
  (cd oksh-$OKSH_VER && ./configure --prefix="$BSD/oksh" && make -s -j"$(nproc)" && make -s install) >/dev/null
fi

if [ ! -x "$BSD/obsdmake/bin/omake" ]; then
  echo "== OpenBSD make (ibara/make $OBSDMAKE_REF)"
  gitpin https://github.com/ibara/make.git obsdmake $OBSDMAKE_REF
  # Its Linux support means musl: glibc has no optreset (BSD getopt reset)
  # and no sys_signame. Against today's musl and GCC it also needs:
  #  - <time.h> forced in (arch.c/compat.c use time(), CLOCK_REALTIME
  #    without it, and implicit declarations are now an error)
  #  - its own sys_signame (NEED_SYS_SIGNAME, as its configure does for
  #    Cygwin), and the <sys/cdefs.h> macros musl does not have
  #  - arc4random_uniform (musl has none): only picks random job order;
  #    WAIT_ANY (-1, a BSD/glibc spelling)
  #  - -j1: its Makefile runs ./generate before it is necessarily built
  #  - -static: other machines (CI jobs given these tools) lack the musl loader
  # configure's probes run with plain cc; musl agrees on each of them.
  cat > obsdmake/compat-musl.h <<'EOF'
#include <time.h>
#define __BEGIN_DECLS
#define __END_DECLS
#define __dead __attribute__((__noreturn__))
#include <stdlib.h>
#define arc4random_uniform(n) ((unsigned)random() % (n))
#define WAIT_ANY (-1)
EOF
  (cd obsdmake && git clean -qfdx -e compat-musl.h && ./configure --prefix="$BSD/obsdmake" --mandir="$BSD/obsdmake/man" &&
   sed -i 's/^CC=.*/CC=\tmusl-gcc -static/; s/^CFLAGS=\t-O2 -pipe -std=c99/CFLAGS=\t-O2 -pipe -std=gnu99 -include compat-musl.h -DNEED_SYS_SIGNAME/' Makefile &&
   make -s && make -s install) >/dev/null 2>&1
fi

# boot-strap builds in an object dir *beside* the source tree
# (../<os>-<arch>), so each version gets a parent dir of its own; sharing
# one silently tested one version's objects against another's tests
bmake_build() {  # bmake_build <srcdir> <version>
  local P=$BSD/bmake-$2
  # boot-strap runs bmake's unit tests (they must pass); LUA=false skips
  # only their optional Lua cross-check, which breaks on Lua 5.4
  (cd "$1" && LUA=false ./boot-strap --prefix="$P" op=build >/dev/null 2>&1 &&
   ./boot-strap --prefix="$P" --install-destdir= op=install >/dev/null)
}

if [ ! -x "$BSD/bmake-20251111/bin/bmake" ]; then
  echo "== bmake 20251111, FreeBSD stable/15's contrib/bmake"
  if [ ! -d freebsd-bmake/.git ]; then
    git init -q freebsd-bmake
    git -C freebsd-bmake remote add origin https://github.com/freebsd/freebsd-src.git
    git -C freebsd-bmake sparse-checkout set contrib/bmake
    git -C freebsd-bmake fetch -q --depth 1 --filter=blob:none origin $FREEBSD_REF
  fi
  git -C freebsd-bmake checkout -q $FREEBSD_REF
  grep -q '_MAKE_VERSION=20251111' freebsd-bmake/contrib/bmake/VERSION
  rm -rf bmake-20251111 && mkdir bmake-20251111 && cp -a freebsd-bmake/contrib/bmake bmake-20251111/src
  bmake_build bmake-20251111/src 20251111
fi

if [ ! -x "$BSD/bmake-$BMAKE_NEW/bin/bmake" ]; then
  echo "== bmake $BMAKE_NEW"
  fetch https://www.crufty.net/ftp/pub/sjg/bmake-$BMAKE_NEW.tar.gz $BMAKE_NEW_SHA256
  rm -rf bmake bmake-$BMAKE_NEW && mkdir bmake-$BMAKE_NEW && tar xzf bmake-$BMAKE_NEW.tar.gz -C bmake-$BMAKE_NEW
  mv bmake-$BMAKE_NEW/bmake bmake-$BMAKE_NEW/src
  bmake_build bmake-$BMAKE_NEW/src $BMAKE_NEW
fi

echo "--- installed"
missing=0
for f in "$BSD"/freebsd/bin/{sh,sed,grep,awk} "$BSD/oksh/bin/oksh" "$BSD/obsdmake/bin/omake" \
         "$BSD/bmake-20251111/bin/bmake" "$BSD/bmake-$BMAKE_NEW/bin/bmake"; do
  if [ -x "$f" ]; then echo "  ok  $f"; else echo "  MISSING $f"; missing=1; fi
done
for b in "$BSD"/bmake-*/bin/bmake; do echo "  $b: $("$b" -V MAKE_VERSION -f /dev/null)"; done
# each make must run a trivial makefile
for m in "$BSD/obsdmake/bin/omake" "$BSD"/bmake-*/bin/bmake; do
  printf 'all:\n\t@true\n' | "$m" -f - >/dev/null 2>&1 || { echo "  BROKEN $m" >&2; missing=1; }
done
exit $missing
