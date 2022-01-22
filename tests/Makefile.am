# Network UPS Tools: tests

all: $(TESTS)

EXTRA_DIST = nut-driver-enumerator-test.sh nut-driver-enumerator-test--ups.conf

TESTS = nutlogtest
CLEANFILES = *.trs *.log

AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/drivers
AM_CXXFLAGS = -I$(top_srcdir)/include

check_PROGRAMS = $(TESTS)

nutlogtest_SOURCES = nutlogtest.c
nutlogtest_LDADD = $(top_builddir)/common/libcommon.la

# Separate the .deps of other dirs from this one
LINKED_SOURCE_FILES = hidparser.c

# NOTE: Not using "$<" due to a legacy Sun/illumos dmake bug with resolver
# of dynamic vars, see e.g. https://man.omnios.org/man1/make#BUGS
hidparser.c: $(top_srcdir)/drivers/hidparser.c
	test -s "$@" || ln -s -f "$(top_srcdir)/drivers/hidparser.c" "$@"

if WITH_USB
TESTS += getvaluetest

getvaluetest_SOURCES = getvaluetest.c
nodist_getvaluetest_SOURCES = hidparser.c
# Pull the right include path for chosen libusb version:
getvaluetest_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
getvaluetest_LDADD = $(top_builddir)/common/libcommon.la
endif

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/common/libcommon.la: dummy
	@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

### Optional tests which can not be built everywhere
# List of src files for CppUnit tests
CPPUNITTESTSRC = example.cpp nutclienttest.cpp
# The test driver which orchestrates running those tests above
CPPUNITTESTERSRC = cpputest.cpp

TESTS_CXX11 = cppunittest

if HAVE_CXX11
if HAVE_CPPUNIT
# Note: per configure script this "SHOULD" also assume
# that we HAVE_CXX11 - but better have it explicit

TESTS += $(TESTS_CXX11)

if WITH_VALGRIND
check-local: $(check_PROGRAMS)
	RES=0; for P in $^ ; do $(VALGRIND) ./$$P || { RES=$$? ; echo "FAILED: $(VALGRIND) ./$$P" >&2; }; done; exit $$RES
endif

cppunittest_CXXFLAGS = $(AM_CXXFLAGS) $(CPPUNIT_CFLAGS) $(CPPUNIT_CXXFLAGS) $(CPPUNIT_NUT_CXXFLAGS) $(CXXFLAGS)
cppunittest_LDFLAGS = $(CPPUNIT_LIBS)
cppunittest_LDADD = $(top_builddir)/clients/libnutclient.la $(top_builddir)/clients/libnutclientstub.la
cppunittest_SOURCES = $(CPPUNITTESTSRC) $(CPPUNITTESTERSRC)

# Make sure out-of-dir C++ dependencies exist (especially when dev-building
# only some parts of NUT):
$(top_builddir)/clients/libnutclient.la \
$(top_builddir)/clients/libnutclientstub.la: dummy
	@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

else !HAVE_CPPUNIT
# Just redistribute test source into tarball

EXTRA_DIST += $(CPPUNITTESTSRC) $(CPPUNITTESTERSRC)

endif !HAVE_CPPUNIT

else !HAVE_CXX11
# Just redistribute test source into tarball

EXTRA_DIST += example.cpp cpputest.cpp

endif !HAVE_CXX11

dummy:

BUILT_SOURCES = $(LINKED_SOURCE_FILES)
CLEANFILES += $(LINKED_SOURCE_FILES)
CLEANFILES += $(TESTS) $(TESTS_CXX11)
MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	rm -rf $(builddir)/.deps
