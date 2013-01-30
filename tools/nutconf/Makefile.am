noinst_PROGRAMS = nutconf
nutconf_SOURCES = nutconf.cpp
nutconf_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/tools/nut-scanner
#nutconf_LDFLAGS = -L$(top_srcdir)/tools/nut-scanner
nutconf_LDADD = $(top_srcdir)/common/libcommon.la  \
                $(top_srcdir)/common/libnutconf.la \
                $(top_srcdir)/tools/nut-scanner/libnutscan.la
