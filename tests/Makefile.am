# Network UPS Tools: tests

if HAVE_CPPUNIT

TESTS = cppunittest

check_PROGRAMS = $(TESTS)

cppunittest_CXXFLAGS = $(CPPUNIT_CFLAGS) -I$(top_srcdir)/include
cppunittest_LDFLAGS  = $(CPPUNIT_LIBS)

# List of src files for CppUnit tests
CPPUNITTESTSRC = example.cpp nut_clock_ut.cpp

cppunittest_SOURCES = $(CPPUNITTESTSRC) cpputest.cpp
cppunittest_LDADD   = ../common/libcommon.la

else !HAVE_CPPUNIT

EXTRA_DIST = example.cpp cpputest.cpp nut_clock_up.cpp

endif !HAVE_CPPUNIT
