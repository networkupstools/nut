# Network UPS Tools: clients

# by default, link programs in this directory with libcommon.a
LDADD = ../common/libcommon.a libupsclient.la $(NETLIBS) $(LIBSSL_LDFLAGS)
AM_CFLAGS = $(LIBSSL_CFLAGS) -I$(top_srcdir)/include

bin_PROGRAMS = upsc upslog upsrw upscmd
dist_bin_SCRIPTS = upssched-cmd
sbin_PROGRAMS = upsmon upssched
if WITH_LIB
 lib_LTLIBRARIES = libupsclient.la
 include_HEADERS = upsclient.h ../include/parseconf.h
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
upssched_CFLAGS = -I$(top_srcdir)/include
upssched_LDADD = ../common/libcommon.a $(NETLIBS) ../common/parseconf.o 

upsimage_cgi_SOURCES = upsimage.c upsclient.h upsimagearg.h cgilib.c cgilib.h
upsimage_cgi_CFLAGS = $(AM_CFLAGS) $(LIBGD_CFLAGS)
upsimage_cgi_LDADD = $(LDADD) $(LIBGD_LDFLAGS)

upsset_cgi_SOURCES = upsset.c upsclient.h cgilib.c cgilib.h
upsstats_cgi_SOURCES = upsstats.c upsclient.h status.h upsstats.h	\
 upsimagearg.h cgilib.c cgilib.h

# not LDADD.
libupsclient_la_SOURCES = upsclient.c upsclient.h
libupsclient_la_LIBADD = ../common/parseconf.lo
libupsclient_la_LDFLAGS = -version-info 0:0:0

# rules for cross-directory targets
../common/libcommon.a:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) libcommon.a)

../common/parseconf.o:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) parseconf.o)

../common/parseconf.lo:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) parseconf.lo)
