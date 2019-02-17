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
DISTCHECK_LIGHT_MAN_FLAGS = --with-all=auto --with-ssl=auto --with-doc=man
DISTCHECK_VALGRIND_FLAGS = --with-all=auto --with-ssl=auto --with-doc=skip --with-valgrind CXXFLAGS='$(CXXFLAGS) -g' CFLAGS='$(CFLAGS) -g'

# The checks for DMF builds involve several distinct combinations of options
# that the recipes should accept as valid. We generally skip items tested
# previously, since the rebuild-tests take a lot of time even so.
DISTCHECK_DMF_COMMON_FLAGS =  --with-all=no --with-dmf=yes --with-snmp=yes --with-neon=yes --with-dev=yes --with-snmp_dmf=yes --with-doc=skip -C
DISTCHECK_DMF_NOREGEN_FLAGS = --with-dmfsnmp-regenerate=no --with-dmfnutscan-regenerate=no --with-dmfsnmp-validate=no --with-dmfnutscan-validate=no
DISTCHECK_DMF_WITH_LTDL_YES_FLAGS = $(DISTCHECK_DMF_COMMON_FLAGS) $(DISTCHECK_DMF_NOREGEN_FLAGS) --with-libltdl=yes
DISTCHECK_DMF_WITH_LTDL_NO_FLAGS = $(DISTCHECK_DMF_COMMON_FLAGS) $(DISTCHECK_DMF_NOREGEN_FLAGS) --with-libltdl=no
DISTCHECK_DMF_WITH_LUA_YES_FLAGS = $(DISTCHECK_DMF_COMMON_FLAGS) $(DISTCHECK_DMF_NOREGEN_FLAGS) --with-lua=yes --with-snmp_dmf_lua=yes
DISTCHECK_DMF_WITH_LUA_YESNO_FLAGS = $(DISTCHECK_DMF_COMMON_FLAGS) $(DISTCHECK_DMF_NOREGEN_FLAGS) --with-lua=yes --with-snmp_dmf_lua=no --with-dmf_lua=no
DISTCHECK_DMF_WITH_LUA_NO_FLAGS = $(DISTCHECK_DMF_COMMON_FLAGS) $(DISTCHECK_DMF_NOREGEN_FLAGS) --with-lua=no --with-snmp_dmf_lua=no
DISTCHECK_DMF_WITH_REGEN_NO_FLAGS = $(DISTCHECK_DMF_COMMON_FLAGS) $(DISTCHECK_DMF_NOREGEN_FLAGS)
DISTCHECK_DMF_WITH_REGEN_YES_FLAGS = $(DISTCHECK_DMF_COMMON_FLAGS) --with-dmfsnmp-regenerate=yes --with-dmfnutscan-regenerate=yes --with-dmfsnmp-validate=yes --with-dmfnutscan-validate=yes
# Enable all DMF-related options, including again generation and
# validation of DMF files from C sources
DISTCHECK_DMF_ALL = $(DISTCHECK_DMF_WITH_REGEN_YES_FLAGS) --with-snmp_dmf_lua=yes --with-libltdl=yes
# Also have a way to test legacy code structure with no DMF support built in
DISTCHECK_DMF_NO = --with-all=auto --with-dmf=no --with-doc=skip --with-dev=yes -C

DISTCHECK_CONFIGURE_FLAGS = ${DISTCHECK_FLAGS}		\
 --with-systemdsystemunitdir='$${prefix}/lib/systemd/system' \
 --with-systemdshutdowndir='$${prefix}/lib/systemd/system-shutdown' \
 --with-augeas-lenses-dir='$${prefix}/usr/share/augeas/lenses'		\
 --with-hotplug-dir='$${prefix}/etc/hotplug'		\
 --with-udev-dir='$${prefix}/etc/udev'			\
 --with-devd-dir='$${prefix}/etc/devd'

distcheck-light:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_LIGHT_FLAGS)" distcheck

distcheck-light-man:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_LIGHT_MAN_FLAGS)" distcheck

# Make a distcheck (and check in particular) with enabled valgrind and debug info
memcheck distcheck-valgrind:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_VALGRIND_FLAGS)" distcheck

distcheck-dmf-features-REGEN_NO:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_WITH_REGEN_NO_FLAGS)" distcheck || 	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_WITH_REGEN_NO_FLAGS" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-features-LTDL_YES:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_WITH_LTDL_YES_FLAGS)" distcheck || 	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_WITH_LTDL_YES_FLAGS" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-features-LTDL_NO:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_WITH_LTDL_NO_FLAGS)" distcheck ||  	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_WITH_LTDL_NO_FLAGS" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-features-LUA_YES:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_WITH_LUA_YES_FLAGS)" distcheck ||  	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_WITH_LUA_YES_FLAGS" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-features-LUA_YESNO:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_WITH_LUA_YESNO_FLAGS)" distcheck ||	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_WITH_LUA_YESNO_FLAGS" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-features-LUA_NO:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_WITH_LUA_NO_FLAGS)" distcheck ||   	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_WITH_LUA_NO_FLAGS" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-features-REGEN_YES:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_WITH_REGEN_YES_FLAGS)" distcheck ||	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_WITH_REGEN_YES_FLAGS" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

# Intentionally done as one rule, to avoid parallelization and conflicts
distcheck-dmf-features:
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features-REGEN_NO
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features-LTDL_YES
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features-LTDL_NO
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features-LUA_YES
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features-LUA_YESNO
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features-LUA_NO
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features-REGEN_YES
	echo "PASSED $@ suite!"

# Just test the common useful case of enabling (requiring) everything
# related to DMF at once
distcheck-dmf-all-yes:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_ALL)" distcheck ||	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_ALL" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-no:
	$(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_DMF_NO)" distcheck ||	{ RES=$$? ; echo "FAILED with DISTCHECK_DMF_NO" >&2; exit $$RES; }
	echo "PASSED $@ suite!"

distcheck-dmf-warnings:
	$(MAKE) $(AM_MAKEFLAGS) CFLAGS="$(CFLAGS) -Werror -Wall" DISTCHECK_FLAGS="$(DISTCHECK_DMF_ALL)" distcheck
	echo "PASSED $@ suite!"

distcheck-dmf:
	$(MAKE) $(AM_MAKEFLAGS) distcheck-light-man
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-features
	$(MAKE) $(AM_MAKEFLAGS) distcheck-dmf-warnings
	echo "PASSED $@ suite!"

# Shortcut for DMF development - only checks its specifics after building NUT
# The extent of actions and tests called below depends on project configuration
check-dmf: all
	cd $(top_srcdir)/scripts/DMF && $(MAKE) check

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
#GITLOG_START_POINT=v2.6.0
# For the FTY NUT fork, the interesting list is differences from common master
GITLOG_START_POINT=master

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
