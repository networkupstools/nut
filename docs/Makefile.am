SHARED_DEPS = nut-names.txt

IMAGE_FILES = images/eaton-logo.png \
	icons/note.png \
	icons/warning.png

USER_MANUAL_DEPS = acknowledgements.txt cables.txt config-notes.txt	\
 configure.txt download.txt documentation.txt features.txt history.txt	\
 outlets.txt scheduling.txt security.txt support.txt user-manual.txt

DEVELOPER_GUIDE_DEPS = design.txt developers.txt	\
 developer-guide.txt hid-subdrivers.txt macros.txt \
 new-clients.txt new-drivers.txt net-protocol.txt sock-protocol.txt

CABLES_DEPS = cables/apc-rs500-serial.txt	\
 cables/apc.txt cables/ge-imv-victron.txt cables/imv.txt		\
 cables/mgeups.txt cables/powerware.txt cables/repotec.txt		\
 cables/sms.txt

EXTRA_DIST = $(SHARED_DEPS) $(USER_MANUAL_DEPS) $(DEVELOPER_GUIDE_DEPS) \
 $(CABLES_DEPS) contact-closure.txt FAQ.txt \
 nut-hal.txt nut-qa.txt packager-guide.txt snmp.txt \
 $(IMAGE_FILES)

ASCIIDOC_HTML_SINGLE = user-manual.html \
	developer-guide.html \
	packager-guide.html \
	FAQ.html

ASCIIDOC_HTML_CHUNKED = user-manual.chunked \
	developer-guide.chunked \
	packager-guide.chunked \
	FAQ.html

ASCIIDOC_PDF = user-manual.pdf \
	developer-guide.pdf \
	packager-guide.pdf \
	FAQ.pdf

# Force build in ./ and man/ before website 
SUBDIRS = . man website
SUFFIXES = .txt .html .pdf

all: doc

doc: @DOC_BUILD_LIST@

pdf: $(ASCIIDOC_PDF)
# also build the HTML manpages with these targets
html-single: $(ASCIIDOC_HTML_SINGLE)
html-chunked: $(ASCIIDOC_HTML_CHUNKED)

if HAVE_ASCIIDOC
website: html-chunked pdf
else !HAVE_ASCIIDOC
website:
	@echo "Not building website documentation since 'asciidoc' was not found."
endif !HAVE_ASCIIDOC

# FIXME: docbook-xsl.css presence is probably a bug either due to the
# copy dance or to a2x itself
clean-local:
	rm -rf *.pdf *.html *.chunked docbook-xsl.css

A2X_COMMON_OPTS = --attribute icons \
    --attribute localdate=`TZ=UTC date +%Y-%m-%d` \
    --attribute localtime=`TZ=UTC date +%H:%M:%S` \
    --attribute iconsdir=$(srcdir)/icons \
    --attribute=badges \
    --attribute=external_title \
    -a toc -a numbered --destination-dir=.

.txt.html:
	$(A2X) $(A2X_COMMON_OPTS) --format=xhtml $<

user-manual.chunked: user-manual.txt
	$(A2X) $(A2X_COMMON_OPTS) --format=chunked $<

developer-guide.chunked: developer-guide.txt
	$(A2X) $(A2X_COMMON_OPTS) --format=chunked $<

.txt.chunked:
	$(A2X) $(A2X_COMMON_OPTS) --format=chunked $<

#html-chunked-build: user-manual.txt developer-guide.txt
#	$(A2X) $(A2X_COMMON_OPTS) --format=chunked $<

.txt.pdf:
	$(A2X) $(A2X_COMMON_OPTS) --format=pdf $<

.PHONY: html html-single html-chunked-build pdf website

### TODO: automatic dependency generation
FULL_USER_MANUAL_DEPS = $(USER_MANUAL_DEPS)  $(SHARED_DEPS) ../README \
	../INSTALL cables.txt ../UPGRADING ../TODO
FULL_DEVELOPER_GUIDE_DEPS = $(DEVELOPER_GUIDE_DEPS)  $(SHARED_DEPS)

user-manual.pdf: $(FULL_USER_MANUAL_DEPS)
user-manual.html: $(FULL_USER_MANUAL_DEPS)
developer-guide.html: $(FULL_DEVELOPER_GUIDE_DEPS)
developer-guide.pdf: $(FULL_DEVELOPER_GUIDE_DEPS)
