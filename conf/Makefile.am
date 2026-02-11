# Network UPS Tools: conf
# (annotated sample configuration files)

# FIXME: Make use of this for SECFILES installation (hook?)
INSTALL_0600 = $(INSTALL) -m 0600

# Note: ups.conf is a secured file, because for networked devices
# it can contain SNMP, NetXML, IPMI or similar credentials
SECFILES_STATIC = upsd.conf.sample upsd.users.sample ups.conf.sample
SECFILES_GENERATED = upsmon.conf.sample
PUBFILES_STATIC = nut.conf.sample
PUBFILES_GENERATED = upssched.conf.sample
CGIPUB_STATIC = hosts.conf.sample upsset.conf.sample
CGIPUB_GENERATED = upsstats.html.sample upsstats-single.html.sample \
        upsstats-modern-list.html.sample upsstats-modern-single.html.sample

SECFILES = $(SECFILES_STATIC) $(SECFILES_GENERATED)

if WITH_CGI
 CGI_INSTALL_STATIC = $(CGIPUB_STATIC)
 CGI_INSTALL_GENERATED = $(CGIPUB_GENERATED)
else
 CGI_INSTALL = 
 CGI_INSTALL_GENERATED = 
endif

conf_examplesdir = @CONFPATH_EXAMPLES@

dist_conf_examples_DATA = $(SECFILES_STATIC) $(PUBFILES_STATIC) $(CGI_INSTALL_STATIC)
nodist_conf_examples_DATA = $(SECFILES_GENERATED) $(PUBFILES_GENERATED) \
 $(CGI_INSTALL_GENERATED)

SPELLCHECK_SRC = $(dist_sysconf_DATA) \
 upssched.conf.sample.in upsmon.conf.sample.in \
 upsstats-modern-list.html.sample.in upsstats-modern-single.html.sample.in \
 upsstats.html.sample.in upsstats-single.html.sample.in

# NOTE: Due to portability, we do not use a GNU percent-wildcard extension.
# We also have to export some variables that may be tainted by relative
# paths when parsing the other makefile (e.g. MKDIR_P that may be defined
# via expanded $(top_builddir)/install-sh):
#%-spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT) @dotMAKE@
#	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

# NOTE: Portable suffix rules do not allow prerequisites, so we shim them here
# by a wildcard target in case the make implementation can put the two together.
*-spellchecked: Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)

.sample.sample-spellchecked: @dotMAKE@
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

.in.in-spellchecked: @dotMAKE@
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict: @dotMAKE@
	+$(MAKE) -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC="$(SPELLCHECK_SRC)" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@


MAINTAINERCLEANFILES = Makefile.in .dirstamp
CLEANFILES = *.pdf *.html *-spellchecked
