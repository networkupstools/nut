# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include

noinst_LTLIBRARIES = libparseconf.la libcommon.la libcommonclient.la
libparseconf_la_SOURCES = parseconf.c

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else
common.c: $(top_builddir)/include/nut_version.h

$(top_builddir)/include/nut_version.h:
	@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

libcommon_la_SOURCES = common.c state.c str.c upsconf.c
libcommonclient_la_SOURCES = common.c state.c str.c
# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in/.ac -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@
libcommonclient_la_LIBADD = libparseconf.la @LTLIBOBJS@

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	rm -rf $(builddir)/.deps
