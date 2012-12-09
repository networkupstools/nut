# Network UPS Tools: script/Windows

AM_CFLAGS = -I$(top_srcdir)/include

../include/nut_version.h: FORCE
	(cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h)

EXTRA_DIST = winevent.mc build-mingw-nut.sh README

FORCE:

if HAVE_MINGW_RESGEN

winevent.rc winevent.h: winevent.mc
	$(WINDMC) $<

winevent.o: winevent.rc winevent.h
	$(WINDRES) winevent.rc winevent.o

wininit.$(OBJEXT): winevent.h

bin_PROGRAMS = nut halt

nut_SOURCES = wininit.c
nut_LDADD = ../../common/libcommon.la winevent.o

halt_SOURCES = halt.c

CLEANFILES = winevent.rc winevent.o winevent.h

endif HAVE_MINGW_RESGEN

