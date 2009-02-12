
if WITH_UDEV
 udevrulesdir = $(udevdir)/rules.d
 udevrules_DATA = 52-nut-usbups.rules
endif

EXTRA_DIST = README

52-nut-usbups.rules: nut-usbups.rules
	cp nut-usbups.rules 52-nut-usbups.rules

DISTCLEANFILES = nut-usbups.rules
CLEANFILES = 52-nut-usbups.rules
# we should never remove this one, apart from a distclean-check
#MAINTAINERCLEANFILES = nut-usbups.rules.in
