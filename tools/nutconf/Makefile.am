noinst_PROGRAMS = nutconf
nutconf_SOURCES = nutconf.cpp
nutconf_CXXFLAGS = -I$(top_srcdir)/include
nutconf_LDADD = ../../common/libcommon.la  \
                ../../common/libnutconf.la

# Add support for device scanning
if WITH_LIBLTDL
nutconf_CXXFLAGS += -DWITH_NUTSCANNER -I$(top_srcdir)/tools/nut-scanner
nutconf_LDADD += ../nut-scanner/libnutscan.la
endif
