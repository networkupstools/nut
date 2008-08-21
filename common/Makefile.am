# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c state.c upsconf.c ../include/nut_version.h
libcommon_a_LIBADD = libparseconf.la

../include/nut_version.h: FORCE
	(cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h)

FORCE:

noinst_LTLIBRARIES = libparseconf.la
libparseconf_la_SOURCES =  parseconf.c
