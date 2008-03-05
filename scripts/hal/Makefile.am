
if WITH_HAL
 halfdidir = $(HAL_FDI_PATH)
 halfdi_DATA = 20-ups-nut-device.fdi
endif

20-ups-nut-device.fdi: ups-nut-device.fdi
	cp $< $@

MOSTLYCLEANFILES = 20-ups-nut-device.fdi

EXTRA_DIST = ups-nut-device.fdi
