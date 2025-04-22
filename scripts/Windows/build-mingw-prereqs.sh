#!/bin/sh

# Copyright (C) 2022-2025 by Jim Klimov <jimklimov+nut@gmail.com>
# Licensed GPLv2+, same as NUT
#
# Helper automating the nuances from NUT::scripts/Windows/README.adoc
# to provide prerequisites needed in semi-native or cross-builds.
#
# NOTE: Currently constrained to providing net-snmp under MSYS2
# (where not packaged/installed) and custom libmodbus from NUT Git.
#
# We can not rely on certain common shell facilities like `true`
# and `false` programs being available (in PATH or at all) so we
# `echo ""` instead.
# TODO: Support `make uninstall` attempts for older versions?..
#
# NOTE: Experimentally can use for prerequisites on other platforms, e.g.
#   SUDO=" " PREFIX_ROOT="${HOME}/nut-deps-inst" PREFIX="" \
#   ARCH="`uname -s`-`uname -p`" ./scripts/Windows/build-mingw-prereqs.sh

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
	else
		BUILD_FLAG=""
		HOST_FLAG=""
		export HOST_FLAG
		if [ -n "${MSYS2_PATH-}" ]; then
			# Assume semi-native build for same env
			[ -n "${ARCH-}" ] || ARCH="$MINGW_CHOST"
			[ -n "${PREFIX_ROOT-}" ] || PREFIX_ROOT="/"
			[ -n "${PREFIX-}" ] || PREFIX="${PREFIX_ROOT}/$MINGW_PREFIX"
			# Normalize away extra slashes, they confuse at least MSYS2 tools
			PREFIX="`echo "${PREFIX}" | sed 's,//*,/,g'`"
			case "${PATH}" in
				"${PREFIX}/bin"|"${PREFIX}/bin:"*|*":${PREFIX}/bin:"*|*":${PREFIX}/bin") ;;
				*) PATH="${PREFIX}/bin:${PATH}" ;;
			esac
			case "${PATH}" in
				*/ccache/*) ;;
				*) if [ -d "${PREFIX}/lib/ccache/bin" ] ; then
					# Potentionally customized by builder
					echo "Injecting MSYS2 ccache symlink collection (under PREFIX) into PATH" >&2
					PATH="${PREFIX}/lib/ccache/bin:${PATH}"
				   else
					if [ -d "${MINGW_PREFIX}/lib/ccache/bin" ] ; then
						echo "Injecting MSYS2 ccache symlink collection (under MINGW_PREFIX) into PATH" >&2
						PATH="${MINGW_PREFIX}/lib/ccache/bin:${PATH}"
					fi
				   fi ;;
			esac
			export ARCH PATH PREFIX

			if ! (command -v sudo) ; then sudo() ( "$@" ) ; fi
		else
			if [ -z "${ARCH-}" ] ; then
				# TODO: Select by args, envvars, directory presence...
				ARCH="x86_64-w64-mingw32"
				#ARCH="i686-w64-mingw32"
			fi

			# Assumes Ubuntu/Debian with mingw prepared, per README
			HOST_FLAG="--host=$ARCH"
			[ -n "${PREFIX_ROOT-}" ] || PREFIX_ROOT="/usr"
			[ -n "${PREFIX-}" ] || PREFIX="${PREFIX_ROOT}/${ARCH}"
			PREFIX="`echo "${PREFIX}" | sed 's,//*,/,g'`"

			export ARCH PREFIX

			if (command -v dpkg-architecture) ; then
				BUILD_FLAG="--build=`dpkg-architecture -qDEB_BUILD_GNU_TYPE`"
			fi
		fi
	fi

	if [ -z "${ARCH-}" ] || [ -z "${PREFIX-}" ] ; then
		echo "FAILED to determine ARCH and/or PREFIX!" >&2
		exit 1
	fi

	case "$CFLAGS" in
		*"${PREFIX}/include"*) ;;
		*) CFLAGS="$CFLAGS -D_POSIX=1 -I${PREFIX}/include/" ;;
	esac
	case "$CXXFLAGS" in
		*"${PREFIX}/include"*) ;;
		*) CXXFLAGS="$CXXFLAGS -D_POSIX=1 -I${PREFIX}/include/" ;;
	esac
	case "$LDFLAGS" in
		*"${PREFIX}/lib"*) ;;
		*) LDFLAGS="$LDFLAGS -L${PREFIX}/lib/" ;;
	esac
	case "$PKG_CONFIG_PATH" in
		*"${PREFIX}/lib"*) ;;
		*) PKG_CONFIG_PATH="${PREFIX}"/lib/pkgconfig ;;
	esac

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
		if pkg-config --exists "$PKGCFG_NAME" ; then
			echo "SKIP: pkg-config says '$PKGCFG_NAME' exists" >&2
			return 0
		fi

		# Quickly install if prebuilt
		if [ -d "${WSDIR}/${DEP_DIRNAME}/.inst" ]; then (
			cd "${WSDIR}/${DEP_DIRNAME}/.inst" || exit
			(command -v rsync) && $SUDO rsync -cavPHK ./ / && exit
			$SUDO cp -pr ./ / && exit
			exit 1
		) && {
			echo "INST: (re-)applied '${WSDIR}/${DEP_DIRNAME}/.inst'" >&2
			return 0
		}
		fi

		# no stashed .inst; any Makefile at least?
		if [ -s "${WSDIR}/${DEP_DIRNAME}/Makefile" ]; then (
			cd "${WSDIR}/${DEP_DIRNAME}" && $SUDO $MAKE install
		) && {
			echo "INST: ran 'make install' from '${WSDIR}/${DEP_DIRNAME}'" >&2
			return
		}
		fi

		# Not pre-built, fall through
	fi

	# (Re-)make and install from scratch
	set -e

	# Funny ways to fetch from Sourceforge help get the archive,
	# not the download page... For some reason, this bites CI
	# builds on Appveyor but not local runs:
	echo "FETCH: ${DEP_ARCHIVE}..." >&2
	( cd "$DLDIR" && curl -L "https://sourceforge.net/projects/${DEP_PRJNAME}/files/${DEP_PRJNAME}/${DEP_VERSION}/${DEP_ARCHIVE}"  > "${DEP_ARCHIVE}" ) || \
	( cd "$DLDIR" && wget -c "https://sourceforge.net/projects/${DEP_PRJNAME}/files/${DEP_PRJNAME}/${DEP_VERSION}/${DEP_ARCHIVE}" -O "${DEP_ARCHIVE}" )

	echo "BUILD: '${WSDIR}/${DEP_DIRNAME}'..." >&2
	cd "${WSDIR}" || exit
	rm -rf ${DEP_DIRNAME} || echo ""

	tar xzf "$DLDIR/${DEP_ARCHIVE}" || exit
	cd "./${DEP_DIRNAME}" || exit

	yes "" | ./configure --prefix="${PREFIX}" --with-default-snmp-version=3 \
		--disable-agent --disable-daemon --with-sys-contact="" --with-sys-location="" \
		--with-logfile=none --with-persistent-directory="${PREFIX}/var/net-snmp" \
		--disable-embedded-perl --without-perl-modules --disable-perl-cc-checks \
		--enable-shared || { cat config.log ; exit 1; }

	$MAKE LDFLAGS="-no-undefined -lws2_32 -lregex -Xlinker --ignore-unresolved-symbol=_app_name_long -Xlinker --ignore-unresolved-symbol=app_name_long" || exit

	# Beside leaving a pre-install location for future runs,
	# this may build some more artifacts:
	rm -rf "`pwd`/.inst" || echo ""
	$MAKE DESTDIR="`pwd`/.inst" install || exit

	# Summarize what we have got
	find ./ -type f -name "*.dll" -o -name "*.dll.a";

	$SUDO $MAKE install
	echo "INST: ran 'make install' from '${WSDIR}/${DEP_DIRNAME}'" >&2
)

provide_libmodbus_git() (
	# Provide the libmodbus version with RTU USB support
	PKGCFG_NAME="libmodbus"
	DEP_PRJNAME="libmodbus-git"
	DEP_GITREPO="https://github.com/networkupstools/libmodbus"
	DEP_VERSION="rtu_usb"	# Git branch/tag/commit point
	# Use PR #3 temporarily:
	#DEP_GITREPO="https://github.com/jimklimov/libmodbus"	# TEMP
	#DEP_VERSION="fix-mingw-build"	# TEMP
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
			OTHER_HASH="`git ls-remote "${DEP_GITREPO}" | grep -E '(refs/(heads|tags)/'"${DEP_VERSION}"'$|^'"${DEP_VERSION}"')' | awk '{print $1}'`" && \
			if [ x"${LOCAL_HASH}" = x"${OTHER_HASH}" ] ; then
				echo "FETCH: Current git commit in '`pwd`' matches current '${DEP_VERSION}' in '${DEP_GITREPO}'" >&2
			else
				echo "FETCH: Update git workspace in `pwd`..." >&2
				git fetch --tags && \
				git fetch --all && \
				git checkout "${DEP_VERSION}" && \
				_GITDIFF="`git diff "origin/${DEP_VERSION}"`" && \
				if [ -n "${_GITDIFF}" ] ; then
					# Ensure rebase etc. or fail
					git pull && \
					./autogen.sh && \
					FORCE=true
				else
					echo "Current content in '`pwd`' matches current '${DEP_VERSION}' in '${DEP_GITREPO}'" >&2
				fi
			fi
		} || { cd "${DLDIR}" ; chmod -R +w "${DEP_DIRNAME}" || echo "" ; rm -rf "${DEP_DIRNAME}" ; }
	fi

	cd "${DLDIR}"
	if [ ! -d "${DEP_DIRNAME}/.git" ] ; then
		echo "FETCH: Clone git workspace in '${DLDIR}/${DEP_DIRNAME}' from '${DEP_VERSION}' in '${DEP_GITREPO}'..." >&2
		FORCE=true
		chmod -R +w "${DEP_DIRNAME}" || echo ""
		rm -rf "${DEP_DIRNAME}"
		git clone "${DEP_GITREPO}" -b "${DEP_VERSION}" "${DEP_DIRNAME}" || exit
	fi
	if [ ! -s "${DEP_DIRNAME}/configure" ] || [ x"$FORCE" = x"true" ] ; then
		echo "FETCH: (Re-)bootstrap git workspace in '${DLDIR}/${DEP_DIRNAME}'..." >&2
		(cd "${DEP_DIRNAME}" && ./autogen.sh) || exit
		FORCE=true
	fi

	set +e
	if [ x"$FORCE" = x"false" ] ; then
		# TODO: Check version - harder with git rolling, so no-op for now
		if false && pkg-config --exists "$PKGCFG_NAME" ; then
			echo "SKIP: pkg-config says '$PKGCFG_NAME' exists" >&2
			return 0
		fi

		# Quickly install if prebuilt
		if [ -d "${WSDIR}/${DEP_DIRNAME}/.inst" ]; then (
			cd "${WSDIR}/${DEP_DIRNAME}/.inst" || exit
			(command -v rsync) && $SUDO rsync -cavPHK ./ / && exit
			$SUDO cp -pr ./ / && exit
			exit 1
		) && {
			echo "INST: (re-)applied '${WSDIR}/${DEP_DIRNAME}/.inst'" >&2
			return 0
		}
		fi

		# no stashed .inst; any Makefile at least?
		if [ -s "${WSDIR}/${DEP_DIRNAME}/Makefile" ]; then (
			cd "${WSDIR}/${DEP_DIRNAME}" && $SUDO $MAKE install
		) && {
			echo "INST: ran 'make install' from '${WSDIR}/${DEP_DIRNAME}'" >&2
			return
		}
		fi

		# Not pre-built, fall through
	fi

	# (Re-)make and install from scratch
	set -e

	echo "BUILD: '${WSDIR}/${DEP_DIRNAME}'..." >&2
	cd "${WSDIR}"
	if [ -e "./${DEP_DIRNAME}" ] ; then
		# Take care of read-only destdir pieces
		chmod -R +w "./${DEP_DIRNAME}" || echo ""
		rm -rf "./${DEP_DIRNAME}" || echo ""
	fi
	mkdir -p "./${DEP_DIRNAME}" || exit
	cd "./${DEP_DIRNAME}" || exit

	"${DLDIR}/${DEP_DIRNAME}/configure" --prefix="${PREFIX}" --with-libusb --enable-static --disable-shared --enable-Werror \
		|| { cat config.log ; exit 1 ; }

	$MAKE || exit
	$MAKE check || echo "WARNING: make check is flaky or failed outright" >&2

	# Beside leaving a pre-install location for future runs,
	# this may build some more artifacts:
	rm -rf "`pwd`/.inst" || echo ""
	$MAKE DESTDIR="`pwd`/.inst" install || exit

	# Summarize what we have got
	find ./ -type f -name "*.dll" -o -name "*.a"

	$SUDO $MAKE install
	echo "INST: ran 'make install' from '${WSDIR}/${DEP_DIRNAME}'" >&2
)

prepareEnv || exit

echo "Prepared environment for $0:" >&2
set | grep -E '^(ARCH|PREFIX|PREFIX_ROOT|PATH|MAKE|MAKEFLAGS|SUDO|DLDIR|WSDIR|CFLAGS|CXXFLAGS|LDFLAGS|PKG_CONFIG_PATH|BUILD_FLAG|HOST_FLAG|MINGW_CHOST|MINGW_PREFIX|MSYS2_PATH)=' >&2

# TODO: Loop, params, help, etc...
# For now, let it pass "-f" to the builder
provide_netsnmp "$@"
ls -la "${PREFIX}/lib/pkgconfig/netsnmp.pc"
cat "${PREFIX}/lib/pkgconfig/netsnmp.pc"
provide_libmodbus_git "$@"
ls -la "${PREFIX}/lib/pkgconfig/libmodbus.pc"
cat "${PREFIX}/lib/pkgconfig/libmodbus.pc"
