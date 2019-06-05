# Generally, list headers and/or sources which are re-generated
# for nut-scanner in the parent dir
NUT_SCANNER_DEPS_H = nutscan-usb.h nutscan-snmp.h
NUT_SCANNER_DEPS_C =

# General set of nut-scanner dependencies generated in the parent dir
NUT_SCANNER_DEPS = $(NUT_SCANNER_DEPS_H) $(NUT_SCANNER_DEPS_C)

BUILT_SOURCES = $(NUT_SCANNER_DEPS)

# Make sure we have the freshest files (no-op if built earlier and then
# no driver sources and other dependencies were edited by a developer)
$(NUT_SCANNER_DEPS):
	cd ..; $(MAKE) $(AM_MAKEFLAGS) nut-scanner-deps

# Only build nut-scanner, and its library, if libltdl was found (required!)
if WITH_LIBLTDL
 bin_PROGRAMS = nut-scanner
 lib_LTLIBRARIES = libnutscan.la
endif
libnutscan_la_SOURCES = scan_nut.c scan_ipmi.c \
			nutscan-device.c nutscan-ip.c nutscan-display.c \
			nutscan-init.c scan_usb.c scan_snmp.c scan_xml_http.c \
			scan_avahi.c scan_eaton_serial.c nutscan-serial.c \
			../../drivers/serial.c \
			../../drivers/bcmxcp_ser.c \
			../../common/common.c ../../common/str.c
libnutscan_la_LIBADD = $(NETLIBS) $(LIBLTDL_LIBS)
#
# Below we set API versions of public libraries
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# Note that changes here may have to be reflected in packaging (the shared
# object .so names would differ)
#
# libnutscan version information
libnutscan_la_LDFLAGS = $(SERLIBS) -version-info 1:0:0
libnutscan_la_CFLAGS = -I$(top_srcdir)/clients -I$(top_srcdir)/include $(LIBLTDL_CFLAGS) -I$(top_srcdir)/drivers

nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = -I$(top_srcdir)/clients -I$(top_srcdir)/include
nut_scanner_LDADD = libnutscan.la

if WITH_SSL
  libnutscan_la_CFLAGS += $(LIBSSL_CFLAGS)
  libnutscan_la_LIBADD += $(LIBSSL_LIBS)
endif
if WITH_USB
  libnutscan_la_CFLAGS += $(LIBUSB_CFLAGS)
endif
if WITH_SNMP
  libnutscan_la_CFLAGS += $(LIBNETSNMP_CFLAGS)
endif
if WITH_NEON
  libnutscan_la_CFLAGS += $(LIBNEON_CFLAGS)
endif
if WITH_AVAHI
  libnutscan_la_CFLAGS += $(LIBAVAHI_CFLAGS)
endif
if WITH_IPMI
  libnutscan_la_CFLAGS += $(LIBIPMI_CFLAGS)
endif

# C is not a header, but there is no dist_noinst_SOURCES
dist_noinst_HEADERS = $(NUT_SCANNER_DEPS_H) $(NUT_SCANNER_DEPS_C)

if WITH_DEV
 include_HEADERS = nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h
else
 dist_noinst_HEADERS += nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h
endif

CLEANFILES = $(BUILT_SOURCES)
