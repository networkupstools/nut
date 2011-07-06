
nutscan-usb.h nutscan-snmp.h:
	cd ..; $(MAKE) $(AM_MAKEFLAGS) nut-scanner-deps

lib_LTLIBRARIES = libnut_scan.la
libnut_scan_la_SOURCES = scan_nut.c scan_avahi.c scan_ipmi.c \
	device.c ip.c display.c
libnut_scan_la_LIBADD = ../../clients/libupsclient.la $(NETLIBS)
libnut_scan_la_LDFLAGS = -version-info 1:0:0
libnut_scan_la_CFLAGS = -I$(top_srcdir)/clients -I$(top_srcdir)/include

bin_PROGRAMS = nut-scanner
nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = -I$(top_srcdir)/clients
nut_scanner_LDADD = libnut_scan.la

if WITH_SSL
  libnut_scan_la_CFLAGS += $(LIBSSL_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBSSL_LIBS)
endif
if WITH_USB
  libnut_scan_la_SOURCES += scan_usb.c
  libnut_scan_la_CFLAGS += $(LIBUSB_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBUSB_LIBS)
endif
if WITH_SNMP
  libnut_scan_la_SOURCES += scan_snmp.c
  libnut_scan_la_CFLAGS += $(LIBNETSNMP_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBNETSNMP_LIBS)
endif
if WITH_NEONXML
  libnut_scan_la_SOURCES += scan_xml_http.c
  libnut_scan_la_CFLAGS += $(LIBNEON_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBNEON_LIBS)
endif

dist_noinst_HEADERS = nut-scan.h nutscan-usb.h nutscan-snmp.h device.h ip.h 

CLEANFILES = nutscan-usb.h nutscan-snmp.h

