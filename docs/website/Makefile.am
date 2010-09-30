EXTRA_DIST = news.txt projects.txt stable-hcl.txt website.txt \
	 ups-html.txt ups-protocols.txt $(LAYOUT).conf $(SCRIPT_FILES) $(IMAGE_FILES)

IMAGE_FILES = images/advanced.png \
	images/asciidoc.png \
	images/bigbox.png \
	images/bizarre.png \
	images/blue-arrow.png \
	images/eaton-logo.png \
	images/note.png \
	images/simple.png \
	images/warning.png \
	css/ie-overrides.css \
	css/web-layout.css \
	css/xhtml11-quirks.css \
	css/xhtml11.css \
	faviconut.ico \
	faviconut.png

SCRIPT_FILES = scripts/filter_png.js \
	scripts/jquery.js \
	scripts/nut_jquery.js \
	scripts/toc.js \
	scripts/ups_data.js

WEBSITE_FILES = index.html projects.html stable-hcl.html \
	documentation.html acknowledgements.html features.html FAQ.html \
	user-manual.html packager-guide.html developer-guide.html ups-protocols.html \
	cables.html man-index.html nut-qa.html download.html support.html

ASCIIDOC_PARAMS = --backend=xhtml11 \
    --conf-file=$(srcdir)/$(LAYOUT).conf \
    --attribute icons \
    --attribute iconsdir=$(srcdir)/images \
    --attribute scriptsdir=$(srcdir)/scripts \
    --attribute=badges \
    --attribute=website \
    --attribute=revision=$(PACKAGE_VERSION) \
    --attribute localdate=`TZ=UTC date +%Y-%m-%d` \
    --attribute localtime=`TZ=UTC date +%H:%M:%S` \
    --attribute=date="`TZ=UTC date`"

# CSS based simulated frames layout.
LAYOUT=web-layout

all:

# transitional target!
html: website

website:	$(WEBSITE_FILES)

$(WEBSITE_FILES): $(LAYOUT).conf

clean-local:
	rm -f $(WEBSITE_FILES)

SUFFIXES = .txt .html

index.html: website.txt news.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a index-only $<

acknowledgements.html: ../acknowledgements.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

features.html: ../features.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

cables.html: ../cables.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

download.html: ../download.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

documentation.html: ../documentation.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

FAQ.html: ../FAQ.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

projects.html: projects.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

support.html: ../support.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

user-manual.html: ../user-manual.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered -a website! $<

packager-guide.html: ../packager-guide.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

developer-guide.html: ../developer-guide.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

man-index.html: ../man/index.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

nut-qa.html: ../nut-qa.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

stable-hcl.html: stable-hcl.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a hcl $<

.txt.html:
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<
