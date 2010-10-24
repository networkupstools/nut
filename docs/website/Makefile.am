# FIXME: do we really have to distribute these many Kb too?
# RCS should be enough, and website generation should use
# source from subversion tags!
EXTRA_DIST = news.txt projects.txt stable-hcl.txt website.txt \
	ups-protocols.txt $(LAYOUT).conf $(SCRIPT_FILES) $(IMAGE_FILES)

IMAGE_FILES = images/asciidoc.png \
	images/eaton-logo.png \
	images/blue-arrow.png \
	css/ie-overrides.css \
	css/web-layout.css \
	css/xhtml11-quirks.css \
	css/xhtml11.css \
	faviconut.ico \
	faviconut.png \
	images/simple.png \
	images/advanced.png \
	images/bigbox.png \
	images/bizarre.png \
	images/note.png \
	images/warning.png
# FIXME: the latter images should shared and be copied from ../images

SCRIPT_FILES = scripts/filter_png.js \
	scripts/jquery.js \
	scripts/nut_jquery.js \
	scripts/toc.js

WEBSITE_FILES = index.html projects.html stable-hcl.html \
	documentation.html acknowledgements.html features.html \
	ups-protocols.html \
	cables.html nut-qa.html download.html support.html

WEBSITE_DEPS = ../user-manual.chunked ../packager-guide.chunked \
	../FAQ.html ../developer-guide.chunked \
	../user-manual.pdf \
	../developer-guide.pdf \
	../packager-guide.pdf \
	../FAQ.pdf ../man/man-index.html

# CSS based simulated frames layout.
LAYOUT = web-layout

# Add --unsafe to allow includes on older versions of asciidoc ( < 8.5.3 ):
ASCIIDOC_PARAMS = --unsafe --backend=xhtml11 \
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

all:

$(WEBSITE_FILES): $(LAYOUT).conf
OUTDIR = output

if HAVE_ASCIIDOC
website: $(WEBSITE_FILES) $(WEBSITE_DEPS) $(OUTDIR) $(SCRIPT_FILES) \
	$(IMAGE_FILES) scripts/ups_data.js ups-html.txt

else !HAVE_ASCIIDOC
website:
	@echo "Not building website since 'asciidoc' was not found."
endif !HAVE_ASCIIDOC

.PHONY: website $(OUTDIR)

$(OUTDIR):
	$(RM) -r $(OUTDIR)
	$(MKDIR_P) $(OUTDIR)/docs/man
	cp -fR $(WEBSITE_FILES) css images scripts $(OUTDIR)
	cp -fR $(WEBSITE_DEPS) $(OUTDIR)/docs/
	cp -f ../man/man-index.html $(OUTDIR)/docs/man/
	cp -f ../man/*.html $(OUTDIR)/docs/man/

clean-local:
	rm -rf $(WEBSITE_FILES) $(OUTDIR)

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

projects.html: projects.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a toc $<

support.html: ../support.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

nut-qa.html: ../nut-qa.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

stable-hcl.html: stable-hcl.txt
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ -a hcl $<

.txt.html:
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<
