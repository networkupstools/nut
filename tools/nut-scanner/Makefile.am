
lib_LTLIBRARIES = libnut_scan.la
libnut_scan_la_SOURCES = scan_xml_http.c scan_xml_http.h scan_nut.c \
	scan.nut.h scan_avahi.c scan_avahi.h scan_ipmi.c scan_ipmi.h \
	device.c device.h
libnut_scan_la_LIBADD = ../../clients/libupsclient.la $(NETLIBS)
libnut_scan_la_LDFLAGS = -version-info 1:0:0
libnut_scan_la_CFLAGS = -I$(top_srcdir)/clients

bin_PROGRAMS = nut-scanner
nut_scanner_SOURCES = nut-scanner.c display.c display.h
nut_scanner_CFLAGS = -I$(top_srcdir)/clients
nut_scanner_LDADD = libnut_scan.la

if WITH_SSL
  libnut_scan_la_CFLAGS += $(LIBSSL_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBSSL_LIBS)
endif
if WITH_USB
  libnut_scan_la_SOURCES += scan_usb.c scan_usb.h
  libnut_scan_la_CFLAGS += $(LIBUSB_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBUSB_LIBS)
endif
if WITH_SNMP
  libnut_scan_la_SOURCES += scan_snmp.c scan_snmp.h
  libnut_scan_la_CFLAGS += $(LIBNETSNMP_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBNETSNMP_LIBS)
endif
if WITH_NEONXML
  libnut_scan_la_CFLAGS += $(LIBNEON_CFLAGS)
  libnut_scan_la_LIBADD += $(LIBNEON_LIBS)
endif

dist_noinst_HEADERS = nutscan-usb.h
