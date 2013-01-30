# TODO: remove redundancies!

# XXX this does not work with Automake!!!
#
# In fact the very concept is entirely antithetical to Automake.
#
# SUBDIRS are explicitly a listing of all the directories that make
# must recurse into BEFORE processing the current directory.
#
# These python scripts must be moved into a sub-directory, and _only_
# executed IF they need to be, and all the nut-scanner sources need
# to be moved out of a sub-directory into this directory.
#
# Anyway, for the time being, we force build in ./ before nut-scanner,
# to have nutscan-{usb,snmp}.h built before going into the nut-scanner
# sub-directory
SUBDIRS = . nut-scanner nutconf

EXTRA_DIST = nut-usbinfo.pl nut-hclinfo.py nut-recorder.sh svn2cl.authors nut-snmpinfo.py

all: nut-scanner-deps 

# XXX these rules are all bogus!  They cause un-named target files to
# always be rebuilt!  None of that is ever the right way to use make,
# and especially not Automake.  Explicit filenames and their exact
# dependencies need to be properly listed.
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
	@if python -c "import json,simplejson,lxml"; then \
		echo "Regenerating the HTML and JSON formated HCL tables."; \
		$(top_srcdir)/tools/nut-hclinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: either Python, or a required module (json, simplejson, lxml) "; \
		echo "is not available."; \
		echo "Skipping the HTML and JSON formated HCL tables regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

# call the USB info script upon "make dist", and if Perl is present
# call the SNMP info script upon "make dist", and if Python is present
# and call both for building nut-scanner
# also generate HCL data files
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

	@if python -c "import json,simplejson,lxml"; then \
		echo "Regenerating the HTML and JSON formated HCL tables."; \
		$(distdir)/nut-hclinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: either Python, or a required module (json, simplejson, lxml) "; \
		echo "is not available."; \
		echo "Skipping the HTML and JSON formated HCL tables regeneration."; \
		echo "----------------------------------------------------------------------"; \
	fi

.PHONY: nut-scanner-deps nut-scanner-snmp-deps nut-scanner-usb-deps
