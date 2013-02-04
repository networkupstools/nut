# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include
AM_CXXFLAGS = -I$(top_srcdir)/include

noinst_LTLIBRARIES = libparseconf.la libcommon.la libnutconf.la
libparseconf_la_SOURCES = parseconf.c

libnutconf_la_SOURCES = nutconf.cpp nutstream.cpp nutwriter.cpp nutipc.cpp

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else
libcommon_la_SOURCES = common.c state.c upsconf.c 
# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@
