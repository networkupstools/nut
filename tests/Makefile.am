# Network UPS Tools: tests

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

SUBDIRS = . NIT

# Note: a few duplicate mentions are possible here, but a decent make should
# handle that properly
all: $(TESTS) $(check_PROGRAMS) $(check_SCRIPTS)

EXTRA_DIST = nut-driver-enumerator-test.sh nut-driver-enumerator-test--ups.conf

TESTS =
noinst_LTLIBRARIES =
CLEANFILES = *.trs *.log

AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/drivers
AM_CXXFLAGS = -I$(top_srcdir)/include

# Compiler flags for cppunit tests
CPPUNIT_NUT_CXXFLAGS = @CPPUNIT_NUT_CXXFLAGS@

check_PROGRAMS = $(TESTS)
check_SCRIPTS =

# NUT Integration Testing suite
check-NIT check-NIT-devel:
	+cd "$(builddir)/NIT" && $(MAKE) $(AM_MAKEFLAGS) $@

nutlogtest_SOURCES = nutlogtest.c
nutlogtest_LDADD = $(top_builddir)/common/libcommon.la

if REQUIRE_NUT_STRARG
check_SCRIPTS += nutlogtest-nofail.sh
CLEANFILES += nutlogtest-nofail.sh nutlogtest$(EXEEXT) nutlogtest

nutlogtest-nofail.sh: nutlogtest$(EXEEXT)
	@echo '#!/bin/sh' > $@
	@echo 'echo "WARNING: Your C library requires workarounds to print NULL values!" >&2' >> $@
	@echo 'echo "If nutlogtest below, or generally some NUT program, crashes with" >&2' >> $@
	@echo 'echo "a segmentation fault (especially during verbose debug) - that may be why" >&2' >> $@
	@echo 'SCRIPT_DIR="`dirname "$$0"`"' >> $@
	@echo '"$${SCRIPT_DIR}/nutlogtest" "$$@" || echo "nutlogtest FAILED but it was expected"' >> $@
	@chmod +x $@

# NOTE: Keep the line above empty!
else !REQUIRE_NUT_STRARG
TESTS += nutlogtest$(EXEEXT)
endif !REQUIRE_NUT_STRARG

TESTS += nuttimetest
nuttimetest_SOURCES = nuttimetest.c
nuttimetest_LDADD = $(top_builddir)/common/libcommon.la

TESTS += nutbooltest
nutbooltest_SOURCES = nutbooltest.c
#nutbooltest_LDADD = $(top_builddir)/common/libcommon.la

# Separate the .deps of other dirs from this one
LINKED_SOURCE_FILES = hidparser.c

# NOTE: Not using "$<" due to a legacy Sun/illumos dmake bug with resolver
# of dynamic vars, see e.g. https://man.omnios.org/man1/make#BUGS
hidparser.c: $(top_srcdir)/drivers/hidparser.c
	test -s "$@" || ln -s -f "$(top_srcdir)/drivers/hidparser.c" "$@"

if WITH_USB
TESTS += getvaluetest getexponenttest-belkin-hid

# We only need to call a few methods, not use the whole source - so
# not linking it as a getvaluetest_SOURCE file (has too many deps):
noinst_LTLIBRARIES += libdriverstubusb.la
nodist_libdriverstubusb_la_SOURCES = driver-stub-usb.c
libdriverstubusb_la_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
#libdriverstubusb_la_LIBADD = $(top_builddir)/common/libcommon.la

getexponenttest-belkin-hid.c: $(top_srcdir)/drivers/belkin-hid.c
getexponenttest_belkin_hid_SOURCES = getexponenttest-belkin-hid.c
getexponenttest_belkin_hid_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
getexponenttest_belkin_hid_LDADD = $(top_builddir)/common/libcommon.la libdriverstubusb.la

getvaluetest_SOURCES = getvaluetest.c
nodist_getvaluetest_SOURCES = hidparser.c
# Pull the right include path for chosen libusb version:
getvaluetest_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
getvaluetest_LDADD = $(top_builddir)/common/libcommon.la
else !WITH_USB
EXTRA_DIST += getvaluetest.c hidparser.c
endif !WITH_USB
EXTRA_DIST += driver-stub-usb.c

if WITH_GPIO
TESTS += gpiotest

# NOTE: Not using "$<" due to a legacy Sun/illumos dmake bug with resolver
# of dynamic vars, see e.g. https://man.omnios.org/man1/make#BUGS
generic_gpio_libgpiod.c: $(top_srcdir)/drivers/generic_gpio_libgpiod.c
	test -s "$@" || ln -s -f "$(top_srcdir)/drivers/generic_gpio_libgpiod.c" "$@"

generic_gpio_common.c: $(top_srcdir)/drivers/generic_gpio_common.c
	test -s "$@" || ln -s -f "$(top_srcdir)/drivers/generic_gpio_common.c" "$@"

gpiotest_SOURCES = generic_gpio_utest.c generic_gpio_liblocal.c
nodist_gpiotest_SOURCES = generic_gpio_libgpiod.c generic_gpio_common.c
gpiotest_LDADD = $(top_builddir)/drivers/libdummy_mockdrv.la
gpiotest_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/tests -DDRIVERS_MAIN_WITHOUT_MAIN=1
else !WITH_GPIO
EXTRA_DIST += generic_gpio_utest.c generic_gpio_liblocal.c
endif !WITH_GPIO

CLEANFILES += generic_gpio_libgpiod.c generic_gpio_common.c
EXTRA_DIST += generic_gpio_utest.h generic_gpio_test.txt

TESTS += driver_methods_utest
driver_methods_utest_SOURCES = driver_methods_utest.c
driver_methods_utest_LDADD = $(top_builddir)/drivers/libdummy_mockdrv.la
driver_methods_utest_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/tests -DDRIVERS_MAIN_WITHOUT_MAIN=1

# Make sure out-of-dir dependencies exist (especially when dev-building parts):
$(top_builddir)/drivers/libdummy_mockdrv.la \
$(top_builddir)/common/libnutconf.la \
$(top_builddir)/common/libcommonclient.la \
$(top_builddir)/common/libcommon.la: dummy
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

### Optional tests which can not be built everywhere
# List of src files for CppUnit tests
CPPUNITTESTSRC = example.cpp nutclienttest.cpp
# These are an optional part of cppunittest, if building WITH_LIBNUTCONF
CPPUNITTESTSRC_NUTCONF = nutconf_parser_ut.cpp nutstream_ut.cpp nutconf_ut.cpp nutipc_ut.cpp
# The test driver which orchestrates running those tests above
CPPUNITTESTERSRC = cpputest.cpp

CPPCLIENTTESTSRC = cpputest-client.cpp

TESTS_CXX11 = cppunittest

if HAVE_CXX11
if HAVE_CPPUNIT
# Note: per configure script this "SHOULD" also assume
# that we HAVE_CXX11 - but better have it explicit

TESTS += $(TESTS_CXX11)

# Note: we only build it, but do not run directly (NIT prepares the sandbox)
check_PROGRAMS += cppnit

cppunittest_CXXFLAGS = $(AM_CXXFLAGS) $(CPPUNIT_CFLAGS) $(CPPUNIT_CXXFLAGS) $(CPPUNIT_NUT_CXXFLAGS) $(CXXFLAGS)
###cppunittest_CXXFLAGS += -I$(top_srcdir)/include -DTOP_SRCDIR="\"$(top_srcdir)\""
cppunittest_LDFLAGS = $(CPPUNIT_LDFLAGS) $(CPPUNIT_LIBS)
cppunittest_LDADD  = $(top_builddir)/clients/libnutclient.la
cppunittest_LDADD += $(top_builddir)/clients/libnutclientstub.la
cppunittest_SOURCES = $(CPPUNITTESTSRC) $(CPPUNITTESTERSRC)

# Currently nutconf and related codebase causes woes for static analysis
# so we do not build it unless explicitly asked to.
if WITH_LIBNUTCONF
cppunittest_SOURCES += $(CPPUNITTESTSRC_NUTCONF)
cppunittest_LDADD += $(top_builddir)/common/libnutconf.la
endif WITH_LIBNUTCONF

cppnit_CXXFLAGS = $(AM_CXXFLAGS) $(CPPUNIT_CFLAGS) $(CPPUNIT_CXXFLAGS) $(CPPUNIT_NUT_CXXFLAGS) $(CXXFLAGS)
cppnit_LDFLAGS = $(CPPUNIT_LDFLAGS) $(CPPUNIT_LIBS)
cppnit_LDADD = $(top_builddir)/clients/libnutclient.la $(top_builddir)/clients/libnutclientstub.la
cppnit_SOURCES = $(CPPCLIENTTESTSRC) $(CPPUNITTESTERSRC)

# Make sure out-of-dir C++ dependencies exist (especially when dev-building
# only some parts of NUT):
$(top_builddir)/clients/libnutclient.la \
$(top_builddir)/clients/libnutclientstub.la: dummy
	+@cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

else !HAVE_CPPUNIT
# Just redistribute test source into tarball if not building tests

EXTRA_DIST += $(CPPUNITTESTSRC) $(CPPCLIENTTESTSRC) $(CPPUNITTESTERSRC) $(CPPUNITTESTSRC_NUTCONF)

cppnit:
	@echo "SKIP: $@ not implemented without C++11 and CPPUNIT enabled" >&2 ; exit 1

endif !HAVE_CPPUNIT

else !HAVE_CXX11
# Just redistribute test source into tarball if not building C++ at all

EXTRA_DIST += $(CPPUNITTESTSRC) $(CPPCLIENTTESTSRC) $(CPPUNITTESTERSRC) $(CPPUNITTESTSRC_NUTCONF)

cppnit:
	@echo "SKIP: $@ not implemented without C++11 and CPPUNIT enabled" >&2 ; exit 1

endif !HAVE_CXX11

if HAVE_VALGRIND
# NOTE: "cppnit", if built, requires running from NIT (with NUT_PORT, etc.)
# Note that FAILED value begins with a space, so we do not echo another
memcheck: $(check_PROGRAMS)
	@RES=0; FAILED=""; \
	 for P in $? ; do \
		case "$$P" in \
			cppnit|cppnit$(EXEEXT)) \
				if [ "$${NUT_PORT-}" -gt 0 ] 2>/dev/null ; then : ; else \
					echo "SKIPPED: $(VALGRIND) ./$$P : NUT_PORT not prepared" ; \
					continue ; \
				fi ;; \
		esac ; \
		'$(top_builddir)/scripts/valgrind/valgrind.sh' "./$$P" \
		|| { RES=$$? ; \
			echo "FAILED: '$(top_builddir)/scripts/valgrind/valgrind.sh' './$$P'" >&2; \
			FAILED="$$FAILED $$P" ; \
		} ; \
	 done; \
	 if [ -n "$$FAILED" ] ; then echo "OVERALL FAILED:$$FAILED" >&2 ; fi ; \
	 exit $$RES
else !HAVE_VALGRIND
memcheck:
	@echo "SKIPPED $@ : valgrind was not detected on this system by configure script" >&2
endif !HAVE_VALGRIND

if WITH_VALGRIND
check-local: memcheck
endif WITH_VALGRIND

dummy:

BUILT_SOURCES = $(LINKED_SOURCE_FILES)
CLEANFILES += $(LINKED_SOURCE_FILES)
CLEANFILES += $(TESTS) $(TESTS_CXX11)
MAINTAINERCLEANFILES = Makefile.in .dirstamp

# NOTE: Do not clean ".deps" in SUBDIRS of the main project,
# the root Makefile.am takes care of that!
#clean-local:
#	$(AM_V_at)rm -rf $(builddir)/.deps

# Helper for only the enabled libs to get built:
all-libs-local: $(lib_LTLIBRARIES) $(noinst_LTLIBRARIES) $(EXTRA_LTLIBRARIES)
