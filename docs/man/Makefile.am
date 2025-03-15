# Network UPS Tools: man pages
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

# Is "egrep == grep -E" always valid? (maybe all a job for configure.ac)
#EGREP = egrep
EGREP = grep -E

SRC_ALL_PAGES = index.txt

ALL_TGT =
if WITH_MANS
ALL_TGT += all-man
endif WITH_MANS
if WITH_HTML_SINGLE
ALL_TGT += all-html
else !WITH_HTML_SINGLE
if WITH_HTML_CHUNKED
ALL_TGT += all-html
endif WITH_HTML_CHUNKED
endif !WITH_HTML_SINGLE

all: $(abs_top_builddir)/docs/man/.prep-src-docs
	+@case "$(ALL_TGT)" in \
	  *all-*) \
		if [ x"$(DOCS_NO_MAN)" = xtrue ] ; then echo "  DOCS_NO_MAN     SKIP: $@ called in docs/man/Makefile" ; exit 0 ; fi ; \
		echo "  DOC-FOLLOW-UP	Basic 'make $@' in `pwd` is done, following up with 'make $(ALL_TGT)' to ensure complex document types (if enabled)" ; \
		$(MAKE) $(AM_MAKEFLAGS) $(ALL_TGT) ; exit ;; \
	  esac ; exit 0

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
	nut.conf.$(MAN_SECTION_CFG) \
	ups.conf.$(MAN_SECTION_CFG) \
	upsd.conf.$(MAN_SECTION_CFG) \
	upsd.users.$(MAN_SECTION_CFG) \
	upsmon.conf.$(MAN_SECTION_CFG) \
	upssched.conf.$(MAN_SECTION_CFG)
endif WITH_MANS

man@MAN_SECTION_CFG_BASE@_MANS = $(MAN_CONF_PAGES)

HTML_CONF_MANS = \
	nut.conf.html \
	ups.conf.html \
	upsd.conf.html \
	upsd.users.html \
	upsmon.conf.html \
	upssched.conf.html

# NOTE: Currently SRC_DRIVERTOOL_PAGES are a separate list to generate
# a linkman-drivertool-names.txt file, but historically remain part of
# MAN/HTML_CLIENT_PAGES in the bigger picture.
SRC_DRIVERTOOL_PAGES = \
	nut-driver-enumerator.txt \
	upsdrvctl.txt \
	upsdrvsvcctl.txt

SRC_CLIENT_PAGES = \
	$(SRC_DRIVERTOOL_PAGES) \
	nutupsdrv.txt \
	upsc.txt \
	upscmd.txt \
	upsd.txt \
	upslog.txt \
	upsmon.txt \
	upsrw.txt \
	upssched.txt

if WITH_MANS
MAN_CLIENT_PAGES = \
	nutupsdrv.$(MAN_SECTION_CMD_SYS) \
	nut-driver-enumerator.$(MAN_SECTION_CMD_SYS) \
	upsc.$(MAN_SECTION_CMD_SYS) \
	upscmd.$(MAN_SECTION_CMD_SYS) \
	upsd.$(MAN_SECTION_CMD_SYS) \
	upsdrvctl.$(MAN_SECTION_CMD_SYS) \
	upsdrvsvcctl.$(MAN_SECTION_CMD_SYS) \
	upslog.$(MAN_SECTION_CMD_SYS) \
	upsmon.$(MAN_SECTION_CMD_SYS) \
	upsrw.$(MAN_SECTION_CMD_SYS) \
	upssched.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if HAVE_WINDOWS
SRC_CLIENT_PAGES += nut.exe.txt
if WITH_MANS
MAN_CLIENT_PAGES += nut.exe.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS
else !HAVE_WINDOWS
SRC_ALL_PAGES += nut.exe.txt
endif !HAVE_WINDOWS

if WITH_NUT_MONITOR
SRC_CLIENT_PAGES += NUT-Monitor.txt
if WITH_MANS
MAN_CLIENT_PAGES += \
	NUT-Monitor-py2gtk2.$(MAN_SECTION_CMD_SYS) \
	NUT-Monitor-py3qt5.$(MAN_SECTION_CMD_SYS) \
	NUT-Monitor.$(MAN_SECTION_CMD_SYS)

# Alias page for one text describing two commands:
NUT-Monitor-py2gtk2.$(MAN_SECTION_CMD_SYS): NUT-Monitor.$(MAN_SECTION_CMD_SYS)
	touch $@

NUT-Monitor-py3qt5.$(MAN_SECTION_CMD_SYS): NUT-Monitor.$(MAN_SECTION_CMD_SYS)
	touch $@

endif WITH_MANS
else !WITH_NUT_MONITOR
SRC_ALL_PAGES += NUT-Monitor.txt
endif !WITH_NUT_MONITOR

man@MAN_SECTION_CMD_SYS_BASE@_MANS = $(MAN_CLIENT_PAGES)

HTML_CLIENT_MANS = \
	nutupsdrv.html \
	nut-driver-enumerator.html \
	upsc.html \
	upscmd.html \
	upsd.html \
	upsdrvctl.html \
	upsdrvsvcctl.html \
	upslog.html \
	upsmon.html \
	upsrw.html \
	upssched.html

if HAVE_WINDOWS
HTML_CLIENT_MANS += nut.exe.html
endif HAVE_WINDOWS

if WITH_NUT_MONITOR
HTML_CLIENT_MANS += NUT-Monitor.html

# Can't make this work on all make implementations at once, so disabled for now
# Anyway it would be the same man-like page for several functions
HTML_CLIENT_MANS_FICTION = \
	NUT-Monitor-py2gtk2.html \
	NUT-Monitor-py3qt5.html

NUT-Monitor-py2gtk2.html: NUT-Monitor.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

NUT-Monitor-py3qt5.html: NUT-Monitor.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

endif WITH_NUT_MONITOR

SRC_TOOL_PAGES = nut-scanner.txt nut-recorder.txt nutconf.txt

if WITH_MANS
MAN_TOOL_PAGES = nut-scanner.$(MAN_SECTION_CMD_SYS) nut-recorder.$(MAN_SECTION_CMD_SYS) nutconf.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_TOOL_PAGES)

HTML_TOOL_MANS = nut-scanner.html nut-recorder.html nutconf.html

# CGI (--with-cgi) related manpages
SRC_CGI_PAGES = \
	hosts.conf.txt \
	upsset.conf.txt \
	upsstats.html.txt \
	upsset.cgi.txt \
	upsstats.cgi.txt \
	upsimage.cgi.txt

if WITH_MANS
MAN@MAN_SECTION_CFG_BASE@_CGI_PAGES = \
	hosts.conf.$(MAN_SECTION_CFG) \
	upsset.conf.$(MAN_SECTION_CFG) \
	upsstats.html.$(MAN_SECTION_CFG)

MAN@MAN_SECTION_CMD_SYS_BASE@_CGI_PAGES = \
	upsset.cgi.$(MAN_SECTION_CMD_SYS) \
	upsstats.cgi.$(MAN_SECTION_CMD_SYS) \
	upsimage.cgi.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_CGI
man@MAN_SECTION_CFG_BASE@_MANS += $(MAN@MAN_SECTION_CFG_BASE@_CGI_PAGES)
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN@MAN_SECTION_CMD_SYS_BASE@_CGI_PAGES)
endif WITH_CGI

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
	nutscan_scan_xml_http_range.txt \
	nutscan_scan_nut.txt \
	nutscan_scan_nut_simulation.txt \
	nutscan_scan_avahi.txt \
	nutscan_scan_ipmi.txt \
	nutscan_scan_eaton_serial.txt \
	nutscan_display_sanity_check.txt \
	nutscan_display_sanity_check_serial.txt \
	nutscan_display_ups_conf_with_sanity_check.txt \
	nutscan_display_ups_conf.txt \
	nutscan_display_parsable.txt \
	nutscan_init_ip_ranges.txt \
	nutscan_free_ip_ranges.txt \
	nutscan_stringify_ip_ranges.txt \
	nutscan_add_ip_range.txt \
	nutscan_ip_ranges_iter_init.txt \
	nutscan_ip_ranges_iter_inc.txt \
	nutscan_cidr_to_ip.txt \
	nutscan_new_device.txt \
	nutscan_free_device.txt \
	nutscan_add_option_to_device.txt \
	nutscan_add_device_to_device.txt \
	nutscan_init.txt \
	nutscan_get_serial_ports_list.txt \
	libupsclient-config.txt \
	sockdebug.txt \
	skel.txt

if WITH_MANS
# NOTE: nutclient_*.$(MAN_SECTION_API) has no source counterpart (libnutclient_*.txt)

LIBNUTCLIENT_MISC_DEPS= \
	nutclient_authenticate.$(MAN_SECTION_API) \
	nutclient_logout.$(MAN_SECTION_API) \
	nutclient_device_login.$(MAN_SECTION_API) \
	nutclient_get_device_num_logins.$(MAN_SECTION_API) \
	nutclient_device_master.$(MAN_SECTION_API) \
	nutclient_device_forced_shutdown.$(MAN_SECTION_API)

$(LIBNUTCLIENT_MISC_DEPS): libnutclient_misc.$(MAN_SECTION_API)
	touch $@

LIBNUTCLIENT_TCP_DEPS= \
	nutclient_tcp_create_client.$(MAN_SECTION_API) \
	nutclient_tcp_disconnect.$(MAN_SECTION_API) \
	nutclient_tcp_get_timeout.$(MAN_SECTION_API) \
	nutclient_tcp_is_connected.$(MAN_SECTION_API) \
	nutclient_tcp_reconnect.$(MAN_SECTION_API) \
	nutclient_tcp_set_timeout.$(MAN_SECTION_API)

$(LIBNUTCLIENT_TCP_DEPS): libnutclient_tcp.$(MAN_SECTION_API)
	touch $@

LIBNUTCLIENT_GENERAL_DEPS= \
	nutclient_destroy.$(MAN_SECTION_API)

$(LIBNUTCLIENT_GENERAL_DEPS): libnutclient_general.$(MAN_SECTION_API)
	touch $@

LIBNUTCLIENT_VARIABLES_DEPS= \
	nutclient_get_device_rw_variables.$(MAN_SECTION_API) \
	nutclient_get_device_variable_description.$(MAN_SECTION_API) \
	nutclient_get_device_variables.$(MAN_SECTION_API) \
	nutclient_get_device_variable_values.$(MAN_SECTION_API) \
	nutclient_has_device_variable.$(MAN_SECTION_API) \
	nutclient_set_device_variable_value.$(MAN_SECTION_API) \
	nutclient_set_device_variable_values.$(MAN_SECTION_API)

$(LIBNUTCLIENT_VARIABLES_DEPS): libnutclient_variables.$(MAN_SECTION_API)
	touch $@

LIBNUTCLIENT_COMMANDS_DEPS= \
	nutclient_execute_device_command.$(MAN_SECTION_API) \
	nutclient_get_device_command_description.$(MAN_SECTION_API) \
	nutclient_get_device_commands.$(MAN_SECTION_API) \
	nutclient_has_device_command.$(MAN_SECTION_API)

$(LIBNUTCLIENT_COMMANDS_DEPS): libnutclient_commands.$(MAN_SECTION_API)
	touch $@

LIBNUTCLIENT_DEVICES_DEPS= \
	nutclient_get_device_description.$(MAN_SECTION_API) \
	nutclient_get_devices.$(MAN_SECTION_API) \
	nutclient_has_device.$(MAN_SECTION_API)

$(LIBNUTCLIENT_DEVICES_DEPS): libnutclient_devices.$(MAN_SECTION_API)
	touch $@

MAN@MAN_SECTION_API_BASE@_DEV_PAGES = \
	upsclient.$(MAN_SECTION_API) \
	upscli_add_host_cert.$(MAN_SECTION_API) \
	upscli_cleanup.$(MAN_SECTION_API) \
	upscli_connect.$(MAN_SECTION_API) \
	upscli_disconnect.$(MAN_SECTION_API) \
	upscli_fd.$(MAN_SECTION_API) \
	upscli_get.$(MAN_SECTION_API) \
	upscli_init.$(MAN_SECTION_API) \
	upscli_list_next.$(MAN_SECTION_API) \
	upscli_list_start.$(MAN_SECTION_API) \
	upscli_readline.$(MAN_SECTION_API) \
	upscli_readline_timeout.$(MAN_SECTION_API) \
	upscli_sendline.$(MAN_SECTION_API) \
	upscli_sendline_timeout.$(MAN_SECTION_API) \
	upscli_splitaddr.$(MAN_SECTION_API) \
	upscli_splitname.$(MAN_SECTION_API) \
	upscli_ssl.$(MAN_SECTION_API) \
	upscli_strerror.$(MAN_SECTION_API) \
	upscli_upserror.$(MAN_SECTION_API) \
	libnutclient.$(MAN_SECTION_API) \
	libnutclient_commands.$(MAN_SECTION_API) \
	$(LIBNUTCLIENT_COMMANDS_DEPS) \
	libnutclient_devices.$(MAN_SECTION_API) \
	$(LIBNUTCLIENT_DEVICES_DEPS) \
	libnutclient_general.$(MAN_SECTION_API) \
	$(LIBNUTCLIENT_GENERAL_DEPS) \
	libnutclient_misc.$(MAN_SECTION_API) \
	$(LIBNUTCLIENT_MISC_DEPS) \
	libnutclient_tcp.$(MAN_SECTION_API) \
	$(LIBNUTCLIENT_TCP_DEPS) \
	libnutclient_variables.$(MAN_SECTION_API) \
	$(LIBNUTCLIENT_VARIABLES_DEPS) \
	nutscan.$(MAN_SECTION_API) \
	nutscan_scan_snmp.$(MAN_SECTION_API) \
	nutscan_scan_usb.$(MAN_SECTION_API) \
	nutscan_scan_xml_http_range.$(MAN_SECTION_API) \
	nutscan_scan_nut.$(MAN_SECTION_API) \
	nutscan_scan_nut_simulation.$(MAN_SECTION_API) \
	nutscan_scan_avahi.$(MAN_SECTION_API) \
	nutscan_scan_ipmi.$(MAN_SECTION_API) \
	nutscan_scan_eaton_serial.$(MAN_SECTION_API) \
	nutscan_display_sanity_check.$(MAN_SECTION_API) \
	nutscan_display_sanity_check_serial.$(MAN_SECTION_API) \
	nutscan_display_ups_conf_with_sanity_check.$(MAN_SECTION_API) \
	nutscan_display_ups_conf.$(MAN_SECTION_API) \
	nutscan_display_parsable.$(MAN_SECTION_API) \
	nutscan_init_ip_ranges.$(MAN_SECTION_API) \
	nutscan_free_ip_ranges.$(MAN_SECTION_API) \
	nutscan_stringify_ip_ranges.$(MAN_SECTION_API) \
	nutscan_add_ip_range.$(MAN_SECTION_API) \
	nutscan_ip_ranges_iter_init.$(MAN_SECTION_API) \
	nutscan_ip_ranges_iter_inc.$(MAN_SECTION_API) \
	nutscan_cidr_to_ip.$(MAN_SECTION_API) \
	nutscan_new_device.$(MAN_SECTION_API) \
	nutscan_free_device.$(MAN_SECTION_API) \
	nutscan_add_option_to_device.$(MAN_SECTION_API) \
	nutscan_add_commented_option_to_device.$(MAN_SECTION_API) \
	nutscan_add_device_to_device.$(MAN_SECTION_API) \
	nutscan_get_serial_ports_list.$(MAN_SECTION_API) \
	nutscan_init.$(MAN_SECTION_API)

# Alias page for one text describing two commands:
upscli_readline_timeout.$(MAN_SECTION_API): upscli_readline.$(MAN_SECTION_API)
	touch $@

upscli_sendline_timeout.$(MAN_SECTION_API): upscli_sendline.$(MAN_SECTION_API)
	touch $@

nutscan_scan_ip_range_snmp.$(MAN_SECTION_API): nutscan_scan_snmp.$(MAN_SECTION_API)
	touch $@

nutscan_scan_ip_range_xml_http.$(MAN_SECTION_API): nutscan_scan_xml_http_range.$(MAN_SECTION_API)
	touch $@

nutscan_scan_ip_range_nut.$(MAN_SECTION_API): nutscan_scan_nut.$(MAN_SECTION_API)
	touch $@

nutscan_scan_ip_range_ipmi.$(MAN_SECTION_API): nutscan_scan_ipmi.$(MAN_SECTION_API)
	touch $@

nutscan_add_commented_option_to_device.$(MAN_SECTION_API): nutscan_add_option_to_device.$(MAN_SECTION_API)
	touch $@

MAN@MAN_SECTION_CMD_USR_BASE@_DEV_PAGES = \
	libupsclient-config.$(MAN_SECTION_CMD_USR)

MAN@MAN_SECTION_CMD_SYS_BASE@_DEV_PAGES = \
	sockdebug.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_DEV
man@MAN_SECTION_API_BASE@_MANS = $(MAN@MAN_SECTION_API_BASE@_DEV_PAGES)
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN@MAN_SECTION_CMD_SYS_BASE@_DEV_PAGES)

if !WITH_PKG_CONFIG
man@MAN_SECTION_CMD_USR_BASE@_MANS = $(MAN@MAN_SECTION_CMD_USR_BASE@_DEV_PAGES)
endif !WITH_PKG_CONFIG

endif WITH_DEV

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
	nutscan_scan_xml_http_range.html \
	nutscan_scan_nut.html \
	nutscan_scan_nut_simulation.html \
	nutscan_scan_avahi.html \
	nutscan_scan_ipmi.html \
	nutscan_scan_eaton_serial.html \
	nutscan_display_sanity_check.html \
	nutscan_display_sanity_check_serial.html \
	nutscan_display_ups_conf_with_sanity_check.html \
	nutscan_display_ups_conf.html \
	nutscan_display_parsable.html \
	nutscan_init_ip_ranges.html \
	nutscan_free_ip_ranges.html \
	nutscan_stringify_ip_ranges.html \
	nutscan_add_ip_range.html \
	nutscan_ip_ranges_iter_init.html \
	nutscan_ip_ranges_iter_inc.html \
	nutscan_cidr_to_ip.html \
	nutscan_new_device.html \
	nutscan_free_device.html \
	nutscan_add_option_to_device.html \
	nutscan_add_device_to_device.html \
	nutscan_get_serial_ports_list.html \
	nutscan_init.html \
	libupsclient-config.html \
	sockdebug.html \
	skel.html

# Can't make this work on all make implementations at once, so disabled for now
# Anyway it would be the same man-like page for several functions
HTML_DEV_MANS_FICTION = \
	upscli_readline_timeout.html \
	upscli_sendline_timeout.html \
	nutscan_scan_ip_range_snmp.html \
	nutscan_scan_ip_range_xml_http.html \
	nutscan_scan_ip_range_nut.html \
	nutscan_scan_ip_range_ipmi.html \
	nutscan_add_commented_option_to_device.html

upscli_readline_timeout.html: upscli_readline.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

upscli_sendline_timeout.html: upscli_sendline.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

nutscan_scan_ip_range_snmp.html: nutscan_scan_snmp.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

nutscan_scan_ip_range_xml_http.html: nutscan_scan_xml_http_range.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

nutscan_scan_ip_range_nut.html: nutscan_scan_nut.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

nutscan_scan_ip_range_ipmi.html: nutscan_scan_ipmi.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

nutscan_add_commented_option_to_device.html: nutscan_add_option_to_device.html
	test -n "$?" -a -s "$@" && rm -f $@ && ln -s $? $@

# Drivers related manpages

# If (--with-drivers=...) then we only build specific documents, however
# still do track (and EXTRA_DIST, and spellcheck) all available sources.
if SOME_DRIVERS
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(DRIVER_MAN_LIST)
endif

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
	bicker_ser.txt	\
	blazer-common.txt	\
	blazer_ser.txt	\
	clone.txt	\
	clone-outlet.txt	\
	dummy-ups.txt	\
	etapro.txt	\
	everups.txt	\
	gamatronic.txt	\
	genericups.txt	\
	isbmex.txt	\
	ivtscd.txt	\
	liebert.txt	\
	liebert-gxe.txt	\
	liebert-esp2.txt	\
	masterguard.txt	\
	metasys.txt	\
	mge-shut.txt	\
	mge-utalk.txt	\
	oneac.txt		\
	microdowell.txt	\
	microsol-apc.txt	\
	nutdrv_siemens_sitop.txt	\
	optiups.txt	\
	powercom.txt 	\
	powerpanel.txt	\
	rhino.txt		\
	riello_ser.txt	\
	sms_ser.txt \
	safenet.txt	\
	solis.txt		\
	tripplite.txt	\
	tripplitesu.txt	\
	upscode2.txt	\
	victronups.txt	\
	apcupsd-ups.txt

if HAVE_LINUX_SERIAL_H
# Temporary, until ported to more OSes
SRC_SERIAL_PAGES += \
	nhs_ser.txt
endif HAVE_LINUX_SERIAL_H

if ! SOME_DRIVERS
if WITH_MANS
MAN_SERIAL_PAGES = \
	al175.$(MAN_SECTION_CMD_SYS)	\
	apcsmart.$(MAN_SECTION_CMD_SYS)	\
	apcsmart-old.$(MAN_SECTION_CMD_SYS)	\
	bcmxcp.$(MAN_SECTION_CMD_SYS) 	\
	belkin.$(MAN_SECTION_CMD_SYS) 	\
	belkinunv.$(MAN_SECTION_CMD_SYS)	\
	bestfortress.$(MAN_SECTION_CMD_SYS)	\
	bestuferrups.$(MAN_SECTION_CMD_SYS)	\
	bestups.$(MAN_SECTION_CMD_SYS) 	\
	bestfcom.$(MAN_SECTION_CMD_SYS)	\
	bicker_ser.$(MAN_SECTION_CMD_SYS)	\
	blazer_ser.$(MAN_SECTION_CMD_SYS)	\
	clone.$(MAN_SECTION_CMD_SYS)	\
	clone-outlet.$(MAN_SECTION_CMD_SYS)	\
	dummy-ups.$(MAN_SECTION_CMD_SYS)	\
	etapro.$(MAN_SECTION_CMD_SYS)	\
	everups.$(MAN_SECTION_CMD_SYS)	\
	gamatronic.$(MAN_SECTION_CMD_SYS)	\
	genericups.$(MAN_SECTION_CMD_SYS)	\
	isbmex.$(MAN_SECTION_CMD_SYS)	\
	ivtscd.$(MAN_SECTION_CMD_SYS)	\
	liebert.$(MAN_SECTION_CMD_SYS)	\
	liebert-gxe.$(MAN_SECTION_CMD_SYS)	\
	liebert-esp2.$(MAN_SECTION_CMD_SYS)	\
	masterguard.$(MAN_SECTION_CMD_SYS)	\
	metasys.$(MAN_SECTION_CMD_SYS)	\
	mge-shut.$(MAN_SECTION_CMD_SYS)	\
	mge-utalk.$(MAN_SECTION_CMD_SYS)	\
	oneac.$(MAN_SECTION_CMD_SYS)		\
	microdowell.$(MAN_SECTION_CMD_SYS)	\
	microsol-apc.$(MAN_SECTION_CMD_SYS)	\
	nutdrv_siemens_sitop.$(MAN_SECTION_CMD_SYS)	\
	optiups.$(MAN_SECTION_CMD_SYS)	\
	powercom.$(MAN_SECTION_CMD_SYS) 	\
	powerpanel.$(MAN_SECTION_CMD_SYS)	\
	rhino.$(MAN_SECTION_CMD_SYS)		\
	riello_ser.$(MAN_SECTION_CMD_SYS)	\
	sms_ser.$(MAN_SECTION_CMD_SYS) \
	safenet.$(MAN_SECTION_CMD_SYS)	\
	solis.$(MAN_SECTION_CMD_SYS)		\
	tripplite.$(MAN_SECTION_CMD_SYS)	\
	tripplitesu.$(MAN_SECTION_CMD_SYS)	\
	upscode2.$(MAN_SECTION_CMD_SYS)	\
	victronups.$(MAN_SECTION_CMD_SYS)	\
	apcupsd-ups.$(MAN_SECTION_CMD_SYS)

if HAVE_LINUX_SERIAL_H
# Temporary, until ported to more OSes
MAN_SERIAL_PAGES += \
	nhs_ser.$(MAN_SECTION_CMD_SYS)
endif HAVE_LINUX_SERIAL_H

endif WITH_MANS


if WITH_SERIAL
man@MAN_SECTION_CMD_SYS_BASE@_MANS +=  $(MAN_SERIAL_PAGES)
endif WITH_SERIAL

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
	bicker_ser.html	\
	blazer_ser.html	\
	clone.html	\
	clone-outlet.html	\
	dummy-ups.html	\
	etapro.html	\
	everups.html	\
	gamatronic.html	\
	genericups.html	\
	isbmex.html	\
	ivtscd.html	\
	liebert.html	\
	liebert-gxe.html\
	liebert-esp2.html	\
	masterguard.html	\
	metasys.html	\
	mge-shut.html	\
	mge-utalk.html	\
	oneac.html		\
	microdowell.html	\
	microsol-apc.html	\
	nutdrv_siemens_sitop.html	\
	optiups.html	\
	powercom.html 	\
	powerpanel.html	\
	rhino.html		\
	riello_ser.html	\
	sms_ser.html \
	safenet.html	\
	solis.html		\
	tripplite.html	\
	tripplitesu.html	\
	upscode2.html	\
	victronups.html	\
	apcupsd-ups.html

if HAVE_LINUX_SERIAL_H
# Temporary, until ported to more OSes
HTML_SERIAL_MANS += \
	nhs_ser.html
endif HAVE_LINUX_SERIAL_H

endif ! SOME_DRIVERS

# (--with-snmp)
SRC_SNMP_PAGES = snmp-ups.txt
if ! SOME_DRIVERS
if WITH_MANS
MAN_SNMP_PAGES = snmp-ups.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_SNMP
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_SNMP_PAGES)
endif WITH_SNMP

HTML_SNMP_MANS = snmp-ups.html
endif ! SOME_DRIVERS

# (--with-usb)
SRC_USB_LIBUSB_PAGES = \
	bcmxcp_usb.txt \
	blazer-common.txt	\
	blazer_usb.txt	\
	nutdrv_atcl_usb.txt \
	nut_usb_addvars.txt \
	richcomm_usb.txt \
	riello_usb.txt	\
	tripplite_usb.txt \
	usbhid-ups.txt

if ! SOME_DRIVERS
# NOTE: nut_usb_addvars and blazer-common are not standalone man pages
if WITH_MANS
MAN_USB_LIBUSB_PAGES = \
	bcmxcp_usb.$(MAN_SECTION_CMD_SYS) \
	blazer_usb.$(MAN_SECTION_CMD_SYS) \
	nutdrv_atcl_usb.$(MAN_SECTION_CMD_SYS) \
	richcomm_usb.$(MAN_SECTION_CMD_SYS) \
	riello_usb.$(MAN_SECTION_CMD_SYS)	\
	tripplite_usb.$(MAN_SECTION_CMD_SYS) \
	usbhid-ups.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_USB
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_USB_LIBUSB_PAGES)
endif WITH_USB

HTML_USB_LIBUSB_MANS = \
	bcmxcp_usb.html \
	blazer_usb.html	\
	nutdrv_atcl_usb.html \
	richcomm_usb.html \
	riello_usb.html	\
	tripplite_usb.html \
	usbhid-ups.html
endif ! SOME_DRIVERS

# (--with-serial / --with-usb)
SRC_SERIAL_USB_PAGES = \
	nutdrv_qx.txt

if ! SOME_DRIVERS
if WITH_MANS
MAN_SERIAL_USB_PAGES = \
	nutdrv_qx.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_SERIAL
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_SERIAL_USB_PAGES)
else !WITH_SERIAL
if WITH_USB
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_SERIAL_USB_PAGES)
endif WITH_USB
endif !WITH_SERIAL

HTML_SERIAL_USB_MANS = \
	nutdrv_qx.html
endif ! SOME_DRIVERS

# (--with-neon)
SRC_NETXML_PAGES = netxml-ups.txt
if ! SOME_DRIVERS
if WITH_MANS
MAN_NETXML_PAGES = netxml-ups.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_NEON
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_NETXML_PAGES)
endif WITH_NEON

HTML_NETXML_MANS = netxml-ups.html
endif ! SOME_DRIVERS

# (--with-powerman)
SRC_POWERMAN_PAGES = powerman-pdu.txt
if ! SOME_DRIVERS
if WITH_MANS
MAN_POWERMAN_PAGES = powerman-pdu.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_LIBPOWERMAN
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_POWERMAN_PAGES)
endif WITH_LIBPOWERMAN

HTML_POWERMAN_MANS = powerman-pdu.html
endif ! SOME_DRIVERS

# (--with-ipmi)
SRC_IPMIPSU_PAGES = nut-ipmipsu.txt
if ! SOME_DRIVERS
if WITH_MANS
MAN_IPMIPSU_PAGES = nut-ipmipsu.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_IPMI
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_IPMIPSU_PAGES)
endif WITH_IPMI

HTML_IPMIPSU_MANS = nut-ipmipsu.html
endif ! SOME_DRIVERS

# (--with-macosx_ups)
SRC_MACOSX_PAGES = macosx-ups.txt
if ! SOME_DRIVERS
if WITH_MANS
MAN_MACOSX_PAGES = macosx-ups.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_MACOSX
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_MACOSX_PAGES)
endif WITH_MACOSX

HTML_MACOSX_MANS = macosx-ups.html
endif ! SOME_DRIVERS

# (--with-modbus)
SRC_MODBUS_PAGES = phoenixcontact_modbus.txt \
	generic_modbus.txt \
	huawei-ups2000.txt \
	socomec_jbus.txt   \
	adelsystem_cbi.txt \
	apc_modbus.txt

if ! SOME_DRIVERS
if WITH_MANS
MAN_MODBUS_PAGES = phoenixcontact_modbus.$(MAN_SECTION_CMD_SYS) \
	generic_modbus.$(MAN_SECTION_CMD_SYS) \
	huawei-ups2000.$(MAN_SECTION_CMD_SYS) \
	socomec_jbus.$(MAN_SECTION_CMD_SYS)   \
	adelsystem_cbi.$(MAN_SECTION_CMD_SYS) \
	apc_modbus.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_MODBUS
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_MODBUS_PAGES)
endif WITH_MODBUS

HTML_MODBUS_MANS = phoenixcontact_modbus.html \
	generic_modbus.html \
	huawei-ups2000.html \
	socomec_jbus.html   \
	adelsystem_cbi.html \
	apc_modbus.html
endif ! SOME_DRIVERS

# (--with-linux_i2c)
SRC_LINUX_I2C_PAGES = asem.txt pijuice.txt hwmon_ina219.txt
if ! SOME_DRIVERS
if WITH_MANS
MAN_LINUX_I2C_PAGES = asem.$(MAN_SECTION_CMD_SYS) pijuice.$(MAN_SECTION_CMD_SYS) hwmon_ina219.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_LINUX_I2C
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_LINUX_I2C_PAGES)
endif WITH_LINUX_I2C

HTML_LINUX_I2C_MANS = asem.html pijuice.html hwmon_ina219.html
endif ! SOME_DRIVERS

# (--with-gpio)
SRC_GPIO_PAGES = generic_gpio.txt
if ! SOME_DRIVERS
if WITH_MANS
MAN_GPIO_PAGES = generic_gpio.$(MAN_SECTION_CMD_SYS)
endif WITH_MANS

if WITH_GPIO
man@MAN_SECTION_CMD_SYS_BASE@_MANS += $(MAN_GPIO_PAGES)
endif WITH_GPIO

HTML_GPIO_MANS = generic_gpio.html
endif ! SOME_DRIVERS

MAN_MANS =
if WITH_MANS
MAN_MANS += \
	$(MAN_CONF_PAGES) \
	$(MAN_CLIENT_PAGES) \
	$(MAN_TOOL_PAGES) \
	$(MAN@MAN_SECTION_CFG_BASE@_CGI_PAGES) \
	$(MAN@MAN_SECTION_CMD_SYS_BASE@_CGI_PAGES) \
	$(MAN@MAN_SECTION_CMD_USR_BASE@_DEV_PAGES) \
	$(MAN@MAN_SECTION_API_BASE@_DEV_PAGES) \
	$(MAN@MAN_SECTION_CMD_SYS_BASE@_DEV_PAGES) \
	$(MAN_SERIAL_PAGES) \
	$(MAN_SNMP_PAGES) \
	$(MAN_USB_LIBUSB_PAGES) \
	$(MAN_SERIAL_USB_PAGES) \
	$(MAN_NETXML_PAGES) \
	$(MAN_POWERMAN_PAGES) \
	$(MAN_IPMIPSU_PAGES) \
	$(MAN_MACOSX_PAGES) \
	$(MAN_MODBUS_PAGES) \
	$(MAN_LINUX_I2C_PAGES) \
	$(MAN_GPIO_PAGES)
endif WITH_MANS

SRC_DRIVERS_PAGES = \
	$(SRC_SERIAL_PAGES) \
	$(SRC_SNMP_PAGES) \
	$(SRC_USB_LIBUSB_PAGES) \
	$(SRC_SERIAL_USB_PAGES) \
	$(SRC_NETXML_PAGES) \
	$(SRC_POWERMAN_PAGES) \
	$(SRC_IPMIPSU_PAGES) \
	$(SRC_MACOSX_PAGES) \
	$(SRC_MODBUS_PAGES) \
	$(SRC_LINUX_I2C_PAGES) \
	$(SRC_GPIO_PAGES)

if SOME_DRIVERS
# (Legacy note) The list above probably came up empty in this case, so make sure
# that the drivers requested by configure script are documented in the build;
# notably in the linkman-driver-names.txt file.
SRC_DRIVERS_PAGES += \
	$(DRIVER_MAN_LIST_PAGES)
endif

# distribute everything, even those not installed by default
# Note that 'dist' target requires AsciiDoc!
SRC_ALL_PAGES += \
	$(SRC_CONF_PAGES) \
	$(SRC_CLIENT_PAGES) \
	$(SRC_TOOL_PAGES) \
	$(SRC_CGI_PAGES) \
	$(SRC_DEV_PAGES) \
	$(SRC_DRIVERS_PAGES)

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
	@echo "ERROR: Manpage building was disabled by configure script, and these pages are required for our proper 'make dist'" >&2 ; false
endif ! SKIP_MANS
endif ! WITH_MANS

# For builds done from dist'ed sources, there may be a conflict of timestamps
# between original *.txt files and pre-built manpages etc. leading to skipping
# of manpage re-generation even if that activity is possible and requested.
# Possibly a cleaner, but less portable, solution would be to touch the
# generated files to long ago. Current solution assumes good valid clocks :)
dist-hook:
	@echo "Fix up manpage source timestamps" && cd $(distdir) && touch $(SRC_ALL_PAGES)

HTML_MANS = \
	index.html \
	$(HTML_CONF_MANS) \
	$(HTML_CLIENT_MANS) \
	$(HTML_TOOL_MANS) \
	$(HTML_CGI_MANS) \
	$(HTML_DEV_MANS) \
	$(HTML_SERIAL_MANS) \
	$(HTML_SNMP_MANS) \
	$(HTML_USB_LIBUSB_MANS) \
	$(HTML_SERIAL_USB_MANS) \
	$(HTML_NETXML_MANS) \
	$(HTML_POWERMAN_MANS) \
	$(HTML_IPMIPSU_MANS) \
	$(HTML_MACOSX_MANS) \
	$(HTML_MODBUS_MANS) \
	$(HTML_LINUX_I2C_MANS) \
	$(HTML_GPIO_MANS)

# htmlmandir is set by autoconf/automake
htmlman_DATA =
if WITH_HTML_SINGLE
htmlman_DATA += $(HTML_MANS)
else !WITH_HTML_SINGLE
if WITH_HTML_CHUNKED
htmlman_DATA += $(HTML_MANS)
endif WITH_HTML_CHUNKED
endif !WITH_HTML_SINGLE

# Note: target documents, except nutupsdrv.txt itself, start the
# list of drivers with `- linkman:nutupsdrv[$(MAN_SECTION_CMD_SYS)]` entry
# To regenerate these files, do `make distclean` first
LINKMAN_INCLUDE_GENERATED = linkman-driver-names.txt linkman-drivertool-names.txt
LINKMAN_INCLUDE_CONSUMERS = index.txt upsd.txt nutupsdrv.txt

linkman-driver-names.txt:
	@if test x"$(abs_srcdir)" != x"$(abs_builddir)" ; then \
	    if ! test -s "$(builddir)/$@" && test -s "$(srcdir)/$(@F)" ; then \
	        ln -fs "$(srcdir)/$(@F)" "$(builddir)/" ; \
	    fi ; \
	 fi
	@if test -s "$@" ; then exit 0 ; fi ; \
	 (LC_ALL=C; LANG=C; export LC_ALL LANG; \
	  for F in $(SRC_DRIVERS_PAGES) ; do echo "$$F" ; done \
	    | grep -vE '^nutupsdrv\.txt$$' \
	    | sort -n | uniq \
	    | sed 's,^\(.*\)\.txt$$,- linkman:\1[$(MAN_SECTION_CMD_SYS)],' ; \
	 ) > "$@"
	@if test ! -s "$@" ; then echo "- No NUT drivers were built" > "$@" ; fi

linkman-drivertool-names.txt:
	@if test x"$(abs_srcdir)" != x"$(abs_builddir)" ; then \
	    if ! test -s "$(builddir)/$@" && test -s "$(srcdir)/$(@F)" ; then \
	        ln -fs "$(srcdir)/$(@F)" "$(builddir)/" ; \
	    fi ; \
	 fi
	@if test -s "$@" ; then exit 0 ; fi ; \
	 (LC_ALL=C; LANG=C; export LC_ALL LANG; \
	  for F in $(SRC_DRIVERTOOL_PAGES) ; do echo "$$F" ; done \
	    | sort -n | uniq \
	    | sed 's,^\(.*\)\.txt$$,- linkman:\1[$(MAN_SECTION_CMD_SYS)],' ; \
	 ) > "$@"
	@if test ! -s "$@" ; then echo "- No NUT driver tools were built" > "$@" ; fi

# Dependencies are about particular filenames, since over time
# we might have several use-cases for LINKMAN_INCLUDE_GENERATED:
$(LINKMAN_INCLUDE_CONSUMERS): linkman-driver-names.txt linkman-drivertool-names.txt

# These files are generated when we build from git source so not tracked in
# git, and not part of tarball (to be in builddir) - so not in EXTRA_DIST.
DISTCLEANFILES = $(LINKMAN_INCLUDE_GENERATED)

# Make sure sources are there for out-of-tree builds:
$(HTML_MANS) $(MAN_MANS): $(abs_top_builddir)/docs/man/.prep-src-docs

all-html html-man: $(HTML_MANS)

# Have a way to build all man pages, not just those that fit currently
# configured drivers, daemons, developer aspect, etc.
all-man man-man: $(MAN_MANS)

if WITH_MANS
if ! SKIP_MANS
check-local: check-man
else
check-local: check-man-txt check-man-pages
	@echo "Man-page generation was SKIPPED per user request, so pregenerated pages were sanity-checked (if any)" >&2
endif
else
check-local: check-man-txt check-man-pages
	@echo "Man-page generation was not done, so pregenerated pages were sanity-checked (if any)" >&2
endif

check-man check-man-man: check-man-txt check-man-pages check-html-man

# the "for" loops might better use $^ but it might be not portable
check-man-html: check-html-man

check-html-man: .check-html-man
.check-html-man: $(HTML_MANS) Makefile
	@FAILED=""; CHECKED=0; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	for F in $(HTML_MANS) ; do \
	    CHECKED="`expr $$CHECKED + 1`"; \
	    test -s "$$F" && { file "$$F" | $(EGREP) -i '(XML|HTML.*document|symbolic link)' > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	done; if test -n "$$FAILED" ; then \
	    echo "FAILED HTML-man sanity check for:$$FAILED" >&2 ; file $$FAILED >&2 ; exit 1; \
	fi; echo "PASSED HTML-man sanity check (checked $$CHECKED files)"; exit 0
	@touch $@

# Note: many man-pages here have code samples and are mis-identified as C code
check-man-page: check-man-pages

# Man-pages may be pre-generated (srcdir), or re-built (builddir)
check-man-pages: .check-man-pages
.check-man-pages: $(MAN_MANS) Makefile
	@FAILED=""; CHECKED=0; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	for F in $(MAN_MANS) ; do \
	    CHECKED="`expr $$CHECKED + 1`"; \
	    ( test -s "$(abs_srcdir)/$$F" && { file "$(abs_srcdir)/$$F" | $(EGREP) -i '(roff.* input|C source|ASCII text)' > /dev/null ; } ) || \
	    ( test -s "$(abs_builddir)/$$F" && { file "$(abs_builddir)/$$F" | $(EGREP) -i '(roff.* input|C source|ASCII text)' > /dev/null ; } ) || \
	    FAILED="$$FAILED $$F" ; \
	done; if test -n "$$FAILED" ; then \
	    echo "FAILED man-page sanity check for:$$FAILED" >&2 ; \
	    ( echo "SRCDIR:"; cd "$(abs_srcdir)/" && file $$FAILED ; \
	      echo "BUILDDIR:"; cd "$(abs_builddir)/" && file $$FAILED ; \
	    ) >&2 ; exit 1; \
	fi; echo "PASSED man-page sanity check (checked $$CHECKED files)"; exit 0
	@touch $@

check-man-source: check-man-txt

# Note: (GNU) make can helpfully add VPATH to the short source filenames
# which we had listed above, so the "case" below handles two possibilities
check-man-txt: .check-man-txt
.check-man-txt: $(SRC_ALL_PAGES) Makefile
	@FAILED=""; CHECKED=0; LANG=C; LC_ALL=C; export LANG; export LC_ALL; \
	( cd $(abs_srcdir) ) || exit; \
	for F in $(SRC_ALL_PAGES) ; do \
	    CHECKED="`expr $$CHECKED + 1`"; \
	    case "$$F" in \
	        */*) ;; \
	        *) F="$(abs_srcdir)/$$F" ;; \
	    esac ; \
	    test -s "$$F" && { file "$$F" | $(EGREP) -i '(ASCII|UTF-8|Unicode|ISO-8859|English).* text' > /dev/null ; } || FAILED="$$FAILED $$F" ; \
	done; if test -n "$$FAILED" ; then \
	    echo "FAILED man-source sanity check for:$$FAILED" >&2 ; file $$FAILED >&2 ; exit 1; \
	fi; echo "PASSED man-source sanity check (checked $$CHECKED files)"; exit 0
	@touch $@

CLEANFILES = *-spellchecked .prep-src-docs *-prepped .check-*

SUFFIXES = .txt-prepped .txt .html .$(MAN_SECTION_CMD_USR) .$(MAN_SECTION_API) .$(MAN_SECTION_CFG) .$(MAN_SECTION_CMD_SYS)

# For builds with allowed installation of prebuilt man pages, check that
# they exist in sources (make would pull them automatically as a fallback
# from failed lookup in build products). For builds that require rebuild
# of man pages, abort with error if build product is missing.
if DOC_INSTALL_DISTED_MANS
SRC_PREBUILT_CLAUSE=|| [ -r "$(srcdir)/`basename $@`" ]
else
SRC_PREBUILT_CLAUSE=
endif

if HAVE_ASCIIDOC

CLEANFILES += *.$(MAN_SECTION_CMD_USR) *.$(MAN_SECTION_API) *.$(MAN_SECTION_CFG) *.$(MAN_SECTION_CMD_SYS) *.xml *.html *.pdf

# Working around a2x not friendly to parallelized runs.
# See more details in the main NUT docs/Makefile.am
# NOTE: MKDIR_P may be defined via expanded $(top_builddir)/install-sh
# so should be run from $(abs_builddir) to be safe, as we jump around
# the build workspace
DOCBUILD_BEGIN = { \
    if test -n "$${A2X_OUTDIR}" && test "$${A2X_OUTDIR}" != '.' ; then \
        rm -rf "./$${A2X_OUTDIR}" || true ; \
        test -d "$@" && rm -rf "$@" || true ; \
        _CWD="`pwd`" && (cd '$(abs_builddir)' && $(MKDIR_P) "$${_CWD}/$${A2X_OUTDIR}") || exit ; \
        for F in $(LINKMAN_INCLUDE_GENERATED) ; do \
            if [ -s "./$$F" ] ; then ln -f -s "../../$$F" "./$${A2X_OUTDIR}/" ; else \
            if [ -s "$(abs_srcdir)/$$F" ] ; then ln -f -s "$(abs_srcdir)/$$F" "./$${A2X_OUTDIR}/" ; fi ; fi ; \
        done ; \
    else A2X_OUTDIR='.' ; fi; \
    if test -s "${builddir}/docbook-xsl.css" \
    && test -r "${builddir}/docbook-xsl.css" \
    && ! test -w "${builddir}/docbook-xsl.css" \
    ; then chmod u+w "${builddir}/docbook-xsl.css" ; fi ; \
    chmod -R u+w "./$${A2X_OUTDIR}" || true ; \
    A2X_VERBOSE="$(ASCIIDOC_VERBOSE)"; if [ "$(V)" = 1 ]; then A2X_VERBOSE="--verbose"; fi; \
}

# Note that documents with sub-pages (see LIBNUTCLIENT_*_DEPS above)
# may generate multiple files in one go... so we move "*" and hope
# for no required hidden files (or would have to `find` them all).
DOCBUILD_END = { \
    if test -n "$${A2X_OUTDIR}" && test "$${A2X_OUTDIR}" != '.' ; then \
        chmod -R u+w "./$${A2X_OUTDIR}" || true ; \
        test -d "$@" && rm -rf "$@" || true ; \
        for F in $(LINKMAN_INCLUDE_GENERATED) ; do \
            rm -f "./$${A2X_OUTDIR}/$$F" || true ; \
        done ; \
        mv -f "./$${A2X_OUTDIR}/$(@F)" ./ || exit ; \
        mv -f "./$${A2X_OUTDIR}/"*.* ./ 2>/dev/null || true ; \
        rm -rf "./$${A2X_OUTDIR}" ; \
    fi ; \
}

### Call the prep step consistently to create symlinks (out-of-tree)
### or just touch-files for peace of mind (in-tree builds). Then we
### use these path names (truncated "-prepped") now surely located
### in the builddir as the sources for rendered docs.
*.txt-prepped: $(abs_top_builddir)/docs/man/.prep-src-docs

#.txt.txt-prepped: $(abs_top_builddir)/docs/man/.prep-src-docs

### Regarding absolute paths in attributes below: by default asciidoc
### resolves include paths relative to the main document, so while we
### see the relative builddir as "." in the makefile, for included
### data it would mean the source directory where the .txt resides.
### Note: `(top_)srcdir` and `(top_)builddir` must end with a path
### separator, or be empty -- so in all cases letting the resulting
### string resolve meaningfully in the filesystem during docs build.
.txt-prepped.html:
	@A2X_OUTDIR="tmp/man-html.$(@F).$$$$" ; \
	 echo "  DOC-MAN-HTML Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(ASCIIDOC) --backend=xhtml11 $${A2X_VERBOSE} \
		--attribute=localdate="`TZ=UTC date +%Y-%m-%d`" \
		--attribute=localtime="`TZ=UTC date +%H:%M:%S`" \
		--attribute=nutversion="@PACKAGE_VERSION@" \
		--attribute=srcdir="$(abs_srcdir)/" \
		--attribute=builddir="$(abs_builddir)/" \
		--attribute=top_srcdir="$(abs_top_srcdir)/" \
		--attribute=top_builddir="$(abs_top_builddir)/" \
		--attribute=MAN_SECTION_API_BASE='@MAN_SECTION_API_BASE@' \
		--attribute=MAN_SECTION_CFG_BASE='@MAN_SECTION_CFG_BASE@' \
		--attribute=MAN_SECTION_CMD_SYS_BASE='@MAN_SECTION_CMD_SYS_BASE@' \
		--attribute=MAN_SECTION_CMD_USR_BASE='@MAN_SECTION_CMD_USR_BASE@' \
		--attribute=NUT_WEBSITE_BASE='@NUT_WEBSITE_BASE@' \
		-o "$${A2X_OUTDIR}/$(@F)" '$<' || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

### Prior to Asciidoc ~8.6.8, the --destination-dir flag didn't seem to affect the location of the intermediate .xml file.
### This parameter is currently required; see docs/Makefile.am for more detail.
A2X_MANPAGE_OPTS = --doctype manpage --format manpage $${A2X_VERBOSE} \
	--xsltproc-opts="--nonet" \
	--attribute=mansource="Network UPS Tools" \
	--attribute=manversion="@PACKAGE_VERSION@" \
	--attribute=manmanual="NUT Manual" \
	--attribute=srcdir="$(abs_srcdir)/" \
	--attribute=builddir="$(abs_builddir)/" \
	--attribute=top_srcdir="$(abs_top_srcdir)/" \
	--attribute=top_builddir="$(abs_top_builddir)/" \
	--destination-dir="$${A2X_OUTDIR}"

.txt-prepped.$(MAN_SECTION_CMD_USR):
	@A2X_OUTDIR="tmp/man$(MAN_SECTION_CMD_USR).$(@F).$$$$" ; \
	 echo "  DOC-MAN  Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(A2X) $(A2X_MANPAGE_OPTS) '$<' || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

.txt-prepped.$(MAN_SECTION_API):
	@A2X_OUTDIR="tmp/man$(MAN_SECTION_API).$(@F).$$$$" ; \
	 echo "  DOC-MAN  Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(A2X) $(A2X_MANPAGE_OPTS) '$<' || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

.txt-prepped.$(MAN_SECTION_CFG):
	@A2X_OUTDIR="tmp/man$(MAN_SECTION_CFG).$(@F).$$$$" ; \
	 echo "  DOC-MAN  Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(A2X) $(A2X_MANPAGE_OPTS) '$<' || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

.txt-prepped.$(MAN_SECTION_CMD_SYS):
	@A2X_OUTDIR="tmp/man$(MAN_SECTION_CMD_SYS).$(@F).$$$$" ; \
	 echo "  DOC-MAN  Generating $@"; \
	 $(DOCBUILD_BEGIN) ; RES=0; \
	 $(A2X) $(A2X_MANPAGE_OPTS) '$<' || RES=$$? ; \
	 $(DOCBUILD_END) ; exit $$RES

else !HAVE_ASCIIDOC

.txt.html:
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.$(MAN_SECTION_CMD_USR):
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.$(MAN_SECTION_API):
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.$(MAN_SECTION_CFG):
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

.txt.$(MAN_SECTION_CMD_SYS):
	@if [ -r "$@" ] $(SRC_PREBUILT_CLAUSE); then \
		echo "Not (re)building $@ manual page, since 'asciidoc', 'xmllint' or 'xsltproc' were not found." ; \
	else \
		echo "Could not find prebuilt $@ manual page." ; \
		echo "If you are building from Git, do you have all of the asciidoc/a2x tools installed?"; \
		exit 1; \
	fi

endif !HAVE_ASCIIDOC

# NOTE: Due to portability, we do not use a GNU percent-wildcard extension.
# We also have to export some variables that may be tainted by relative
# paths when parsing the other makefile (e.g. MKDIR_P that may be defined
# via expanded $(top_builddir)/install-sh):
#%-spellchecked: % Makefile.am $(top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
#	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE='$<' SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

# NOTE: Portable suffix rules do not allow prerequisites, so we shim them here
# by a wildcard target in case the make implementation can put the two together.
*.txt-spellchecked: Makefile.am $(abs_top_srcdir)/docs/Makefile.am $(abs_srcdir)/$(NUT_SPELL_DICT)
.txt.txt-spellchecked:
	+$(MAKE) -s -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC_ONE='$<' SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

spellcheck spellcheck-interactive spellcheck-sortdict:
	+$(MAKE) -f $(top_builddir)/docs/Makefile $(AM_MAKEFLAGS) MKDIR_P="$(MKDIR_P)" builddir="$(builddir)" srcdir="$(srcdir)" top_builddir="$(top_builddir)" top_srcdir="$(top_srcdir)" SPELLCHECK_SRC="$(SRC_ALL_PAGES)" SPELLCHECK_SRCDIR="$(srcdir)" SPELLCHECK_BUILDDIR="$(builddir)" $@

# When building out-of-tree, be sure to have all asciidoc resources
# under the same dir structure (tool limitation)
# NOTE: Not including *all* of EXTRA_DIST here, because we also "dist"
# the built man pages in the tarball for end-users without an asciidoc
# stack (causing a dependency loop)!
PREP_SRC = $(LINKMAN_INCLUDE_GENERATED) $(SRC_ALL_PAGES) asciidoc.conf

# NOTE: Some "make" implementations prefix a relative or absent path to
# the filenames in PREP_SRC, others (e.g. Sun make) prepend the absolute
# path to locate the sources, so we end up with bogus trees under docs/.
# Code below tries to detect and truncate this mess, including possible
# source texts located in/under parent dirs.
# We also handle man page links (section-aware) and titles for platforms
# where they differ from common defaults. NOTE: Page titles are a line
# with the title, followed by another with a sequence of "===" characters
# of same length, give or take one (or so). Currently we do not bother to
# adjust the second line, as we expect minor length changes like "8"=>"1m".
# NOTE: MKDIR_P may be defined via expanded $(top_builddir)/install-sh
# so should be run from $(abs_builddir) to be safe, as we jump around
# the build workspace
# NOTE: For DOCS_NO_MAN case we expect that the top-level Makefile caused
# parallel runs of several directories, so we can reasonably wait some
# time for the target file to just appear.
MAINTAINER_DOCS_PREP_MAN_DELAY = 60
prep-src-docs: $(abs_top_builddir)/docs/man/.prep-src-docs
$(abs_top_builddir)/docs/man/.prep-src-docs: $(PREP_SRC) Makefile
	@cd "$(@D)" || exit ; \
	 if [ x"$(DOCS_NO_MAN)" = xtrue ] ; then \
	    if [ "$(MAINTAINER_DOCS_PREP_MAN_DELAY)" -gt 0 ] 2>/dev/null ; then \
	        echo "  DOCS_NO_MAN     BLOCK: $@ called in docs/man/Makefile : waiting for other thread to complete this target" ; \
	        W=0 ; while [ "$${W}" -lt "$(MAINTAINER_DOCS_PREP_MAN_DELAY)" ] && ( [ \! -e '$@' ] || [ x"`find $(PREP_SRC) \! -newer '$@' 2>/dev/null`" = x ] ) ; do sleep 1 ; W="`expr $$W + 1`"; done ; \
	        echo "  DOCS_NO_MAN     SKIP: $@ called in docs/man/Makefile : waited $$W seconds" ; \
	    fi ; \
	    exit 0 ; \
	 fi ; \
	 linkroot="$(abs_builddir)" ; \
	 MAN_SECTIONS_DEFAULT=false ; \
	 if [ x"$(MAN_SECTION_API)$(MAN_SECTION_CFG)$(MAN_SECTION_CMD_SYS)$(MAN_SECTION_CMD_USR)" = x3581 ] ; then \
	    MAN_SECTIONS_DEFAULT=true ; \
	 fi ; \
	 if test x"$(abs_srcdir)" = x"$(abs_builddir)" ; then \
	    COUNT=0; \
	    for F in $(PREP_SRC) ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed "s#^$(abs_top_srcdir)/*#./#"`"; \
	                if test x"$${linkroot}" = x"$(abs_builddir)" ; then \
	                    linkroot="$(abs_top_builddir)" ; \
	                    cd "$(abs_top_builddir)" ; \
	                fi ;; \
	        esac ; \
	        if $$MAN_SECTIONS_DEFAULT ; then \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 @NUT_WEBSITE_BASE@/,' ; \
	        else \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 @NUT_WEBSITE_BASE@/,' \
	                -e 's,\(linkman:[^ []*\[\)3\],\1$(MAN_SECTION_API)],g' \
	                -e 's,\(linkman:[^ []*\[\)5\],\1$(MAN_SECTION_CFG)],g' \
	                -e 's,\(linkman:[^ []*\[\)8\],\1$(MAN_SECTION_CMD_SYS)],g' \
	                -e 's,\(linkman:[^ []*\[\)1\],\1$(MAN_SECTION_CMD_USR)],g' \
	                -e '1 s,\(^.*(\)3)$$,\1$(MAN_SECTION_API)),g' \
	                -e '1 s,\(^.*(\)5)$$,\1$(MAN_SECTION_CFG)),g' \
	                -e '1 s,\(^.*(\)8)$$,\1$(MAN_SECTION_CMD_SYS)),g' \
	                -e '1 s,\(^.*(\)1)$$,\1$(MAN_SECTION_CMD_USR)),g' ; \
	        fi < "$${F}" > "$${F}-prepped" || exit ; \
	        COUNT="`expr $$COUNT + 1`" ; \
	    done ; \
	 else \
	    COUNT=30 ; \
	    touch "$@.$$$$" ; \
	    while test -e "$@.working" -a "$$COUNT" -gt 0 ; do sleep 1; COUNT="`expr $$COUNT - 1`"; done ; \
	    touch "$@.working" ; \
	    if test -n "`find "$@" -newer "$@.$$$$" 2>/dev/null`" ; then \
	        rm -f "$@.$$$$" "$@.working" ; \
	        exit 0; \
	    fi ; \
	    rm -f "$@.$$$$" ; \
	    COUNT=0; \
	    linksrcroot="$(abs_srcdir)" ; \
	    for F in `echo $(PREP_SRC) | tr ' ' '\n' | sort -n | uniq` ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed "s#^$(abs_top_srcdir)/*#./#"`"; \
	                if test x"$${linkroot}" = x"$(abs_builddir)" ; then \
	                    linkroot="$(abs_top_builddir)" ; \
	                    linksrcroot="$(abs_top_srcdir)" ; \
	                    cd "$(abs_top_builddir)" ; \
	                fi ;; \
	            "$(srcdir)"/*) F="`echo "$$F" | sed 's#^$(srcdir)/*#./#'`" ;; \
	            */*) ;; \
	            *) \
	                linkroot="$(abs_builddir)" ; \
	                linksrcroot="$(abs_srcdir)" ; \
	                cd "$(abs_top_builddir)" ;; \
	        esac ; \
	        D="`dirname "$$F"`" ; \
	        (cd '$(abs_builddir)' && $(MKDIR_P) "$${linkroot}/$$D") || { rm -f "$@.working" ; exit 1 ; } ; \
	        if ! test -e "$${linkroot}/$$F" && test -s "$${linksrcroot}/$$F" ; then \
	            echo "  LN     '$${linksrcroot}/$$F' => '$${linkroot}/$$F' (PWD = '`pwd`')" >&2 ; \
	            ln -fs "$${linksrcroot}/$$F" "$${linkroot}/$$F" || { rm -f "$@.working" ; exit 1 ; } ; \
	            COUNT="`expr $$COUNT + 1`" ; \
	        fi ; \
	        case "$$F" in \
	            *.txt|*.adoc) IS_TEXT=true ;; \
	            *.*) IS_TEXT=false ;; \
	            *) IS_TEXT=true ;; \
	        esac; \
	        if $$IS_TEXT ; then \
	            grep -w linkman "$${linkroot}/$${F}" > /dev/null || IS_TEXT=false ; \
	        fi ; \
	        if $$MAN_SECTIONS_DEFAULT || ! $$IS_TEXT ; then \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 @NUT_WEBSITE_BASE@/,' ; \
	        else \
	            sed \
	                -e 's,\(home page:\) https://www.networkupstools.org/*$$,\1 @NUT_WEBSITE_BASE@/,' \
	                -e 's,\(linkman:[^ []*\[\)3\],\1$(MAN_SECTION_API)],g' \
	                -e 's,\(linkman:[^ []*\[\)5\],\1$(MAN_SECTION_CFG)],g' \
	                -e 's,\(linkman:[^ []*\[\)8\],\1$(MAN_SECTION_CMD_SYS)],g' \
	                -e 's,\(linkman:[^ []*\[\)1\],\1$(MAN_SECTION_CMD_USR)],g' \
	                -e '1 s,\(^.*(\)3)$$,\1$(MAN_SECTION_API)),g' \
	                -e '1 s,\(^.*(\)5)$$,\1$(MAN_SECTION_CFG)),g' \
	                -e '1 s,\(^.*(\)8)$$,\1$(MAN_SECTION_CMD_SYS)),g' \
	                -e '1 s,\(^.*(\)1)$$,\1$(MAN_SECTION_CMD_USR)),g' ; \
	        fi < "$${linkroot}/$${F}" > "$${linkroot}/$${F}-prepped" \
	        || { rm -f "$@.working" ; exit 1 ; } ; \
	        COUNT="`expr $$COUNT + 1`" ; \
	    done ; \
	 fi ; \
	 if test "$$COUNT" -gt 0 -o ! -e "$@" ; then touch "$@" ; fi
	@rm -f "$@.working"

MAINTAINERCLEANFILES = Makefile.in .dirstamp

clean-local:
	$(AM_V_at)rm -rf tmp
	$(AM_V_at)for F in $(PREP_SRC) ; do \
	        case "$$F" in \
	            /*) F="`echo "$$F" | sed "s#^$(abs_top_srcdir)/*#./#"`"; cd "$(abs_top_builddir)" ;; \
	        esac ; \
	        if test x"$(abs_srcdir)" != x"$(abs_builddir)" ; then \
	            if test -L "$$F" || test -h "$$F" ; then \
	                rm -f "$$F" ; \
	            fi ; \
	        fi ; \
	        rm -f "$${F}-prepped" ; \
	    done ; \
	    rm -f "$(abs_top_builddir)/docs/man/.prep-src-docs"*
