# Network UPS Tools: main docs

MAINTAINERCLEANFILES = Makefile.in .dirstamp
EXTRA_DIST =

all: doc

# Is "egrep == grep -E" always valid? (maybe all a job for configure.ac)
#EGREP = egrep
EGREP = grep -E

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
 release-notes.txt ChangeLog.txt solaris-usb.txt

ASPELL_FILTER_PATH = @ASPELL_FILTER_PATH@
NUT_SPELL_DICT = nut.dict
EXTRA_DIST += $(ALL_TXT_SRC) $(SHARED_DEPS) $(IMAGE_FILES) \
 $(IMAGE_LOGO_FILES) $(IMAGE_LOGO_FILES_JENKINS_NUT) $(CABLES_IMAGES) $(NUT_SPELL_DICT) \
 docinfo.xml common.xsl xhtml.xsl chunked.xsl asciidoc.txt asciidoc-vars.conf

ASCIIDOC_HTML_SINGLE = user-manual.html \
	developer-guide.html \
	packager-guide.html \
	release-notes.html \
	ChangeLog.html \
	solaris-usb.html \
	cables.html \
	FAQ.html

ASCIIDOC_HTML_CHUNKED = user-manual.chunked \
	developer-guide.chunked \
	packager-guide.chunked \
	release-notes.chunked \
	ChangeLog.chunked \
	solaris-usb.chunked \
	cables.chunked \
	FAQ.chunked

ASCIIDOC_PDF = user-manual.pdf \
	developer-guide.pdf \
	packager-guide.pdf \
	release-notes.pdf \
	ChangeLog.pdf \
	solaris-usb.pdf \
	cables.pdf \
	FAQ.pdf

SUBDIRS = man cables
SUFFIXES = .txt .html .pdf -spellchecked .txt-prepped

# This list is defined by configure script choices and options:
CHECK_LOCAL_TARGETS = @DOC_CHECK_LIST@
if WITH_SPELLCHECK
CHECK_LOCAL_TARGETS += spellcheck
endif WITH_SPELLCHECK
check-local: $(CHECK_LOCAL_TARGETS)

# Make sure sources are there for out-of-tree builds:
all-local all-am-local \
@DOC_BUILD_LIST@ $(ASCIIDOC_PDF) $(ASCIIDOC_HTML_SINGLE) $(ASCIIDOC_HTML_CHUNKED): $(abs_top_builddir)/docs/.prep-src-docs

# This list is defined by configure script choices and options:
doc: $(abs_top_builddir)/docs/.prep-src-docs @DOC_BUILD_LIST@

# This target can be called by developers to go around the configure
# script choices at their risk (e.g. missing tools are possible):
docs: pdf html-single html-chunked man-man html-man

all-docs: docs

check-docs: check-pdf check-html-single check-html-chunked check-man

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
# Note: default `make all` in the man directory caters to drivers etc.
# chosen during configure script execution. The "all-man" and "all-html"
# rules build everything documented.
check-man all-man man-man all-html html-man:
	+cd $(abs_top_builddir)/docs/man/ && $(MAKE) $(AM_MAKEFLAGS) -f Makefile $@

man:
	+cd $(abs_top_builddir)/docs/man/ && $(MAKE) $(AM_MAKEFLAGS) -f Makefile all

CLEANFILES = *.xml *.html *.pdf *-spellchecked docbook-xsl.css docinfo.xml.in.tmp
CLEANFILES += $(top_builddir)/INSTALL.nut $(top_builddir)/UPGRADING $(top_builddir)/NEWS $(top_builddir)/ChangeLog.adoc $(top_builddir)/README
CLEANFILES += $(top_builddir)/*.adoc-parsed *.adoc-parsed

### TODO: general automatic dependency generation

# Prepare text files (currently a manually tracked list)
# with known presence of GitHub links to convert them from
# short <hash><number> notation into asciidoc link markup
# before rendering into HTML/PDF.
# Work around some documents that have originally included
# the asciidoc markup (use double-hash to avoid conversion).

DOCBUILD_FILTER_GITHUB_LINKS = { \
    $(SED) \
        -e 's%\(link:https*://github.com/networkupstools/[a-zA-Z0-9./-]*/[1-9][0-9]*/*\[[^]]*\)\#\([1-9][0-9]*\)%\1\#\#\2%g' \
        -e 's%\(issue\) *\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/nut/issues/\2[\1 \#\#\2]\3%g' \
        -e 's%\(PR\|pull request\) *\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/nut/pull/\2[\1 \#\#\2]\3%g' \
        -e 's%\([[ ,]\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%\1link:https://github.com/networkupstools/nut/issues/\2[\#\#\2]\3%g' \
        -e 's%\(issue\) networkupstools/\([^ ][^ ]*\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/\2/issues/\3[\1 \2\#\#\3]\4%g' \
        -e 's%\(PR\|pull request\) *networkupstools/\([^ ][^ ]*\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%link:https://github.com/networkupstools/\2/pull/\3[\1 \2\#\#\3]\4%g' \
        -e 's%\([[ ,]\)networkupstools/\([^ ][^ ]*\)\#\([1-9][0-9]*\)\([^0-9]\|$$\)%\1link:https://github.com/networkupstools/\2/issues/\3[\2\#\#\3]\4%g' \
        -e 's%\#\(\#[1-9][0-9]*\)%\1%g' \
; }

# The $< is okay here, it is used in a suffix rule below
DOCBUILD_CONVERT_GITHUB_LINKS = { \
    echo "  DOC-ASCIIDOC-GITHUB-LINKS    Parsing GitHub link patterns $< => $@"; \
    cat "$<" | $(DOCBUILD_FILTER_GITHUB_LINKS) > "$@.tmp" \
    && mv -f "$@.tmp" "$@" \
; }

.adoc.adoc-parsed:
	@$(DOCBUILD_CONVERT_GITHUB_LINKS)

$(top_builddir)/ChangeLog:
	+@echo "  DOC-CHANGELOG-GENERATE	$@" \
	 && cd $(top_builddir) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

# BSD Make dislikes the path resolution here and does not always populate "$<"
# (and claims why: "Using $< in a non-suffix rule context is a GNUmake idiom"),
# but it has a "$?" for "list of dependencies that are newer than the target".
# For more details see https://man.freebsd.org/cgi/man.cgi
if WITH_PDF_NONASCII_TITLES
A2X_ASCII_IDS =
else !WITH_PDF_NONASCII_TITLES
A2X_ASCII_IDS = ":ascii-ids:\n"
endif !WITH_PDF_NONASCII_TITLES

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
	 echo "  DOC-CHANGELOG-ASCIIDOC	$${INPUT} => $@" \
	 && printf "ifdef::txt[]\n== Very detailed Change Log\n"$(A2X_ASCII_IDS)"endif::txt[]\n\n" > "$@.tmp" \
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
	   < "$${INPUT}" >> "$@.tmp" \
	 && mv -f "$@.tmp" "$@"

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

user-manual.html user-manual.chunked user-manual.pdf: $(FULL_USER_MANUAL_DEPS)
developer-guide.html developer-guide.chunked developer-guide.pdf: $(FULL_DEVELOPER_GUIDE_DEPS)
packager-guide.html packager-guide.chunked packager-guide.pdf: packager-guide.txt asciidoc.conf
release-notes.html release-notes.chunked release-notes.pdf: release-notes.txt $(top_builddir)/NEWS.adoc-parsed $(top_builddir)/UPGRADING.adoc-parsed asciidoc.conf
ChangeLog.html ChangeLog.chunked ChangeLog.pdf: ChangeLog.txt $(top_builddir)/ChangeLog.adoc-parsed asciidoc.conf
solaris-usb.html solaris-usb.chunked solaris-usb.pdf: solaris-usb.txt asciidoc.conf

# Note: without the "-v", asciidoc (circa 8.6.2) sometimes hangs when
# generating the chunked HTML. In this case, export the environment
# variable ASCIIDOC_VERBOSE to "-v", ie:
#   $ ASCIIDOC_VERBOSE=-v make
# Note: `(top_)srcdir` and `(top_)builddir` must end with a path
# separator, or be empty -- so in all cases letting the resulting
# string resolve meaningfully in the filesystem during docs build.
A2X_COMMON_OPTS = $(ASCIIDOC_VERBOSE) \
    --attribute=icons \
    --xsltproc-opts="--nonet" \
    --xsltproc-opts="--stringparam nut.localdate \"`TZ=UTC date +%Y-%m-%d`\"" \
    --xsltproc-opts="--stringparam nut.localtime \"`TZ=UTC date +%H:%M:%S`\"" \
    --xsltproc-opts="--stringparam nut.nutversion \"@PACKAGE_VERSION@\"" \
    --attribute=docinfodir="$(builddir)" \
    --attribute=iconsdir="$(srcdir)/images" \
    --attribute=badges \
    --attribute=external_title \
    --attribute=tree_version="@TREE_VERSION@" \
    --attribute=srcdir="$(abs_srcdir)/" \
    --attribute=builddir="$(abs_builddir)/" \
    --attribute=top_srcdir="$(abs_top_srcdir)/" \
    --attribute=top_builddir="$(abs_top_builddir)/" \
    -a toc -a numbered --destination-dir=$${A2X_OUTDIR}
# NOTE: a2x newer than 8.6.8 says "--destination-dir" is only valid for HTML.
# As of version 8.6.9 it lies, and the argument is required for our distcheck
# (and does affect PDF builds, as found during work on collision-avoidance -
# true with at least asciidoc/a2x versions 9.0.0rc2).
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

# Note that empirically it treats "destination-dir" as the source root for
# PDF generation (even though it claims the argument is ignored for non-HTML
# targets) so we have to provide the "images/" in this case. ONLY for PDF!

# Note we only remove the original target (if present), if it is a directory -
# e.g. created by "html-chunked" targets.
DOCBUILD_BEGIN = { \
    if test -n "$${A2X_OUTDIR}" && test "$${A2X_OUTDIR}" != '.' ; then \
        rm -rf "./$${A2X_OUTDIR}" || true ; \
        test -d "$@" && rm -rf "$@" || true ; \
        $(MKDIR_P) "./$${A2X_OUTDIR}" || exit ; \
        case "$${A2X_OUTDIR}" in \
            tmp/pdf.*) ln -s ../../images "./$${A2X_OUTDIR}" ;; \
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
        mv -f "./$${A2X_OUTDIR}/$(@F)" ./ || exit ; \
        mv -f "./$${A2X_OUTDIR}/"*.* ./ 2>/dev/null || true ; \
        rm -rf "./$${A2X_OUTDIR}" ; \
    fi ; \
}

### Call the prep step consistently to create symlinks (out-of-tree)
### or just touch-files for peace of mind (in-tree builds). Then we
### use these path names (truncated "-prepped") now surely located
### in the builddir as the sources for rendered docs.
*.txt-prepped: $(abs_top_builddir)/docs/.prep-src-docs

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

*.html: common.xsl xhtml.xsl
.txt-prepped.html:
	@A2X_OUTDIR="tmp/html-single.$(@F).$$$$" ; \
	 echo "  DOC-HTML Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(A2X) $(A2X_COMMON_OPTS) --attribute=xhtml11_format --format=xhtml --xsl-file=$(srcdir)/xhtml.xsl "$(<D)/`basename "$(<F)" -prepped`" || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

*.chunked: common.xsl chunked.xsl
.txt-prepped.chunked:
	@A2X_OUTDIR="tmp/html-chunked.$(@F).$$$$" ; \
	 echo "  DOC-HTML-CHUNKED Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(A2X) $(A2X_COMMON_OPTS) --attribute=chunked_format --format=chunked --xsl-file=$(srcdir)/chunked.xsl "$(<D)/`basename "$(<F)" -prepped`" || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

# Note: non-HTML a2x modes may ignore the destination directory
*.pdf: docinfo.xml
.txt-prepped.pdf:
	@A2X_OUTDIR="tmp/pdf.$(@F).$$$$" ; \
	 echo "  DOC-PDF  Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(A2X) $(A2X_COMMON_OPTS) --attribute=pdf_format --format=pdf -a docinfo1 "$(<D)/`basename "$(<F)" -prepped`" || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

# Used below for spellcheck and for .prep-src-docs
SPELLCHECK_SRC_DEFAULT = $(ALL_TXT_SRC) \
	asciidoc-vars.conf \
	../README.adoc ../NEWS.adoc \
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

# Directory SPELLCHECK_SRC files are relative to. Overriden by other Makefiles.
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
				REPORT_SRCDIR="`dirname "$(SPELLCHECK_SRC_ONE)"`"; \
			else \
				echo "APPEND: SPELLCHECK_SRCDIR='$(SPELLCHECK_SRCDIR)' SPELLCHECK_SRC_ONE='$(SPELLCHECK_SRC_ONE)' dirname='`dirname "$(SPELLCHECK_SRC_ONE)"`'" >$(SPELLCHECK_RECIPE_DEBUG_STREAM) ; \
				REPORT_SRCDIR="$${REPORT_SRCDIR}/`dirname "$(SPELLCHECK_SRC_ONE)"`"; \
			fi ; \
			REPORT_SRC_ONE="`basename "$(SPELLCHECK_SRC_ONE)"`"; \
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
	 echo "  ASPELL   Spell checking on $${REPORT_PREFIX}$${REPORT_SRCDIR}$${REPORT_SRC_ONE}"; \
	 OUT="`(sed 's,^\(.*\)$$, \1,' | $(ASPELL) -a $(ASPELL_NUT_TEXMODE_ARGS) $(ASPELL_NUT_COMMON_ARGS) 2>&1) < "$(SPELLCHECK_SRCDIR)/$(SPELLCHECK_SRC_ONE)"`" \
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
		(command -v dpkg) && ( dpkg -l | grep -i aspell ) || true ; \
		echo "ASPELL automatic execution line is : ( sed 's,^\(.*\)$$, \1,' < docfile.txt | $(ASPELL) -a $(ASPELL_NUT_TEXMODE_ARGS) $(ASPELL_NUT_COMMON_ARGS) | $(EGREP) -b -v '$(ASPELL_OUT_NOTERRORS)' )" ; \
		echo "ASPELL proceeding to spellchecking job..."; \
	 else true; fi
	+@FAILED="" ; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	 for docsrc in $(SPELLCHECK_SRC); do \
		if test "$(SPELLCHECK_ENV_DEBUG)" != no ; then \
			echo "ASPELL MAKEFILE DEBUG: Will see from `pwd` if '$(SPELLCHECK_SRCDIR)/$${docsrc}-spellchecked' is up to date" >&2; \
		else true ; fi ; \
		$(MAKE) $(AM_MAKEFLAGS) -s -f "$(abs_top_builddir)/docs/Makefile" SPELLCHECK_SRC="" SPELLCHECK_SRC_ONE="$${docsrc}" SPELLCHECK_BUILDDIR="$(SPELLCHECK_BUILDDIR)" SPELLCHECK_SRCDIR="$(SPELLCHECK_SRCDIR)" "$(SPELLCHECK_BUILDDIR)/$${docsrc}-spellchecked" \
		|| FAILED="$$FAILED $(SPELLCHECK_SRCDIR)/$$docsrc"; \
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
	    WORDLIST="`tail -n +2 < "$?" | sort | uniq`"; \
	    WORDCOUNT="`echo "$$WORDLIST" | wc -l`"; \
	    head -1 < "$?" | while read P L C E ; do echo "$$P $$L $$WORDCOUNT $$E"; break; done ; \
	    echo "$$WORDLIST"; \
	 ) > "$@"
	@cp -f "$@" "$(abs_builddir)/$(NUT_SPELL_DICT)"
	@if [ "$(abs_builddir)" != "$(abs_srcdir)" ] ; then \
	    cp -f "$@" "$?" || true ; \
	    cp -f "$(abs_builddir)/$(NUT_SPELL_DICT).bak-pre-sorting" "$(abs_srcdir)/" || true ; \
	 fi

DISTCLEANFILES = $(NUT_SPELL_DICT).bak-pre-sorting .$(NUT_SPELL_DICT).sorted $(NUT_SPELL_DICT).sorted

spellcheck-interactive: 
	@FAILED="" ; for docsrc in $(SPELLCHECK_SRC); do \
		echo "Spell checking on $(SPELLCHECK_SRCDIR)/$$docsrc"; \
		LANG=$(ASPELL_ENV_LANG) LC_ALL=$(ASPELL_ENV_LANG) $(ASPELL) check $(ASPELL_NUT_COMMON_ARGS) $(SPELLCHECK_SRCDIR)/$$docsrc || \
			FAILED="$$FAILED $(SPELLCHECK_SRCDIR)/$$docsrc"; \
	done ; \
	if test -n "$$FAILED" ; then \
		echo "FAILED interactive spellcheck for the following sources (relative to `pwd`): $$FAILED" >&2 ; \
		exit 1; \
	fi ; exit 0
	+$(MAKE) $(AM_MAKEFLAGS) spellcheck-sortdict
	@echo "------------------------------------------------------------------------"; \
	 echo "Custom dictionary file $(NUT_SPELL_DICT) may have been updated now."; \
	 echo "Use 'git add -p docs/$(NUT_SPELL_DICT) && git checkout -- docs/$(NUT_SPELL_DICT) && make spellcheck-sortdict && git add -p docs/$(NUT_SPELL_DICT)'"; \
	 echo "to review changes (please DO NOT REMOVE LINES that aspell chose to drop,"; \
	 echo "because other systems might not know these words in their system dictionaries)"; \
	 echo "------------------------------------------------------------------------"

else !HAVE_ASPELL
# This rule would probably just fail; normally with no ASPELL there are no callers for it
*/*-spellchecked *-spellchecked: Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
	@echo "  SKIP-ASPELL   $@ : Documentation spell check not available since 'aspell' was not found (or missing its English dictionary)." >&2
spellcheck:
	@echo "Documentation spell check not available since 'aspell' was not found (or missing its English dictionary)."
spellcheck-interactive:
	@echo "Documentation spell check not available since 'aspell' was not found (or missing its English dictionary)."
endif !HAVE_ASPELL


# When building out-of-tree, be sure to have all asciidoc resources
# under the same dir structure (tool limitation)
PREP_SRC = $(EXTRA_DIST) $(SPELLCHECK_SRC_DEFAULT)

# NOTE: Some "make" implementations prefix a relative or absent path to
# the filenames in PREP_SRC, others (e.g. Sun make) prepend the absolute
# path to locate the sources, so we end up with bogus trees under docs/.
# Code below tries to detect and truncate this mess, including possible
# source texts located in/under parent dirs.
$(abs_top_builddir)/docs/.prep-src-docs: $(PREP_SRC)
	@cd "$(@D)" || exit ; \
	 linkroot="$(abs_builddir)" ; \
	 if test x"$(abs_srcdir)" = x"$(abs_builddir)" ; then \
	    COUNT=0; \
	    for F in $(PREP_SRC) ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed "s#^$(abs_top_srcdir)/*#./#"`"; \
	                if test x"$${linkroot}" = x"$(abs_builddir)" ; then \
	                    linkroot="$(abs_top_builddir)" ; \
	                    cd "$(abs_top_builddir)" ; \
	                fi ;; \
	        esac ; \
	        if ! test -e "$${F}-prepped" ; then \
	            touch "$${F}-prepped" || exit ; \
	            COUNT="`expr $$COUNT + 1`" ; \
	        fi ; \
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
	            /*) F="`echo "$$F" | sed "s#^$(abs_top_srcdir)/*#./#"`"; \
	                if test x"$${linkroot}" = x"$(abs_builddir)" ; then \
	                    linkroot="$(abs_top_builddir)" ; \
	                    linksrcroot="$(abs_top_srcdir)" ; \
	                    cd "$(abs_top_builddir)" ; \
	                fi ;; \
	            "$(srcdir)"/*) F="`echo "$$F" | sed 's#^$(srcdir)/*#./#'`" ;; \
	        esac ; \
	        D="`dirname "$$F"`" ; \
	        $(MKDIR_P) "$${linkroot}/$$D" || { rm -f "$@.working" ; exit 1 ; } ; \
	        if ! test -s "$${linkroot}/$$F" && test -s "$${linksrcroot}/$$F" ; then \
	            echo "  LN     '$${linksrcroot}/$$F' => '$${linkroot}/$$F' (PWD = '`pwd`')" >&2 ; \
	            ln -fs "$${linksrcroot}/$$F" "$${linkroot}/$$F" || { rm -f "$@.working" ; exit 1 ; } ; \
	            COUNT="`expr $$COUNT + 1`" ; \
	        fi ; \
	        if ! test -e "$${linkroot}/$${F}-prepped" ; then \
	            touch "$${linkroot}/$${F}-prepped" || { rm -f "$@.working" ; exit 1 ; } ; \
	            COUNT="`expr $$COUNT + 1`" ; \
	        fi ; \
	    done ; \
	 fi ; \
	 if test "$$COUNT" -gt 0 -o ! -e "$@" ; then touch "$@" ; fi
	@rm -f "$@.working"

# Dirs to clean, etc.
clean-local:
	$(AM_V_at)rm -rf *.chunked *.bak tmp
	$(AM_V_at)for F in $(PREP_SRC) ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed "s#^$(abs_top_srcdir)/*#./#"`"; cd "$(abs_top_builddir)" ;; \
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
