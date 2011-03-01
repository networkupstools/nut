

EXTRA_PROGRAMS = nut-scanner
nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = -I$(top_srcdir)/clients
nut_scanner_LDADD = ../../clients/libupsclient.la $(NETLIBS)

if WITH_SSL
  nut_scanner_CFLAGS += $(LIBSSL_CFLAGS)
  nut_scanner_LDADD += $(LIBSSL_LIBS)
endif
if WITH_USB
  nut_scanner_CFLAGS += $(LIBUSB_CFLAGS)
  nut_scanner_LDADD += $(LIBUSB_LIBS)
endif
if WITH_SNMP
  nut_scanner_CFLAGS += $(LIBNETSNMP_CFLAGS)
  nut_scanner_LDADD += $(LIBNETSNMP_LIBS)
endif
if WITH_NEONXML
  nut_scanner_CFLAGS += $(LIBNEON_CFLAGS)
  nut_scanner_LDADD += $(LIBNEON_LIBS)
endif

dist_noinst_HEADERS = nutscan-usb.h
