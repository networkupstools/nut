#! /bin/sh
#
# Autoreconf wrapper script to ensure that the source tree is
# in a buildable state

# re-generate files needed by configure, and created otherwise at 'dist' time
if [ ! -f scripts/augeas/nutupsconf.aug.in ]
then
	if python -c "import re,glob,codecs"; then
		echo "Regenerating Augeas ups.conf lens..."
		cd scripts/augeas && {
			./gen-nutupsconf-aug.py || exit 1
			cd ../..
		}
	else
		echo "----------------------------------------------------------------------"
		echo "Error: Python is not available."
		echo "Unable to regenerate Augeas ups.conf lens."
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

# we'd rather regenerate this file every time, because the script to generate
# its content (and the set of input files) can change over time; note that the
# script produces a $srcdir/scripts/DMF/legacy-mibfiles-list.mk.in template
# later converted to $builddir/scripts/DMF/legacy-mibfiles-list.mk by configure
echo "Regenerating the list of legacy *-mib.c files in current codebase to produce DMFs later"
( cd scripts/DMF && ./gen-legacy-mibfiles-list.sh )

# now we can safely call autoreconf
echo "Calling autoreconf..."
autoreconf -i
