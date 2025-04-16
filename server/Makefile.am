# Network UPS Tools: server

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/common/libcommon.la \
$(top_builddir)/common/libparseconf.la: dummy
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include
LDADD = $(top_builddir)/common/libcommon.la $(top_builddir)/common/libparseconf.la $(NETLIBS)

sbin_PROGRAMS = upsd
EXTRA_PROGRAMS = sockdebug

upsd_SOURCES = upsd.c user.c conf.c netssl.c sstate.c desc.c		\
 netget.c netmisc.c netlist.c netuser.c netset.c netinstcmd.c		\
 conf.h nut_ctype.h desc.h netcmds.h neterr.h netget.h netinstcmd.h		\
 netlist.h netmisc.h netset.h netuser.h netssl.h sstate.h stype.h upsd.h   \
 upstype.h user-data.h user.h
upsd_CFLAGS = $(AM_CFLAGS)
upsd_LDADD = $(LDADD)
upsd_LDFLAGS = $(AM_LDFLAGS)

if WITH_WRAP
  upsd_CFLAGS += $(LIBWRAP_CFLAGS)
  upsd_LDADD += $(LIBWRAP_LIBS)
endif WITH_WRAP
if WITH_SSL
  upsd_CFLAGS += $(LIBSSL_CFLAGS)
  upsd_LDADD += $(LIBSSL_LIBS)
  upsd_LDFLAGS += $(LIBSSL_LDFLAGS_RPATH)
endif WITH_SSL

# Developer, troubleshooting, or odd automation aid tool:
if HAVE_WINDOWS
  sockdebug_SOURCES = pipedebug.c
else !HAVE_WINDOWS
  sockdebug_SOURCES = sockdebug.c
endif !HAVE_WINDOWS

if WITH_DEV
# Have it installed properly
libexec_PROGRAMS = sockdebug
endif WITH_DEV

dummy:

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
clean-local:
	$(AM_V_at)rm -rf $(EXTRA_PROGRAMS)
#	$(AM_V_at)rm -rf $(builddir)/.deps
