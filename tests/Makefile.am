# Network UPS Tools: tests

# Tests with gcc-4.8 require this C++11 option to be provided explicitly
# gcc-4.6 does not support this yet; newer gcc's should be ok by default
CPPUNIT_NUT_CFLAGS = -g -O0 -std=c++11

if HAVE_CPPUNIT

TESTS = cppunittest

check_PROGRAMS = $(TESTS)

if WITH_VALGRIND
check-local: $(check_PROGRAMS)
	RES=0; for P in $^ ; do $(VALGRIND) ./$$P || { RES=$$? ; echo "FAILED: $(VALGRIND) ./$$P" >&2; }; done; exit $$RES
endif

cppunittest_CXXFLAGS = $(CPPUNIT_CFLAGS) $(CPPUNIT_NUT_CFLAGS)
cppunittest_LDFLAGS = $(CPPUNIT_LIBS)
cppunittest_LDADD = ../clients/libnutclient.la

# List of src files for CppUnit tests
CPPUNITTESTSRC = example.cpp nutclienttest.cpp

cppunittest_SOURCES = $(CPPUNITTESTSRC) cpputest.cpp

else !HAVE_CPPUNIT

EXTRA_DIST = example.cpp cpputest.cpp

endif !HAVE_CPPUNIT
