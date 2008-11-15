# only call the USB info script upon "make dist", and if Perl is present
dist:
	@if test -x /usr/bin/perl; then \
		echo "Regenerating the USB files for udev, hotplug and HAL."; \
		./nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB files regeneration for udev, hotplug and HAL."; \
		echo "----------------------------------------------------------------------"; \
	fi
