# Network UPS Tools: data

SUBDIRS = html

dist_data_DATA = cmdvartab
nodist_data_DATA = driver.list
EXTRA_DIST = evolution500.seq epdu-managed.dev

spellcheck spellcheck-interactive:
	$(MAKE) -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC="cmdvartab" $@
