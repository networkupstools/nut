
EXTRA_DIST = nut-usbinfo.pl nut-hclinfo.py

website:
	@if test -x /usr/bin/python; then \
		echo "Regenerating the HTML and JSON formated HCL tables."; \
		./nut-hclinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the HTML and JSON formated HCL tables regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

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
