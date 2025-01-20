#!/bin/sh

# Copyright (C) 2022-2025 by Jim Klimov <jimklimov+nut@gmail.com>
# Licensed GPLv2+, same as NUT
#
# Helper automating the nuances from NUT::scripts/Windows/README.adoc
# to provide prerequisites needed in semi-native or cross-builds.
#
# NOTE: Currently constrained to providing net-snmp under MSYS2.
# We can not rely on certain common shell facilities like `true`
# and `false` programs being available (in PATH or at all).
# TODO: Support `make uninstall` attempts for older versions?..

prepareEnv() {
	[ -n "${MAKE-}" ] || {
		(command -v gmake) 2>/dev/null >/dev/null \
		&& MAKE="gmake" \
		|| MAKE="make"
	}
	export MAKE

	[ -n "${MAKEFLAGS-}" ] || {
		MAKEFLAGS="-j 8"
		export MAKEFLAGS
	}

	if [ -z "${SUDO-}" ] ; then
		SUDO=" " # avoid reeval
		if (command -v sudo) ; then SUDO="sudo" ; fi
	fi
	export SUDO

	[ -n "${DLDIR-}" ] || DLDIR=~/nut-win-deps

	if [ -n "${ARCH-}" ] && [ -n "${PREFIX-}" ] ; then
		[ -n "${WSDIR-}" ] || WSDIR="$DLDIR"/"$ARCH"
		mkdir -p "$WSDIR" "$DLDIR"
		return 0
	fi

	BUILD_FLAG=""
	HOST_FLAG=""
	export HOST_FLAG
	if [ -n "${MSYS2_PATH-}" ]; then
		# Assume semi-native build for same env
		ARCH="$MINGW_CHOST"
		PREFIX="$MINGW_PREFIX"
		PATH="$PREFIX/bin:$PATH"
		export ARCH PATH PREFIX

		if ! (command -v sudo) ; then sudo() ( "$@" ) ; fi
	else
		# TODO: Select by args, envvars, directory presence...
		ARCH="x86_64-w64-mingw32"
		#ARCH="i686-w64-mingw32"

		# Assumes Ubuntu/Debian with mingw prepared, per README
		HOST_FLAG="--host=$ARCH"
		PREFIX="/usr/$ARCH"

		export ARCH PREFIX

		if (command -v dpkg-architecture) ; then
			BUILD_FLAG="--build=`dpkg-architecture -qDEB_BUILD_GNU_TYPE`"
		fi
	fi

	CFLAGS="$CFLAGS -D_POSIX=1 -I${PREFIX}/include/"
	CXXFLAGS="$CXXFLAGS -D_POSIX=1 -I${PREFIX}/include/"
	LDFLAGS="$LDFLAGS -L${PREFIX}/lib/"
	PKG_CONFIG_PATH="${PREFIX}"/lib/pkgconfig

	export CFLAGS CXXFLAGS LDFLAGS PKG_CONFIG_PATH

	[ -n "${WSDIR-}" ] || WSDIR="$DLDIR"/"$ARCH"
	mkdir -p "$WSDIR" "$DLDIR"
}

# Provide prerequisites; reentrant (quick skip if installed; quick install if built)
provide_netsnmp() (
	PKGCFG_NAME="netsnmp"
	DEP_PRJNAME="net-snmp"
	DEP_VERSION="5.9.1"
	DEP_DIRNAME="${DEP_PRJNAME}-${DEP_VERSION}"
	DEP_ARCHIVE="${DEP_DIRNAME}.tar.gz"

	FORCE=false
	if [ x"${1-}" = x"-f" ] ; then FORCE=true ; fi

	set +e
	if [ x"$FORCE" = x"false" ] ; then
		# TODO: Check version
		if pkg-config --exists "$PKGCFG_NAME" ; then return 0 ; fi

		# Quickly install if prebuilt
		if [ -d "${WSDIR}/${DEP_DIRNAME}/.inst" ]; then (
			cd "${WSDIR}/${DEP_DIRNAME}/.inst" || exit
			(command -v rsync) && $SUDO rsync -cavPHK ./ / && exit
			$SUDO cp -pr ./ / && exit
			exit 1
			) && return 0
		fi

		# no stashed .inst; any Makefile at least?
		if [ -s "${WSDIR}/${DEP_DIRNAME}/Makefile" ]; then ( cd "${WSDIR}/${DEP_DIRNAME}" && $SUDO $MAKE install ) && return ; fi

		# Not pre-built, fall through
	fi

	# (Re-)make and install from scratch
	set -e

	# Funny ways to fetch from Sourceforge help get the archive,
	# not the download page... For some reason, this bites CI
	# builds on Appveyor but not local runs:
	( cd "$DLDIR" && curl -vL "https://sourceforge.net/projects/${DEP_PRJNAME}/files/${DEP_PRJNAME}/${DEP_VERSION}/${DEP_ARCHIVE}" > "${DEP_ARCHIVE}" ) || \
	( cd "$DLDIR" && wget -c "https://sourceforge.net/projects/${DEP_PRJNAME}/files/${DEP_PRJNAME}/${DEP_VERSION}/${DEP_ARCHIVE}" -O "${DEP_ARCHIVE}" )

	cd "${WSDIR}"
	rm -rf ${DEP_DIRNAME} || echo ""

	tar xzf "$DLDIR/${DEP_ARCHIVE}" || exit
	cd "./${DEP_DIRNAME}"

	yes "" | ./configure --prefix="$PREFIX" --with-default-snmp-version=3 \
		--disable-agent --disable-daemon --with-sys-contact="" --with-sys-location="" \
		--with-logfile=none --with-persistent-directory="${PREFIX}/var/net-snmp" \
		--disable-embedded-perl --without-perl-modules --disable-perl-cc-checks \
		--enable-shared || exit

	$MAKE LDFLAGS="-no-undefined -lws2_32 -lregex -Xlinker --ignore-unresolved-symbol=_app_name_long -Xlinker --ignore-unresolved-symbol=app_name_long" || exit

	# Beside leaving a pre-install location for future runs,
	# this may build some more artifacts:
	rm -rf "`pwd`/.inst" || echo ""
	$MAKE DESTDIR="`pwd`/.inst" install || exit

	# Summarize what we have got
	find ./ -type f -name "*.dll" -o -name "*.dll.a";

	$SUDO $MAKE install
)

provide_libmodbus_git() (
	# Provide the libmodbus version with RTU USB support
	PKGCFG_NAME="libmodbus"
	DEP_PRJNAME="libmodbus-git"
	#DEP_GITREPO="https://github.com/networkupstools/libmodbus"
	#DEP_VERSION="rtu_usb"	# Git branch/tag/commit point
	# Use PR #3 temporarily:
	DEP_GITREPO="https://github.com/jimklimov/libmodbus"	# TEMP
	DEP_VERSION="fix-mingw-build"	# TEMP
	DEP_DIRNAME="${DEP_PRJNAME}-${DEP_VERSION}"

	FORCE=false
	if [ x"${1-}" = x"-f" ] ; then FORCE=true ; fi

	# Make sure we have current repo code checked out
	cd "${DLDIR}"
	if [ -d "${DEP_DIRNAME}" ] ; then
		{
			# NOTE: Use `git ls-remote {URL|.}` to not modify the local FS if
			# there's nothing to change (avoid re-packaging of CI artifact cache)
			cd "${DEP_DIRNAME}" && \
			LOCAL_HASH="`git log -1 --format='%H'`" && \
			OTHER_HASH="`git ls-remote "${DEP_GITREPO}" | grep -E '(refs/(heads|tags)/'"${DEP_VERSION}"'$|^'"${DEP_VERSION}"')'`" && \
			if [ x"${LOCAL_HASH}" = x"${OTHER_HASH}" ] ; then
				echo "Current commit in '`pwd`' matches current '${DEP_VERSION}' in '${DEP_GITREPO}'" >&2
			else
				git fetch --tags && \
				git fetch --all && \
				git checkout "${DEP_VERSION}" && \
				_GITDIFF="`git diff "origin/${DEP_VERSION}"`" && \
				if [ -n "${_GITDIFF}" ] ; then
					FORCE=true
					# Ensure rebase etc. or fail
					git pull || exit
					./autogen.sh || exit
				else
					echo "Current content in '`pwd`' matches current '${DEP_VERSION}' in '${DEP_GITREPO}'" >&2
				fi
			fi
		} || { chmod -R +w "${DEP_DIRNAME}" || true ; rm -rf "${DEP_DIRNAME}" ; }
	fi

	cd "${DLDIR}"
	if [ ! -d "${DEP_DIRNAME}/.git" ] ; then
		FORCE=true
		chmod -R +w "${DEP_DIRNAME}" || true
		rm -rf "${DEP_DIRNAME}"
		git clone "${DEP_GITREPO}" -b "${DEP_VERSION}" "${DEP_DIRNAME}" || exit
	fi
	if [ ! -s "${DEP_DIRNAME}/configure" ] || [ x"$FORCE" = x"true" ] ; then
		(cd "${DEP_DIRNAME}" && ./autogen.sh) || exit
	fi

	set +e
	if [ x"$FORCE" = x"false" ] ; then
		# TODO: Check version - harder with git rolling
		#if pkg-config --exists "$PKGCFG_NAME" ; then return 0 ; fi

		# Quickly install if prebuilt
		if [ -d "${WSDIR}/${DEP_DIRNAME}/.inst" ]; then (
			cd "${WSDIR}/${DEP_DIRNAME}/.inst" || exit
			(command -v rsync) && $SUDO rsync -cavPHK ./ / && exit
			$SUDO cp -pr ./ / && exit
			exit 1
			) && return 0
		fi

		# no stashed .inst; any Makefile at least?
		if [ -s "${WSDIR}/${DEP_DIRNAME}/Makefile" ]; then ( cd "${WSDIR}/${DEP_DIRNAME}" && $SUDO $MAKE install ) && return ; fi

		# Not pre-built, fall through
	fi

	# (Re-)make and install from scratch
	set -e

	cd "${WSDIR}"
	# Take care of read-only destdir pieces
	chmod -R +w "./${DEP_DIRNAME}" || echo ""
	rm -rf "./${DEP_DIRNAME}" || echo ""
	mkdir -p "./${DEP_DIRNAME}" || exit
	cd "./${DEP_DIRNAME}" || exit

	"${DLDIR}/${DEP_DIRNAME}/configure" --prefix="$PREFIX" --with-libusb --enable-static --disable-shared --enable-Werror

	$MAKE || exit
	$MAKE check || echo "WARNING: make check is flaky or failed outright" >&2

	# Beside leaving a pre-install location for future runs,
	# this may build some more artifacts:
	rm -rf "`pwd`/.inst" || echo ""
	$MAKE DESTDIR="`pwd`/.inst" install || exit

	# Summarize what we have got
	find ./ -type f -name "*.dll" -o -name "*.a"

	$SUDO $MAKE install
)

prepareEnv || exit

# TODO: Loop, params, help, etc...
# For now, let it pass "-f" to the builder
provide_netsnmp "$@"
provide_libmodbus_git "$@"
