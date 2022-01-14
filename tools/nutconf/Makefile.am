all: $(bin_PROGRAMS)

bin_PROGRAMS = nutconf
nutconf_SOURCES = nutconf.cpp
nutconf_CXXFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include
nutconf_LDADD = $(top_builddir)/common/libcommon.la \
                $(top_builddir)/common/libnutconf.la

# Add support for device scanning
if WITH_LIBLTDL
nutconf_CXXFLAGS += -DWITH_NUTSCANNER -I$(top_srcdir)/tools/nut-scanner
nutconf_LDADD += $(top_builddir)/tools/nut-scanner/libnutscan.la
endif

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/common/libcommon.la \
$(top_builddir)/common/libnutconf.la \
$(top_builddir)/tools/nut-scanner/libnutscan.la: dummy
	@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

dummy:

MAINTAINERCLEANFILES = Makefile.in .dirstamp
