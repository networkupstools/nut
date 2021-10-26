# Network UPS Tools: server

# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include
if WITH_WRAP
  AM_CFLAGS += $(LIBWRAP_CFLAGS)
endif
if WITH_SSL
  AM_CFLAGS += $(LIBSSL_CFLAGS)
endif
if WITH_DBUS
  AM_CFLAGS += $(LIBDBUS_CFLAGS)
endif
LDADD = ../common/libcommon.la ../common/libparseconf.la $(NETLIBS)
if WITH_WRAP
   LDADD += $(LIBWRAP_LIBS)
endif
if WITH_SSL
   LDADD += $(LIBSSL_LIBS)
endif
if WITH_DBUS
  LDADD += $(LIBDBUS_LIBS)
endif

sbin_PROGRAMS = upsd
EXTRA_PROGRAMS = sockdebug

upsd_SOURCES = upsd.c user.c conf.c netssl.c sstate.c desc.c 		\
 netget.c netmisc.c netlist.c netuser.c netset.c netinstcmd.c dbus.c	\
 conf.h nut_ctype.h desc.h netcmds.h neterr.h netget.h netinstcmd.h		\
 netlist.h netmisc.h netset.h netuser.h netssl.h sstate.h stype.h upsd.h   \
 upstype.h user-data.h user.h dbus.h

sockdebug_SOURCES = sockdebug.c
