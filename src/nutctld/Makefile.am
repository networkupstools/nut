# Network UPS Tools: nutctld

# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include

LDADD = ../../common/libcommon.la ../../common/libnutconf.la $(NETLIBS)

sbin_PROGRAMS = nutctld

nutctld_SOURCES = nutctld.cpp

