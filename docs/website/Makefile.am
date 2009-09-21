if MANUALUPDATE

WEBSITE_FILES = index.html download.html projects.html stable-hcl.html \
	documentation.html support.html acknowledgements.html features.html FAQ.html \
	user-manual.html packager-guide.html developer-guide.html ups-protocols.html \
	cables.html man-index.html install.html

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

features.html: ../features.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

FAQ.html: ../FAQ
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

user-manual.html: ../user-manual.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

packager-guide.html: ../packager-guide.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

developer-guide.html: ../developer-guide.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

cables.html: ../cables.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

man-index.html: ../man/index.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

install.html: ../../INSTALL
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc -a numbered $<

stable-hcl.html: stable-hcl.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a hcl $<

.txt.html:
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<
	
else !MANUALUPDATE
html: html-recursive
	@echo "Not building website since 'asciidoc' was not found."
endif !MANUALUPDATE
