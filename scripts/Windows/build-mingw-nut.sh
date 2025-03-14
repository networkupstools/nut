#!/usr/bin/env bash

# NOTE: bash syntax (non-POSIX script) is used below!
#
# script to cross compile NUT for Windows from Linux using MinGW-w64
# http://mingw-w64.sourceforge.net/

#set -x

SCRIPTDIR="`dirname "$0"`"
SCRIPTDIR="`cd "$SCRIPTDIR" && pwd`"

DLLLDD_SOURCED=true . "${SCRIPTDIR}/dllldd.sh"

# This should match the tarball and directory name,
# if a stable version is used:
[ -n "$VER_OPT" ] || VER_OPT=2.8.2
DEBUG=true

# default to 32bits build
# Note: README specifies dependencies to pre-build and install;
# those DLLs should correspond to same architecture selection
cmd=all32
if [ -n "$1" ] ; then
	cmd="$1"
fi

if [ "$cmd" == "all64" ] || [ "$cmd" == "b64" ] || [ "$cmd" == "all32" ] || [ "$cmd" == "b32" ] ; then
	ARCH="x86_64-w64-mingw32"
	if [ "$cmd" == "all32" ] || [ "$cmd" == "b32" ] ; then
		ARCH="i686-w64-mingw32"
	fi
else
	echo "Usage:"
	echo "		$0 [all64 | b64 | all32 | b32]"
	echo "		Default: 'all32'"
	echo "Optionally export SOURCEMODE=[stable|dist|out-of-tree]"
	case "${cmd}" in
		-h|--help|help) exit 0 ;;
		*) exit 1 ;;
	esac
fi

# These paths are somewhat related:
[ -n "${WINDIR-}" ] || WINDIR="$(pwd)"
[ -n "${TOP_DIR-}" ] || TOP_DIR="$WINDIR/../.."

# These may be located elsewhere:
[ -n "${BUILD_DIR-}" ] || BUILD_DIR="$WINDIR/nut_build_${ARCH}"
[ -n "${INSTALL_DIR-}" ] || INSTALL_DIR="$WINDIR/nut_install_${ARCH}"

# Convenience symlink to find the latest completed build workspace and
# installed results; not used by the script other than updating the symlink:
[ -n "${BUILD_DIR_SYMLINK-}" ] || BUILD_DIR_SYMLINK="$WINDIR/nut_build"
[ -n "${INSTALL_DIR_SYMLINK-}" ] || INSTALL_DIR_SYMLINK="$WINDIR/nut_install"

rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$BUILD_DIR_SYMLINK" "$INSTALL_DIR_SYMLINK"
if [ x"$BUILD_DIR" != x"$BUILD_DIR_SYMLINK" ] ; then
	ln -fsr "$BUILD_DIR" "$BUILD_DIR_SYMLINK"
fi
if [ x"$INSTALL_DIR" != x"$INSTALL_DIR_SYMLINK" ] ; then
	ln -fsr "$INSTALL_DIR" "$INSTALL_DIR_SYMLINK"
fi

CONFIGURE_SCRIPT="./configure"
[ -n "$SOURCEMODE" ] || SOURCEMODE="out-of-tree"

case "$SOURCEMODE" in
stable)
# FIXME
# Stable version (download the latest stable archive)
	VER_OPT_SHORT="`echo "$VER_OPT" | awk -F. '{print $1"."$2}'`"
	if [ ! -s "nut-$VER_OPT.tar.gz" ] ; then
		wget "https://www.networkupstools.org/source/$VER_OPT_SHORT/nut-$VER_OPT.tar.gz"
	fi
	rm -rf "nut-$VER_OPT"
	tar -xzf "nut-$VER_OPT.tar.gz"
	mv "nut-$VER_OPT" "$BUILD_DIR"
	;;
dist)
	# In-place version (no download)
	cd ../..
	rm -f nut-?.?.?*.tar.gz
	[ -s Makefile ] || { ./autogen.sh && ./configure; }
	make dist
	SRC_ARCHIVE="$(ls -1 nut-?.?.?*.tar.gz | sort -n | tail -1)"
	cd scripts/Windows
	tar -xzf "../../$SRC_ARCHIVE"
	mv nut-?.?.?* "$BUILD_DIR"
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
	mkdir -p "$BUILD_DIR"
	;;
esac

if [ -z "$INSTALL_WIN_BUNDLE" ]; then
	echo "NOTE: You might want to export INSTALL_WIN_BUNDLE=true to use main NUT Makefile"
	echo "recipe for DLL co-bundling (default: false to use logic maintained in $0"
fi >&2

do_build_mingw_nut() {
	cd "$BUILD_DIR" || exit

	HOST_FLAG="--host=$ARCH"
	# --build needs to be specified, beside of --host, to avoid Warning
	# but this version is very Debian specific!!!
	# FIXME: find something more generic
	BUILD_FLAG="--build=`dpkg-architecture -qDEB_BUILD_GNU_TYPE`"
	export CC="$ARCH-gcc"
	export CXX="$ARCH-g++"

	# TODO: Detect/parameterize?
	#  This prefix is currently valid for mingw packaging in Debian/Ubuntu.
	ARCH_PREFIX="/usr/$ARCH"
	export PATH="${ARCH_PREFIX}/bin:$PATH"

	# Note: _WIN32_WINNT>=0x0600 is needed for inet_ntop in mingw headers
	# and the value 0xffff is anyway forced into some components at least
	# by netsnmp cflags.
	# _POSIX_THREAD_SAFE_FUNCTIONS whould help with localtime_r() gmtime_r()
	# on recent mingw releases (as of 2019), per
	# https://stackoverflow.com/questions/18551409/localtime-r-support-on-mingw
	export CFLAGS+=" -D_POSIX=1 -D_POSIX_C_SOURCE=200112L -D_POSIX_THREAD_SAFE_FUNCTIONS=200112L -I${ARCH_PREFIX}/include/ -D_WIN32_WINNT=0xffff"
	export CXXFLAGS+=" -D_POSIX=1 -D_POSIX_C_SOURCE=200112L -D_POSIX_THREAD_SAFE_FUNCTIONS=200112L -I${ARCH_PREFIX}/include/ -D_WIN32_WINNT=0xffff"
	export LDFLAGS+=" -L${ARCH_PREFIX}/lib/"

	KEEP_NUT_REPORT_FEATURE_FLAG=""
	if [ x"${KEEP_NUT_REPORT_FEATURE-}" = xtrue ]; then
		KEEP_NUT_REPORT_FEATURE_FLAG="--enable-keep_nut_report_feature"
	fi

	# Note: installation prefix here is "/" and desired INSTALL_DIR
	# location is passed to `make install` as DESTDIR below.
	# FIXME: Implement support for --without-pkg-config in m4 and use it
	RES_CFG=0
	$CONFIGURE_SCRIPT $HOST_FLAG $BUILD_FLAG --prefix=/ \
	    $KEEP_NUT_REPORT_FEATURE_FLAG \
	    PKG_CONFIG_PATH="${ARCH_PREFIX}/lib/pkgconfig" \
	    --with-all=auto \
	    --with-doc="man=auto html-single=auto html-chunked=skip pdf=skip" \
	    --without-systemdsystemunitdir \
	    --with-pynut=app \
	    --with-augeas-lenses-dir=/augeas-lenses \
	    --enable-Werror \
	|| RES_CFG=$?
	echo "$0: configure phase complete ($RES_CFG)" >&2
	[ x"$RES_CFG" = x0 ] || exit $RES_CFG

	echo "Configuration finished, starting make" >&2
	if [ -n "$PARMAKE_FLAGS" ]; then
		echo "For parallel builds, '$PARMAKE_FLAGS' options would be used" >&2
	fi
	if [ -n "$MAKEFLAGS" ]; then
		echo "Generally, MAKEFLAGS='$MAKEFLAGS' options would be passed" >&2
	fi

	# FIXME: parameterize ${MAKE} ?
	make 1>/dev/null || exit
	make doc 1>/dev/null || exit
	make -k man-man html-man 1>/dev/null || true
	(cd docs && make check) 1>/dev/null || exit
	echo "$0: build phase complete ($?)" >&2

	if [ "x$INSTALL_WIN_BUNDLE" = xtrue ] ; then
		# Going forward, this should be the main mode - "legacy code"
		# below picked up and transplanted into main build scenarios:
		echo "NOTE: INSTALL_WIN_BUNDLE==true so using main NUT Makefile logic for DLL co-bundling" >&2
		make install-win-bundle DESTDIR="${INSTALL_DIR}" || exit
	else
		# Legacy code from when NUT for Windows effort started;
		# there is no plan to maintain it much (this script is PoC):
		echo "NOTE: INSTALL_WIN_BUNDLE!=true so using built-in logic for DLL co-bundling" >&2

		make install DESTDIR="${INSTALL_DIR}" || exit

		# Per docs, Windows loads DLLs from EXE file's dir or some
		# system locations or finally PATH, so unless the caller set
		# the latter, we can not load the pre-linked DLLs from ../lib:
		#   http://msdn.microsoft.com/en-us/library/windows/desktop/ms682586(v=vs.85).aspx#standard_search_order_for_desktop_applications

		# Be sure upsmon can run even if at cost of some duplication
		# (maybe even do "cp -pf" if some system dislikes "ln"); also
		# on a modern Windows one could go to their installed "sbin" to
		#   mklink .\libupsclient-3.dll ..\bin\libupsclient-3.dll
		(cd "$INSTALL_DIR/bin" && ln libupsclient*.dll ../sbin/)
		(cd "$INSTALL_DIR/cgi-bin" 2>/dev/null && ln ../bin/libupsclient*.dll ./) \
		|| echo "NOTE: FAILED to process OPTIONAL cgi-bin directory; was NUT CGI enabled?" >&2

		echo "NOTE: Adding third-party dependency libraries for each installed program" >&2
		echo "      Do not worry about lack of libnut* and libups* in system locations" >&2

		# Cover dependencies for nut-scanner (not pre-linked)
		# Note: lib*snmp*.dll not listed below, it is
		# statically linked into binaries that use it
		(cd "$INSTALL_DIR/bin" && cp -pf "${ARCH_PREFIX}/bin"/{libgnurx,libusb,libltdl}*.dll .) || true
		(cd "$INSTALL_DIR/bin" && cp -pf "${ARCH_PREFIX}/lib"/libwinpthread*.dll .) || true

		# Steam-roll over all executables/libs we have here and copy
		# over resolved dependencies from the cross-build environment:
		(cd "$INSTALL_DIR" && { dllldddir . | while read D ; do cp -pf "$D" ./bin/ ; done ; } ) || true

		# Hardlink libraries for sbin (alternative: all bins in one dir):
		(cd "$INSTALL_DIR/sbin" && { DESTDIR="$INSTALL_DIR" dllldddir . | while read D ; do ln -f ../bin/"`basename "$D"`" ./ ; done ; } ) || true

		# Hardlink libraries for cgi-bin if present:
		(cd "$INSTALL_DIR/cgi-bin" 2>/dev/null && { DESTDIR="$INSTALL_DIR" dllldddir . | while read D ; do ln -f ../bin/"`basename "$D"`" ./ ; done ; } ) \
		|| echo "NOTE: FAILED to process OPTIONAL cgi-bin directory; was NUT CGI enabled?" >&2
	fi

	# If we had fatal errors above, we exited there - so here we are SUCCESSful
	echo "$0: SUCCESS: install phase complete ($?)" >&2
	cd ..
}

do_build_mingw_nut
