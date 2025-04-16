# Network UPS Tools: common

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

AM_CFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include
AM_CXXFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include
AM_LDFLAGS = -no-undefined
EXTRA_DIST =
CLEANFILES =

noinst_LTLIBRARIES = libparseconf.la libcommon.la libcommonclient.la
lib_LTLIBRARIES =

# We define the recipe below in any case, but only activate it by default
# if the build configuration tells us to:
if WITH_DEV_LIBNUTCONF
# We want it built and delivered (standalone or with tool)
  lib_LTLIBRARIES += libnutconf.la
else !WITH_DEV_LIBNUTCONF
if WITH_NUTCONF
# We only want the tool, make a private build
  noinst_LTLIBRARIES += libnutconf.la
# else do not build at all, e.g. do not have C++ support
endif WITH_NUTCONF
endif !WITH_DEV_LIBNUTCONF

libparseconf_la_SOURCES = parseconf.c

# do not hard depend on '../include/nut_version.h', since it blocks
# 'dist', and is only required for actual build, in which case
# BUILT_SOURCES (in ../include) will ensure nut_version.h will
# be built before anything else... but do depend on its build area:
if BUILDING_IN_TREE
# No need for symlink hack
  common.c: $(top_builddir)/include/nut_version.h
else !BUILDING_IN_TREE
# Surprisingly, for some "make" implementations this dependency means
# that the "common.c" required for builds below will be seeked in the
# current directory. So for out-of-tree builds like distcheck, we have
# to symlink the "real" source to build area. And then when we handle
# subsequent dependencies, we already have a filename that "make" now
# discovers and is confused about:
  common.c: $(top_builddir)/include/nut_version.h $(srcdir)/common.c
	@if [ x"$(abs_top_srcdir)" = x"$(abs_top_builddir)" ] || test -s "$@" ; then \
		exit 0 ; \
	 else \
		echo "  LN         $(top_srcdir)/common/common.c => $@ (relative to `pwd`)" ; \
		ln -s -f "$(top_srcdir)/common/common.c" "$@" ; \
	 fi
endif !BUILDING_IN_TREE

$(top_builddir)/include/nut_version.h:
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

# FIXME: If we maintain some of those helper libs as subsets of the others
# (strictly), maybe build the lowest common denominator only and link the
# bigger scopes with it (rinse and repeat)?
libcommon_la_SOURCES = state.c str.c upsconf.c
libcommonclient_la_SOURCES = state.c str.c

# several other Makefiles include the two helpers common.c str.c (and
# perhaps some other string-related code), so make them a library too;
# note that LTLIBOBJS pulls in snprintf.c contents too.
noinst_LTLIBRARIES += libcommonstr.la
libcommonstr_la_SOURCES = str.c
libcommonstr_la_CFLAGS = $(AM_CFLAGS) -DWITHOUT_LIBSYSTEMD=1
libcommonstr_la_LIBADD = @LTLIBOBJS@ @BSDKVMPROCLIBS@

if BUILDING_IN_TREE
  libcommon_la_SOURCES += common.c
  libcommonstr_la_SOURCES += common.c
  libcommonclient_la_SOURCES += common.c
else !BUILDING_IN_TREE
  nodist_libcommon_la_SOURCES = common.c
  nodist_libcommonstr_la_SOURCES = common.c
  nodist_libcommonclient_la_SOURCES = common.c
  CLEANFILES += $(top_builddir)/common/common.c
  BUILT_SOURCES = common.c
endif !BUILDING_IN_TREE

if HAVE_STRPTIME
  EXTRA_DIST += strptime.c
else !HAVE_STRPTIME
  # fall back to NetBSD implem
  libcommon_la_SOURCES += strptime.c
  libcommonstr_la_SOURCES += strptime.c
  libcommonclient_la_SOURCES += strptime.c
endif !HAVE_STRPTIME

if HAVE_STRNLEN
  EXTRA_DIST += strnlen.c
else !HAVE_STRNLEN
  # fall back to FreeBSD implem
  libcommon_la_SOURCES += strnlen.c
  libcommonstr_la_SOURCES += strnlen.c
  libcommonclient_la_SOURCES += strnlen.c
endif !HAVE_STRNLEN

if HAVE_STRSEP
  EXTRA_DIST += strsep.c
else !HAVE_STRSEP
  # fall back to simple implem
  libcommon_la_SOURCES += strsep.c
  libcommonstr_la_SOURCES += strsep.c
  libcommonclient_la_SOURCES += strsep.c
endif !HAVE_STRSEP

if WANT_TIMEGM_FALLBACK
  # fall back to simple implem
  libcommon_la_SOURCES += timegm_fallback.c
  libcommonstr_la_SOURCES += timegm_fallback.c
  libcommonclient_la_SOURCES += timegm_fallback.c
else !WANT_TIMEGM_FALLBACK
  EXTRA_DIST += timegm_fallback.c
endif !WANT_TIMEGM_FALLBACK

if HAVE_WINDOWS
  libnutwincompat_la_SOURCES = wincompat.c $(top_srcdir)/include/wincompat.h
  libnutwincompat_la_LDFLAGS =
  libnutwincompat_la_LIBADD =
  # Assume setenv() provided by OS or nut_setenv() provided by
  # another NUT library and linked to the final NUT program/lib
  # (anyhow, avoid a link-time conflict with two definitions):
  libnutwincompat_la_CFLAGS = $(AM_CFLAGS) -DHAVE_SETENV=1
  noinst_LTLIBRARIES += libnutwincompat.la

  libcommon_la_SOURCES += wincompat.c $(top_srcdir)/include/wincompat.h
  libcommonclient_la_SOURCES += wincompat.c $(top_srcdir)/include/wincompat.h
endif HAVE_WINDOWS

# ensure inclusion of local implementation of missing systems functions
# using LTLIBOBJS. Refer to configure.in/.ac -> AC_REPLACE_FUNCS
libcommon_la_LIBADD = libparseconf.la @LTLIBOBJS@ @NETLIBS@ @BSDKVMPROCLIBS@
libcommonclient_la_LIBADD = libparseconf.la @LTLIBOBJS@ @NETLIBS@ @BSDKVMPROCLIBS@

libcommon_la_CFLAGS = $(AM_CFLAGS)
libcommonclient_la_CFLAGS = $(AM_CFLAGS)

if WITH_LIBNUTCONF
if WITH_DEV_LIBNUTCONF
  # libnutconf version information and build
  # currently considered a highly experimental and so unstable API at least
  # (at least, headers contain a lot of data/code, not sure they should)
  libnutconf_la_LDFLAGS = -version-info 0:1:0
if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libnutconf_la_LDFLAGS += -no-undefined
endif HAVE_WINDOWS
endif WITH_DEV_LIBNUTCONF
  libnutconf_la_CXXFLAGS = $(AM_CXXFLAGS)
  # NOTE: No @LTLIBOBJS@ here, because libcommonclient.la includes them (if any)
  libnutconf_la_LIBADD = @NETLIBS@ libcommonclient.la
  libnutconf_la_SOURCES = nutconf.cpp nutstream.cpp nutwriter.cpp nutipc.cpp \
	../include/nutconf.hpp ../include/nutipc.hpp \
	../include/nutstream.hpp ../include/nutwriter.hpp
else !WITH_LIBNUTCONF
  EXTRA_DIST += nutconf.cpp nutstream.cpp nutwriter.cpp nutipc.cpp
endif !WITH_LIBNUTCONF

if HAVE_LIBREGEX
  libcommon_la_CFLAGS += $(LIBREGEX_CFLAGS)
  libcommon_la_LIBADD += $(LIBREGEX_LIBS)

  libcommonstr_la_CFLAGS += $(LIBREGEX_CFLAGS)
  libcommonstr_la_LIBADD += $(LIBREGEX_LIBS)

  libcommonclient_la_CFLAGS += $(LIBREGEX_CFLAGS)
  libcommonclient_la_LIBADD += $(LIBREGEX_LIBS)
endif HAVE_LIBREGEX

# Did the user request, and build env support, tighter integration with
# libsystemd methods such as sd_notify()?
if WITH_LIBSYSTEMD
  libcommon_la_CFLAGS += $(LIBSYSTEMD_CFLAGS)
  libcommon_la_LIBADD += $(LIBSYSTEMD_LIBS)

# A typical client should not need this,
# but just in case (and to simplify linking)...
#  libcommonclient_la_CFLAGS += $(LIBSYSTEMD_CFLAGS)
#  libcommonclient_la_LIBADD += $(LIBSYSTEMD_LIBS)
  libcommonclient_la_CFLAGS += -DWITHOUT_LIBSYSTEMD=1
endif WITH_LIBSYSTEMD

MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
clean-local:
	if test -L $(builddir)/common.c || test -h $(builddir)/common.c ; then rm -f $(builddir)/common.c ; fi
#	$(AM_V_at)rm -rf $(builddir)/.deps

# Helper for only the enabled libs to get built:
all-libs-local: $(lib_LTLIBRARIES) $(noinst_LTLIBRARIES) $(EXTRA_LTLIBRARIES)
