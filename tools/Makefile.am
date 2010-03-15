
EXTRA_DIST = nut-usbinfo.pl device-recorder.sh svn2cl.authors

# only call the USB info script upon "make dist", and if Perl is present
dist-hook:
	@if test -x /usr/bin/perl; then \
		echo "Regenerating the USB helper files."; \
		$(distdir)/nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB helper files regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi
