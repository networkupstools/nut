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

# now we can safely call autoreconf
echo "Calling autoreconf..."
autoreconf -iv
