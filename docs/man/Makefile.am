if MANUALUPDATE

CONVERTED_TO_ASCIIDOC = nut.conf.html \
	tripplite_usb.html \
	ups.conf.html \
	upsc.html \
	upscmd.html

EXTRA_DIST = nut.conf.txt \
	tripplite_usb.txt \
	ups.conf.txt \
	upsc.txt \
	upscmd.txt \
	$(CONVERTED_TO_ASCIIDOC)

# http://www.gnu.org/software/hello/manual/automake/Man-pages.html
man5_MANS = nut.conf.5 ups.conf.5
man8_MANS = tripplite_usb.8 upsc.8 upscmd.8

html:	$(CONVERTED_TO_ASCIIDOC)

clean-local:
	rm -f $(CONVERTED_TO_ASCIIDOC) $(MANS) *.xml

SUFFIXES = .txt .html .1 .3 .5 .8

.txt.html:
	$(ASCIIDOC) --backend=xhtml11 \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		-o $@ $<

### The --destination-dir flag doesn't seem to affect the intermediate .xml file.
### Hence, the copying dance below.
A2X_MANPAGE_OPTS = -f manpage

# .txt.1:
# 	$(A2X) $(A2X_MANPAGE_OPTS) $<
# 	
# .txt.3:
# 	$(A2X) $(A2X_MANPAGE_OPTS) $<
	
.txt.5:
	test -f `basename $<` || cp -p $< .
	$(A2X) $(A2X_MANPAGE_OPTS) `basename $<`
	
.txt.8:
	test -f `basename $<` || cp -p $< .
	$(A2X) $(A2X_MANPAGE_OPTS) `basename $<`
	
else !MANUALUPDATE
html:	html-recursive
	@echo "Not building HTML documentation since 'asciidoc' was not found."
endif !MANUALUPDATE
