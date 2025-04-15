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
CLEANFILES =

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
  LDADD_FULL += $(LIBSSL_LIBS) $(LIBSSL_LDFLAGS_RPATH)
endif WITH_SSL

LDADD_CLIENT = $(top_builddir)/common/libcommonclient.la libupsclient.la $(NETLIBS)
if WITH_SSL
  LDADD_CLIENT += $(LIBSSL_LIBS) $(LIBSSL_LDFLAGS_RPATH)
endif WITH_SSL

# by default, link programs in this directory with
# the more compact libcommonclient.a bundle
LDADD = $(LDADD_CLIENT)

# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include
if WITH_SSL
  AM_CFLAGS += $(LIBSSL_CFLAGS)
endif WITH_SSL
if WITH_CGI
  AM_CFLAGS += $(LIBGD_CFLAGS)
endif WITH_CGI

bin_PROGRAMS = upsc upslog upsrw upscmd
dist_bin_SCRIPTS = upssched-cmd
sbin_PROGRAMS = upsmon upssched
if HAVE_WINDOWS_SOCKETS
  sbin_PROGRAMS += message
endif HAVE_WINDOWS_SOCKETS

lib_LTLIBRARIES = libupsclient.la
if HAVE_CXX11
  lib_LTLIBRARIES += libnutclient.la
  lib_LTLIBRARIES += libnutclientstub.la
endif HAVE_CXX11

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
endif WITH_CGI

upsc_SOURCES = upsc.c upsclient.h
upscmd_SOURCES = upscmd.c upsclient.h
upsrw_SOURCES = upsrw.c upsclient.h
upslog_SOURCES = upslog.c upsclient.h upslog.h
upslog_LDADD = $(LDADD_FULL)
upsmon_SOURCES = upsmon.c upsmon.h upsclient.h
upsmon_LDADD = $(LDADD_FULL)
if HAVE_WINDOWS_SOCKETS
message_SOURCES = message.c
endif HAVE_WINDOWS_SOCKETS

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
endif HAVE_WINDOWS_SOCKETS
if WITH_SSL
  libupsclient_la_LIBADD += $(LIBSSL_LDFLAGS_RPATH) $(LIBSSL_LIBS)
endif WITH_SSL

# Below we set API versions of public libraries
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# Note that changes here may have to be reflected in packaging (the shared
# object .so names would differ)

# libupsclient version information
libupsclient_la_LDFLAGS = -version-info 6:2:0
libupsclient_la_LDFLAGS += -export-symbols-regex '^(upscli_|nut_debug_level)'
#|s_upsdebug|fatalx|fatal_with_errno|xcalloc|xbasename|print_banner_once)'
if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libupsclient_la_LDFLAGS += -no-undefined
endif HAVE_WINDOWS

# ./clients/libupsclient.la samples (partial!) on...
# Linux:
#   # The name that we can dlopen(3).
#   dlname='libupsclient.so.6'
#   # Names of this library.
#   library_names='libupsclient.so.6.0.1 libupsclient.so.6 libupsclient.so'
#   # Directory that this library needs to be installed in:
#   libdir='/usr/local/ups/lib'
# WIN32:
#   dlname='libupsclient-6.dll'
#   library_names='libupsclient.dll.a'
#   libdir='//lib'
CLEANFILES += libupsclient-version.h libupsclient-version.h.tmp*
libupsclient-version.h: libupsclient.la
	@echo "  GENERATE-HEADER	$@" ; \
	 RES=0; \
	 dlname_filter() { sed -e 's/^[^=]*=//' -e 's/^"\(.*\)"$$/\1/' -e 's/^'"'"'\(.*\)'"'"'$$/\1/' ; }; \
	 SOFILE_LIBUPSCLIENT="`grep -E '^dlname' '$?' | dlname_filter`" \
	 || SOFILE_LIBUPSCLIENT="" ; \
	 if [ x"$${SOFILE_LIBUPSCLIENT-}" = x ] ; then \
	    printf "#ifdef SOFILE_LIBUPSCLIENT\n# undef SOFILE_LIBUPSCLIENT\n#endif\n\n" ; \
	    printf "#ifdef SOPATH_LIBUPSCLIENT\n# undef SOPATH_LIBUPSCLIENT\n#endif\n\n" ; \
	 else \
	    printf "#ifndef SOFILE_LIBUPSCLIENT\n# define SOFILE_LIBUPSCLIENT \"%s\"\n#endif\n\n" "$${SOFILE_LIBUPSCLIENT}" ; \
	    printf "#ifndef SOPATH_LIBUPSCLIENT\n# define SOPATH_LIBUPSCLIENT \"%s/%s\"\n#endif\n\n" "@LIBDIR@" "$${SOFILE_LIBUPSCLIENT}" ; \
	 fi > "$@".tmp.$$$$ && \
	 if test -f "$@" && test -s "$@" ; then \
		if cmp -s "$@.tmp.$$$$" "$@" ; then \
			echo "  GENERATE-HEADER	$@ did not change" ; \
		else \
			echo "  GENERATE-HEADER	$@ got changed" ; \
			cp "$@.tmp.$$$$" "$@" ; \
		fi ; \
	 else \
		echo "  GENERATE-HEADER	$@ was absent"; \
		cp "$@.tmp.$$$$" "$@" ; \
	 fi \
	 || RES=$$?; \
	 rm -f "$@.tmp.$$$$" ; \
	 exit $$RES

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
endif HAVE_WINDOWS
else !HAVE_CXX11
EXTRA_DIST += nutclient.h nutclient.cpp
endif !HAVE_CXX11

if HAVE_CXX11
# libnutclientstub version information and build
libnutclientstub_la_SOURCES = nutclientmem.h nutclientmem.cpp
libnutclientstub_la_LDFLAGS = -version-info 1:1:0
libnutclientstub_la_LIBADD = libnutclient.la
if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libnutclientstub_la_LDFLAGS += -no-undefined
endif HAVE_WINDOWS
else !HAVE_CXX11
EXTRA_DIST += nutclientmem.h nutclientmem.cpp
endif !HAVE_CXX11

dummy:

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	$(AM_V_at)rm -rf $(builddir)/.deps

# Helper for only the enabled libs to get built:
all-libs-local: $(lib_LTLIBRARIES) $(noinst_LTLIBRARIES) $(EXTRA_LTLIBRARIES) \
	libupsclient-version.h
