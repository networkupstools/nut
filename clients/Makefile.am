# Network UPS Tools: clients

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

EXTRA_DIST =

# nutclient.cpp for some legacy reason (maybe initial detached development?)
# optionally includes "common.h" with the NUT build setup - and this option
# was never triggered in fact, not until pushed through command line like this:
AM_CXXFLAGS = -DHAVE_NUTCOMMON=1 -I$(top_srcdir)/include

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/common/libcommon.la \
$(top_builddir)/common/libcommonclient.la \
$(top_builddir)/common/libparseconf.la: dummy
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

LDADD_FULL = $(top_builddir)/common/libcommon.la libupsclient.la $(NETLIBS)
if WITH_SSL
  LDADD_FULL += $(LIBSSL_LIBS)
endif

LDADD_CLIENT = $(top_builddir)/common/libcommonclient.la libupsclient.la $(NETLIBS)
if WITH_SSL
  LDADD_CLIENT += $(LIBSSL_LIBS)
endif

# by default, link programs in this directory with
# the more compact libcommonclient.a bundle
LDADD = $(LDADD_CLIENT)

# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include
if WITH_SSL
  AM_CFLAGS += $(LIBSSL_CFLAGS)
endif
if WITH_CGI
  AM_CFLAGS += $(LIBGD_CFLAGS)
endif

bin_PROGRAMS = upsc upslog upsrw upscmd
dist_bin_SCRIPTS = upssched-cmd
sbin_PROGRAMS = upsmon upssched
if HAVE_WINDOWS_SOCKETS
  sbin_PROGRAMS += message
endif

lib_LTLIBRARIES = libupsclient.la
if HAVE_CXX11
  lib_LTLIBRARIES += libnutclient.la
  lib_LTLIBRARIES += libnutclientstub.la
endif

# Optionally deliverable as part of NUT public API:
if WITH_DEV
 include_HEADERS = upsclient.h
if HAVE_CXX11
 include_HEADERS += nutclient.h nutclientmem.h
else !HAVE_CXX11
 EXTRA_DIST += nutclient.h nutclientmem.h
endif !HAVE_CXX11
endif WITH_DEV

if WITH_CGI
 cgiexec_PROGRAMS = upsstats.cgi upsimage.cgi upsset.cgi
endif

upsc_SOURCES = upsc.c upsclient.h
upscmd_SOURCES = upscmd.c upsclient.h
upsrw_SOURCES = upsrw.c upsclient.h
upslog_SOURCES = upslog.c upsclient.h upslog.h
upsmon_SOURCES = upsmon.c upsmon.h upsclient.h
upsmon_LDADD = $(LDADD_FULL)
if HAVE_WINDOWS_SOCKETS
message_SOURCES = message.c
endif

upssched_SOURCES = upssched.c upssched.h
upssched_LDADD = $(top_builddir)/common/libcommonclient.la $(top_builddir)/common/libparseconf.la $(NETLIBS)

upsimage_cgi_SOURCES = upsimage.c upsclient.h upsimagearg.h cgilib.c cgilib.h
upsimage_cgi_LDADD = $(LDADD) $(LIBGD_LDFLAGS)

upsset_cgi_SOURCES = upsset.c upsclient.h cgilib.c cgilib.h
upsstats_cgi_SOURCES = upsstats.c upsclient.h status.h upsstats.h	\
 upsimagearg.h cgilib.c cgilib.h

# not LDADD... why?
libupsclient_la_SOURCES = upsclient.c upsclient.h
libupsclient_la_LIBADD = $(top_builddir)/common/libcommonclient.la
if HAVE_WINDOWS_SOCKETS
  libupsclient_la_LIBADD += -lws2_32
endif
if WITH_SSL
  libupsclient_la_LIBADD += $(LIBSSL_LIBS)
endif

# Below we set API versions of public libraries
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# Note that changes here may have to be reflected in packaging (the shared
# object .so names would differ)

# libupsclient version information
libupsclient_la_LDFLAGS = -version-info 6:1:0
libupsclient_la_LDFLAGS += -export-symbols-regex ^upscli_
if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libupsclient_la_LDFLAGS += -no-undefined
endif

if HAVE_CXX11
# libnutclient version information and build
libnutclient_la_SOURCES = nutclient.h nutclient.cpp
libnutclient_la_LDFLAGS = -version-info 2:2:0
# Needed in not-standalone builds with -DHAVE_NUTCOMMON=1
# which is defined for in-tree CXX builds above:
libnutclient_la_LIBADD = $(top_builddir)/common/libcommonclient.la
if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libnutclient_la_LDFLAGS += -no-undefined
endif
else
EXTRA_DIST += nutclient.h nutclient.cpp
endif

if HAVE_CXX11
# libnutclientstub version information and build
libnutclientstub_la_SOURCES = nutclientmem.h nutclientmem.cpp
libnutclientstub_la_LDFLAGS = -version-info 1:1:0
libnutclientstub_la_LIBADD = libnutclient.la
if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libnutclientstub_la_LDFLAGS += -no-undefined
endif
else
EXTRA_DIST += nutclientmem.h nutclientmem.cpp
endif

dummy:

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	$(AM_V_at)rm -rf $(builddir)/.deps
