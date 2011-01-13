#! /bin/sh
#
# Autoreconf wrapper script to ensure that the source tree is
# in a buildable state

# re-generate files needed by configure, and created otherwise at 'dist' time
if [ ! -f scripts/augeas/nutupsconf.aug.in ]
then
	if python -c pass; then
		echo "Regenerating Augeas ups.conf lens..."
		cd scripts/augeas && ./gen-nutupsconf-aug.py && cd ../..
	else
		echo "----------------------------------------------------------------------"
		echo "Warning: Python is not available."
		echo "Skipping Augeas ups.conf lens regeneration."
		echo "----------------------------------------------------------------------"
	fi
fi

if [ ! -f scripts/hal/ups-nut-device.fdi.in ] || [ ! -f scripts/udev/nut-usbups.rules.in ]
then
	if perl -e 1; then
		echo "Regenerating the USB helper files..."
		cd tools && ./nut-usbinfo.pl && cd ..
	else 
		echo "----------------------------------------------------------------------"
		echo "Error: Perl is not available."
		echo "Skipping the USB helper files regeneration."
		echo "----------------------------------------------------------------------"
	fi
fi

# now we can safely call autoreconf
echo "Calling autoreconf..."
autoreconf -i
