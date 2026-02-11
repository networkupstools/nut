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

# Track unique NUT implementation-detail libraries that are commonly
# used in different programs, and may get bundled into a shared lib.
# Note that for rebuild-time optimization with static libraries, we
# have historically used a number of source-file selections which may
# overlap (e.g. libcommon includes libcommonclient and some more).
noinst_LTLIBRARIES = libparseconf.la libcommon.la libcommonclient.la
lib_LTLIBRARIES =

if ENABLE_SHARED_PRIVATE_LIBS
### Hopefully we can bundle this code together and
### install this somewhat away from the public eye?..
### We still do build static (noinst) libs to include
### them into shared library files that should remain
### compatible for existing consumers (in/out of tree).
  lib_LTLIBRARIES += libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-all.la
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_all_la_SOURCES =
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_all_la_LIBADD = libcommon.la libcommonversion-private.la

  lib_LTLIBRARIES += libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-client.la
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_client_la_SOURCES =
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_client_la_LIBADD = libcommonclient.la libcommonversion-private.la

### Avoid unexpected linking across NUT generations
### far apart (e.g. custom rebuilds of new programs
### tucked into an existing deployment); see libtool
### docs about current:revision:age interface numbering.
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_all_la_LDFLAGS = -version-info 1:0:0
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_client_la_LDFLAGS = -version-info 1:0:0

if HAVE_WINDOWS
  # Many versions of MingW seem to fail to build non-static DLL without this
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_all_la_LDFLAGS += -no-undefined
  libnutprivate_@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@_common_client_la_LDFLAGS += -no-undefined
endif HAVE_WINDOWS
endif ENABLE_SHARED_PRIVATE_LIBS

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
# No need for symlink hack, just rebuild if header gets updated:
  common-nut_version.c: $(top_builddir)/include/nut_version.h
else !BUILDING_IN_TREE
# Surprisingly, for some "make" implementations this dependency means that
# the "common-nut_version.c" required for builds below will be seeked in the
# current directory. So for out-of-tree builds like distcheck, we have
# to symlink the "real" source to build area. And then when we handle
# subsequent dependencies, we already have a filename that "make" now
# discovers and is confused about:
  common-nut_version.c: $(top_builddir)/include/nut_version.h $(srcdir)/common-nut_version.c
	@if [ x"$(abs_top_srcdir)" = x"$(abs_top_builddir)" ] || test -s "$@" ; then \
		exit 0 ; \
	 else \
		echo "  LN	$(top_srcdir)/common/common-nut_version.c => $@ (relative to `pwd`)" ; \
		ln -s -f "$(top_srcdir)/common/common-nut_version.c" "$@" ; \
	 fi

  CLEANFILES += $(top_builddir)/common/nut_version.c
  BUILT_SOURCES = common-nut_version.c
endif !BUILDING_IN_TREE

$(top_builddir)/include/nut_version.h: @dotMAKE@
	+@if [ -s '$@' ] && ( [ x"$(NUT_VERSION_H_GENERATED)" = xtrue ] || [ x"$${NUT_VERSION_H_GENERATED}" = xtrue ] ) ; then \
	    if [ x"$(MAINTAINER_GENERATE_HEADER_DEBUG)" = xyes ] ; then \
	        echo "=== SKIP (common) $@ (NUT_VERSION_H_GENERATED makevar=$(NUT_VERSION_H_GENERATED) shellvar=$${NUT_VERSION_H_GENERATED})" >&2; \
	    fi; \
	    exit 0 ; \
	  fi ; \
	  cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

# FIXME: If we maintain some of those helper libs as subsets of the others
# (strictly), maybe build the lowest common denominator only and link the
# bigger scopes with it (rinse and repeat)?
libcommon_la_SOURCES = state.c str.c upsconf.c
libcommonclient_la_SOURCES = state.c str.c

# several other Makefiles include the three helpers common.c common-nut_version.c str.c
# (and perhaps some other string-related code), so we make them a library too;
# note that LTLIBOBJS pulls in snprintf.c contents too.
noinst_LTLIBRARIES += libcommonstr.la
libcommonstr_la_SOURCES = str.c
libcommonstr_la_CFLAGS = $(AM_CFLAGS) -DWITHOUT_LIBSYSTEMD=1
libcommonstr_la_LIBADD = @LTLIBOBJS@ @BSDKVMPROCLIBS@

# Common methods for JSON outputs from NUT programs
noinst_LTLIBRARIES += libcommonstrjson.la
libcommonstrjson_la_SOURCES = strjson.c

# This one includes only version-related data and methods and should be
# linked along with one of the other libcommon*.la for logging methods:
noinst_LTLIBRARIES += libcommonversion.la
libcommonversion_la_SOURCES = common-nut_version.c
#libcommonversion_la_CFLAGS = $(AM_CFLAGS) -DWITHOUT_LIBSYSTEMD=1
#libcommonversion_la_LIBADD = @LTLIBOBJS@ @BSDKVMPROCLIBS@

if ENABLE_SHARED_PRIVATE_LIBS
# A special build of the above for linking into shared private libs,
# so they also know about the NUT version they were built from
  noinst_LTLIBRARIES += libcommonversion-private.la
  libcommonversion_private_la_SOURCES = common-nut_version.c
  libcommonversion_private_la_CFLAGS = $(AM_CFLAGS) -DBUILD_FOR_SHARED_PRIVATE_LIBS=1
endif

# Eventually we expect more sources as the big common.c source gets split
# into smaller better-focused files.
COMMON_SRC = \
    common.c

if BUILDING_IN_TREE
  libcommon_la_SOURCES += $(COMMON_SRC)
  libcommonstr_la_SOURCES += $(COMMON_SRC)
  libcommonclient_la_SOURCES += $(COMMON_SRC)
else !BUILDING_IN_TREE
  nodist_libcommon_la_SOURCES = $(COMMON_SRC)
  nodist_libcommonstr_la_SOURCES = $(COMMON_SRC)
  nodist_libcommonclient_la_SOURCES = $(COMMON_SRC)
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
  libnutconf_la_LIBADD = @NETLIBS@
if ENABLE_SHARED_PRIVATE_LIBS
  libnutconf_la_LIBADD += libnutprivate-@NUT_SOURCE_GITREV_SEMVER_UNDERSCORES@-common-client.la
else !ENABLE_SHARED_PRIVATE_LIBS
  libnutconf_la_LIBADD += libcommonclient.la
endif !ENABLE_SHARED_PRIVATE_LIBS
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

if WITH_NEON
if WITH_SNMP
if WITH_DMF
# TODO: split the engine and its consumer codebases, and use WITH_DMFMIB
# specifically for SNMP and WITH_DMF for general DMF codebase.
# Naming may be clumsy, but the current intention is that DMF technique
# can allow dynamic loading of other protocols and mappings - the SNMP
# application is just the first of many possible. Some code from current
# dmfsnmp.c/.h is shareable and may become a separate set of sources/headers
# sometime later, but the libraries for consumers to bind to are named
# properly already.
  noinst_LTLIBRARIES += libnutdmfsnmp.la
  libnutdmfsnmp_la_SOURCES = dmfsnmp.c dmfcore.c alist.c
  libnutdmfsnmp_la_CFLAGS = $(AM_CFLAGS) -I$(top_builddir)/drivers -I$(top_srcdir)/drivers \
        -I$(top_builddir)/tools/nut-scanner -I$(top_srcdir)/tools/nut-scanner \
        $(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) -DWITH_DMFMIB=1 -DWITH_SNMP=1 -DWITH_NEON=1
  libnutdmfsnmp_la_LDFLAGS =
# Note: no LTLIBOBJS here, so we do not conflict when linked along with
# other libraries we build (typically consumed with libcommonstr.la)
  libnutdmfsnmp_la_LIBADD = $(LIBNETSNMP_LIBS)

if WITH_LIBLTDL
# NOTE: the C macro WITH_LIBLTDL=1 will probably be defined by config.h
# and unlike the DMF macros, we do not care much about this case.
# We do care about it not defined when not enabled (so we define =0 below).
  libnutdmfsnmp_la_LIBADD += $(LIBLTDL_LIBS)
  libnutdmfsnmp_la_CFLAGS += -DWITH_LIBLTDL=1 $(LIBLTDL_CFLAGS)
else !WITH_LIBLTDL
  libnutdmfsnmp_la_LIBADD += $(LIBNEON_LIBS)
  libnutdmfsnmp_la_CFLAGS += -DWITH_LIBLTDL=0
endif !WITH_LIBLTDL

# Note: absence of LUA does not require to set -DWITH_DMF_FUNCTIONS=0
if WITH_DMF_LUA
  libnutdmfsnmp_la_CFLAGS += -DWITH_DMF_LUA=1 -DWITH_DMF_FUNCTIONS=1 $(LUA_INCLUDE)
  libnutdmfsnmp_la_LIBADD += $(LUA_LIB)
else !WITH_DMF_LUA
  libnutdmfsnmp_la_CFLAGS += -DWITH_DMF_LUA=0
endif !WITH_DMF_LUA

# Mention libxml2 last, its search paths seem to upset builds on FreeBSD
# at least (due to entanglement of SO cross-references).
if WITH_LIBXML2
  libnutdmfsnmp_la_LIBADD += $(LIBXML2_LIBS)
  libnutdmfsnmp_la_CFLAGS += $(LIBXML2_CFLAGS)
endif WITH_LIBXML2

else !WITH_DMF
  EXTRA_DIST += dmfsnmp.c dmfcore.c alist.c
endif !WITH_DMF

# Note: Consumers of this lib will generally want the dependencies linked via:
#  libnutdmfsnmp_consumer_LDADD += $(abs_top_builddir)/common/libnutdmfsnmp.la
#  libnutdmfsnmp_consumer_LDADD += $(abs_top_builddir)/common/libcommonstr.la
#  libnutdmfsnmp_consumer_CFLAGS += $(AM_CFLAGS) -I$(top_srcdir)/drivers \
#     -I$(top_srcdir)/tools/nut-scanner $(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) \
#      -DWITH_DMFMIB=1 -DWITH_SNMP=1 -DWITH_NEON=1
#if WITH_DMF_LUA
#  libnutdmfsnmp_consumer_CFLAGS += -DWITH_DMF_LUA=1 -DWITH_DMF_FUNCTIONS=1 $(LUA_INCLUDE)
#endif WITH_DMF_LUA
else !WITH_SNMP
  EXTRA_DIST += dmfsnmp.c dmfcore.c alist.c
endif !WITH_SNMP
else !WITH_NEON
  EXTRA_DIST += dmfsnmp.c dmfcore.c alist.c
endif !WITH_NEON

MAINTAINERCLEANFILES = Makefile.in .dirstamp
# We only build it optionally, but if it lingers after a broken build -
# do clean it always anyway!
CLEANFILES += libnutdmfsnmp.la

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
clean-local:
	if test -L $(builddir)/common-nut_version.c \
	|| test -h $(builddir)/common-nut_version.c \
	; then rm -f $(builddir)/common-nut_version.c ; fi
#	$(AM_V_at)rm -rf $(builddir)/.deps

# Helper for only the enabled libs to get built:
all-libs-local: $(lib_LTLIBRARIES) $(noinst_LTLIBRARIES) $(EXTRA_LTLIBRARIES)
