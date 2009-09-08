if MANUALUPDATE

EXTRA_DIST = tripplite_usb.8.txt \
	ups.conf.5.txt \
	upsc.8.txt

CONVERTED_TO_ASCIIDOC = tripplite_usb.8.html \
	ups.conf.5.html \
	upsc.8.html

html:	$(CONVERTED_TO_ASCIIDOC)

clean-local:
	rm -f $(CONVERTED_TO_ASCIIDOC)

SUFFIXES = .txt .html

.txt.html:
	$(ASCIIDOC) --backend=xhtml11 \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		-o $@ $^
	
else !MANUALUPDATE
html:	html-recursive
	@echo "Not building HTML documentation since 'asciidoc' was not found."
endif !MANUALUPDATE
