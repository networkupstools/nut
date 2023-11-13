# Network UPS Tools: script/Windows

AM_CFLAGS = -I$(top_srcdir)/include

../include/nut_version.h: FORCE
	(cd ../include/ && $(MAKE) $(AM_MAKEFLAGS) nut_version.h)

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
