# Network UPS Tools: tests

all: $(TESTS)

EXTRA_DIST = nut-driver-enumerator-test.sh nut-driver-enumerator-test--ups.conf

TESTS = nutlogtest

AM_CFLAGS = -I$(top_srcdir)/include
AM_CXXFLAGS = -I$(top_srcdir)/include

check_PROGRAMS = $(TESTS)

nutlogtest_SOURCES = nutlogtest.c
nutlogtest_LDADD = $(top_builddir)/common/libcommon.la

### Optional tests which can not be built everywhere
# List of src files for CppUnit tests
CPPUNITTESTSRC = example.cpp nutclienttest.cpp
# The test driver which orchestrates running those tests above
CPPUNITTESTERSRC = cpputest.cpp

if HAVE_CXX11
if HAVE_CPPUNIT
# Note: per configure script this "SHOULD" also assume
# that we HAVE_CXX11 - but better have it explicit

TESTS += cppunittest

if WITH_VALGRIND
check-local: $(check_PROGRAMS)
	RES=0; for P in $^ ; do $(VALGRIND) ./$$P || { RES=$$? ; echo "FAILED: $(VALGRIND) ./$$P" >&2; }; done; exit $$RES
endif

cppunittest_CXXFLAGS = $(AM_CXXFLAGS) $(CPPUNIT_CFLAGS) $(CPPUNIT_CXXFLAGS) $(CPPUNIT_NUT_CXXFLAGS) $(CXXFLAGS)
cppunittest_LDFLAGS = $(CPPUNIT_LIBS)
cppunittest_LDADD = $(top_builddir)/clients/libnutclient.la
cppunittest_SOURCES = $(CPPUNITTESTSRC) $(CPPUNITTESTERSRC)

else !HAVE_CPPUNIT
# Just redistribute test source into tarball

EXTRA_DIST += $(CPPUNITTESTSRC) $(CPPUNITTESTERSRC)

endif !HAVE_CPPUNIT

else !HAVE_CXX11
# Just redistribute test source into tarball

EXTRA_DIST += example.cpp cpputest.cpp

endif !HAVE_CXX11
