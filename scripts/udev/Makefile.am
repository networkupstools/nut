
if WITH_UDEV
 udevrulesdir = $(udevdir)/rules.d
 udevrules_DATA = 52_nut-usbups.rules
endif

EXTRA_DIST = README

52_nut-usbups.rules: nut-usbups.rules
	cp nut-usbups.rules 52_nut-usbups.rules

MOSTLYCLEANFILES = 52_nut-usbups.rules
