# Network UPS Tools: common

AM_CFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -no-undefined

noinst_LTLIBRARIES = libparseconf.la libcommon.la libcommonclient.la
libparseconf_la_SOURCES = parseconf.c

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else... but do depend on its build area:
if BUILDING_IN_TREE
# No need for symlink hack
common.c: $(top_builddir)/include/nut_version.h
else
# Surprisingly, for some "make" implementations this dependency means
# that the "common.c" required for builds below will be seeked in the
# current directory. So for out-of-tree builds like distcheck, we have
# to symlink the "real" source to build area:
common.c: $(top_builddir)/include/nut_version.h $(srcdir)/common.c
	test -s "$@" || ln -s -f "$(top_srcdir)/common/common.c" "$@"
endif

$(top_builddir)/include/nut_version.h:
	@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

libcommon_la_SOURCES = state.c str.c upsconf.c
libcommonclient_la_SOURCES = state.c str.c
if BUILDING_IN_TREE
libcommon_la_SOURCES += common.c
libcommonclient_la_SOURCES += common.c
else
nodist_libcommon_la_SOURCES = common.c
nodist_libcommonclient_la_SOURCES = common.c
CLEANFILES = $(top_builddir)/common/common.c
BUILT_SOURCES = common.c
endif

# TODO: libnutwincompat.la?
libcommon_la_SOURCES += wincompat.c $(top_srcdir)/include/wincompat.h
libcommonclient_la_SOURCES += wincompat.c $(top_srcdir)/include/wincompat.h

# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in/.ac -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@ @NETLIBS@
libcommonclient_la_LIBADD = libparseconf.la @LTLIBOBJS@ @NETLIBS@

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	rm -rf $(builddir)/.deps
