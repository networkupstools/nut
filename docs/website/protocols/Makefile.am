EXTRA_DIST =	\
	apcsmart.txt	\
	belkin.txt	\
	belkin-protocol.txt	\
	belkin-universal.txt	\
	fortress.txt	\
	megatec.txt	\
	minicol.txt	\
	oneac.txt	\
	optiups.txt	\
	sec.txt	\
	sec-protocol.txt	\
	sec-standard.txt	\
	sola.txt	\
	us9003.txt	\
	voltronic.txt

all:

# Keep using 'HAVE_ASCIIDOC', instead of 'WITH_ASCIIDOC' since we have
# an explicit target
if HAVE_ASCIIDOC
website: $(PROTOCOLS_FILES)

else !HAVE_ASCIIDOC
website:
	@echo "Not building website>protocols since 'asciidoc' was not found."
endif !HAVE_ASCIIDOC

PROTOCOLS_FILES =	\
	apcsmart.html	\
	belkin.html	\
	belkin-protocol.html	\
	belkin-universal.html	\
	fortress.html	\
	megatec.html	\
	minicol.html	\
	oneac.html	\
	optiups.html	\
	sec.html	\
	sec-protocol.html	\
	sec-standard.html	\
	sola.html	\
	us9003.html	\
	voltronic.html

# CSS based simulated frames layout.
LAYOUT = web-layout

# 'root' attribute declares the relative path of the root directory of the website
ASCIIDOC_PARAMS =	\
	--conf-file=../$(LAYOUT).conf \
	--backend=xhtml11	\
	--attribute icons	\
	--attribute iconsdir=../images	\
	--attribute root=../	\
	--attribute linkcss	\
	--attribute scriptsdir=../scripts	\
	--attribute stylesdir=../css	\
	--attribute=badges	\
	--attribute toc

clean-local:
	rm -rf $(PROTOCOLS_FILES)

SUFFIXES = .txt .html

.txt.html:
	$(ASCIIDOC) $(ASCIIDOC_PARAMS) -o $@ $<

.PHONY: website

