# Generally, list headers and/or sources which are re-generated
# for nut-scanner in the parent dir
NUT_SCANNER_DEPS_H = nutscan-usb.h
NUT_SCANNER_DEPS_C = nutscan-snmp.c

# General set of nut-scanner dependencies generated in the parent dir
NUT_SCANNER_DEPS = $(NUT_SCANNER_DEPS_H) $(NUT_SCANNER_DEPS_C)

BUILT_SOURCES = $(NUT_SCANNER_DEPS)

# Make sure we have the freshest files (no-op if built earlier and then
# no driver sources and other dependencies were edited by a developer)
$(NUT_SCANNER_DEPS):
	cd ..; $(MAKE) $(AM_MAKEFLAGS) nut-scanner-deps

LUA_INCLUDE ?= -I/usr/include/lua5.1
CFLAGS_LUA  ?= $(LUA_INCLUDE)
LDFLAGS_LUA ?= $(LUA_LIB)
if WITH_DMF_LUA
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=1 -DWITH_DMF_FUNCTIONS=1 $(CFLAGS_LUA)
 LDFLAGS_DMF_LUA = $(LDFLAGS_LUA)
else
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=0
 LDFLAGS_DMF_LUA =
endif

# We optionally append values to this below
bin_PROGRAMS =
lib_LTLIBRARIES =

# Included dependencies
$(top_builddir)/include/nut_version.h $(top_builddir)/common/libcommon.la \
$(top_builddir)/common/libparseconf.la $(top_builddir)/common/libnutdmfsnmp.la \
$(top_builddir)/drivers/libserial-nutscan.la:
	cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

if WITH_NEON
if WITH_SNMP
if WITH_DMF
# Note: this does not require WITH_DMFMIB specifically, just the abilities
 bin_PROGRAMS += nut-scanner-reindex-dmfsnmp
 nut_scanner_reindex_dmfsnmp_SOURCES = nut-scanner-reindex-dmfsnmp.c \
    $(top_srcdir)/include/dmfsnmp.h $(top_builddir)/include/nut_version.h
 nut_scanner_reindex_dmfsnmp_LDADD = $(top_builddir)/common/libnutdmfsnmp.la \
    $(top_builddir)/common/libcommon.la $(top_builddir)/common/libparseconf.la \
    $(LIBNETSNMP_LIBS)
 nut_scanner_reindex_dmfsnmp_CFLAGS = -I$(top_srcdir)/include \
    -I$(top_srcdir)/drivers -I$(top_srcdir)/tools/nut-scanner \
    $(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) \
    -DDMFREINDEXER_MAKECHECK=0

if WITH_DMF_LUA
 nut_scanner_reindex_dmfsnmp_LDADD += $(LDFLAGS_DMF_LUA)
 nut_scanner_reindex_dmfsnmp_CFLAGS += $(CFLAGS_DMF_LUA)
endif
if ! WITH_LIBLTDL
 nut_scanner_reindex_dmfsnmp_LDADD += $(LIBNEON_LIBS)
endif
endif
endif
endif

# Only build nut-scanner, and its library, if libltdl was found (required ATM!)
if WITH_LIBLTDL
 bin_PROGRAMS += nut-scanner
 lib_LTLIBRARIES += libnutscan.la
endif
libnutscan_la_SOURCES = scan_nut.c scan_ipmi.c nutscan-snmp.c \
			nutscan-device.c nutscan-ip.c nutscan-display.c \
			nutscan-init.c scan_usb.c scan_snmp.c scan_xml_http.c \
			scan_avahi.c scan_eaton_serial.c nutscan-serial.c
libnutscan_la_LIBADD = $(NETLIBS) $(top_builddir)/drivers/libserial-nutscan.la
if WITH_LIBLTDL
libnutscan_la_LIBADD += $(LIBLTDL_LIBS)
endif
libnutscan_la_LDFLAGS = $(SERLIBS) -version-info 1:0:0
libnutscan_la_CFLAGS = -I$(top_srcdir)/clients -I$(top_srcdir)/include \
			$(LIBLTDL_CFLAGS) -I$(top_srcdir)/drivers
if WITH_DMF
libnutscan_la_CFLAGS += -DWITH_DMFMIB=1
if WITH_DMF_LUA
libnutscan_la_LDFLAGS += $(LDFLAGS_DMF_LUA)
libnutscan_la_CFLAGS += -DWITH_DMF_LUA=1 $(CFLAGS_DMF_LUA)
else
libnutscan_la_CFLAGS += -DWITH_DMF_LUA=0
endif
else
libnutscan_la_CFLAGS += -DWITH_DMFMIB=0 -DWITH_DMF_LUA=0
endif

if WITH_DMF
libnutscan_la_LIBADD += $(top_builddir)/common/libnutdmfsnmp.la
endif
libnutscan_la_LIBADD += $(top_builddir)/common/libcommonstr.la
#libnutscan_la_LIBADD += $(top_builddir)/common/libcommon.la $(top_builddir)/common/libparseconf.la

nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = -I$(top_srcdir)/clients -I$(top_srcdir)/include
nut_scanner_LDADD = libnutscan.la $(top_builddir)/common/libcommonstr.la

if WITH_SNMP
if WITH_NEON
nut_scanner_CFLAGS += -I$(top_srcdir)/drivers
nut_scanner_CFLAGS += $(LIBNEON_CFLAGS)
if WITH_DMF
nut_scanner_CFLAGS += -DWITH_DMFMIB=1
if WITH_DMF_LUA
nut_scanner_CFLAGS += -DWITH_DMF_LUA=1 $(CFLAGS_DMF_LUA)
nut_scanner_LDFLAGS = $(LDFLAGS_DMF_LUA)
else
nut_scanner_CFLAGS += -DWITH_DMF_LUA=0
endif
else
nut_scanner_CFLAGS += -DWITH_DMFMIB=0 -DWITH_DMF_LUA=0
endif
if ! WITH_LIBLTDL
# The libs needed to handle XML parsing and usage of DMF SNMP are ltdl()'ed
# but without this builds fail (TODO: Revise responsible code? LTDL in DMF?)
#	$(LIBNETSNMP_LIBS)
libnutscan_la_LIBADD += $(LIBNEON_LIBS)
endif
endif
endif

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
 include_HEADERS = nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h nutscan-snmp.h
else
 dist_noinst_HEADERS += nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h nutscan-snmp.h
endif

CLEANFILES = $(BUILT_SOURCES)
