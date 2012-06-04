# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -no-undefined

noinst_LTLIBRARIES = libparseconf.la libcommon.la
libparseconf_la_SOURCES = parseconf.c

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else
libcommon_la_SOURCES = common.c state.c upsconf.c wincompat.c
# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@ @NETLIBS@
