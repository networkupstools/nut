# Network UPS Tools: script/Windows

AM_CFLAGS = -I$(top_srcdir)/include

../include/nut_version.h: FORCE
	(cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h)

FORCE:

LDADD = ../../common/libcommon.la

bin_PROGRAMS = nut halt

nut_SOURCES = wininit.c
halt_SOURCES = halt.c
