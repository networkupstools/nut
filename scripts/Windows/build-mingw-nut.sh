#!/bin/bash
# script to cross compile NUT for Windows from Linux using MinGW-w64
# http://mingw-w64.sourceforge.net/

#set -x

# default to update source then build
WINDIR=$(pwd)
TOP_DIR=$WINDIR/../..
BUILD_DIR=$WINDIR/nut_build
INSTALL_DIR=$WINDIR/nut_install

VER_OPT=2.6.5
DEBUG=true

# default to 32bits build
cmd=all32
if [ -n "$1" ] ; then
	cmd=$1
fi

# FIXME
# Stable version (download the latest stable archive)
#	if [ ! -f nut-$VER_OPT.tar.gz ] ; then
#		wget http://www.networkupstools.org/source/2.6/nut-$VER_OPT.tar.gz
#	fi
#	rm -rf nut-$VER_OPT
#	tar -xzf nut-$VER_OPT.tar.gz
#	mv nut-$VER_OPT $BUILD_DIR

# In-place version (no download)
rm -rf $BUILD_DIR $INSTALL_DIR
cd ../..
rm -f nut-?.?.?.tar.gz
make dist
SRC_ARCHIVE=$(ls nut-?.?.?.tar.gz)
cd scripts/Windows
tar -xzf ../../$SRC_ARCHIVE
mv nut-?.?.? $BUILD_DIR
cd $BUILD_DIR

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
	export PATH=/usr/$ARCH/bin:$PATH

	export CFLAGS+="-D_POSIX=1 -I/usr/$ARCH/include/"
	export LDFLAGS+="-L/usr/$ARCH/lib/"
	./configure $HOST_FLAG $BUILD_FLAG --prefix=$INSTALL_DIR
	make 1>/dev/null
	make install
	cd ..
else
	echo "Usage:"
	echo "		$0 [all64 | b64 | all32 | b32]"
	echo "		Default: 'all32'"
fi
