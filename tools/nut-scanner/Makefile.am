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
NUT_SCANNER_DEPS_H = nutscan-usb.h
### DMF recipe modification: we generate C not H here for SNMP:
###NUT_SCANNER_DEPS_H += nutscan-snmp.h
NUT_SCANNER_DEPS_C = nutscan-snmp.c

# General set of nut-scanner dependencies generated in the parent dir
NUT_SCANNER_DEPS = $(NUT_SCANNER_DEPS_H) $(NUT_SCANNER_DEPS_C)

BUILT_SOURCES = $(NUT_SCANNER_DEPS)
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = README.adoc

# Built by same rule, so order them and avoid stepping on each other's toes
# (TODO: Better deps automation than hard-coding here?)
nutscan-usb.h: nutscan-snmp.c

# Make sure we have the freshest files (no-op if built earlier and then
# no driver sources and other dependencies were edited by a developer)
$(NUT_SCANNER_DEPS): dummy @dotMAKE@
	+@cd .. && $(MAKE) $(AM_MAKEFLAGS) nut-scanner-deps

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/clients/libupsclient-version.h \
$(top_builddir)/common/libparseconf.la \
$(top_builddir)/common/libnutdmfsnmp.la \
$(top_builddir)/common/libnutwincompat.la \
$(top_builddir)/drivers/libserial-nutscan.la \
$(top_builddir)/common/libcommonstr.la \
$(top_builddir)/common/libcommonversion.la \
$(top_builddir)/common/libcommon.la: dummy @dotMAKE@
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

$(top_builddir)/include/nut_version.h: @dotMAKE@
	+@if [ -s '$@' ] && ( [ x"$(NUT_VERSION_H_GENERATED)" = xtrue ] || [ x"$${NUT_VERSION_H_GENERATED}" = xtrue ] ) ; then \
	    if [ x"$(MAINTAINER_GENERATE_HEADER_DEBUG)" = xyes ] ; then \
	        echo "=== SKIP (nut-scanner) $@ (NUT_VERSION_H_GENERATED makevar=$(NUT_VERSION_H_GENERATED) shellvar=$${NUT_VERSION_H_GENERATED})" >&2; \
	    fi; \
	    exit 0 ; \
	  fi ; \
	  cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

# Builds from root dir arrange stuff decently. Make sure parallel builds
# started from scratch right in this dir get dependencies in proper order
# (sub-makes are independent as far as trying to write into same files):
$(top_builddir)/common/libcommon.la: $(top_builddir)/common/libparseconf.la
$(top_builddir)/common/libcommonversion.la: $(top_builddir)/include/nut_version.h
# To extract the values used in the factually built library, this header's
# recipe internally (clients/Makefile.am) requires libupsclient.la, and in
# turn libcommonclient.la and in turn libparseconf.la:
$(top_builddir)/clients/libupsclient-version.h: $(top_builddir)/common/libparseconf.la

# do not hard depend on '../clients/libupsclient-version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../clients) will ensure libupsclient-version.h will
# be built before anything else
# FIXME: this can cause re-evaluation, possibly rebuilds, of libupsclient.la
#  to look up its metadata and generate libupsclient-version.h which
#  may be a bit of overkill (especially in cross-build cases etc.)
#  Notably this pops up in `configure && make dist(check)` rituals.
nutscan-init.c: $(top_builddir)/clients/libupsclient-version.h

if ENABLE_SHARED_PRIVATE_LIBS
$(top_builddir)/common/libcommonversion-private.la \
$(top_builddir)/common/libcommonclient.la \
$(top_builddir)/common/libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-all.la \
$(top_builddir)/common/libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-client.la: dummy @dotMAKE@
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

$(top_builddir)/common/libcommonversion-private.la: $(top_builddir)/include/nut_version.h
$(top_builddir)/common/libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-all.la: $(top_builddir)/common/libcommon.la $(top_builddir)/common/libcommonversion-private.la
$(top_builddir)/common/libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-client.la: $(top_builddir)/common/libcommonclient.la $(top_builddir)/common/libcommonversion-private.la
endif ENABLE_SHARED_PRIVATE_LIBS

# We optionally append values to this below
bin_PROGRAMS =
lib_LTLIBRARIES =
include_HEADERS =
dist_noinst_HEADERS =

CFLAGS_LUA  = $(LUA_INCLUDE)
LDFLAGS_LUA = $(LUA_LIB)
if WITH_DMF_LUA
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=1 -DWITH_DMF_FUNCTIONS=1 $(CFLAGS_LUA)
 LDFLAGS_DMF_LUA = $(LDFLAGS_LUA)
else !WITH_DMF_LUA
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=0
 LDFLAGS_DMF_LUA =
endif !WITH_DMF_LUA

if WITH_REGENERATE_DMF_NUTSCAN
# Note: this does not require WITH_DMFMIB specifically, just the abilities
#  WITH_DMF WITH_NEON WITH_SNMP
 nut-scanner-reindex-dmfsnmp.c: $(top_builddir)/include/nut_version.h $(top_srcdir)/include/dmfsnmp.h

 bin_PROGRAMS += nut-scanner-reindex-dmfsnmp
 nut_scanner_reindex_dmfsnmp_SOURCES = nut-scanner-reindex-dmfsnmp.c
 nut_scanner_reindex_dmfsnmp_LDADD = \
	$(top_builddir)/common/libnutdmfsnmp.la \
	$(top_builddir)/common/libcommon.la \
	$(top_builddir)/common/libparseconf.la \
	$(LIBNETSNMP_LIBS)
 nut_scanner_reindex_dmfsnmp_CFLAGS = -I$(top_srcdir)/include \
	-I$(top_srcdir)/drivers -I$(top_srcdir)/tools/nut-scanner \
	$(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) \
	-DDMFREINDEXER_MAKECHECK=0

if WITH_DMF_LUA
 nut_scanner_reindex_dmfsnmp_LDADD += $(LDFLAGS_DMF_LUA)
 nut_scanner_reindex_dmfsnmp_CFLAGS += $(CFLAGS_DMF_LUA)
endif WITH_DMF_LUA
if ! WITH_LIBLTDL
 nut_scanner_reindex_dmfsnmp_LDADD += $(LIBNEON_LIBS)
endif ! WITH_LIBLTDL
endif WITH_REGENERATE_DMF_NUTSCAN

# Note: we only build nut-scanner, and its library, if libltdl was found (required ATM!)
if WITH_NUT_SCANNER
 bin_PROGRAMS += nut-scanner
 lib_LTLIBRARIES += libnutscan.la
endif WITH_NUT_SCANNER
libnutscan_la_SOURCES = scan_nut.c scan_nut_simulation.c scan_ipmi.c \
			nutscan-snmp.c \
			nutscan-device.c nutscan-ip.c nutscan-display.c \
			nutscan-init.c scan_usb.c scan_snmp.c scan_xml_http.c \
			scan_avahi.c scan_eaton_serial.c nutscan-serial.c \
			scan_upower.c
libnutscan_la_LIBADD = $(NETLIBS)
libnutscan_la_LIBADD += $(top_builddir)/drivers/libserial-nutscan.la

# Make sure generated sources are there when needed
scan_snmp.c: nutscan-snmp.h
scan_usb.c: nutscan-usb.h

if WITH_LIBLTDL
libnutscan_la_LIBADD += $(LIBLTDL_LIBS)
endif WITH_LIBLTDL

if WITH_THREADING
if HAVE_SEMAPHORE_LIBS
# Are additional libraries needed for semaphore support?
libnutscan_la_LIBADD += $(SEMLIBS)
endif HAVE_SEMAPHORE_LIBS
endif WITH_THREADING

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
libnutscan_la_LDFLAGS += -version-info 5:0:1

# libnutscan exported symbols regex
# WARNING: Since the library includes parts of libcommon (as much as needed
# e.g. for logging or string manipulations), we export a few symbols from
# those too. This may cause issues for (third-party) code development that
# would use both libnutscan and libcommon or similar libs. Here we did have
# a problem with nut-scanner using two copies of common.c and conflicting
# copies of "nut_debug_level" making fun of our debug-logging attempts.
# One solution to tackle if needed for those cases would be to make some
# dynamic/shared libnutcommon (etc.)
if WITH_DMF
export_symbols_DMF=(dmf)?nutscan_|uninit_snmp_device_table
else !WITH_DMF
export_symbols_DMF=nutscan_
endif !WITH_DMF

libnutscan_la_LDFLAGS += -export-symbols-regex '^($(export_symbols_DMF)|nut_debug_level|s_upsdebug|fatalx|fatal_with_errno|xcalloc|xbasename|snprintfcat|snprintf_dynamic|max_threads|curr_threads|nut_report_config_flags|upsdebugx_report_search_paths|nut_prepare_search_paths|print_banner_once|suggest_doc_links)'

libnutscan_la_CFLAGS = \
			-I$(top_builddir)/clients -I$(top_srcdir)/clients \
			-I$(top_builddir)/include -I$(top_srcdir)/include \
			$(LIBLTDL_CFLAGS) -I$(top_srcdir)/drivers

if WITH_NEON
if WITH_SNMP
if WITH_DMF
libnutscan_la_CFLAGS += -DWITH_DMFMIB=1
if WITH_DMF_LUA
libnutscan_la_LDFLAGS += $(LDFLAGS_DMF_LUA)
libnutscan_la_CFLAGS += -DWITH_DMF_LUA=1 $(CFLAGS_DMF_LUA)
else !WITH_DMF_LUA
libnutscan_la_CFLAGS += -DWITH_DMF_LUA=0
endif !WITH_DMF_LUA
libnutscan_la_LIBADD += $(top_builddir)/common/libnutdmfsnmp.la
else !WITH_DMF
libnutscan_la_CFLAGS += -DWITH_DMFMIB=0 -DWITH_DMF_LUA=0
endif !WITH_DMF
endif WITH_SNMP
endif WITH_NEON

libnutscan_la_LIBADD += \
	$(top_builddir)/common/libcommonversion.la \
	$(top_builddir)/common/libcommonstr.la

nut_scanner_SOURCES = nut-scanner.c
nut_scanner_CFLAGS = \
			-I$(top_builddir)/clients -I$(top_srcdir)/clients \
			-I$(top_builddir)/include -I$(top_srcdir)/include
nut_scanner_LDADD = libnutscan.la
#FTY-old# nut_scanner_LDADD = $(top_builddir)/common/libcommon.la libnutscan.la $(top_builddir)/common/libcommonstr.la

if ENABLE_SHARED_PRIVATE_LIBS
# ensure the "fatal_with_errno" symbol, and others that libnutscan uses
# TODO: Check if common-client suffices (or do we need whatever more "all" brings?)
nut_scanner_LDADD += $(top_builddir)/common/libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-all.la
# get LIBNUTPRIVATE_UPS_VERSION seen for the library code itself
libnutscan_la_LIBADD += $(top_builddir)/common/libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-all.la
endif ENABLE_SHARED_PRIVATE_LIBS

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

if WITH_SNMP
if WITH_NEON
nut_scanner_CFLAGS += -I$(top_srcdir)/drivers
nut_scanner_CFLAGS += $(LIBNEON_CFLAGS)
if WITH_DMF
nut_scanner_CFLAGS += -DWITH_DMFMIB=1
if WITH_DMF_LUA
nut_scanner_CFLAGS += -DWITH_DMF_LUA=1 $(CFLAGS_DMF_LUA)
nut_scanner_LDFLAGS += $(LDFLAGS_DMF_LUA)
else !WITH_DMF_LUA
nut_scanner_CFLAGS += -DWITH_DMF_LUA=0
endif !WITH_DMF_LUA
else !WITH_DMF
nut_scanner_CFLAGS += -DWITH_DMFMIB=0 -DWITH_DMF_LUA=0
endif !WITH_DMF
if ! WITH_LIBLTDL
# The libs needed to handle XML parsing and usage of DMF SNMP are ltdl()'ed
# but without this builds fail (TODO: Revise responsible code? LTDL in DMF?)
#	$(LIBNETSNMP_LIBS)
libnutscan_la_LIBADD += $(LIBNEON_LIBS)
endif !WITH_LIBLTDL
endif WITH_NEON
endif WITH_SNMP

if WITH_SSL
  libnutscan_la_CFLAGS += $(LIBSSL_CFLAGS)
  libnutscan_la_LIBADD += $(LIBSSL_LDFLAGS_RPATH) $(LIBSSL_LIBS)
  nut_scanner_LDFLAGS += $(LIBSSL_LDFLAGS_RPATH) $(LIBSSL_LIBS)
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
if WITH_UPOWER
  libnutscan_la_CFLAGS += $(LIBGIO_CFLAGS)
endif WITH_UPOWER

# C is not a header, but there is no dist_noinst_SOURCES
dist_noinst_HEADERS += $(NUT_SCANNER_DEPS_H) $(NUT_SCANNER_DEPS_C)

# Optionally deliverable as part of NUT public API:
if WITH_DEV
 include_HEADERS += nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h
 include_HEADERS += nutscan-snmp.h
else !WITH_DEV
 dist_noinst_HEADERS += nut-scan.h nutscan-device.h nutscan-ip.h nutscan-init.h nutscan-serial.h
 dist_noinst_HEADERS += nutscan-snmp.h
endif !WITH_DEV

dummy:

CLEANFILES += *-spellchecked README
CLEANFILES += nut-scanner-reindex-dmfsnmp dmf-reindex
MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	$(AM_V_at)rm -rf $(builddir)/.deps

EXTRA_PROGS =
if WITH_REGENERATE_DMF_NUTSCAN
EXTRA_PROGS += nut-scanner-reindex-dmfsnmp
endif WITH_REGENERATE_DMF_NUTSCAN

# Helper for only the enabled libs to get built:
all-libs-local: $(lib_LTLIBRARIES) $(noinst_LTLIBRARIES) $(EXTRA_LTLIBRARIES) $(EXTRA_PROGS)
