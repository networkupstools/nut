# Network UPS Tools: tests

EXTRA_DIST = nut-driver-enumerator-test.sh nut-driver-enumerator-test--ups.conf

if HAVE_CXX11
if HAVE_CPPUNIT
# Note: per configure script this "SHOULD" also assume
# that we HAVE_CXX11 - but better have it explicit

TESTS = cppunittest

check_PROGRAMS = $(TESTS)

if WITH_VALGRIND
check-local: $(check_PROGRAMS)
	RES=0; for P in $^ ; do $(VALGRIND) ./$$P || { RES=$$? ; echo "FAILED: $(VALGRIND) ./$$P" >&2; }; done; exit $$RES
endif

cppunittest_CXXFLAGS = $(CPPUNIT_CFLAGS) $(CPPUNIT_CXXFLAGS) $(CPPUNIT_NUT_CXXFLAGS) $(CXXFLAGS)
cppunittest_LDFLAGS = $(CPPUNIT_LIBS)
cppunittest_LDADD = ../clients/libnutclient.la

# List of src files for CppUnit tests
CPPUNITTESTSRC = example.cpp nutclienttest.cpp

cppunittest_SOURCES = $(CPPUNITTESTSRC) cpputest.cpp

else !HAVE_CPPUNIT

EXTRA_DIST += example.cpp cpputest.cpp

endif !HAVE_CPPUNIT

else !HAVE_CXX11

EXTRA_DIST += example.cpp cpputest.cpp

endif !HAVE_CXX11
