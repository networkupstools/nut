RM ?= /usr/bin/rm -f
# Take compiler from PATH, may be wrapped with ccache etc.
CC = gcc
CPP = gcc
CXX = g++

AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/drivers \
    -I$(top_srcdir)/tools/nut-scanner

# Note: this causes a private build of dmf-reindex with debugging etc. which
# may be slightly different from the distributed `nutdmfsnmp-reindex` program
PROGS = dmf-test dmf-reindex
PROGS_EXPERIMENTAL = dmf-lua-test

DMFSNMP_SUBDIR = dmfsnmp
DMFNUTSCAN_SUBDIR = dmfnutscan

# Automake installation
dmfsnmpdir = $(datadir)/@PACKAGE@/dmfsnmp
dmfsnmp_DATA = $(DMFSNMP_SUBDIR)/*.dmf

dmfnutscandir = $(datadir)/@PACKAGE@/dmfnutscan
dmfnutscan_DATA = $(DMFNUTSCAN_SUBDIR)/*.dmf

EXTRA_DIST = $(dmfsnmp_DATA) $(dmfnutscan_DATA)

# First target is default
all: progs dmf

clean: clean-local

clean-local:
	$(RM) $(PROGS) *.o *.dmf.tmp *.json.tmp *_TEST.c *_TEST.exe

check: check-local

check-local: progs-all run-dmf-test run-dmf-lua-test

# Recipes to build DMF files with the sources and tools we have.
# NOTE: At a later stage, when the legacy method is deprecated and *-mib.c are
# removed, these DMF files would become redistributable contents by themselves.
$(DMFSNMP_SUBDIR)/%.dmf: $(top_srcdir)/drivers/%.c dmfify-mib.sh jsonify-mib.py xmlify-mib.py
	cd $(@D) && $(srcdir)/dmfify-mib.sh $<

$(DMFSNMP_SUBDIR)/.uptodate: $(patsubst %.c,$(DMFSNMP_SUBDIR)/%.dmf,$(notdir $(top_srcdir)/*-mib.c))
	touch $@

$(DMFSNMP_SUBDIR)/*.dmf: $(DMFSNMP_SUBDIR)/.uptodate

$(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf: $(DMFSNMP_SUBDIR)/.uptodate dmf-reindex
	cd $(@D) && $(srcdir)/dmf-reindex > $@

$(DMFNUTSCAN_SUBDIR)/.uptodate: $(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf
	touch $@

$(DMFNUTSCAN_SUBDIR)/*.dmf: $(DMFNUTSCAN_SUBDIR)/.uptodate

dmf: $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate

progs: $(PROGS)

progs-all: $(PROGS) $(PROGS_EXPERIMENTAL)

dmf-test: dmf-test.c $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/src.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon $(AM_CFLAGS) \
	-DDEBUG=1 \
	-o $@ $^

dmf-reindex: $(top_srcdir)/clients/nutdmfsnmp-reindex.c $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/src.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon $(AM_CFLAGS) \
	-DDEBUG=1 \
	-o $@ $^

dmf-lua-test: dmf-test.c $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/src.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon $(AM_CFLAGS) \
	-llua5.1 -I/usr/include/lua5.1 -DWITH_DMF_LUA=1 \
	-DDEBUG=1 \
	-o $@ $^

run-dmf-test: dmf-test
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full $(builddir)/$<

run-dmf-reindex: dmf-reindex
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full $(builddir)/$<

run-dmf-lua-test: dmf-lua-test
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full --track-origins=yes $(builddir)/$<
