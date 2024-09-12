# Network UPS Tools: conf

INSTALL_0600 = $(INSTALL) -m 0600

SECFILES = upsd.conf.sample upsd.users.sample
PUBFILES = nut.conf.sample ups.conf.sample
CGIPUB = hosts.conf.sample upsset.conf.sample upsstats.html.sample	\
 upsstats-single.html.sample

if WITH_CGI
 CGI_INSTALL = $(CGIPUB)
else
 CGI_INSTALL = 
endif

dist_sysconf_DATA = $(SECFILES) $(PUBFILES) $(CGI_INSTALL)
nodist_sysconf_DATA = upssched.conf.sample upsmon.conf.sample

SPELLCHECK_SRC = $(dist_sysconf_DATA) \
 upssched.conf.sample.in upsmon.conf.sample.in

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


MAINTAINERCLEANFILES = Makefile.in .dirstamp
CLEANFILES = *.pdf *.html *-spellchecked
