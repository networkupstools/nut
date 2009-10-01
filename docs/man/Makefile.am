if MANUALUPDATE

CONVERTED_TO_ASCIIDOC = \
	hosts.conf.html \
	index.html \
	nut.conf.html \
	nutupsdrv.html \
	tripplite_usb.html \
	ups.conf.html \
	upsc.html \
	upscmd.html \
	upsd.html \
	upsd.conf.html \
	upsd.users.html \
	upslog.html \
	upsmon.html \
	upsmon.conf.html \
	upsrw.html \
	upssched.html \
	upssched.conf.html \
	upsset.conf.html \
	upsstats.html.html \
	usbhid-ups.html

EXTRA_DIST = \
	hosts.conf.txt \
	index.txt \
	nut.conf.txt \
	nutupsdrv.txt \
	tripplite_usb.txt \
	ups.conf.txt \
	upsc.txt \
	upscmd.txt \
	upsd.conf.txt \
	upsd.users.txt \
	upsd.txt \
	upslog.txt \
	upsmon.txt \
	upsmon.conf.txt \
	upsrw.txt \
	upssched.txt \
	upssched.conf.txt \
	upsset.conf.txt \
	upsstats.html.txt \
	usbhid-ups.txt \
	$(CONVERTED_TO_ASCIIDOC) \
	$(MANS)

# http://www.gnu.org/software/hello/manual/automake/Man-pages.html
man5_MANS = \
	hosts.conf.5 \
	nut.conf.5 \
	ups.conf.5 \
	upsd.conf.5 \
	upsd.users.5 \
	upsmon.conf.5 \
	upssched.conf.5 \
	upsset.conf.5 \
	upsstats.html.5

man8_MANS = nutupsdrv.8 \
	tripplite_usb.8 \
	upsc.8 \
	upsd.8 \
	upslog.8 \
	upscmd.8 \
	upsmon.8 \
	upsrw.8 \
	upssched.8 \
	usbhid-ups.8

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
A2X_MANPAGE_OPTS = -f manpage --attribute nutversion="@PACKAGE_VERSION@"

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
