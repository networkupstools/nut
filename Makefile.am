# top-level Makefile for NUT

# include directory for aclocal
ACLOCAL_AMFLAGS = -I m4

# subdirectories to build and distribute. The order matters, as
# several subdirectories depend on stuff in "common" being built first
SUBDIRS = common clients conf data docs drivers include lib m4 man	\
  packaging scripts server

EXTRA_DIST = MAINTAINERS UPGRADING

# ----------------------------------------------------------------------
# flags to pass to ./configure when calling "make distcheck". Try
# to check as many features as possible! Also need to give pkgconfig-dir,
# so that staged install does not fail.
DISTCHECK_CONFIGURE_FLAGS = --with-all --with-ssl \
 --with-pkgconfig-dir='$${prefix}/lib/pkgconfig'

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
	@echo "Use './configure --with-lib' instead."
usb build-usb install-usb:
	@echo "Error: 'make $@' no longer exists."
	@echo "Use './configure --with-usb' instead."
snmp build-snmp install-snmp install-snmp-mgr install-snmp-man: 
	@echo "Error: 'make $@' no longer exists."
	@echo "Use './configure --with-snmp' instead."
setver:
	@echo "Error: 'make setver' no longer exists."
	@echo "Edit configure.in to set version number."
