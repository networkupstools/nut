# Network UPS Tools: nut-scanner

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

# Generally, list headers and/or sources which are re-generated
# for nut-scanner in the parent dir
# Note there is also a libupsclient-version.h which we ensure and
# manage via usual `make` dependencies below.
NUT_SCANNER_DEPS_H = nutscan-usb.h nutscan-snmp.h
NUT_SCANNER_DEPS_C =

# General set of nut-scanner dependencies generated in the parent dir
NUT_SCANNER_DEPS = $(NUT_SCANNER_DEPS_H) $(NUT_SCANNER_DEPS_C)

BUILT_SOURCES = $(NUT_SCANNER_DEPS)
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = README.adoc

# Make sure we have the freshest files (no-op if built earlier and then
# no driver sources and other dependencies were edited by a developer)
$(NUT_SCANNER_DEPS): dummy
	+@cd .. && $(MAKE) $(AM_MAKEFLAGS) nut-scanner-deps

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/include/nut_version.h \
$(top_builddir)/clients/libupsclient-version.h \
$(top_builddir)/common/libnutwincompat.la \
$(top_builddir)/drivers/libserial-nutscan.la \
$(top_builddir)/common/libcommonstr.la \
$(top_builddir)/common/libcommon.la: dummy
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else
nut-scanner.c: $(top_builddir)/include/nut_version.h
nutscan-init.c: $(top_builddir)/clients/libupsclient-version.h

# We optionally append values to this below
bin_PROGRAMS =
lib_LTLIBRARIES =
include_HEADERS =
dist_noinst_HEADERS =

# Note: we only build nut-scanner, and its library, if libltdl was found (required ATM!)
if WITH_NUT_SCANNER
 bin_PROGRAMS += nut-scanner
 lib_LTLIBRARIES += libnutscan.la
endif WITH_NUT_SCANNER
libnutscan_la_SOURCES = scan_nut.c scan_nut_simulation.c scan_ipmi.c \
			nutscan-device.c nutscan-ip.c nutscan-display.c \
			nutscan-init.c scan_usb.c scan_snmp.c scan_xml_http.c \
			scan_avahi.c scan_eaton_serial.c nutscan-serial.c
libnutscan_la_LIBADD = $(NETLIBS)
libnutscan_la_LIBADD += $(top_builddir)/drivers/libserial-nutscan.la

# Make sure generated sources are there when needed
scan_snmp.c: nutscan-snmp.h
scan_usb.c: nutscan-usb.h

if WITH_LIBLTDL
libnutscan_la_LIBADD += $(LIBLTDL_LIBS)
endif WITH_LIBLTDL

if HAVE_SEMAPHORE_LIBS
# Are additional libraries needed for semaphore support?
libnutscan_la_LIBADD += $(SEMLIBS)
endif HAVE_SEMAPHORE_LIBS

libnutscan_la_LIBADD += $(SERLIBS)

libnutscan_la_LDFLAGS =
if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libnutscan_la_LIBADD += $(top_builddir)/common/libnutwincompat.la
  libnutscan_la_LDFLAGS += -no-undefined
endif HAVE_WINDOWS
# Technically, we might only have this one set on Windows so far...
libnutscan_la_LDFLAGS += @NETLIBS_GETADDRS@

#
# Below we set API versions of public libraries
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# Note that changes here may have to be reflected in packaging (the shared
# object .so names would differ)
#
# libnutscan version information
libnutscan_la_LDFLAGS += -version-info 2:6:0

# libnutscan exported symbols regex
# WARNING: Since the library includes parts of libcommon (as much as needed
# e.g. for logging or string manipulations), we export a few symbols from
# those too. This may cause issues for (third-party) code development that
# would use both libnutscan and libcommon or similar libs. Here we did have
# a problem with nut-scanner using two copies of common.c and conflicting
# copies of "nut_debug_level" making fun of our debug-logging attempts.
# One solution to tackle if needed for those cases would be to make some
# dynamic/shared libnutcommon (etc.)
libnutscan_la_LDFLAGS += -export-symbols-regex '^(nutscan_|nut_debug_level|s_upsdebug|fatalx|fatal_with_errno|xcalloc|xbasename|snprintfcat|max_threads|curr_threads|nut_report_config_flags|upsdebugx_report_search_paths|nut_prepare_search_paths|print_banner_once|suggest_doc_links)'
libnutscan_la_CFLAGS = \
			-I$(top_builddir)/clients -I$(top_srcdir)/clients \
			-I$(top_builddir)/include -I$(top_srcdir)/include \
			$(LIBLTDL_CFLAGS) -I$(top_srcdir)/drivers

libnutscan_la_LIBADD += $(top_builddir)/common/libcommonstr.la

nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = \
			-I$(top_builddir)/clients -I$(top_srcdir)/clients \
			-I$(top_builddir)/include -I$(top_srcdir)/include
nut_scanner_LDADD = libnutscan.la

# Hack to allow correct RPATH/RUNPATH like /usr/lib/mps/64 (Mozilla NSS)
# to be used on Solaris/illumos and similar platforms. For more details,
# see https://github.com/networkupstools/nut/issues/2674 but in short -
# we know the libs and options from pkg-config, and then they are well
# embedded into libnutscan.la, and then the -R/some/path options are
# chopped away by libtool when linking the program (while for some reason
# it adds the libraries wanted by libnutscan.la directly as libs wanted
# by the program, and ALSO they have a link chain through the shared lib).
# I'd argue this is a libtool bug, but here's a workaround that works...
nut_scanner_LDFLAGS = $(AM_LDFLAGS)

if WITH_SSL
  libnutscan_la_CFLAGS += $(LIBSSL_CFLAGS)
  libnutscan_la_LIBADD += $(LIBSSL_LIBS)
  nut_scanner_LDFLAGS += $(LIBSSL_LDFLAGS_RPATH)
endif WITH_SSL
if WITH_USB
  libnutscan_la_CFLAGS += $(LIBUSB_CFLAGS)
endif WITH_USB
# Note: do not indent automake "if" lines
if WITH_SNMP
  libnutscan_la_CFLAGS += $(LIBNETSNMP_CFLAGS)
if !WITH_OPENSSL
  libnutscan_la_CFLAGS += -UNETSNMP_USE_OPENSSL
endif !WITH_OPENSSL
if    WITH_SNMP_STATIC
    # MinGW builds of libnetsnmp are static-only, so we link it in:
    libnutscan_la_CFLAGS += -DWITH_SNMP_STATIC=1
    # Some workarouds here, to avoid libtool bailing out like this:
    # *** Warning: This system cannot link to static lib archive /usr/x86_64-w64-mingw32/lib//libnetsnmp.la.
    # *** I have the capability to make that library automatically link in when
    # *** you link to this library.  But I can only do this if you have a
    # *** shared version of the library, which you do not appear to have.
    # Note that LIBNETSNMP_LIBS prepared by nut_check_libnetsnmp.m4 for such
    # builds particularly WITH_SNMP_STATIC is special (with -Wl passthrough,
    # and for that automake requires LDFLAGS not LIBADD):
    libnutscan_la_LDFLAGS += $(LIBNETSNMP_LIBS)
endif WITH_SNMP_STATIC
endif WITH_SNMP
if WITH_NEON
  libnutscan_la_CFLAGS += $(LIBNEON_CFLAGS)
endif WITH_NEON
if WITH_AVAHI
  libnutscan_la_CFLAGS += $(LIBAVAHI_CFLAGS)
endif WITH_AVAHI
if WITH_IPMI
  libnutscan_la_CFLAGS += $(LIBIPMI_CFLAGS)
endif WITH_IPMI

# C is not a header, but there is no dist_noinst_SOURCES
dist_noinst_HEADERS += $(NUT_SCANNER_DEPS_H) $(NUT_SCANNER_DEPS_C)

# Optionally deliverable as part of NUT public API:
if WITH_DEV
 include_HEADERS += nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h
else !WITH_DEV
 dist_noinst_HEADERS += nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h
endif !WITH_DEV

dummy:

CLEANFILES += *-spellchecked README
MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	$(AM_V_at)rm -rf $(builddir)/.deps

# Helper for only the enabled libs to get built:
all-libs-local: $(lib_LTLIBRARIES) $(noinst_LTLIBRARIES) $(EXTRA_LTLIBRARIES)
