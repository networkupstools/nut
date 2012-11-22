# Network UPS Tools: tests

if HAVE_CPPUNIT

TESTS = cppunittest

check_PROGRAMS = $(TESTS)

cppunittest_CXXFLAGS = $(CPPUNIT_CFLAGS) -I$(top_srcdir)/include
cppunittest_LDFLAGS = $(CPPUNIT_LIBS)
cppunittest_LDADD = ../common/libnutconf.la

# List of src files for CppUnit tests
CPPUNITTESTSRC = example.cpp nutconf.cpp nutstream_ut.cpp

cppunittest_SOURCES = $(CPPUNITTESTSRC) cpputest.cpp

else !HAVE_CPPUNIT

EXTRA_DIST = example.cpp nutconf.cpp cpputest.cpp nutstream_ut.cpp

endif !HAVE_CPPUNIT
