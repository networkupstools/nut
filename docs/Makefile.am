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

# Add --unsafe to allow includes on older versions of asciidoc ( < 8.5.3 ):
.txt.html:
	$(ASCIIDOC) --unsafe --backend=xhtml11 \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		--attribute=external_title \
		-a toc -a numbered -o $@ $<

### The --destination-dir flag doesn't seem to affect the intermediate .xml file.
### Hence, the copying dance below.
html-chunked-build: user-manual.txt developer-guide.txt
	test -f `basename $<` || cp -p $< .
	$(A2X) --format=chunked \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		--attribute=external_title \
		-a toc -a numbered --destination-dir=. `basename $<`

.txt.pdf:
	test -f `basename $<` || cp -p $< .
	$(A2X) --format=pdf \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		--attribute=external_title \
		-a toc -a numbered --destination-dir=. `basename $<`

.PHONY: html html-single html-chunked-build pdf

### TODO: automatic dependency generation
FULL_USER_MANUAL_DEPS = $(USER_MANUAL_DEPS)  $(SHARED_DEPS) ../README \
	../INSTALL cables.txt ../UPGRADING ../TODO
FULL_DEVELOPER_GUIDE_DEPS = $(DEVELOPER_GUIDE_DEPS)  $(SHARED_DEPS)

user-manual.pdf: $(FULL_USER_MANUAL_DEPS)
user-manual.html: $(FULL_USER_MANUAL_DEPS)
developer-guide.html: $(FULL_DEVELOPER_GUIDE_DEPS)
developer-guide.pdf: $(FULL_DEVELOPER_GUIDE_DEPS)
