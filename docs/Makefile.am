DISTCLEANFILES =
EXTRA_DIST =

# Is "egrep == grep -E" always valid? (maybe all a job for configure.ac)
EGREP = egrep
#EGREP = grep -E

IMAGE_FILES = images/asciidoc.png \
	images/hostedby.png \
	images/nut_layering.png \
	images/nut-logo.png \
	images/note.png \
	images/warning.png \
	images/blue-arrow.png \
	images/simple.png \
	images/advanced.png \
	images/bigbox.png \
	images/bizarre.png \
	images/old-cgi.png

# Only track here the local deps
SHARED_DEPS = nut-names.txt daisychain.txt asciidoc.conf asciidoc.txt

USER_MANUAL_DEPS = acknowledgements.txt cables.txt config-notes.txt	\
 config-prereqs.txt ci-farm-lxc-setup.txt	\
 configure.txt download.txt documentation.txt features.txt history.txt	\
 outlets.txt scheduling.txt security.txt support.txt user-manual.txt

DEVELOPER_GUIDE_DEPS = contact-closure.txt design.txt developers.txt	\
 developer-guide.txt hid-subdrivers.txt macros.txt new-clients.txt	\
 new-drivers.txt net-protocol.txt nutdrv_qx-subdrivers.txt	\
 snmp-subdrivers.txt sock-protocol.txt

CABLES_DEPS = cables/apc-rs500-serial.txt	\
 cables/apc.txt cables/ge-imv-victron.txt cables/imv.txt		\
 cables/mgeups.txt cables/powerware.txt cables/repotec.txt		\
 cables/sms.txt

CABLES_IMAGES = images/cables/73-0724.png images/cables/940-0024C.jpg \
 images/cables/belkin-f6cx-rkm-xu-cable.jpg images/cables/Lansafecable.jpg \
 images/cables/mac-940-0024C.png images/cables/mge-66049.png \
 images/cables/mge-db9-rj12.jpg images/cables/mge-db9-rj45.jpg \
 images/cables/mge-usb-rj45.jpg \
 images/cables/SOLA-330.png

ALL_TXT_SRC = nut-names.txt daisychain.txt \
 $(USER_MANUAL_DEPS) $(DEVELOPER_GUIDE_DEPS) \
 $(CABLES_DEPS) FAQ.txt nut-qa.txt packager-guide.txt snmp.txt \
 solaris-usb.txt

NUT_SPELL_DICT = nut.dict
EXTRA_DIST += $(ALL_TXT_SRC) $(SHARED_DEPS) $(IMAGE_FILES) \
 $(CABLES_IMAGES) $(NUT_SPELL_DICT) \
 common.xsl xhtml.xsl chunked.xsl asciidoc.txt

ASCIIDOC_HTML_SINGLE = user-manual.html \
	developer-guide.html \
	packager-guide.html \
	solaris-usb.html \
	cables.html \
	FAQ.html

ASCIIDOC_HTML_CHUNKED = user-manual.chunked \
	developer-guide.chunked \
	packager-guide.chunked \
	solaris-usb.chunked \
	cables.chunked \
	FAQ.chunked

ASCIIDOC_PDF = user-manual.pdf \
	developer-guide.pdf \
	packager-guide.pdf \
	solaris-usb.pdf \
	cables.pdf \
	FAQ.pdf

SUBDIRS = man cables
SUFFIXES = .txt .html .pdf -spellchecked

all: doc

check-local: @DOC_CHECK_LIST@

doc: @DOC_BUILD_LIST@

pdf: $(ASCIIDOC_PDF)
# also build the HTML manpages with these targets
html-single: $(ASCIIDOC_HTML_SINGLE)
html-chunked: $(ASCIIDOC_HTML_CHUNKED)

# the "for" loops might better use $^ but it might be not portable
check-pdf: $(ASCIIDOC_PDF)
	@FAILED=""; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	for F in $(ASCIIDOC_PDF) ; do \
	    test -s "$$F" && { file "$$F" | $(EGREP) -i 'PDF document' > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	done; if test -n "$$FAILED" ; then \
	    echo "FAILED PDF sanity check for:$$FAILED" >&2 ; file $$FAILED >&2 ; exit 1; \
	fi; echo "PASSED PDF sanity check"; exit 0

check-html-single: $(ASCIIDOC_HTML_SINGLE)
	@FAILED=""; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	for F in $(ASCIIDOC_HTML_SINGLE) ; do \
	    test -s "$$F" && { file "$$F" | $(EGREP) -i '(XML|HTML.*document)' > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	done; if test -n "$$FAILED" ; then \
	    echo "FAILED HTML-single sanity check for:$$FAILED" >&2 ; file $$FAILED >&2 ; exit 1; \
	fi; echo "PASSED HTML-single sanity check"; exit 0

check-html-chunked: $(ASCIIDOC_HTML_CHUNKED)
	@FAILED=""; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	for D in $(ASCIIDOC_HTML_CHUNKED); do \
	    for F in "$$D"/*.html ; do \
	        test -s "$$F" && { file "$$F" | $(EGREP) -i '(XML|HTML.*document)' > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	    done; \
	    for F in "$$D"/*.css ; do \
	        test -s "$$F" && { $(EGREP) -i 'CSS stylesheet' "$$F" > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	    done; \
	done; if test -n "$$FAILED" ; then \
	    echo "FAILED HTML-chunked sanity check for:$$FAILED" >&2 ; file $$FAILED >&2 ; exit 1; \
	fi; echo "PASSED HTML-chunked sanity check"; exit 0

# Note: usually the results from man-page check will be reported twice:
# once as a SUBDIRS child makefile, and once via DOC_CHECK_LIST expansion
check-man:
	cd $(top_builddir)/docs/man/ && $(MAKE) -f Makefile $@

clean-local:
	rm -f *.pdf *.html *-spellchecked docbook-xsl.css
	rm -rf *.chunked *.bak

### TODO: automatic dependency generation
# Add other directory deps (not for local EXTRA_DIST) and generated contents
FULL_USER_MANUAL_DEPS = $(USER_MANUAL_DEPS) $(SHARED_DEPS) ../README \
	../INSTALL.nut ../UPGRADING ../TODO ../scripts/ufw/README
FULL_DEVELOPER_GUIDE_DEPS = $(DEVELOPER_GUIDE_DEPS) $(SHARED_DEPS) \
	../scripts/augeas/README ../TODO ../lib/README \
	../tools/nut-scanner/README

user-manual.html user-manual.chunked user-manual.pdf: $(FULL_USER_MANUAL_DEPS)
developer-guide.html developer-guide.chunked developer-guide.pdf: $(FULL_DEVELOPER_GUIDE_DEPS)
packager-guide.html packager-guide.chunked packager-guide.pdf: packager-guide.txt asciidoc.conf
solaris-usb.html solaris-usb.chunked solaris-usb.pdf: solaris-usb.txt asciidoc.conf

# Note: without the "-v", asciidoc (circa 8.6.2) sometimes hangs when
# generating the chunked HTML. In this case, export the environment
# variable ASCIIDOC_VERBOSE to "-v", ie:
#   $ ASCIIDOC_VERBOSE=-v make
A2X_COMMON_OPTS = $(ASCIIDOC_VERBOSE) --attribute icons \
    --xsltproc-opts="--nonet" \
    --xsltproc-opts="--stringparam nut.localdate \"`TZ=UTC date +%Y-%m-%d`\"" \
    --xsltproc-opts="--stringparam nut.localtime \"`TZ=UTC date +%H:%M:%S`\"" \
    --xsltproc-opts="--stringparam nut.nutversion \"@PACKAGE_VERSION@\"" \
    --attribute iconsdir=$(srcdir)/images \
    --attribute=badges \
    --attribute=external_title \
    --attribute tree_version=@TREE_VERSION@ \
    -a toc -a numbered --destination-dir=.
# NOTE: a2x newer than 8.6.8 says "--destination-dir" is only valid for HTML.
# As of version 8.6.9 it lies, and the argument is required for our distcheck.
# For more details see issues https://github.com/asciidoc/asciidoc/issues/44
# and https://github.com/networkupstools/nut/pull/281 (in short, attempts
# to "fix" this warning broke NUT build). If this is to be retried later, see
# https://github.com/networkupstools/nut/pull/281/commits/fe17861c4ea12679b3ebfefa8a6d692d79d99f2d
# and do not forget to fix up docs/man/Makefile.am too ;)

# NOTE: a2x tends to copy some files into its working area, preserving original
# permissions. If those files are read-only in origin (e.g. packaged stylesheet
# or our resources coming from EXTRA_DIST) the next a2x can not overwrite it.
# Also note that such hoarding of files has potential to break parallel builds
# (or cause them to produce undefined results if some bad timing happens).
# As a brutal workaround for the former problem, we chmod. For second one we
# might try magic with .SEQUENTIAL recipe hints, but that is gmake-dependent.

# There are a few best-effort scripting approaches to this below, but still
# with a chance to race and collide in parallel a2x runs. The trick here is
# to take a random number (starting with $$ as PID of current make thread is
# at least a portable approach - not all shells tested served $RANDOM values),
# sleep for a random initial delay in each thread based on a guess at amount
# of competing files and typical parallelism of 4 or so, and then block for
# as long as a "docbook-xsl.css" placed and used by a2x exists (checking each
# random interval, individual but possibly no unique per thread).
# When one thread is done processing a document file, it removes this css and
# another thread may jump in, whichever checks first.
# While this does not eliminate collisions in theory, they happen to be much
# less frequent in practice now (e.g. with `make distcheck -j N`), at a cost
# of potentially longer lags in the docs-build phase.
# NOTE: It seems (empirically) that simple "DOC-HTML" builds in the manpage
# subdirectory are not impacted by a2x collisions, so the proposed avoidance
# logic is not deployed in this case. Larger documents like "user-manual.html"
# below are susceptible and "protected" this way, however.
# Another part of the solution is to just order different doc types after
# each other, if several are enabled, this way avoiding a conflict on any
# temporary file names that could be based on the source being parsed.

if WITH_HTML_SINGLE
if WITH_HTML_CHUNKED
$(ASCIIDOC_HTML_CHUNKED): $(ASCIIDOC_HTML_SINGLE)
endif WITH_HTML_CHUNKED
if WITH_PDFS
$(ASCIIDOC_PDF): $(ASCIIDOC_HTML_SINGLE)
endif WITH_PDFS
endif WITH_HTML_SINGLE

if WITH_HTML_CHUNKED
if WITH_PDFS
$(ASCIIDOC_PDF): $(ASCIIDOC_HTML_CHUNKED)
endif WITH_PDFS
endif WITH_HTML_CHUNKED

# PORTABILITY NOTE: POSIX Make forbids the suffix rule definitions with
# prerequisites like done below, and GNU Make of some versions complains;
# https://www.gnu.org/software/make/manual/html_node/Error-Messages.html
# says the prerequisites were ignored while a suffix rule was created;
# eventually the POSIX stance would be taken to define a rule for a weird
# verbatim target file name with prerequisites:
# ../docs/Makefile:936: warning: ignoring prerequisites on suffix rule definition
# Changes from ".txt.pdf: docinfo.xml" to "*.pdf: docinfo.xml" = ".txt.pdf:"
# as done below may be pointless in the end (with regard to a portable way
# to trigger builds by a changed dependency), but at least predictable and
# not toxic.
*.html: common.xsl xhtml.xsl
.txt.html:
	@case "$(MAKEFLAGS)$(MFLAGS)" in \
	 *-j{0,1,2,3,4,5,6,7,8,9}*|*-j" "|*--job*) \
	    RNDSLEEP=`expr $$$$ % 19 + 5 + $$$$ % 5` ; \
	    echo "  DOC-HTML Delaying at least $$RNDSLEEP sec before $@ (parallel-make a2x collision avoidance)"; \
	    sleep $$RNDSLEEP ; N=10; \
	    while test -e "$(builddir)/docbook-xsl.css" && test "$$N" -gt 0 ; \
	        do sleep $$RNDSLEEP ; N="`expr $$N - 1`"; done; \
	    if [ -s "$(builddir)/docbook-xsl.css" ]; then \
	        echo "WARNING: docbook-xsl.css from an earlier a2x already exists when building $@" >&2 ; \
	    fi ; \
	    touch "$(builddir)/docbook-xsl.css" || true ;; \
	 esac
	@if test -s "$(builddir)/docbook-xsl.css" && test -r "$(builddir)/docbook-xsl.css" && ! test -w "$(builddir)/docbook-xsl.css" ; then chmod u+w "$(builddir)/docbook-xsl.css" ; fi
	@chmod -R u+w .
	@echo "  DOC-HTML Generating $@"; \
	$(A2X) $(A2X_COMMON_OPTS) --attribute=xhtml11_format --format=xhtml --xsl-file=$(srcdir)/xhtml.xsl $<
	@rm -f "$(builddir)/docbook-xsl.css"

*.chunked: common.xsl chunked.xsl
.txt.chunked:
	@case "$(MAKEFLAGS)$(MFLAGS)" in \
	 *-j{0,1,2,3,4,5,6,7,8,9}*|*-j" "|*--job*) \
	    RNDSLEEP=`expr $$$$ % 12 + 2 + $$$$ % 7` ; \
	    echo "  DOC-HTML-CHUNKED Delaying at least $$RNDSLEEP sec before $@ (parallel-make a2x collision avoidance)"; \
	    sleep $$RNDSLEEP ; N=10; \
	    while test -e "$(builddir)/docbook-xsl.css" && test "$$N" -gt 0 ; \
	        do sleep $$RNDSLEEP ; N="`expr $$N - 1`"; done; \
	    if [ -s "$(builddir)/docbook-xsl.css" ]; then \
	        echo "WARNING: docbook-xsl.css from an earlier a2x already exists when building $@" >&2 ; \
	    fi ; \
	    touch "$(builddir)/docbook-xsl.css" || true ;; \
	 esac
	@if test -s "$(builddir)/docbook-xsl.css" && test -r "$(builddir)/docbook-xsl.css" && ! test -w "$(builddir)/docbook-xsl.css" ; then chmod u+w "$(builddir)/docbook-xsl.css" ; fi
	@chmod -R u+w .
	@echo "  DOC-HTML-CHUNKED Generating $@"; \
	$(A2X) $(A2X_COMMON_OPTS) --attribute=chunked_format --format=chunked --xsl-file=$(srcdir)/chunked.xsl $<
	@rm -f "$(builddir)/docbook-xsl.css"

*.pdf: docinfo.xml
.txt.pdf:
	@case "$(MAKEFLAGS)$(MFLAGS)" in \
	 *-j{0,1,2,3,4,5,6,7,8,9}*|*-j" "|*--job*) \
	    RNDSLEEP=`expr $$$$ % 13 + 11 + $$$$ % 9` ; \
	    echo "  DOC-PDF  Delaying at least $$RNDSLEEP sec before $@ (parallel-make a2x collision avoidance)"; \
	    sleep $$RNDSLEEP ; N=10; \
	    while test -e "$(builddir)/docbook-xsl.css" && test "$$N" -gt 0 ; \
	        do sleep $$RNDSLEEP ; N="`expr $$N - 1`"; done; \
	    if [ -s "$(builddir)/docbook-xsl.css" ]; then \
	        echo "WARNING: docbook-xsl.css from an earlier a2x already exists when building $@" >&2 ; \
	    fi ; \
	    touch "$(builddir)/docbook-xsl.css" || true ;; \
	 esac
	@if test -s "$(builddir)/docbook-xsl.css" && test -r "$(builddir)/docbook-xsl.css" && ! test -w "$(builddir)/docbook-xsl.css" ; then chmod u+w "$(builddir)/docbook-xsl.css" ; fi
	@chmod -R u+w .
	@echo "  DOC-PDF  Generating $@"; \
	$(A2X) $(A2X_COMMON_OPTS) --attribute=pdf_format --format=pdf -a docinfo1 $<
	@rm -f "$(builddir)/docbook-xsl.css"

if HAVE_ASPELL
# Non-interactively spell check all documentation source files.
# This is useful for Buildbot and automated QA processing
# FIXME: how to present output (std{out,err}, single file or per target)?
# NOTE: ../ChangeLog is nowadays generated from commit messages, so
# its spelling (or errors in that) are not fixable and thus irrelevant.
# Similarly for the ../INSTALL file that is prepared by autoconf and not
# tracked as a source file by NUT Git repository.
SPELLCHECK_SRC = $(ALL_TXT_SRC) ../README ../INSTALL.nut ../UPGRADING  ../NEWS \
	../TODO ../scripts/ufw/README ../scripts/augeas/README ../lib/README \
	../tools/nut-scanner/README \
	../AUTHORS ../COPYING ../LICENSE-GPL2 ../LICENSE-GPL3

# Directory SPELLCHECK_SRC files are relative to. Overriden by other Makefiles.
SPELLCHECK_DIR = $(srcdir)

# Note: de-facto our documentation is beyond ASCII (at least in names of
# international committers). The grep tests below look if the aspell output
# contained something other than the OK lines (tagged with asterisk) and
# aspell's version (tagged with @) and if it did - those lines must be the
# spellcheck complaints. Empty OUT is ok.
# We also must indent the input, because certain piped-in characters are
# interpreted as commands, and seems this feature can not be turned off.
# See also http://aspell.net/man-html/Through-A-Pipe.html
# TODO: Is "grep -a" or "grep -b" (treat input as ascii/bin) portable enough?
# Set SPELLCHECK_ERROR_FATAL=no if there are some unavoidable issues
# due to spellchecking, to temporarily not fail builds due to this.
# For Travis CI in particular, see ci_build.sh in NUT codebase root.
SPELLCHECK_ERROR_FATAL = yes
SPELLCHECK_ENV_DEBUG = no
ASPELL_NUT_COMMON_ARGS = -p $(abs_srcdir)/$(NUT_SPELL_DICT)
ASPELL_NUT_COMMON_ARGS += -d en --lang=en --ignore-accents
ASPELL_NUT_COMMON_ARGS += --encoding=utf-8
ASPELL_ENV_LANG = en.UTF-8
ASPELL_OUT_NOTERRORS = (^[ \t]*[\*\@]|^$$)

# WARNING: The percent wildcard is a GNU extension; otherwise we need
# a ".txt.txt-spellchecked" type of rule and files like "README" all
# renamed to *.txt, or lots of rules for files without the extensions
# Other Makefiles have a relatively simple life, dealing with just a
# few texts and name/extension patterns in their directories.
#?#.txt.txt-spellchecked: Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#%-spellchecked: % Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#*-spellchecked */*-spellchecked: $(@:-spellchecked=) $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
# NOTE: This portable  rule RELIES on just one SPELLCHECK_SRC defined
# at a time, with an outer Makefile caller ensuring the looping:
$(SPELLCHECK_DIR)/$(SPELLCHECK_SRC_ONE)-spellchecked: $(SPELLCHECK_DIR)/$(SPELLCHECK_SRC_ONE) $(abs_top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
	@LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	 rm -f "$@" || true ; \
	 echo "  ASPELL   Spell checking on $(SPELLCHECK_DIR)/$(SPELLCHECK_SRC_ONE)"; \
	 OUT="`(sed 's,^\(.*\)$$, \1,' | $(ASPELL) -a -t $(ASPELL_NUT_COMMON_ARGS) 2>&1) < "$(SPELLCHECK_DIR)/$(SPELLCHECK_SRC_ONE)"`" \
		&& { if test -n "$$OUT" ; then OUT="`echo "$$OUT" | $(EGREP) -b -v '$(ASPELL_OUT_NOTERRORS)' `" ; fi; \
		     test -z "$$OUT" ; } \
		|| { RES=$$? ; \
		     echo "FAILED : Aspell reported errors here:" >&2 \
		     && echo "----- vvv" >&2 \
		     && echo "$$OUT" >&2 \
		     && echo "----- ^^^" >&2 ; \
		     exit $$RES; } ; \
	 touch "$@"

spellcheck: 
	@if test "$(SPELLCHECK_ENV_DEBUG)" != no ; then \
		echo "ASPELL DEBUG : information about the setup follows:"; \
		LANG=$(ASPELL_ENV_LANG); LC_ALL=$(ASPELL_ENV_LANG); export LANG; export LC_ALL; \
		$(ASPELL) --help || true; \
		dpkg -l |grep -i aspell || true ; \
		echo "ASPELL automatic execution line is : ( sed 's,^\(.*\)$$, \1,' < docfile.txt | $(ASPELL) -a -t $(ASPELL_NUT_COMMON_ARGS) | $(EGREP) -b -v '$(ASPELL_OUT_NOTERRORS)' )" ; \
		echo "ASPELL proceeding to spellchecking job..."; \
	 else true; fi
	@FAILED="" ; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	 for docsrc in $(SPELLCHECK_SRC); do \
		if test "$(SPELLCHECK_ENV_DEBUG)" != no ; then \
			echo "ASPELL MAKEFILE DEBUG: Will see from `pwd` if '$(SPELLCHECK_DIR)/$${docsrc}-spellchecked' is up to date" >&2; \
		else true ; fi ; \
		$(MAKE) -s -f "$(abs_top_builddir)/docs/Makefile" SPELLCHECK_SRC_ONE="$${docsrc}" SPELLCHECK_DIR="$(SPELLCHECK_DIR)" "$(SPELLCHECK_DIR)/$${docsrc}-spellchecked" \
		|| FAILED="$$FAILED $(SPELLCHECK_DIR)/$$docsrc"; \
	 done ; \
	 if test -n "$$FAILED" ; then \
		echo "=====================================================================" ; \
		echo "FAILED automatic spellcheck for the following sources (relative to `pwd`): $$FAILED" ; \
		echo "=====================================================================" ; \
		echo "Please 'cd $(abs_top_builddir) && make spellcheck-interactive'"; \
		echo "to either fix document sources or update the dictionary of accepted"; \
		echo "words and spellings listed in the '$(NUT_SPELL_DICT)' file there."; \
		echo "Either way, please follow up by posting a pull request or a patch"; \
		echo "to integrate your fixes into the common NUT codebase."; \
		echo "=====================================================================" ; \
		test x"$(SPELLCHECK_ERROR_FATAL)" = xno || exit 1; \
		echo "NOTE: SPELLCHECK_ERROR_FATAL == no so this make does not break the build!"; \
		echo "=====================================================================" ; \
	 fi >&2 ; exit 0

# Interactively spell check all documentation source files below (so a human
# can edit the documentation errors and/or add words to custom dictionary).
# Note that here we do not restrain reported issues, so this might catch more
# than the automated test above.
spellcheck-sortdict: $(abs_builddir)/$(NUT_SPELL_DICT).sorted

# Note that the source file may be not overwritable (distcheck, cdrom, ...),
# so we'd ignore that failure. But the practical use-case is a developer's
# in-tree workspace, so we want the working copy of the dictionary fixed up
# for easy `git diff`ing if possible.
# Note also that "$(<F)" is not POSIX portable, so we spell out the name var :(
$(abs_builddir)/$(NUT_SPELL_DICT).sorted: $(abs_srcdir)/$(NUT_SPELL_DICT)
	@cp -pf $(abs_srcdir)/$(NUT_SPELL_DICT) $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting
	@LANG=$(ASPELL_ENV_LANG); LC_ALL=$(ASPELL_ENV_LANG); export LANG; export LC_ALL; ( \
	    WORDLIST="`tail -n +2 < "$<" | sort | uniq`"; \
	    WORDCOUNT="`echo "$$WORDLIST" | wc -l`"; \
	    head -1 < "$<" | while read P L C E ; do echo "$$P $$L $$WORDCOUNT $$E"; break; done ; \
	    echo "$$WORDLIST"; \
	 ) > "$@"
	@cp -f "$@" "$(abs_builddir)/$(NUT_SPELL_DICT)"
	@if [ "$(abs_builddir)" != "$(abs_srcdir)" ] ; then \
	    cp -f "$@" "$<" || true ; \
	    cp -f "$(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting" "$(abs_srcdir)/" || true ; \
	 fi

DISTCLEANFILES += $(NUT_SPELL_DICT).bak-pre-sorting .$(NUT_SPELL_DICT).sorted $(NUT_SPELL_DICT).sorted

spellcheck-interactive: 
	@FAILED="" ; for docsrc in $(SPELLCHECK_SRC); do \
		echo "Spell checking on $(SPELLCHECK_DIR)/$$docsrc"; \
		LANG=$(ASPELL_ENV_LANG) LC_ALL=$(ASPELL_ENV_LANG) $(ASPELL) check $(ASPELL_NUT_COMMON_ARGS) $(SPELLCHECK_DIR)/$$docsrc || \
			FAILED="$$FAILED $(SPELLCHECK_DIR)/$$docsrc"; \
	done ; \
	if test -n "$$FAILED" ; then \
		echo "FAILED interactive spellcheck for the following sources (relative to `pwd`): $$FAILED" >&2 ; \
		exit 1; \
	fi ; exit 0
	$(MAKE) spellcheck-sortdict
	@echo "------------------------------------------------------------------------"; \
	 echo "Custom dictionary file $(NUT_SPELL_DICT) may have been updated now."; \
	 echo "Use 'git add -p docs/$(NUT_SPELL_DICT) && git checkout -- docs/$(NUT_SPELL_DICT) && make spellcheck-sortdict && git add -p docs/$(NUT_SPELL_DICT)'"; \
	 echo "to review changes (please DO NOT REMOVE LINES that aspell chose to drop,"; \
	 echo "because other systems might not know these words in their system dictionaries)"; \
	 echo "------------------------------------------------------------------------"
else !HAVE_ASPELL
# This rule woulf probably just fail; normally with no ASPELL there are no callers for it
*/*-spellchecked *-spellchecked: Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
	@echo "  SKIP-ASPELL   $@ : Documentation spell check not available since 'aspell' was not found." >&2
spellcheck:
	@echo "Documentation spell check not available since 'aspell' was not found."
spellcheck-interactive:
	@echo "Documentation spell check not available since 'aspell' was not found."
endif !HAVE_ASPELL

.PHONY: html html-single pdf
