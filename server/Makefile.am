# Network UPS Tools: server

# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include
if WITH_SSL
  AM_CFLAGS += $(LIBSSL_CFLAGS)
endif
LDADD = ../common/libcommon.a ../common/state.o ../common/upsconf.o	\
 ../common/parseconf.o $(NETLIBS)
if WITH_SSL
   LDADD += $(LIBSSL_LDFLAGS)
endif

sbin_PROGRAMS = upsd
EXTRA_PROGRAMS = sockdebug

upsd_SOURCES = upsd.c user.c conf.c ssl.c sstate.c access.c desc.c	\
 netget.c netmisc.c netlist.c netuser.c netset.c netinstcmd.c access.h	\
 conf.h ctype.h desc.h netcmds.h neterr.h netget.h netinstcmd.h		\
 netlist.h netmisc.h netset.h netuser.h ssl.h sstate.h stype.h upsd.h   \
 upstype.h user-data.h user.h

sockdebug_SOURCES = sockdebug.c 

# rules for cross-directory targets
../common/libcommon.a: FORCE
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) libcommon.a)

../common/upsconf.o:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) upsconf.o)

../common/parseconf.o:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) parseconf.o)

../common/state.o:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) state.o)

FORCE:
