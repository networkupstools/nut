EXTRA_DIST = acknowledgements.txt		\
 commands.txt configure.txt config-notes.txt contact-closure.txt	\
 design.txt developers.txt download.txt FAQ.txt hid-subdrivers.txt		\
 history.txt ideas.txt macros.txt megatec.txt new-drivers.txt new-names.txt \
 nut-hal.txt support.txt	\
 outlets.txt protocol.txt security.txt snmp.txt 	\
 sock-protocol.txt scheduling.txt cables/apc-rs500-serial.txt	\
 cables/apc.txt cables/ge-imv-victron.txt cables/imv.txt		\
 cables/mgeups.txt cables/powerware.txt cables/repotec.txt		\
 cables/sms.txt

if MANUALUPDATE

CONVERTED_TO_ASCIIDOC = asciidoc.html \
	user-manual.html \
	packager-guide.html \
	developer-guide.html \
	download.html \
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
		-a toc -a numbered -o $@ $<

### TODO: automatic dependency generation
user-manual.html: ../README features.txt download.txt ../INSTALL config-notes.txt scheduling.txt outlets.txt security.txt acknowledgements.txt cables.txt configure.txt history.txt ../UPGRADING

else !MANUALUPDATE
html:	html-recursive
	@echo "Not building HTML documentation since 'asciidoc' was not found."
endif !MANUALUPDATE
