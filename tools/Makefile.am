
EXTRA_DIST = nut-usbinfo.pl

# only call the USB info script upon "make dist", and if Perl is present
dist-hook:
	@if test -x /usr/bin/perl; then \
		echo "Regenerating the USB files for udev, hotplug and HAL."; \
		$(distdir)/nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB files regeneration for udev, hotplug and HAL."; \
		echo "----------------------------------------------------------------------"; \
	fi
