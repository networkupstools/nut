if MANUALUPDATE

CONVERTED_TO_ASCIIDOC = \
	apcsmart.html \
	bcmxcp.html \
	bcmxcp_usb.html \
	belkin.html \
	belkinunv.html \
	bestfcom.html \
	bestuferrups.html \
	bestups.html \
	blazer.html \
	clone.html \
	dummy-ups.html \
	etapro.html \
	everups.html \
	gamatronic.html \
	genericups.html \
	hosts.conf.html \
	index.html \
	isbmex.html \
	liebert.html \
	masterguard.html \
	megatec.html \
	megatec_usb.html \
	metasys.html \
	microdowell.html \
	nut.conf.html \
	nutupsdrv.html \
	oneac.html \
	optiups.html \
	powercom.html \
	powerman-pdu.html \
	tripplite_usb.html \
	ups.conf.html \
	upsc.html \
	upscmd.html \
	upsd.html \
	upsd.conf.html \
	upsd.users.html \
	upsdrvctl.html \
	upsimage.cgi.html \
	upslog.html \
	upsmon.html \
	upsmon.conf.html \
	upsrw.html \
	upssched.html \
	upssched.conf.html \
	upsset.cgi.html \
	upsset.conf.html \
	upsstats.cgi.html \
	upsstats.html.html \
	usbhid-ups.html

EXTRA_DIST = \
	apcsmart.txt \
	bcmxcp.txt \
	bcmxcp_usb.txt \
	belkin.txt \
	belkinunv.txt \
	bestfcom.txt \
	bestuferrups.txt \
	bestups.txt \
	blazer.txt \
	clone.txt \
	dummy-ups.txt \
	etapro.txt \
	everups.txt \
	gamatronic.txt \
	genericups.txt \
	hosts.conf.txt \
	index.txt \
	isbmex.txt \
	liebert.txt \
	masterguard.txt \
	megatec.txt \
	megatec_usb.txt \
	metasys.txt \
	microdowell.txt \
	nut.conf.txt \
	nutupsdrv.txt \
	oneac.txt \
	optiups.txt \
	powercom.txt \
	powerman-pdu.txt \
	tripplite_usb.txt \
	ups.conf.txt \
	upsc.txt \
	upscmd.txt \
	upsd.conf.txt \
	upsd.users.txt \
	upsd.txt \
	upsdrvctl.txt \
	upsimage.cgi.txt \
	upslog.txt \
	upsmon.txt \
	upsmon.conf.txt \
	upsrw.txt \
	upssched.txt \
	upssched.conf.txt \
	upsset.cgi.txt \
	upsset.conf.txt \
	upsstats.cgi.txt \
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

# TODO: pull in bestfortress from trunk
man8_MANS = \
	apcsmart.8 \
	bcmxcp.8 \
	bcmxcp_usb.8 \
	belkin.8 \
	belkinunv.8 \
	bestfcom.8 \
	bestuferrups.8 \
	bestups.8 \
	blazer.8 \
	clone.8 \
	dummy-ups.8 \
	etapro.8 \
	everups.8 \
	gamatronic.8 \
	genericups.8 \
	isbmex.8 \
	liebert.8 \
	masterguard.8 \
	megatec.8 \
	megatec_usb.8 \
	metasys.8 \
	microdowell.8 \
	nutupsdrv.8 \
	oneac.8 \
	optiups.8 \
	powercom.8 \
	powerman-pdu.8 \
	tripplite_usb.8 \
	upsc.8 \
	upsd.8 \
	upslog.8 \
	upscmd.8 \
	upsdrvctl.8 \
	upsimage.cgi.8 \
	upsmon.8 \
	upsrw.8 \
	upssched.8 \
	upsset.cgi.8 \
	upsstats.cgi.8 \
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
