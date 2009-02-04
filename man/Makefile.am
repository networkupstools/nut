# Network UPS Tools: man

LIB_PAGES = upsclient.3 upscli_connect.3 upscli_disconnect.3	\
 upscli_readline.3 upscli_sendline.3 upscli_splitaddr.3		\
 upscli_splitname.3 upscli_strerror.3 upscli_fd.3 upscli_get.3	\
 upscli_list_next.3 upscli_list_start.3 upscli_ssl.3		\
 upscli_upserror.3
if WITH_DEV
if !WITH_PKG_CONFIG
 LIB_PAGES += libupsclient-config.1
endif
endif

CONF_PAGES = ups.conf.5 upssched.conf.5 upsd.conf.5 upsmon.conf.5	\
 upsd.users.5
CLIENT_PAGES = upsc.8 upsrw.8 upscmd.8 upslog.8 upssched.8	\
 upsmon.8 upsd.8 nutupsdrv.8 upsdrvctl.8

CGI_PAGES = hosts.conf.5 upsset.conf.5 upsstats.html.5	\
 upsset.cgi.8 upsstats.cgi.8 upsimage.cgi.8

SERIAL_PAGES = \
	apcsmart.8	\
	bcmxcp.8 	\
	belkin.8 	\
	belkinunv.8	\
	bestuferrups.8	\
	bestups.8 	\
	bestfcom.8	\
	blazer.8	\
	cyberpower.8	\
	dummy-ups.8	\
	etapro.8	\
	everups.8	\
	gamatronic.8	\
	genericups.8	\
	isbmex.8	\
	liebert.8	\
	masterguard.8	\
	metasys.8	\
	mge-shut.8	\
	mge-utalk.8	\
	oneac.8		\
	microdowell.8	\
	optiups.8	\
	powercom.8 	\
	powerpanel.8	\
	megatec.8	\
	rhino.8		\
	safenet.8	\
	solis.8		\
	tripplite.8	\
	tripplitesu.8	\
	upscode2.8	\
	victronups.8

SNMP_PAGES = snmp-ups.8

USB_LIBUSB_PAGES = usbhid-ups.8 bcmxcp_usb.8 tripplite_usb.8 megatec_usb.8 richcomm_usb.8
NETXML_PAGES = netxml-ups.8
POWERMAN_PAGES = powerman-pdu.8

# decide which pages to install
man_MANS = $(CONF_PAGES) $(CLIENT_PAGES)

if WITH_CGI
 man_MANS += $(CGI_PAGES)
endif
if WITH_DEV
 man_MANS += $(LIB_PAGES)
endif

if SOME_DRIVERS
 man_MANS += $(DRIVER_MAN_LIST)
else
if WITH_SERIAL
  man_MANS += $(SERIAL_PAGES)
endif
if WITH_SNMP
  man_MANS += $(SNMP_PAGES)
endif
if WITH_USB
   man_MANS += $(USB_LIBUSB_PAGES)
endif
if WITH_NEONXML
   man_MANS += $(NETXML_PAGES)
endif
if WITH_LIBPOWERMAN
   man_MANS += $(POWERMAN_PAGES)
endif
endif

# distribute everything, even that which is not installed by default
EXTRA_DIST = $(LIB_PAGES) $(CONF_PAGES) $(CLIENT_PAGES)		\
 $(CGI_PAGES) $(SERIAL_PAGES) $(SNMP_PAGES) $(USB_LIBUSB_PAGES)	\
 $(NETXML_PAGES) $(POWERMAN_PAGES) libupsclient-config.1 skel.8
