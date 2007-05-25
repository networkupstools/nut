# Network UPS Tools: common

SVNREV = $(shell svnversion -n $(topdir) 2>/dev/null)

# Should be touched each time the SVN revision number goes up 
revision-stamp: FORCE
	if [ x"$(SVNREV)" != x`cat revision-stamp 2>/dev/null` ]; then echo -n $(SVNREV) >revision-stamp; fi

FORCE:

AM_CFLAGS = -I$(top_srcdir)/include "-DSVNREV=\"$(SVNREV)\""

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c upsversion.c
libcommon_a_LIBADD = $(LIBOBJS) $(ALLOCA)

upsversion.o: revision-stamp 

all: libcommon.a upsconf.o parseconf.o state.o parseconf.lo

# Define a dummy library so that Automake builds rules for the
# corresponding object files.  This library is not actually built,
EXTRA_LIBRARIES = libdummy.a
libdummy_a_SOURCES = parseconf.c state.c upsconf.c



