# Network UPS Tools: scripts/installer

EXTRA_DIST = \
	README.adoc \
	make_package.sh	\
	version.sh	\
	install.sh	\
	uninstall-lsnw.sh	\
	uninstall-ipp	\
	nutconf-dummy	\
	common_EN/license.txt	\
	common_EN/install.res	\
	common/solaris_init	\
	common/ipp-shutdown-daemon.sh	\
	common/README_ipp-os-shutdown.adoc	\
	common/ipp-status	\
	common/ipp.conf	\
	common/ipp-wrapper	\
	common/ipp-event.sh	\
	common/string.sh	\
	common/ipp-host-shutdown.sample	\
	common/shutdown	\
	common/ipp-notifier.sh	\
	common/ipp-os-shutdown	\
	common/init	\
	common/aix_init	\
	aix/ipp-os-shutdown.conf.sample	\
	aix/aix_init	\
	hpux/ipp-os-shutdown.conf.sample	\
	solcmn/solaris_init	\
	solcmn/ipp-os-shutdown.conf.sample

SPELLCHECK_SRC = README.adoc common/README_ipp-os-shutdown.adoc

nut:
	rm -f "$@"
	$(LN_S) $(top_srcdir) "$@"

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

# Remove "nut" if it is a symlink to the source tree
clean-local:
	if test -L nut || test -h nut ; then rm -f nut ; fi

MAINTAINERCLEANFILES = Makefile.in .dirstamp
