
if WITH_HAL
 halfdidir = $(HAL_FDI_PATH)
 halfdi_DATA = 20-ups-nut-device.fdi
endif

20-ups-nut-device.fdi: ups-nut-device.fdi
	cp $< $@

EXTRA_DIST = ups-nut-device.fdi.in

DISTCLEANFILES = ups-nut-device.fdi
# we should never remove this one, apart from a distclean-check
#MAINTAINERCLEANFILES = ups-nut-device.fdi.in
CLEANFILES = 20-ups-nut-device.fdi
