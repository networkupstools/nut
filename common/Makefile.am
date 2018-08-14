# Network UPS Tools: common

AM_CFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include

noinst_LTLIBRARIES = libparseconf.la libcommon.la libcommonclient.la
libparseconf_la_SOURCES = parseconf.c

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else
libcommon_la_SOURCES = common.c state.c str.c upsconf.c
libcommonclient_la_SOURCES = common.c state.c str.c
# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in/.ac -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@
libcommonclient_la_LIBADD = libparseconf.la @LTLIBOBJS@

# several other Makefiles include these two helpers, so make them a library too
# note that LTLIBOBJS pulls in snprintf.c contents too
noinst_LTLIBRARIES += libcommonstr.la
libcommonstr_la_SOURCES = common.c str.c
libcommonstr_la_LIBADD = @LTLIBOBJS@

if WITH_NEON
if WITH_SNMP
if WITH_DMF
# TODO: split the engine and its consumer codebases, and use WITH_DMFMIB
# specifically for SNMP and WITH_DMF for general DMF codebase.
# Naming may be clumsy, but the current intention is that DMF technique
# can allow dynamic loading of other protocols and mappings - the SNMP
# application is just the first of many possible. Some code from current
# dmfsnmp.c/.h is shareable and may become a separate set of sources/headers
# sometime later, but the libraries for consumers to bind to are named
# properly already.
  noinst_LTLIBRARIES += libnutdmfsnmp.la
  libnutdmfsnmp_la_SOURCES = dmfsnmp.c dmfcore.c alist.c
  libnutdmfsnmp_la_CFLAGS = $(AM_CFLAGS) -I$(top_builddir)/drivers -I$(top_srcdir)/drivers \
        -I$(top_builddir)/tools/nut-scanner -I$(top_srcdir)/tools/nut-scanner \
        $(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) -DWITH_DMFMIB=1 -DWITH_SNMP=1 -DWITH_NEON=1
  libnutdmfsnmp_la_LDFLAGS =
# Note: no LTLIBOBJS here, so we do not conflict when linked along with
# other libraries we build (typically consumed with libcommonstr.la)
  libnutdmfsnmp_la_LIBADD = $(LIBNETSNMP_LIBS)

if WITH_LIBLTDL
# NOTE: the C macro WITH_LIBLTDL=1 will probably be defined by config.h
# and unlike the DMF macros, we do not care much about this case.
# We do care about it not defined when not enabled (so we define =0 below).
  libnutdmfsnmp_la_LIBADD += $(LIBLTDL_LIBS)
  libnutdmfsnmp_la_CFLAGS += -DWITH_LIBLTDL=1 $(LIBLTDL_CFLAGS)
else
  libnutdmfsnmp_la_LIBADD += $(LIBNEON_LIBS)
  libnutdmfsnmp_la_CFLAGS += -DWITH_LIBLTDL=0
endif

# Note: absence of LUA does not require to set -DWITH_DMF_FUNCTIONS=0
if WITH_DMF_LUA
  LUA_INCLUDE ?= -I/usr/include/lua5.1
  libnutdmfsnmp_la_CFLAGS += -DWITH_DMF_LUA=1 -DWITH_DMF_FUNCTIONS=1 $(LUA_INCLUDE)
  libnutdmfsnmp_la_LIBADD += $(LUA_LIB)
else
  libnutdmfsnmp_la_CFLAGS += -DWITH_DMF_LUA=0
endif

endif

# Note: Consumers of this lib will generally want the dependencies linked via:
#  libnutdmfsnmp_consumer_LDADD += $(abs_top_builddir)/common/libnutdmfsnmp.la
#  libnutdmfsnmp_consumer_LDADD += $(abs_top_builddir)/common/libcommonstr.la
#  libnutdmfsnmp_consumer_CFLAGS += $(AM_CFLAGS) -I$(top_srcdir)/drivers \
#     -I$(top_srcdir)/tools/nut-scanner $(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) \
#      -DWITH_DMFMIB=1 -DWITH_SNMP=1 -DWITH_NEON=1
#if WITH_DMF_LUA
#  libnutdmfsnmp_consumer_CFLAGS += -DWITH_DMF_LUA=1 -DWITH_DMF_FUNCTIONS=1 $(LUA_INCLUDE)
#endif
endif
endif
