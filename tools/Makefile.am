# TODO: remove redundancies!

# Force build in ./ before nut-scanner, to have nutscan-{usb,snmp}.h
# built before going into the nut-scanner sub-directory
SUBDIRS = . nut-scanner

EXTRA_DIST = nut-usbinfo.pl nut-hclinfo.py device-recorder.sh svn2cl.authors nut-snmpinfo.py

all: nut-scanner-deps 

nut-scanner-deps:
	@if python -c 1; then \
		echo "Regenerating the SNMP helper files."; \
		$(top_srcdir)/tools/nut-snmpinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the SNMP helper files regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

	@if perl -e 1; then \
		echo "Regenerating the USB helper files."; \
		$(top_srcdir)/tools/nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB helper files regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

website:
	@if python -c pass; then \
		echo "Regenerating the HTML and JSON formated HCL tables."; \
		$(top_srcdir)/tools/nut-hclinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the HTML and JSON formated HCL tables regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

# call the USB info script upon "make dist", and if Perl is present
# call the SNMP info script upon "make dist", and if Python is present
# and call both for building nut-scanner
dist-hook:
	@if python -c 1; then \
		echo "Regenerating the SNMP helper files."; \
		$(distdir)/nut-snmpinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the SNMP helper files regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

	@if perl -e 1; then \
		echo "Regenerating the USB helper files."; \
		$(distdir)/nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB helper files regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

	@if python -c pass; then \
		echo "Regenerating the HTML and JSON formated HCL tables."; \
		$(distdir)/nut-hclinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the HTML and JSON formated HCL tables regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

.PHONY: nut-scanner-deps nut-scanner-snmp-deps nut-scanner-usb-deps
