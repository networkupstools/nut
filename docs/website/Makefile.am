if MANUALUPDATE

WEBSITE_FILES = index.html download.html projects.html stable-hcl.html \
	documentation.html support.html acknowledgements.html features.html FAQ.html 

ASCIIDOC_PARAMS = --unsafe --backend=xhtml11 \
    --conf-file=$(LAYOUT).conf \
    --attribute icons \
    --attribute iconsdir=$(srcdir)/icons \
    --attribute=badges \
    --attribute=revision=$(PACKAGE_VERSION) \
    --attribute=date="`TZ=UTC date`"

# CSS based simulated frames layout.
LAYOUT=web-layout

html:	$(WEBSITE_FILES)

SUFFIXES = .txt .html

index.html: website.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a index-only $^

features.html: ../features.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $^

FAQ.html: ../FAQ
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $^

.txt.html:
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $^
	
endif
