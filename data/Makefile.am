# Network UPS Tools: data

SUBDIRS = html

dist_data_DATA = cmdvartab
nodist_data_DATA = driver.list
EXTRA_DIST = evolution500.seq epdu-managed.dev

# NOTE: Due to portability, we do not use a GNU percent-wildcard extension:
#%-spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#	$(MAKE) -s -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC_ONE="$<" SPELLCHECK_DIR="$(srcdir)" $@

cmdvartab-spellchecked: cmdvartab Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
	$(MAKE) -s -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC_ONE="$<" SPELLCHECK_DIR="$(srcdir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict:
	$(MAKE) -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC="cmdvartab" SPELLCHECK_DIR="$(srcdir)" $@

DISTCLEANFILES = Makefile.in
CLEANFILES = *.pdf *.html *-spellchecked
