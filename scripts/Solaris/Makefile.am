EXTRA_DIST = makelocal.sh 

package: makelocal.sh pkginfo
	$ cd @prefix@; $ find . -print | pkgproto > prototype1
	$ cp makelocal.sh precheck.py pkginfo nut preinstall postinstall preremove postremove preproto.pl @prefix@ 
	$ cd @prefix@; perl preproto.pl
	$ cd @prefix@; python precheck.py	
	$ cd @prefix@; rm -f prototype1	
	$ cd @prefix@; ./makelocal.sh
	$ cp @prefix@/*.gz $(srcdir)
	if test `uname -p` = "i386"; then \
		mv NUT_solaris_package.local.gz NUT_solaris_i386_package@PACKAGE_VERSION@.local.gz; \
	else \
		if test `uname -p` = "sparc"; then \
			mv NUT_solaris_package.local.gz NUT_solaris_sparc_package@PACKAGE_VERSION@.local.gz; \
		fi; \
	fi;
