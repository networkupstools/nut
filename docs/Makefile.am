SHARED_DEPS =  nut-names.txt

USER_MANUAL_DEPS =  acknowledgements.txt cables.txt config-notes.txt	\
 configure.txt download.txt features.txt history.txt outlets.txt	\
 scheduling.txt security.txt support.txt user-manual.txt

DEVELOPER_GUIDE_DEPS = design.txt developers.txt	\
 developer-guide.txt hid-subdrivers.txt macros.txt \
 new-drivers.txt net-protocol.txt sock-protocol.txt

CABLES_DEPS = cables/apc-rs500-serial.txt	\
 cables/apc.txt cables/ge-imv-victron.txt cables/imv.txt		\
 cables/mgeups.txt cables/powerware.txt cables/repotec.txt		\
 cables/sms.txt

EXTRA_DIST = $(SHARED_DEPS) $(USER_MANUAL_DEPS) $(DEVELOPER_GUIDE_DEPS) \
 $(CABLES_DEPS) contact-closure.txt FAQ.txt \
 megatec.txt nut-hal.txt snmp.txt

if MANUALUPDATE

CONVERTED_TO_ASCIIDOC = asciidoc.html \
	user-manual.html \
	developer-guide.html \
	packager-guide.html \
	FAQ.html

html:	$(CONVERTED_TO_ASCIIDOC) html-recursive

clean-local:
	rm -f $(CONVERTED_TO_ASCIIDOC)

SUBDIRS = website man

SUFFIXES = .txt .html

.txt.html:
	$(ASCIIDOC) --unsafe --backend=xhtml11 \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		--attribute=external_title \
		-a toc -a numbered -o $@ $<

### TODO: automatic dependency generation
user-manual.html: $(USER_MANUAL_DEPS)  $(SHARED_DEPS) ../README \
	../INSTALL cables.txt ../UPGRADING ../TODO

developer-guide.html: $(DEVELOPER_GUIDE_DEPS)  $(SHARED_DEPS)

else !MANUALUPDATE
html:	html-recursive
	@echo "Not building HTML documentation since 'asciidoc' was not found."
endif !MANUALUPDATE
