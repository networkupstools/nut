
EXTRA_DIST = nut-usbinfo.pl nut-hclinfo.py device-recorder.sh svn2cl.authors

website:
	@if python -c pass; then \
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
	@if perl -e 1; then \
		echo "Regenerating the USB helper files."; \
		$(distdir)/nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB helper files regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi
