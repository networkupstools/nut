# Network UPS Tools: drivers

# by default, link programs in this directory with libcommon.la
# (libtool version of the static lib, in order to access LTLIBOBJS)
#FIXME: SERLIBS is only useful for LDADD_DRIVERS_SERIAL not for LDADD_COMMON
LDADD_COMMON = ../common/libcommon.la ../common/libparseconf.la
LDADD_DRIVERS = $(LDADD_COMMON) main.o dstate.o
LDADD_DRIVERS_SERIAL = $(LDADD_DRIVERS) $(SERLIBS) serial.o

# most targets are drivers, so make this the default
LDADD = $(LDADD_DRIVERS_SERIAL)
# Avoid per-target CFLAGS, because this will prevent re-use of object
# files. In any case, CFLAGS are only -I options, so there is no harm,
# but only add them if we really use the target.
AM_CFLAGS = -I$(top_srcdir)/include
if WITH_USB
  AM_CFLAGS += $(LIBUSB_CFLAGS)
endif
if WITH_NEON
  AM_CFLAGS += $(LIBNEON_CFLAGS)
endif
if WITH_LIBPOWERMAN
  AM_CFLAGS += $(LIBPOWERMAN_CFLAGS)
endif
if WITH_IPMI
  AM_CFLAGS += $(LIBIPMI_CFLAGS)
endif
if WITH_MODBUS
  AM_CFLAGS += $(LIBMODBUS_CFLAGS)
endif

SERIAL_DRIVERLIST = al175 bcmxcp belkin belkinunv bestfcom	\
 bestfortress bestuferrups bestups dummy-ups etapro everups 	\
 gamatronic genericups isbmex liebert liebert-esp2 masterguard metasys	\
 mge-utalk microdowell mge-shut oneac optiups powercom rhino 	\
 safenet nutdrv_siemens-sitop skel solis tripplite tripplitesu upscode2 victronups powerpanel \
 blazer_ser clone clone-outlet ivtscd apcsmart apcsmart-old apcupsd-ups riello_ser
SNMP_DRIVERLIST = snmp-ups
USB_LIBUSB_DRIVERLIST = usbhid-ups bcmxcp_usb tripplite_usb \
 blazer_usb richcomm_usb riello_usb \
 nutdrv_atcl_usb
USB_DRIVERLIST = $(USB_LIBUSB_DRIVERLIST)
SERIAL_USB_DRIVERLIST = \
	nutdrv_qx
NEONXML_DRIVERLIST = netxml-ups
MACOSX_DRIVERLIST = macosx-ups
MODBUS_DRIVERLIST = phoenixcontact_modbus
LINUX_I2C_DRIVERLIST = asem pijuice

# distribute all drivers, even ones that are not built by default
EXTRA_PROGRAMS = $(SERIAL_DRIVERLIST) $(SNMP_DRIVERLIST) $(USB_DRIVERLIST) $(SERIAL_USB_DRIVERLIST) $(NEONXML_DRIVERLIST) $(MACOSX_DRIVERLIST)

# construct the list of drivers to build
if SOME_DRIVERS
 driverexec_PROGRAMS = $(DRIVER_BUILD_LIST)
else
 driverexec_PROGRAMS =
if WITH_SERIAL
  driverexec_PROGRAMS += $(SERIAL_DRIVERLIST) $(SERIAL_USB_DRIVERLIST)
else
if WITH_USB
  driverexec_PROGRAMS += $(SERIAL_USB_DRIVERLIST)
endif
endif
if WITH_SNMP
  driverexec_PROGRAMS += $(SNMP_DRIVERLIST)
endif
if WITH_USB
   driverexec_PROGRAMS += $(USB_LIBUSB_DRIVERLIST)
endif
if WITH_NEON
   driverexec_PROGRAMS += $(NEONXML_DRIVERLIST)
endif
if WITH_LIBPOWERMAN
   driverexec_PROGRAMS += powerman-pdu
endif
if WITH_IPMI
   driverexec_PROGRAMS += nut-ipmipsu
endif
if WITH_MACOSX
   driverexec_PROGRAMS += $(MACOSX_DRIVERLIST)
endif
if WITH_LINUX_I2C
   driverexec_PROGRAMS += $(LINUX_I2C_DRIVERLIST)
endif
if WITH_MODBUS
  driverexec_PROGRAMS += $(MODBUS_DRIVERLIST)
endif
else
   driverexec_PROGRAMS += skel
endif

# always build upsdrvctl
sbin_PROGRAMS = upsdrvctl

# ==========================================================================
# Driver build details

# upsdrvctl: the all-singing all-dancing driver control program
upsdrvctl_SOURCES = upsdrvctl.c
upsdrvctl_LDADD = $(LDADD_COMMON)

# serial drivers: all of them use standard LDADD and CFLAGS
al175_SOURCES = al175.c
apcsmart_SOURCES = apcsmart.c apcsmart_tabs.c
apcsmart_old_SOURCES = apcsmart-old.c
bcmxcp_SOURCES = bcmxcp.c bcmxcp_ser.c
bcmxcp_LDADD = $(LDADD) -lm
belkin_SOURCES = belkin.c
belkinunv_SOURCES = belkinunv.c
bestfcom_SOURCES = bestfcom.c
bestuferrups_SOURCES = bestuferrups.c
bestups_SOURCES = bestups.c
blazer_ser_SOURCES = blazer.c blazer_ser.c
blazer_ser_LDADD = $(LDADD) -lm
etapro_SOURCES = etapro.c
everups_SOURCES = everups.c
gamatronic_SOURCES = gamatronic.c
genericups_SOURCES = genericups.c
isbmex_SOURCES = isbmex.c
isbmex_LDADD = $(LDADD) -lm
ivtscd_SOURCES = ivtscd.c
liebert_SOURCES = liebert.c
liebert_esp2_SOURCES = liebert-esp2.c
masterguard_SOURCES = masterguard.c
metasys_SOURCES = metasys.c
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
nutdrv_siemens_sitop_SOURCES = nutdrv_siemens_sitop.c
solis_SOURCES = solis.c
solis_LDADD = $(LDADD) -lm
tripplite_SOURCES = tripplite.c
tripplite_LDADD = $(LDADD) -lm
tripplitesu_SOURCES = tripplitesu.c
upscode2_SOURCES = upscode2.c
upscode2_LDADD = $(LDADD) -lm
victronups_SOURCES = victronups.c
riello_ser_SOURCES = riello.c riello_ser.c
riello_ser_LDADD = $(LDADD) -lm

# non-serial drivers: these use custom LDADD and/or CFLAGS

# dummy
dummy_ups_SOURCES = dummy-ups.c
dummy_ups_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/clients
dummy_ups_LDADD = $(LDADD_DRIVERS) ../clients/libupsclient.la
if WITH_SSL
  dummy_ups_CFLAGS += $(LIBSSL_CFLAGS)
  dummy_ups_LDADD += $(LIBSSL_LIBS)
endif

# Clone drivers
clone_SOURCES = clone.c
clone_outlet_SOURCES = clone-outlet.c

# apcupsd client driver
apcupsd_ups_SOURCES = apcupsd-ups.c
apcupsd_ups_CFLAGS = $(AM_CFLAGS)
apcupsd_ups_LDADD = $(LDADD_DRIVERS)

# sample skeleton driver

skel_SOURCES = skel.c
skel_LDADD = $(LDADD_DRIVERS)

# USB
if WITH_LIBUSB_0_1
LIBUSB_IMPL = libusb0.c
endif
if WITH_LIBUSB_1_0
LIBUSB_IMPL = libusb1.c
endif
USBHID_UPS_SUBDRIVERS = apc-hid.c belkin-hid.c cps-hid.c explore-hid.c \
 liebert-hid.c mge-hid.c powercom-hid.c tripplite-hid.c idowell-hid.c \
 openups-hid.c
usbhid_ups_SOURCES = usbhid-ups.c libhid.c $(LIBUSB_IMPL) hidparser.c	\
 usb-common.c $(USBHID_UPS_SUBDRIVERS)
usbhid_ups_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LIBS) -lm

tripplite_usb_SOURCES = tripplite_usb.c $(LIBUSB_IMPL) usb-common.c
tripplite_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LIBS) -lm

bcmxcp_usb_SOURCES = bcmxcp_usb.c bcmxcp.c usb-common.c
bcmxcp_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LIBS) -lm

blazer_usb_SOURCES = blazer.c blazer_usb.c $(LIBUSB_IMPL) usb-common.c
blazer_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LIBS) -lm

nutdrv_atcl_usb_SOURCES = nutdrv_atcl_usb.c usb-common.c
nutdrv_atcl_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LIBS)

richcomm_usb_SOURCES = richcomm_usb.c usb-common.c
richcomm_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LIBS)

riello_usb_SOURCES = riello.c riello_usb.c $(LIBUSB_IMPL) usb-common.c
riello_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LIBS) -lm


# HID-over-serial
mge_shut_SOURCES = usbhid-ups.c libshut.c libhid.c hidparser.c mge-hid.c
# per-target CFLAGS are necessary here
mge_shut_CFLAGS = $(AM_CFLAGS) -DSHUT_MODE
mge_shut_LDADD = $(LDADD) -lm

# SNMP
snmp_ups_SOURCES = snmp-ups.c apc-mib.c baytech-mib.c compaq-mib.c \
 eaton-pdu-genesis2-mib.c eaton-pdu-marlin-mib.c \
 eaton-pdu-pulizzi-mib.c eaton-pdu-revelation-mib.c \
 ietf-mib.c mge-mib.c netvision-mib.c powerware-mib.c raritan-pdu-mib.c \
 bestpower-mib.c cyberpower-mib.c delta_ups-mib.c xppc-mib.c huawei-mib.c \
 eaton-ats16-mib.c apc-ats-mib.c raritan-px2-mib.c eaton-ats30-mib.c \
 apc-pdu-mib.c emerson-avocent-pdu-mib.c hpe-pdu-mib.c
snmp_ups_CFLAGS = $(AM_CFLAGS)
snmp_ups_CFLAGS += $(LIBNETSNMP_CFLAGS)
snmp_ups_LDADD = $(LDADD_DRIVERS) $(LIBNETSNMP_LIBS)

# NEON XML/HTTP
netxml_ups_SOURCES = netxml-ups.c mge-xml.c
netxml_ups_LDADD = $(LDADD_DRIVERS) $(LIBNEON_LIBS)

# Powerman
powerman_pdu_SOURCES = powerman-pdu.c
powerman_pdu_LDADD = $(LDADD) $(LIBPOWERMAN_LIBS)

# IPMI PSU
nut_ipmipsu_SOURCES = nut-ipmipsu.c
if WITH_FREEIPMI
  nut_ipmipsu_SOURCES += nut-libfreeipmi.c
endif
nut_ipmipsu_LDADD = $(LDADD) $(LIBIPMI_LIBS)

# Mac OS X metadriver
macosx_ups_LDADD = $(LDADD_DRIVERS)
macosx_ups_LDFLAGS = $(LDFLAGS) -framework IOKit -framework CoreFoundation
macosx_ups_SOURCES = macosx-ups.c

# Modbus driver
phoenixcontact_modbus_SOURCES = phoenixcontact_modbus.c
phoenixcontact_modbus_LDADD = $(LDADD_DRIVERS) $(LIBMODBUS_LIBS)

# Linux I2C drivers
asem_LDADD = $(LDADD_DRIVERS)
asem_SOURCES = asem.c
pijuice_LDADD = $(LDADD_DRIVERS)
pijuice_SOURCES = pijuice.c

# nutdrv_qx USB/Serial
nutdrv_qx_SOURCES = nutdrv_qx.c
nutdrv_qx_LDADD = $(LDADD_DRIVERS) -lm
nutdrv_qx_CFLAGS = $(AM_CFLAGS)
if WITH_SERIAL
nutdrv_qx_CFLAGS += -DQX_SERIAL
nutdrv_qx_LDADD += $(SERLIBS) serial.o
endif
if WITH_USB
nutdrv_qx_CFLAGS += -DQX_USB
nutdrv_qx_SOURCES += $(LIBUSB_IMPL) usb-common.c
nutdrv_qx_LDADD += $(LIBUSB_LIBS)
endif
NUTDRV_QX_SUBDRIVERS = nutdrv_qx_bestups.c nutdrv_qx_blazer-common.c	\
 nutdrv_qx_mecer.c nutdrv_qx_megatec.c nutdrv_qx_megatec-old.c	\
 nutdrv_qx_mustek.c nutdrv_qx_q1.c nutdrv_qx_voltronic.c	\
 nutdrv_qx_voltronic-qs.c nutdrv_qx_voltronic-qs-hex.c nutdrv_qx_zinto.c
nutdrv_qx_SOURCES += $(NUTDRV_QX_SUBDRIVERS)

# ----------------------------------------------------------------------
# List of header files. The purpose of this list is not dependency
# tracking (which is automatic), but to ensure these files are
# distributed by "make dist".

dist_noinst_HEADERS = apc-mib.h apc-hid.h baytech-mib.h bcmxcp.h	\
 bcmxcp_io.h belkin.h belkin-hid.h bestpower-mib.h blazer.h cps-hid.h dstate.h \
 dummy-ups.h explore-hid.h gamatronic.h genericups.h	\
 hidparser.h hidtypes.h ietf-mib.h libhid.h libshut.h nut_libusb.h liebert-hid.h	\
 main.h mge-hid.h mge-mib.h mge-utalk.h		\
 mge-xml.h microdowell.h netvision-mib.h netxml-ups.h nut-ipmi.h oneac.h		\
 powercom.h powerpanel.h powerp-bin.h powerp-txt.h powerware-mib.h raritan-pdu-mib.h	\
 safenet.h serial.h snmp-ups.h solis.h tripplite.h tripplite-hid.h 			\
 upshandler.h usb-common.h usbhid-ups.h powercom-hid.h compaq-mib.h idowell-hid.h \
 apcsmart.h apcsmart_tabs.h apcsmart-old.h apcupsd-ups.h cyberpower-mib.h riello.h openups-hid.h \
 delta_ups-mib.h nutdrv_qx.h nutdrv_qx_bestups.h nutdrv_qx_blazer-common.h nutdrv_qx_mecer.h	\
 nutdrv_qx_megatec.h nutdrv_qx_megatec-old.h nutdrv_qx_mustek.h nutdrv_qx_q1.h	\
 nutdrv_qx_voltronic.h nutdrv_qx_voltronic-qs.h nutdrv_qx_voltronic-qs-hex.h nutdrv_qx_zinto.h \
 xppc-mib.h huawei-mib.h eaton-ats16-mib.h apc-ats-mib.h raritan-px2-mib.h eaton-ats30-mib.h \
 apc-pdu-mib.h eaton-pdu-genesis2-mib.h eaton-pdu-marlin-mib.h \
 eaton-pdu-pulizzi-mib.h eaton-pdu-revelation-mib.h emerson-avocent-pdu-mib.h \
 hpe-pdu-mib.h

# Define a dummy library so that Automake builds rules for the
# corresponding object files.  This library is not actually built,
EXTRA_LIBRARIES = libdummy.a
libdummy_a_SOURCES = main.c dstate.c serial.c
