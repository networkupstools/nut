
if WITH_UDEV
 udevrulesdir = $(udevdir)/rules.d
 udevrules_DATA =
if WITH_USB
 udevrules_DATA += 62-nut-usbups.rules
endif
if WITH_IPMI
 udevrules_DATA += 52-nut-ipmipsu.rules
endif
endif

EXTRA_DIST = README

62-nut-usbups.rules: nut-usbups.rules
	cp nut-usbups.rules $@

52-nut-ipmipsu.rules: nut-ipmipsu.rules
	cp nut-ipmipsu.rules $@

DISTCLEANFILES = nut-usbups.rules nut-ipmipsu.rules
CLEANFILES = 62-nut-usbups.rules 52-nut-ipmipsu.rules
# we should never remove this one, apart from a distclean-check
#MAINTAINERCLEANFILES = nut-usbups.rules.in
