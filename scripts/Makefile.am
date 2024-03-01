# Network UPS Tools: scripts (root)

EXTRA_DIST = \
    README.adoc \
    avahi/nut.service.in \
    HP-UX/nut-drvctl \
    HP-UX/nut-drvctl.sh \
    HP-UX/nut-upsd \
    HP-UX/nut-upsd.sh \
    HP-UX/nut-upsmon \
    HP-UX/nut-upsmon.sh \
    logrotate/nutlogd \
    misc/nut.bash_completion \
    misc/osd-notify \
    perl/Nut.pm \
    RedHat/halt.patch \
    RedHat/README.adoc \
    RedHat/ups.in \
    RedHat/upsd.in \
    RedHat/upsmon.in \
    Solaris8/S99upsmon \
    subdriver/gen-usbhid-subdriver.sh \
    subdriver/gen-snmp-subdriver.sh \
    upower/95-upower-hid.hwdb \
    upower/95-upower-hid.rules \
    usb_resetter/README.adoc \
    usb_resetter/nut-driver.service \
    Windows/halt.c \
    Windows/Makefile

SUBDIRS = augeas devd hotplug installer python systemd udev ufw Solaris Windows upsdrvsvcctl

SPELLCHECK_SRC = README.adoc RedHat/README.adoc usb_resetter/README.adoc

# NOTE: Due to portability, we do not use a GNU percent-wildcard extension.
# We also have to export some variables that may be tainted by relative
# paths when parsing the other makefile (e.g. MKDIR_P that may be defined
# via expanded $(top_builddir)/install-sh):
#%-spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#	+$(MAKE) -s -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

# NOTE: Portable suffix rules do not allow prerequisites, so we shim them here
# by a wildcard target in case the make implementation can put the two together.
*-spellchecked: Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)

.sample.sample-spellchecked:
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

.in.in-spellchecked:
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE="$<" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict:
	+$(MAKE) -f $(top_builddir)/docs/Makefile MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC="$(SPELLCHECK_SRC)" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

CLEANFILES = *-spellchecked RedHat/*-spellchecked usb_resetter/*-spellchecked

MAINTAINERCLEANFILES = Makefile.in .dirstamp
