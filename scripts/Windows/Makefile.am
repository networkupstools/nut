# Network UPS Tools: script/Windows

AM_CFLAGS = -I$(top_srcdir)/include

../include/nut_version.h: FORCE
	(cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h)

EXTRA_DIST = winevent.mc build-mingw-nut.sh README.adoc

bin_PROGRAMS =

FORCE:

if HAVE_MINGW_RESGEN

# Avoid "Using $< in a non-suffix rule context is a GNUmake idiom" by $?
winevent.rc winevent.h: winevent.mc
	$(WINDMC) $?

winevent.o: winevent.rc winevent.h
	$(WINDRES) winevent.rc winevent.o

wininit.$(OBJEXT): winevent.h

# Some binutils packages (e.g. on Slackware 15) include windres and windmc
# tools, even though they do not deliver the rest of files such as windows.h
# that would be needed for actual builds targeting Windows.
if HAVE_WINDOWS
bin_PROGRAMS += nut halt

nut_SOURCES = wininit.c
nut_LDADD = ../../common/libcommon.la winevent.o

halt_SOURCES = halt.c
endif HAVE_WINDOWS

CLEANFILES = winevent.rc winevent.o winevent.h

endif HAVE_MINGW_RESGEN

MAINTAINERCLEANFILES = Makefile.in .dirstamp
