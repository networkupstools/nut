
if WITH_DEVD
 devdconfdir = $(devddir)
 devdconf_DATA =
if WITH_USB
 devdconf_DATA += nut-usb.conf
endif
endif

EXTRA_DIST = README

DISTCLEANFILES = nut-usb.conf
# we should never remove this one, apart from a distclean-check
#MAINTAINERCLEANFILES = nut-usbups.rules.in
