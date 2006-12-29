# Network UPS Tools: drivers

# by default, link programs in this directory with libcommon.a
#FIXME: SERLIBS is only useful for LDADD_DRIVERS_SERIAL not for LDADD_COMMON
LDADD_COMMON = ../common/libcommon.a
LDADD_DRIVERS = $(LDADD_COMMON) ../common/upsconf.o	\
 ../common/parseconf.o ../common/state.o main.o dstate.o
LDADD_HAL_DRIVERS = $(LDADD_COMMON) ../common/upsconf.o	\
 ../common/parseconf.o libnuthalmain.a
LDADD_DRIVERS_SERIAL = $(LDADD_DRIVERS) $(SERLIBS) serial.o

# most targets are drivers, so make this the default
LDADD = $(LDADD_DRIVERS_SERIAL)
AM_CFLAGS = -I$(top_srcdir)/include

SERIAL_DRIVERLIST = al175 apcsmart bcmxcp belkin belkinunv bestfcom	\
 bestuferrups bestups blazer cpsups cyberpower dummy-ups esupssmart	\
 etapro everups fentonups gamatronic genericups ippon isbmex liebert	\
 masterguard megatec metasys mge-shut mge-utalk mustek newmge-shut	\
 nitram oneac optiups powercom rhino safenet skel sms solis tripplite	\
 tripplitesu upscode2 victronups
SNMP_DRIVERLIST = snmp-ups
USB_LIBUSB_DRIVERLIST = newhidups bcmxcp_usb tripplite_usb
USB_HIDDEV_DRIVERLIST = hidups energizerups
USB_DRIVERLIST = $(USB_LIBUSB_DRIVERLIST) $(USB_HIDDEV_DRIVERLIST)
HAL_DRIVERLIST = hald-addon-usbhid-ups hald-addon-bcmxcp_usb hald-addon-tripplite_usb

# distribute all drivers, even ones that are not built by default
EXTRA_PROGRAMS = $(SERIAL_DRIVERLIST) $(SNMP_DRIVERLIST) $(USB_DRIVERLIST)

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
if HAVE_LIBUSB
   driverexec_PROGRAMS += $(USB_LIBUSB_DRIVERLIST)
endif
if HAVE_HIDDEV
   driverexec_PROGRAMS += $(USB_HIDDEV_DRIVERLIST)
endif
endif
if WITH_HAL
   driverexec_PROGRAMS += $(HAL_DRIVERLIST)
endif
endif

# always build upsdrvctl
driverexec_PROGRAMS += upsdrvctl

# ==========================================================================
# Driver build details

# upsdrvctl: the all-singing all-dancing driver control program
upsdrvctl_SOURCES = upsdrvctl.c
upsdrvctl_LDADD = $(LDADD_COMMON) ../common/upsconf.o ../common/parseconf.o

# serial drivers: all of them use standard LDADD and CFLAGS
al175_SOURCES = al175.c
apcsmart_SOURCES = apcsmart.c
bcmxcp_SOURCES = bcmxcp.c bcmxcp_ser.c
belkin_SOURCES = belkin.c
belkinunv_SOURCES = belkinunv.c
bestfcom_SOURCES = bestfcom.c
bestuferrups_SOURCES = bestuferrups.c
bestups_SOURCES = bestups.c
blazer_SOURCES = blazer.c
cpsups_SOURCES = cpsups.c
cyberpower_SOURCES = cyberpower.c
esupssmart_SOURCES = esupssmart.c
etapro_SOURCES = etapro.c
everups_SOURCES = everups.c
fentonups_SOURCES = fentonups.c
gamatronic_SOURCES = gamatronic.c
genericups_SOURCES = genericups.c
ippon_SOURCES = ippon.c
isbmex_SOURCES = isbmex.c
isbmex_LDADD = $(LDADD) -lm
liebert_SOURCES = liebert.c
masterguard_SOURCES = masterguard.c
megatec_SOURCES = megatec.c
metasys_SOURCES = metasys.c
mge_shut_SOURCES = mge-shut.c hidparser.c 
mge_utalk_SOURCES = mge-utalk.c
mustek_SOURCES = mustek.c
nitram_SOURCES = nitram.c
oneac_SOURCES = oneac.c
optiups_SOURCES = optiups.c
powercom_SOURCES = powercom.c
rhino_SOURCES = rhino.c
rhino_LDADD = $(LDADD) -lm
safenet_SOURCES = safenet.c
sms_SOURCES = sms.c
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
dummy_ups_LDADD = $(LDADD_DRIVERS)

skel_SOURCES = skel.c
skel_LDADD = $(LDADD_DRIVERS)

# USB
NEWHIDUPS_SUBDRIVERS = apc-hid.c belkin-hid.c explore-hid.c \
 mge-hid.c tripplite-hid.c
newhidups_SOURCES = newhidups.c libhid.c libusb.c hidparser.c	\
 $(NEWHIDUPS_SUBDRIVERS)
newhidups_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
newhidups_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS)

hidups_SOURCES = hidups.c
hidups_LDADD = $(LDADD_DRIVERS)

energizerups_SOURCES = energizerups.c
energizerups_LDADD = $(LDADD_DRIVERS)

tripplite_usb_SOURCES = tripplite_usb.c libhid.c libusb.c hidparser.c
tripplite_usb_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
tripplite_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS) -lm

bcmxcp_usb_SOURCES = bcmxcp_usb.c bcmxcp.c nut_usb.c
bcmxcp_usb_CFLAGS = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
bcmxcp_usb_LDADD = $(LDADD_DRIVERS) $(LIBUSB_LDFLAGS)

# HID-over-serial
newmge_shut_SOURCES = newhidups.c libshut.c libhid.c hidparser.c mge-hid.c
newmge_shut_CFLAGS = $(AM_CFLAGS) -DSHUT_MODE
newmge_shut_LDADD = $(LDADD)

# SNMP
snmp_ups_SOURCES = snmp-ups.c
snmp_ups_CFLAGS = $(AM_CFLAGS) $(LIBNETSNMP_CFLAGS)
snmp_ups_LDADD = $(LDADD_DRIVERS) $(LIBNETSNMP_LDFLAGS)

# HAL
hald_addon_usbhid_ups_SOURCES = newhidups.c libhid.c libusb.c hidparser.c	\
 $(NEWHIDUPS_SUBDRIVERS)
hald_addon_usbhid_ups_LDADD = $(LDADD_HAL_DRIVERS) $(LIBUSB_LDFLAGS) $(LIBHAL_LDFLAGS)

hald_addon_tripplite_usb_SOURCES = tripplite_usb.c libhid.c libusb.c hidparser.c
hald_addon_tripplite_usb_LDADD = $(LDADD_HAL_DRIVERS) $(LIBUSB_LDFLAGS) $(LIBHAL_LDFLAGS) -lm

hald_addon_bcmxcp_usb_SOURCES = bcmxcp_usb.c bcmxcp.c nut_usb.c
hald_addon_bcmxcp_usb_LDADD = $(LDADD_HAL_DRIVERS) $(LIBUSB_LDFLAGS) $(LIBHAL_LDFLAGS)

# ----------------------------------------------------------------------
# List of header files. The purpose of this list is not dependency
# tracking (which is automatic), but to ensure these files are
# distributed by "make dist".

dist_noinst_HEADERS = al175.h apccmib.h apc-hid.h apcsmart.h bcmxcp.h	\
 bcmxcp_io.h belkin.h belkin-hid.h belkinunv.h bestfcom.h		\
 bestuferrups.h bestups.h blazer.h cpsups.h cyberpower.h dstate.h	\
 dstate-hal.h								\
 dummy-ups.h esupssmart.h etapro.h everups.h fentonups.h gamatronic.h	\
 explore-hid.h genericups.h hidparser.h hidtypes.h hidups.h ietfmib.h	\
 ippon.h isbmex.h libhid.h libshut.h libusb.h liebert.h main.h		\
 main-hal.h								\
 masterguard.h megatec.h metasys.h mge-hid.h mgemib.h mge-shut.h	\
 mge-utalk.h mustek.h netvisionmib.h newhidups.h nitram.h nut_usb.h	\
 oneac.h optiups.h powercom.h pwmib.h safenet.h serial.h sms.h		\
 snmp-ups.h solis.h tripplite.h tripplite-hid.h tripplitesu.h		\
 upscode2.h victronups.h

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
libnuthalmain_a_SOURCES = main-hal.c dstate-hal.c
libnuthalmain_a_CFLAGS = $(AM_CFLAGS) $(LIBHAL_CFLAGS)

EXTRA_DIST = Doxyfile
MOSTLYCLEANFILES = libnuthalmain.a

# rules for cross-directory targets
../common/libcommon.a:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) libcommon.a)

../common/upsconf.o:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) upsconf.o)

../common/parseconf.o:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) parseconf.o)

../common/state.o:
	(cd ../common/ && $(MAKE) $(AM_MAKEFLAGS) state.o)

