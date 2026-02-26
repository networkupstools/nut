# Network UPS Tools: lib

# NOTE: Here we deliver metadata for third-party development to integrate
# their programs with NUT libraries. Actual libraries (code and recipes) are
# built in other directories, such as common, clients and tools/nut-scanner.

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

# Note: do not need to EXTRA_DIST the *.in files since they are listed
# in configure.ac for template conversion and automake knows about them.
EXTRA_DIST = README.adoc

if WITH_DEV
if WITH_PKG_CONFIG
 pkgconfig_DATA = libupsclient.pc libnutscan.pc libnutclient.pc libnutclientstub.pc
if WITH_DEV_LIBNUTCONF
 pkgconfig_DATA += libnutconf.pc
endif WITH_DEV_LIBNUTCONF
else !WITH_PKG_CONFIG
 bin_SCRIPTS = libupsclient-config
endif !WITH_PKG_CONFIG
endif WITH_DEV

CLEANFILES = *-spellchecked README
MAINTAINERCLEANFILES = Makefile.in .dirstamp
