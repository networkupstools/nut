# Network UPS Tools: clients
EXTRA_DIST =

# nutclient.cpp for some legacy reason (maybe initial detached development?)
# optionally includes "common.h" with the NUT build setup - and this option
# was never triggered in fact, not until pushed through command line like this:
AM_CXXFLAGS = -DHAVE_NUTCOMMON=1

# by default, link programs in this directory with libcommon.a
LDADD = ../common/libcommon.la libupsclient.la $(NETLIBS)
if WITH_SSL
  LDADD += $(LIBSSL_LIBS)
endif

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
lib_LTLIBRARIES = libupsclient.la
if HAVE_CXX11
  lib_LTLIBRARIES += libnutclient.la
endif
if WITH_DEV
 include_HEADERS = upsclient.h ../include/parseconf.h nutclient.h
endif
if WITH_CGI
 cgiexec_PROGRAMS = upsstats.cgi upsimage.cgi upsset.cgi
endif

upsc_SOURCES = upsc.c upsclient.h
upscmd_SOURCES = upscmd.c upsclient.h
upsrw_SOURCES = upsrw.c upsclient.h
upslog_SOURCES = upslog.c upsclient.h upslog.h
upsmon_SOURCES = upsmon.c upsmon.h upsclient.h

upssched_SOURCES = upssched.c upssched.h
upssched_LDADD = ../common/libcommon.la ../common/libparseconf.la $(NETLIBS)

upsimage_cgi_SOURCES = upsimage.c upsclient.h upsimagearg.h cgilib.c cgilib.h
upsimage_cgi_LDADD = $(LDADD) $(LIBGD_LDFLAGS)

upsset_cgi_SOURCES = upsset.c upsclient.h cgilib.c cgilib.h
upsstats_cgi_SOURCES = upsstats.c upsclient.h status.h upsstats.h	\
 upsimagearg.h cgilib.c cgilib.h

# not LDADD.
libupsclient_la_SOURCES = upsclient.c upsclient.h
libupsclient_la_LIBADD = ../common/libcommonclient.la
if WITH_SSL
  libupsclient_la_LIBADD += $(LIBSSL_LIBS)
endif

# Below we set API versions of public libraries
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# Note that changes here may have to be reflected in packaging (the shared
# object .so names would differ)

# libupsclient version information
libupsclient_la_LDFLAGS = -version-info 5:0:0 -export-symbols-regex ^upscli_

if HAVE_CXX11
# libnutclient version information and build
libnutclient_la_SOURCES = nutclient.h nutclient.cpp
libnutclient_la_LDFLAGS = -version-info 1:0:0
else
EXTRA_DIST += nutclient.h nutclient.cpp
endif
