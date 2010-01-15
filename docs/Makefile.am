EXTRA_DIST = acpi.txt acknowledgements.txt chroot.txt		\
 commands.txt configure.txt config-notes.txt contact-closure.txt	\
 design.txt developers.txt FAQ.txt hid-subdrivers.txt		\
 ideas.txt macros.txt megatec.txt new-drivers.txt new-names.txt \
 nut-hal.txt osd-notify.txt	\
 pager.txt powersaving.txt protocol.txt snmp.txt	\
 sock-protocol.txt ssl.txt upssched.txt cables/apc-rs500-serial.txt	\
 cables/apc.txt cables/ge-imv-victron.txt cables/imv.txt		\
 cables/mgeups.txt cables/powerware.txt cables/repotec.txt		\
 cables/sms.txt

if MANUALUPDATE

CONVERTED_TO_ASCIIDOC = asciidoc.html \
	user-manual.html \
	packager-guide.html \
	developer-guide.html \
	install.html \
	FAQ.html

html:	$(CONVERTED_TO_ASCIIDOC) html-recursive

clean-local:
	rm -f $(CONVERTED_TO_ASCIIDOC)

SUBDIRS = website man

SUFFIXES = .txt .html

install.html: ../INSTALL
	$(ASCIIDOC) --unsafe --backend=xhtml11 \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		-a toc -a numbered -o $@ $^

.txt.html:
	$(ASCIIDOC) --unsafe --backend=xhtml11 \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		-a toc -a numbered -o $@ $^
	
else !MANUALUPDATE
html:	html-recursive
	@echo "Not building HTML documentation since 'asciidoc' was not found."
endif !MANUALUPDATE
