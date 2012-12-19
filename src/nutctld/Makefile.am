# Network UPS Tools: nutctld

BUILT_SOURCES = nutctl_adaptor.hpp nutctl_proxy.hpp

CLEANFILES = nutctl_adaptor.hpp nutctl_proxy.hpp

# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/tools/nut-scanner

LDADD = ../../common/libcommon.la ../../common/libnutconf.la  \
	../../tools/nut-scanner/libnutscan.la $(NETLIBS)

# TODO: move these to configure
AM_CXXFLAGS = $(AM_CFLAGS) `pkg-config --cflags dbus-c++-1`
AM_LDFLAGS = `pkg-config --libs dbus-c++-1`

sbin_PROGRAMS = nutctld

nutctld_SOURCES = nutctld.cpp \
	control.hpp control.cpp \
	nutctl_adaptor.hpp


lib_LTLIBRARIES = libnutctl.la

libnutctl_la_SOURCES = libnutctl.cpp libnutctl.hpp \
	nutctl_proxy.hpp

nutctl_adaptor.hpp: nutctl.xml
	dbusxx-xml2cpp nutctl.xml --adaptor=nutctl_adaptor.hpp

nutctl_proxy.hpp: nutctl.xml
	dbusxx-xml2cpp nutctl.xml --proxy=nutctl_proxy.hpp


bin_PROGRAMS = nutctlclient

nutctlclient_SOURCES = client.cpp
nutctlclient_LDADD = libnutctl.la

