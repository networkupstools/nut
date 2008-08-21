# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c state.c upsconf.c ../include/nut_version.h
libcommon_a_LIBADD = libparseconf.la

noinst_LTLIBRARIES = libparseconf.la
libparseconf_la_SOURCES =  parseconf.c
