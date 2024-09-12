# Network UPS Tools: scripts/Windows

AM_CFLAGS = -I$(top_srcdir)/include

../include/nut_version.h: FORCE
	+cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h

EXTRA_DIST = \
	winevent.mc				\
	build-mingw-nut.sh			\
	build-mingw-prereqs.sh			\
	dllldd.sh				\
	README.adoc				\
	DriverInstaller/wdi-simple.c		\
	DriverInstaller/README.adoc		\
	Installer/README.adoc			\
	Installer/BuildInstaller.bat		\
	Installer/NUT-Installer.xml.in		\
	Installer/ImageFiles/icons/completi.ico	\
	Installer/ImageFiles/icons/info.ico	\
	Installer/ImageFiles/icons/custicon.ico	\
	Installer/ImageFiles/icons/Up.ico	\
	Installer/ImageFiles/icons/exclamic.ico	\
	Installer/ImageFiles/icons/powernut_Stop.ico	\
	Installer/ImageFiles/icons/repairic.ico	\
	Installer/ImageFiles/icons/New.ico	\
	Installer/ImageFiles/icons/removico.ico	\
	Installer/ImageFiles/icons/powernut_logo.ico	\
	Installer/ImageFiles/icons/insticon.ico	\
	Installer/ImageFiles/Images/NUT_wix_vertical.bmp	\
	Installer/ImageFiles/Images/NUT_wix_horizontal.bmp	\
	Installer/ImageFiles/Others/StopService.bat	\
	Installer/ImageFiles/Others/StartService.bat	\
	Installer/ImageFiles/emptyDir/man1/temp.txt	\
	Installer/ImageFiles/emptyDir/cgi-bin/temp.txt	\
	Installer/ImageFiles/emptyDir/pkgconfig/temp.txt	\
	Installer/ImageFiles/emptyDir/include/temp.txt	\
	Installer/ImageFiles/emptyDir/html/temp.txt	\
	Installer/ImageFiles/emptyDir/man3/temp.txt	\
	Installer/ImageFiles/emptyDir/run/temp.txt

CLEANFILES = *-spellchecked */*-spellchecked

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

CLEANFILES += winevent.rc winevent.o winevent.h

endif HAVE_MINGW_RESGEN

SPELLCHECK_SRC = README.adoc DriverInstaller/README.adoc Installer/README.adoc

# NOTE: Due to portability, we do not use a GNU percent-wildcard extension.
# We also have to export some variables that may be tainted by relative
# paths when parsing the other makefile (e.g. MKDIR_P that may be defined
# via expanded $(top_builddir)/install-sh):
#%-spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

# NOTE: Portable suffix rules do not allow prerequisites, so we shim them here
# by a wildcard target in case the make implementation can put the two together.
*-spellchecked: Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)

.sample.sample-spellchecked:
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

.in.in-spellchecked:
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict:
	+$(MAKE) -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC="$(SPELLCHECK_SRC)" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

MAINTAINERCLEANFILES = Makefile.in .dirstamp
