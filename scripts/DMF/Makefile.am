# Rules and recipes to build DMF-related files, e.g.:
# * regenerate the *-mib.dmf from the legacy *-mib.c sources
# * generate the dmfnutscan-snmp.dmf from existing DMF files
# * build and run some test programs to verify that DMF code works

# TODO: GNU Make syntax may be assumed below; fix this to be portable to other make's

# Note that some include files may be generated, so we prefer a builddir
# and then srcdir - just in case these differ
DMFTOOLS_CPPFLAGS = -I$(abs_top_builddir) -I$(abs_top_srcdir) \
    -I$(abs_top_builddir)/include -I$(abs_top_srcdir)/include \
    -I$(abs_top_builddir)/drivers -I$(abs_top_srcdir)/drivers \
    -I$(abs_top_builddir)/tools/nut-scanner -I$(abs_top_srcdir)/tools/nut-scanner
DISTCLEANFILES =
DISTCLEANDIRS =
CLEAN_FILES =
EXTRA_DIST =

# Note: this causes a private build of dmf-reindex with debugging etc. which
# may be slightly different from the distributed `nut-scanner-reindex-dmfsnmp`
PROGS = dmf-test dmf-reindex
PROGS_EXPERIMENTAL = dmf-lua-test

DMFSNMP_SUBDIR = dmfsnmp
DMFNUTSCAN_SUBDIR = dmfnutscan

# This generated file defines LEGACY_NUT_C_MIBS and LEGACY_NUT_DMFS lists
# based on existing C sources for ../../drivers/*-mib.c
DISTCLEANFILES += legacy-mibfiles-list.mk legacy-mibfiles-list.mk.in
# Use "-include" to avoid errors in automake (the file is not yet available
# in builddir at this moment)
-include legacy-mibfiles-list.mk

# Automake installation
# NOTE: If you provide your own DMF files that are not generated from "*-mib.c"
# existing sources, use some filename pattern that does not end in "-mib.dmf"!
dmfsnmpdir = @dmfsnmpdir@
# Note that LEGACY_NUT_DMFS list includes the DMFSNMP_SUBDIR subdirectory.
dmfsnmp_DMFS = $(LEGACY_NUT_DMFS)
# dmfsnmp_DMFS += $(YOUR_CUSTOM_COMPILED_DMFS)
# dmfsnmp_DMFS += $(YOUR_CUSTOM_VERBATIM_DMFS)
dmfsnmp_DATA = $(dmfsnmp_DMFS) $(LEGACY_NUT_DMF_SYMLINKS) dmfsnmp.xsd

dmfnutscandir = @dmfnutscandir@
DMFNUTSCAN_SYMLINK = $(DMFNUTSCAN_SUBDIR)/S95_dmfnutscan-snmp.dmf
dmfnutscan_DATA = $(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf $(DMFNUTSCAN_SYMLINK) dmfnutscan.xsd

# Do not extra-dist these, to cause distcheck rebuilds and avoid copy-conflicts
#EXTRA_DIST += $(dmfsnmp_DATA) $(dmfnutscan_DATA)
EXTRA_DIST += dmfsnmp.xsd dmfnutscan.xsd
DISTCLEANFILES += $(LEGACY_NUT_DMFS) $(LEGACY_NUT_DMF_SYMLINKS) $(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf $(DMFNUTSCAN_SYMLINK)
DISTCLEANDIRS += $(DMFSNMP_SUBDIR) $(DMFNUTSCAN_SUBDIR)
CLEAN_FILES += $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate $(DMFSNMP_SUBDIR)/.validated $(DMFNUTSCAN_SUBDIR)/.validated

# This does not rely on automake semantics, so this Makefile.am can be directly
# used and changed during development without reconfiguring all of NUT sources
DMFTEST_SRC = dmf-test.c
EXTRA_DIST += $(DMFTEST_SRC)

# First target is default
all: progs dmf

clean: clean-local

CLEAN_FILES += $(PROGS) $(PROGS_EXPERIMENTAL) $(DMFSNMP_SUBDIR)/*.o \
		$(DMFSNMP_SUBDIR)/*.dmf.tmp $(DMFSNMP_SUBDIR)/*.json.tmp \
		$(DMFSNMP_SUBDIR)/*_TEST.c $(DMFSNMP_SUBDIR)/*_TEST.exe

clean-local:
	$(RM) $(CLEAN_FILES)

check: check-local

# Note: this is automake "if"
if HAVE_XMLLINT
  check-local: progs run-dmf-test dmf-validate
else
  check-local: progs run-dmf-test
endif

check-experimental: progs-all check-local run-dmf-lua-test

# Recipes to build DMF files with the sources and tools we have are defined
# via $(DMFGEN_CMD) as used in legacy-mibfiles-list.mk for example.
# NOTE: At a later stage, when the legacy ways are deprecated and %=*-mib.c are
# removed, these DMF files would become redistributable contents by themselves.
DMFGEN_DEPS = $(srcdir)/dmfify-mib.sh $(srcdir)/jsonify-mib.py $(srcdir)/xmlify-mib.py $(srcdir)/nut_cpp
EXTRA_DIST += $(DMFGEN_DEPS)
DMFGEN_CMD = ( \
	mkdir -p $(@D) || exit ; \
	if $(abs_srcdir)/dmfify-mib.sh --sanity-check; then \
	    cd $(@D) && CFLAGS="$(DMFTOOLS_CPPFLAGS) $(CFLAGS) $(AM_CFLAGS)" CPPFLAGS="$(DMFTOOLS_CPPFLAGS) $(CPPFLAGS) $(AM_CPPFLAGS)" $(abs_srcdir)/dmfify-mib.sh $< || exit; \
	else echo "SKIP: NOT REBUILDING DMF file $< due to absence of prerequisites like Python or pycparser" >&2; \
	fi; \
	)

# We can not touch read-only source dirs (e.g. during distcheck) so make sure
# that we use builddir for this final touch. The DMF files themselves may be
# prebuilt and distributed as source, the gmake VPATH should figure that out.
# This may cause harmless "circular dependency" notices during in-tree builds.
# We try to use the .PHONY target to quiesce the warnings below.
# However, the dmf-reindexer is currently hardcoded to use only one directory,
# so we copy stuff below just in case (if needed).
.PHONY: $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate $(DMFSNMP_SUBDIR)/.validated $(DMFNUTSCAN_SUBDIR)/.validated
$(DMFSNMP_SUBDIR)/.uptodate: $(builddir)/$(DMFSNMP_SUBDIR)/.uptodate
$(builddir)/$(DMFSNMP_SUBDIR)/.uptodate: $(dmfsnmp_DMFS)
	@echo "DMFSNMP_SUBDIR content is now up to date, made of: $^" >&2
	mkdir -p $(@D)
	@if [ "`cd $(abs_builddir) && pwd`" != "`cd $(abs_srcdir) && pwd`" ]; then \
		for F in "$(abs_srcdir)/$(DMFSNMP_SUBDIR)/"*.dmf ; do \
			B="`basename "$$F"`"; \
			[ "$$B" = '*.dmf' ] && exit 0 ; \
			N="$(abs_builddir)/$(DMFSNMP_SUBDIR)/$$B"; \
			[ -s "$$F" ] && [ ! -s "$$N" ] && \
				echo "  CP    $$F => $$N" >&2 && \
				cp -f "$$F" "$$N" || exit ; \
		done; \
	fi
	touch $@

# The recipe assumes we have separate opening and closing lines in the DMF file
# just generated by reindexer (the "<?xml...>\n<nut>" and "</nut>" lines) and a
# randomly ordered set of mib2nut lines that we want consistent (to get no
# `git diff` complaints).
# Note that parsing a directory with DMF files only considers 'S*.dmf' names
# TODO: The proper solution (sort, uniq) should be in the C code!
$(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf: $(DMFSNMP_SUBDIR)/.uptodate dmf-reindex
	mkdir -p $(@D)
	@cd "$(abs_builddir)/$(DMFSNMP_SUBDIR)" && \
		for F in *.dmf ; do case "$$F" in \
			S*|K*) ;; \
			ietf-mib.dmf)	echo "  LN     $$F => S90_$$F" >&2 && \
				ln -fs "$$F" "S90_$$F" || exit ;; \
			*)	echo "  LN     $$F => S10_$$F" >&2 && \
				ln -fs "$$F" "S10_$$F" || exit ;; \
		esac; done
	$(abs_builddir)/dmf-reindex > $@.tmp
	( LANG=C; LC_ALL=C; TZ=UTC; export LANG LC_ALL TZ; \
	  egrep -v '^$$|\<mib2nut ' "$@.tmp" | head -2 ; \
	  grep '<mib2nut ' "$@.tmp" | sort | uniq ; \
	  egrep -v '^$$|\<mib2nut ' "$@.tmp" | tail -1 ; \
	) > $@
	@LANG=C; LC_ALL=C; TZ=UTC; export LANG LC_ALL TZ; \
	  sort "$@.tmp" | egrep -v '^$$' | uniq > "$@.tmp1" ; \
	    sort "$@" | egrep -v '^$$' | uniq > "$@.tmp2" ; \
	  diff "$@.tmp1" "$@.tmp2" >/dev/null \
	|| { echo "FAILED to sort generated $@ without losses!" >&2; \
	     rm -f "$@.tmp"* ; exit 1; } ; exit 0
	rm -f "$@.tmp"*
	@cd "$(abs_builddir)/$(DMFNUTSCAN_SUBDIR)" && \
		for F in *.dmf ; do case "$$F" in \
			S*|K*) ;; \
			dmfnutscan-snmp.dmf)	echo "  LN     $$F => S95_$$F" >&2 && \
				ln -fs "$$F" "S95_$$F" || exit ;; \
			*)	echo "  LN     $$F => S10_$$F" >&2 && \
				ln -fs "$$F" "S10_$$F" || exit ;; \
		esac; done

# TODO: Currently the recipe relies on the two files being really in same dir
# Mostly it is here to make the Make happy (dependency for *_DATA above)
$(DMFNUTSCAN_SYMLINK): $(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf
	@cd "$(@D)" && ln -fs "$(<F)" "$(@F)"

$(DMFNUTSCAN_SUBDIR)/.uptodate: $(builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate
$(builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate: $(DMFNUTSCAN_SUBDIR)/dmfnutscan-snmp.dmf
	@echo "DMFNUTSCAN_SUBDIR is now up to date" >&2
	mkdir -p $(@D)
	touch $@

# Validation requires xmllint, which we have if we succeed with asciidoc usage
$(DMFSNMP_SUBDIR)/.validated: $(builddir)/$(DMFSNMP_SUBDIR)/.validated
$(builddir)/$(DMFSNMP_SUBDIR)/.validated: $(builddir)/$(DMFSNMP_SUBDIR)/.uptodate
	@cd $(@D) && for F in *.dmf ; do \
	    echo " XMLLINT  $$F"; \
	    $(XMLLINT) --noout --schema $(abs_srcdir)/dmfsnmp.xsd "$$F" || exit; \
	done

# TODO: Maybe per-DMF *.dmf.validated files would be better?
$(DMFNUTSCAN_SUBDIR)/.validated: $(builddir)/$(DMFNUTSCAN_SUBDIR)/.validated
$(builddir)/$(DMFNUTSCAN_SUBDIR)/.validated: $(builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate
	@ERRCODE=0; cd $(@D) && for F in *.dmf ; do \
	    echo " XMLLINT  $$F"; \
	    $(XMLLINT) --noout --schema $(abs_srcdir)/dmfnutscan.xsd "$$F" || ERRCODE=1; \
	done; exit $$ERRCODE

# Make sure all generated DMFs are up to date vs. C sources and recipes
dmf-uptodate: $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate

# Make sure all present DMFs are valid vs. XSD schema
dmf-validate: dmf-uptodate $(DMFSNMP_SUBDIR)/.validated $(DMFNUTSCAN_SUBDIR)/.validated

# Note: this is automake "if"
if HAVE_XMLLINT
  dmf: dmf-validate
else
  dmf: dmf-uptodate
endif

LUA_INCLUDE ?= -I/usr/include/lua5.1
CFLAGS_LUA ?= $(LUA_INCLUDE)
LDFLAGS_LUA ?= $(LUA_LIB)
if WITH_DMF_LUA
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=1 $(CFLAGS_LUA)
 LDFLAGS_DMF_LUA = $(LDFLAGS_LUA)
else
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=0
 LDFLAGS_DMF_LUA =
endif

if WITH_LIBLTDL
  dmfsnmp_ltdlXneon_LDADD = $(LIBLTDL_LIBS)
  dmfsnmp_ltdlXneon_CFLAGS = $(LIBLTDL_CFLAGS) -DWITH_LIBLTDL=1
else
if WITH_NEON
  dmfsnmp_ltdlXneon_LDADD = $(LIBNEON_LIBS)
  dmfsnmp_ltdlXneon_CFLAGS = $(LIBNEON_CFLAGS) -DWITH_LIBLTDL=0
endif
endif


progs: $(PROGS)

progs-all: $(PROGS) $(PROGS_EXPERIMENTAL)

# Note: These recipes are explicit and do not use NUT libs,
# so they can be built quickly without configuring NUT
dmf-test: $(DMFTEST_SRC) $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/str.c $(top_srcdir)/common/common.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	 -D_FORTIFY_SOURCE=2 -O -fstack-protector \
	 $(dmfsnmp_ltdlXneon_LDADD) $(dmfsnmp_ltdlXneon_CFLAGS) \
	 $(DMFTOOLS_CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(CFLAGS_DMF_LUA) $(LDFLAGS_DMF_LUA) \
	 -DWITH_DMFMIB=1 -DDEBUG=1 \
	 -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"$(abs_top_builddir)/scripts/DMF/$(DMFSNMP_SUBDIR)\"" \
	 -o $@ $^

dmf-reindex: $(top_srcdir)/tools/nut-scanner/nut-scanner-reindex-dmfsnmp.c $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/common.c $(top_srcdir)/common/str.c 
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	 -D_FORTIFY_SOURCE=2 -O -fstack-protector \
	 $(dmfsnmp_ltdlXneon_LDADD) $(dmfsnmp_ltdlXneon_CFLAGS) \
	 $(DMFTOOLS_CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(CFLAGS_DMF_LUA) $(LDFLAGS_DMF_LUA) \
	 -DWITH_DMFMIB=1 -DDMFREINDEXER_MAKECHECK=1 -DDEBUG=1 \
	 -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"$(abs_top_builddir)/scripts/DMF/$(DMFSNMP_SUBDIR)\"" \
	 -o $@ $^

# Note: This uses CFLAGS_LUA not CFLAGS_DMF_LUA so the test bits can build
# even if the general project does not want to build DMF+LUA at this moment
if WITH_LUA
dmf-lua-test: $(DMFTEST_SRC) $(top_srcdir)/common/dmfsnmp.c $(top_srcdir)/common/str.c $(top_srcdir)/common/common.c
	$(CC) -ggdb -std=c11 -std=gnu99 -Werror -Wall -pedantic -Wc++-compat \
	 -D_FORTIFY_SOURCE=2 -O -fstack-protector \
	 $(dmfsnmp_ltdlXneon_LDADD) $(dmfsnmp_ltdlXneon_CFLAGS) \
	 $(DMFTOOLS_CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) \
	 -DWITH_DMF_LUA=1 $(CFLAGS_LUA) $(LDFLAGS_LUA) \
	 -DWITH_DMFMIB=1 -DDEBUG=1 \
	 -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"$(abs_top_builddir)/scripts/DMF/$(DMFSNMP_SUBDIR)\"" \
	 -o $@ $^
else
dmf-lua-test:
	@echo "  SKIP   LUA support was not enabled or detected during project configuration, can not build dmf-lua-test program" >&2
endif

# Note: DMF dirs are currently hardcoded (above) so no big need to "cd" anywhere
# But some data should be prepopulated there before we run leak tests etc.
run-dmf-test: dmf-test $(DMFSNMP_SUBDIR)/.uptodate
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full $(abs_builddir)/$<

run-dmf-reindex: dmf-reindex $(DMFSNMP_SUBDIR)/.uptodate
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full $(abs_builddir)/$<

if WITH_LUA
run-dmf-lua-test: dmf-lua-test $(DMFSNMP_SUBDIR)/.uptodate
	cd $(DMFSNMP_SUBDIR) && valgrind --leak-check=full --track-origins=yes $(abs_builddir)/$<
else
run-dmf-lua-test:
	@echo "  SKIP   LUA support was not enabled or detected during project configuration, can not run-dmf-lua-test" >&2
endif
