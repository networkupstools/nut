# Network UPS Tools: man
#

# Notes:
# - sources (.txt) and groff formats are both distributed,
# - only sources are versioned ; groff files are generated at worst
#   during 'make dist' (while preparing a release tarball)
# - HTML files are built upon request, if AsciiDoc is available,
# - groff update will only happen if AsciiDoc is available too,
# - all this can probably (and hopefully) be improved, but I've not
#   found a way to do pattern replacement on the fly for target deps!
#   FIXME: investigate an autogen.sh hook
# - Ref: http://www.gnu.org/software/hello/manual/automake/Man-pages.html
# - WITH_MANS can be enabled if either we are building man-pages, or if
#   the --with-doc=man=auto detected an inability to build the man-pages
#   but enabled the DOC_INSTALL_DISTED_MANS toggle so we deliver disted
#   files from source tree

# Base configuration and client manpages, always installed
SRC_CONF_PAGES = \
	nut.conf.txt \
	ups.conf.txt \
	upsd.conf.txt \
	upsd.users.txt \
	upsmon.conf.txt \
	upssched.conf.txt

if WITH_MANS
MAN_CONF_PAGES = \
	nut.conf.5 \
	ups.conf.5 \
	upsd.conf.5 \
	upsd.users.5 \
	upsmon.conf.5 \
	upssched.conf.5
endif

man5_MANS = $(MAN_CONF_PAGES)

HTML_CONF_MANS = \
	nut.conf.html \
	ups.conf.html \
	upsd.conf.html \
	upsd.users.html \
	upsmon.conf.html \
	upssched.conf.html

SRC_CLIENT_PAGES = \
	nutupsdrv.txt \
	upsc.txt \
	upscmd.txt \
	upsd.txt \
	upsdrvctl.txt \
	upslog.txt \
	upsmon.txt \
	upsrw.txt \
	upssched.txt

if WITH_MANS
MAN_CLIENT_PAGES = \
	nutupsdrv.8 \
	upsc.8 \
	upscmd.8 \
	upsd.8 \
	upsdrvctl.8 \
	upslog.8 \
	upsmon.8 \
	upsrw.8 \
	upssched.8
endif

man8_MANS = $(MAN_CLIENT_PAGES)

HTML_CLIENT_MANS = \
	nutupsdrv.html \
	upsc.html \
	upscmd.html \
	upsd.html \
	upsdrvctl.html \
	upslog.html \
	upsmon.html \
	upsrw.html \
	upssched.html

SRC_TOOL_PAGES = nut-scanner.txt nut-recorder.txt

if WITH_MANS
MAN_TOOL_PAGES = nut-scanner.8 nut-recorder.8
endif

man8_MANS += $(MAN_TOOL_PAGES)

HTML_TOOL_MANS = nut-scanner.html nut-recorder.html

# CGI (--with-cgi) related manpages
SRC_CGI_PAGES = \
	hosts.conf.txt \
	upsset.conf.txt \
	upsstats.html.txt \
	upsset.cgi.txt \
	upsstats.cgi.txt \
	upsimage.cgi.txt

if WITH_MANS
MAN5_CGI_PAGES = \
	hosts.conf.5 \
	upsset.conf.5 \
	upsstats.html.5

MAN8_CGI_PAGES = \
	upsset.cgi.8 \
	upsstats.cgi.8 \
	upsimage.cgi.8
endif

if WITH_CGI
 man5_MANS += $(MAN5_CGI_PAGES)

 man8_MANS += $(MAN8_CGI_PAGES)
endif

HTML_CGI_MANS = \
	hosts.conf.html \
	upsset.conf.html \
	upsstats.html.html \
	upsset.cgi.html \
	upsstats.cgi.html \
	upsimage.cgi.html


# Development (--with-dev) related manpages
SRC_DEV_PAGES = \
	upsclient.txt \
	upscli_add_host_cert.txt \
	upscli_cleanup.txt \
	upscli_connect.txt \
	upscli_disconnect.txt \
	upscli_fd.txt \
	upscli_get.txt \
	upscli_init.txt \
	upscli_list_next.txt \
	upscli_list_start.txt \
	upscli_readline.txt \
	upscli_sendline.txt \
	upscli_splitaddr.txt \
	upscli_splitname.txt \
	upscli_ssl.txt \
	upscli_strerror.txt \
	upscli_upserror.txt \
	libnutclient.txt \
	libnutclient_commands.txt \
	libnutclient_devices.txt \
	libnutclient_general.txt \
	libnutclient_misc.txt \
	libnutclient_tcp.txt \
	libnutclient_variables.txt \
	nutscan.txt \
	nutscan_scan_snmp.txt \
	nutscan_scan_usb.txt \
	nutscan_scan_xml_http.txt \
	nutscan_scan_nut.txt \
	nutscan_scan_avahi.txt \
	nutscan_scan_ipmi.txt \
	nutscan_scan_eaton_serial.txt \
	nutscan_display_ups_conf.txt \
	nutscan_display_parsable.txt \
	nutscan_cidr_to_ip.txt \
	nutscan_new_device.txt \
	nutscan_free_device.txt \
	nutscan_add_option_to_device.txt \
	nutscan_add_device_to_device.txt \
	nutscan_init.txt \
	nutscan_get_serial_ports_list.txt \
	libupsclient-config.txt \
	skel.txt

if WITH_MANS
# NOTE: nutclient_*.3 has no source counterpart (libnutclient_*.txt)
MAN3_DEV_PAGES = \
	upsclient.3 \
	upscli_add_host_cert.3 \
	upscli_cleanup.3 \
	upscli_connect.3 \
	upscli_disconnect.3 \
	upscli_fd.3 \
	upscli_get.3 \
	upscli_init.3 \
	upscli_list_next.3 \
	upscli_list_start.3 \
	upscli_readline.3 \
	upscli_sendline.3 \
	upscli_splitaddr.3 \
	upscli_splitname.3 \
	upscli_ssl.3 \
	upscli_strerror.3 \
	upscli_upserror.3 \
	libnutclient.3 \
	libnutclient_commands.3 \
	libnutclient_devices.3 \
	libnutclient_general.3 \
	libnutclient_misc.3 \
	libnutclient_tcp.3 \
	libnutclient_variables.3 \
	nutclient_authenticate.3 \
	nutclient_destroy.3 \
	nutclient_device_forced_shutdown.3 \
	nutclient_device_login.3 \
	nutclient_device_master.3 \
	nutclient_execute_device_command.3 \
	nutclient_get_device_command_description.3 \
	nutclient_get_device_commands.3 \
	nutclient_get_device_description.3 \
	nutclient_get_device_num_logins.3 \
	nutclient_get_device_rw_variables.3 \
	nutclient_get_devices.3 \
	nutclient_get_device_variable_description.3 \
	nutclient_get_device_variables.3 \
	nutclient_get_device_variable_values.3 \
	nutclient_has_device.3 \
	nutclient_has_device_command.3 \
	nutclient_has_device_variable.3 \
	nutclient_logout.3 \
	nutclient_set_device_variable_value.3 \
	nutclient_set_device_variable_values.3 \
	nutclient_tcp_create_client.3 \
	nutclient_tcp_disconnect.3 \
	nutclient_tcp_get_timeout.3 \
	nutclient_tcp_is_connected.3 \
	nutclient_tcp_reconnect.3 \
	nutclient_tcp_set_timeout.3 \
	nutscan.3 \
	nutscan_scan_snmp.3 \
	nutscan_scan_usb.3 \
	nutscan_scan_xml_http.3 \
	nutscan_scan_nut.3 \
	nutscan_scan_avahi.3 \
	nutscan_scan_ipmi.3 \
	nutscan_scan_eaton_serial.3 \
	nutscan_display_ups_conf.3 \
	nutscan_display_parsable.3 \
	nutscan_cidr_to_ip.3 \
	nutscan_new_device.3 \
	nutscan_free_device.3 \
	nutscan_add_option_to_device.3 \
	nutscan_add_device_to_device.3 \
	nutscan_get_serial_ports_list.3 \
	nutscan_init.3

MAN1_DEV_PAGES = \
	libupsclient-config.1
endif

if WITH_DEV
 man3_MANS = $(MAN3_DEV_PAGES)

if !WITH_PKG_CONFIG
 man1_MANS = $(MAN1_DEV_PAGES)
endif
# WITH_DEV
endif

HTML_DEV_MANS = \
	upsclient.html \
	upscli_add_host_cert.html \
	upscli_cleanup.html \
	upscli_connect.html \
	upscli_disconnect.html \
	upscli_fd.html \
	upscli_get.html \
	upscli_init.html \
	upscli_list_next.html \
	upscli_list_start.html \
	upscli_readline.html \
	upscli_sendline.html \
	upscli_splitaddr.html \
	upscli_splitname.html \
	upscli_ssl.html \
	upscli_strerror.html \
	upscli_upserror.html \
	libnutclient.html \
	libnutclient_commands.html \
	libnutclient_devices.html \
	libnutclient_general.html \
	libnutclient_misc.html \
	libnutclient_tcp.html \
	libnutclient_variables.html \
	nutscan.html \
	nutscan_scan_snmp.html \
	nutscan_scan_usb.html \
	nutscan_scan_xml_http.html \
	nutscan_scan_nut.html \
	nutscan_scan_avahi.html \
	nutscan_scan_ipmi.html \
	nutscan_scan_eaton_serial.html \
	nutscan_display_ups_conf.html \
	nutscan_display_parsable.html \
	nutscan_cidr_to_ip.html \
	nutscan_new_device.html \
	nutscan_free_device.html \
	nutscan_add_option_to_device.html \
	nutscan_add_device_to_device.html \
	nutscan_get_serial_ports_list.html \
	nutscan_init.html \
	libupsclient-config.html \
	skel.html


# Drivers related manpages

# (--with-drivers=...)
if SOME_DRIVERS
 man8_MANS += $(DRIVER_MAN_LIST)

else

# (--with-serial)
SRC_SERIAL_PAGES = \
	al175.txt	\
	apcsmart.txt	\
	apcsmart-old.txt	\
	bcmxcp.txt 	\
	belkin.txt 	\
	belkinunv.txt	\
	bestfortress.txt	\
	bestuferrups.txt	\
	bestups.txt 	\
	bestfcom.txt	\
	blazer-common.txt	\
	blazer_ser.txt	\
	clone.txt \
	dummy-ups.txt	\
	etapro.txt	\
	everups.txt	\
	gamatronic.txt	\
	genericups.txt	\
	isbmex.txt	\
	ivtscd.txt	\
	liebert.txt	\
	liebert-esp2.txt	\
	masterguard.txt	\
	metasys.txt	\
	mge-shut.txt	\
	mge-utalk.txt	\
	oneac.txt		\
	microdowell.txt	\
	nutdrv_qx.txt	\
	optiups.txt	\
	powercom.txt 	\
	powerpanel.txt	\
	rhino.txt		\
	riello_ser.txt	\
	safenet.txt	\
	solis.txt		\
	tripplite.txt	\
	tripplitesu.txt	\
	upscode2.txt	\
	victronups.txt	\
	apcupsd-ups.txt

if WITH_MANS
MAN_SERIAL_PAGES = \
	al175.8	\
	apcsmart.8	\
	apcsmart-old.8	\
	bcmxcp.8 	\
	belkin.8 	\
	belkinunv.8	\
	bestfortress.8	\
	bestuferrups.8	\
	bestups.8 	\
	bestfcom.8	\
	blazer_ser.8	\
	clone.8 \
	dummy-ups.8	\
	etapro.8	\
	everups.8	\
	gamatronic.8	\
	genericups.8	\
	isbmex.8	\
	ivtscd.8	\
	liebert.8	\
	liebert-esp2.8	\
	masterguard.8	\
	metasys.8	\
	mge-shut.8	\
	mge-utalk.8	\
	nutdrv_qx.8	\
	oneac.8		\
	microdowell.8	\
	optiups.8	\
	powercom.8 	\
	powerpanel.8	\
	rhino.8		\
	riello_ser.8	\
	safenet.8	\
	solis.8		\
	tripplite.8	\
	tripplitesu.8	\
	upscode2.8	\
	victronups.8	\
	apcupsd-ups.8
endif

if WITH_SERIAL
  man8_MANS +=  $(MAN_SERIAL_PAGES)
endif

HTML_SERIAL_MANS = \
	al175.html	\
	apcsmart.html	\
	apcsmart-old.html	\
	bcmxcp.html 	\
	belkin.html 	\
	belkinunv.html	\
	bestfortress.html	\
	bestuferrups.html	\
	bestups.html 	\
	bestfcom.html	\
	blazer_ser.html	\
	clone.html \
	dummy-ups.html	\
	etapro.html	\
	everups.html	\
	gamatronic.html	\
	genericups.html	\
	isbmex.html	\
	ivtscd.html	\
	liebert.html	\
	liebert-esp2.html	\
	masterguard.html	\
	metasys.html	\
	mge-shut.html	\
	mge-utalk.html	\
	nutdrv_qx.html	\
	oneac.html		\
	microdowell.html	\
	optiups.html	\
	powercom.html 	\
	powerpanel.html	\
	rhino.html		\
	riello_ser.html	\
	safenet.html	\
	solis.html		\
	tripplite.html	\
	tripplitesu.html	\
	upscode2.html	\
	victronups.html	\
	apcupsd-ups.html

# (--with-snmp)
SRC_SNMP_PAGES = snmp-ups.txt
if WITH_MANS
MAN_SNMP_PAGES = snmp-ups.8
endif

if WITH_SNMP
  man8_MANS += $(MAN_SNMP_PAGES)
endif

HTML_SNMP_MANS = snmp-ups.html

# (--with-usb)
SRC_USB_LIBUSB_PAGES = \
	bcmxcp_usb.txt \
	blazer-common.txt	\
	blazer_usb.txt	\
	nutdrv_atcl_usb.txt \
	nutdrv_qx.txt	\
	richcomm_usb.txt \
	riello_usb.txt	\
	tripplite_usb.txt \
	usbhid-ups.txt

if WITH_MANS
MAN_USB_LIBUSB_PAGES = \
	bcmxcp_usb.8 \
	blazer_usb.8 \
	nutdrv_atcl_usb.8 \
	nutdrv_qx.8	\
	richcomm_usb.8 \
	riello_usb.8	\
	tripplite_usb.8 \
	usbhid-ups.8
endif

if WITH_USB
 man8_MANS += $(MAN_USB_LIBUSB_PAGES)
endif

HTML_USB_LIBUSB_MANS = \
	bcmxcp_usb.html \
	blazer_usb.html	\
	nutdrv_qx.html	\
	nutdrv_atcl_usb.html \
	richcomm_usb.html \
	riello_usb.html	\
	tripplite_usb.html \
	usbhid-ups.html

# (--with-neon)
SRC_NETXML_PAGES = netxml-ups.txt
if WITH_MANS
MAN_NETXML_PAGES = netxml-ups.8
endif

if WITH_NEON
   man8_MANS += $(MAN_NETXML_PAGES)
endif

HTML_NETXML_MANS = netxml-ups.html

# (--with-powerman)
SRC_POWERMAN_PAGES = powerman-pdu.txt
if WITH_MANS
MAN_POWERMAN_PAGES = powerman-pdu.8
endif

if WITH_LIBPOWERMAN
   man8_MANS += $(MAN_POWERMAN_PAGES)
endif

HTML_POWERMAN_MANS = powerman-pdu.html

# (--with-ipmi)
SRC_IPMIPSU_PAGES = nut-ipmipsu.txt
if WITH_MANS
MAN_IPMIPSU_PAGES = nut-ipmipsu.8
endif

if WITH_IPMI
   man8_MANS += $(MAN_IPMIPSU_PAGES)
endif

HTML_IPMIPSU_MANS = nut-ipmipsu.html

SRC_MACOSX_PAGES = macosx-ups.txt
if WITH_MANS
MAN_MACOSX_PAGES = macosx-ups.8
endif

if WITH_MACOSX
   man8_MANS += $(MAN_MACOSX_PAGES)
endif

HTML_MACOSX_MANS = macosx-ups.html

SRC_MODBUS_PAGES = phoenixcontact_modbus.txt
if WITH_MANS
MAN_MODBUS_PAGES = phoenixcontact_modbus.8
endif

if WITH_MODBUS
   man8_MANS += $(MAN_MODBUS_PAGES)
endif

HTML_MODBUS_MANS = phoenixcontact_modbus.html

SRC_LINUX_I2C_PAGES = asem.txt
if WITH_MANS
MAN_LINUX_I2C_PAGES = asem.8
endif

if WITH_LINUX_I2C
   man8_MANS += $(LINUX_I2C_PAGES)
endif

HTML_LINUX_I2C_MANS = asem.html

# SOME_DRIVERS
endif

MAN_MANS =
if WITH_MANS
MAN_MANS += \
	$(MAN_CONF_PAGES) \
	$(MAN_CLIENT_PAGES) \
	$(MAN_TOOL_PAGES) \
	$(MAN5_CGI_PAGES) \
	$(MAN8_CGI_PAGES) \
	$(MAN1_DEV_PAGES) \
	$(MAN3_DEV_PAGES) \
	$(MAN_SERIAL_PAGES) \
	$(MAN_SNMP_PAGES) \
	$(MAN_USB_LIBUSB_PAGES) \
	$(MAN_NETXML_PAGES) \
	$(MAN_POWERMAN_PAGES) \
	$(MAN_IPMIPSU_PAGES) \
	$(MAN_MACOSX_PAGES) \
	$(MAN_MODBUS_PAGES) \
	$(MAN_LINUX_I2C_PAGES)
endif

# distribute everything, even those not installed by default
# Note that 'dist' target requires AsciiDoc!
SRC_ALL_PAGES = \
	$(SRC_CONF_PAGES) \
	$(SRC_CLIENT_PAGES) \
	$(SRC_TOOL_PAGES) \
	$(SRC_CGI_PAGES) \
	$(SRC_DEV_PAGES) \
	$(SRC_SERIAL_PAGES) \
	$(SRC_SNMP_PAGES) \
	$(SRC_USB_LIBUSB_PAGES) \
	$(SRC_NETXML_PAGES) \
	$(SRC_POWERMAN_PAGES) \
	$(SRC_IPMIPSU_PAGES) \
	$(SRC_MACOSX_PAGES) \
	$(SRC_MODBUS_PAGES) \
	$(SRC_LINUX_I2C_PAGES)

EXTRA_DIST = \
	$(SRC_ALL_PAGES) \
	$(MAN_MANS) \
	asciidoc.conf

if ! WITH_MANS
if ! SKIP_MANS
# Cause "make dist" to fail, unless caller (like the stack of distcheck-dmf)
# runs ./configure --with-doc=skip (or --with-doc=man=skip specifically)
EXTRA_DIST += dist
dist:
	echo "ERROR: Manpage building was disabled by configure script, and these pages are required for our proper 'make dist'" >&2 ; false
endif
endif

# For builds done from dist'ed sources, there may be a conflict of timestamps
# between original *.txt files and pre-built manpages etc. leading to skipping
# of manpage re-generation even if that activity is possible and requested.
# Possibly a cleaner, but less portable, solution would be to touch the
# generated files to long ago. Current solution assumes good valid clocks :)
dist-hook:
	@echo "Fix up manpage source timestamps" && cd $(distdir) && touch $(SRC_ALL_PAGES)

HTML_MANS = \
	$(HTML_CONF_MANS) \
	$(HTML_CLIENT_MANS) \
	$(HTML_TOOL_MANS) \
	$(HTML_CGI_MANS) \
	$(HTML_DEV_MANS) \
	$(HTML_SERIAL_MANS) \
	$(HTML_SNMP_MANS) \
	$(HTML_USB_LIBUSB_MANS) \
	$(HTML_NETXML_MANS) \
	$(HTML_POWERMAN_MANS) \
	$(HTML_IPMIPSU_MANS) \
	$(HTML_MACOSX_MANS) \
	$(HTML_MODBUS_MANS) \
	$(HTML_LINUX_I2C_MANS)

all:

html-man: $(HTML_MANS) index.html

CLEANFILES = *.xml *.html

SUFFIXES = .txt .html .1 .3 .5 .8

# For builds with allowed installation of prebuild man pages, check that
# they exist in sources (make would pull them automatically as a fallback
# from failed lookup in build products). For builds that require rebuild
# of man pages, abort with error if build product is missing.
if DOC_INSTALL_DISTED_MANS
SRC_PREBUILT_CLAUSE=|| [ -r "$(srcdir)/`basename $@`" ]
else
SRC_PREBUILT_CLAUSE=
endif

if HAVE_ASCIIDOC

.txt.html:
	$(ASCIIDOC) --backend=xhtml11 \
		--attribute localdate=`TZ=UTC date +%Y-%m-%d` \
		--attribute localtime=`TZ=UTC date +%H:%M:%S` \
		--attribute nutversion="@PACKAGE_VERSION@" \
		-o $@ $<

### Prior to Asciidoc ~8.6.8, the --destination-dir flag didn't seem to affect the location of the intermediate .xml file.
### This parameter is currently required; see docs/Makefile.am for more detail.
A2X_MANPAGE_OPTS = --doctype manpage --format manpage \
	--xsltproc-opts "--nonet" \
	--attribute mansource="Network UPS Tools" \
	--attribute manversion="@PACKAGE_VERSION@" \
	--attribute manmanual="NUT Manual" \
	--destination-dir=.

.txt.1:
	$(A2X) $(A2X_MANPAGE_OPTS) $<

.txt.3:
	$(A2X) $(A2X_MANPAGE_OPTS) $<

.txt.5:
	$(A2X) $(A2X_MANPAGE_OPTS) $<

.txt.8:
	$(A2X) $(A2X_MANPAGE_OPTS) $<

else !HAVE_ASCIIDOC

.txt.html:
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.1:
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.3:
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.5:
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.8:
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

endif !HAVE_ASCIIDOC

spellcheck spellcheck-interactive spellcheck-sortdict:
	$(MAKE) -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC="$(SRC_ALL_PAGES)" $@
