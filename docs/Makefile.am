SHARED_DEPS = nut-names.txt

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
 nut-hal.txt snmp.txt packager-guide.txt

CONVERTED_TO_ASCIIDOC_HTML = user-manual.html \
	developer-guide.html \
	packager-guide.html \
	FAQ.html

CONVERTED_TO_ASCIIDOC_PDF = user-manual.pdf developer-guide.pdf

all: doc

doc: @DOC_BUILD_LIST@

# this target is mostly for testing and QA purpose
all-doc: html-single html-chunked pdf

# FIXME: Transitional compatibility target
html: html-single html-recursive

pdf: $(CONVERTED_TO_ASCIIDOC_PDF)
# also build the HTML manpages with these targets
html-single: $(CONVERTED_TO_ASCIIDOC_HTML) html-recursive
html-chunked: html-chunked-build html-recursive

clean-local:
	rm -rf *.pdf *.html *.chunked

SUBDIRS = website man

SUFFIXES = .txt .html .pdf

.txt.html:
	$(ASCIIDOC) --unsafe --backend=xhtml11 \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		--attribute=external_title \
		-a toc -a numbered -o $@ $<

html-chunked-build: user-manual.txt developer-guide.txt
	$(A2X) --format=chunked \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		--attribute=external_title \
		-a toc -a numbered $<

.txt.pdf:
	$(A2X) --format=pdf \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		--attribute=external_title \
		-a toc -a numbered $<

.PHONY: html html-single html-chuncked pdf

### TODO: automatic dependency generation
FULL_USER_MANUAL_DEPS = $(USER_MANUAL_DEPS)  $(SHARED_DEPS) ../README \
	../INSTALL cables.txt ../UPGRADING ../TODO
FULL_DEVELOPER_GUIDE_DEPS = $(DEVELOPER_GUIDE_DEPS)  $(SHARED_DEPS)

user-manual.pdf: $(FULL_USER_MANUAL_DEPS)
user-manual.html: $(FULL_USER_MANUAL_DEPS)
developer-guide.html: $(FULL_DEVELOPER_GUIDE_DEPS)
developer-guide.pdf: $(FULL_DEVELOPER_GUIDE_DEPS)
