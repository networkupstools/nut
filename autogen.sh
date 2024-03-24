#! /bin/sh
#
# Autoreconf wrapper script to ensure that the source tree is
# in a buildable state
# NOTE: uses cumbersome dumbest-possible shell syntax for extra portability

# perl tends to complain if locale is not set (or its files are absent)
if [ -z "${LANG-}" ]; then
	LANG="C"
	export LANG
fi

if [ -z "${LC_ALL-}" ] ; then
	LC_ALL="C"
	export LC_ALL
fi

VERBOSE_FLAG=""
if [ x"${DEBUG-}" = xtrue ] || [ x"${CI_DEBUG-}" = xtrue ] || [ x"$1" = x-v ] ; then
	DEBUG=true
	VERBOSE_FLAG="-v"
	echo "NUT script $0 will call the tools with higher debug verbosity"
else
	DEBUG=false
fi

if [ -n "${PYTHON-}" ] ; then
	# May be a name/path of binary, or one with args - check both
	(command -v "$PYTHON") \
	|| $PYTHON -c "import re,glob,codecs" \
	|| {
		echo "----------------------------------------------------------------------"
		echo "WARNING: Caller-specified PYTHON='$PYTHON' is not available."
		echo "----------------------------------------------------------------------"
		# Do not die just here, we may not need the interpreter
	}
else
	$DEBUG && echo "=== Picking usable Python version..."
	PYTHON=""
	# FIXME: Use something like TAB-completion to find every name on PATH?
	for P in python python3 python2 \
		python-3.14 python3.14 \
		python-3.13 python3.13 \
		python-3.12 python3.12 \
		python-3.11 python3.11 \
		python-3.10 python3.10 \
		python-3.9 python3.9 \
		python-3.7 python3.7 \
		python-3.5 python3.5 \
		python-3.4 python3.4 \
		python-2.7 python2.7 \
	; do
		if (command -v "$P" >/dev/null) && $P $VERBOSE_FLAG -c "import re,glob,codecs" ; then
			$DEBUG && echo "=== Picked usable Python version: $P"
			PYTHON="$P"
			break
		fi
	done
	if $DEBUG && [ -z "$PYTHON" ] ; then
		echo "=== Did not pick any usable Python version"
	fi
fi

rm -f *.in.AUTOGEN_WITHOUT || true

# re-generate files needed by configure, and created otherwise at 'dist' time
if [ ! -f scripts/augeas/nutupsconf.aug.in ]
then
	if [ -n "${PYTHON-}" ] && $PYTHON $VERBOSE_FLAG -c "import re,glob,codecs"; then
		echo "Regenerating Augeas ups.conf lens with '$PYTHON'..."
		(   # That script is templated; assume @PYTHON@ is the only
		    # road-bump there
		    cd scripts/augeas \
		    && $PYTHON $VERBOSE_FLAG ./gen-nutupsconf-aug.py.in
		) || exit 1
	else
		echo "----------------------------------------------------------------------"
		echo "Error: Python is not available."
		echo "Unable to regenerate Augeas lens for ups.conf parsing."
		echo "----------------------------------------------------------------------"
		if [ "${WITHOUT_NUT_AUGEAS-}" = true ]; then
			echo "Proceeding without Augeas integration, be sure to not require it in configure script" >&2
			touch scripts/augeas/nutupsconf.aug.in scripts/augeas/nutupsconf.aug.in.AUTOGEN_WITHOUT
		else
			echo "Aborting $0! To avoid this, please   export WITHOUT_NUT_AUGEAS=true   and re-run" >&2
			echo "or better yet,    export PYTHON=python-x.y   and re-run" >&2
			exit 1
		fi
	fi
fi

# Keep in sync with tools/nut-usbinfo.pl outputs:
# * List actual file opens:
#    grep -i '">' tools/nut-usbinfo.pl
# * List the names involved:
#    grep -E 'output.*=' tools/nut-usbinfo.pl
# Also check that the last re-generation is newer than the sources involved
# (stay on top of CI rebuilds, development, Git branch switching...)
# Someone please tell me why GNU `find dir -newer X -name Y -o -name Z` does
# not filter away layer by layer, but rather finds the names Z and beyond
# (same for the other way around)? Anyway, dumbed down for the most trivial
# `find` implementations out there...
if [ ! -f scripts/udev/nut-usbups.rules.in -o \
     ! -f scripts/hotplug/libhid.usermap -o \
     ! -f scripts/upower/95-upower-hid.hwdb -o \
     ! -f scripts/devd/nut-usb.conf.in -o \
     ! -f scripts/devd/nut-usb.quirks -o \
     ! -f tools/nut-scanner/nutscan-usb.h ] \
|| [ -n "`find drivers -newer scripts/hotplug/libhid.usermap | grep -E '(-hid|nutdrv_qx|usb.*)\.c'`" ] \
|| [ -n "`find drivers -not -newer tools/nut-usbinfo.pl | grep -E '(-hid|nutdrv_qx|usb.*)\.c'`" ] \
; then
	if perl -e 1; then
		VERBOSE_FLAG_PERL=""
		if $DEBUG ; then
			if perl -d:Devel::Trace -e 1 >/dev/null 2>/dev/null ; then
				VERBOSE_FLAG_PERL="-d:Devel::Trace"
			else
				echo "=== Can not trace perl, try sudo cpan install 'Devel::Trace'"
			fi
		fi
		echo "Regenerating the USB helper files..."
		cd tools && {
			perl $VERBOSE_FLAG_PERL ./nut-usbinfo.pl || exit 1
			cd ..
		}
	else
		echo "----------------------------------------------------------------------"
		echo "Error: Perl is not available."
		echo "Unable to regenerate USB helper files."
		echo "----------------------------------------------------------------------"
		if [ "${WITHOUT_NUT_USBINFO-}" = true ]; then
			echo "Proceeding without NUT USB Info, be sure to not require it in configure script" >&2
			touch scripts/udev/nut-usbups.rules.in scripts/udev/nut-usbups.rules.in.AUTOGEN_WITHOUT
			touch scripts/devd/nut-usb.conf.in scripts/devd/nut-usb.conf.in.AUTOGEN_WITHOUT
		else
			echo "Aborting $0! To avoid this, please   export WITHOUT_NUT_USBINFO=true   and re-run" >&2
			exit 1
		fi
	fi
fi

if [ ! -f scripts/systemd/nut-common-tmpfiles.conf.in ]; then
	( echo '# autoconf requires this file exists before generating configure script;'
	  echo '# it will be overwritten by running configure during an actual build'
	) > scripts/systemd/nut-common-tmpfiles.conf.in
fi

# now we can safely call autoreconf
if ( command -v dos2unix ) 2>/dev/null >/dev/null ; then
	if ( dos2unix < configure.ac | cmp - configure.ac ) 2>/dev/null >/dev/null ; then
		:
	else
		echo "WARNING: Did not confirm that configure.ac has Unix EOL markers;"
		echo "this may cause issues for m4 parsing with autotools below."
		if [ -f .git ] || [ -d .git ] ; then
			echo "You may want to enforce that Git uses 'lf' line endings and re-checkout:"
			echo "    :; git config core.autocrlf false && git config core.eol lf && rm .git/index && git checkout -f"
		fi
		echo ""
	fi
fi >&2

# Required by autoconf for non-"foreign" projects;
# is tracked as a NEWS.adoc for us however.
[ -f NEWS ] || { echo "Please see NEWS.adoc for actual contents" > NEWS; }
[ -f README ] || { echo "Please see README.adoc for actual contents" > README; }

echo "Calling autoreconf..."
AUTOTOOL_RES=0
if $DEBUG ; then
	autoreconf -iv --warnings=all -d || AUTOTOOL_RES=$?
else
	# This tool's own verbosity is rather compact (whom it called)
	# and not too useful for actual troubleshooting, while not too
	# noisy to just disable.
	autoreconf -iv || AUTOTOOL_RES=$?
fi

[ "$AUTOTOOL_RES" = 0 ] && [ -s configure ] && [ -x configure ] \
|| { cat << EOF
FAILED: did not generate an executable configure script!

# Note: on some systems "autoreconf", "automake" et al are dispatcher
# scripts, and need you to explicitly say which version you want, e.g.
#    export AUTOCONF_VERSION=2.65 AUTOMAKE_VERSION=1.13
# If you get issues with AC_DISABLE_STATIC make sure you have libtool.
#
# If it complains about "too few" or "excess" "arguments to builtin ifdef",
# check the configure.ac line it refers to and un-comment (or comment away)
# the third argument for AM_SILENT_RULES check, or comment away the whole
# "ifdef" block if your autotools still would not grok it.
EOF
	exit 1
} >&2

# Some autoconf versions may leave "/bin/sh" regardless of CONFIG_SHELL
# which originally was made for "recheck" operations
if [ -n "${CONFIG_SHELL-}" ]; then
	case "${CONFIG_SHELL-}" in
		*/*)	;; # use as is, assume full path
		*)
			ENV_PROG="`command -v env`" 2>/dev/null
			if [ -n "$ENV_PROG" -a -x "$ENV_PROG" ] ; then
				echo "Using '$ENV_PROG' to call unqualified CONFIG_SHELL program name '$CONFIG_SHELL'" >&2
				CONFIG_SHELL="$ENV_PROG $CONFIG_SHELL"
			fi
			;;
	esac

	echo "Injecting caller-provided CONFIG_SHELL='$CONFIG_SHELL' into the script" >&2
	echo "#!${CONFIG_SHELL}" > configure.tmp
	cat configure >> configure.tmp
	# keep the original file rights intact
	cat configure.tmp > configure
	rm configure.tmp
else
	CONFIG_SHELL="`head -1 configure | sed 's,^#!,,'`"
fi

# NOTE: Unquoted, may be multi-token
$CONFIG_SHELL -n configure 2>/dev/null >/dev/null \
|| { echo "FAILED: configure script did not pass shell interpreter syntax checks with $CONFIG_SHELL" >&2
	echo "NOTE: If you are using an older OS release, try executing the script with" >&2
	echo "a more functional shell implementation (dtksh, bash, dash...)" >&2
	echo "You can re-run this script with a CONFIG_SHELL in environment" >&2
	exit 1
}

echo "The generated configure script passed shell interpreter syntax checks"
echo "Please proceed by running './configure --with-many-desired-options'"
echo "For details check './configure --help' or docs/configure.txt in NUT sources"
