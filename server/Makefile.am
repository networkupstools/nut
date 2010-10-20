# Network UPS Tools: server

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/common/libcommon.la \
$(top_builddir)/common/libparseconf.la: dummy
	@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

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
LDADD = $(top_builddir)/common/libcommon.la $(top_builddir)/common/libparseconf.la
LDADD += /c/MinGW/lib/libws2_32.a
LDADD += $(NETLIBS)
if WITH_WRAP
   LDADD += $(LIBWRAP_LIBS)
endif
if WITH_SSL
   LDADD += $(LIBSSL_LIBS)
endif

sbin_PROGRAMS = upsd
EXTRA_PROGRAMS = sockdebug

upsd_SOURCES = upsd.c user.c conf.c netssl.c sstate.c desc.c		\
 netget.c netmisc.c netlist.c netuser.c netset.c netinstcmd.c		\
 conf.h ctype.h desc.h netcmds.h neterr.h netget.h netinstcmd.h		\
 netlist.h netmisc.h netset.h netuser.h ssl.h sstate.h stype.h upsd.h   \
 upstype.h user-data.h user.h

sockdebug_SOURCES = sockdebug.c

dummy:

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	rm -rf $(builddir)/.deps
