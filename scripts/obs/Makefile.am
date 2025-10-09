# Network UPS Tools: obs (root)

EXTRA_DIST = README.adoc

# Also EXTRA_DIST the recipe-related files (so OBS can use tarballs if they like)

# Debian/Ubuntu recipes:
EXTRA_DIST += \
	debian.Makefile.am \
	debian.NEWS \
	debian.changelog \
	debian.compat \
	debian.control \
	debian.copyright \
	debian.description.subst \
	debian.gbp.conf \
	debian.hotplug \
	debian.libnutclient-dev.install \
	debian.libnutclient-dev.manpages \
	debian.libnutclient1.install \
	debian.libnutclient1.lintian-overrides \
	debian.libnutclientstub-dev.install \
	debian.libnutclientstub1.install \
	debian.libnutclientstub1.lintian-overrides \
	debian.libnutscan-dev.install \
	debian.libnutscan1.install \
	debian.libups-nut-perl.install \
	debian.libupsclient-dev.install \
	debian.libupsclient-dev.manpages \
	debian.libupsclient4.install \
	debian.nut.changes \
	debian.nut-cgi.README.Debian \
	debian.nut-cgi.docs \
	debian.nut-cgi.install \
	debian.nut-cgi.manpages \
	debian.nut-cgi.postinst \
	debian.nut-cgi.postrm \
	debian.nut-client.init.in \
	debian.nut-client.install \
	debian.nut-client.links \
	debian.nut-client.lintian-overrides \
	debian.nut-client.maintscript \
	debian.nut-client.manpages \
	debian.nut-client.postinst \
	debian.nut-client.preinst \
	debian.nut-client.prerm \
	debian.nut-common.install \
	debian.nut-common.postinst.in \
	debian.nut-common.prerm.in \
	debian.nut-common.tmpfiles.in \
	debian.nut-doc.doc-base.nut-developer-guide \
	debian.nut-doc.doc-base.nut-faq \
	debian.nut-doc.doc-base.nut-packager-guide \
	debian.nut-doc.doc-base.nut-user-manual \
	debian.nut-doc.install \
	debian.nut-ipmi.install \
	debian.nut-ipmi.manpages \
	debian.nut-linux-i2c.install \
	debian.nut-linux-i2c.manpages \
	debian.nut-modbus.install \
	debian.nut-modbus.manpages \
	debian.nut-monitor.install \
	debian.nut-monitor.menu \
	debian.nut-monitor.xpm \
	debian.nut-powerman-pdu.install \
	debian.nut-powerman-pdu.manpages \
	debian.nut-server.dirs \
	debian.nut-server.init.in \
	debian.nut-server.install \
	debian.nut-server.manpages \
	debian.nut-server.postinst \
	debian.nut-server.postrm \
	debian.nut-server.preinst \
	debian.nut-server.prerm.in \
	debian.nut-snmp.docs \
	debian.nut-snmp.install \
	debian.nut-snmp.manpages \
	debian.nut-xml.install \
	debian.nut-xml.manpages \
	debian.nut.README.Debian \
	debian.nut.TODO.Debian \
	debian.nut.docs \
	debian.python-nut.install \
	debian.rules \
	debian.series \
	debian.watch \
	nut.dsc

# RedHAT/Fedora/CentOS/... recipes:
EXTRA_DIST += \
	nut.spec

# PKGSRC recipes:
EXTRA_DIST += \
	pkgsrc.obs

SPELLCHECK_SRC = README.adoc

# NOTE: Due to portability, we do not use a GNU percent-wildcard extension.
# We also have to export some variables that may be tainted by relative
# paths when parsing the other makefile (e.g. MKDIR_P that may be defined
# via expanded $(top_builddir)/install-sh):
#%-spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#	+$(MAKE) $(AM_MAKEFLAGS) -s -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

# NOTE: Portable suffix rules do not allow prerequisites, so we shim them here
# by a wildcard target in case the make implementation can put the two together.
*-spellchecked: Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)

.sample.sample-spellchecked:
	+$(MAKE) $(AM_MAKEFLAGS) -s -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

.in.in-spellchecked:
	+$(MAKE) $(AM_MAKEFLAGS) -s -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict:
	+$(MAKE) $(AM_MAKEFLAGS) -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC="$(SPELLCHECK_SRC)" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

CLEANFILES = *-spellchecked

MAINTAINERCLEANFILES = Makefile.in .dirstamp
