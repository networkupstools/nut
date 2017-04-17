# top-level Makefile for NUT

# include directory for aclocal
ACLOCAL_AMFLAGS = -I m4

# subdirectories to build and distribute. The order matters, as
# several subdirectories depend on stuff in "common" or tools being built first
SUBDIRS = include common clients conf data tools docs drivers \
  lib scripts server tests

# COPYING is included automatically.
EXTRA_DIST = INSTALL.nut LICENSE-GPL2 LICENSE-GPL3 MAINTAINERS UPGRADING

# ----------------------------------------------------------------------
# flags to pass to ./configure when calling "make distcheck" and "make
# distcheck-light". Try to check as many features as possible! Also
# need to give augeas-lenses-dir, hotplug-dir and udev-dir, so that
# staged install does not fail.

DISTCHECK_FLAGS = --with-all --with-ssl --with-doc=auto
DISTCHECK_LIGHT_FLAGS = --with-all=auto --with-ssl=auto --with-doc=auto
DISTCHECK_VALGRIND_FLAGS = --with-all=auto --with-ssl=auto --with-doc=skip --with-valgrind CXXFLAGS='$(CXXFLAGS) -g' CFLAGS='$(CFLAGS) -g'

DISTCHECK_CONFIGURE_FLAGS = ${DISTCHECK_FLAGS}		\
 --with-systemdsystemunitdir='$${prefix}/lib/systemd/system' \
 --with-augeas-lenses-dir='$${prefix}/usr/share/augeas/lenses'		\
 --with-hotplug-dir='$${prefix}/etc/hotplug'		\
 --with-udev-dir='$${prefix}/etc/udev'			\
 --with-devd-dir='$${prefix}/etc/devd'

distcheck-light:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_LIGHT_FLAGS)" distcheck

# Make a distcheck (and check in particular) with enabled valgrind and debug info
memcheck distcheck-valgrind:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_VALGRIND_FLAGS)" distcheck

# workaround the dist generated files that are also part of the distribution
# Note that distcleancheck is disabled for now, while waiting for a proper
# solution, that do not break older unix systems
#distcleancheck_listfiles = \
#	find . -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'
distcleancheck:
	@:

# Hook the documentation building and validating recipes
# Note: these are optionally available (as determined during configure runs)
spellcheck spellcheck-interactive:
	@RES=0; \
	(cd $(srcdir)/docs && $(MAKE) $@) || RES=$$? ; \
	(cd $(srcdir)/docs/man && $(MAKE) $@) || RES=$$? ; \
	(cd $(srcdir)/conf && $(MAKE) $@) || RES=$$? ; \
	(cd $(srcdir)/data && $(MAKE) $@) || RES=$$? ; \
	exit $$RES

doc spellcheck-sortdict:
	cd $(srcdir)/docs && $(MAKE) $@

# ----------------------------------------------------------------------
# Automatically generate the ChangeLog from Git logs:
MAINTAINERCLEAN_FILES = ChangeLog

# Older boundary of the ChangeLog commits range
# It can be a tag ('v2.2.0'), a commit hash, a date, ...
# See gitrevisions for more information on specifying ranges
GITLOG_START_POINT=v2.6.0

# Force ChangeLog regeneration upon make dist (due to nonexistant 'dummy-stamp'),
# in case it has already been generated previously
dummy-stamp:
ChangeLog: tools/gitlog2changelog.py dummy-stamp
	$(top_srcdir)/tools/gitlog2changelog.py $(GITLOG_START_POINT) || \
		echo "gitlog2changelog.py failed to generate the ChangeLog. See https://github.com/networkupstools/nut/commits/master" > $@

# ----------------------------------------------------------------------
# Maintainers targets: distribution signature and hashes
dist-sig:
	rm -f nut-@PACKAGE_VERSION@.tar.gz.sig
	gpg --detach-sign nut-@PACKAGE_VERSION@.tar.gz

dist-hash:
	md5sum nut-@PACKAGE_VERSION@.tar.gz > nut-@PACKAGE_VERSION@.tar.gz.md5
	sha256sum nut-@PACKAGE_VERSION@.tar.gz > nut-@PACKAGE_VERSION@.tar.gz.sha256

# ----------------------------------------------------------------------
# targets from old build system (pre-automake).
# supported for a period of time for backward "compatibility".

WARN="----------------------------------------------------------------------"

build:
	@echo $(WARN)
	@echo "Warning: 'make build' is deprecated. Use 'make all' instead."
	@echo $(WARN)
	$(MAKE) $(AM_MAKEFLAGS) all
install-bin:
	@echo $(WARN)
	@echo "Warning: 'make install-bin' is deprecated." 
	@echo "Use 'make install-exec' instead for a similar effect."
	@echo $(WARN)
	cd common;  $(MAKE) $(AM_MAKEFLAGS) install
	cd drivers; $(MAKE) $(AM_MAKEFLAGS) install
	cd server;  $(MAKE) $(AM_MAKEFLAGS) install
	cd clients; $(MAKE) $(AM_MAKEFLAGS) install
install-man: install-data-recursive
	@echo $(WARN)
	@echo "Warning: 'make install-man' is deprecated."
	@echo "Use 'cd man; make install' instead."
	@echo $(WARN)
	cd man; $(MAKE) $(AM_MAKEFLAGS) install
install-conf:
	@echo $(WARN)
	@echo "Warning: 'make install-conf' is deprecated."
	@echo "Use 'cd conf; make install' instead."
	@echo $(WARN)
	cd conf; $(MAKE) $(AM_MAKEFLAGS) install
# The target install-data already has a standardized meaning under automake
install-dirs:
	@echo $(WARN)
	@echo "Warning: 'make install-dirs' is deprecated."
	@echo "Use 'make installdirs' instead."
	@echo $(WARN)
	make installdirs
cgi build-cgi install-cgi install-cgi-dir install-cgi-bin \
install-cgi-man install-cgi-conf install-cgi-html: 
	@echo "Error: 'make $@' no longer exists."
	@echo "Use './configure --with-cgi' instead."
install-lib:
	@echo "Error: 'make $@' no longer exists."
	@echo "Use './configure --with-dev' instead."
usb build-usb install-usb:
	@echo "Error: 'make $@' no longer exists."
	@echo "Use './configure --with-usb' instead."
snmp build-snmp install-snmp install-snmp-mgr install-snmp-man: 
	@echo "Error: 'make $@' no longer exists."
	@echo "Use './configure --with-snmp' instead."
setver:
	@echo "Error: 'make setver' no longer exists."
	@echo "Edit configure.in to set version number."
package:
	if test `uname -s` = "HP-UX"; then \
		cd scripts/HP-UX; \
		make package; \
		mv NUT_HPUX_package.depot NUT_HPUX_package@PACKAGE_VERSION@.depot; \
	elif test `uname -s` = "SunOS"; then \
		make; \
		rm -rf @prefix@; \
		make install; \
		cd scripts/Solaris; \
		make package; \
		make uninstall; \
		rm -rf @prefix@; \
	elif test `uname -s` = "AIX"; then \
		make dist; \
		cp scripts/Aix/nut-aix.spec /usr/src/packages/SPECS; \
		cp scripts/Aix/nut.init nut-@PACKAGE_VERSION@.tar.gz /usr/src/packages/SOURCES; \
		rpm -ba /usr/src/packages/SPECS/nut-aix.spec; \
	fi;
