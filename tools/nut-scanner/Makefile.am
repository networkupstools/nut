BUILT_SOURCES = nutscan-usb.h nutscan-snmp.h

nutscan-usb.h nutscan-snmp.h:
	cd ..; $(MAKE) $(AM_MAKEFLAGS) nut-scanner-deps

lib_LTLIBRARIES = libnutscan.la
libnutscan_la_SOURCES = scan_nut.c scan_ipmi.c \
	nutscan-device.c nutscan-ip.c nutscan-display.c
libnutscan_la_LIBADD = ../../clients/libupsclient.la $(NETLIBS)
libnutscan_la_LDFLAGS = -version-info 1:0:0
libnutscan_la_CFLAGS = -I$(top_srcdir)/clients -I$(top_srcdir)/include

bin_PROGRAMS = nut-scanner
nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = -I$(top_srcdir)/clients -I$(top_srcdir)/include
nut_scanner_LDADD = libnutscan.la

if WITH_SSL
  libnutscan_la_CFLAGS += $(LIBSSL_CFLAGS)
  libnutscan_la_LIBADD += $(LIBSSL_LIBS)
endif
if WITH_USB
  libnutscan_la_SOURCES += scan_usb.c
  libnutscan_la_CFLAGS += $(LIBUSB_CFLAGS)
  libnutscan_la_LIBADD += $(LIBUSB_LIBS)
endif
if WITH_SNMP
  libnutscan_la_SOURCES += scan_snmp.c
  libnutscan_la_CFLAGS += $(LIBNETSNMP_CFLAGS)
  libnutscan_la_LIBADD += $(LIBNETSNMP_LIBS)
endif
if WITH_NEONXML
  libnutscan_la_SOURCES += scan_xml_http.c
  libnutscan_la_CFLAGS += $(LIBNEON_CFLAGS)
  libnutscan_la_LIBADD += $(LIBNEON_LIBS)
endif
if WITH_AVAHI
  libnutscan_la_SOURCES += scan_avahi.c
  libnutscan_la_CFLAGS += $(LIBAVAHI_CFLAGS)
  libnutscan_la_LIBADD += $(LIBAVAHI_LIBS)
endif

dist_noinst_HEADERS = nutscan-usb.h nutscan-snmp.h

if WITH_DEV
 include_HEADERS = nut-scan.h nutscan-device.h nutscan-ip.h
else
 dist_noinst_HEADERS += nut-scan.h nutscan-device.h nutscan-ip.h
endif

CLEANFILES = nutscan-usb.h nutscan-snmp.h

