# Network UPS Tools: main docs

# FIXME: There is a lot of shell-scripting below which gets processed by
# whatever system shell there is. While bash handles expressions like
#   VAL="`cmd "some params"`"
# in a way that "some params" are a single token passed to "cmd" as an
# argument, and the stdout of such command execution is collected into a
# single-token string as "VAL" (even if with white-spaces). Some other
# shells, e.g. "ksh" seems to actively dislike unbalanced double-quotes
# inside the backticks (e.g. matched in some grep/sed regexes below),
# although generally the approach works for such "VAL" assignments too.
# Note that the newer "$(...)" syntax is not portable, older shells
# have no idea about it, and it is cumbersome with `make` substitution.
# Keep a lookout with multi-platform NUT CI jobs, and try to use single
# quotes where possible (e.g. where pre-expanded `make` variables are
# involved - so shell should not process them again anyway).

MAINTAINERCLEANFILES = Makefile.in .dirstamp
EXTRA_DIST =

# Note: "doc" ensures the `configure`-specified list of documents we actually
# want, while the default generated "all: all-am" target historically causes
# some but not all of these targets to get built (e.g. ChangeLog html/pdf is
# usually not made there). Post-processing "doc" as part of "all" helps
# ensure that we do not rebuild stuff in vain during parallel builds (where
# "all-am" and "doc" would be unordered parallel goals of the "all" target)
# while getting those further goals achieved eventually in the default build.
# Crucially, this allows to make sure "ChangeLog(.adoc*)" files have been
# generated once (can take a looong while), settled into place, and only then
# we revisit them for html/pdf rendering (another long while) without randomly
# confusing the system with new timestamps and needless regenerations later on.
all:
	@echo "  DOC-FOLLOW-UP	Basic 'make $@' in `pwd` is done, following up with 'make doc' to ensure complex document types"
	+@$(MAKE) $(AM_MAKEFLAGS) doc

# Is "egrep == grep -E" always valid? (maybe all a job for configure.ac)
#EGREP = egrep
EGREP = grep -E

# Possible man page section remapping in some distros:
MAN_SECTION_API_BASE=@MAN_SECTION_API_BASE@
MAN_SECTION_CFG_BASE=@MAN_SECTION_CFG_BASE@
MAN_SECTION_CMD_SYS_BASE=@MAN_SECTION_CMD_SYS_BASE@
MAN_SECTION_CMD_USR_BASE=@MAN_SECTION_CMD_USR_BASE@
# Other rewritable properties:
NUT_WEBSITE_BASE=@NUT_WEBSITE_BASE@
PACKAGE_VERSION=@PACKAGE_VERSION@
TREE_VERSION=@TREE_VERSION@

IMAGE_FILES = \
	images/asciidoc.png \
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

# Logos which pop up in README.adoc acknowledgements and maybe other places:
IMAGE_LOGO_FILES = \
	images/ci/AppVeyor_logo-2x.png \
	images/ci/AppVeyor_logo-ar21.png \
	images/ci/CircleCI_vertical_black_logo.png \
	images/ci/DO_Powered_by_Badge_blue.png \
	images/ci/DO_Powered_by_Badge_blue_140pxW.png \
	images/ci/fosshost_org_Host_Dark_56px.png \
	images/ci/fosshost_org_Host_Light_309px.png \
	images/ci/fosshost_org_Host_Light_38px.png \
	images/ci/gandi-ar21.png \
	images/ci/gandi-ar21.svg \
	images/ci/GitHub-Mark-140pxW.png \
	images/ci/GitHub-Mark-ea2971cee799.png \
	images/ci/OC_logotype.png \
	images/ci/OC_logo-watercolor-256.png \
	images/ci/OC_logo_merged_171x32.png \
	images/ci/OC_logo_merged_140x26.png

IMAGE_LOGO_FILES_JENKINS_NUT = \
	images/ci/ci-root.css \
	images/ci/jenkins-nut-large-256px.png \
	images/ci/jenkins-nut-large-squared.png \
	images/ci/jenkins-nut-large.pdn \
	images/ci/jenkins-nut-large.png \
	images/ci/jenkins-nut-small-256px.png \
	images/ci/jenkins-nut-small.pdn \
	images/ci/jenkins-nut-small.png \
	images/ci/jenkins-nut-squared.png \
	images/ci/jenkins-nut-transparent-bg-140pxW.png \
	images/ci/jenkins-nut-transparent-bg-40px.png \
	images/ci/jenkins-nut-transparent-bg.png \
	images/ci/jenkins-nut.css \
	images/ci/jenkins-nut.png \
	images/ci/jenkins-nut.txt

# Only track here the local deps
SHARED_DEPS = nut-names.txt daisychain.txt asciidoc.conf asciidoc.txt

# See also conversions included via FULL_USER_MANUAL_DEPS
USER_MANUAL_DEPS = acknowledgements.txt cables.txt config-notes.txt	\
 configure.txt download.txt documentation.txt features.txt history.txt	\
 outlets.txt scheduling.txt security.txt support.txt user-manual.txt

# See also conversions included via FULL_DEVELOPER_GUIDE_DEPS
DEVELOPER_GUIDE_DEPS = contact-closure.txt design.txt developers.txt	\
 developer-guide.txt hid-subdrivers.txt macros.txt new-clients.txt	\
 new-drivers.txt net-protocol.txt nutdrv_qx-subdrivers.txt	\
 nut-versioning.adoc snmp-subdrivers.txt sock-protocol.txt

# See also conversions included via FULL_QA_GUIDE_DEPS
QA_GUIDE_DEPS =  nut-qa.txt config-prereqs.txt ci-farm-do-setup.adoc	\
 ci-farm-lxc-setup.txt qa-guide.adoc

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
 $(CABLES_DEPS) $(QA_GUIDE_DEPS) FAQ.txt packager-guide.txt snmp.txt \
 release-notes.txt ChangeLog.txt solaris-usb.txt

ASPELL_FILTER_PATH = @ASPELL_FILTER_PATH@
# NOTE: This can be set by caller such as nut-website builder:
NUT_SPELL_DICT = nut.dict
EXTRA_DIST += $(ALL_TXT_SRC) $(SHARED_DEPS) $(IMAGE_FILES) \
 $(IMAGE_LOGO_FILES) $(IMAGE_LOGO_FILES_JENKINS_NUT) $(CABLES_IMAGES) $(NUT_SPELL_DICT) \
 docinfo.xml common.xsl xhtml.xsl chunked.xsl asciidoc.txt asciidoc-vars.conf

# NOTE: ALL_TXT_SRC does not include sms-brazil-protocol.txt because it
# primarily includes samples of configuration files with a lot of Spanish
# (Brazilian) words which confuse the spell-checker. Adding them to common
# NUT_SPELL_DICT would compromise its usefulness for purely English documents.
# FIXME: Add support for custom additional dictionaries for specific document
# files, e.g. "something.txt.dict" if present? There is precedent and code in
# nut-website recipes by now...
EXTRA_DIST += sms-brazil-protocol.txt

ASCIIDOC_HTML_SINGLE = \
	$(top_builddir)/docs/ChangeLog.html \
	user-manual.html \
	developer-guide.html \
	packager-guide.html \
	qa-guide.html \
	release-notes.html \
	solaris-usb.html \
	cables.html \
	FAQ.html

ASCIIDOC_HTML_CHUNKED = \
	$(top_builddir)/docs/ChangeLog.chunked \
	user-manual.chunked \
	developer-guide.chunked \
	packager-guide.chunked \
	qa-guide.chunked \
	release-notes.chunked \
	solaris-usb.chunked \
	cables.chunked \
	FAQ.chunked

ASCIIDOC_PDF = \
	$(top_builddir)/docs/ChangeLog.pdf \
	user-manual.pdf \
	developer-guide.pdf \
	packager-guide.pdf \
	qa-guide.pdf \
	release-notes.pdf \
	solaris-usb.pdf \
	cables.pdf \
	FAQ.pdf

# Note: "man" subdir is handled separately via all-local and check-local
# for a few goals, and among SUBDIRs directly from the top-level Makefile
# due to some potential dependency collisions with parallel builds.
SUBDIRS = cables
SUFFIXES = .txt .html .pdf
SUFFIXES += .txt-spellchecked-auto .txt-spellchecked .txt-prepped
SUFFIXES += .adoc-spellchecked-auto .adoc-spellchecked .adoc-prepped
SUFFIXES += .in-spellchecked-auto .in-spellchecked .in-prepped
SUFFIXES += .sample-spellchecked-auto .sample-spellchecked .sample-prepped
SUFFIXES += .conf-spellchecked-auto .conf-spellchecked .conf-prepped

# This list is defined by configure script choices and options:
CHECK_LOCAL_TARGETS = @DOC_CHECK_LIST@
if WITH_SPELLCHECK
CHECK_LOCAL_TARGETS += spellcheck
endif WITH_SPELLCHECK
check-local: $(CHECK_LOCAL_TARGETS)
	+@cd $(builddir)/man && $(MAKE) $(AM_MAKEFLAGS) check

# Make sure sources are there for out-of-tree builds:
all-local all-am-local \
@DOC_BUILD_LIST@ $(ASCIIDOC_PDF) $(ASCIIDOC_HTML_SINGLE) $(ASCIIDOC_HTML_CHUNKED): $(abs_top_builddir)/docs/.prep-src-docs

all-local:
	+@cd $(builddir)/man && $(MAKE) $(AM_MAKEFLAGS) all-optional

# This list is defined by configure script choices and options:
doc: $(abs_top_builddir)/docs/.prep-src-docs @DOC_BUILD_LIST@

# This target can be called by developers to go around the configure
# script choices at their risk (e.g. missing tools are possible):
docs: pdf html-single html-chunked man-man html-man

all-docs: docs

check-docs: check-pdf check-html-single check-html-chunked check-man

# Not called by default, but handy for maintainers to check which words
# in the custom dictionary are used or not by the current NUT codebase.
# Note that historically many words were added to facilitate rendition
# of the nut-website (long ago splintered from main nut repository),
# but since recently it has a way to track its own additions to the
# dictionary file. This code should help populate it as well, and keep
# only relevant entries in the appropriate corner of the sources.
# Note this can take 5-10 minutes!
spellcheck-report-dict-usage: $(NUT_SPELL_DICT).usage-report

pdf: $(ASCIIDOC_PDF)
# also build the HTML manpages with these targets
html-single: $(ASCIIDOC_HTML_SINGLE)
html-chunked: $(ASCIIDOC_HTML_CHUNKED)

# htmldocdir and pdfdir are set by autoconf/automake
htmldoc_DATA =
if WITH_HTML_SINGLE
htmldoc_DATA += $(ASCIIDOC_HTML_SINGLE)
endif WITH_HTML_SINGLE
# FIXME: Install tools refuse to work with directories in this context
# and html-chunked documentation has a separate tree per document.
# Maybe an "(un)install-data-local" or "install-data-hook" for this?
#if WITH_HTML_CHUNKED
#htmldoc_DATA += $(ASCIIDOC_HTML_CHUNKED)
#endif WITH_HTML_CHUNKED
if WITH_PDFS
pdf_DATA = $(ASCIIDOC_PDFS)
endif WITH_PDFS

# the "for" loops might better use $^ but it might be not portable
check-pdf: .check-pdf
.check-pdf: $(ASCIIDOC_PDF) Makefile
	@FAILED=""; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	for F in $(ASCIIDOC_PDF) ; do \
	    test -s "$$F" && { file "$$F" | $(EGREP) -i 'PDF document' > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	done; if test -n "$$FAILED" ; then \
	    echo "FAILED PDF sanity check for:$$FAILED" >&2 ; file $$FAILED >&2 ; exit 1; \
	fi; echo "PASSED PDF sanity check"; exit 0
	@touch $@

# Regarding ChangeLog check: sometimes asciidoc gives up early
# (we have megabytes of text and thousands of sections here).
# In some cases, the intermediate XML is broken and a2x=>xmllint
# notices it. In others, it is truncated at just the right place
# structurally and leads to a short HTML with only part of the
# expected contents. We should no longer have several processes
# trying to create the files involved (or rather do so atomically
# and rename into final path, in case we still have competition
# here); earlier when several generators appended to the same
# file we could have several copies overlaid, with one of the
# document's copies starting mid-sentence of another.
# The two expected mentions are in the table of contents and
# in the eventual section. Checking for first/second entries,
# and not exactly two mentions, should allow to catch the case
# of overlapping documents. Checking for the last entry allows
# to catch incomplete parses, where asciidoc gave up early.
# NOTE: Technically it may be more than two, if the author and
# date were used several times in the original ChangeLog file
# (either with different e-mails, or if different author's work
# is interleaved during the day, e.g. many PRs merged, and no
# CHANGELOG_REQUIRE_GROUP_BY_DATE_AUTHOR=true setting was in place.
# NOTE: No dependencies, avoids (re-)generation and log messages
# but causes re-run of the check every time.
ChangeLog.html-contentchecked:
	@FAILED=""; \
	 if [ -s '$(top_builddir)/ChangeLog' ] && [ -s ChangeLog.html ] ; then \
	    SECOND_ENTRY="`grep -E '^[0-9]' '$(top_builddir)/ChangeLog' | head -2 | tail -1 | sed 's/ *[\"<].*//'`" || SECOND_ENTRY="" ; \
	    FIRST_ENTRY="`grep -E '^[0-9]' '$(top_builddir)/ChangeLog' | head -1 | sed 's/ *[\"<].*//'`" || FIRST_ENTRY="" ; \
	    LAST_ENTRY="`grep -E '^[0-9]' '$(top_builddir)/ChangeLog' | tail -1 | sed 's/ *[\"<].*//'`" || LAST_ENTRY="" ; \
	    if [ -n "$${FIRST_ENTRY}" ] ; then \
	        O="`grep -cE "^$${FIRST_ENTRY}" '$(top_builddir)/ChangeLog'`" ; \
	        N="`grep -cE "title.*$${FIRST_ENTRY}" 'ChangeLog.html'`" ; \
	        MIN="`expr $${O} + 1`"     && [ "$${MIN}" -gt 0 ] 2>/dev/null || MIN=1 ; \
	        MAX="`expr $${O} + $${O}`" && [ "$${MAX}" -gt 2 ] 2>/dev/null || MAX=2 ; \
	        if [ "$${N}" -lt "$${MIN}" ] || [ "$${N}" -gt "$${MAX}" ]; then \
	            echo "FAILED ChangeLog.html check: does not contain expected first entry the right amount of times (huge doc, must have got aborted mid-way): $${FIRST_ENTRY} (seen $${N} times, expected between $${MIN} and $${MAX})" >&2 ; \
	            if [ -z "$$FAILED" ] ; then \
	                echo "Expected size over 3MB (for common builds):" >&2 ; \
	                ls -la "ChangeLog.html" '$(top_builddir)/ChangeLog'* >&2 ; \
	                FAILED="ChangeLog.html" ; \
	            fi ; \
	        fi ; \
	    fi; \
	    if [ -n "$${SECOND_ENTRY}" ] ; then \
	        O="`grep -cE "^$${SECOND_ENTRY}" '$(top_builddir)/ChangeLog'`" ; \
	        N="`grep -cE "title.*$${SECOND_ENTRY}" 'ChangeLog.html'`" ; \
	        MIN="`expr $${O} + 1`"     && [ "$${MIN}" -gt 0 ] 2>/dev/null || MIN=1 ; \
	        MAX="`expr $${O} + $${O}`" && [ "$${MAX}" -gt 2 ] 2>/dev/null || MAX=2 ; \
	        if [ "$${N}" -lt "$${MIN}" ] || [ "$${N}" -gt "$${MAX}" ]; then \
	            echo "FAILED ChangeLog.html check: does not contain expected second entry the right amount of times (huge doc, must have got aborted mid-way): $${SECOND_ENTRY} (seen $${N} times, expected between $${MIN} and $${MAX})" >&2 ; \
	            if [ -z "$$FAILED" ] ; then \
	                echo "Expected size over 3MB (for common builds):" >&2 ; \
	                ls -la "ChangeLog.html" '$(top_builddir)/ChangeLog'* >&2 ; \
	                FAILED="ChangeLog.html" ; \
	            fi ; \
	        fi ; \
	    fi; \
	    if [ -n "$${LAST_ENTRY}" ] ; then \
	        O="`grep -cE "^$${LAST_ENTRY}" '$(top_builddir)/ChangeLog'`" ; \
	        N="`grep -cE "title.*$${LAST_ENTRY}" 'ChangeLog.html'`" ; \
	        MIN="`expr $${O} + 1`"     && [ "$${MIN}" -gt 0 ] 2>/dev/null || MIN=1 ; \
	        MAX="`expr $${O} + $${O}`" && [ "$${MAX}" -gt 2 ] 2>/dev/null || MAX=2 ; \
	        if [ "$${N}" -lt "$${MIN}" ] || [ "$${N}" -gt "$${MAX}" ]; then \
	            echo "FAILED ChangeLog.html check: does not contain expected last entry the right amount of times (huge doc, must have got aborted mid-way): $${LAST_ENTRY} (seen $${N} times, expected between $${MIN} and $${MAX})" >&2 ; \
	            if [ -z "$$FAILED" ] ; then \
	                echo "Expected size over 3MB (for common builds):" >&2 ; \
	                ls -la "ChangeLog.html" '$(top_builddir)/ChangeLog'* >&2 ; \
	                FAILED="ChangeLog.html" ; \
	            fi ; \
	        fi ; \
	    fi; \
	    if [ x"$$FAILED" = x ] ; then \
	        echo "PASSED $@" >&2 ; \
	        exit 0 ; \
	    fi ; \
	    if [ x"$$FAILED" != x ] && [ -s '$(top_builddir)/ChangeLog.adoc' ] \
	    && [ "`head -1 $(top_builddir)/ChangeLog.adoc`" = "=== Failed to generate the ChangeLog" ] \
	    ; then \
	        FAILED="" ; \
	    fi; \
	 fi; \
	 if [ x"$$FAILED" = x ] ; then \
	    echo "SKIPPED $@ because input files were not available" >&2 ; \
	    exit 0 ; \
	 fi ; \
	 exit 1

check-html-single: .check-html-single
.check-html-single: $(ASCIIDOC_HTML_SINGLE) Makefile
	+@FAILED=""; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	 for F in $(ASCIIDOC_HTML_SINGLE) ; do \
	    test -s "$$F" && { file "$$F" | $(EGREP) -i '(XML|HTML.*document)' > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	    case "$$F" in \
	        *ChangeLog*) if [ -s '$(top_builddir)/ChangeLog' ] ; then \
	                $(MAKE) $(AM_MAKEFLAGS) "`basename "$$F"`"-contentchecked || FAILED="$$FAILED $$F" ; \
	            fi ;; \
	    esac ; \
	 done; if test -n "$$FAILED" ; then \
	    echo "FAILED HTML-single sanity check for:$$FAILED" >&2 ; file $$FAILED >&2 ; exit 1; \
	 fi; echo "PASSED HTML-single sanity check"; exit 0
	@touch $@

check-html-chunked: .check-html-chunked
.check-html-chunked: $(ASCIIDOC_HTML_CHUNKED) Makefile
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
	@touch $@

# Note: usually the results from man-page check will be reported twice:
# once as a SUBDIRS child makefile, and once via DOC_CHECK_LIST expansion
# Note: default `make all` in the man directory caters to drivers etc.
# chosen during configure script execution. The "all-man" and "all-html"
# rules build everything documented.
# NOTE; we rig it up with a DOCS_NO_MAN option to simplify parallel work
# from top-level Makefile, while allowing legacy "cd docs && make" to
# still do the right thing by default :)
check-man check-man-man all-man man-man all-html html-man:
	+@if [ x"$(DOCS_NO_MAN)" = xtrue ] ; then echo "  DOC-NOT-MAN     SKIP: $@ called in docs/Makefile" ; exit 0 ; fi ; \
	  cd $(abs_top_builddir)/docs/man/ && $(MAKE) $(AM_MAKEFLAGS) -f Makefile $@

man:
	+@if [ x"$(DOCS_NO_MAN)" = xtrue ] ; then echo "  DOC-NOT-MAN     SKIP: $@ called in docs/Makefile" ; exit 0 ; fi ; \
	  cd $(abs_top_builddir)/docs/man/ && $(MAKE) $(AM_MAKEFLAGS) -f Makefile all

CLEANFILES = *.xml *.html *.pdf *-spellchecked* *-contentchecked* .check-* docbook-xsl.css docinfo.xml.in.tmp docinfo-since-v*.xml* *-docinfo.xml*
CLEANFILES += $(top_builddir)/INSTALL.nut $(top_builddir)/UPGRADING $(top_builddir)/ChangeLog.adoc
CLEANFILES += $(top_builddir)/*.adoc-parsed *.adoc-parsed
# These two "must be" there per autotools standards, so a "make clean"
# should not compromise a rebuild:
DISTCLEANFILES = $(top_builddir)/NEWS $(top_builddir)/README

### TODO: general automatic dependency generation

MAINTAINER_ASCIIDOCS_CHANGELOG_DEBUG = no

# Prepare text files (currently a manually tracked list)
# with known presence of GitHub links to convert them from
# short <hash><number> notation into asciidoc link markup
# before rendering into HTML/PDF.
# Work around some documents that have originally included
# the asciidoc markup (use double-hash to avoid conversion).
# The $< is okay here, it is used in a suffix rule below
.adoc.adoc-parsed:
	@if [ -s '$@' ] && [ x"`find '$@' -newer '$<'`" != x ] ; then \
		echo "  DOC-ASCIIDOC-GITHUB-LINKS    SKIP: $@ already exists and is newer than $<" ; \
		if [ x"$(MAINTAINER_ASCIIDOCS_CHANGELOG_DEBUG)" != xno ] ; then \
			ls -lad $@ $< || true ; \
			stat $@ $< || true ; \
		fi ; \
		exit 0 ; \
	 fi ; \
	 echo "  DOC-ASCIIDOC-GITHUB-LINKS    Parsing GitHub link patterns $< => $@"; \
	 if [ x"$(MAINTAINER_ASCIIDOCS_CHANGELOG_DEBUG)" != xno ] ; then \
		ls -lad $@ $< || true ; \
		stat $@ $< || true ; \
	 fi ; \
	 cat '$<' | { $(SED) \
		-e 's%\(link:https*://github.com/networkupstools/[a-zA-Z0-9./-]*/[1-9][0-9]*/*\[[^]]*\)\#\([1-9][0-9]*\)%\1\#\#\2%g' \
		-e 's%\(issue\) *\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/nut/issues/\2[\1 \#\#\2]\3%g' \
		-e 's%\(PR\|pull request\) *\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/nut/pull/\2[\1 \#\#\2]\3%g' \
		-e 's%\([[ ,]\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%\1link:https://github.com/networkupstools/nut/issues/\2[\#\#\2]\3%g' \
		-e 's%\(issue\) networkupstools/\([^ ][^ ]*\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/\2/issues/\3[\1 \2\#\#\3]\4%g' \
		-e 's%\(PR\|pull request\) *networkupstools/\([^ ][^ ]*\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/\2/pull/\3[\1 \2\#\#\3]\4%g' \
		-e 's%\([[ ,]\)networkupstools/\([^ ][^ ]*\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%\1link:https://github.com/networkupstools/\2/issues/\3[\2\#\#\3]\4%g' \
		-e 's%\#\(\#[1-9][0-9]*\)%\1%g' \
	 ; } > "$@.tmp.$$$$" \
	 && mv -f "$@.tmp.$$$$" "$@"
	@if [ x'$@' = x'$(top_builddir)/ChangeLog.adoc-parsed' ] ; then \
		touch -r '$@' '$(top_builddir)/docs/.ChangeLog.adoc-parsed.latest' || touch '$(top_builddir)/docs/.ChangeLog.adoc-parsed.latest' ; \
	 fi

$(top_builddir)/ChangeLog.adoc-parsed: $(top_builddir)/ChangeLog.adoc

.ChangeLog.adoc-parsed.latest: $(top_builddir)/ChangeLog.adoc-parsed
	@[ x"$?" != x ] && touch -r '$?' '$@' || touch '$@'

CLEANFILES += .ChangeLog.adoc-parsed.latest

dummy:
$(top_builddir)/ChangeLog: dummy
	@+echo "  DOC-CHANGELOG-GENERATE-WRAPPER	$@ : call parent Makefile to decide if (re-)generation is needed" \
	 && cd $(top_builddir) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

# BSD Make dislikes the path resolution here and does not always populate '$<'
# (and claims why: "Using $< in a non-suffix rule context is a GNUmake idiom"),
# but it has a "$?" for "list of dependencies that are newer than the target".
# For more details see https://man.freebsd.org/cgi/man.cgi
if WITH_PDF_NONASCII_TITLES
A2X_ASCII_IDS =
else !WITH_PDF_NONASCII_TITLES
A2X_ASCII_IDS = ":ascii-ids:\n"
endif !WITH_PDF_NONASCII_TITLES

# Timeout for ChangeLog.adoc rule to wait for competing threads to maybe create
# (or start creating) the file first - then this "make" thread just waits for
# it to be done, and exits, as the goal is fulfilled.
MAINTAINER_ASCIIDOCS_CHANGELOG_DELAY = 10

# Probably due to the web of makefiles and an overwhelmed job server in some
# implementations, during parallel builds we can end up scheduling several
# threads creating this asciidoc (and adoc-parsed later). This step only
# costs a few seconds, however the updated timestamp may cause new HTML/PDF
# builds which cost a lot more. Below we try a few ways to detect a build
# already running and bail out early if the file exists. Otherwise we bite
# the bullet and spend a few seconds, and then re-check if another thread
# did exist and finished first.
$(top_builddir)/ChangeLog.adoc: $(top_builddir)/ChangeLog
	@INPUT="$?"; \
	 test -n "$${INPUT}" || INPUT="$$(top_builddir)/ChangeLog" ; \
	 test -n "$${INPUT}" && test -n "$@" && test -s "$${INPUT}" \
	 || { \
	     MSG="FAILED to resolve input or output filename with this make implementation, or input was not generated!"; \
	     echo "  DOC-CHANGELOG-ASCIIDOC	SKIP: $${MSG}" >&2; \
	     test -n "$@" && { printf '=== Failed to generate the ChangeLog\n\n%s\n\nNOTE: See https://github.com/networkupstools/nut/commits/master for change history.\n\n' "$${MSG}" > "$@" ; } ; \
	     exit ; \
	 } ; \
	 if [ -s '$@' ] && [ x"`find '$@' -newer "$${INPUT}" 2>/dev/null`" != x ] ; then echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@ : SKIP (keep existing)"; rm -f "$@.tmp.$$$$"; exit 0 ; fi ; \
	 W=0 ; \
	 if [ "$(MAINTAINER_ASCIIDOCS_CHANGELOG_DELAY)" -gt 0 ] 2>/dev/null ; then \
	        echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@ : Block for up to $(MAINTAINER_ASCIIDOCS_CHANGELOG_DELAY) sec, maybe another thread will make the file first" ; \
	        while [ "$${W}" -lt "$(MAINTAINER_ASCIIDOCS_CHANGELOG_DELAY)" ] && [ x"`find '$@.tmp.'* '$@' -newer "$${INPUT}" 2>/dev/null`" = x ] ; do sleep 1 ; W="`expr $$W + 1`"; done ; touch "$@.tmp.$$$$"; \
	        if [ x"`find '$@' -newer "$${INPUT}" 2>/dev/null`" != x ] ; then echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@ : SKIP (keep existing)"; rm -f "$@.tmp.$$$$"; exit 0 ; fi ; \
	 fi ; \
	 if [ -s "$@" ] ; then echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@ : RE-GENERATE" ; else echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@ : GENERATE" ; fi ; \
	 { if [ x"$(MAINTAINER_ASCIIDOCS_CHANGELOG_DEBUG)" != xno ] ; then ls -la "$${INPUT}" "$@" || true ; stat "$${INPUT}" "$@" || true ; fi ; } ; \
	 if [ "$(MAINTAINER_ASCIIDOCS_CHANGELOG_DELAY)" -gt 0 ] 2>/dev/null ; then \
	        echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@ : PROCEED, waited for $${W} sec" ; \
	 fi ; \
	 printf "ifdef::txt[]\n== Very detailed Change Log\n"$(A2X_ASCII_IDS)"endif::txt[]\n\n" > "$@.tmp.$$$$" \
	 && TABCHAR="`printf '\t'`" \
	 && $(SED) \
	        -e 's,^\([0-9a-zA-Z]\),=== \1,' \
	        -e 's,^=== \(NOTE: \),\1,' \
	        -e 's,/[+],/\\\\\+,g' \
	        -e 's,[+][+],\\\+\\\+,g' \
	        -e 's,^\([ '"$${TABCHAR}"'][^+]*\)\([^+/\]\)[+],\1\2\\\+,g' \
	        -e 's,^\([ '"$${TABCHAR}"'].*\)\([~|^]\),\1\\\2,g' \
	        -e 's,\[\[\([^]]*\)\]\],[\1],g' \
	        -e 's,^\(\s\s*\)\([0-9]\),\1{empty}\2,g' \
	   < "$${INPUT}" >> "$@.tmp.$$$$" \
	 && if [ x"`find '$@' -newer "$${INPUT}" 2>/dev/null`" != x ] ; then echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@ : SKIP (keep recently born competitor)"; rm -f "$@.tmp.$$$$"; \
	    else mv -f "$@.tmp.$$$$" "$@" ; fi

# Add other directory deps (not for local EXTRA_DIST) and generated contents
FULL_USER_MANUAL_DEPS = $(USER_MANUAL_DEPS) $(SHARED_DEPS) \
	$(top_builddir)/README.adoc-parsed \
	$(top_builddir)/INSTALL.nut.adoc-parsed \
	$(top_builddir)/UPGRADING.adoc-parsed \
	../TODO.adoc ../scripts/ufw/README.adoc
FULL_DEVELOPER_GUIDE_DEPS = $(DEVELOPER_GUIDE_DEPS) $(SHARED_DEPS) \
	../scripts/augeas/README.adoc ../TODO.adoc \
	../lib/README.adoc \
	../tools/nut-scanner/README.adoc
FULL_QA_GUIDE_DEPS = $(QA_GUIDE_DEPS) $(SHARED_DEPS) \
	$(top_builddir)/ci_build.adoc-parsed

user-manual.html user-manual.chunked user-manual.pdf: $(FULL_USER_MANUAL_DEPS)
developer-guide.html developer-guide.chunked developer-guide.pdf: $(FULL_DEVELOPER_GUIDE_DEPS)
packager-guide.html packager-guide.chunked packager-guide.pdf: packager-guide.txt asciidoc.conf
qa-guide.html qa-guide.chunked qa-guide.pdf: $(FULL_QA_GUIDE_DEPS)
release-notes.html release-notes.chunked release-notes.pdf: release-notes.txt $(top_builddir)/NEWS.adoc-parsed $(top_builddir)/UPGRADING.adoc-parsed asciidoc.conf
solaris-usb.html solaris-usb.chunked solaris-usb.pdf: solaris-usb.txt asciidoc.conf

# We intentionally evaluate that the original generated ChangeLog file is
# up to date (from dist or against git) every time we look at it. However
# we do want to skip re-generation of heavy file formats afterwards if it
# is still valid (for make, a need for re-evaluation without timestamps
# to look at is cause to run a recipe always). We define recipes outside
# the suffix-based handling and require *them* for default target builds.
ChangeLog.html ChangeLog.chunked ChangeLog.pdf: ChangeLog.txt .ChangeLog.adoc-parsed.latest asciidoc.conf
$(top_builddir)/docs/ChangeLog.html $(top_builddir)/docs/ChangeLog.chunked $(top_builddir)/docs/ChangeLog.pdf: ChangeLog.txt $(top_builddir)/ChangeLog.adoc-parsed asciidoc.conf
	@+if [ -s '$(@F)' ] && [ x"`find '$(@F)' -newer '$(top_builddir)/ChangeLog.adoc-parsed'`" != x ] ; then \
		echo "  DOC-CHANGELOG-RENDER-WRAPPER	SKIP: `pwd`/$(@F) already exists and is newer than ChangeLog.adoc-parsed" ; \
		if [ x"$(MAINTAINER_ASCIIDOCS_CHANGELOG_DEBUG)" != xno ] ; then \
			ls -lad `pwd` $@ ChangeLog.txt $(top_builddir)/ChangeLog.adoc-parsed asciidoc.conf $(top_builddir)/ChangeLog $(top_builddir)/ChangeLog.adoc .ChangeLog.adoc-parsed.latest || true ; \
			stat `pwd` $@ ChangeLog.txt $(top_builddir)/ChangeLog.adoc-parsed asciidoc.conf $(top_builddir)/ChangeLog $(top_builddir)/ChangeLog.adoc .ChangeLog.adoc-parsed.latest || true ; \
		fi ; \
		exit 0 ; \
	  else \
		echo "  DOC-CHANGELOG-RENDER-WRAPPER	`pwd`/$(@F) does not already exist or is older than ChangeLog.adoc-parsed" ; \
		if [ x"$(MAINTAINER_ASCIIDOCS_CHANGELOG_DEBUG)" != xno ] ; then \
			ls -lad `pwd` $@ ChangeLog.txt $(top_builddir)/ChangeLog.adoc-parsed asciidoc.conf $(top_builddir)/ChangeLog $(top_builddir)/ChangeLog.adoc .ChangeLog.adoc-parsed.latest || true ; \
			stat `pwd` $@ ChangeLog.txt $(top_builddir)/ChangeLog.adoc-parsed asciidoc.conf $(top_builddir)/ChangeLog $(top_builddir)/ChangeLog.adoc .ChangeLog.adoc-parsed.latest || true ; \
			time $(MAKE) $(AM_MAKEFLAGS) $(@F) || exit ; \
		else \
			$(MAKE) $(AM_MAKEFLAGS) $(@F) || exit ; \
		fi ; \
		echo "  DOC-CHANGELOG-RENDER-WRAPPER	`pwd`/$(@F): SUCCESS" ; \
	  fi

# Assume revisions are sorted in the file, from newest to oldest
# FIXME: If we ever get more than one file to chomp this way, make it a snippet
docinfo-since-v2.8.3.xml: docinfo.xml
	@OLDEST_REL="2.8.3"; \
	 IN_REVISION=false ; OLDEST_SEEN=false ; \
	 while read LINE ; do \
		case "$${LINE}" in \
			*'<revision>'*) if ! $$OLDEST_SEEN ; then IN_REVISION=true ; fi ;; \
			*'<revnumber>'"$${OLDEST_REL}"'</revnumber>'*) if $$IN_REVISION ; then echo '<revision>'; OLDEST_SEEN=true ; fi ; echo "$$LINE" ;; \
			*'<revnumber>'*'</revnumber>'*) if ! $$OLDEST_SEEN ; then if $$IN_REVISION ; then echo '<revision>'; fi ; echo "$$LINE" ; fi ;; \
			*'</revision>'*) if $$IN_REVISION ; then echo "$$LINE" ; fi ; IN_REVISION=false ;; \
			*'</revhistory>'*) echo "$$LINE" ;; \
			*) if ! $$OLDEST_SEEN || $$IN_REVISION ; then echo "$$LINE" ; fi ;; \
		esac ; \
	 done < "$?" > "$@".tmp \
	 && mv -f "$@".tmp "$@"

# Some versions of asciidoc ignore the argument of :docinfo: tag
# and require (TXT_BASE_NAME)-docinfo.xml to exist:
qa-guide-docinfo.xml: docinfo-since-v2.8.3.xml
	@rm -f '$@' || true
	@$(LN_S) '$?' '$@'

# Note: without the "-v", asciidoc (circa 8.6.2) sometimes hangs when
# generating the chunked HTML. In this case, export the environment
# variable ASCIIDOC_VERBOSE to "-v", ie:
#   $ ASCIIDOC_VERBOSE=-v make
# Note: `(top_)srcdir` and `(top_)builddir` must end with a path
# separator, or be empty -- so in all cases letting the resulting
# string resolve meaningfully in the filesystem during docs build.
A2X_DOCINFO_DIR=$(builddir)
A2X_COMMON_OPTS = $(ASCIIDOC_VERBOSE) \
    --attribute=icons \
    --xsltproc-opts="--nonet" \
    --xsltproc-opts="--stringparam nut.localdate \"`TZ=UTC date +%Y-%m-%d`\"" \
    --xsltproc-opts="--stringparam nut.localtime \"`TZ=UTC date +%H:%M:%S`\"" \
    --xsltproc-opts="--stringparam nut.nutversion \"$(PACKAGE_VERSION)\"" \
    --attribute=docinfodir="$${A2X_DOCINFO_DIR}" \
    --attribute=iconsdir="$(srcdir)/images" \
    --attribute=badges \
    --attribute=external_title \
    --attribute=tree_version="$(TREE_VERSION)" \
    --attribute=srcdir="$(abs_srcdir)/" \
    --attribute=builddir="$(abs_builddir)/" \
    --attribute=top_srcdir="$(abs_top_srcdir)/" \
    --attribute=top_builddir="$(abs_top_builddir)/" \
    --attribute=MAN_SECTION_API_BASE='$(MAN_SECTION_API_BASE)' \
    --attribute=MAN_SECTION_CFG_BASE='$(MAN_SECTION_CFG_BASE)' \
    --attribute=MAN_SECTION_CMD_SYS_BASE='$(MAN_SECTION_CMD_SYS_BASE)' \
    --attribute=MAN_SECTION_CMD_USR_BASE='$(MAN_SECTION_CMD_USR_BASE)' \
    --attribute=NUT_WEBSITE_BASE='$(NUT_WEBSITE_BASE)' \
    -a toc -a numbered --destination-dir=$${A2X_OUTDIR}
# NOTE: a2x newer than 8.6.8 says "--destination-dir" is only valid for HTML.
# As of version 8.6.9 it lies, and the argument is required for our distcheck
# (and does affect PDF builds, as found during work on collision-avoidance -
# true with at least asciidoc/a2x versions 9.0.0rc2).
# For more details see issues https://web.archive.org/web/20201207082352/https://github.com/asciidoc/asciidoc/issues/44
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

# Note that empirically it treats "destination-dir" as the source root for
# PDF generation (even though it claims the argument is ignored for non-HTML
# targets) so we have to provide the "images/" in this case. ONLY for PDF!

# Note we only remove the original target (if present), if it is a directory -
# e.g. created by "html-chunked" targets.

# NOTE: MKDIR_P may be defined via expanded $(top_builddir)/install-sh
# so should be run from $(abs_builddir) to be safe, as we jump around
# the build workspace
DOCBUILD_BEGIN = { \
    if test -n "$${A2X_OUTDIR}" && test "$${A2X_OUTDIR}" != '.' ; then \
        rm -rf "./$${A2X_OUTDIR}" || true ; \
        test -d "$@" && rm -rf "$@" || true ; \
        _CWD="`pwd`" && (cd '$(abs_builddir)' && $(MKDIR_P) "$${_CWD}/$${A2X_OUTDIR}") || exit ; \
        case "$${A2X_OUTDIR}" in \
            tmp/pdf.*) ln -s ../../images "./$${A2X_OUTDIR}" ; \
                case "$(@F)" in \
                    qa-guide.pdf) \
                        ln -s ../../docinfo-since-v2.8.3.xml "./$${A2X_OUTDIR}/docinfo.xml" ; \
                        ln -s ../../docinfo-since-v2.8.3.xml "./$${A2X_OUTDIR}/qa-guide-docinfo.xml" ;; \
                esac \
                ;; \
        esac; \
    else A2X_OUTDIR='.' ; fi; \
    if test -s "${builddir}/docbook-xsl.css" \
    && test -r "${builddir}/docbook-xsl.css" \
    && ! test -w "${builddir}/docbook-xsl.css" \
    ; then chmod u+w "${builddir}/docbook-xsl.css" ; fi ; \
    chmod -R u+w "./$${A2X_OUTDIR}" || true; \
}

# When moving "*" hope a2x did not make any "hidden" files
# like ".*" that would be required for resulting documents.
# Leave the "images/" dir there, though.
# Otherwise, we would have to `find` them all.
DOCBUILD_END = { \
    if test -n "$${A2X_OUTDIR}" && test "$${A2X_OUTDIR}" != '.' ; then \
        chmod -R u+w "./$${A2X_OUTDIR}" || true; \
        test -d "$@" && rm -rf "$@" || true ; \
        rm -f "./$${A2X_OUTDIR}/"*docinfo*.xml* || exit ; \
        mv -f "./$${A2X_OUTDIR}/$(@F)" ./ || exit ; \
        mv -f "./$${A2X_OUTDIR}/"*.* ./ 2>/dev/null || true ; \
        rm -rf "./$${A2X_OUTDIR}" ; \
    fi ; \
}

### Call the prep step consistently to create symlinks (out-of-tree)
### or just touch-files for peace of mind (in-tree builds). Then we
### use these path names (truncated "-prepped") now surely located
### in the builddir as the sources for rendered docs.
*.txt-prepped *.adoc-prepped: $(abs_top_builddir)/docs/.prep-src-docs

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
###.txt.txt-prepped: $(abs_top_builddir)/docs/.prep-src-docs
###.adoc.adoc-prepped: $(abs_top_builddir)/docs/.prep-src-docs

# NOTE: inference rules can have only one target before the colon (POSIX),
# so we use helper snippets to share code for *.adoc and *.txt sources
GENERATE_HTML_SINGLE = ( \
	A2X_OUTDIR="tmp/html-single.$(@F).$$$$" ; \
	echo "  DOC-HTML	Generating $@"; \
	$(DOCBUILD_BEGIN) ; RES=0; \
	$(A2X) $(A2X_COMMON_OPTS) --attribute=xhtml11_format --format=xhtml --xsl-file=$(srcdir)/xhtml.xsl '$<' || RES=$$? ; \
	$(DOCBUILD_END) ; \
	case "$(@F)" in \
	    *ChangeLog*) \
	        if [ -s '$(top_builddir)/ChangeLog' ] ; then \
	            $(MAKE) $(AM_MAKEFLAGS) "`basename '$(@F)'`"-contentchecked || RES=$$? ; \
	            if [ "$$RES" != 0 ] ; then \
	                echo "  DOC-HTML	Generating $@ (retry once)" >&2; \
	                rm -f "$@"; \
	                A2X_OUTDIR="tmp/html-single.$(@F).$$$$-retry" ; \
	                $(DOCBUILD_BEGIN) ; RES=0; rm -f "`basename '$(@F)'`"-contentchecked || true ; \
	                $(A2X) $(A2X_COMMON_OPTS) --attribute=xhtml11_format --format=xhtml --xsl-file=$(srcdir)/xhtml.xsl '$<' || RES=$$? ; \
	                $(DOCBUILD_END) ; \
	                $(MAKE) $(AM_MAKEFLAGS) "`basename '$(@F)'`"-contentchecked || RES=$$? ; \
	            fi ; \
	        fi ;; \
	esac ; \
	exit $$RES ; \
)

*.html: common.xsl xhtml.xsl
.txt-prepped.html:
	+@$(GENERATE_HTML_SINGLE)
.adoc-prepped.html:
	+@$(GENERATE_HTML_SINGLE)

# Note: extra age check here because *.chunked is a directory and not all
# "make" implementations check its age vs. source files, just always build:
GENERATE_HTML_CHUNKED = ( \
	if [ -d "$@" ] && [ x"`find '$@' -newer "$<" 2>/dev/null`" != x ] ; then \
	    echo "  DOC-HTML-CHUNKED	SKIP: keep existing $@"; \
	    exit 0 ; \
	fi ; \
	A2X_OUTDIR="tmp/html-chunked.$(@F).$$$$" ; \
	echo "  DOC-HTML-CHUNKED	Generating $@"; \
	$(DOCBUILD_BEGIN) ; RES=0; \
	$(A2X) $(A2X_COMMON_OPTS) --attribute=chunked_format --format=chunked --xsl-file=$(srcdir)/chunked.xsl '$<' || RES=$$? ; \
	$(DOCBUILD_END) ; exit $$RES ; \
)

*.chunked: common.xsl chunked.xsl
.txt-prepped.chunked:
	@$(GENERATE_HTML_CHUNKED)
.adoc-prepped.chunked:
	@$(GENERATE_HTML_CHUNKED)

# Note: non-HTML a2x modes may ignore the destination directory
GENERATE_PDF = ( \
	A2X_OUTDIR="tmp/pdf.$(@F).$$$$" ; \
	echo "  DOC-PDF	Generating $@"; \
	$(DOCBUILD_BEGIN) ; RES=0; \
	if [ -s "$${A2X_OUTDIR}/docinfo.xml" ] ; then A2X_DOCINFO_DIR="$${A2X_OUTDIR}"; fi ; \
	$(A2X) $(A2X_COMMON_OPTS) --attribute=pdf_format --format=pdf -a docinfo1 '$<' || RES=$$? ; \
	$(DOCBUILD_END) ; exit $$RES ; \
)

*.pdf: docinfo.xml
qa-guide.pdf: docinfo-since-v2.8.3.xml qa-guide-docinfo.xml
.txt-prepped.pdf:
	@$(GENERATE_PDF)
.adoc-prepped.pdf:
	@$(GENERATE_PDF)

# Used below for spellcheck and for .prep-src-docs
SPELLCHECK_SRC_DEFAULT = $(ALL_TXT_SRC) \
	asciidoc-vars.conf \
	../ci_build.adoc ../README.adoc ../NEWS.adoc \
	../INSTALL.nut.adoc ../UPGRADING.adoc \
	../TODO.adoc ../scripts/ufw/README.adoc \
	../scripts/augeas/README.adoc ../lib/README.adoc \
	../tools/nut-scanner/README.adoc \
	../AUTHORS ../COPYING ../LICENSE-GPL2 ../LICENSE-GPL3 ../LICENSE-DCO

if HAVE_ASPELL
# Non-interactively spell check all documentation source files.
# This is useful for Buildbot and automated QA processing
# FIXME: how to present output (std{out,err}, single file or per target)?
# NOTE: ../ChangeLog is nowadays generated from commit messages, so
# its spelling (or errors in that) are not fixable and thus irrelevant.
# Similarly for the ../INSTALL file that is prepared by autoconf and not
# tracked as a source file by NUT Git repository.
# Note that `docs/asciidoc-vars.conf` is included into docs and so impacts
# their resulting spellcheck verdicts.
SPELLCHECK_SRC = $(SPELLCHECK_SRC_DEFAULT)

# Directory SPELLCHECK_SRC files are relative to. Overridden by other Makefiles.
SPELLCHECK_SRCDIR = $(srcdir)
SPELLCHECK_BUILDDIR = $(builddir)

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
# Note: If there is a need to use filter path (e.g. in mingw/msys2 builds),
# it must be before --mode=tex (-t) option!
ASPELL_NUT_TEXMODE_ARGS =
if HAVE_ASPELL_FILTER_TEX_PATH
ASPELL_NUT_TEXMODE_ARGS += --filter-path="$(ASPELL_FILTER_TEX_PATH)"
endif HAVE_ASPELL_FILTER_TEX_PATH
ASPELL_NUT_TEXMODE_ARGS += -t
ASPELL_ENV_LANG = en.UTF-8
ASPELL_OUT_NOTERRORS = (^[ \t]*[\*\@]|^$$)

# WARNING: The percent wildcard is a GNU extension; otherwise we need
# a ".txt.txt-spellchecked" type of rule and files like "README" all
# renamed to *.txt, or lots of rules for files without the extensions.
# Maybe this will get simplified with renaming to *.adoc though ;)
#
# Other Makefiles have a relatively simple life, dealing with just a
# few texts and name/extension patterns in their directories.
#?#.txt.txt-spellchecked: Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#%-spellchecked: % Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#*-spellchecked */*-spellchecked: $(@:-spellchecked=) $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#
# NOTE: For some reason, at least GNU make insists on bogus calls:
#   update target 'asciidoc-vars.conf' due to: asciidoc-vars.conf-spellchecked
# when we e.g. `make dist` after a `make spellcheck` and ended up
# with removed and touched (emptied) file, only this one so far.
#
# NOTE: This portable rule RELIES on just one SPELLCHECK_SRC defined
# at a time, with an outer Makefile caller ensuring the looping:

SPELLCHECK_RECIPE_DEBUG_STREAM = /dev/null
#SPELLCHECK_RECIPE_DEBUG_STREAM = &2

# Note: if we do an interactive spell-check, it updates "nut.dict"
# timestamp even if contents remain. If the caller left a copy of
# the file as "$(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting",
# and/or "$(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-interactive",
# and the dictionary was NOT in fact modified, restore the timestamp.
$(SPELLCHECK_BUILDDIR)/$(SPELLCHECK_SRC_ONE)-spellchecked: $(SPELLCHECK_SRCDIR)/$(SPELLCHECK_SRC_ONE) $(abs_top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
	@LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	 if test x"$(SPELLCHECK_SRC_ONE)" = x ; then echo "SKIP: Bogus spellcheck call for empty target filename (with make target $@ from `pwd`)" >&2 ; exit 0; fi; \
	 case "$@" in *-spellchecked) ;; *) echo "SKIP: Bogus spellcheck call for non '*-spellchecked' target filename (with make target $@ from `pwd`)" >&2 ; exit 0;; esac; \
	 rm -f "$@" || true ; \
	 $(MKDIR_P) "$(@D)" || exit ; \
	 REPORT_SRCDIR="$(SPELLCHECK_SRCDIR)"; \
	 REPORT_SRC_ONE="$(SPELLCHECK_SRC_ONE)"; \
	 REPORT_PREFIX="" ; \
	 case "$(SPELLCHECK_SRC_ONE)" in \
		/*) ;; \
		*/*)	if [ x"$${REPORT_SRCDIR}" = x ] ; then \
				echo EMPTY >$(SPELLCHECK_RECIPE_DEBUG_STREAM) ; \
				REPORT_SRCDIR="`dirname '$(SPELLCHECK_SRC_ONE)'`"; \
			else \
				echo "APPEND: SPELLCHECK_SRCDIR='$(SPELLCHECK_SRCDIR)' SPELLCHECK_SRC_ONE='$(SPELLCHECK_SRC_ONE)' dirname='`dirname '$(SPELLCHECK_SRC_ONE)'`'" >$(SPELLCHECK_RECIPE_DEBUG_STREAM) ; \
				REPORT_SRCDIR="$${REPORT_SRCDIR}/`dirname '$(SPELLCHECK_SRC_ONE)'`"; \
			fi ; \
			REPORT_SRC_ONE="`basename '$(SPELLCHECK_SRC_ONE)'`"; \
			;; \
		*) ;; \
	 esac; \
	 if [ x"$${REPORT_SRCDIR}" != x ] ; then \
		tmpREPORT_PREFIX="NUT source root :: $${REPORT_SRCDIR} :: " ; \
		REPORT_SRCDIR="`cd "$${REPORT_SRCDIR}" && { pwd >$(SPELLCHECK_RECIPE_DEBUG_STREAM) ; pwd | sed 's|^'"$(abs_top_srcdir)"'/*||' ; }`" \
		|| { REPORT_SRCDIR="$(SPELLCHECK_SRCDIR)" ; REPORT_SRC_ONE="$(SPELLCHECK_SRC_ONE)" ; REPORT_PREFIX="" ; } ; \
	 fi ; \
	 echo "=== Got REPORT_SRCDIR='$${REPORT_SRCDIR}'" >$(SPELLCHECK_RECIPE_DEBUG_STREAM) ; \
	 case "$${REPORT_SRCDIR}" in \
		"") ;; \
		*/) ;; \
		*)  REPORT_SRCDIR="$${REPORT_SRCDIR}/" ;; \
	 esac ; \
	 if [ x"$(SPELLCHECK_INTERACTIVE)" = xtrue ] ; then \
		echo "  ASPELL   Spell checking (interactively) on $${REPORT_PREFIX}$${REPORT_SRCDIR}$${REPORT_SRC_ONE}"; \
		LANG=$(ASPELL_ENV_LANG) LC_ALL=$(ASPELL_ENV_LANG) $(ASPELL) check $(ASPELL_NUT_COMMON_ARGS) '$(SPELLCHECK_SRCDIR)/$(SPELLCHECK_SRC_ONE)' || exit ; \
		if [ -s $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-interactive ] && [ -s $(abs_srcdir)/$(NUT_SPELL_DICT) ] && diff $(abs_srcdir)/$(NUT_SPELL_DICT) $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-interactive >/dev/null ; then \
			touch -r $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-interactive $(abs_srcdir)/$(NUT_SPELL_DICT) ; \
		else \
		if [ -s $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting ] && [ -s $(abs_srcdir)/$(NUT_SPELL_DICT) ] && diff $(abs_srcdir)/$(NUT_SPELL_DICT) $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting >/dev/null ; then \
			touch -r $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting $(abs_srcdir)/$(NUT_SPELL_DICT) ; \
		fi ; fi ; \
	 else \
		echo "  ASPELL   Spell checking on $${REPORT_PREFIX}$${REPORT_SRCDIR}$${REPORT_SRC_ONE}"; \
	 fi ; \
	 OUT="`(sed 's,^\(.*\)$$, \1,' | $(ASPELL) -a $(ASPELL_NUT_TEXMODE_ARGS) $(ASPELL_NUT_COMMON_ARGS) 2>&1) < '$(SPELLCHECK_SRCDIR)/$(SPELLCHECK_SRC_ONE)'`" \
		&& { if test -n "$$OUT" ; then OUT="`echo "$$OUT" | $(EGREP) -b -v '$(ASPELL_OUT_NOTERRORS)' `" ; fi; \
		     test -z "$$OUT" ; } \
		|| { RES=$$? ; \
		     echo "FAILED : Aspell reported errors here:" >&2 \
		     && echo "----- vvv" >&2 \
		     && echo "$$OUT" >&2 \
		     && echo "----- ^^^" >&2 ; \
		     exit $$RES; } ; \
	 touch "$@"

# If NUT_MAKE_SKIP_FANOUT!=true we have a hack to spellcheck certain files more
# quickly (in parallel). This approach is constrained to files with a known
# extension and located in same directory as the Makefile that called them
# (extension-less docs and relative links are built sequentially).
SPELLCHECK_AUTO_ONE = ( \
	SPELLCHECK_SRC_ONE="`basename '$?'`" ; \
	rm -f "$@".failed ; \
	$(MAKE) $(AM_MAKEFLAGS) -k -s -f "$(abs_top_builddir)/docs/Makefile" SPELLCHECK_SRC="" SPELLCHECK_SRC_ONE="$${SPELLCHECK_SRC_ONE}" SPELLCHECK_BUILDDIR="$(SPELLCHECK_BUILDDIR)" SPELLCHECK_SRCDIR="$(SPELLCHECK_SRCDIR)" "$(SPELLCHECK_BUILDDIR)/$${SPELLCHECK_SRC_ONE}-spellchecked" \
	|| { RES=$$? ; touch "$@".failed; exit $$RES; } \
)

.txt.txt-spellchecked-auto:
	+@$(SPELLCHECK_AUTO_ONE)

.adoc.adoc-spellchecked-auto:
	+@$(SPELLCHECK_AUTO_ONE)

.in.in-spellchecked-auto:
	+@$(SPELLCHECK_AUTO_ONE)

.sample.sample-spellchecked-auto:
	+@$(SPELLCHECK_AUTO_ONE)

.conf.conf-spellchecked-auto:
	+@$(SPELLCHECK_AUTO_ONE)

spellcheck:
	@if test "$(SPELLCHECK_ENV_DEBUG)" = detailed ; then \
		echo "ASPELL DEBUG : information about the setup follows:"; \
		LANG=$(ASPELL_ENV_LANG); LC_ALL=$(ASPELL_ENV_LANG); export LANG; export LC_ALL; \
		$(ASPELL) --help || true; \
		(command -v dpkg) && ( dpkg -l | grep -i aspell ) || true ; \
		echo "ASPELL automatic execution line is : ( sed 's,^\(.*\)$$, \1,' < docfile.txt | $(ASPELL) -a $(ASPELL_NUT_TEXMODE_ARGS) $(ASPELL_NUT_COMMON_ARGS) | $(EGREP) -b -v '$(ASPELL_OUT_NOTERRORS)' )" ; \
		echo "ASPELL proceeding to spellchecking job..."; \
	 else true; fi
	+@FAILED="" ; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	 if [ x"$(NUT_MAKE_SKIP_FANOUT)" = xtrue ] ; then \
		for docsrc in $(SPELLCHECK_SRC); do \
			if test "$(SPELLCHECK_ENV_DEBUG)" != no ; then \
				echo "ASPELL MAKEFILE DEBUG: Will see from `pwd` if '$(SPELLCHECK_SRCDIR)/$${docsrc}-spellchecked' is up to date" >&2; \
			else true ; fi ; \
			$(MAKE) $(AM_MAKEFLAGS) -s -f "$(abs_top_builddir)/docs/Makefile" SPELLCHECK_SRC="" SPELLCHECK_SRC_ONE="$${docsrc}" SPELLCHECK_BUILDDIR="$(SPELLCHECK_BUILDDIR)" SPELLCHECK_SRCDIR="$(SPELLCHECK_SRCDIR)" "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked" \
			|| FAILED="$$FAILED $(SPELLCHECK_SRCDIR)/$$docsrc"; \
		done ; \
	 else \
		SPELLCHECK_AUTO_TGT="`for docsrc in $(SPELLCHECK_SRC); do case "$${docsrc}" in */*) ;; *.adoc|*.txt|*.in|*.conf|*.sample) printf '%s ' "$${docsrc}-spellchecked-auto" ;; esac ; done`" ; \
		SPELLCHECK_NOEXT_DOCS="`for docsrc in $(SPELLCHECK_SRC); do case "$${docsrc}" in */*) printf '%s ' "$${docsrc}" ;; *.adoc|*.txt|*.in|*.conf|*.sample) ;; *) printf '%s ' "$${docsrc}" ;; esac ; done`" ; \
		if test "$(SPELLCHECK_ENV_DEBUG)" != no ; then \
			echo "ASPELL MAKEFILE DEBUG: from `pwd`: SPELLCHECK_NOEXT_DOCS='$${SPELLCHECK_NOEXT_DOCS}' SPELLCHECK_AUTO_TGT='$${SPELLCHECK_AUTO_TGT}'" ; \
		else true ; fi ; \
		if [ x"$${SPELLCHECK_AUTO_TGT}" != x ] ; then \
			$(MAKE) $(AM_MAKEFLAGS) -k -s -f "$(abs_top_builddir)/docs/Makefile" SPELLCHECK_SRC="" SPELLCHECK_BUILDDIR="$(SPELLCHECK_BUILDDIR)" SPELLCHECK_SRCDIR="$(SPELLCHECK_SRCDIR)" $${SPELLCHECK_AUTO_TGT} ; \
			FAILED="`for docsrc in $(SPELLCHECK_SRC); do if [ -e "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked-auto.failed" ] ; then printf '%s ' "$(SPELLCHECK_SRCDIR)/$${docsrc}" ; rm -f "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked-auto.failed" ; fi ; done`" ; \
		fi ; \
		if [ x"$${SPELLCHECK_NOEXT_DOCS}" != x ] ; then \
			for docsrc in $${SPELLCHECK_NOEXT_DOCS} ; do \
				$(MAKE) $(AM_MAKEFLAGS) -k -s -f "$(abs_top_builddir)/docs/Makefile" SPELLCHECK_SRC="" SPELLCHECK_SRC_ONE="$${docsrc}" SPELLCHECK_BUILDDIR="$(SPELLCHECK_BUILDDIR)" SPELLCHECK_SRCDIR="$(SPELLCHECK_SRCDIR)" "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked" \
				|| FAILED="$$FAILED $(SPELLCHECK_SRCDIR)/$$docsrc"; \
			done ; \
		fi ; \
	 fi ; \
	 if test -n "$$FAILED" ; then \
		echo "=====================================================================" ; \
		echo "FAILED automatic spellcheck for the following sources (relative to `pwd`) using custom dictionary file '$(NUT_SPELL_DICT)': $$FAILED" ; \
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
	    WORDLIST="`tail -n +2 < '$(?)' | sort | uniq`"; \
	    WORDCOUNT="`echo "$$WORDLIST" | wc -l`"; \
	    head -1 < "$?" | while read P L C E ; do echo "$$P $$L $$WORDCOUNT $$E"; break; done ; \
	    echo "$$WORDLIST"; \
	 ) > "$@"
	@cp -f "$@" "$(abs_builddir)/$(NUT_SPELL_DICT)"
	@if [ "$(abs_builddir)" != "$(abs_srcdir)" ] ; then \
	    cp -f "$@" "$?" || true ; \
	    cp -f "$(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting" "$(abs_srcdir)/" || true ; \
	 fi

DISTCLEANFILES += $(NUT_SPELL_DICT).bak-pre-sorting $(NUT_SPELL_DICT).bak-pre-interactive .$(NUT_SPELL_DICT).sorted $(NUT_SPELL_DICT).sorted

# NOTE: In "make SPELLCHECK_INTERACTIVE=true ${docsrc}-spellchecked",
# after an interactive "aspell check" we follow-up by a run of usual
# non-interactive spell-checker to verify that the developer actually
# has fixed all of the files that the tool had concerns about, and
# that the touch-file is updated if the file is okay (to speed up
# any future re-runs). We also must update all relevant *-spellchecked
# touch-files after "make spellcheck-sortdict" which updates "nut.dict"
# file which is a prerequisite for docs checks.
# After the (possibly SUBDIR-based) run we may report to the developer
# that their dictionary was updated and may need a Git recommit - either
# if it did change, or if caller's SPELLCHECK_REPORT_MAYBE_UPDATED_DICT=yes.
SPELLCHECK_REPORT_MAYBE_UPDATED_DICT = no
spellcheck-interactive:
	@cp -pf $(abs_srcdir)/$(NUT_SPELL_DICT) $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-interactive
	+@FAILED="" ; for docsrc in $(SPELLCHECK_SRC); do \
		if test "$(SPELLCHECK_ENV_DEBUG)" != no ; then \
			echo "ASPELL (INTERACTIVE) MAKEFILE DEBUG: Will see from `pwd` if '$(SPELLCHECK_SRCDIR)/$${docsrc}-spellchecked' is up to date" >&2; \
		else true ; fi ; \
		$(MAKE) $(AM_MAKEFLAGS) -s -f "$(abs_top_builddir)/docs/Makefile" SPELLCHECK_INTERACTIVE="true" SPELLCHECK_SRC="" SPELLCHECK_SRC_ONE="$${docsrc}" SPELLCHECK_BUILDDIR="$(SPELLCHECK_BUILDDIR)" SPELLCHECK_SRCDIR="$(SPELLCHECK_SRCDIR)" "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked" \
		|| FAILED="$$FAILED $(SPELLCHECK_SRCDIR)/$$docsrc"; \
	 done ; \
	 if test -n "$$FAILED" ; then \
		echo "FAILED interactive spellcheck for the following sources (relative to `pwd`) using custom dictionary file '$(NUT_SPELL_DICT)': $$FAILED" >&2 ; \
		exit 1; \
	 fi ; \
	 $(MAKE) $(AM_MAKEFLAGS) spellcheck-sortdict || exit ; \
	 for docsrc in $(SPELLCHECK_SRC); do \
		if test -e "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked" ; then \
			touch "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked" ; \
		fi ; \
	 done
	@if [ "$(SPELLCHECK_REPORT_MAYBE_UPDATED_DICT)" != no ] \
	 || ( [ -s $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-interactive ] && [ -s $(abs_srcdir)/$(NUT_SPELL_DICT) ] \
	      && ! diff $(abs_srcdir)/$(NUT_SPELL_DICT) $(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-interactive >/dev/null ) \
	 ; then \
		echo "------------------------------------------------------------------------"; \
		echo "Custom dictionary file $(NUT_SPELL_DICT) may have been updated now."; \
		echo "Use e.g. 'git add -p docs/$(NUT_SPELL_DICT) && git checkout -- docs/$(NUT_SPELL_DICT) && make spellcheck-sortdict && git add -p docs/$(NUT_SPELL_DICT)'"; \
		echo "to review changes (please DO NOT REMOVE LINES that aspell chose to drop,"; \
		echo "because other systems might not know these words in their system dictionaries)"; \
		echo "------------------------------------------------------------------------" ; \
	 fi

else !HAVE_ASPELL
# This rule would probably just fail; normally with no ASPELL there are no callers for it
*/*-spellchecked *-spellchecked: Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
	@echo "  SKIP-ASPELL   $@ : Documentation spell check not available since 'aspell' was not found (or missing its English dictionary)." >&2
spellcheck:
	@echo "Documentation spell check not available since 'aspell' was not found (or missing its English dictionary)."
spellcheck-interactive:
	@echo "Documentation spell check not available since 'aspell' was not found (or missing its English dictionary)."
endif !HAVE_ASPELL

# Note that NUT_SPELL_DICT may be an include snippet without the header line.
# To exclude files like `docs/nut.dict` or `nut-website.dict(.addons)` from
# the usage lookups, we assume that a `*.dict*` pattern fits any used names.
# Entries prefixed with '+++' mean something used in NUT sources in context
# that aspell is likely to treat as a word (standalone or surrounded by certain
# chars); otherwise in entries prefixed with '---' we print hit counts and
# contents (if any, ending with '^^^') for the character pattern across the
# whole Git-tracked codebase (case-insensitively for good measure).
# Note this can take 5-10 minutes!
# TOTHINK: Constrain to (caller-specified or default) SPELLCHECK_SRC?
$(NUT_SPELL_DICT).usage-report: $(NUT_SPELL_DICT)
	@echo "Preparing $@"; \
	 LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	 grep -v -E '^personal_ws' < $? \
	 | while read W ; do ( \
		cd "$(abs_top_srcdir)" || exit ; \
		git grep -q "$$W" -- ':!*.dict*' || git grep -qE "[0-9_,./\ -]$$W[0-9_,./\ -]" -- ':!*.dict*' ) \
		&& echo "+++ $$W" \
		|| ( \
			HITS_CS="`git grep "$$W" -- ':!*.dict*'`" || true; \
			HITS_CI="`git grep -i "$$W" -- ':!*.dict*'`" || true; \
			if [ -n "$$HITS_CS" ] ; then HITC_CS="`echo "$$HITS_CS" | wc -l`" ; else HITC_CS=0; fi; \
			if [ -n "$$HITS_CI" ] ; then HITC_CI="`echo "$$HITS_CI" | wc -l`" ; else HITC_CI=0; fi; \
			printf '%s (%d case-sensitive/%d case-insensitive)\n' "--- $$W" "$$HITC_CS" "$$HITC_CI"; \
			if [ "$$HITC_CS" != 0 ] ; then echo "$$HITS_CS" ; echo "^^^"; else \
				if [ "$$HITC_CI" != 0 ] ; then echo "$$HITS_CI" ; echo "^^^"; fi; \
			fi; \
		); \
	 done > "$@.tmp.$$$$" \
	 && test -f "$@.tmp.$$$$" \
	 && mv -f "$@.tmp.$$$$" "$@"
	@echo "Reporting words from $? possibly not used in current inspected code base revision under $(abs_top_srcdir)" >&2 ; \
	 grep -E '^--- ' < "$@" | grep '(0 ' || echo "SUCCESS: None found"

CLEANFILES += $(NUT_SPELL_DICT).usage-report.tmp
MAINTAINERCLEANFILES += $(NUT_SPELL_DICT).usage-report

# When building out-of-tree, be sure to have all asciidoc resources
# under the same dir structure (tool limitation)
PREP_SRC = $(EXTRA_DIST) $(SPELLCHECK_SRC_DEFAULT)

# NOTE: Some "make" implementations prefix a relative or absent path to
# the filenames in PREP_SRC, others (e.g. Sun make) prepend the absolute
# path to locate the sources, so we end up with bogus trees under docs/.
# Code below tries to detect and truncate this mess, including possible
# source texts located in/under parent dirs.
# We also handle man page links (section-aware) for platforms where they
# differ from common defaults.
# NOTE: MKDIR_P may be defined via expanded $(top_builddir)/install-sh
# so should be run from $(abs_builddir) to be safe, as we jump around
# the build workspace
prep-src-docs: $(abs_top_builddir)/docs/.prep-src-docs
$(abs_top_builddir)/docs/.prep-src-docs: $(PREP_SRC) Makefile
	@cd "$(@D)" || exit ; \
	 linkroot="$(abs_builddir)" ; \
	 MAN_SECTIONS_DEFAULT=false ; \
	 if [ x"$(MAN_SECTION_API)$(MAN_SECTION_CFG)$(MAN_SECTION_CMD_SYS)$(MAN_SECTION_CMD_USR)" = x3581 ] ; then \
	    MAN_SECTIONS_DEFAULT=true ; \
	 fi ; \
	 if test x"$(abs_srcdir)" = x"$(abs_builddir)" ; then \
	    COUNT=0; \
	    for F in $(PREP_SRC) ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed 's#^$(abs_top_srcdir)/*#./#'`"; \
	                if test x"$${linkroot}" = x"$(abs_builddir)" ; then \
	                    linkroot="$(abs_top_builddir)" ; \
	                    cd "$(abs_top_builddir)" ; \
	                fi ;; \
	        esac ; \
	        if $$MAN_SECTIONS_DEFAULT ; then \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 $(NUT_WEBSITE_BASE)/,' ; \
	        else \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 $(NUT_WEBSITE_BASE)/,' \
	                -e 's,\(linkman:[^ []*\[\)3\],\1$(MAN_SECTION_API)],g' \
	                -e 's,\(linkman:[^ []*\[\)5\],\1$(MAN_SECTION_CFG)],g' \
	                -e 's,\(linkman:[^ []*\[\)8\],\1$(MAN_SECTION_CMD_SYS)],g' \
	                -e 's,\(linkman:[^ []*\[\)1\],\1$(MAN_SECTION_CMD_USR)],g' ; \
	        fi < "$${F}" > "$${F}-prepped" || exit ; \
	        COUNT="`expr $$COUNT + 1`" ; \
	    done ; \
	    if ! test -e "$@" ; then touch "$@" ; fi ; \
	 else \
	    COUNT=30 ; \
	    touch "$@.$$$$" ; \
	    while test -e "$@.working" -a "$$COUNT" -gt 0 ; do sleep 1; COUNT="`expr $$COUNT - 1`"; done ; \
	    touch "$@.working" ; \
	    if test -n "`find "$@" -newer "$@.$$$$" 2>/dev/null`" ; then \
	        rm -f "$@.$$$$" "$@.working" ; \
	        exit 0; \
	    fi ; \
	    rm -f "$@.$$$$" ; \
	    COUNT=0; \
	    linksrcroot="$(abs_srcdir)" ; \
	    for F in `echo $(PREP_SRC) | tr ' ' '\n' | sort -n | uniq` ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed 's#^$(abs_top_srcdir)/*#./#'`"; \
	                if test x"$${linkroot}" = x"$(abs_builddir)" ; then \
	                    linkroot="$(abs_top_builddir)" ; \
	                    linksrcroot="$(abs_top_srcdir)" ; \
	                    cd "$(abs_top_builddir)" ; \
	                fi ;; \
	            "$(srcdir)"/*) F="`echo "$$F" | sed 's#^$(srcdir)/*#./#'`" ;; \
	            */*) ;; \
	            *) \
	                linkroot="$(abs_builddir)" ; \
	                linksrcroot="$(abs_srcdir)" ; \
	                cd "$(abs_top_builddir)" ;; \
	        esac ; \
	        D="`dirname "$$F"`" ; \
	        (cd '$(abs_builddir)' && $(MKDIR_P) "$${linkroot}/$$D") || { rm -f "$@.working" ; exit 1 ; } ; \
	        if ! test -s "$${linkroot}/$$F" && test -s "$${linksrcroot}/$$F" ; then \
	            echo "  LN     '$${linksrcroot}/$$F' => '$${linkroot}/$$F' (PWD = '`pwd`')" ; \
	            ln -fs "$${linksrcroot}/$$F" "$${linkroot}/$$F" || { rm -f "$@.working" ; exit 1 ; } ; \
	            COUNT="`expr $$COUNT + 1`" ; \
	        fi ; \
	        case "$$F" in \
	            *.txt|*.adoc) IS_TEXT=true ;; \
	            *.*) IS_TEXT=false ;; \
	            *) IS_TEXT=true ;; \
	        esac; \
	        if $$IS_TEXT ; then \
	            grep -w linkman "$${linkroot}/$${F}" > /dev/null || IS_TEXT=false ; \
	        fi ; \
	        if $$MAN_SECTIONS_DEFAULT || ! $$IS_TEXT ; then \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 $(NUT_WEBSITE_BASE)/,' ; \
	        else \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 $(NUT_WEBSITE_BASE)/,' \
	                -e 's,\(linkman:[^ []*\[\)3\],\1$(MAN_SECTION_API)],g' \
	                -e 's,\(linkman:[^ []*\[\)5\],\1$(MAN_SECTION_CFG)],g' \
	                -e 's,\(linkman:[^ []*\[\)8\],\1$(MAN_SECTION_CMD_SYS)],g' \
	                -e 's,\(linkman:[^ []*\[\)1\],\1$(MAN_SECTION_CMD_USR)],g' ; \
	        fi < "$${linkroot}/$${F}" > "$${linkroot}/$${F}-prepped" \
	        || { rm -f "$@.working" ; exit 1 ; } ; \
	        COUNT="`expr $$COUNT + 1`" ; \
	    done ; \
	 fi ; \
	 if test "$$COUNT" -gt 0 -o ! -e "$@" ; then touch "$@" ; fi
	@rm -f "$@.working"

# Dirs to clean, etc.
clean-local:
	$(AM_V_at)rm -rf *.chunked *.bak tmp
	$(AM_V_at)for F in $(PREP_SRC) ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed 's#^$(abs_top_srcdir)/*#./#'`"; cd "$(abs_top_builddir)" ;; \
	        esac ; \
	        if test x"$(abs_srcdir)" != x"$(abs_builddir)" ; then \
	            if test -L "$$F" || test -h "$$F" ; then \
	                rm -f "$$F" ; \
	            fi ; \
	        fi ; \
	        rm -f "$${F}-prepped" ; \
	    done ; \
	    rm -f "$(abs_top_builddir)/docs/.prep-src-docs"*

.PHONY: html html-chunked html-single pdf man
