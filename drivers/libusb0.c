/*!
 * @file libusb0.c
 * @brief HID Library - Generic USB communication sub driver (using libusb 0.1)
 *
 * @author Copyright (C)
 *  2003 - 2007 Arnaud Quette <aquette.dev@gmail.com>
 *  2005 - 2007 Peter Selinger <selinger@users.sourceforge.net>
 *  2021 - 2024 Jim Klimov <jimklimov+nut@gmail.com>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 *
 *      The logic of this file is ripped from mge-shut driver (also from
 *      Arnaud Quette), which is a "HID over serial link" UPS driver for
 *      Network UPS Tools <https://www.networkupstools.org/>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

#include "config.h" /* for HAVE_USB_DETACH_KERNEL_DRIVER_NP flag */
#include "common.h" /* for xmalloc, upsdebugx prototypes */
#include "usb-common.h"
#include "nut_libusb.h"
#ifdef WIN32
#include "wincompat.h"
#endif

#define USB_DRIVER_NAME		"USB communication driver (libusb 0.1)"
#define USB_DRIVER_VERSION	"0.50"

/* driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	USB_DRIVER_NAME,
	USB_DRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

#define MAX_REPORT_SIZE         0x1800

#if (!HAVE_STRCASESTR) && (HAVE_STRSTR && HAVE_STRLWR && HAVE_STRDUP)
/* Only used in this file of all NUT codebase, so not in str.{c,h}
 * where it happens to conflict with netsnmp-provided variant for
 * some of our build products.
 */
static char *strcasestr(const char *haystack, const char *needle);
#endif

static void nut_libusb_close(usb_dev_handle *udev);

/*! Add USB-related driver variables with addvar() and dstate_setinfo().
 * This removes some code duplication across the USB drivers.
 */
void nut_usb_addvars(void)
{
	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");

	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");

	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
	addvar(VAR_VALUE, "device", "Regular expression to match USB device name");
	/* Not supported by libusb0, but let's not crash config
	 * parsing on unknown keywords due to such nuances! :) */
	addvar(VAR_VALUE, "busport", "Regular expression to match USB bus port name"
		" (tolerated but ignored in this build)"
	);

	/* Warning: this feature is inherently non-deterministic!
	 * If you only care to know that at least one of your no-name UPSes is online,
	 * this option can help. If you must really know which one, it will not!
	 */
	addvar(VAR_FLAG, "allow_duplicates",
		"If you have several UPS devices which may not be uniquely "
		"identified by options above, allow each driver instance with this "
		"option to take the first match if available, or try another "
		"(association of driver to device may vary between runs)");

	addvar(VAR_VALUE, "usb_set_altinterface", "Force redundant call to usb_set_altinterface() (value=bAlternateSetting; default=0)");

	addvar(VAR_VALUE, "usb_config_index",	"Deeper tuning of USB communications for complex devices");
	addvar(VAR_VALUE, "usb_hid_rep_index",	"Deeper tuning of USB communications for complex devices");
	addvar(VAR_VALUE, "usb_hid_desc_index",	"Deeper tuning of USB communications for complex devices");
	addvar(VAR_VALUE, "usb_hid_ep_in",	"Deeper tuning of USB communications for complex devices");
	addvar(VAR_VALUE, "usb_hid_ep_out",	"Deeper tuning of USB communications for complex devices");

	dstate_setinfo("driver.version.usb", "libusb-0.1 (or compat)");

	upsdebugx(1, "Using USB implementation: %s", dstate_getinfo("driver.version.usb"));
}

/* From usbutils: workaround libusb (0.1) API goofs:
 * "byte" should never be sign extended;
 * using "char" is trouble.
 * Likewise, sizes should never be negative.
 */

/*
static inline int typesafe_control_msg(usb_dev_handle *dev,
        unsigned char requesttype, unsigned char request,
        int value, int index,
        unsigned char *bytes, unsigned size, int timeout)
{
	return usb_control_msg(dev, requesttype, request, value, index,
		(char *) bytes, (int) size, timeout);
}
*/

static inline int typesafe_control_msg(
	usb_dev_handle *dev,
	unsigned char requesttype,
	unsigned char request,
	int value,
	int index,
	usb_ctrl_charbuf bytes,
	usb_ctrl_charbufsize size,
	usb_ctrl_timeout_msec timeout)
{
	return usb_control_msg(dev, requesttype, request, value, index,
		(char *) bytes, (int) size, timeout);
}


/* invoke matcher against device */
static inline int matches(USBDeviceMatcher_t *matcher, USBDevice_t *device) {
	if (!matcher) {
		return 1;
	}
	return matcher->match_function(device, matcher->privdata);
}

/*! If needed, set the USB alternate interface.
 *
 * In NUT 2.7.2 and earlier, the following call was made unconditionally:
 *   usb_set_altinterface(udev, 0);
 *
 * Although harmless on Linux and *BSD, this extra call prevents old Tripp Lite
 * devices from working on Mac OS X (presumably the OS is already setting
 * altinterface to 0).
 */
static int nut_libusb_set_altinterface(usb_dev_handle *udev)
{
	int altinterface = 0, ret = 0;
	char *alt_string, *endp = NULL;

	if(testvar("usb_set_altinterface")) {
		alt_string = getval("usb_set_altinterface");
		if(alt_string) {
			altinterface = (int)strtol(alt_string, &endp, 10);
			if(endp && !(endp[0] == 0)) {
				upslogx(LOG_WARNING, "%s: '%s' is not a valid number", __func__, alt_string);
			}
			if(altinterface < 0 || altinterface > 255) {
				upslogx(LOG_WARNING, "%s: setting bAlternateInterface to %d will probably not work", __func__, altinterface);
			}
		}
		/* set default interface */
		upsdebugx(2, "%s: calling usb_set_altinterface(udev, %d)",
			__func__, altinterface);
		ret = usb_set_altinterface(udev, altinterface);
		if(ret != 0) {
			upslogx(LOG_WARNING, "%s: usb_set_altinterface(udev, %d) returned %d (%s)",
					__func__, altinterface, ret, usb_strerror() );
		}
		upslogx(LOG_NOTICE, "%s: usb_set_altinterface() should not be necessary - "
			"please email the nut-upsdev list with information about your UPS.", __func__);
	} else {
		upsdebugx(3, "%s: skipped usb_set_altinterface(udev, 0)", __func__);
	}
	return ret;
}

static void nut_libusb_subdriver_defaults(usb_communication_subdriver_t *subdriver)
{
	if (!getval("usb_config_index"))
		subdriver->usb_config_index = LIBUSB_DEFAULT_CONF_INDEX;
	if (!getval("usb_hid_rep_index"))
		subdriver->hid_rep_index = LIBUSB_DEFAULT_INTERFACE;
	if (!getval("usb_hid_desc_index"))
		subdriver->hid_desc_index = LIBUSB_DEFAULT_DESC_INDEX;
	if (!getval("usb_hid_ep_in"))
		subdriver->hid_ep_in = LIBUSB_DEFAULT_HID_EP_IN;
	if (!getval("usb_hid_ep_out"))
		subdriver->hid_ep_out = LIBUSB_DEFAULT_HID_EP_OUT;
}

#define usb_control_msg         typesafe_control_msg

/* On success, fill in the curDevice structure and return the report
 * descriptor length. On failure, return -1.
 * Note: When callback is not NULL, the report descriptor will be
 * passed to this function together with the udev and USBDevice_t
 * information. This callback should return a value > 0 if the device
 * is accepted, or < 1 if not. If it isn't accepted, the next device
 * (if any) will be tried, until there are no more devices left.
 */
static int nut_libusb_open(usb_dev_handle **udevp,
	USBDevice_t *curDevice, USBDeviceMatcher_t *matcher,
	int (*callback)(usb_dev_handle *udev,
		USBDevice_t *hd, usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen)
	)
{
#ifdef HAVE_USB_DETACH_KERNEL_DRIVER_NP
	int retries;
#endif
	usb_ctrl_charbufsize rdlen1, rdlen2; /* report descriptor length, method 1+2 */
	USBDeviceMatcher_t *m;
	struct usb_device *dev;
	struct usb_bus *bus;
	usb_dev_handle *udev;
	struct usb_interface_descriptor *iface;

	int ret, res;
	usb_ctrl_char buf[20];
	usb_ctrl_char *p;
	char string[256];
	int i;
	int count_open_EACCESS = 0;
	int count_open_errors = 0;
	int count_open_attempts = 0;

	/* report descriptor */
	usb_ctrl_char	rdbuf[MAX_REPORT_SIZE];
	usb_ctrl_charbufsize		rdlen;

	struct usb_bus *busses;

	static int usb_hid_number_opts_parsed = 0;
	if (!usb_hid_number_opts_parsed) {
		const char *s;
		unsigned short us = 0;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-type-limit-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
		if ((s = getval("usb_config_index"))) {
			if (!str_to_ushort(s, &us, 16) || (us > USB_CTRL_CFGINDEX_MAX)) {
				fatalx(EXIT_FAILURE, "%s: could not parse usb_config_index", __func__);
			}
			usb_subdriver.usb_config_index = (usb_ctrl_cfgindex)us;
		}
		if ((s = getval("usb_hid_rep_index"))) {
			if (!str_to_ushort(s, &us, 16) || (us > USB_CTRL_REPINDEX_MAX)) {
				fatalx(EXIT_FAILURE, "%s: could not parse usb_hid_rep_index", __func__);
			}
			usb_subdriver.hid_rep_index = (usb_ctrl_repindex)us;
		}
		if ((s = getval("usb_hid_desc_index"))) {
			if (!str_to_ushort(s, &us, 16) || (us > USB_CTRL_DESCINDEX_MAX)) {
				fatalx(EXIT_FAILURE, "%s: could not parse usb_hid_desc_index", __func__);
			}
			usb_subdriver.hid_desc_index = (usb_ctrl_descindex)us;
		}
		if ((s = getval("usb_hid_ep_in"))) {
			if (!str_to_ushort(s, &us, 16) || (us > USB_CTRL_ENDPOINT_MAX)) {
				fatalx(EXIT_FAILURE, "%s: could not parse usb_hid_ep_in", __func__);
			}
			usb_subdriver.hid_ep_in = (usb_ctrl_endpoint)us;
		}
		if ((s = getval("usb_hid_ep_out"))) {
			if (!str_to_ushort(s, &us, 16) || (us > USB_CTRL_ENDPOINT_MAX)) {
				fatalx(EXIT_FAILURE, "%s: could not parse usb_hid_ep_out", __func__);
			}
			usb_subdriver.hid_ep_out = (usb_ctrl_endpoint)us;
		}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE) )
# pragma GCC diagnostic pop
#endif

		usb_hid_number_opts_parsed = 1;
	}

	/* libusb base init */
	usb_init();
	usb_find_busses();
	usb_find_devices();

#ifdef WIN32
	busses = usb_get_busses();
#else
	/* libusb built-in; not sure why original NUT for WIN32
	 * code differed or if it is actually better? Or why
	 * this was not tackled in a few other files for USB?..
	 */
	busses = usb_busses;
#endif

#ifndef __linux__ /* SUN_LIBUSB (confirmed to work on Solaris and FreeBSD) */
	/* Causes a double free corruption in linux if device is detached! */
	nut_libusb_close(*udevp);
#endif

	upsdebugx(3, "usb_busses=%p", (void*)usb_busses);

	for (bus = busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			/* int	if_claimed = 0; */

			count_open_attempts++;

			upsdebugx(2, "Checking device (%04X/%04X) (%s/%s)",
				dev->descriptor.idVendor, dev->descriptor.idProduct,
				bus->dirname, dev->filename);

			/* supported vendors are now checked by the
			   supplied matcher */

			/* open the device */
			*udevp = udev = usb_open(dev);
			if (!udev) {
				/* It seems that with libusb-0.1 API we
				 * can only evaluate the string value of
				 * usb_strerror() return values - in the
				 * library source there is magic about
				 * tracking errors in their string buffer
				 * or as a printable errno, and no reliably
				 * usable way to learn of an EACCESS or
				 * other situation diagnostics otherwise.
				 * So we have to search for sub-strings
				 * and hope for locale to be right...
				 */
				char *libusb_error = usb_strerror();
				upsdebugx(1, "Failed to open device (%04X/%04X), skipping: %s",
					dev->descriptor.idVendor,
					dev->descriptor.idProduct,
					libusb_error);

				count_open_errors++;
				if (strcasestr(libusb_error, "Access denied")
				||  strcasestr(libusb_error, "insufficient permissions")
				) {
					count_open_EACCESS++;
				}

				continue;
			}

			/* collect the identifying information of this
			   device. Note that this is safe, because
			   there's no need to claim an interface for
			   this (and therefore we do not yet need to
			   detach any kernel drivers). */

			free(curDevice->Vendor);
			free(curDevice->Product);
			free(curDevice->Serial);
			free(curDevice->Bus);
			free(curDevice->Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
			free(curDevice->BusPort);
#endif
			memset(curDevice, '\0', sizeof(*curDevice));

			/* Keep the list of items in sync with those matched by
			 * drivers/libusb1.c and tools/nut-scanner/scan_usb.c:
			 */
			curDevice->VendorID = dev->descriptor.idVendor;
			curDevice->ProductID = dev->descriptor.idProduct;
			curDevice->Bus = xstrdup(bus->dirname);
			curDevice->Device = xstrdup(dev->filename);
			curDevice->bcdDevice = dev->descriptor.bcdDevice;

#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
			curDevice->BusPort = (char *)malloc(4);
			if (curDevice->BusPort == NULL) {
				fatal_with_errno(EXIT_FAILURE, "Out of memory");
			}
			upsdebugx(2, "%s: NOTE: BusPort is always zero with libusb0", __func__);
			sprintf(curDevice->BusPort, "%03d", 0);
#endif

			if (dev->descriptor.iManufacturer) {
				ret = nut_usb_get_string(udev, dev->descriptor.iManufacturer,
					string, sizeof(string));
				if (ret > 0) {
					curDevice->Vendor = xstrdup(string);
				} else {
					upsdebugx(1, "%s: get Manufacturer string failed", __func__);
				}
			}

			if (dev->descriptor.iProduct) {
				ret = nut_usb_get_string(udev, dev->descriptor.iProduct,
					string, sizeof(string));
				if (ret > 0) {
					curDevice->Product = xstrdup(string);
				} else {
					upsdebugx(1, "%s: get Product string failed", __func__);
				}
			}

			if (dev->descriptor.iSerialNumber) {
				ret = nut_usb_get_string(udev, dev->descriptor.iSerialNumber,
					string, sizeof(string));
				if (ret > 0) {
					curDevice->Serial = xstrdup(string);
				} else {
					upsdebugx(1, "%s: get Serial Number string failed", __func__);
				}
			}

			upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
			upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
			upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor ? curDevice->Vendor : "unknown");
			upsdebugx(2, "- Product: %s", curDevice->Product ? curDevice->Product : "unknown");
			upsdebugx(2, "- Serial Number: %s", curDevice->Serial ? curDevice->Serial : "unknown");
			upsdebugx(2, "- Bus: %s", curDevice->Bus ? curDevice->Bus : "unknown");
			upsdebugx(2, "- Device: %s", curDevice->Device ? curDevice->Device : "unknown");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
			upsdebugx(2, "- Bus Port: %s", curDevice->BusPort ? curDevice->BusPort : "unknown");
#endif
			upsdebugx(2, "- Device release number: %04x", curDevice->bcdDevice);

			/* FIXME: extend to Eaton OEMs (HP, IBM, ...) */
			if ((curDevice->VendorID == 0x463) && (curDevice->bcdDevice == 0x0202)) {
				if (!getval("usb_hid_desc_index"))
					usb_subdriver.hid_desc_index = 1;
			}

			upsdebugx(2, "Trying to match device");
			for (m = matcher; m; m=m->next) {
				ret = matches(m, curDevice);
				if (ret==0) {
					upsdebugx(2, "Device does not match - skipping");
					goto next_device;
				} else if (ret==-1) {
					fatal_with_errno(EXIT_FAILURE, "matcher");
#ifndef HAVE___ATTRIBUTE__NORETURN
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunreachable-code"
# endif
					goto next_device;
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
#  pragma GCC diagnostic pop
# endif
#endif
				} else if (ret==-2) {
					upsdebugx(2, "matcher: unspecified error");
					goto next_device;
				}
			}

			/* If we got here, none of the matchers said
			 * that the device is not what we want. */
			upsdebugx(2, "Device matches");

			/* Now we have matched the device we wanted. Claim it. */

#ifdef HAVE_USB_DETACH_KERNEL_DRIVER_NP
			/* this method requires at least libusb 0.1.8:
			 * it force device claiming by unbinding
			 * attached driver... From libhid */
#ifdef WIN32
			usb_set_configuration(udev, 1);
#endif
			retries = 3;
			while ((ret = usb_claim_interface(udev, usb_subdriver.hid_rep_index)) < 0) {
				upsdebugx(2, "failed to claim USB device: %s",
					usb_strerror());

				if (ret == LIBUSB_ERROR_BUSY && testvar("allow_duplicates")) {
					upsdebugx(2, "Configured to allow_duplicates so looking for another similar device");
					goto next_device;
				}

				if (usb_detach_kernel_driver_np(udev, usb_subdriver.hid_rep_index) < 0) {
					upsdebugx(2, "failed to detach kernel driver from USB device: %s",
						usb_strerror());
				} else {
					upsdebugx(2, "detached kernel driver from USB device...");
				}

				if (retries-- > 0) {
					continue;
				}

				fatalx(EXIT_FAILURE,
					"Can't claim USB device [%04x:%04x]@%d/%d/%d: %s",
					curDevice->VendorID, curDevice->ProductID,
					usb_subdriver.usb_config_index,
					usb_subdriver.hid_rep_index,
					usb_subdriver.hid_desc_index,
					usb_strerror());
			}
#else
			if ((ret = usb_claim_interface(udev, usb_subdriver.hid_rep_index)) < 0) {
				if (ret == LIBUSB_ERROR_BUSY && testvar("allow_duplicates")) {
					upsdebugx(2, "Configured to allow_duplicates so looking for another similar device");
					goto next_device;
				}

				fatalx(EXIT_FAILURE,
					"Can't claim USB device [%04x:%04x]@%d/%d/%d: %s",
					curDevice->VendorID, curDevice->ProductID,
					usb_subdriver.usb_config_index,
					usb_subdriver.hid_rep_index,
					usb_subdriver.hid_desc_index,
					usb_strerror());
			}
#endif
			/* if_claimed = 1; */

			nut_libusb_set_altinterface(udev);

			/* Did the driver provide a callback method for any further
			 * device acceptance checks (e.g. when same ID is supported
			 * by several sub-drivers, differing by vendor/model strings)?
			 */
			if (!callback) {
				return 1;
			}

			if (!dev->config) { /* ?? this should never happen */
				upsdebugx(2, "  Couldn't retrieve descriptors");
				goto next_device;
			}

			rdlen1 = -1;
			rdlen2 = -1;

			/* Get HID descriptor */

			/* FIRST METHOD: ask for HID descriptor directly. */
			/* res = usb_get_descriptor(udev, USB_DT_HID, hid_desc_index, buf, 0x9); */
			res = usb_control_msg(udev,
				USB_ENDPOINT_IN + 1,
				USB_REQ_GET_DESCRIPTOR,
				(USB_DT_HID << 8) + usb_subdriver.hid_desc_index,
				usb_subdriver.hid_rep_index,
				buf, 0x9, USB_TIMEOUT);

			if (res < 0) {
				upsdebugx(2, "Unable to get HID descriptor (%s)",
					usb_strerror());
			} else if (res < 9) {
				upsdebugx(2, "HID descriptor too short (expected %d, got %d)", 9, res);
			} else {
				upsdebugx(2, "Retrieved HID descriptor (expected %d, got %d)", 9, res);
				upsdebug_hex(3, "HID descriptor, method 1", buf, 9);

				rdlen1 = ((uint8_t)buf[7]) | (((uint8_t)buf[8]) << 8);
			}

			if (rdlen1 < -1) {
				upsdebugx(2, "Warning: HID descriptor, method 1 failed");
			}
			upsdebugx(3,
				"HID descriptor length (method 1) %" PRI_NUT_USB_CTRL_CHARBUFSIZE,
				rdlen1);

			/* SECOND METHOD: find HID descriptor among "extra" bytes of
			   interface descriptor, i.e., bytes tucked onto the end of
			   descriptor 2. */

			/* Note: on some broken UPS's (e.g. Tripp Lite Smart1000LCD),
				only this second method gives the correct result */
			iface = &dev->config[usb_subdriver.usb_config_index].interface[usb_subdriver.hid_rep_index].altsetting[0];
			for (i=0; i<iface->extralen; i+=iface->extra[i]) {
				upsdebugx(4, "i=%d, extra[i]=%02x, extra[i+1]=%02x", i,
					iface->extra[i], iface->extra[i+1]);

				if (i+9 <= iface->extralen
				&&  iface->extra[i] >= 9
				&&  iface->extra[i+1] == 0x21
				) {
					p = (usb_ctrl_char *)&iface->extra[i];
					upsdebug_hex(3, "HID descriptor, method 2", p, 9);
					rdlen2 = ((uint8_t)p[7]) | (((uint8_t)p[8]) << 8);
					break;
				}
			}

			if (rdlen2 < -1) {
				upsdebugx(2, "Warning: HID descriptor, method 2 failed");
			}
			upsdebugx(3,
				"HID descriptor length (method 2) %" PRI_NUT_USB_CTRL_CHARBUFSIZE,
				rdlen2);

			/* when available, always choose the second value, as it
				seems to be more reliable (it is the one reported e.g. by
				lsusb). Note: if the need arises, can change this to use
				the maximum of the two values instead. */
			if ((curDevice->VendorID == 0x463) && (curDevice->bcdDevice == 0x0202)) {
				upsdebugx(1, "Eaton device v2.02. Using full report descriptor");
				rdlen = rdlen1;
			}
			else {
				rdlen = rdlen2 >= 0 ? rdlen2 : rdlen1;
			}

			if (rdlen < 0) {
				upsdebugx(2, "Unable to retrieve any HID descriptor");
				goto next_device;
			}
			if (rdlen1 >= 0 && rdlen2 >= 0 && rdlen1 != rdlen2) {
				upsdebugx(2, "Warning: two different HID descriptors retrieved "
					"(Reportlen = %" PRI_NUT_USB_CTRL_CHARBUFSIZE
					" vs. %" PRI_NUT_USB_CTRL_CHARBUFSIZE ")",
					rdlen1, rdlen2);
			}

			upsdebugx(2,
				"HID descriptor length %" PRI_NUT_USB_CTRL_CHARBUFSIZE,
				rdlen);

			if ((uintmax_t)rdlen > sizeof(rdbuf)) {
				upsdebugx(2,
					"HID descriptor too long %" PRI_NUT_USB_CTRL_CHARBUFSIZE
					" (max %" PRIuSIZE ")",
					rdlen, sizeof(rdbuf));
				goto next_device;
			}

			/* Note: rdlen is safe to cast to unsigned below,
			 * since the <0 case was ruled out above */
			/* res = usb_get_descriptor(udev, USB_DT_REPORT, hid_desc_index, bigbuf, rdlen); */
			res = usb_control_msg(udev,
				USB_ENDPOINT_IN + 1,
				USB_REQ_GET_DESCRIPTOR,
				(USB_DT_REPORT << 8) + usb_subdriver.hid_desc_index,
				usb_subdriver.hid_rep_index,
				rdbuf, rdlen, USB_TIMEOUT);

			if (res < 0)
			{
				upsdebug_with_errno(2, "Unable to get Report descriptor");
				goto next_device;
			}

			if (res < rdlen)
			{
#ifndef WIN32
				upsdebugx(2, "Warning: report descriptor too short "
					"(expected %" PRI_NUT_USB_CTRL_CHARBUFSIZE
					", got %d)", rdlen, res);
#else
				/* https://github.com/networkupstools/nut/issues/1690#issuecomment-1455206002 */
				upsdebugx(0, "Warning: report descriptor too short "
					"(expected %" PRI_NUT_USB_CTRL_CHARBUFSIZE
					", got %d)", rdlen, res);
				upsdebugx(0, "Please check your Windows Device Manager: "
					"perhaps the UPS was recognized by default OS\n"
					"driver such as HID UPS Battery (hidbatt.sys, "
					"hidusb.sys or similar). It could have been\n"
					"\"restored\" by Windows Update. You can try "
					"https://zadig.akeo.ie/ to handle it with\n"
					"either WinUSB, libusb0.sys or libusbK.sys.");
#endif	/* WIN32 */
				rdlen = res; /* correct rdlen if necessary */
			}

			res = callback(udev, curDevice, rdbuf, rdlen);
			if (res < 1) {
				upsdebugx(2, "Caller doesn't like this device");
				goto next_device;
			}

			upsdebugx(2,
				"Report descriptor retrieved (Reportlen = %"
				PRI_NUT_USB_CTRL_CHARBUFSIZE ")", rdlen);
			upsdebugx(2, "Found HID device");

			upsdebugx(3, "Using default, detected or customized USB HID numbers: "
				"usb_config_index=%d usb_hid_rep_index=%d "
				"usb_hid_desc_index=%d "
				"usb_hid_ep_in=%d usb_hid_ep_out=%d",
				usb_subdriver.usb_config_index,
				usb_subdriver.hid_rep_index,
				usb_subdriver.hid_desc_index,
				usb_subdriver.hid_ep_in,
				usb_subdriver.hid_ep_out
				);

			fflush(stdout);

			return rdlen;

		next_device:
			/* usb_release_interface() sometimes blocks
			 * and goes into uninterruptible sleep.
			 * So don't do it. */
			/* if (if_claimed)
				usb_release_interface(udev, 0); */
			usb_close(udev);
			/* reset any parameters modified by unmatched drivers back to defaults */
			nut_libusb_subdriver_defaults(&usb_subdriver);
		}
	}

	/* If we got here, we did not return a successfully chosen device above */
	*udevp = NULL;
	upsdebugx(2, "libusb0: No appropriate HID device found");
	fflush(stdout);

	if (count_open_attempts == 0) {
		upslogx(LOG_WARNING,
			"libusb0: Could not open any HID devices: "
			"no USB buses (or devices) found");
	}
	else
	if (count_open_errors > 0
	&&  count_open_errors == count_open_EACCESS
	) {
		upslogx(LOG_WARNING,
			"libusb0: Could not open any HID devices: "
			"insufficient permissions on everything");
		if (count_open_attempts > count_open_errors) {
			upslogx(LOG_WARNING,
				"libusb0: except %d devices "
				"tried but not matching the "
				"requested criteria",
				count_open_attempts - count_open_errors);
		}
	}

	return -1;
}

/*
 * Error handler for usb_get/set_* functions. Return value > 0 success,
 * 0 unknown or temporary failure (ignored), < 0 permanent failure (reconnect)
 */
static int nut_libusb_strerror(const int ret, const char *desc)
{
	if (ret > 0) {
		return ret;
	}

	switch(ret)
	{
	case -EBUSY:	/* Device or resource busy */
	case -EPERM:	/* Operation not permitted */
	case -ENODEV:	/* No such device */
	case -EACCES:	/* Permission denied */
	case -EIO:	/* I/O error */
	case -ENXIO:	/* No such device or address */
	case -ENOENT:	/* No such file or directory */
	case -EPIPE:	/* Broken pipe */
	case -ENOSYS:	/* Function not implemented */
		upslogx(LOG_DEBUG, "%s: %s", desc, usb_strerror());
		return ret;

	case -ETIMEDOUT:	/* Connection timed out */
		upsdebugx(2, "%s: Connection timed out", desc);
		return 0;

/* libusb-win32 does not know EPROTO and EOVERFLOW,
 * it only returns EIO for any IO errors */
#ifndef WIN32
	case -EOVERFLOW:	/* Value too large for defined data type */
# ifdef EPROTO
	case -EPROTO:	/* Protocol error */
# endif
		upsdebugx(2, "%s: %s", desc, usb_strerror());
		return 0;
#endif	/* WIN32 */

	case 0: 	/** TOTHINK: Should this (probably LIBUSB_SUCCESS) be quiet? */
	default:	/* Undetermined, log only */
		upslogx(LOG_DEBUG, "%s: %s", desc, usb_strerror());
		return 0;
	}
}

/* return the report of ID=type in report
 * return -1 on failure, report length on success
 */

/* Expected evaluated types for the API:
 * static int nut_libusb_get_report(usb_dev_handle *udev,
 *	int ReportId, unsigned char *raw_buf, int ReportSize)
 */
static int nut_libusb_get_report(
	usb_dev_handle *udev,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	int	ret;

	upsdebugx(4, "Entering nut_libusb_get_report");

	if (!udev) {
		return 0;
	}

	ret = usb_control_msg(udev,
		USB_ENDPOINT_IN + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
		0x01, /* HID_REPORT_GET */
		ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		usb_subdriver.hid_rep_index,
		raw_buf, ReportSize, USB_TIMEOUT);

#ifdef WIN32
	errno = -ret;
#endif

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == -EPIPE) {
		return 0;
	}

	return nut_libusb_strerror(ret, __func__);
}

/* Expected evaluated types for the API:
 * static int nut_libusb_set_report(usb_dev_handle *udev,
 *	int ReportId, unsigned char *raw_buf, int ReportSize)
 */
static int nut_libusb_set_report(
	usb_dev_handle *udev,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	int	ret;

	if (!udev) {
		return 0;
	}

	ret = usb_control_msg(udev,
		USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
		0x09, /* HID_REPORT_SET = 0x09*/
		ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		usb_subdriver.hid_rep_index,
		raw_buf, ReportSize, USB_TIMEOUT);

#ifdef WIN32
	errno = -ret;
#endif

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == -EPIPE) {
		return 0;
	}

	return nut_libusb_strerror(ret, __func__);
}

/* Expected evaluated types for the API:
 * static int nut_libusb_get_string(usb_dev_handle *udev,
 *	int StringIdx, char *buf, int buflen)
 */
static int nut_libusb_get_string(
	usb_dev_handle *udev,
	usb_ctrl_strindex StringIdx,
	char *buf,
	usb_ctrl_charbufsize buflen)
{
	int ret;

	/*
	 * usb.h:int  nut_usb_get_string(usb_dev_handle *dev, int index,
	 * usb.h-         char *buf, size_t buflen);
	 */
	if (!udev
	|| StringIdx < 1 || StringIdx > 255
	|| buflen < 1
	) {
		return -1;
	}

	ret = nut_usb_get_string(udev, StringIdx, buf, (size_t)buflen);

#ifdef WIN32
	errno = -ret;
#endif

	/** 0 can be seen as an empty string, or as a success for
	 * logging below - also tends to happen */
	if (ret == 0) {
		size_t len = strlen(buf);
		upsdebugx(2, "%s: nut_usb_get_string() returned "
			"0 (might be just success code), "
			"actual buf length is %" PRIuSIZE, __func__, len);
		/* if (len) */
			return len;
		/* else may log "nut_libusb_get_string: Success" and return 0 below */
	}

	return nut_libusb_strerror(ret, __func__);
}

/* Expected evaluated types for the API:
 * static int nut_libusb_get_interrupt(usb_dev_handle *udev,
 *	unsigned char *buf, int bufsize, int timeout)
 */
static int nut_libusb_get_interrupt(
	usb_dev_handle *udev,
	usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize,
	usb_ctrl_timeout_msec timeout)
{
	int ret;

	if (!udev) {
		return -1;
	}

	/* Interrupt EP is USB_ENDPOINT_IN with offset defined in hid_ep_in, which is 0 by default, unless overridden in subdriver. */
	ret = usb_interrupt_read(udev, USB_ENDPOINT_IN + usb_subdriver.hid_ep_in, (char *)buf, bufsize, timeout);

#ifdef WIN32
	errno = -ret;
#endif

	/* Clear stall condition */
	if (ret == -EPIPE) {
		ret = usb_clear_halt(udev, 0x81);
	}

	return nut_libusb_strerror(ret, __func__);
}

static void nut_libusb_close(usb_dev_handle *udev)
{
	if (!udev) {
		return;
	}

	/* usb_release_interface() sometimes blocks and goes
	 * into uninterruptible sleep.  So don't do it.
	 */
	/* usb_release_interface(udev, 0); */
	usb_close(udev);
}

#if (!HAVE_STRCASESTR) && (HAVE_STRSTR && HAVE_STRLWR && HAVE_STRDUP)
static char *strcasestr(const char *haystack, const char *needle) {
	/* work around "const char *" and guarantee the original is not
	 * touched... not efficient but we have few uses for this method */
	char * dH = NULL, *dN = NULL, *lH = NULL, *lN = NULL, *first = NULL;

	dH = strdup(haystack);
	if (dH == NULL) goto err;
	dN = strdup(needle);
	if (dN == NULL) goto err;
	lH = strlwr(dH);
	if (lH == NULL) goto err;
	lN = strlwr(dN);
	if (lN == NULL) goto err;
	first = strstr(lH, lN);

err:
	if (dH != NULL) free(dH);
	if (dN != NULL) free(dN);
	/* Does this implementation of strlwr() change original buffer? */
	if (lH != dH && lH != NULL) free(lH);
	if (lN != dN && lN != NULL) free(lN);
	if (first == NULL) {
		return NULL;
	}

	/* Pointer to first char of the needle found in original haystack */
	return (char *)(haystack + (first - lH));
}
#endif

usb_communication_subdriver_t usb_subdriver = {
	USB_DRIVER_NAME,
	USB_DRIVER_VERSION,
	nut_libusb_open,
	nut_libusb_close,
	nut_libusb_get_report,
	nut_libusb_set_report,
	nut_libusb_get_string,
	nut_libusb_get_interrupt,
	LIBUSB_DEFAULT_CONF_INDEX,
	LIBUSB_DEFAULT_INTERFACE,
	LIBUSB_DEFAULT_DESC_INDEX,
	LIBUSB_DEFAULT_HID_EP_IN,
	LIBUSB_DEFAULT_HID_EP_OUT
};
