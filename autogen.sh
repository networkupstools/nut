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
		exit 1
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
		exit 1
	fi
fi

if [ ! -e scripts/systemd/nut-common.tmpfiles.in ]; then
	echo '# autoconf requires this file exists before generating configure script; it will be overwritten by configure during a build' > scripts/systemd/nut-common.tmpfiles.in
fi

# now we can safely call autoreconf
echo "Calling autoreconf..."
autoreconf -iv && {
    sh -n configure 2>/dev/null >/dev/null \
    || { echo "FAILED: configure script did not pass shell interpreter syntax checks" >&2 ; exit 1; }
}
