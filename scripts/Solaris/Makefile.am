# Network UPS Tools: scripts/Solaris

EXTRA_DIST = makelocal.sh precheck.py.in preproto.pl.in README.adoc
PROTOTYPE_DIR = $(DESTDIR)@prefix@
SOLARIS_CHECK_TARGETS =
PYTHON = @PYTHON@

SOLARIS_SMF_MANIFESTS = \
	nut.xml \
	nut-server.xml \
	nut-logger.xml \
	nut-monitor.xml \
	nut-driver.xml \
	nut-driver-enumerator.xml

SOLARIS_SMF_METHODSCRIPTS = \
	svc-nut-server \
	svc-nut-logger \
	svc-nut-monitor

if WITH_SOLARIS_SMF
# OS equivalent of /lib/svc/method and /var/svc/manifest/application
# but we can just use then from this location
solarissmfmethoddir = @datadir@/solaris-smf/method
solarissmfmanifestdir = @datadir@/solaris-smf/manifest
solarissmfmethod_SCRIPTS = $(SOLARIS_SMF_METHODSCRIPTS)
solarissmfmanifest_DATA = $(SOLARIS_SMF_MANIFESTS)

libexec_SCRIPTS = ../upsdrvsvcctl/nut-driver-enumerator.sh

sbin_SCRIPTS = ../upsdrvsvcctl/upsdrvsvcctl

SOLARIS_CHECK_TARGETS += check-local-solaris-smf
endif

if WITH_SOLARIS_INIT
solarisinitscriptdir = @datadir@/solaris-init
solarisinitscript_SCRIPTS = nut reset-ups-usb-solaris.sh.sample
endif

EXTRA_DIST += reset-ups-usb-solaris.sh.sample

SOLARIS_PACKAGE_TARGETS =

if WITH_SOLARIS_PKG_IPS
SOLARIS_PACKAGE_TARGETS += package-solaris-ips
endif

if WITH_SOLARIS_PKG_SVR4
SOLARIS_PACKAGE_TARGETS += package-solaris-svr4
endif

package: $(SOLARIS_PACKAGE_TARGETS)

# TODO: Reduce build dependencies (implicit!) on python and perl
#       by shelling the scripts used below
# NOTE: This assumes the rest of the product has already been built
#       and installed under PROTOTYPE_DIR, but declares no explicit
#       dependency on that
SOLARIS_PACKAGE_SVR4_HELPERSCRIPTS = makelocal.sh precheck.py preproto.pl
SOLARIS_PACKAGE_SVR4_INSTALLSCRIPTS = preinstall postinstall preremove postremove
SOLARIS_PACKAGE_SVR4_INSTALLDATA = pkginfo
package-solaris-svr4: $(SOLARIS_PACKAGE_SVR4_HELPERSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLDATA)
	ISANAME=''; case x"$(target_cpu)" in \
		xi386|xsparc|xsparcv9|xamd64)	ISANAME='$(target_cpu)' ;; \
		xx86_64)	ISANAME='amd64' ;; \
		xi686)	ISANAME='i386' ;; \
		xsparcv7)ISANAME='sparc' ;; \
		x) UNAME_P="`uname -p`" && ISANAME="$${UNAME_P}" ;; \
	esac ; \
	ISABITS=32; case x"$${ISANAME}" in \
		xi386|xsparc)	ISABITS=32 ;; \
		xsparcv9|xamd64)	ISABITS=64 ;; \
		x*) echo "WARNING: Unexpected ISANAME='$${ISANAME}'" >&2 ;; \
	esac; \
	case x"$${ISABITS}" in \
		x32)	ISA_PKGNAME="NUT" ;; \
		x64)	ISA_PKGNAME="NUTx" ;; \
	esac; \
	sed -e 's/^PKG="[^"][^"]*"$$/PKG="'"$${ISA_PKGNAME}"'"/' -i $(builddir)/pkginfo || exit ; \
	if test -n "@auglensdir@" && test -d "$(DESTDIR)@auglensdir@" ; then \
		$(MKDIR_P) "$(DESTDIR)@datadir@/augeas-lenses" && \
		( cd "$(DESTDIR)@auglensdir@" && \
		  ( cp -prf ./ "$(DESTDIR)@datadir@/augeas-lenses/" || cp -rf ./ "$(DESTDIR)@datadir@/augeas-lenses/" ) ) || exit ; \
	fi ; \
	( cd $(PROTOTYPE_DIR) && find . -print | pkgproto > prototype1 ) || exit ; \
	cp $(SOLARIS_PACKAGE_SVR4_HELPERSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLDATA) $(PROTOTYPE_DIR) || exit ; \
	( cd $(PROTOTYPE_DIR) && chmod +x $(SOLARIS_PACKAGE_SVR4_HELPERSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLSCRIPTS) ) || exit ; \
	( cd $(PROTOTYPE_DIR) && perl preproto.pl ) || exit ; \
	if test -n '$(PYTHON)' ; then \
		( cd $(PROTOTYPE_DIR) && $(PYTHON) precheck.py ) || exit ; \
	fi ; \
	( cd $(PROTOTYPE_DIR) && rm -f prototype1 ) || exit ; \
	( cd $(PROTOTYPE_DIR) && ./makelocal.sh ) || exit ; \
	cp $(PROTOTYPE_DIR)/*.gz $(builddir) || exit ; \
	if [ -n "$${ISANAME}" ] ; then \
		mv -f NUT*_solaris_package.local.gz "$(abs_top_builddir)/$${ISA_PKGNAME}_solaris_$${ISANAME}_package-@PACKAGE_VERSION@.local.gz" || exit ; \
	fi

# TODO: Define support for IPS packaging (provide p5m files and make rules)
package-solaris-ips:
	@echo "SKIPPED : Target $@ is not implemented yet"

check-local: $(SOLARIS_CHECK_TARGETS)

check-local-solaris-smf: $(SOLARIS_SMF_MANIFESTS)
	@[ -x /usr/sbin/svccfg ] || { echo "WARNING : Target $@ skipped due to absent /usr/sbin/svccfg" >&2; return 0; } ; \
	 RES=0 ; for F in $? ; do \
	    echo "  SVCCFG-VALIDATE  $$F"; \
	    /usr/sbin/svccfg validate "$$F" || RES=$$? ; \
	 done; exit $$RES

SPELLCHECK_SRC = README.adoc

# NOTE: Due to portability, we do not use a GNU percent-wildcard extension.
# We also have to export some variables that may be tainted by relative
# paths when parsing the other makefile (e.g. MKDIR_P that may be defined
# via expanded $(top_builddir)/install-sh):
#%-spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

# NOTE: Portable suffix rules do not allow prerequisites, so we shim them here
# by a wildcard target in case the make implementation can put the two together.
*-spellchecked: Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)

.sample.sample-spellchecked:
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

.in.in-spellchecked:
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict:
	+$(MAKE) -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC="$(SPELLCHECK_SRC)" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

CLEANFILES = *-spellchecked

MAINTAINERCLEANFILES = Makefile.in .dirstamp
