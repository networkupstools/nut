# Network UPS Tools: common

AM_CFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include
AM_CXXFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include
AM_LDFLAGS = -no-undefined
EXTRA_DIST =

noinst_LTLIBRARIES = libparseconf.la libcommon.la libcommonclient.la
if WITH_NUTCONF
# We define the recipe below in any case, but only activate it by default
# if the build configuration tells us to:
noinst_LTLIBRARIES += libnutconf.la
endif

libparseconf_la_SOURCES = parseconf.c

libnutconf_la_SOURCES = nutconf.cpp nutstream.cpp nutwriter.cpp nutipc.cpp

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@export PATH=@PATH_DURING_CONFIGURE@

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else... but do depend on its build area:
if BUILDING_IN_TREE
# No need for symlink hack
common.c: $(top_builddir)/include/nut_version.h
else
# Surprisingly, for some "make" implementations this dependency means
# that the "common.c" required for builds below will be seeked in the
# current directory. So for out-of-tree builds like distcheck, we have
# to symlink the "real" source to build area:
common.c: $(top_builddir)/include/nut_version.h $(srcdir)/common.c
	test -s "$@" || ln -s -f "$(top_srcdir)/common/common.c" "$@"
endif

$(top_builddir)/include/nut_version.h:
	@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

libcommon_la_SOURCES = state.c str.c upsconf.c
libcommonclient_la_SOURCES = state.c str.c
if BUILDING_IN_TREE
libcommon_la_SOURCES += common.c
libcommonclient_la_SOURCES += common.c
else
nodist_libcommon_la_SOURCES = common.c
nodist_libcommonclient_la_SOURCES = common.c
CLEANFILES = $(top_builddir)/common/common.c
BUILT_SOURCES = common.c
endif

if HAVE_STRPTIME
EXTRA_DIST += strptime.c
else
# fall back to NetBSD implem
libcommon_la_SOURCES += strptime.c
libcommonclient_la_SOURCES += strptime.c
endif

if HAVE_STRNLEN
EXTRA_DIST += strnlen.c
else
# fall back to FreeBSD implem
libcommon_la_SOURCES += strnlen.c
libcommonclient_la_SOURCES += strnlen.c
endif

if HAVE_STRSEP
EXTRA_DIST += strsep.c
else
# fall back to simple implem
libcommon_la_SOURCES += strsep.c
libcommonclient_la_SOURCES += strsep.c
endif

# TODO: libnutwincompat.la?
if HAVE_WINDOWS
libcommon_la_SOURCES += wincompat.c $(top_srcdir)/include/wincompat.h
libcommonclient_la_SOURCES += wincompat.c $(top_srcdir)/include/wincompat.h
endif

# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in/.ac -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@ @NETLIBS@
libcommonclient_la_LIBADD = libparseconf.la @LTLIBOBJS@ @NETLIBS@

libcommon_la_CFLAGS = $(AM_CFLAGS)
libcommonclient_la_CFLAGS = $(AM_CFLAGS)

if HAVE_LIBREGEX
  libcommon_la_CFLAGS += $(LIBREGEX_CFLAGS)
  libcommon_la_LIBADD += $(LIBREGEX_LIBS)

  libcommonclient_la_CFLAGS += $(LIBREGEX_CFLAGS)
  libcommonclient_la_LIBADD += $(LIBREGEX_LIBS)
endif

# Did the user request, and build env support, tighter integration with
# libsystemd methods such as sd_notify()?
if WITH_LIBSYSTEMD
  libcommon_la_CFLAGS += $(LIBSYSTEMD_CFLAGS)
  libcommon_la_LIBADD += $(LIBSYSTEMD_LIBS)

# A typical client should not need this,
# but just in case (and to simplify linking)...
#  libcommonclient_la_CFLAGS += $(LIBSYSTEMD_CFLAGS)
#  libcommonclient_la_LIBADD += $(LIBSYSTEMD_LIBS)
  libcommonclient_la_CFLAGS += -DWITHOUT_LIBSYSTEMD=1
endif

# several other Makefiles include these two helpers, so make them a library too
# note that LTLIBOBJS pulls in snprintf.c contents too
noinst_LTLIBRARIES += libcommonstr.la
libcommonstr_la_SOURCES = common.c str.c
libcommonstr_la_CFLAGS = $(AM_CFLAGS) -DWITHOUT_LIBSYSTEMD=1
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

if WITH_LIBXML2
  libnutdmfsnmp_la_LIBADD += $(LIBXML2_LIBS)
  libnutdmfsnmp_la_CFLAGS += $(LIBXML2_CFLAGS)
endif

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

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	$(AM_V_at)rm -rf $(builddir)/.deps
