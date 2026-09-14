# In 2026, Debian obsoletes and removes CDBS and its scripts
# Stashing https://salsa.debian.org/debian/cdbs/-/blob/a110afb99997b94de6acb1e0758210cdfd8ec3dd/1/class/autotools.mk.in
# and neighbors

# -*- mode: makefile; coding: utf-8 -*-
# Copyright © 2002,2003 Colin Walters <walters@debian.org>
# Copyright © 2008-2010, 2014, 2016 Jonas Smedegaard <dr@jones.dk>
# Description: A class to configure and build GNU autoconf+automake packages
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#PATH_RULES#

ifndef _cdbs_class_autotools
_cdbs_class_autotools = 1

# BEGIN # include $(_cdbs_class_path)/autotools-files.mk$(_cdbs_makefile_suffix)

ifndef _cdbs_class_autotools_files
_cdbs_class_autotools_files = 1

# BEGIN # include $(_cdbs_class_path)/autotools-vars.mk$(_cdbs_makefile_suffix)

ifndef _cdbs_class_autotools_vars
_cdbs_class_autotools_vars = 1

# BEGIN # include $(_cdbs_class_path)/makefile.mk$(_cdbs_makefile_suffix)

ifndef _cdbs_class_makefile
_cdbs_class_makefile = 1

# Included by caller # include $(_cdbs_rules_path)/buildcore.mk$(_cdbs_makefile_suffix)
# BEGIN # include $(_cdbs_class_path)/makefile-vars.mk$(_cdbs_makefile_suffix)

ifndef _cdbs_class_makefile_vars
_cdbs_class_makefile_vars = 1

# BEGIN # include $(_cdbs_class_path)/langcore.mk$(_cdbs_makefile_suffix)

ifndef _cdbs_class_langcore
_cdbs_class_langcore = 1

include $(_cdbs_rules_path)/buildvars.mk$(_cdbs_makefile_suffix)

# Resolve our defaults
# GNU Make doesn't export current environment in $(shell ..) function.
# We need at least some of the DEB_* flags for dpkg-buildflags, so
# extract them from the defined variables.  Sadly there seems to be no
# way to just get all exported variables.
#
# massage dpkg-buildflag output:
#  * filter to include only lines matching expected format
#  * transform prefix, e.g. "export LDFLAGS := ..." -> "LDFLAGS ?= ..."
$(shell \
	$(call cdbs_set_nondefaultvars,\
		$(foreach flag,$(shell dpkg-buildflags --list),\
			$(foreach op,SET STRIP APPEND PREPEND,\
				DEB_$(flag)_$(op) DEB_$(flag)_MAINT_$(op)))\
		DEB_BUILD_OPTIONS DEB_BUILD_MAINT_OPTIONS) \
	dpkg-buildflags --export=make \
	| perl -pe 's/^export\s+//; s/:=/?=/' \
	> debian/_cdbs_buildflags.mk )
-include debian/_cdbs_buildflags.mk
$(shell rm -f debian/_cdbs_buildflags.mk)

$(eval $(and $(cdbs_crossbuild),$(filter default,$(origin CC)),\
	CC := $(DEB_HOST_GNU_TYPE)-gcc))
$(eval $(and $(cdbs_crossbuild),$(filter default,$(origin CXX)),\
	CXX := $(DEB_HOST_GNU_TYPE)-g++))

ifneq (,$(filter parallel=%,$(DEB_BUILD_OPTIONS)))
	DEB_PARALLEL_JOBS ?= $(patsubst parallel=%,%,$(filter parallel=%,$(DEB_BUILD_OPTIONS)))
endif

endif

#  END  # include $(_cdbs_class_path)/langcore.mk$(_cdbs_makefile_suffix)

#DEB_MAKE_MAKEFILE =
DEB_MAKE_ENVVARS ?= $(if $(cdbs_crossbuild),\
	CC="$(CC)" CXX="$(CXX)" PKG_CONFIG="$(DEB_HOST_GNU_TYPE)-pkg-config")
DEB_MAKE_PARALLEL ?= $(and $(DEB_BUILD_PARALLEL),$(DEB_PARALLEL_JOBS),\
	-j$(DEB_PARALLEL_JOBS))

# Derived classes that supply the flags some other way (e.g., configure)
# should set this variable to empty.
DEB_MAKE_EXTRA_ARGS ?= \
	CFLAGS="$(or $(CFLAGS_$(cdbs_curpkg)),$(CFLAGS))" \
	CXXFLAGS="$(or $(CXXFLAGS_$(cdbs_curpkg)),$(CXXFLAGS))" \
	CPPFLAGS="$(or $(CPPFLAGS_$(cdbs_curpkg)),$(CPPFLAGS))" \
	LDFLAGS="$(or $(LDFLAGS_$(cdbs_curpkg)),$(LDFLAGS))" \
	$(DEB_MAKE_PARALLEL)

DEB_MAKE_INVOKE ?= $(strip \
	$(DEB_MAKE_ENVVARS) $(MAKE) \
	$(if $(DEB_MAKE_MAKEFILE),\
		-f $(DEB_MAKE_MAKEFILE)) \
	-C $(or $(cdbs_make_curbuilddir),$(cdbs_curbuilddir)) \
	$(DEB_MAKE_EXTRA_ARGS))

#DEB_MAKE_BUILD_TARGET =

# If your Makefile provides an "install" target, you need to give the requisite commands
# here to install it into the staging directory.  For automake-using programs, this
# looks like: install DESTDIR=$(cdbs_make_curdestdir)
# (which expands to either DEB_DESTDIR_xxx or DEB_DESTDIR)
# If you're using automake though, you likely want to be including autotools.mk instead
# of this file.

DEB_MAKE_CLEAN_TARGET ?= clean

#DEB_MAKE_CHECK_TARGET = test

# If DEB_MAKE_FLAVORS is set compilation is done once per flavor.
# NB! This must be declared _before_ including makefile.mk
#DEB_MAKE_FLAVORS = light normal enhanced

# If building multiple flavors, skeleton strings are used for
# DEB_BUILDDIR and DEB_DESTDIR, with @FLAVOR@ expanding to actual
# flavor.
DEB_MAKE_BUILDDIRSKEL ?= $(cdbs_curbuilddir)/@FLAVOR@
DEB_MAKE_DESTDIRSKEL ?= $(cdbs_curdestdir)/@FLAVOR@

endif

#  END  # include $(_cdbs_class_path)/makefile-vars.mk$(_cdbs_makefile_suffix)

cdbs_make_flavors = $(sort $(DEB_MAKE_FLAVORS))
cdbs_make_builddir_check = $(if $(call cdbs_streq,$(DEB_BUILDDIR),$(DEB_SRCDIR)),\
	$(error DEB_MAKE_FLAVORS in use: \
		DEB_BUILDDIR must be different from DEB_SRCDIR$(comma) \
		and needs to be declared before including makefile.mk))
cdbs_make_build_stamps = $(if $(cdbs_make_flavors),\
	$(cdbs_make_builddir_check)\
	$(patsubst %,debian/stamp-makefile-build/%,\
		$(cdbs_make_flavors)),\
	debian/stamp-makefile-build)
cdbs_make_install_stamps = $(if $(cdbs_make_flavors),\
	$(cdbs_make_builddir_check)\
	$(patsubst %,debian/stamp-makefile-install/%,\
		$(cdbs_make_flavors)),\
	debian/stamp-makefile-install)
cdbs_make_check_stamps = $(if $(cdbs_make_flavors),\
	$(cdbs_make_builddir_check)\
	$(patsubst %,debian/stamp-makefile-check/%,\
		$(cdbs_make_flavors)),\
	debian/stamp-makefile-check)
cdbs_make_clean_nonstamps = $(if $(cdbs_make_flavors),\
	$(patsubst %,makefile-clean/%,$(cdbs_make_flavors)),\
	makefile-clean)
cdbs_make_curflavor = $(strip $(if $(cdbs_make_flavors),\
	$(filter-out %/,$(subst /,/ ,$@))))
cdbs_make_curbuilddir = $(strip $(if $(cdbs_make_flavors),\
	$(subst @FLAVOR@,$(cdbs_make_curflavor),$(or $(strip \
		$(DEB_MAKE_BUILDDIRSKEL_$(cdbs_make_curflavor))),$(strip \
		$(DEB_MAKE_BUILDDIRSKEL)))),\
	$(cdbs_curbuilddir)))
cdbs_make_curdestdir = $(strip $(if $(cdbs_make_flavors),\
	$(subst @FLAVOR@,$(cdbs_make_curflavor),$(or $(strip \
		$(DEB_MAKE_DESTDIRSKEL_$(cdbs_make_curflavor))),$(strip \
		$(DEB_MAKE_DESTDIRSKEL)))),\
	$(cdbs_curdestdir)))

DEB_PHONY_RULES += makefile-clean $(cdbs_make_clean_nonstamps)

pre-build::
	$(if $(cdbs_make_flavors),\
		mkdir -p \
			debian/stamp-makefile-build \
			debian/stamp-makefile-install)
	$(and $(cdbs_make_flavors),$(DEB_MAKE_CHECK_TARGET),\
		mkdir -p debian/stamp-makefile-check)

common-build-arch common-build-indep:: $(cdbs_make_build_stamps)
$(cdbs_make_build_stamps):
	+$(DEB_MAKE_INVOKE) $(DEB_MAKE_BUILD_TARGET)
	touch $@

cleanbuilddir:: makefile-clean
makefile-clean:: $(if $(cdbs_make_flavors),$(cdbs_make_clean_nonstamps))
	$(if $(cdbs_make_flavors),\
		-rmdir --ignore-fail-on-non-empty \
			debian/stamp-makefile-build \
			debian/stamp-makefile-install,\
		rm -f \
			debian/stamp-makefile-build \
			debian/stamp-makefile-install)

$(cdbs_make_clean_nonstamps)::
	$(if $(DEB_MAKE_CLEAN_TARGET),\
		+-$(DEB_MAKE_INVOKE) -k $(DEB_MAKE_CLEAN_TARGET),\
		$(call cdbs_warn,DEB_MAKE_CLEAN_TARGET unset$(comma) \
			not running clean))
	$(if $(cdbs_make_flavors),\
		rm -f \
			$(@:makefile-clean%=debian/stamp-makefile-build%) \
			$(@:makefile-clean%=debian/stamp-makefile-install%))

common-install-arch common-install-indep:: common-install-impl
common-install-impl:: $(cdbs_make_install_stamps)
$(cdbs_make_install_stamps)::
	$(if $(DEB_MAKE_INSTALL_TARGET),\
		+$(DEB_MAKE_INVOKE) $(DEB_MAKE_INSTALL_TARGET),\
		$(call cdbs_warn,DEB_MAKE_INSTALL_TARGET unset$(comma) \
			skipping default makefile.mk common-install target))
	$(if $(DEB_MAKE_INSTALL_TARGET),\
		touch $@)

ifeq (,$(filter nocheck,$(DEB_BUILD_OPTIONS)))
common-build-arch common-build-indep:: $(cdbs_make_check_stamps)
$(cdbs_make_check_stamps) : debian/stamp-makefile-check% : debian/stamp-makefile-build%
	$(if $(DEB_MAKE_CHECK_TARGET),\
		+$(DEB_MAKE_INVOKE) $(DEB_MAKE_CHECK_TARGET),\
		$(call cdbs_warn,DEB_MAKE_CHECK_TARGET unset$(comma) \
			not running checks))
	$(if $(DEB_MAKE_CHECK_TARGET),\
		touch $@)

makefile-clean::
	$(if $(DEB_MAKE_CHECK_TARGET),\
		$(if $(cdbs_make_flavors),\
			-rmdir --ignore-fail-on-non-empty \
				debian/stamp-makefile-check,\
		rm -f debian/stamp-makefile-check))

$(cdbs_make_clean_nonstamps)::
	$(if $(cdbs_make_flavors),\
		rm -f $(@:makefile-clean%=debian/stamp-makefile-check%))
endif

endif


#  END  # include $(_cdbs_class_path)/makefile.mk$(_cdbs_makefile_suffix)

DEB_MAKE_INSTALL_TARGET ?= install DESTDIR=$(cdbs_make_curdestdir)
# FIXME: Restructure to allow early override
DEB_MAKE_CLEAN_TARGET = distclean
#DEB_MAKE_CHECK_TARGET = check

DEB_AC_AUX_DIR ?= $(DEB_SRCDIR)

# Declare CC and CXX only if explicitly set in environment or makefile
# (i.e. skip if builtin make default would have been used)
# This is needed for proper cross-compilation - see bug#450483)
DEB_CONFIGURE_SCRIPT_ENV ?= \
	$(call cdbs_set_nondefaultvars,CC CXX) \
	CFLAGS="$(CFLAGS)" \
	CXXFLAGS="$(CXXFLAGS)" \
	CPPFLAGS="$(CPPFLAGS)" \
	LDFLAGS="$(LDFLAGS)"

DEB_CONFIGURE_SCRIPT ?= $(CURDIR)/$(DEB_SRCDIR)/configure

# Provide --host only if different from --build, to support cross-
# compiling with autotools 2.52+ without slowing down normal builds.
# Cross-compiling with autotools 2.13 is unsupported, as it needs
# other tweaks (more info at autotools-dev README.Debian)
DEB_CONFIGURE_CROSSBUILD_ARGS ?= \
	--build=$(DEB_BUILD_GNU_TYPE) \
	$(if $(cdbs_crossbuild),\
		--host=$(DEB_HOST_GNU_TYPE))

DEB_CONFIGURE_PREFIX ?=/usr
DEB_CONFIGURE_INCLUDEDIR ?= "\$${prefix}/include"
DEB_CONFIGURE_MANDIR ?= "\$${prefix}/share/man"
DEB_CONFIGURE_INFODIR ?= "\$${prefix}/share/info"
DEB_CONFIGURE_SYSCONFDIR ?= /etc
DEB_CONFIGURE_LOCALSTATEDIR ?= /var
DEB_CONFIGURE_LIBEXECDIR ?= "\$${prefix}/lib/$(DEB_SOURCE_PACKAGE)"
# --srcdir=. is required because otherwise configure wants to analyse
# $0 to see whether a VPATH build is needed.  This tells it with
# absolute certainly that this is NOT a VPATH build.
DEB_CONFIGURE_PATH_ARGS ?= \
	--prefix=$(DEB_CONFIGURE_PREFIX) \
	--includedir=$(DEB_CONFIGURE_INCLUDEDIR) \
	--mandir=$(DEB_CONFIGURE_MANDIR) \
	--infodir=$(DEB_CONFIGURE_INFODIR) \
	--sysconfdir=$(DEB_CONFIGURE_SYSCONFDIR) \
	--localstatedir=$(DEB_CONFIGURE_LOCALSTATEDIR) \
	--libexecdir=$(DEB_CONFIGURE_LIBEXECDIR) \
	$(if $(subst $(DEB_SRCDIR),,$(cdbs_make_curbuilddir)),\
		,\
		--srcdir=.)

DEB_CONFIGURE_NORMAL_ARGS ?= \
	$(DEB_CONFIGURE_CROSSBUILD_ARGS) \
	$(DEB_CONFIGURE_PATH_ARGS) \
	--disable-maintainer-mode \
	--disable-dependency-tracking \
	--disable-silent-rules

# all environment settings for autotools configure execution
# (potentially extended by other snippets)
cdbs_autotools_configure_env = $(DEB_CONFIGURE_SCRIPT_ENV)

DEB_CONFIGURE_INVOKE ?= cd $(cdbs_make_curbuilddir) && \
	$(cdbs_autotools_configure_env) \
	$(DEB_CONFIGURE_SCRIPT) \
	$(DEB_CONFIGURE_NORMAL_ARGS) \
	$(DEB_CONFIGURE_DEBUG_ARGS)

#DEB_CONFIGURE_EXTRA_FLAGS =

endif


#  END  # include $(_cdbs_class_path)/autotools-vars.mk$(_cdbs_makefile_suffix)

# Compatibility blurb, will be eventualy removed
ifneq (,$(DEB_AUTO_UPDATE_AUTOMAKE))
ifeq (,$(DEB_AUTO_UPDATE_ACLOCAL))
$(call cdbs_warn,DEB_AUTO_UPDATE_AUTOMAKE will eventually stop implying \
	DEB_AUTO_UPDATE_ACLOCAL.  If you meant aclocal.m4 to be \
	regenerated, please use DEB_AUTO_UPDATE_ACLOCAL.)
DEB_AUTO_UPDATE_ACLOCAL ?= $(DEB_AUTO_UPDATE_AUTOMAKE)
endif
endif

# Some update rules are useless on their own
ifeq (pre,$(DEB_AUTO_UPDATE_LIBTOOL))
ifeq (,$(DEB_AUTO_UPDATE_ACLOCAL))
$(call cdbs_warn,DEB_AUTO_UPDATE_LIBTOOL requires DEB_AUTO_UPDATE_ACLOCAL.)
endif
endif
ifneq (,$(DEB_AUTO_UPDATE_ACLOCAL))
ifeq (,$(DEB_AUTO_UPDATE_AUTOCONF))
$(call cdbs_warn,DEB_AUTO_UPDATE_ACLOCAL requires DEB_AUTO_UPDATE_AUTOCONF.)
endif
endif

DEB_ACLOCAL_ARGS ?= $(if $(wildcard $(DEB_SRCDIR)/m4),\
	-I m4)

# resolve make rule from autotools command version hints
# usage: $(call _cdbs_autotools_invoke,$(VERSION),$(VERSIONEDBINARY),$(BINARY),$(LEGACY))
#  * when VERSION contains comma: return "$(BINARY)"
#  * else, when LEGACY exist: return $$(which "$(VERSIONEDBINARY)$(VERSION)" || which "$(BINARY)")
#  * else: return "$(VERSIONEDBINARY)$(VERSION)"
# see also autotools-vars.mk
_cdbs_autotools_invoke = $(if $(findstring $(comma),$1),\
	$3,\
	$(if $4,\
		$$(which "$2$1" || which "$3"),\
		$2$1))

common-configure-arch common-configure-indep:: debian/stamp-autotools-files
debian/stamp-autotools-files:
	$(if $(filter pre,$(DEB_AUTO_UPDATE_LIBTOOL)),\
		cd $(DEB_SRCDIR) && \
		libtoolize -c -f)
	$(if $(DEB_AUTO_UPDATE_AUTOPOINT),\
		cd $(DEB_SRCDIR) && \
		$(call _cdbs_autotools_invoke,$(DEB_AUTO_UPDATE_AUTOPOINT),autopoint,autopoint) \
			$(DEB_AUTOPOINT_ARGS))
	$(if $(DEB_AUTO_UPDATE_ACLOCAL),\
		cd $(DEB_SRCDIR) && \
		$(call _cdbs_autotools_invoke,$(DEB_AUTO_UPDATE_ACLOCAL),aclocal-,aclocal) \
			$(DEB_ACLOCAL_ARGS))
	$(if $(DEB_AUTO_UPDATE_AUTOCONF),\
		$(if $(wildcard $(DEB_SRCDIR)/configure.ac $(DEB_SRCDIR)/configure.in),\
			cd $(DEB_SRCDIR) && \
			$(call _cdbs_autotools_invoke,$(DEB_AUTO_UPDATE_AUTOCONF),autoconf,autoconf,legacy) \
				$(DEB_AUTOCONF_ARGS)))
	$(if $(DEB_AUTO_UPDATE_AUTOHEADER),\
		$(if $(wildcard $(DEB_SRCDIR)/configure.ac $(DEB_SRCDIR)/configure.in),\
			cd $(DEB_SRCDIR) && \
			$(call _cdbs_autotools_invoke,$(DEB_AUTO_UPDATE_AUTOHEADER),autoheader,autoheader,legacy) \
				$(DEB_AUTOHEADER_ARGS)))
	$(if $(DEB_AUTO_UPDATE_AUTOMAKE),\
		$(if $(wildcard $(DEB_SRCDIR)/Makefile.am),\
			cd $(DEB_SRCDIR) && \
			$(call _cdbs_autotools_invoke,$(DEB_AUTO_UPDATE_AUTOMAKE),automake-,automake) \
				$(DEB_AUTOMAKE_ARGS)))
	touch debian/stamp-autotools-files

clean::
	rm -f debian/stamp-autotools-files

endif

#  END  # include $(_cdbs_class_path)/autotools-files.mk$(_cdbs_makefile_suffix)


cdbs_autotools_configure_stamps = $(if $(cdbs_make_flavors),\
	$(cdbs_make_builddir_check)$(patsubst %,debian/stamp-autotools/%,$(cdbs_make_flavors)),\
	debian/stamp-autotools)

cdbs_configure_flags += $(DEB_CONFIGURE_FLAGS_$(cdbs_make_curflavor))

# Overriden from makefile-vars.mk.  We pass CFLAGS and friends to
# ./configure, so no need to pass them to make.
# FIXME: Restructure to allow early override
DEB_MAKE_EXTRA_ARGS = $(DEB_MAKE_PARALLEL)

pre-build::
	$(if $(cdbs_make_flavors),mkdir -p debian/stamp-autotools)

common-configure-arch common-configure-indep:: common-configure-impl
common-configure-impl:: $(cdbs_autotools_configure_stamps)
$(cdbs_autotools_configure_stamps):
	chmod a+x $(DEB_CONFIGURE_SCRIPT)
	$(if $(call cdbs_streq,$(cdbs_make_curbuilddir),$(DEB_BUILDDIR_$(cdbs_curpkg))),\
		,\
		mkdir -p $(cdbs_make_curbuilddir))
	$(strip $(DEB_CONFIGURE_INVOKE) \
		$(cdbs_configure_flags) \
		$(DEB_CONFIGURE_EXTRA_FLAGS) \
		$(DEB_CONFIGURE_USER_FLAGS))
	$(if $(filter post,$(DEB_AUTO_UPDATE_LIBTOOL)),\
		$(if $(wildcard $(cdbs_make_curbuilddir)/libtool),\
			cp -f /usr/bin/libtool $(cdbs_make_curbuilddir)/libtool))
	touch $@

makefile-clean::
	$(if $(cdbs_make_flavors),\
		-rmdir --ignore-fail-on-non-empty \
			debian/stamp-autotools,\
		rm -f debian/stamp-autotools)

$(cdbs_make_clean_nonstamps)::
	$(if $(call cdbs_streq,$(cdbs_make_curbuilddir),$(DEB_BUILDDIR_$(cdbs_curpkg))),\
		,\
		-rmdir --ignore-fail-on-non-empty \
			$(cdbs_make_curbuilddir))
	$(if $(cdbs_make_flavors),\
		rm -f $(@:makefile-clean%=debian/stamp-autotools%))

endif
