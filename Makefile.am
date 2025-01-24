# top-level Makefile for NUT

# Export certain values for ccache which NUT ci_build.sh can customize,
# to facilitate developer iteration re-runs of "make" later.
# At least GNU and BSD make implementations are okay with this syntax.
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_NAMESPACE@export CCACHE_NAMESPACE=@CCACHE_NAMESPACE@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_BASEDIR@export CCACHE_BASEDIR=@CCACHE_BASEDIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_DIR@export CCACHE_DIR=@CCACHE_DIR@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export CCACHE_PATH=@CCACHE_PATH@
@NUT_AM_MAKE_CAN_EXPORT@@NUT_AM_EXPORT_CCACHE_PATH@export PATH=@PATH_DURING_CONFIGURE@

# First target often defines default behavior: all
# We follow up with another pass to `make doc` because our wild recipes
# sometimes preclude generating all of them on the first pass (FIXME!)
# missing e.g. PDF and HTML which then pop up in `make check` footprint,
# or misses a .prep-src-docs stage needed to pattern-make man page files
# with some "make" implementations...
all all-am-local all-local: all-recursive
	+@$(MAKE) $(AM_MAKEFLAGS) doc
	+@$(MAKE) $(AM_MAKEFLAGS) doc

# include directory for aclocal
ACLOCAL_AMFLAGS = -I m4

# subdirectories to build and distribute. The order matters, as
# several subdirectories depend on stuff in "common" or tools being built first
SUBDIRS = include common clients conf data docs drivers tools \
  lib scripts server tests

bindir = @bindir@
sbindir = @sbindir@
driverexecdir = @driverexecdir@
cgiexecdir = @cgiexecdir@

# Automatically update the libtool script if it becomes out-of-date
# See https://www.gnu.org/software/libtool/manual/html_node/LT_005fINIT.html
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

# COPYING and other autotools-standard files are included automatically
# by automake. Note that the INSTALL file is (re-)imposed by autotools
# runs and is essentially a manual on configure script general usage, so
# NUT's actual installation notes have had to use a different filename.
EXTRA_DIST = LICENSE-GPL2 LICENSE-GPL3 LICENSE-DCO MAINTAINERS

# Since the renaming of documentation to `*.adoc` extension to help IDE
# and GitHub UIs to render the source files in a pretty fashion, we need
# to list them:
EXTRA_DIST += INSTALL.nut.adoc UPGRADING.adoc TODO.adoc NEWS.adoc README.adoc

# Tarballs created by `make dist` include the `configure.ac` and `m4/*` sources
# but lack NUT magic logic to recreate the `configure` script if someone would
# want to adapt it to their autotools or locally fix a tarball-based build.
EXTRA_DIST += autogen.sh

if KEEP_NUT_REPORT
nodist_data_DATA = config.nut_report_feature.log
endif

# ----------------------------------------------------------------------
# flags to pass to ./configure when calling "make distcheck" and "make
# distcheck-light". Try to check as many features as possible! Also
# need to give augeas-lenses-dir, hotplug-dir and udev-dir, and request
# PyNUT to be installed near the NUT-Monitor app (if feasible) so that
# staged install does not fail. Note that by default PyNUT tries to go
# into the system Python site-packages location, and autotools does not
# tweak paths not using ${prefix} so `make distcheck` fails for it as
# it does not play with a `DESTDIR` either.

DISTCHECK_FLAGS = --with-all --with-ssl --with-doc=auto --with-pynut=app --with-nut_monitor=force CXXFLAGS='@NUT_CONFIG_CXXFLAGS@' CFLAGS='@NUT_CONFIG_CFLAGS@' CPPFLAGS='@NUT_CONFIG_CPPFLAGS@' LDFLAGS='@NUT_CONFIG_LDFLAGS@'
DISTCHECK_LIGHT_FLAGS = --with-all=auto --with-ssl=auto --with-doc=auto --with-pynut=app --with-nut_monitor=force CXXFLAGS='@NUT_CONFIG_CXXFLAGS@' CFLAGS='@NUT_CONFIG_CFLAGS@' CPPFLAGS='@NUT_CONFIG_CPPFLAGS@' LDFLAGS='@NUT_CONFIG_LDFLAGS@'
DISTCHECK_LIGHT_MAN_FLAGS = --with-all=auto --with-ssl=auto --with-doc=man --with-pynut=app --with-nut_monitor=force CXXFLAGS='@NUT_CONFIG_CXXFLAGS@' CFLAGS='@NUT_CONFIG_CFLAGS@' CPPFLAGS='@NUT_CONFIG_CPPFLAGS@' LDFLAGS='@NUT_CONFIG_LDFLAGS@'
DISTCHECK_VALGRIND_FLAGS = --with-all=auto --with-ssl=auto --with-doc=skip --with-valgrind CXXFLAGS='@NUT_CONFIG_CXXFLAGS@ -g' CFLAGS='@NUT_CONFIG_CFLAGS@ -g' CPPFLAGS='@NUT_CONFIG_CPPFLAGS@' LDFLAGS='@NUT_CONFIG_LDFLAGS@' --with-pynut=app --with-nut_monitor=force

# Note: this rule uses envvar DISTCHECK_FLAGS expanded at run-time
DISTCHECK_CONFIGURE_FLAGS = ${DISTCHECK_FLAGS}		\
 PKG_CONFIG_PATH='@PKG_CONFIG_PATH@'			\
 --with-systemdsystemunitdir='$${prefix}/lib/systemd/system' \
 --with-systemdsystempresetdir='$${prefix}/usr/lib/systemd/system-preset' \
 --with-systemdshutdowndir='$${prefix}/lib/systemd/system-shutdown' \
 --with-systemdtmpfilesdir='$${prefix}/usr/lib/tmpfiles.d' \
 --with-augeas-lenses-dir='$${prefix}/usr/share/augeas/lenses'		\
 --with-hotplug-dir='$${prefix}/etc/hotplug'		\
 --with-udev-dir='$${prefix}/etc/udev'			\
 --with-devd-dir='$${prefix}/etc/devd'			\
 --with-pynut=app --with-nut_monitor=force

# Note: trickery with prefix below is needed to expand it from
# DISTCHECK_CONFIGURE_FLAGS defaults defined above in a manner
# that is meaningful for sub-make program (gets stripped away
# otherwise and breaks custom distchecks).
distcheck-light:
	+prefix='$${prefix}'; $(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_LIGHT_FLAGS)" distcheck

distcheck-light-man:
	+prefix='$${prefix}'; $(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_LIGHT_MAN_FLAGS)" distcheck

if HAVE_VALGRIND
# Make a distcheck (and check in particular) with enabled valgrind and debug info
memcheck distcheck-valgrind:
	@echo "See also scripts/valgrind in NUT sources for a helper tool"
	+prefix='$${prefix}'; $(MAKE) $(AM_MAKEFLAGS) DISTCHECK_FLAGS="$(DISTCHECK_VALGRIND_FLAGS)" distcheck
else !HAVE_VALGRIND
memcheck distcheck-valgrind:
	@echo "See also scripts/valgrind in NUT sources for a helper tool"
	@echo "SKIPPED $@ : valgrind was not detected on this system by configure script" >&2
endif !HAVE_VALGRIND

# workaround the dist generated files that are also part of the distribution
# Note that distcleancheck is disabled for now, while waiting for a proper
# solution, that do not break older unix systems
#distcleancheck_listfiles = \
#	find . -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'
distcleancheck:
	@:

# Quick alias for root dir recipe:
realclean: maintainer-clean

# Files made by our targets:
CLEANFILES = *-spellchecked *.adoc-parsed cppcheck*.xml config.log.inplace-outer
DISTCLEANFILES = ChangeLog

# Most of the files generated by custom rules in the configure script
# or by autogen.sh are cleaned by the Makefile.am in their directories.
# Files below are re-created by running `configure` script and may be
# wiped by a `make distclean`:
DISTCLEANFILES += config.log configure~
#???# configure.ac~
DISTCLEANFILES += include/config.h.in~

# Files made by autotools and common rituals of the configure script,
# these are needed to run the configure script itself so are not wiped
# by a mere `make distclean`; most of these are copied by autotools
# from their installation, or made by `automake` etc. on the system
# which generates `configure`; rebuilding NUT after deleting these
# requires `autogen.sh` script to be re-run (and tools available):
MAINTAINERCLEANFILES = INSTALL
MAINTAINERCLEANFILES += aclocal.m4 config.guess config.sub
MAINTAINERCLEANFILES += configure
MAINTAINERCLEANFILES += depcomp install-sh ltmain.sh test-driver ar-lib
MAINTAINERCLEANFILES += m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4
MAINTAINERCLEANFILES += Makefile.in .dirstamp include/config.h.in

# Executed after default rules
maintainer-clean-local:
	$(AM_V_at)rm -f missing || true

# Do not let $SUBDIRS/Makefile rules delete their local .deps because
# this breaks our ability to clean up (e.g. some common/.../*.Plo files
# are included by generated Makefiles from other subdirectories, so they
# should be available during their clean-up). Just in case, we make sure
# here that their sub-distcleans complete first.
distclean-local:
	+@for DIR in $(SUBDIRS) ; do \
		if test -f "$${DIR}/Makefile" ; then \
			echo "  DISTCLEAN  in $${DIR}" >&2 ; \
			( cd "$${DIR}" && $(MAKE) $(AM_MAKEFLAGS) -s distclean ) || exit ; \
		fi ; \
	 done
	$(AM_V_at)rm -rf .inst tmp autom4te.cache
	$(AM_V_at)find "$(builddir)" -type d -name '.deps' | while read DIR ; do rm -rf "$${DIR}" ; done

# Hook the documentation building and validating recipes
# Note: these are optionally available (as determined during configure runs)
# Maint: grep -l 'SPELLCHECK_' `git grep -lw spellcheck '*.am'`
spellcheck spellcheck-interactive:
	+@RES=0; \
	(cd $(builddir)/docs && $(MAKE) $(AM_MAKEFLAGS) -s $(abs_top_builddir)/docs/.prep-src-docs) || RES=$$? ; \
	(cd $(builddir)/docs/man && $(MAKE) $(AM_MAKEFLAGS) -s $(abs_top_builddir)/docs/man/.prep-src-docs) || RES=$$? ; \
	(cd $(builddir)/docs && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/docs/man && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/conf && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/data && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/data/html && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/Solaris && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/Windows && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/devd && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/hotplug && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/installer && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/python && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/systemd && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/udev && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/scripts/upsdrvsvcctl && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	(cd $(builddir)/tests/NIT && $(MAKE) $(AM_MAKEFLAGS) -s $@) || RES=$$? ; \
	exit $$RES

# Note: the "all-docs" and "check-docs" targets may require tools not
# found by `configure` script (and so avoided by conventional recipes)
# such as PDF generators, so it should only be called at developer's
# discretion, choice and risk. The "check-man" targets covers source
# texts, man pages and HTML rendering of man pages, as enabled by tools.
doc spellcheck-sortdict spellcheck-report-dict-usage \
all-docs check-docs \
man all-man man-man check-man html-man all-html:
	+cd $(abs_top_builddir)/docs && $(MAKE) $(AM_MAKEFLAGS) -s $(abs_top_builddir)/docs/.prep-src-docs
	+cd $(abs_top_builddir)/docs/man && $(MAKE) $(AM_MAKEFLAGS) -s $(abs_top_builddir)/docs/man/.prep-src-docs
	+cd $(abs_top_builddir)/docs && $(MAKE) $(AM_MAKEFLAGS) $@

INSTALL.nut UPGRADING NEWS README:
	+cd $(abs_top_builddir)/docs && $(MAKE) $(AM_MAKEFLAGS) ../$(@F).adoc-parsed && cp -f ../$(@F).adoc-parsed ../$(@F)

# Workarounds for https://github.com/github/markup/issues/1095
# require direct definition of our attributes in each source
# document, in order for GitHub Web-UI to render them nicely
# (unfortunately, asciidoc configs and includes are not handled
# at this time). Hopefully this will go away at some point.
# The following rule updates definitions in source asciidoc files
# between GH_MARKUP_1095_INCLUDE_BEGIN/END tags with contents of
# current docs/asciidoc-vars.conf file. It is intended to be used
# by maintainers (or brave contributors who would dare edit those
# definitions), to apply them into the committed document sources.
# Not bothering about with "make dist" constraints etc. - changes
# the contents of srcdir directly and intentionally.
# NOTE: There is a problem with `printf '%s' "${LINE}"` (double-quoted)
# that causes backslashes to be treated as escape characters, and
# shell replacements with `${VAR/pat/sub}` are not really portable.
# To address this, we use the pure POSIX shell `replace_all()`
# method suggested at https://stackoverflow.com/a/75037170/4715872
# WARNING: It may succumb to lines ending with asterisk however!
MAINTAINER_ASCIIDOCS_RECIPE_DEBUG_STREAM = /dev/null
#MAINTAINER_ASCIIDOCS_RECIPE_DEBUG_STREAM = &2

maintainer-asciidocs:
	@USEDREV="`git log -1 --oneline --pretty=format:'%h (%cs) %s' docs/asciidoc-vars.conf`" || exit ; \
	 USEDREV_NOSUBJ="`git log -1 --oneline --pretty=format:'%h (%cs)' docs/asciidoc-vars.conf`" || exit ; \
	 echo "$@: Updating asciidoc text sources with docs/asciidoc-vars.conf as of commit: $${USEDREV}"; \
	 echo "//GH_MARKUP_1095_INCLUDE_BEGIN//$${USEDREV}" > docs/asciidoc-vars.conf.lastrev.tmp || exit ; \
	 replace_all() { \
	   case "$$1" in *"$$2"*) ;; *) echo "$$1" ; return ;; esac ; \
	   RIGHT="$$1" ; R=''; \
	   while [ -n "$$RIGHT" ]; do \
	    echo "LEFT='$$LEFT' RIGHT='$$RIGHT' => R='$$R'" >$(MAINTAINER_ASCIIDOCS_RECIPE_DEBUG_STREAM) ; \
	    LEFT="$${RIGHT%%$$2*}" ; \
	    echo "=> LEFT='$$LEFT'" >$(MAINTAINER_ASCIIDOCS_RECIPE_DEBUG_STREAM) ; \
	    if [ x"$$LEFT" = x"$$RIGHT" ]; then \
	        R="$$R$$RIGHT" ; \
	        return ; \
	    fi ; \
	    R="$$R$$LEFT$$3" ; \
	    RIGHT="$${RIGHT#*$$2}" ; \
	  done ; \
	  echo "$$R" ; \
	 } ; \
	 find . -name '*.adoc' -or -name '*.txt' | ( \
	  FILES=""; \
	  while read F ; do \
	    grep -E '^//+GH_MARKUP_1095_INCLUDE_(BEGIN|END)' "$$F" >/dev/null \
	    || { echo "$@: SKIP: no GH_MARKUP_1095_INCLUDE_* tags: $$F"; continue ; } ; \
	    rm -f "$${F}"*.tmp || exit ; \
	    EXT="1.tmp"; \
	    while IFS='' read LINE ; do \
	        case "$${LINE}" in \
	            "//GH_MARKUP_1095_INCLUDE_BEGIN"*) EXT="2.tmp" ; continue ;; \
	            "//GH_MARKUP_1095_INCLUDE_END"*|"////GH_MARKUP_1095_INCLUDE_END"*) EXT="3.tmp" ; continue ;; \
	        esac ; \
	        printf '%s\n' "`replace_all "$${LINE}" '\\' '''\\'`" >> "$${F}.$${EXT}" || exit ; \
	    done < "$$F" || { echo "$@: FAILED injection for $${F}" >&2; exit 1; } ; \
	    if test -s "$${F}.2.tmp" && test -z "`diff "$${F}.2.tmp" docs/asciidoc-vars.conf | tr -d '\n'`" ; then \
	        rm -f "$${F}"*.tmp ; \
	        echo "$@: SKIP: no changes: $$F"; continue ; \
	    fi; \
	    cat "$${F}.1.tmp" docs/asciidoc-vars.conf.lastrev.tmp docs/asciidoc-vars.conf > "$${F}.tmp" \
	    && echo '//GH_MARKUP_1095_INCLUDE_END//' >> "$${F}.tmp" \
	    && cat "$${F}.3.tmp" >> "$${F}.tmp" \
	    && mv -f "$${F}.tmp" "$${F}" \
	    || { echo "$@: FAILED injection for $${F}" >&2; exit 1; } ; \
	    echo "$@: UPDATED: $$F"; \
	    FILES="$${FILES} $${F}"; \
	    rm -f "$${F}"*.tmp ; \
	  done; \
	  rm -f docs/asciidoc-vars.conf.lastrev.tmp; \
	  if test -z "$${FILES}" ; then \
	    echo "$@: OVERALL-SKIP: No text files found with GH_MARKUP_1095_INCLUDE_ tags, or obsoleted docs/asciidoc-vars.conf contents";\
	  else \
	    echo "$@: OVERALL-UPDATED: You may now want to:"; \
	    echo "    git add -p $${FILES} && git commit -sm 'Update NUT documentation sources with current docs/asciidoc-vars.conf: $${USEDREV_NOSUBJ}'"; \
	  fi; \
	 )

check-NIT check-NIT-devel check-NIT-sandbox check-NIT-sandbox-devel:
	+cd $(builddir)/tests/NIT && $(MAKE) $(AM_MAKEFLAGS) $@

VERSION_DEFAULT: dummy-stamp
	@abs_top_srcdir='$(abs_top_srcdir)' ; \
	 abs_top_builddir='$(abs_top_builddir)' ; \
	 export abs_top_srcdir ; export abs_top_builddir ; \
	 NUT_VERSION_QUERY=UPDATE_FILE '$(abs_top_srcdir)/tools/gitlog2version.sh'

CLEANFILES += VERSION_DEFAULT.tmp
EXTRA_DIST += VERSION_DEFAULT

# Best-effort delivery for (overly?) customized distros, e.g. via
#   echo NUT_VERSION_FORCED_SEMVER=1.1.1 > VERSION_FORCED_SEMVER
dist-hook:
	for D in "$(abs_top_srcdir)" "$(abs_top_builddir)" ; do \
	    for F in VERSION_FORCED VERSION_FORCED_SEMVER ; do \
	        if [ -s "$$D/$$F" ] ; then \
	            cat "$$D/$$F" > "$(top_distdir)/$$F" || true ; \
	        fi ; \
	    done ; \
	done

# This target adds syntax-checking for committed shell script files,
# to avoid surprises and delays in finding fatal typos after packaging
###
### Note: currently, shellcheck target calls check-scripts-syntax
### so when both are invoked at once, in the end the check is only
### executed once. Later it is anticipated that shellcheck would
### be implemented by requiring, configuring and calling the tool
### named "shellcheck" for even more code inspection and details.
### Still, there remains value in also checking the script syntax
### by the very version of the shell interpreter that would run
### these scripts in production usage of the resulting packages.
###
check-scripts-syntax:
	@echo 'NOTE: modern bash complains about scripts using backticks (warning not error), which we ignore in NUT codebase for portability reasons: `...` obsolete, use $$(...)'
	@RUNBASH=bash; if [ -x /bin/bash ] && /bin/bash -c 'echo $${BASH_VERSION}' | grep -E '^[456789]\.' ; then RUNBASH=/bin/bash ; else if [ -x /usr/bin/env ] ; then RUNBASH="/usr/bin/env bash"; fi; fi ; \
	 for F in `git ls-files || find . -type f` ; do \
	    case "`file "$$F"`" in \
	        *"Bourne-Again shell script"*) ( set -x ; $$RUNBASH -n "$$F" ; ) ;; \
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

# Note: currently not part of shellcheck target, because the script below
# can test the logic with numerous SHELL_PROGS in a CI setting, and because
# check-scripts-syntax probably has checked the basic syntax above already.
shellcheck-nde:
	cd $(srcdir)/tests && SERVICE_FRAMEWORK="selftest" ./nut-driver-enumerator-test.sh

shellcheck: shellcheck-disclaimer check-scripts-syntax

CPPCHECK = @CPPCHECK@
if HAVE_CPPCHECK
cppcheck: cppcheck-cxx11.xml cppcheck-c99.xml

# Let the analysis get regenerated due to any change in source;
# but note that with our different make implementations to support,
# we can not either $(shell find ...) nor blindly say e.g. *.cpp
# for each FS structure layer because e.g. there are no ./*.cpp
# in the root dir of the codebase (and so make complains there is
# `No rule to make target `*.cpp', needed by `cppcheck-cxx11.xml'`)
#
# Note that the actual `cppcheck` scan finds all files it likes
# (so if CPPCHECK_SRC_* misses something, it just won't trigger
# automagically a rebuild of the XML in developer working cycles).
CPPCHECK_SRC_H = $(top_srcdir)/*/*.h $(top_srcdir)/*/*/*.h
# CPPCHECK_SRC_H += $(top_srcdir)/*.h

CPPCHECK_SRC_C = $(top_srcdir)/*/*.c $(top_srcdir)/*/*/*.c
# CPPCHECK_SRC_C += $(top_srcdir)/*.cpp

CPPCHECK_SRC_CXX = $(top_srcdir)/*/*.cpp
# CPPCHECK_SRC_CXX += $(top_srcdir)/*.cpp $(top_srcdir)/*/*/*.cpp

cppcheck-cxx11.xml: $(CPPCHECK_SRC_CXX) $(CPPCHECK_SRC_H)
	$(CPPCHECK) --std=c++11 --enable=all --inconclusive --xml --xml-version=2 . 2>$@

cppcheck-c99.xml: $(CPPCHECK_SRC_C) $(CPPCHECK_SRC_H)
	$(CPPCHECK) --std=c99 --enable=all --inconclusive --xml --xml-version=2 . 2>$@
else !HAVE_CPPCHECK
cppcheck:
	@echo "CPPCHECK analysis not available since 'cppcheck' was not found."
endif !HAVE_CPPCHECK

sockdebug:
	+cd $(builddir)/server && $(MAKE) $(AM_MAKEFLAGS) sockdebug$(EXEEXT)

# ----------------------------------------------------------------------
# Automatically generate the ChangeLog from Git logs:
MAINTAINERCLEANFILES += ChangeLog

# CI builds can leave a log of selected features:
MAINTAINERCLEANFILES += config.nut_report_feature.log*

# Older boundary of the ChangeLog commits range
# It can be a tag ('v2.2.0'), a commit hash, a date, ...
# See gitrevisions for more information on specifying ranges
GITLOG_START_POINT=v2.6.0

# Force ChangeLog regeneration upon make dist (due to nonexistant 'dummy-stamp'),
# in case it has already been generated previously
# Note that the script is hard-coded to inspect Git workspace which contains
# the current dir, and defaults to generate a "ChangeLog" in the current dir.
# The script itself is generated from a template, so resides in builddir.
dummy-stamp:
ChangeLog: dummy-stamp
	+@$(MAKE) $(AM_MAKEFLAGS) $(abs_top_builddir)/ChangeLog

if WITH_PDF_NONASCII_TITLES
WITH_PDF_NONASCII_TITLES_ENVVAR = WITH_PDF_NONASCII_TITLES=yes
else
WITH_PDF_NONASCII_TITLES_ENVVAR = WITH_PDF_NONASCII_TITLES=no
endif

# Be sure to not confuse with a DIST'ed file (and so try to overwrite it);
# do however avoid re-generating it if already made on a previous pass and
# the Git HEAD pointer (branch) or its actual "index" or "object" database
# did not change since then - meaning the local developer or CI did not
# modify the metadata (subsequent generation of the huge PDF/HTML files
# can cost dearly).
# Note there's a bit more fuss about Git internals which NUT should not
# really care about encapsulation-wise (detection of NUT_GITDIR location
# which may reside elsewhere, e.g. with local repo clones with reference
# repo configuration, or submodules). But this is a Git-crawling target
# anyway, and in the worst case (Git's design changes) we would spend a
# bit of time researching the FS in vain, and go on to re-generate the
# ChangeLog when maybe we should not have - oh well.
# WARNING: The CHANGELOG_REQUIRE_GROUP_BY_DATE_AUTHOR=true mode here is
# default to allow for prettier documentation, but it can require too much
# memory for weaker build systems. Set it to false when calling make there.
CHANGELOG_REQUIRE_GROUP_BY_DATE_AUTHOR_ENVVAR = true
$(abs_top_builddir)/ChangeLog: tools/gitlog2changelog.py dummy-stamp
	@cd $(abs_top_srcdir) && \
	    if test -e .git ; then \
	        NUT_GITDIR=".git" ; if test -r "$${NUT_GITDIR}" -a ! -d "$${NUT_GITDIR}" ; then GD="`grep -E '^gitdir:' "$${NUT_GITDIR}" | sed 's/^gitdir: *//'`" && test -n "$$GD" -a -d "$$GD" && NUT_GITDIR="$$GD" ; fi ; \
	        if test -s "$@" -a -d "$${NUT_GITDIR}" && test -z "`find "$${NUT_GITDIR}" -newer "$@" 2>/dev/null`" ; then \
	            echo "  DOC-CHANGELOG-GENERATE        $@ : SKIP (keep existing)" ; \
	            echo "Using still-valid ChangeLog file generated earlier from same revision of Git source metadata in '$${NUT_GITDIR}'" >&2 ; \
	        else \
	            echo "  DOC-CHANGELOG-GENERATE        $@" ; \
	            CHANGELOG_FILE="$@" $(WITH_PDF_NONASCII_TITLES_ENVVAR) \
	            CHANGELOG_REQUIRE_GROUP_BY_DATE_AUTHOR="$(CHANGELOG_REQUIRE_GROUP_BY_DATE_AUTHOR_ENVVAR)" \
	              $(abs_top_builddir)/tools/gitlog2changelog.py $(GITLOG_START_POINT) || { \
	                echo "  DOC-CHANGELOG-GENERATE        $@ : FAILED (non-fatal)" >&2 ; \
	                printf "gitlog2changelog.py failed to generate the ChangeLog.\n\nNOTE: See https://github.com/networkupstools/nut/commits/master for change history.\n\n" > "$@" ; \
	            } ; \
	        fi ; \
	    else \
	        if test x"$(abs_top_srcdir)" != x"$(abs_top_builddir)" -a -s ./ChangeLog ; then \
	            echo "  DOC-CHANGELOG-GENERATE        $@ : SKIP (keep existing)" ; \
	            echo "Using distributed ChangeLog file from sources" >&2 ; \
	            rm -f "$@" || true ; \
	            cat ./ChangeLog > "$@" ; \
	        else \
	            if test -s "$@" ; then \
	                echo "  DOC-CHANGELOG-GENERATE        $@ : SKIP (keep existing)" ; \
	                echo "Using distributed ChangeLog file from sources" >&2 ; \
	            else \
	                echo "  DOC-CHANGELOG-GENERATE        $@ : FAILED (non-fatal)" >&2 ; \
	                printf "Failed to generate the ChangeLog.\n\nNOTE: See https://github.com/networkupstools/nut/commits/master for change history.\n\n" > "$@" ; \
	            fi ; \
	        fi ; \
	    fi

ChangeLog.adoc: ChangeLog
	+cd $(abs_top_builddir)/docs && $(MAKE) $(AM_MAKEFLAGS) ../ChangeLog.adoc

nut_version.h include/nut_version.h:
	+cd $(abs_top_builddir)/include && $(MAKE) $(AM_MAKEFLAGS) nut_version.h

tools/gitlog2changelog.py: tools/gitlog2changelog.py.in
	+cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) -s $(@F)

# ----------------------------------------------------------------------
# Maintainers targets: distribution signature and hashes
nut-@PACKAGE_VERSION@.tar.gz: dist
nut-@PACKAGE_VERSION@.tar.gz.sig: dist-sig
nut-@PACKAGE_VERSION@.tar.gz.md5 nut-@PACKAGE_VERSION@.tar.gz.sha256: dist-hash

dist-sig: nut-@PACKAGE_VERSION@.tar.gz
	rm -f nut-@PACKAGE_VERSION@.tar.gz.sig
	gpg --detach-sign nut-@PACKAGE_VERSION@.tar.gz

dist-hash: nut-@PACKAGE_VERSION@.tar.gz
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
	+$(MAKE) $(AM_MAKEFLAGS) all
install-bin:
	@echo $(WARN)
	@echo "Warning: 'make install-bin' is deprecated." 
	@echo "Use 'make install-exec' instead for a similar effect."
	@echo $(WARN)
	+cd common;  $(MAKE) $(AM_MAKEFLAGS) install
	+cd drivers; $(MAKE) $(AM_MAKEFLAGS) install
	+cd server;  $(MAKE) $(AM_MAKEFLAGS) install
	+cd clients; $(MAKE) $(AM_MAKEFLAGS) install
install-man: install-data-recursive
	@echo $(WARN)
	@echo "Warning: 'make install-man' is deprecated."
	@echo "Use 'cd docs/man; make install' instead."
	@echo $(WARN)
	+cd docs/man; $(MAKE) $(AM_MAKEFLAGS) install
install-conf:
	@echo $(WARN)
	@echo "Warning: 'make install-conf' is deprecated."
	@echo "Use 'cd conf; make install' instead."
	@echo $(WARN)
	+cd conf; $(MAKE) $(AM_MAKEFLAGS) install
# The target install-data already has a standardized meaning under automake
install-dirs:
	@echo $(WARN)
	@echo "Warning: 'make install-dirs' is deprecated."
	@echo "Use 'make installdirs' instead."
	@echo $(WARN)
	+$(MAKE) $(AM_MAKEFLAGS) installdirs
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

# Adjust permissions when installing as `root` into the actual system.
# We honour DESTDIR anyway, as someone can install into a chroot etc.
# NOTE: Might be an 'install-data-hook' (for dirs) and/or 'install-exec-hook'
# (for service restart) but better not force this on everyone?
# It is also up to the end-user making such an installation to remove (or not)
# dirs and files made below.
# To err on the safe side in cross builds, we ignore Windows builds and those
# not built for the same system as the build host.
install-data-hook:
	@case "@target_os@" in *mingw*) exit 0;; esac ; \
	 if [ x"@host_os@" != x"@build_os@" ]; then exit 0 ; fi ; \
	 if [ x"@target_os@" != x"@build_os@" ]; then exit 0 ; fi ; \
	 if (command -v id) && [ x"`id -u`" = x0 ] && [ x"$(DESTDIR)" = x -o x"$(DESTDIR)" = x/ ] ; then \
		echo "================================================================================" >&2 ; \
		echo "| NUT data files have been installed into the system, now consider running     |" >&2 ; \
		echo "| '(sudo) make install-as-root' to apply permissions and service state changes |" >&2 ; \
		echo "================================================================================" >&2 ; \
	 fi

if HAVE_SYSTEMD
HAVE_SYSTEMD = true
else
HAVE_SYSTEMD = false
endif

if WITH_SYSTEMD_TMPFILES
WITH_SYSTEMD_TMPFILES = true
else
WITH_SYSTEMD_TMPFILES = false
endif

if WITH_SYSTEMD_PRESET
WITH_SYSTEMD_PRESET = true
else
WITH_SYSTEMD_PRESET = false
endif

if WITH_CGI
WITH_CGI = true
else
WITH_CGI = false
endif

if WITH_SOLARIS_SMF
WITH_SOLARIS_SMF = true
else
WITH_SOLARIS_SMF = false
endif

if WITH_SOLARIS_INIT
WITH_SOLARIS_INIT = true
else
WITH_SOLARIS_INIT = false
endif

# TODO: Actually move this into scripts like Solaris/postinstall
# using OS-specific `useradd`/`groupadd`, etc.
# Note that as we stop services, we may be dealing with (older)
# distros that do not follow current naming in NUT code base.
install-as-root:
	@+echo "$@: starting (no-op if not root)" >&2 ; \
	 case "@target_os@" in *mingw*) exit 0;; esac ; \
	 if [ x"@host_os@" != x"@build_os@" ]; then exit 0 ; fi ; \
	 if [ x"@target_os@" != x"@build_os@" ]; then exit 0 ; fi ; \
	 prefix="@prefix@"; \
	 if (command -v id) && [ x"`id -u`" = x0 ] ; then \
		if [ x"$(DESTDIR)" = x -o x"$(DESTDIR)" = x/ ] ; then \
			if $(HAVE_SYSTEMD) ; then \
				echo "$@: Stop NUT services, if any" >&2 ; \
				@SYSTEMD_SYSTEMCTL_PROGRAM@ stop nut-monitor.service nut-server.service || true ; \
				@SYSTEMD_SYSTEMCTL_PROGRAM@ stop nut-driver.service || true ; \
				@SYSTEMD_SYSTEMCTL_PROGRAM@ stop nut-driver.target || true ; \
				@SYSTEMD_SYSTEMCTL_PROGRAM@ stop nut.target || true ; \
			fi ; \
			if $(WITH_SOLARIS_SMF) || $(WITH_SOLARIS_INIT) ; then \
				if $(WITH_SOLARIS_SMF) ; then \
					echo "$@: Stop NUT services, if any" >&2 ; \
					SMF_ACTIVE="`/usr/bin/svcs -a -Hostate,fmri | grep svc:/system/power/ | grep -v disabled | awk '{print $$2}'`" ; \
					for S in $$SMF_ACTIVE ; do \
						/usr/sbin/svcadm disable -ts $$S || true ; \
					done ; \
				fi ; \
				$(top_builddir)/scripts/Solaris/preremove \
				|| exit ; \
			fi ; \
		fi ; \
		$(MAKE) $(AM_FLAGS) DESTDIR="$(DESTDIR)" install || exit ; \
		if [ x"$(DESTDIR)" = x -o x"$(DESTDIR)" = x/ ] ; then \
			if $(WITH_SOLARIS_SMF) || $(WITH_SOLARIS_INIT) ; then \
				$(top_builddir)/scripts/Solaris/preinstall && \
				$(top_builddir)/scripts/Solaris/postinstall ; \
				exit ; \
			fi ; \
		fi ; \
		echo "  MKDIR   $(DESTDIR)/@STATEPATH@ $(DESTDIR)/@STATEPATH@/upssched" >&2 ; \
		$(MKDIR_P) "$(DESTDIR)/@STATEPATH@/upssched" && \
		for D in "@PIDPATH@" "@ALTPIDPATH@" "@ALTSTATEPATH@" "@CONFPATH@" ; do \
			case x"$$D" in \
			x|x@*) ;; \
			*)	echo "  MKDIR   $(DESTDIR)/$$D" >&2 ; \
				$(MKDIR_P) "$(DESTDIR)/$$D" \
				|| exit ;; \
			esac ; \
		done ; \
		if (command -v chmod) ; then \
			echo "  CHMOD(0770)   $(DESTDIR)/@STATEPATH@/upssched" >&2 ; \
			chmod 0770 "$(DESTDIR)/@STATEPATH@/upssched" \
			|| exit ; \
			for D in "@STATEPATH@" "@PIDPATH@" "@ALTPIDPATH@" "@ALTSTATEPATH@" ; do \
				case x"$$D" in \
				x|x@*|x/run|x/var/run|x/tmp|x/var/tmp|x/dev/shm|x/etc|x/var|x/usr|x/usr/local|x/usr/local/etc|x/usr/etc) ;; \
				*)	echo "  CHMOD(0770)   $(DESTDIR)/$$D" >&2 ; \
					chmod 0770 "$(DESTDIR)/$$D" \
					|| exit ;; \
				esac ; \
			done ; \
			case x"@CONFPATH@" in \
				x|x@*|x/run|x/var/run|x/tmp|x/var/tmp|x/dev/shm|x/etc|x/var|x/usr|x/usr/local|x/usr/local/etc|x/usr/etc) ;; \
				*)	echo "  CHMOD(0751)   $(DESTDIR)/@CONFPATH@" >&2 ; \
					chmod 0751 "$(DESTDIR)/@CONFPATH@" \
					|| exit ;; \
			esac ; \
			for F in hosts.conf.sample upsstats-single.html.sample upsstats.html.sample upsset.conf.sample ; do \
				echo "  CHMOD(0644)   CGI: $(DESTDIR)/@CONFPATH@/$$F" >&2 ; \
				chmod 0644 "$(DESTDIR)/@CONFPATH@/$$F" \
				|| { if $(WITH_CGI) ; then exit 1 ; else true ; fi ; } ; \
			done ; \
			for F in nut.conf.sample ups.conf.sample upsd.conf.sample upsd.users.sample upsmon.conf.sample upssched.conf.sample ; do \
				echo "  CHMOD(0640)   $(DESTDIR)/@CONFPATH@/$$F" >&2 ; \
				chmod 0640 "$(DESTDIR)/@CONFPATH@/$$F" \
				|| exit ; \
			done ; \
		else \
			echo "$@: WARNING: Can not CHMOD created locations!" >&2 ; \
		fi ; \
		if (command -v chown) && test 0 -lt "`id -u '@RUN_AS_USER@'`" \
		&& ( test 0 -lt "`getent group '@RUN_AS_GROUP@' | awk -F: '{print $$3}'`" || test 0 -lt "`id -g '@RUN_AS_GROUP@'`" ) \
		; then \
			echo "  CHOWN(@RUN_AS_USER@:@RUN_AS_GROUP@)   $(DESTDIR)/@STATEPATH@/upssched" >&2 ; \
			chown "@RUN_AS_USER@:@RUN_AS_GROUP@" "$(DESTDIR)/@STATEPATH@/upssched" \
			|| exit ; \
			for D in "@STATEPATH@" "@PIDPATH@" "@ALTPIDPATH@" "@ALTSTATEPATH@" ; do \
				case x"$$D" in \
				x|x@*|x/run|x/var/run|x/tmp|x/var/tmp|x/dev/shm|x/etc|x/var|x/usr|x/usr/local|x/usr/local/etc|x/usr/etc) ;; \
				*)	echo "  CHOWN(@RUN_AS_USER@:@RUN_AS_GROUP@)   $(DESTDIR)/$$D" >&2 ; \
					chown "@RUN_AS_USER@:@RUN_AS_GROUP@" "$(DESTDIR)/$$D" \
					|| exit ;; \
				esac ; \
			done ; \
			case x"@CONFPATH@" in \
				x|x@*|x/run|x/var/run|x/tmp|x/var/tmp|x/dev/shm|x/etc|x/var|x/usr|x/usr/local|x/usr/local/etc|x/usr/etc) ;; \
				*)	echo "  CHOWN(root:@RUN_AS_GROUP@)   $(DESTDIR)/@CONFPATH@" >&2 ; \
					chown "root:@RUN_AS_GROUP@" "$(DESTDIR)/@CONFPATH@" \
					|| exit ;; \
			esac ; \
			for F in hosts.conf.sample upsstats-single.html.sample upsstats.html.sample upsset.conf.sample ; do \
				echo "  CHOWN(root:@RUN_AS_GROUP@)   CGI: $(DESTDIR)/@CONFPATH@/$$F" >&2 ; \
				chown "root:@RUN_AS_GROUP@" "$(DESTDIR)/@CONFPATH@/$$F" \
				|| { if $(WITH_CGI) ; then exit 1 ; else true ; fi ; } ; \
			done ; \
			for F in nut.conf.sample ups.conf.sample upsd.conf.sample upsd.users.sample upsmon.conf.sample upssched.conf.sample ; do \
				echo "  CHOWN(root:@RUN_AS_GROUP@)   $(DESTDIR)/@CONFPATH@/$$F" >&2 ; \
				chown "root:@RUN_AS_GROUP@" "$(DESTDIR)/@CONFPATH@/$$F" \
				|| exit ; \
			done ; \
		else \
			echo "$@: WARNING: Can not CHOWN created locations!" >&2 ; \
		fi ; \
		if [ x"$(DESTDIR)" = x -o x"$(DESTDIR)" = x/ ] ; then \
			if $(HAVE_SYSTEMD) ; then \
				echo "$@: Activate default systemd layout, restart services:" >&2 ; \
				if $(WITH_SYSTEMD_TMPFILES) ; then \
					echo "$@: Apply systemd-tmpfiles presets" >&2 ; \
					@SYSTEMD_TMPFILES_PROGRAM@ --create || exit ; \
				fi ; \
				echo "$@: Learn systemd definition changes" >&2 ; \
				@SYSTEMD_SYSTEMCTL_PROGRAM@ daemon-reload || exit ; \
				if $(WITH_SYSTEMD_PRESET) ; then \
					echo "$@: Apply systemd enabled/disabled service presets" >&2 ; \
					@SYSTEMD_SYSTEMCTL_PROGRAM@ preset-all || exit ; \
				else \
					echo "$@: Apply systemd enabled/disabled service defaults" >&2 ; \
					@SYSTEMD_SYSTEMCTL_PROGRAM@ disable nut.target nut-driver.target nut-monitor nut-server nut-driver-enumerator.path nut-driver-enumerator.service || exit ; \
					@SYSTEMD_SYSTEMCTL_PROGRAM@ enable  nut.target nut-driver.target nut-monitor nut-server nut-driver-enumerator.path nut-driver-enumerator.service || exit ; \
				fi ; \
				echo "$@: Reconfigure nut-driver-enumerator (service instance wrapping)" >&2 ; \
				@SYSTEMD_SYSTEMCTL_PROGRAM@ restart udev || true ; \
				$(top_builddir)/scripts/upsdrvsvcctl/nut-driver-enumerator.sh --reconfigure || { RES=$$?; if [ $$RES != 42 ] ; then exit $$RES ; fi ; } ; \
				echo "$@: Restart NUT services" >&2 ; \
				@SYSTEMD_SYSTEMCTL_PROGRAM@ restart nut-driver-enumerator.service nut-monitor.service nut-server.service || exit ; \
			fi ; \
		fi ; \
		echo "$@: finished" >&2 ; \
	 fi

# Clean the dist tarball and packages
MAINTAINERCLEANFILES_DISTBALL = nut-*.tar.gz
# HP-UX:
MAINTAINERCLEANFILES_PACKAGES = NUT_HPUX_package@PACKAGE_VERSION@.depot NUT_HPUX_package-@PACKAGE_VERSION@.depot
# AIX as below, and RedHat-compatible (cover binary and source packages):
MAINTAINERCLEANFILES_PACKAGES += nut*rpm
# Debian-compatible (cover binary and source packages):
MAINTAINERCLEANFILES_PACKAGES += nut*deb
# Solaris SVR4 package archives:
MAINTAINERCLEANFILES_PACKAGES += NUT_solaris_*_package@PACKAGE_VERSION@.local.gz NUT_solaris_*_package-@PACKAGE_VERSION@.local.gz
# Newer Solaris IPS (aka "pkg(5)" format archives)
MAINTAINERCLEANFILES_PACKAGES += *.p5p

MAINTAINERCLEANFILES += $(MAINTAINERCLEANFILES_DISTBALL)
MAINTAINERCLEANFILES += $(MAINTAINERCLEANFILES_PACKAGES)

package: dist
	+DESTDIR="$(abs_builddir)/_install_pkgprotodir" ; export DESTDIR; \
	rm -rf "$$DESTDIR"; \
	case "`uname -s`" in \
	"HP-UX") \
		( cd scripts/HP-UX && \
		  $(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$DESTDIR" package && \
		  mv NUT_HPUX_package.depot $(abs_top_builddir)/NUT_HPUX_package-@PACKAGE_VERSION@.depot ) ;; \
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

if HAVE_WINDOWS
# Steam-roll over all executables/libs we have placed in DESTDIR and copy over
# any resolved dependencies from the cross-build (or native MSYS2) environment.
# Then hardlink libraries for sbin... (alternative: all bins in one dir)
# TOTHINK: Are there more dirs to consider? So far we cover bindir, sbindir and
# driverexecdir (e.g. some Linux distros place drivers to /lib/nut while tools
# and daemons are in /usr/bin and /usr/sbin), and cgiexecdir, and occasional
# helpers like "sockdebug.exe" in libexecdir; anything else?..
# Note we hold existance of cgiexecdir as optional, but the name is expected to
# be defined. Other dirs are "just assumed" to exist (that we are not packaging
# some NUT build without drivers/tools/daemons). Subject to change if needed.
# Currently this is handled by a CHECKING... step that should fail if it hits
# anything.
install-win-bundle: all
	@if test -z "$(DESTDIR)" ; then echo "ERROR: '$@': Bundle may only be installed to some DESTDIR prototype area'" >&2 ; exit 1; fi
	+$(MAKE) $(AM_MAKEFLAGS) DESTDIR='$(DESTDIR)' install
	+$(MAKE) $(AM_MAKEFLAGS) DESTDIR='$(DESTDIR)' install-win-bundle-thirdparty

install-win-bundle-thirdparty:
	@if test -z "$(DESTDIR)" ; then echo "ERROR: '$@': Bundle may only be installed to some DESTDIR prototype area'" >&2 ; exit 1; fi
	@echo "Searching which DLLs need to be bundled with NUT for Windows..." >&2
	@if test -z "$$ARCH" ; then \
	    if test -n "$(target)" ; then \
	        ARCH='$(target)' \
	    ; else \
	        if test -n "$(target_triplet)" ; then ARCH='$(target_triplet)' ; fi ; \
	    fi ; \
	 fi ; \
	 if test -n "$$ARCH" ; then export ARCH ; fi ; \
	 DESTDIR='$(DESTDIR)' ; export DESTDIR ; \
	 (  cd '$(DESTDIR)' || exit ; \
	    DESTDIR="" '$(abs_top_srcdir)/scripts/Windows/dllldd.sh' dllldddir . \
	    | while read D ; do \
	        echo "   DLL->bin       $$D" 2>&1 ; \
	        cp -pf "$$D" './$(bindir)/' ; \
	    done ; \
	 ) || exit ; \
	 (  if test x"$(bindir)" = x"$(sbindir)" ; then exit 0 ; fi ; \
	    cd '$(DESTDIR)/$(sbindir)' || exit ; \
	    '$(abs_top_srcdir)/scripts/Windows/dllldd.sh' dllldddir . \
	    | while read D ; do \
	        echo "   DLL->sbin      $$D" 2>&1 ; \
	        ln -f '$(DESTDIR)/$(bindir)'/"`basename "$$D"`" ./ ; \
	    done ; \
	 ) || exit ; \
	 (  if test x"$(driverexecdir)" = x"$(bindir)" ; then exit 0 ; fi ; \
	    if test x"$(driverexecdir)" = x"$(sbindir)" ; then exit 0 ; fi ; \
	    cd '$(DESTDIR)/$(driverexecdir)' || exit ; \
	    '$(abs_top_srcdir)/scripts/Windows/dllldd.sh' dllldddir . \
	    | while read D ; do \
	        echo "   DLL->drv       $$D" 2>&1 ; \
	        ln -f '$(DESTDIR)/$(bindir)'/"`basename "$$D"`" ./ ; \
	    done ; \
	 ) || exit ; \
	 (  if test -z "$(cgiexecdir)" -o ! -d "$(DESTDIR)/$(cgiexecdir)" ; then exit 0 ; fi ; \
	    if test x"$(cgiexecdir)" = x"$(bindir)" ; then exit 0 ; fi ; \
	    if test x"$(cgiexecdir)" = x"$(sbindir)" ; then exit 0 ; fi ; \
	    if test x"$(driverexecdir)" = x"$(cgiexecdir)" ; then exit 0 ; fi ; \
	    cd '$(DESTDIR)/$(cgiexecdir)' || exit ; \
	    '$(abs_top_srcdir)/scripts/Windows/dllldd.sh' dllldddir . \
	    | while read D ; do \
	        echo "   DLL->cgi       $$D" 2>&1 ; \
	        ln -f '$(DESTDIR)/$(bindir)'/"`basename "$$D"`" ./ ; \
	    done ; \
	 ) || exit ; \
	 (  if test x"$(libexecdir)" = x"$(bindir)" ; then exit 0 ; fi ; \
	    if test x"$(libexecdir)" = x"$(sbindir)" ; then exit 0 ; fi ; \
	    if test x"$(libexecdir)" = x"$(driverexecdir)" ; then exit 0 ; fi ; \
	    if test x"$(libexecdir)" = x"$(cgiexecdir)" ; then exit 0 ; fi ; \
	    cd '$(DESTDIR)/$(libexecdir)' || exit ; \
	    '$(abs_top_srcdir)/scripts/Windows/dllldd.sh' dllldddir . \
	    | while read D ; do \
	        echo "   DLL->libexec   $$D" 2>&1 ; \
	        ln -f '$(DESTDIR)/$(bindir)'/"`basename "$$D"`" ./ ; \
	    done ; \
	 ) || exit
	@echo "CHECKING if any executable files were installed to locations other than those covered by this recipe, so might not have needed DLLs bundled near them" >&2 ; \
	 relbindir="`echo './$(bindir)/' | sed 's,//*,/,g'`" ; \
	 relsbindir="`echo './$(sbindir)/' | sed 's,//*,/,g'`" ; \
	 reldriverexecdir="`echo './$(driverexecdir)/' | sed 's,//*,/,g'`" ; \
	 relcgiexecdir="`echo './$(cgiexecdir)/' | sed 's,//*,/,g'`" ; \
	 rellibexecdir="`echo './$(libexecdir)/' | sed 's,//*,/,g'`" ; \
	 cd '$(DESTDIR)' || exit ; \
	 find . -type f | grep -Ei '\.(exe|dll)$$' \
	 | grep -vE "^($${relbindir}|$${relsbindir}|$${reldriverexecdir}|$${relcgiexecdir}|$${rellibexecdir})" \
	 | ( RES=0 ; while IFS= read LINE ; do echo "$$LINE" ; RES=1; done; exit $$RES )

else
install-win-bundle:
	@echo "SKIP: '$@' not enabled for current build configuration"

install-win-bundle-thirdparty:
	@echo "SKIP: '$@' not enabled for current build configuration"
endif

print-MAINTAINERCLEANFILES print-REALCLEANFILES:
	@echo $(MAINTAINERCLEANFILES)

print-DISTCLEANFILES:
	@echo $(DISTCLEANFILES)

# TODO: Recursive mode to consider patterns defined in sub-dir makefiles
git-realclean-check:
	@if test -e .git && (command -v git); then \
		git status --ignored || while read F ; do \
			for P in $(MAINTAINERCLEANFILES) ; do \
				case "$$F" in \
				*/$$P) exit 1 ;; \
				esac ; \
			done; \
		done ; \
	 fi
