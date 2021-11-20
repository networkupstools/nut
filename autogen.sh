#! /bin/sh
#
# Autoreconf wrapper script to ensure that the source tree is
# in a buildable state

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
	PYTHON=""
	for P in python python3 python2 ; do
		if (command -v "$P" >/dev/null) && $P -c "import re,glob,codecs" ; then
			PYTHON="$P"
			break
		fi
	done
fi

rm -f *.in.AUTOGEN_WITHOUT || true

# re-generate files needed by configure, and created otherwise at 'dist' time
if [ ! -f scripts/augeas/nutupsconf.aug.in ]
then
	if [ -n "${PYTHON-}" ] && $PYTHON -c "import re,glob,codecs"; then
		echo "Regenerating Augeas ups.conf lens with '$PYTHON'..."
		(   # That script is templated; assume @PYTHON@ is the only
		    # road-bump there
		    cd scripts/augeas \
		    && $PYTHON ./gen-nutupsconf-aug.py.in
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
			exit 1
		fi
	fi
fi

if [ ! -f scripts/udev/nut-usbups.rules.in -o \
     ! -f scripts/devd/nut-usb.conf.in ]
then
	if perl -e 1; then
		echo "Regenerating the USB helper files..."
		cd tools && {
			./nut-usbinfo.pl || exit 1
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

if [ ! -e scripts/systemd/nut-common.tmpfiles.in ]; then
	echo '# autoconf requires this file exists before generating configure script; it will be overwritten by configure during a build' > scripts/systemd/nut-common.tmpfiles.in
fi

# now we can safely call autoreconf
if ( command -v dos2unix ) 2>/dev/null >/dev/null \
&& ! ( dos2unix < configure.ac | cmp - configure.ac ) 2>/dev/null >/dev/null ; then
	echo "WARNING: Did not confirm that configure.ac has Unix EOL markers;"
	echo "this may cause issues for m4 parsing with autotools below."
	if [ -e .git ] ; then
		echo "You may want to enforce that Git uses 'lf' line endings and re-checkout:"
		echo "    :; git config core.autocrlf false && git config core.eol lf"
	fi
	echo ""
fi >&2

echo "Calling autoreconf..."
autoreconf -iv && {
	sh -n configure 2>/dev/null >/dev/null \
	|| { echo "FAILED: configure script did not pass shell interpreter syntax checks" >&2 ; exit 1; }
}
