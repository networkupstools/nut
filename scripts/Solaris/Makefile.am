EXTRA_DIST = makelocal.sh 

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

package-svr4: makelocal.sh pkginfo
	cd @prefix@ && find . -print | pkgproto > prototype1
	cp makelocal.sh precheck.py pkginfo nut preinstall postinstall preremove postremove preproto.pl @prefix@
	cd @prefix@ && perl preproto.pl
	cd @prefix@ && python precheck.py
	cd @prefix@ && rm -f prototype1
	cd @prefix@ && ./makelocal.sh
	cp @prefix@/*.gz $(srcdir)
	UNAME_P="`uname -p`" && case "$${UNAME_P}" in \
		i386|sparc) \
			mv NUT_solaris_package.local.gz NUT_solaris_$${UNAME_P}_package@PACKAGE_VERSION@.local.gz;; \
	esac

package-ips:
	@echo "Target $@ is not implemented yet"
