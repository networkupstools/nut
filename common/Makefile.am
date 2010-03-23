# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include

../include/nut_version.h: FORCE
	(cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h)

FORCE:

noinst_LTLIBRARIES = libparseconf.la libcommon.la
libparseconf_la_SOURCES =  parseconf.c

libcommon_la_SOURCES = common.c state.c upsconf.c ../include/nut_version.h
# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@
