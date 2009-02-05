# Network UPS Tools: drivers

# by default, link programs in this directory with libcommon.a
#FIXME: SERLIBS is only useful for LDADD_DRIVERS_SERIAL not for LDADD_COMMON
LDADD_COMMON = ../common/libcommon.a ../common/libparseconf.la
LDADD_DRIVERS = $(LDADD_COMMON) main.o dstate.o
LDADD_HAL_DRIVERS = $(LDADD_COMMON) libnuthalmain.a
LDADD_DRIVERS_SERIAL = $(LDADD_DRIVERS) $(SERLIBS) serial.o

# most targets are drivers, so make this the default
LDADD = $(LDADD_DRIVERS_SERIAL)
# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include
if WITH_HAL
  AM_CFLAGS += $(LIBHAL_CFLAGS)
endif
if WITH_USB
  AM_CFLAGS += $(LIBUSB_CFLAGS)
endif
if WITH_SNMP
  AM_CFLAGS += $(LIBNETSNMP_CFLAGS)
endif
if WITH_SNMP
  AM_CFLAGS += $(LIBSSL_CFLAGS)
endif
if WITH_NEONXML
  AM_CFLAGS += $(LIBNEON_CFLAGS)
endif
if WITH_LIBPOWERMAN
  AM_CFLAGS += $(LIBPOWERMAN_CFLAGS)
endif

SERIAL_DRIVERLIST = apcsmart bcmxcp belkin belkinunv bestfcom	\
 bestuferrups bestups cyberpower dummy-ups etapro everups 	\
 gamatronic genericups isbmex liebert masterguard megatec metasys 	\
 mge-shut mge-utalk microdowell newmge-shut oneac optiups powercom rhino 	\
 safenet skel solis tripplite tripplitesu upscode2 victronups powerpanel \
 blazer_ser
SNMP_DRIVERLIST = snmp-ups
USB_LIBUSB_DRIVERLIST = usbhid-ups bcmxcp_usb tripplite_usb megatec_usb \
 blazer_usb richcomm_usb
USB_DRIVERLIST = $(USB_LIBUSB_DRIVERLIST)
HAL_DRIVERLIST = hald-addon-usbhid-ups hald-addon-bcmxcp_usb \
 hald-addon-tripplite_usb hald-addon-megatec_usb
NEONXML_DRIVERLIST = netxml-ups

# distribute all drivers, even ones that are not built by default
EXTRA_PROGRAMS = $(SERIAL_DRIVERLIST) $(SNMP_DRIVERLIST) $(USB_DRIVERLIST) $(NEONXML_DRIVERLIST)

# construct the list of drivers to build
if SOME_DRIVERS
 driverexec_PROGRAMS = $(DRIVER_BUILD_LIST)
else
 driverexec_PROGRAMS =
if WITH_SERIAL
  driverexec_PROGRAMS += $(SERIAL_DRIVERLIST)
endif
if WITH_SNMP
  driverexec_PROGRAMS += $(SNMP_DRIVERLIST)
endif
if WITH_USB
   driverexec_PROGRAMS += $(USB_LIBUSB_DRIVERLIST)
endif
if WITH_HAL
   halexecdir = $(HAL_CALLOUTS_PATH)
   halexec_PROGRAMS = $(HAL_DRIVERLIST)
endif
if WITH_NEONXML
   driverexec_PROGRAMS += $(NEONXML_DRIVERLIST)
endif
if WITH_LIBPOWERMAN
   driverexec_PROGRAMS += powerman-pdu
endif
else
   driverexec_PROGRAMS += skel
endif

# always build upsdrvctl
driverexec_PROGRAMS += upsdrvctl

# ==========================================================================
# Driver build details

# upsdrvctl: the all-singing all-dancing driver control program
upsdrvctl_SOURCES = upsdrvctl.c
upsdrvctl_LDADD = $(LDADD_COMMON)

# serial drivers: all of them use standard LDADD and CFLAGS
apcsmart_SOURCES = apcsmart.c
bcmxcp_SOURCES = bcmxcp.c bcmxcp_ser.c
belkin_SOURCES = belkin.c
belkinunv_SOURCES = belkinunv.c
bestfcom_SOURCES = bestfcom.c
bestuferrups_SOURCES = bestuferrups.c
bestups_SOURCES = bestups.c
blazer_ser_SOURCES = blazer.c blazer_ser.c
blazer_ser_LDADD = $(LDADD) -lm
cyberpower_SOURCES = cyberpower.c
etapro_SOURCES = etapro.c
everups_SOURCES = everups.c
gamatronic_SOURCES = gamatronic.c
genericups_SOURCES = genericups.c
isbmex_SOURCES = isbmex.c
isbmex_LDADD = $(LDADD) -lm
liebert_SOURCES = liebert.c
masterguard_SOURCES = masterguard.c
megatec_SOURCES = megatec.c
metasys_SOURCES = metasys.c
mge_shut_SOURCES = mge-shut.c hidparser.c 
mge_utalk_SOURCES = mge-utalk.c
microdowell_SOURCES = microdowell.c
oneac_SOURCES = oneac.c
optiups_SOURCES = optiups.c
powercom_SOURCES = powercom.c
powercom_LDADD = $(LDADD) -lm
powerpanel_SOURCES = powerpanel.c powerp-bin.c powerp-txt.c
powerpanel_LDADD = $(LDADD) -lm
rhino_SOURCES = rhino.c
rhino_LDADD = $(LDADD) -lm
safenet_SOURCES = safenet.c
solis_SOURCES = solis.c
tripplite_SOURCES = tripplite.c
tripplite_LDADD = $(LDADD) -lm
tripplitesu_SOURCES = tripplitesu.c
upscode2_SOURCES = upscode2.c
upscode2_LDADD = $(LDADD) -lm
victronups_SOURCES = victronups.c

# non-serial drivers: these use custom LDADD and/or CFLAGS

# dummy
dummy_ups_SOURCES = dummy-ups.c
dummy_ups_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/clients
dummy_ups_LDADD = $(LDADD_DRIVERS) ../clients/libupsclient.la 
if WITH_SSL
  dummy_ups_CFLAGS += $(LIBSSL_CFLAGS)
  dummy_ups_LDADD += $(LIBSSL_LDFLAGS)
endif

# sample skeleton driver

skel_SOURCES = skel.c
skel_LDADD = $(LDADD_DRIVERS)

# USB
USBHID_UPS_SUBDRIVERS = apc-hid.c belkin-hid.c cps-hid.c explore-hid.c \
 liebert-hid.c mge-hid.c tripplite-hid.c
usbhid_ups_SOURCES = usbhid-ups.c libhid.c libusb.c hidparser.c	\
 usb-common.c $(USBHID_UPS_SUBDRIVERS)
usbhid_ups_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS)
if WITH_SSL
  usbhid_ups_LDADD += $(LIBSSL_LDFLAGS)
endif

tripplite_usb_SOURCES = tripplite_usb.c libusb.c usb-common.c
tripplite_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS) -lm

bcmxcp_usb_SOURCES = bcmxcp_usb.c bcmxcp.c nut_usb.c usb-common.c
bcmxcp_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS)

megatec_usb_SOURCES = megatec.c megatec_usb.c libusb.c usb-common.c
megatec_usb_CFLAGS = $(AM_CFLAGS) -DMEGATEC_SUBDRV
megatec_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS)

blazer_usb_SOURCES = blazer.c blazer_usb.c libusb.c usb-common.c
blazer_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS) -lm

richcomm_usb_SOURCES = richcomm_usb.c usb-common.c
richcomm_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS)

# HID-over-serial
newmge_shut_SOURCES = usbhid-ups.c libshut.c libhid.c hidparser.c mge-hid.c
# per-target CFLAGS are necessary here
newmge_shut_CFLAGS = $(AM_CFLAGS) -DSHUT_MODE
newmge_shut_LDADD = $(LDADD)

# SNMP
snmp_ups_SOURCES = snmp-ups.c
snmp_ups_LDADD = $(LDADD_DRIVERS) $(LIBNETSNMP_LDFLAGS)

# HAL
hald_addon_usbhid_ups_SOURCES = usbhid-ups.c libhid.c libusb.c hidparser.c	\
 $(USBHID_UPS_SUBDRIVERS)
hald_addon_usbhid_ups_LDADD = $(LDADD_HAL_DRIVERS) $(LIBUSB_LDFLAGS) $(LIBHAL_LDFLAGS) $(LIBSSL_LDFLAGS)

hald_addon_tripplite_usb_SOURCES = tripplite_usb.c libusb.c
hald_addon_tripplite_usb_LDADD = $(LDADD_HAL_DRIVERS) $(LIBUSB_LDFLAGS) $(LIBHAL_LDFLAGS) -lm

hald_addon_bcmxcp_usb_SOURCES = bcmxcp_usb.c bcmxcp.c nut_usb.c
hald_addon_bcmxcp_usb_LDADD = $(LDADD_HAL_DRIVERS) $(LIBUSB_LDFLAGS) $(LIBHAL_LDFLAGS)

hald_addon_megatec_usb_SOURCES = megatec.c megatec_usb.c libusb.c
hald_addon_megatec_usb_CFLAGS = $(AM_CFLAGS) -DMEGATEC_SUBDRV
hald_addon_megatec_usb_LDADD = $(LDADD_HAL_DRIVERS) $(LIBUSB_LDFLAGS) $(LIBHAL_LDFLAGS)

# NEON XML/HTTP
netxml_ups_SOURCES = netxml-ups.c mge-xml.c
netxml_ups_LDADD = $(LDADD_DRIVERS) $(LIBNEON_LDFLAGS)

# Powerman
powerman_pdu_SOURCES = powerman-pdu.c
powerman_pdu_LDADD = $(LDADD) $(LIBPOWERMAN_LDFLAGS)

# ----------------------------------------------------------------------
# List of header files. The purpose of this list is not dependency
# tracking (which is automatic), but to ensure these files are
# distributed by "make dist".

dist_noinst_HEADERS = apccmib.h apc-hid.h apcsmart.h bcmxcp.h bcmxcp_io.h belkin.h	\
 belkin-hid.h blazer.h cps-hid.h dstate.h dstate-hal.h dummy-ups.h eaton-aphel-mib.h	\
 explore-hid.h gamatronic.h genericups.h hidparser.h hidtypes.h ietfmib.h libhid.h	\
 libshut.h libusb.h liebert-hid.h main.h main-hal.h megatec.h mge-hid.h mgemib.h	\
 mge-shut.h mge-utalk.h mge-xml.h microdowell.h netvisionmib.h netxml-ups.h nut_usb.h oneac.h	\
 powercom.h powerpanel.h powerp-bin.h powerp-txt.h pwmib.h raritan-mib.h	\
 safenet.h serial.h snmp-ups.h solis.h tripplite.h tripplite-hid.h upshandler.h		\
 usb-common.h usbhid-ups.h

# Define a dummy library so that Automake builds rules for the
# corresponding object files.  This library is not actually built,
EXTRA_LIBRARIES = libdummy.a
libdummy_a_SOURCES = main.c dstate.c serial.c

# the nuthalmain library combines the code for main-hal.c and
# dstate-hal.c. It is necessary for Automake-technical reasons,
# because per-object CFLAGS can only be specified for libraries, not
# for object files. This library is used during the build process, 
# and is not meant to be installed.

EXTRA_LIBRARIES += libnuthalmain.a
libnuthalmain_a_SOURCES = main-hal.c dstate-hal.c usb-common.c

MOSTLYCLEANFILES = libnuthalmain.a
