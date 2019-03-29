# top-level Makefile for NUT

# include directory for aclocal
ACLOCAL_AMFLAGS = -I m4

# subdirectories to build and distribute. The order matters, as
# several subdirectories depend on stuff in "common" or tools being built first
SUBDIRS = include common clients conf data docs drivers tools \
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
 --with-systemdshutdowndir='$${prefix}/lib/systemd/system-shutdown' \
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

# This target adds syntax-checking for committed shell script files,
# to avoid surprises and delays in finding fatal typos after packaging
check-scripts-syntax:
	@echo 'NOTE: modern bash complains about scripts using backticks (warning not error), which we ignore in NUT codebase for portability reasons: `...` obsolete, use $$(...)'
	for F in `git ls-files || find . -type f` ; do \
	    case "`file "$$F"`" in \
	        *"Bourne-Again shell script"*) ( set -x ; /bin/bash -n "$$F" ; ) ;; \
	        *"POSIX shell script"*|*"shell script"*) ( set -x ; /bin/sh -n "$$F" ; ) ;; \
	    esac || { RES=$$? ; echo "ERROR: Syntax check failed for script file: $$F" >&2 ; exit $$RES ; } ; \
	done
	@echo 'SUCCESS: Shell scripts syntax is acceptable, no fatal issues were found'

shellcheck-disclaimer:
	@echo "==============================================================================="
	@echo "NOTICE: 'make shellcheck' is currently an alias for 'make check-scripts-syntax'"
	@echo "Later it may become a call to the real shellcheck tool (if available on the"
	@echo "build system during the configure phase)"
	@echo "==============================================================================="

shellcheck: shellcheck-disclaimer check-scripts-syntax

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
	$(MAKE) installdirs
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
	@echo "Edit configure.ac to set version number."
package:
	DESTDIR="$(abs_builddir)/_install_pkgprotodir" ; export DESTDIR; \
	rm -rf "$$DESTDIR"; \
	case "`uname -s`" in \
	"HP-UX") \
		( cd scripts/HP-UX && \
		  $(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$DESTDIR" package && \
		  mv NUT_HPUX_package.depot $(abs_top_builddir)/NUT_HPUX_package@PACKAGE_VERSION@.depot ) ;; \
	"SunOS") \
		$(MAKE) $(AM_MAKEFLAGS) && \
		$(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$DESTDIR" install && \
		( cd scripts/Solaris && \
		  $(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$DESTDIR" package ) && \
		$(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$DESTDIR" uninstall && \
		rm -rf "$$DESTDIR" || \
		{ echo "FAILED to produce SunOS packages, inspect '$$DESTDIR' for clues" >&2 ; exit 1; } ;; \
	"AIX") \
		if test -d /usr/src/packages/SPECS -a -w /usr/src/packages/SPECS ; then : ; else echo "Can not write to /usr/src/packages/SPECS" >&2 ; exit 1; fi ; \
		if test -d /usr/src/packages/SOURCES -a -w /usr/src/packages/SOURCES ; then : ; else echo "Can not write to /usr/src/packages/SOURCES" >&2 ; exit 1; fi ; \
		$(MAKE) $(AM_MAKEFLAGS) dist && \
		cp scripts/Aix/nut-aix.spec /usr/src/packages/SPECS && \
		cp scripts/Aix/nut.init nut-@PACKAGE_VERSION@.tar.gz /usr/src/packages/SOURCES && \
		rpm -ba /usr/src/packages/SPECS/nut-aix.spec && \
		mv /usr/src/packages/RPMS/nut*rpm $(abs_top_builddir)/ ;; \
	*)	echo "Unsupported OS for 'make $@' (no recipe bound)" >&2; exit 1;; \
	esac
