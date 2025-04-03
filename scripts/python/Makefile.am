# Network UPS Tools: scripts/python

SUBDIRS = module

# Recognize settings from configure.ac (for make install handling)
nut_with_nut_monitor = @nut_with_nut_monitor@
nut_with_nut_monitor_dir = @nut_with_nut_monitor_dir@
nut_with_nut_monitor_py2gtk2 = @nut_with_nut_monitor_py2gtk2@
nut_with_nut_monitor_py3qt5 =  @nut_with_nut_monitor_py3qt5@

# Note: this may be "desktop-file-install" to use in e.g.
# packaging postinstall scripts for tighter OS integration;
# note also the icon files, etc. that may want symlinks to
# system-defined locations. For examples please see e.g.
# https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=nut-monitor
#nut_with_nut_monitor_desktop = @nut_with_nut_monitor_desktop@

nut_with_pynut = @nut_with_pynut@
nut_with_pynut_py =  @nut_with_pynut_py@
nut_with_pynut_py2 = @nut_with_pynut_py2@
nut_with_pynut_py3 = @nut_with_pynut_py3@

PYTHON_SITE_PACKAGES =  @PYTHON_SITE_PACKAGES@
PYTHON2_SITE_PACKAGES = @PYTHON2_SITE_PACKAGES@
PYTHON3_SITE_PACKAGES = @PYTHON3_SITE_PACKAGES@

BINDIR = @BINDIR@

NUT_MONITOR_PY2GTK2 = \
	app/ui/gui-1.3.glade \
	app/nut-monitor-py2gtk2.desktop

NUT_MONITOR_PY2GTK2_TEMPLATE = \
	app/NUT-Monitor-py2gtk2.in

NUT_MONITOR_PY2GTK2_GENERATED_SCRIPT = \
	app/NUT-Monitor-py2gtk2

NUT_MONITOR_PY3QT5 = \
	app/ui/aboutdialog1.ui \
	app/ui/dialog1.ui \
	app/ui/dialog2.ui \
	app/ui/window1.ui \
	app/nut-monitor-py3qt5.desktop

NUT_MONITOR_PY3QT5_TEMPLATE = \
	app/NUT-Monitor-py3qt5.in

NUT_MONITOR_PY3QT5_GENERATED_SCRIPT = \
	app/NUT-Monitor-py3qt5

NUT_MONITOR_COMMON = \
	README.adoc \
	app/nut-monitor.appdata.xml \
	app/icons/48x48/nut-monitor.png \
	app/icons/64x64/nut-monitor.png \
	app/icons/256x256/nut-monitor.png \
	app/icons/scalable/nut-monitor.svg \
	app/README.adoc \
	app/screenshots/nut-monitor-1.png \
	app/screenshots/nut-monitor-2.png \
	app/screenshots/nut-monitor-3.png \
	app/pixmaps/on_battery.png \
	app/pixmaps/on_line.png \
	app/pixmaps/var-ro.png \
	app/pixmaps/var-rw.png \
	app/pixmaps/warning.png \
	app/locale/fr/LC_MESSAGES/NUT-Monitor.mo \
	app/locale/it/LC_MESSAGES/NUT-Monitor.mo \
	app/locale/ru/LC_MESSAGES/NUT-Monitor.mo

# Reserved for shipping additional modules or scripts "as is"
PYNUT_COMMON_CODE =

PYNUT_COMMON_MISC = \
	module/README.adoc

PYNUT_COMMON = $(PYNUT_COMMON_CODE) $(PYNUT_COMMON_MISC)

# Note: we both distribute and install the generated *.mo translation files
# so they are listed above and not in NUT_MONITOR_COMMON_TEMPLATE
NUT_MONITOR_COMMON_TEMPLATE = \
	app/locale/NUT-Monitor.pot \
	app/locale/fr/fr.po \
	app/locale/it/it.po \
	app/locale/ru/ru.po


# Craft locale subdirs with "xx_YY.UTF-8" patterned names,
# and similarly alias the "*.po" files inside, as needed
# by some platforms but not others to find these files.
install-data-hook-app-locale-symlinks:
	@cd "$(DESTDIR)$(nutmonitordir)/app/locale" && \
	 for L in fr it ru ; do \
	    $(LN_S) "$$L".po "$$L"/"$$L"_"`echo "$$L" | tr '[a-z]' '[A-Z]'`".UTF-8.po || exit ; \
	    $(LN_S) "$$L" "$$L"_"`echo "$$L" | tr '[a-z]' '[A-Z]'`".UTF-8 || exit ; \
	 done

uninstall-hook-app-locale-symlinks:
	@RES=0 ; \
	 cd "$(DESTDIR)$(nutmonitordir)/app/locale" && \
	 for L in fr it ru ; do \
	    rm -f "$$L"/"$$L"_"`echo "$$L" | tr '[a-z]' '[A-Z]'`".UTF-8.po || RES=$$? ; \
	    rm -f "$$L"_"`echo "$$L" | tr '[a-z]' '[A-Z]'`".UTF-8 || RES=$$? ; \
	 done ; \
	 exit $$RES

PYNUT_TEMPLATE = \
	module/setup.py.in \
	module/PyNUT.py.in \
	module/test_nutclient.py.in

PYNUT_GENERATED_NOEXEC = \
	module/PyNUT.py

PYNUT_GENERATED_SCRIPT = \
	module/test_nutclient.py

# For now, we have a "dispatcher" script and applet manifest,
# to select a functional choice of the GUI client, if possible:
NUT_MONITOR_DISPATCHER_NOEXEC = \
	app/nut-monitor.desktop
NUT_MONITOR_DISPATCHER_SCRIPT = \
	app/NUT-Monitor

#################################################################
# `make dist` tarball contents:
EXTRA_DIST = \
	$(NUT_MONITOR_DISPATCHER_NOEXEC) $(NUT_MONITOR_DISPATCHER_SCRIPT) \
	$(NUT_MONITOR_COMMON) $(NUT_MONITOR_COMMON_TEMPLATE) \
	$(PYNUT_COMMON) $(PYNUT_TEMPLATE)

EXTRA_DIST += $(NUT_MONITOR_PY2GTK2) $(NUT_MONITOR_PY2GTK2_TEMPLATE)
EXTRA_DIST += $(NUT_MONITOR_PY3QT5) $(NUT_MONITOR_PY3QT5_TEMPLATE)

#################################################################
# `make install` handling (nobase_ to keep tree structure):
# Make py2/py3-only builds, delivered preferred symlinks, etc. optional
if WITH_NUT_MONITOR
nutmonitordir = $(nut_with_nut_monitor_dir)
nobase_nutmonitor_DATA = $(NUT_MONITOR_DISPATCHER_NOEXEC) $(NUT_MONITOR_COMMON)
nobase_nutmonitor_SCRIPTS = $(NUT_MONITOR_DISPATCHER_SCRIPT)

# Runs after the rest of automake install goals
install-data-hook: install-data-hook-app-locale-symlinks

# Not separated into data/exec
# TOTHINK: Do we only want to call this recipe when uninstalling a NUT build
#  that could have installed NUT-Monitor in the first place, or in any case?
uninstall-hook: uninstall-hook-app-locale-symlinks

if HAVE_MSGFMT
# Note lack of "$<" below - it is a non-portable GNU Make extension
# The POT-Creation-Date: is removed by current python gettext builder to avoid
# "spurious" changes that do not benefit (otherwise unmodified) contents; see:
#   https://github.com/sphinx-doc/sphinx/pull/3490
#   https://github.com/sphinx-doc/sphinx/issues/3443
# Note that OUTFILE may be in builddir (not necessarily same as srcdir)
ACT_MSGFMT = { \
	$(GREP) -v -E '^.?POT-Creation-Date: ' < "$${SRCFILE}" > "$${OUTFILE}.tmpsrc" && \
	$(MSGFMT) -o "$${OUTFILE}" "$${OUTFILE}.tmpsrc" && \
	rm -f "$${OUTFILE}.tmpsrc" ; \
}

app/locale/fr/LC_MESSAGES/NUT-Monitor.mo: $(abs_builddir)/app/locale/fr/LC_MESSAGES/NUT-Monitor.mo
$(abs_builddir)/app/locale/fr/LC_MESSAGES/NUT-Monitor.mo: app/locale/fr/fr.po
	@$(MKDIR_P) "$(builddir)/app/locale/fr/LC_MESSAGES"
	SRCFILE="$?"; OUTFILE="$@"; $(ACT_MSGFMT)

app/locale/it/LC_MESSAGES/NUT-Monitor.mo: $(abs_builddir)/app/locale/it/LC_MESSAGES/NUT-Monitor.mo
$(abs_builddir)/app/locale/it/LC_MESSAGES/NUT-Monitor.mo: app/locale/it/it.po
	@$(MKDIR_P) "$(builddir)/app/locale/it/LC_MESSAGES"
	SRCFILE="$?"; OUTFILE="$@"; $(ACT_MSGFMT)

app/locale/ru/LC_MESSAGES/NUT-Monitor.mo: $(abs_builddir)/app/locale/ru/LC_MESSAGES/NUT-Monitor.mo
$(abs_builddir)/app/locale/ru/LC_MESSAGES/NUT-Monitor.mo: app/locale/ru/ru.po
	@$(MKDIR_P) "$(builddir)/app/locale/ru/LC_MESSAGES"
	SRCFILE="$?"; OUTFILE="$@"; $(ACT_MSGFMT)
endif HAVE_MSGFMT

if WITH_NUT_MONITOR_PY2GTK2
nobase_nutmonitor_DATA += $(NUT_MONITOR_PY2GTK2)
nobase_nutmonitor_SCRIPTS += $(NUT_MONITOR_PY2GTK2_GENERATED_SCRIPT)
endif WITH_NUT_MONITOR_PY2GTK2

if WITH_NUT_MONITOR_PY3QT5
nobase_nutmonitor_DATA += $(NUT_MONITOR_PY3QT5)
nobase_nutmonitor_SCRIPTS += $(NUT_MONITOR_PY3QT5_GENERATED_SCRIPT)
endif WITH_NUT_MONITOR_PY3QT5

if WITH_PYNUT_APP
nobase_nutmonitor_DATA += $(PYNUT_COMMON) $(PYNUT_GENERATED_NOEXEC)
nobase_nutmonitor_SCRIPTS += $(PYNUT_GENERATED_SCRIPT)
endif WITH_PYNUT_APP

sysbindir = $(BINDIR)
sysbin_SCRIPTS = NUT-Monitor

# Dummy redirector for /usr/bin/... presence
NUT-Monitor: Makefile
	@echo '#!/bin/sh' > "$@"
	@echo 'exec "$(nutmonitordir)/app/NUT-Monitor" "$$@"' >> "$@"

endif WITH_NUT_MONITOR


# These are dumped into site-packages directly, right?
if WITH_PYNUT_PY
pynut_py_sitedir = $(PYTHON_SITE_PACKAGES)
pynut_py_site_DATA = $(PYNUT_GENERATED_NOEXEC)
pynut_py_site_SCRIPTS = $(PYNUT_GENERATED_SCRIPT) $(PYNUT_COMMON_CODE)
endif


if WITH_PYNUT_PY2
pynut_py2_sitedir = $(PYTHON2_SITE_PACKAGES)
pynut_py2_site_DATA = $(PYNUT_GENERATED_NOEXEC)
pynut_py2_site_SCRIPTS = $(PYNUT_GENERATED_SCRIPT) $(PYNUT_COMMON_CODE)
endif


if WITH_PYNUT_PY3
pynut_py3_sitedir = $(PYTHON3_SITE_PACKAGES)
pynut_py3_site_DATA = $(PYNUT_GENERATED_NOEXEC)
pynut_py3_site_SCRIPTS = $(PYNUT_GENERATED_SCRIPT) $(PYNUT_COMMON_CODE)
endif

#################################################################

SPELLCHECK_SRC = \
	README.adoc \
	app/README.adoc \
	module/README.adoc

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

CLEANFILES = *-spellchecked */*-spellchecked

#################################################################

MAINTAINERCLEANFILES = Makefile.in .dirstamp

clean-local:
	$(AM_V_at)rm -rf *.pyc __pycache__ */*.pyc */__pycache__ */*/*.pyc */*/__pycache__
	$(AM_V_at)rm -f NUT-Monitor module/setup.py
