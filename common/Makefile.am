# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c
libcommon_a_LIBADD = $(LIBOBJS) $(ALLOCA)

all: libcommon.a upsconf.o parseconf.o state.o parseconf.lo

# Define a dummy library so that Automake builds rules for the
# corresponding object files.  This library is not actually built,
EXTRA_LIBRARIES = libdummy.a
libdummy_a_SOURCES = parseconf.c state.c upsconf.c
