# Network UPS Tools: script/Windows

AM_CFLAGS = -I$(top_srcdir)/include

../include/nut_version.h: FORCE
	(cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h)

FORCE:

if HAVE_MINGW_RESGEN

winevent.rc winevent.h: winevent.mc
	$(WINDMC) winevent.mc

winevent.o: winevent.rc winevent.h
	$(WINDRES) winevent.rc winevent.o

winevent.dll: winevent.o
	$(DLLWRAP) --output-lib=libwinevent.a --dllname=winevent.dll --driver-name=gcc winevent.o

bin_PROGRAMS = nut halt

nut$(EXEEXT): winevent.dll
nut_SOURCES = wininit.c
nut_LDADD = ../../common/libcommon.la

halt_SOURCES = halt.c

CLEANFILES = winevent.rc winevent.o winevent.dll winevent.h

else !HAVE_MINGW_RESGEN

winevent.dll:
	@echo "Not building MingW resources since the needed programs (windmc, windres, dllwrap) were not found."

endif !HAVE_MINGW_RESGEN

