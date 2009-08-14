EXTRA_DIST = acpi.txt big-servers.txt chroot.txt		\
 commands.txt config-files.txt configure.txt contact-closure.txt	\
 data-room.txt design.txt developers.txt FAQ hid-subdrivers.txt		\
 ideas.txt macros.txt megatec.txt new-drivers.txt new-names.txt \
 nut-hal.txt osd-notify.txt	\
 pager.txt powersaving.txt protocol.txt shutdown.txt snmp.txt	\
 sock-protocol.txt ssl.txt upssched.txt cables/apc-rs500-serial.txt	\
 cables/apc.txt cables/ge-imv-victron.txt cables/imv.txt		\
 cables/mgeups.txt cables/powerware.txt cables/repotec.txt		\
 cables/sms.txt

if MANUALUPDATE

CONVERTED_TO_ASCIIDOC = asciidoc.html \
	user-manual.html \
	packager-guide.html \
	developer-guide.html

html:	$(CONVERTED_TO_ASCIIDOC) html-recursive

clean-local:
	rm -f $(CONVERTED_TO_ASCIIDOC)

SUBDIRS = website

SUFFIXES = .txt .html

.txt.html:
	$(ASCIIDOC) --unsafe --backend=xhtml11 \
		--attribute icons \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute iconsdir=$(srcdir)/icons \
		--attribute=badges \
		-a toc -a numbered -o $@ $^
	
endif
