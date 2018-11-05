# SUBDIRS are explicitly a listing of all the directories that make
# must recurse into BEFORE processing the current directory.
#
# These python scripts must be moved into a sub-directory, and _only_
# executed IF they need to be, and all the nut-scanner sources need
# to be moved out of a sub-directory into this directory.
#
# Anyway, for the time being, we force build in ./ before nut-scanner,
# to have nutscan-{usb,snmp}.h built before going into the nut-scanner
# sub-directory. For good measure we also call this from nut-scanner's
# make, to handle developer workflow (editing the *.c sources this uses).
SUBDIRS = . nut-scanner

EXTRA_DIST = nut-usbinfo.pl nut-recorder.sh nut-ddl-dump.sh \
  gitlog2changelog.py nut-snmpinfo.py driver-list-format.sh

# DMF recipe modification:
#GENERATED_SNMP_FILES = nut-scanner/nutscan-snmp.h
GENERATED_SNMP_FILES = nut-scanner/nutscan-snmp.c

GENERATED_USB_FILES = nut-scanner/nutscan-usb.h

# Hotplug output file
GENERATED_USB_OS_FILES = ../scripts/hotplug/libhid.usermap

# udev output file
GENERATED_USB_OS_FILES += ../scripts/udev/nut-usbups.rules.in

# BSD devd output file
GENERATED_USB_OS_FILES += ../scripts/devd/nut-usb.conf.in

# UPower output file
GENERATED_USB_OS_FILES += ../scripts/upower/95-upower-hid.rules

all: nut-scanner-deps $(GENERATED_USB_OS_FILES)

# This target is called from the making of nut-scanner to ensure its bits
nut-scanner-deps: $(GENERATED_SNMP_FILES) $(GENERATED_USB_FILES)

# Aliases for particular files, if someone has a need:
nut-scanner-deps-snmpinfo: $(GENERATED_SNMP_FILES)
nut-scanner-deps-usb: $(GENERATED_USB_FILES)

$(GENERATED_SNMP_FILES): $(top_srcdir)/drivers/*-mib.c
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

$(GENERATED_USB_FILES): $(top_srcdir)/drivers/*-hid.c $(top_srcdir)/drivers/*usb*.c $(top_srcdir)/drivers/nutdrv_qx.c
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
