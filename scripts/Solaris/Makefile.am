EXTRA_DIST = makelocal.sh prototype

package: makelocal.sh pkginfo prototype
	$ python $(srcdir)/prepackage.py	
	$(srcdir)/makelocal.sh
	$ mv NUT_solaris_package.local.gz NUT_solaris_package@PACKAGE_VERSION@.local.gz
