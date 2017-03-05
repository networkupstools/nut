EXTRA_DIST = makelocal.sh
PROTOTYPE_DIR = $(DESTDIR)@prefix@

if WITH_SOLARIS_SMF
solarissmfmethoddir = /lib/svc/method
solarissmfmanifestdir = /var/svc/manifest/application
solarissmfmethod_SCRIPTS = \
	../systemd/nut-driver-enumerator.sh \
	svc-nut \
	svc-nut-client
solarissmfmanifest_DATA = \
	nut.xml \
	nut-client.xml \
	nut-driver.xml \
	nut-driver-enumerator.xml
endif

SOLARIS_PACKAGE_TARGETS =

# TODO: Define support for IPS packaging
if WITH_SOLARIS_PKG_IPS
SOLARIS_PACKAGE_TARGETS += package-ips
endif

if WITH_SOLARIS_PKG_SVR4
SOLARIS_PACKAGE_TARGETS += package-svr4
endif

package: $(SOLARIS_PACKAGE_TARGETS)

# TODO: Reduce build dependencies (implicit!) on python and perl
#       by shelling the scripts used below
# NOTE: This assumes the rest of the product has already been built
#       and installed under PROTOTYPE_DIR, but declares no explicit
#       dependency on that
package-svr4: makelocal.sh pkginfo
	cd $(PROTOTYPE_DIR) && find . -print | pkgproto > prototype1
	cp makelocal.sh precheck.py pkginfo nut preinstall postinstall preremove postremove preproto.pl $(PROTOTYPE_DIR)
	cd $(PROTOTYPE_DIR) && perl preproto.pl
	cd $(PROTOTYPE_DIR) && python precheck.py
	cd $(PROTOTYPE_DIR) && rm -f prototype1
	cd $(PROTOTYPE_DIR) && ./makelocal.sh
	cp $(PROTOTYPE_DIR)/*.gz $(srcdir)
	UNAME_P="`uname -p`" && case "$${UNAME_P}" in \
		i386|sparc) \
			mv NUT_solaris_package.local.gz "NUT_solaris_$${UNAME_P}_package@PACKAGE_VERSION@.local.gz" ;; \
	esac

package-ips:
	@echo "Target $@ is not implemented yet"
