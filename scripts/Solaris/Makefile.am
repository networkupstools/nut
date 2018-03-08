EXTRA_DIST = makelocal.sh README
PROTOTYPE_DIR = $(DESTDIR)@prefix@
SOLARIS_CHECK_TARGETS =

SOLARIS_SMF_MANIFESTS = \
	nut.xml \
	nut-server.xml \
	nut-monitor.xml \
	nut-driver.xml \
	nut-driver-enumerator.xml

SOLARIS_SMF_METHODSCRIPTS = \
	svc-nut-server \
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

solarisinitscriptdir = @datadir@/solaris-init
solarisinitscript_SCRIPTS = nut

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
	if test -n "@auglensdir@" && test -d "$(DESTDIR)@auglensdir@" ; then \
		mkdir -p "$(DESTDIR)@datadir@/augeas-lenses" && \
		cd "$(DESTDIR)@auglensdir@" && \
		( cp -prf ./ "$(DESTDIR)@datadir@/augeas-lenses/" || cp -rf ./ "$(DESTDIR)@datadir@/augeas-lenses/" ) ; fi
	cd $(PROTOTYPE_DIR) && find . -print | pkgproto > prototype1
	cp $(SOLARIS_PACKAGE_SVR4_HELPERSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLDATA) $(PROTOTYPE_DIR)
	cd $(PROTOTYPE_DIR) && chmod +x $(SOLARIS_PACKAGE_SVR4_HELPERSCRIPTS) $(SOLARIS_PACKAGE_SVR4_INSTALLSCRIPTS)
	cd $(PROTOTYPE_DIR) && perl preproto.pl
	cd $(PROTOTYPE_DIR) && python precheck.py
	cd $(PROTOTYPE_DIR) && rm -f prototype1
	cd $(PROTOTYPE_DIR) && ./makelocal.sh
	cp $(PROTOTYPE_DIR)/*.gz $(builddir)
	UNAME_P="`uname -p`" && case "$${UNAME_P}" in \
		i386|sparc) \
			mv -f NUT_solaris_package.local.gz "$(abs_top_builddir)/NUT_solaris_$${UNAME_P}_package@PACKAGE_VERSION@.local.gz" ;; \
	esac

# TODO: Define support for IPS packaging (provide p5m files and make rules)
package-solaris-ips:
	@echo "SKIPPED : Target $@ is not implemented yet"

check-local: $(SOLARIS_CHECK_TARGETS)

check-local-solaris-smf: $(SOLARIS_SMF_MANIFESTS)
	@[ -x /usr/sbin/svccfg ] || { echo "WARNING : Target $@ skipped due to absent /usr/sbin/svccfg" >&2; return 0; } ; \
	 RES=0 ; for F in $^ ; do \
	    echo "  SVCCFG-VALIDATE  $$F"; \
	    /usr/sbin/svccfg validate "$$F" || RES=$$? ; \
	 done; exit $$RES
