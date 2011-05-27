

EXTRA_PROGRAMS = nut-scanner
nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = -I$(top_srcdir)/clients
nut_scanner_LDADD = ../../clients/libupsclient.la $(NETLIBS)

lib_LTLIBRARIES = libnut-scan.la
libnut_scan_la_SOURCES = ../../clients/upsclient.h
libnut_scan_la_LIBADD = ../../clients/libupsclient.la $(NETLIBS)
libnut_scan_la_LDFLAGS = -version-info 1:0:0

if WITH_SSL
  nut_scanner_CFLAGS += $(LIBSSL_CFLAGS)
  nut_scanner_LDADD += $(LIBSSL_LIBS)
  libnut_scan_la_LIBADD += $(LIBSSL_LIBS)
endif
if WITH_USB
  nut_scanner_CFLAGS += $(LIBUSB_CFLAGS)
  nut_scanner_LDADD += $(LIBUSB_LIBS)
  libnut_scan_la_LIBADD += $(LIBUSB_LIBS)
endif
if WITH_SNMP
  nut_scanner_CFLAGS += $(LIBNETSNMP_CFLAGS)
  nut_scanner_LDADD += $(LIBNETSNMP_LIBS)
  libnut_scan_la_LIBADD += $(LIBNETSNMP_LIBS)
endif
if WITH_NEONXML
  nut_scanner_CFLAGS += $(LIBNEON_CFLAGS)
  nut_scanner_LDADD += $(LIBNEON_LIBS)
  libnut_scan_la_LIBADD += $(LIBNEON_LIBS)
endif

dist_noinst_HEADERS = nutscan-usb.h
