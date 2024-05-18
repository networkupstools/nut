# Network UPS Tools: NUT configuration tool

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

all: $(bin_PROGRAMS)

bin_PROGRAMS =

# Currently nutconf and related codebase causes woes for static analysis
# so we do not build it unless explicitly asked to.
if WITH_NUTCONF
bin_PROGRAMS += nutconf
nutconf_SOURCES = nutconf-cli.cpp
nutconf_CXXFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include
nutconf_LDADD = $(top_builddir)/common/libcommon.la \
                $(top_builddir)/common/libnutconf.la

# Add support for device scanning
if WITH_LIBLTDL
nutconf_CXXFLAGS += -DWITH_NUTSCANNER -I$(top_srcdir)/tools/nut-scanner
nutconf_LDADD += $(top_builddir)/tools/nut-scanner/libnutscan.la
endif WITH_LIBLTDL

endif WITH_NUTCONF

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/common/libcommon.la \
$(top_builddir)/common/libnutconf.la \
$(top_builddir)/tools/nut-scanner/libnutscan.la: dummy
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

dummy:

MAINTAINERCLEANFILES = Makefile.in .dirstamp
