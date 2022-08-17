#!/bin/bash
# script to cross compile NUT for Windows from Linux using MinGW-w64
# http://mingw-w64.sourceforge.net/

#set -x

SCRIPTDIR="`dirname "$0"`"
SCRIPTDIR="`cd "$SCRIPTDIR" && pwd`"

DLLLDD_SOURCED=true . "${SCRIPTDIR}/dllldd.sh"

# default to update source then build
WINDIR=$(pwd)
TOP_DIR=$WINDIR/../..
BUILD_DIR=$WINDIR/nut_build
INSTALL_DIR=$WINDIR/nut_install

# This should match the tarball and directory name,
# if a stable version is used:
[ -n "$VER_OPT" ] || VER_OPT=2.8.0
DEBUG=true

# default to 32bits build
# Note: README specifies dependencies to pre-build and install;
# those DLLs should correspond to same architecture selection
cmd=all32
if [ -n "$1" ] ; then
	cmd=$1
fi

[ -n "$SOURCEMODE" ] || SOURCEMODE="out-of-tree"

rm -rf $BUILD_DIR $INSTALL_DIR
CONFIGURE_SCRIPT="./configure"
case "$SOURCEMODE" in
stable)
# FIXME
# Stable version (download the latest stable archive)
	VER_OPT_SHORT="`echo "$VER_OPT" | awk -F. '{print $1"."$2}'`"
	if [ ! -s nut-$VER_OPT.tar.gz ] ; then
		wget http://www.networkupstools.org/source/$VER_OPT_SHORT/nut-$VER_OPT.tar.gz
	fi
	rm -rf nut-$VER_OPT
	tar -xzf nut-$VER_OPT.tar.gz
	mv nut-$VER_OPT $BUILD_DIR
	;;
dist)
	# In-place version (no download)
	cd ../..
	rm -f nut-?.?.?*.tar.gz
	[ -s Makefile ] || { ./autogen.sh && ./configure; }
	make dist
	SRC_ARCHIVE=$(ls -1 nut-?.?.?*.tar.gz | sort -n | tail -1)
	cd scripts/Windows
	tar -xzf ../../$SRC_ARCHIVE
	mv nut-?.?.?* $BUILD_DIR
	;;
out-of-tree)
	CONFIGURE_SCRIPT="../../../configure"
	cd ../..
	if [ ! -x ./configure ]; then
		./autogen.sh
	fi
	if [ -s Makefile ]; then
		make distclean
	fi
	cd scripts/Windows
	mkdir -p $BUILD_DIR
	;;
esac

cd $BUILD_DIR || exit

if [ "$cmd" == "all64" ] || [ "$cmd" == "b64" ] || [ "$cmd" == "all32" ] || [ "$cmd" == "b32" ] ; then
	ARCH="x86_64-w64-mingw32"
	if [ "$cmd" == "all32" ] || [ "$cmd" == "b32" ] ; then
		ARCH="i686-w64-mingw32"
	fi

	HOST_FLAG="--host=$ARCH"
	# --build needs to be specified, beside of --host, to avoid Warning
	# but this version is very Debian specific!!!
	# FIXME: find something more generic
	BUILD_FLAG="--build=`dpkg-architecture -qDEB_BUILD_GNU_TYPE`"
	export CC="$ARCH-gcc"
	export CXX="$ARCH-g++"
	export PATH="/usr/$ARCH/bin:$PATH"

	# Note: _WIN32_WINNT>=0x0600 is needed for inet_ntop in mingw headers
	# and the value 0xffff is anyway forced into some components at least
	# by netsnmp cflags.
	export CFLAGS+=" -D_POSIX=1 -D_POSIX_C_SOURCE=200112L -I/usr/$ARCH/include/ -D_WIN32_WINNT=0xffff"
	export CXXFLAGS+=" $CFLAGS"
	export LDFLAGS+=" -L/usr/$ARCH/lib/"
	$CONFIGURE_SCRIPT $HOST_FLAG $BUILD_FLAG --prefix=$INSTALL_DIR \
	    PKG_CONFIG_PATH=/usr/$ARCH/lib/pkgconfig \
	    --without-pkg-config --with-all=auto \
	    --without-systemdsystemunitdir \
	    --with-pynut=app \
	    --with-augeas-lenses-dir=$INSTALL_DIR/augeas-lenses \
	    --enable-Werror \
	|| exit
	make 1>/dev/null || exit
	make install || exit

	# Per docs, Windows loads DLLs from EXE file's dir or some
	# system locations or finally PATH, so unless the caller set
	# the latter, we can not load the pre-linked DLLs from ../lib:
	#   http://msdn.microsoft.com/en-us/library/windows/desktop/ms682586(v=vs.85).aspx#standard_search_order_for_desktop_applications

	# Be sure upsmon can run even if at cost of some duplication
	# (maybe even do "cp -pf" if some system dislikes "ln"); also
	# on a modern Windows one could go to their installed "sbin" to
	#   mklink .\libupsclient-3.dll ..\bin\libupsclient-3.dll
	(cd $INSTALL_DIR/bin && ln libupsclient*.dll ../sbin/)

	# Cover dependencies for nut-scanner (not pre-linked)
	# Note: lib*snmp*.dll not listed below, it is
	# statically linked into binaries that use it
	(cd $INSTALL_DIR/bin && cp -pf /usr/$ARCH/bin/{libgnurx,libusb,libltdl}*.dll .) || true
	(cd $INSTALL_DIR/bin && cp -pf /usr/$ARCH/lib/libwinpthread*.dll .) || true

	# Steam-roll over all executables/libs we have here and copy
	# over resolved dependencies from the cross-build environment:
	(cd $INSTALL_DIR && { dllldddir . | while read D ; do cp -pf "$D" ./bin/ ; done ; } ) || true

	# Hardlink libraries for sbin (alternative: all bins in one dir):
	(cd $INSTALL_DIR/sbin && { dllldddir . | while read D ; do ln ../bin/"`basename "$D"`" ./ ; done ; } ) || true
	cd ..
else
	echo "Usage:"
	echo "		$0 [all64 | b64 | all32 | b32]"
	echo "		Default: 'all32'"
	echo "Optionally export SOURCEMODE=[stable|dist|out-of-tree]"
fi
