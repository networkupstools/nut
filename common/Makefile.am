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

if WITH_NEON
if WITH_SNMP
# Naming may be clumsy, but the current intention is that DMF technique
# can allow dynamic loading of other protocols and mappings - the SNMP
# application is just the first of many possible. Some code from current
# dmfsnmp.c/.h is shareable and may become a separate set of sources/headers
# sometime later, but the libraries for consumers to bind to are named
# properly already.
  noinst_LTLIBRARIES += libnutdmfsnmp.la
  libnutdmfsnmp_la_SOURCES = dmfsnmp.c
  libnutdmfsnmp_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/drivers \
        -I$(top_srcdir)/tools/nut-scanner \
        $(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) -DWITH_DMFMIB
if HAVE_DMF_LUA
libnutdmfsnmp_la_CFLAGS += -DWITH_DMF_LUA=1 $(LUA_INCLUDE)
endif

# Note: Consumers of this lib will generally want these linked in:
#  libnutdmfsnmp_consumer_LDADD = $(LIBNETSNMP_LIBS) $(LIBNEON_LIBS) $(LUA_LIB) libcommon.la
endif
endif
