# Rules and recipes to build DMF-related files, e.g.:
# * regenerate the *-mib.dmf from the legacy *-mib.c sources
# * generate the dmfnutscan-snmp.dmf from existing DMF files
# * build and run some test programs to verify that DMF code works

# Note that some include files may be generated, so we prefer a builddir
# and then srcdir - just in case these differ
DMFTOOLS_INCLUDE_FLAGS = -I$(abs_top_builddir) -I$(abs_top_srcdir) \
    -I$(abs_top_builddir)/include -I$(abs_top_srcdir)/include \
    -I$(abs_top_builddir)/drivers -I$(abs_top_srcdir)/drivers \
    -I$(abs_top_builddir)/tools/nut-scanner -I$(abs_top_srcdir)/tools/nut-scanner
DMFTOOLS_CFLAGS = $(DMFTOOLS_INCLUDE_FLAGS) -DWITH_DMFMIB=1
DMFTOOLS_CPPFLAGS = $(DMFTOOLS_INCLUDE_FLAGS) -DWITH_DMFMIB=1

# For experiments with lookup fun/nuf: enable the structure and have
# a no-op compiled in for the sake of selftests:
#   make DMFTOOLS_ADD_FLAGS='$(DMFSNMP_LKP_FUN_FLAGS)' dmfsnmp/eaton-pdu-{genesis2,marlin}-mib.dmf
#   make DMFTOOLS_ADD_FLAGS='$(DMFSNMP_LKP_FUN_FLAGS)' dmf-test
DMFSNMP_LKP_FUN_FLAGS = -DWITH_SNMP_LKP_FUN_DUMMY=1 -DWITH_SNMP_LKP_FUN=1
DMFTOOLS_CFLAGS += $(DMFTOOLS_ADD_FLAGS)
DMFTOOLS_CPPFLAGS += $(DMFTOOLS_ADD_FLAGS)

DISTCLEANFILES =
DISTCLEANDIRS =
CLEANFILES =
EXTRA_DIST =
noinst_PROGRAMS =

# First target is default
all: progs dmf

clean: clean-local

clean-local:
	rm -f $(CLEANFILES)

check: check-local

# A way to to relative symlinks, or hardlinks, or copies... depends on OS.
LN_S_R = @LN_S_R@ -f

# The dmf*resdir variables point to resources, original DMFs with their
# original basenames same as *-mib.c files they were generated from.
# The dmf*dir variables are separate directories where filenames (symlinks)
# are specially ordered so that some mappings are loaded and tried before
# others (specifically, "ietf" standard fallback MIBs must be last); these
# are the directories used by default in DMF binaries.

# install dirs:
dmfsnmpdir = @dmfsnmpdir@
dmfsnmpresdir = @dmfsnmpresdir@
dmfnutscandir = @dmfnutscandir@
dmfnutscanresdir = @dmfnutscanresdir@

# build dirs:
DMFSNMP_RES_SUBDIR = dmfsnmp
DMFNUTSCAN_RES_SUBDIR = dmfnutscan
DMFSNMP_SUBDIR = dmfsnmp.d
DMFNUTSCAN_SUBDIR = dmfnutscan.d

# This generated file defines LEGACY_NUT_C_MIBS and LEGACY_NUT_DMFS lists
# based on existing C sources for ../../drivers/*-mib.c
DISTCLEANFILES += legacy-mibfiles-list.mk legacy-mibfiles-list.mk.in
# Use "-include" to avoid errors in automake (the file is not yet available
# in builddir at this moment)
-include legacy-mibfiles-list.mk

# Support development experiments for lookup with functions
testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf: $(abs_top_builddir)/scripts/DMF/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf
	@true
$(abs_builddir)/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf: $(abs_top_builddir)/scripts/DMF/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf
	@true

$(abs_top_builddir)/scripts/DMF/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf: DEBUG=yes
$(abs_top_builddir)/scripts/DMF/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf: DMFTOOLS_CFLAGS += -DWITH_SNMP_LKP_FUN_DUMMY=1 -DWITH_SNMP_LKP_FUN=1
$(abs_top_builddir)/scripts/DMF/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf: DMFTOOLS_CPPFLAGS += -DWITH_SNMP_LKP_FUN_DUMMY=1 -DWITH_SNMP_LKP_FUN=1
$(abs_top_builddir)/scripts/DMF/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf : $(abs_top_srcdir)/drivers/eaton-pdu-marlin-mib.c $(DMFGEN_DEPS)
	@DMFFILE="$(abs_top_builddir)/scripts/DMF/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf"; \
	 CMIBFILE="$(abs_top_srcdir)/drivers/eaton-pdu-marlin-mib.c"; export DEBUG; \
	 $(DMFGEN_CMD)

# Automake installation - currently only if built WITH_DMF (we don't have the
# directory names otherwise)
# NOTE: If you provide your own DMF files that are not generated from "*-mib.c"
# existing sources, use some filename pattern that does not end in "-mib.dmf",
# to avoid possible conflicts in the future!

# Note that LEGACY_NUT_DMFS list defined in the included file lists the DMF
# files generated from *-mib.c sources (both tracked in Git or tarballs), and
# the make-variable value already includes the DMFSNMP_RES_SUBDIR subdirectory.
dmfsnmpres_DMFS = $(LEGACY_NUT_DMFS)
# dmfsnmpres_DMFS += $(YOUR_CUSTOM_COMPILED_DMFS)
# dmfsnmpres_DMFS += $(YOUR_CUSTOM_VERBATIM_DMFS)
dmfsnmpres_DATA =

DMFNUTSCAN_SYMLINK = $(DMFNUTSCAN_SUBDIR)/S95_dmfnutscan-snmp.dmf
dmfnutscanres_DATA =

# The symlinks will be installed via automake hook below, so
# these _DATA variables will stay empty
dmfnutscan_DATA =
dmfsnmp_DATA =

if WITH_DMF
dmfsnmpres_DATA += $(dmfsnmpres_DMFS) dmfsnmp.xsd
dmfnutscanres_DATA += $(DMFNUTSCAN_RES_SUBDIR)/dmfnutscan-snmp.dmf dmfnutscan.xsd

# Similar INSTALL_NUT_DMF_SYMLINKS is defined in the included generated makefile
INSTALL_NUT_DMFNUTSCAN_SYMLINK = $(DESTDIR)$(dmfnutscandir)/S95_dmfnutscan-snmp.dmf
INSTALL_NUT_DMFNUTSCAN_FILE = $(DESTDIR)$(dmfnutscanresdir)/dmfnutscan-snmp.dmf

install_dmfsnmp_data:  $(INSTALL_NUT_DMF_SYMLINKS)
install_dmfnutscan_data: $(INSTALL_NUT_DMFNUTSCAN_SYMLINK)

install-data-hook: install_dmfsnmp_data install_dmfnutscan_data

uninstall-hook:
	-rm -f $(INSTALL_NUT_DMFNUTSCAN_SYMLINK) $(INSTALL_NUT_DMF_SYMLINKS)
endif

EXTRA_DIST += dmfsnmp.xsd dmfnutscan.xsd
DISTCLEANFILES += $(LEGACY_NUT_DMF_SYMLINKS) $(DMFNUTSCAN_SYMLINK)
DISTCLEANDIRS += $(DMFSNMP_SUBDIR) $(DMFNUTSCAN_SUBDIR)
CLEANFILES += $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate $(DMFSNMP_SUBDIR)/.validated $(DMFNUTSCAN_SUBDIR)/.validated \
        $(DMFSNMP_RES_SUBDIR)/.uptodate $(DMFNUTSCAN_RES_SUBDIR)/.uptodate $(DMFSNMP_RES_SUBDIR)/.validated $(DMFNUTSCAN_RES_SUBDIR)/.validated

# This does not rely on automake semantics, so this Makefile.am can be directly
# used and changed during development without reconfiguring all of NUT sources
DMFTEST_SRC = dmf-test.c
DMFTEST_SRC_ADD = dmfcore.c dmfsnmp.c alist.c
EXTRA_DIST += $(DMFTEST_SRC)

dmfsnmp.c : $(top_srcdir)/common/dmfsnmp.c
	@ln -fs $< $@

dmfcore.c : $(top_srcdir)/common/dmfcore.c
	@ln -fs $< $@

alist.c : $(top_srcdir)/common/alist.c
	@ln -fs $< $@


CLEANFILES += $(noinst_PROGRAMS) $(DMFSNMP_RES_SUBDIR)/*.o \
		$(DMFSNMP_RES_SUBDIR)/*.dmf.tmp $(DMFSNMP_RES_SUBDIR)/*.json.tmp \
		$(DMFSNMP_RES_SUBDIR)/*_TEST.c $(DMFSNMP_RES_SUBDIR)/*_TEST.exe

# A bit of magic for the simpler "automake if" below
# which can not process "if (A || B)" statements
TGT_CHECK_LOCAL = progs
TGT_DMF = dmf-uptodate
ADD_TGT_CHECK_LOCAL =
ADD_TGT_DMF =

if WITH_DMF
TGT_CHECK_LOCAL += run-dmf-test
endif

if WITH_VALIDATE_DMF_SNMP
  ADD_TGT_CHECK_LOCAL += dmf-validate
  ADD_TGT_DMF += dmf-validate
else
if WITH_VALIDATE_DMF_NUTSCAN
  ADD_TGT_CHECK_LOCAL += dmf-validate
  ADD_TGT_DMF += dmf-validate
endif
endif

check-local: $(TGT_CHECK_LOCAL) $(ADD_TGT_CHECK_LOCAL)
dmf: $(TGT_DMF) $(ADD_TGT_DMF)

if WITH_DMF
check-experimental: progs check-local run-dmf-lua-test run-dmf-fun-test
check-all: check-experimental
check-lua: run-dmf-lua-test
check-fun: run-dmf-fun-test
endif

# Recipes to build DMF files with the sources and tools we have are defined
# via $(DMFGEN_CMD) as used in legacy-mibfiles-list.mk for example.
# NOTE: At a later stage, when the legacy ways are deprecated and %=*-mib.c are
# removed, these DMF files would become redistributable contents by themselves.
# The generation rules might stay, to help modernize third-party or in-house
# packages that maintain their own C MIB structures not part of NUT upstream.
DMFGEN_DEPS_FILES = $(srcdir)/dmfify-mib.sh $(srcdir)/jsonify-mib.py $(srcdir)/xmlify-mib.py $(srcdir)/nut_cpp
DMFGEN_DEPS = $(DMFGEN_DEPS_FILES)
EXTRA_DIST += $(DMFGEN_DEPS_FILES)
EXTRA_DIST += $(srcdir)/gen-legacy-mibfiles-list.sh

# NOTE: In "dmfify-mib.sh" runs, the C preprocessor must be called without
# optimization flags (or with explicit -O0 for gcc), otherwise the code
# generated by C preprocessor may be incompatible with pycparser and so
# fails the build (even if the simpler tests in configure script succeeded).
# So we specifically do not include original makefile CFLAGS below.

DMFGEN_SANITY_VALIDATED = $(abs_builddir)/.dmfgen-sanity-validated
CLEANFILES += $(DMFGEN_SANITY_VALIDATED)

EXTRA_DIST += $(dmfsnmpres_DMFS)

if WITH_REGENERATE_DMF_SNMP
  DMFGEN_DEPS += $(abs_top_builddir)/Makefile $(abs_builddir)/Makefile $(DMFGEN_SANITY_VALIDATED)
# NOTE: Tricks like the externally-fed DMFFILE, CMIBFILE and DMFLINK
# are needed for portability of the recipes to non-GNU make programs
# (we can not use $< in this context); this approach is a hack that
# relies on known/assumed fixed input filename location and relation
# to output filenames - which we sort of guarantee in other places.
  DMFGEN_CMD = ( \
	echo "  DMFIFY '$$CMIBFILE' => '$@' (PWD = `pwd`)" >&2 ; \
	test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)" || exit ; \
	if test -f "$(DMFGEN_SANITY_VALIDATED)" ; then \
	    ( cd "$(@D)" && \
	      CFLAGS="$(DMFTOOLS_CFLAGS) $(AM_CFLAGS)" \
	      LDFLAGSx="$(abs_top_builddir)/common/libcommon.la $(abs_top_builddir)/drivers/dstate.o" \
	      CPPFLAGS="$(DMFTOOLS_CPPFLAGS) $(CPPFLAGS) $(AM_CPPFLAGS)" \
	      $(abs_srcdir)/dmfify-mib.sh --skip-sanity-check "$$CMIBFILE" "$@" \
	    ) || exit; \
	fi ; \
	if test ! -s "$@" -a ! -s "$(abs_top_builddir)/$@" -a ! -s "$(abs_top_srcdir)/$@"; then \
	    echo "ERROR: Generated file $@ is missing" >&2; \
	    ls -la "$@" "$(abs_top_builddir)/$@" "$(abs_top_srcdir)/$@"; \
	    exit 2; \
	fi; \
	)

# NOTE: a non-file target "dmfgen-sanity" follows, which is not DISTed
$(DMFGEN_SANITY_VALIDATED): dmfgen-sanity

dmfgen-sanity: $(srcdir)/dmfify-mib.sh
	@if $(abs_srcdir)/dmfify-mib.sh --sanity-check ; then \
	    echo "Sanity-checked the low-level ability to generate DMF files - OK" >&2; \
	    test -f "$(DMFGEN_SANITY_VALIDATED)" || touch $(DMFGEN_SANITY_VALIDATED); \
	else \
	    echo "SKIP: NOT REBUILDING DMF files due to absence of prerequisites like Python or pycparser" >&2; \
	    rm -f $(DMFGEN_SANITY_VALIDATED); \
	fi
else
  DMFGEN_CMD = ( \
	echo "SKIP: Generation of $@ from $$CMIBFILE not enabled by the configure script (--with-dmfsnmp-regenerate option)"; \
	rm -f $(DMFGEN_SANITY_VALIDATED); \
	if test ! -s "$@" -a ! -s "$(abs_srcdir)/$@" -a ! -s "$(abs_top_builddir)/$@" -a ! -s "$(abs_top_srcdir)/$@"; then \
		echo "ERROR: Distributed file $@ is missing" >&2; exit 2; \
	fi; \
	)
endif

DMFLNK_CMD = ( \
	echo "  LN     '$$DMFFILE' => '$$DMFLINK' (PWD = `pwd`)" >&2 ; \
	[ "$(V)" = 1 ] && echo "BEFORE:"; ls -la "$$DMFFILE" "$$DMFLINK" || true; \
	DMFLINK_DIR="`dirname "$$DMFLINK"`"; \
	test -d "$$DMFLINK_DIR" -a -w "$$DMFLINK_DIR" || $(MKDIR_P) "$$DMFLINK_DIR" || exit ; \
	test -f "$$DMFFILE" || { echo "File '$$DMFFILE' not found" >&2; exit 22; } ; \
	[ "$(V)" = 1 ] && echo "$(LN_S_R) '$$DMFFILE' '$$DMFLINK' || exit"; \
	rm -f "$$DMFLINK" || true ; \
	$(LN_S_R) "$$DMFFILE" "$$DMFLINK" || exit ; \
	[ "$(V)" = 1 ] && echo "AFTER:"; ls -la "$$DMFFILE" "$$DMFLINK" || true; \
	)

# We can not touch read-only source dirs (e.g. during distcheck) so make sure
# that we use builddir for this final touch. The DMF files themselves may be
# prebuilt and distributed as source, the gmake VPATH should figure that out.
# This may cause harmless "circular dependency" notices during in-tree builds.
# We try to use the .PHONY target to quiesce the warnings below.
# However, the dmf-reindexer is currently hardcoded to use only one directory,
# so we copy stuff below just in case (if needed).
.PHONY: $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate $(DMFSNMP_SUBDIR)/.validated $(DMFNUTSCAN_SUBDIR)/.validated \
        $(DMFSNMP_RES_SUBDIR)/.uptodate $(DMFNUTSCAN_RES_SUBDIR)/.uptodate $(DMFSNMP_RES_SUBDIR)/.validated $(DMFNUTSCAN_RES_SUBDIR)/.validated
$(DMFSNMP_RES_SUBDIR)/.uptodate: $(abs_builddir)/$(DMFSNMP_RES_SUBDIR)/.uptodate
$(abs_builddir)/$(DMFSNMP_RES_SUBDIR)/.uptodate: $(dmfsnmpres_DMFS)
	@echo "DMFSNMP_RES_SUBDIR content is now up to date, made of: $(dmfsnmpres_DMFS)" >&2
	@test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	@if [ "`cd $(abs_builddir) && pwd`" = "`cd $(abs_srcdir) && pwd`" ]; then \
		echo "INFO: Build was done in-place, no need to copy files around"; \
	 else \
		echo "INFO: Build was done out-of-tree, so will copy files from source dir if needed (if nothing got generated)"; \
		for F in "$(abs_srcdir)/$(DMFSNMP_RES_SUBDIR)/"*.dmf ; do \
			B="`basename "$$F"`"; \
			case "$$B" in \
				'*.dmf') exit 0 ;; \
				S*|K*) ;; \
				*) N="$(abs_builddir)/$(DMFSNMP_RES_SUBDIR)/$$B"; \
					if [ -s "$$F" ] && [ ! -s "$$N" ] ; then \
						echo "  CP    $$F => $$N" >&2 ; \
						cp -f "$$F" "$$N" || exit ; \
					fi ;; \
			esac ; \
		done; \
	 fi
	touch "$@"

$(DMFSNMP_SUBDIR)/.uptodate: $(abs_builddir)/$(DMFSNMP_SUBDIR)/.uptodate
$(abs_builddir)/$(DMFSNMP_SUBDIR)/.uptodate: $(LEGACY_NUT_DMF_SYMLINKS)
	touch "$@"

# The recipe assumes we have separate opening and closing lines in the DMF file
# just generated by reindexer (the "<?xml...>\n<nut>" and "</nut>" lines) and a
# randomly ordered set of mib2nut lines that we want consistent (to get no
# `git diff` complaints).
# TODO: The proper solution (sort, uniq) should probably be in the C code!
# NOTE: Sorting is a problem now that some MIBs must be last (e.g. ietf) -
#       so we add them in desired order explicitly...
DMFNUTSCAN_DEPS = $(DMFSNMP_RES_SUBDIR)/.uptodate
EXTRA_DIST += $(DMFNUTSCAN_RES_SUBDIR)/dmfnutscan-snmp.dmf

if WITH_REGENERATE_DMF_NUTSCAN
DMFNUTSCAN_DEPS += dmf-reindex $(top_builddir)/Makefile $(abs_builddir)/Makefile
$(DMFNUTSCAN_RES_SUBDIR)/dmfnutscan-snmp.dmf: $(DMFNUTSCAN_DEPS)
	@test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	$(DMF_REINDEX) -Z "$(abs_builddir)/$(DMFSNMP_RES_SUBDIR)" > "$@.tmp"
	( LANG=C; LC_ALL=C; TZ=UTC; export LANG LC_ALL TZ; \
	  egrep -v '^$$|\<mib2nut ' "$@.tmp" | head -2 ; \
	  grep '<mib2nut ' "$@.tmp" | sort | uniq | grep -v 'mib_name="ietf"'; \
	  grep '<mib2nut ' "$@.tmp" | sort | uniq | grep 'mib_name="ietf"'; \
	  egrep -v '^$$|\<mib2nut ' "$@.tmp" | tail -1 ; \
	) > "$@"
	@LANG=C; LC_ALL=C; TZ=UTC; export LANG LC_ALL TZ; \
	  sort "$@.tmp" | egrep -v '^$$' | uniq > "$@.tmp1" ; \
	    sort "$@" | egrep -v '^$$' | uniq > "$@.tmp2" ; \
	  diff "$@.tmp1" "$@.tmp2" >/dev/null \
	|| { echo "FAILED to sort generated $@ without losses!" >&2; \
	     rm -f "$@.tmp"* ; exit 1; } ; exit 0
	rm -f "$@.tmp"*
	test -s "$@"
else
$(DMFNUTSCAN_RES_SUBDIR)/dmfnutscan-snmp.dmf: $(DMFNUTSCAN_DEPS)
	@echo "SKIP: Generation of $@ not enabled by the configure script (--with-dmfnutscan-regenerate option)"
	@if test ! -s "$@" -a ! -s "$(abs_srcdir)/$@"; then echo "ERROR: Distributed file $@ is missing" >&2; exit 2; fi; true
	@test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	@if [ "`cd $(abs_builddir) && pwd`" != "`cd $(abs_srcdir) && pwd`" ]; then \
		F="$(abs_srcdir)/$(DMFNUTSCAN_RES_SUBDIR)/$(@F)"; \
		N="$(abs_builddir)/$(DMFNUTSCAN_RES_SUBDIR)/$(@F)"; \
		if [ -s "$$F" ] && [ ! -s "$$N" ] ; then \
			echo "  CP    $$F => $$N" >&2 && \
			cp -f "$$F" "$$N" || exit ; \
		fi; \
	 fi
	test -s "$@"
endif

$(DMFNUTSCAN_SYMLINK): $(DMFNUTSCAN_RES_SUBDIR)/dmfnutscan-snmp.dmf
	@DMFFILE="$(DMFNUTSCAN_RES_SUBDIR)/dmfnutscan-snmp.dmf"; DMFLINK="$@"; $(DMFLNK_CMD)

$(INSTALL_NUT_DMFNUTSCAN_SYMLINK): $(INSTALL_NUT_DMFNUTSCAN_FILE)
	@DMFFILE="$(INSTALL_NUT_DMFNUTSCAN_FILE)"; DMFLINK="$@"; $(DMFLNK_CMD)

$(DMFNUTSCAN_RES_SUBDIR)/.uptodate: $(abs_builddir)/$(DMFNUTSCAN_RES_SUBDIR)/.uptodate
$(abs_builddir)/$(DMFNUTSCAN_RES_SUBDIR)/.uptodate: $(DMFNUTSCAN_RES_SUBDIR)/dmfnutscan-snmp.dmf
	@echo "DMFNUTSCAN_RES_SUBDIR is now up to date" >&2
	@test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	touch "$@"

$(DMFNUTSCAN_SUBDIR)/.uptodate: $(abs_builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate
$(abs_builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate: $(DMFNUTSCAN_SYMLINK)
	@echo "DMFNUTSCAN_SUBDIR is now up to date" >&2
	@test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	touch "$@"

# Validation requires xmllint, which we have if we succeed with asciidoc usage
# TODO: Maybe per-DMF *.dmf.validated files would be better?
$(DMFSNMP_RES_SUBDIR)/.validated: $(abs_builddir)/$(DMFSNMP_RES_SUBDIR)/.validated
$(DMFSNMP_SUBDIR)/.validated: $(abs_builddir)/$(DMFSNMP_SUBDIR)/.validated
$(DMFNUTSCAN_RES_SUBDIR)/.validated: $(abs_builddir)/$(DMFNUTSCAN_RES_SUBDIR)/.validated
$(DMFNUTSCAN_SUBDIR)/.validated: $(abs_builddir)/$(DMFNUTSCAN_SUBDIR)/.validated
# The DMFLINT_XSD and DMFLINT_OPTION are fed by caller
if WITH_VALIDATE_DMF_SNMP
  DMFLINT_CMD = ( \
	test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)" || exit ; \
	cd "$(@D)" && for F in *.dmf ; do \
	    echo " XMLLINT  $$F"; \
	    $(XMLLINT) --noout --schema "$$DMFLINT_XSD" "$$F" || exit; \
	done; \
	touch "$(@F)"; \
	)
else
  DMFLINT_CMD = ( \
	test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)" || exit ; \
	echo "SKIP: Validation of $@ not enabled by the configure script ($$DMFLINT_OPTION option)" ; \
	echo "VALIDATION-SKIPPED" > "$@" ; \
	)
endif

$(abs_builddir)/$(DMFSNMP_RES_SUBDIR)/.validated: $(abs_builddir)/$(DMFSNMP_RES_SUBDIR)/.uptodate
	@DMFLINT_XSD="$(abs_srcdir)/dmfsnmp.xsd" ; DMFLINT_OPTION="--with-dmfsnmp-validate"; $(DMFLINT_CMD)

$(abs_builddir)/$(DMFSNMP_SUBDIR)/.validated: $(abs_builddir)/$(DMFSNMP_SUBDIR)/.uptodate
	@DMFLINT_XSD="$(abs_srcdir)/dmfsnmp.xsd" ; DMFLINT_OPTION="--with-dmfsnmp-validate"; $(DMFLINT_CMD)

$(abs_builddir)/$(DMFNUTSCAN_RES_SUBDIR)/.validated: $(abs_builddir)/$(DMFNUTSCAN_RES_SUBDIR)/.uptodate
	@DMFLINT_XSD="$(abs_srcdir)/dmfnutscan.xsd" ; DMFLINT_OPTION="--with-dmfnutscan-validate"; $(DMFLINT_CMD)

$(abs_builddir)/$(DMFNUTSCAN_SUBDIR)/.validated: $(abs_builddir)/$(DMFNUTSCAN_SUBDIR)/.uptodate
	@DMFLINT_XSD="$(abs_srcdir)/dmfnutscan.xsd" ; DMFLINT_OPTION="--with-dmfnutscan-validate"; $(DMFLINT_CMD)


# Make sure all generated DMFs are up to date vs. C sources and recipes
dmf-uptodate: $(DMFSNMP_RES_SUBDIR)/.uptodate $(DMFNUTSCAN_RES_SUBDIR)/.uptodate $(DMFSNMP_SUBDIR)/.uptodate $(DMFNUTSCAN_SUBDIR)/.uptodate

# Make sure all present DMFs are valid vs. XSD schema
dmf-validate: dmf-uptodate $(DMFSNMP_RES_SUBDIR)/.validated $(DMFNUTSCAN_RES_SUBDIR)/.validated $(DMFSNMP_SUBDIR)/.validated $(DMFNUTSCAN_SUBDIR)/.validated


LUA_INCLUDE ?= -I/usr/include/lua5.1
CFLAGS_LUA ?= $(LUA_INCLUDE)
LDFLAGS_LUA ?= $(LUA_LIB)
if WITH_DMF_LUA
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=1 -DWITH_DMF_FUNCTIONS=1 $(CFLAGS_LUA)
 LDFLAGS_DMF_LUA = $(LDFLAGS_LUA)
else
# Note: absence of LUA does not require to set -DWITH_DMF_FUNCTIONS=0
 CFLAGS_DMF_LUA = -DWITH_DMF_LUA=0
 LDFLAGS_DMF_LUA =
endif

if WITH_LIBLTDL
  LIBS_dmfsnmp_ltdlXneon = $(LIBLTDL_LIBS)
  CFLAGS_dmfsnmp_ltdlXneon = $(LIBNEON_CFLAGS) $(LIBLTDL_CFLAGS) -DWITH_LIBLTDL=1
else
if WITH_NEON
  LIBS_dmfsnmp_ltdlXneon = $(LIBNEON_LIBS)
  CFLAGS_dmfsnmp_ltdlXneon = $(LIBNEON_CFLAGS) -DWITH_LIBLTDL=0
endif
endif


progs: $(noinst_PROGRAMS)

# Note: -DDMFREINDEXER_MAKECHECK=1 is needed to avoid inclusion
# of "nut_version.h" in "common.c" (if project is not conf'ed).
# Depending on project configuration, these tests may be built
# with or without dmf+lua support.
DMFTESTS_COMMON_CFLAGS = \
	$(CFLAGS_dmfsnmp_ltdlXneon) $(DMFTOOLS_CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) \
	$(LIBNETSNMP_CFLAGS) \
	-DWITH_DMFMIB=1 -DWITH_SNMP=1 -DWITH_NEON=1 -DDMFREINDEXER_MAKECHECK=1 -DDEBUG=1 \
	-DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"$(abs_builddir)/$(DMFSNMP_SUBDIR)\""

# Some GCCisms... TODO: A configure flag variable like WITH_GCC
DMFTESTS_COMMON_CFLAGS += \
	-ggdb -std=c99 -Wall -pedantic -Wc++-compat -D_FORTIFY_SOURCE=2 -fno-stack-check -fno-omit-frame-pointer -fstack-protector -ggdb3 -O0

# For fans of suicide on obscure platforms ;)
#DMFTESTS_COMMON_CFLAGS += -Werror

if WITH_DMF
# Included dependencies
$(top_builddir)/common/libnutdmfsnmp.la :
	cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

noinst_PROGRAMS += dmf-test

dmf_test_SOURCES = $(DMFTEST_SRC) $(DMFTEST_SRC_ADD)
dmf_test_LDADD = $(LIBS_dmfsnmp_ltdlXneon) \
	$(top_builddir)/common/libcommonstr.la
# $(top_builddir)/common/libnutdmfsnmp.la
# $(top_builddir)/common/libcommon.la $(top_builddir)/drivers/dstate.o
dmf_test_CFLAGS = $(DMFTESTS_COMMON_CFLAGS) $(CFLAGS_DMF_LUA)
#dmf_test_CFLAGS += $(AM_CFLAGS) -I$(top_builddir)/drivers -I$(top_srcdir)/drivers \
#        -I$(top_builddir)/tools/nut-scanner -I$(top_srcdir)/tools/nut-scanner \
#        $(LIBNETSNMP_CFLAGS) $(LIBNEON_CFLAGS) -DWITH_DMFMIB=1 -DWITH_SNMP=1 -DWITH_NEON=1
dmf_test_LDFLAGS = $(LDFLAGS_DMF_LUA)
endif

# This seems to cause issues regardless of "if", so
# devs desiring to hack should just uncomment this part
#if USE_CUSTOM_BUILD_OF_DMF_REINDEX
#DMF_REINDEX = $(abs_builddir)/dmf-reindex
#noinst_PROGRAMS += dmf-reindex
#dmf_reindex_SOURCES = $(top_srcdir)/tools/nut-scanner/nut-scanner-reindex-dmfsnmp.c
#dmf_reindex_LDADD = $(top_builddir)/common/libnutdmfsnmp.la $(LIBS_dmfsnmp_ltdlXneon) \
#	$(top_builddir)/common/libcommonstr.la
#dmf_reindex_CFLAGS = $(DMFTESTS_COMMON_CFLAGS) $(CFLAGS_DMF_LUA)
#dmf_reindex_LDFLAGS = $(LDFLAGS_DMF_LUA)
#else
CLEANFILES += dmf-reindex
DMF_REINDEX = $(abs_top_builddir)/tools/nut-scanner/nut-scanner-reindex-dmfsnmp
dmf-reindex:
	cd $(abs_top_builddir)/tools/nut-scanner/ && $(MAKE) nut-scanner-reindex-dmfsnmp
	$(LN_S_R) $(abs_top_builddir)/tools/nut-scanner/nut-scanner-reindex-dmfsnmp dmf-reindex
#endif

if WITH_VALGRIND
DMF_TEST_RUNNER = $(VALGRIND) --leak-check=full --track-origins=yes --show-leak-kinds=all -v
else
DMF_TEST_RUNNER =
endif

# Note: DMF dirs are currently hardcoded (above) so no big need to "cd" anywhere
# But some data should be prepopulated there before we run leak tests etc.
if WITH_DMF
run-dmf-test: dmf-test $(DMFSNMP_SUBDIR)/.uptodate
	cd $(DMFSNMP_SUBDIR) && ls -la && $(DMF_TEST_RUNNER) $(abs_builddir)/dmf-test

run-dmf-reindex: dmf-reindex $(DMFSNMP_SUBDIR)/.uptodate
	cd $(DMFSNMP_SUBDIR) && ls -la && $(DMF_TEST_RUNNER) $(DMF_REINDEX)
else
run-dmf-test run-dmf-reindex:
	@echo "  SKIP   DMF support was not enabled or detected during project configuration, can not run test programs" >&2
endif

if WITH_DMF
if WITH_LUA

noinst_PROGRAMS += dmf-lua-test
# Note: This uses CFLAGS_LUA not CFLAGS_DMF_LUA so the test bits can build
# even if the general project does not want to build DMF+LUA at this moment
dmf_lua_test_SOURCES = $(DMFTEST_SRC) $(DMFTEST_SRC_ADD)
dmf_lua_test_LDADD = $(LIBS_dmfsnmp_ltdlXneon) \
	$(top_builddir)/common/libcommonstr.la
# $(top_builddir)/common/libnutdmfsnmp.la
# $(top_builddir)/common/libcommon.la $(top_builddir)/drivers/dstate.o
dmf_lua_test_CFLAGS = $(DMFTESTS_COMMON_CFLAGS) $(CFLAGS_DMF_LUA) \
	-UDEFAULT_DMFSNMP_DIR_OVERRIDE -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"./\""
dmf_lua_test_LDFLAGS = $(LDFLAGS_LUA)

noinst_PROGRAMS += dmf-fun-test
# Note: This uses CFLAGS_LUA not CFLAGS_DMF_LUA so the test bits can build
# even if the general project does not want to build DMF+LUA at this moment
dmf_fun_test_SOURCES = $(DMFTEST_SRC) $(DMFTEST_SRC_ADD)
dmf_fun_test_LDADD = $(LIBS_dmfsnmp_ltdlXneon) \
	$(top_builddir)/common/libcommonstr.la
# $(top_builddir)/common/libnutdmfsnmp.la
# $(top_builddir)/common/libcommon.la $(top_builddir)/drivers/dstate.o
dmf_fun_test_CFLAGS = $(DMFTESTS_COMMON_CFLAGS) $(CFLAGS_DMF_LUA) $(DMFSNMP_LKP_FUN_FLAGS) \
	-UDEFAULT_DMFSNMP_DIR_OVERRIDE -DDEFAULT_DMFSNMP_DIR_OVERRIDE="\"./\""
dmf_fun_test_LDFLAGS = $(LDFLAGS_LUA)

testdata-lua/.validated: $(abs_builddir)/testdata-lua/.validated
testdata-fun/.validated: $(abs_builddir)/testdata-fun/.validated

LUA_EXAMPLE_DMFS = $(abs_srcdir)/testdata-lua/lua-eaton-mib.dmf $(abs_srcdir)/testdata-lua/lua-mge-mib.dmf
# LUA_EXAMPLE_DMFS += $(abs_srcdir)/testdata-lua/lua-example-mib.dmf
FUN_EXAMPLE_DMFS = $(abs_srcdir)/testdata-fun/eaton-pdu-marlin-mib_WITH_SNMP_LKP_FUN_DUMMY.dmf

testdata-lua/.uptodate: $(abs_builddir)/testdata-lua/.uptodate
$(abs_builddir)/testdata-lua/.uptodate: $(LUA_EXAMPLE_DMFS)
	@echo "testdata-lua is now up to date" >&2
	@test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	touch "$@"

testdata-fun/.uptodate: $(abs_builddir)/testdata-fun/.uptodate
$(abs_builddir)/testdata-fun/.uptodate: $(FUN_EXAMPLE_DMFS)
	@echo "testdata-fun is now up to date" >&2
	@test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	touch "$@"

if WITH_VALIDATE_DMF_SNMP
$(abs_builddir)/testdata-lua/.validated: testdata-lua/.uptodate
	test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	@cd "$(@D)" && for F in $(LUA_EXAMPLE_DMFS) ; do \
	    echo " XMLLINT-LUA  $$F"; \
	    $(XMLLINT) --noout --schema $(abs_srcdir)/dmfsnmp.xsd "$$F" || exit; \
	done
	touch "$@"

$(abs_builddir)/testdata-fun/.validated: testdata-fun/.uptodate
	test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	@cd "$(@D)" && for F in $(FUN_EXAMPLE_DMFS) ; do \
	    echo " XMLLINT-LUA  $$F"; \
	    $(XMLLINT) --noout --schema $(abs_srcdir)/dmfsnmp.xsd "$$F" || exit; \
	done
	touch "$@"
else
$(abs_builddir)/testdata-lua/.validated $(abs_builddir)/testdata-fun/.validated:
	test -d "$(@D)" -a -w "$(@D)" || $(MKDIR_P) "$(@D)"
	@echo "SKIP: Validation of $@ not enabled by the configure script (--with-dmfsnmp-validate option)"
	@echo "VALIDATION-SKIPPED" > "$@"
endif
CLEANFILES += $(abs_builddir)/testdata-lua/.validated testdata-lua/.validated
CLEANFILES += $(abs_builddir)/testdata-fun/.validated testdata-fun/.validated

#$(DMFSNMP_SUBDIR)/.uptodate 
run-dmf-lua-test: dmf-lua-test testdata-lua/.validated
	cd $(abs_top_builddir)/scripts/DMF/testdata-lua && $(DMF_TEST_RUNNER) $(abs_builddir)/dmf-lua-test

run-dmf-fun-test: dmf-fun-test testdata-fun/.validated
	cd $(abs_top_builddir)/scripts/DMF/testdata-fun && $(DMF_TEST_RUNNER) $(abs_builddir)/dmf-fun-test
else
dmf-lua-test:
	@echo "  SKIP   LUA support was not enabled or detected during project configuration, can not build dmf-lua-test program" >&2

run-dmf-lua-test:
	@echo "  SKIP   LUA support was not enabled or detected during project configuration, can not run-dmf-lua-test" >&2

dmf-fun-test:
	@echo "  SKIP   LUA support was not enabled or detected during project configuration, can not build dmf-fun-test program" >&2

run-dmf-fun-test:
	@echo "  SKIP   LUA support was not enabled or detected during project configuration, can not run-dmf-fun-test" >&2
endif
else
dmf-lua-test:
	@echo "  SKIP   DMF support was not enabled or detected during project configuration, can not build dmf-lua-test program" >&2

run-dmf-lua-test:
	@echo "  SKIP   DMF support was not enabled or detected during project configuration, can not run test programs" >&2

dmf-fun-test:
	@echo "  SKIP   DMF support was not enabled or detected during project configuration, can not build dmf-fun-test program" >&2

run-dmf-fun-test:
	@echo "  SKIP   DMF support was not enabled or detected during project configuration, can not run test programs" >&2
endif
