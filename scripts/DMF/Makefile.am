# GNU Make syntax assumed below

RM ?= /usr/bin/rm -f
# Take compiler from PATH, may be wrapped with ccache etc.
CC = gcc
CPP = gcc
CXX = g++

AM_CFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include \
    -I$(top_srcdir)/drivers -I$(top_srcdir)/tools/nut-scanner

# Note: this causes a private build of dmf-reindex with debugging etc. which
# may be slightly different from the distributed `nut-scanner-reindex-dmfsnmp`
PROGS = dmf-test dmf-reindex
PROGS_EXPERIMENTAL = dmf-lua-test

DMFSNMP_SUBDIR = dmfsnmp
DMFNUTSCAN_SUBDIR = dmfnutscan

# Automake installation
dmfsnmpdir = @dmfsnmpdir@
dmfsnmp_DATA = $(wildcard $(DMFSNMP_SUBDIR)/*.dmf)

dmfnutscandir = @dmfnutscandir@
dmfnutscan_DATA = $(wildcard $(DMFNUTSCAN_SUBDIR)/*.dmf)

EXTRA_DIST = $(dmfsnmp_DATA) $(dmfnutscan_DATA)

# This does not rely on automake semantics, so this Makefile.am can be directly
# used and changed during development without reconfiguring all of NUT sources
DMFTEST_SRC = dmf-test.c
EXTRA_DIST += $(DMFTEST_SRC)

# First target is default
all: progs dmf

clean: clean-local

clean-local:
	$(RM) $(PROGS) $(DMFSNMP_SUBDIR)/*.o \
		$(DMFSNMP_SUBDIR)/*.dmf.tmp $(DMFSNMP_SUBDIR)/*.json.tmp \
		$(DMFSNMP_SUBDIR)/*_TEST.c $(DMFSNMP_SUBDIR)/*_TEST.exe

check: check-local

check-local: progs-all run-dmf-test run-dmf-lua-test

# Recipes to build DMF files with the sources and tools we have.
# NOTE: At a later stage, when the legacy ways are deprecated and %=*-mib.c are
# removed, these DMF files would become redistributable contents by themselves.
$(DMFSNMP_SUBDIR)/%-mib.dmf: $(abs_top_srcdir)/drivers/%-mib.c dmfify-mib.sh jsonify-mib.py xmlify-mib.py
	mkdir -p $(@D)
	cd $(@D) && $(abs_srcdir)/dmfify-mib.sh $<

# We can not touch read-only source dirs (e.g. during distcheck) so make sure
# that we use builddir for this final touch. The DMF filess themselves may be
# prebuilt and distributed as source, the gmake VPATH should figure that out.
$(DMFSNMP_SUBDIR)/.uptodate: $(builddir)/$(DMFSNMP_SUBDIR)/.uptodate
$(builddir)/$(DMFSNMP_SUBDIR)/.uptodate: $(addprefix $(DMFSNMP_SUBDIR)/,$(patsubst %.c,%.dmf,$(notdir $(wildcard $(abs_top_srcdir)/drivers/*-mib.c))))
	@echo "DMFSNMP_SUBDIR is now up to date, made of: $^" >&2
	mkdir -p $(@D)
	touch $@

$(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf: $(DMFSNMP_SUBDIR)/.uptodate dmf-reindex
	mkdir -p $(@D)
	cd $(@D) && $(abs_srcdir)/dmf-reindex > $(@F)

$(DMFNUTSCAN_SUBDIR)/.uptodate: $(builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate
$(builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate: $(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf
	@echo "DMFNUTSCAN_SUBDIR is now up to date" >&2
	mkdir -p $(@D)
	touch $@

# The *_DATA rules above refer to these strings, satisfy them
$(DMFSNMP_SUBDIR)/*.dmf: $(DMFSNMP_SUBDIR)/.uptodate
$(DMFNUTSCAN_SUBDIR)/*.dmf: $(DMFNUTSCAN_SUBDIR)/.uptodate

dmf: $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate

progs: $(PROGS)

progs-all: $(PROGS) $(PROGS_EXPERIMENTAL)

# Note: These recipes are explicit so they can be built without configuring NUT
dmf-test: $(DMFTEST_SRC) $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/str.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon $(AM_CFLAGS) \
	-DDEBUG=1 -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"$(abs_top_builddir)/scripts/DMF/$(DMFSNMP_SUBDIR)\"" \
	-o $@ $^

dmf-reindex: $(top_srcdir)/tools/nut-scanner/nut-scanner-reindex-dmfsnmp.c $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/str.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon $(AM_CFLAGS) \
	-DDEBUG=1 -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"$(abs_top_builddir)/scripts/DMF/$(DMFSNMP_SUBDIR)\"" \
	-o $@ $^

dmf-lua-test: $(DMFTEST_SRC) $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/str.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	-D_FORTIFY_SOURCE=2 -O -fstack-protector \
	-lneon $(AM_CFLAGS) \
	-llua5.1 -I/usr/include/lua5.1 -DWITH_DMF_LUA=1 \
	-DDEBUG=1 -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"$(abs_top_builddir)/scripts/DMF/$(DMFSNMP_SUBDIR)\"" \
	-o $@ $^

run-dmf-test: dmf-test
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full $(builddir)/$<

run-dmf-reindex: dmf-reindex
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full $(builddir)/$<

run-dmf-lua-test: dmf-lua-test
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full --track-origins=yes $(builddir)/$<
