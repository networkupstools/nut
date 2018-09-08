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
SUBDIRS = . nut-scanner

EXTRA_DIST = nut-usbinfo.pl nut-recorder.sh nut-ddl-dump.sh \
  gitlog2changelog.py nut-snmpinfo.py driver-list-format.sh

GENERATED_USB_FILES = nut-scanner/nutscan-usb.h

# Hotplug output file
GENERATED_USB_FILES += ../scripts/hotplug/libhid.usermap

# udev output file
GENERATED_USB_FILES += ../scripts/udev/nut-usbups.rules.in

# BSD devd output file
GENERATED_USB_FILES += ../scripts/devd/nut-usb.conf.in

# UPower output file
GENERATED_USB_FILES += ../scripts/upower/95-upower-hid.rules

all: nut-scanner-deps $(GENERATED_USB_FILES)

# XXX these rules are all bogus!  They cause un-named target files to
# always be rebuilt!  None of that is ever the right way to use make,
# and especially not Automake.  Explicit filenames and their exact
# dependencies need to be properly listed.
nut-scanner-deps: nut-scanner/nutscan-snmp.h nut-scanner/nutscan-usb.h

nut-scanner/nutscan-snmp.h: $(top_srcdir)/drivers/*-mib.c
	@if python -c 1; then \
		echo "Regenerating the SNMP helper files in SRC dir."; \
		TOP_SRCDIR="$(top_srcdir)" ; export TOP_SRCDIR; \
		TOP_BUILDDIR="$(top_builddir)" ; export TOP_BUILDDIR; \
		cd $(builddir) && $(top_srcdir)/tools/nut-snmpinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the SNMP helper files regeneration in SRC dir."; \
		echo "----------------------------------------------------------------------"; \
	fi

$(GENERATED_USB_FILES): $(top_srcdir)/drivers/*-hid.c $(top_srcdir)/drivers/*usb*.c
	@if perl -e 1; then \
		echo "Regenerating the USB helper files in SRC dir."; \
		TOP_SRCDIR="$(top_srcdir)" ; export TOP_SRCDIR; \
		TOP_BUILDDIR="$(top_builddir)" ; export TOP_BUILDDIR; \
		cd $(builddir) && $(top_srcdir)/tools/nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB helper files regeneration in SRC dir."; \
		echo "----------------------------------------------------------------------"; \
	fi

# call the USB info script upon "make dist", and if Perl is present
# call the SNMP info script upon "make dist", and if Python is present
# and call both for building nut-scanner
# Also ensure that data/driver.list is well formatted
# NOTE: Beware that current working directory for the script should be builddir
# so it may write the files in "dist" case (read-only sources), but the script
# is called from the distdir where its copy is present.
dist-hook:
	@if python -c 1; then \
		echo "Regenerating the SNMP helper files in DIST dir."; \
		TOP_SRCDIR="$(top_srcdir)" ; export TOP_SRCDIR; \
		TOP_BUILDDIR="$(top_builddir)" ; export TOP_BUILDDIR; \
		$(distdir)/nut-snmpinfo.py; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Python is not available."; \
		echo "Skipping the SNMP helper files regeneration in DIST dir."; \
		echo "----------------------------------------------------------------------"; \
	fi

	@if perl -e 1; then \
		echo "Regenerating the USB helper files in DIST dir."; \
		TOP_SRCDIR="$(top_srcdir)" ; export TOP_SRCDIR; \
		TOP_BUILDDIR="$(top_builddir)" ; export TOP_BUILDDIR; \
		$(distdir)/nut-usbinfo.pl; \
	else \
		echo "----------------------------------------------------------------------"; \
		echo "Warning: Perl is not available."; \
		echo "Skipping the USB helper files regeneration in DIST dir."; \
		echo "----------------------------------------------------------------------"; \
	fi

	@$(distdir)/driver-list-format.sh;

.PHONY: nut-scanner-deps nut-scanner-snmp-deps nut-scanner-usb-deps
