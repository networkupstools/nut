# Network UPS Tools: lib
EXTRA_DIST = README.adoc

if WITH_DEV
if WITH_PKG_CONFIG
 pkgconfig_DATA = libupsclient.pc libnutscan.pc libnutclient.pc libnutclientstub.pc
else
 bin_SCRIPTS = libupsclient-config
endif
endif

CLEANFILES = *-spellchecked README
MAINTAINERCLEANFILES = Makefile.in .dirstamp
