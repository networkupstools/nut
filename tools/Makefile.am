
SUBDIRS = nut-scanner

EXTRA_DIST = nut-usbinfo.pl nut-hclinfo.py device-recorder.sh svn2cl.authors nut-snmpinfo.py

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
# only call the SNMP info script upon "make dist", and if Python is present
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

	@if python -c 1; then \
		echo "Regenerating the SNMP helper files."; \
		$(distdir)/nut-snmpinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the SNMP helper files regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi
