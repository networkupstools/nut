if MANUALUPDATE

WEBSITE_FILES = index.html download.html projects.html stable-hcl.html \
	documentation.html support.html acknowledgements.html features.html FAQ.html \
	user-manual.html packager-guide.html developer-guide.html ups-protocols.html \
	cables.html man-index.html nut-qa.html

ASCIIDOC_PARAMS = --unsafe --backend=xhtml11 \
    --conf-file=$(LAYOUT).conf \
    --attribute icons \
    --attribute iconsdir=$(srcdir)/images \
    --attribute scriptsdir=$(srcdir)/scripts \
    --attribute=badges \
    --attribute=revision=$(PACKAGE_VERSION) \
    --attribute localdate=`TZ=UTC date +%Y-%m-%d` \
    --attribute localtime=`TZ=UTC date +%H:%M:%S` \
    --attribute=date="`TZ=UTC date`"

# CSS based simulated frames layout.
LAYOUT=web-layout

html:	$(WEBSITE_FILES)

$(WEBSITE_FILES): $(LAYOUT).conf

clean-local:
	rm -f $(WEBSITE_FILES)

SUFFIXES = .txt .html

index.html: website.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a index-only $<

acknowledgements.html: ../acknowledgements.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a website $<

features.html: ../features.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

cables.html: ../cables.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a website $<

FAQ.html: ../FAQ.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

user-manual.html: ../user-manual.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

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
	
else !MANUALUPDATE
html: html-recursive
	@echo "Not building website since 'asciidoc' was not found."
endif !MANUALUPDATE
