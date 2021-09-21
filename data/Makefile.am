# Network UPS Tools: data

SUBDIRS = html

dist_data_DATA = cmdvartab
nodist_data_DATA = driver.list
EXTRA_DIST = evolution500.seq epdu-managed.dev

clean-local:
	rm -f *.pdf *.html *.spellchecked

%.spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
	$(MAKE) -s -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC="$<" SPELLCHECK_DIR="$(srcdir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict:
	$(MAKE) -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC="cmdvartab" SPELLCHECK_DIR="$(srcdir)" $@
