# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include

noinst_LTLIBRARIES = libparseconf.la libcommon.la libcommonclient.la
libparseconf_la_SOURCES = parseconf.c

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else
libcommon_la_SOURCES = common.c state.c str.c upsconf.c
libcommonclient_la_SOURCES = common.c state.c str.c
# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@
libcommonclient_la_LIBADD = libparseconf.la @LTLIBOBJS@

if WITH_LIBUSB_0_1
  noinst_LTLIBRARIES += libusb-compat-1.0.la
  libusb_compat_1_0_la_SOURCES = libusb-compat-1.0.c common.c str.c
  libusb_compat_1_0_la_LIBADD = $(LIBUSB_LIBS) @LTLIBOBJS@
  libusb_compat_1_0_la_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
endif
