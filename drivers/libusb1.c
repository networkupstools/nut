/*!
 * @file libusb1.c
 * @brief Generic USB communication backend (using libusb 1.0)
 *
 * @author Copyright (C) 2016 Eaton
 *         Copyright (C) 2016 Arnaud Quette <aquette.dev@gmail.com>
 *         Copyright (C) 2021 Jim Klimov <jimklimov+nut@gmail.com>
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

#include "config.h" /* for HAVE_LIBUSB_DETACH_KERNEL_DRIVER flag */
#include "common.h" /* for xmalloc, upsdebugx prototypes */
#include "usb-common.h"
#include "nut_libusb.h"
#include "nut_stdint.h"

#define USB_DRIVER_NAME		"USB communication driver (libusb 1.0)"
#define USB_DRIVER_VERSION	"0.47"

/* driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	USB_DRIVER_NAME,
	USB_DRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

#define MAX_REPORT_SIZE         0x1800
#define MAX_RETRY               3

static void nut_libusb_close(libusb_device_handle *udev);

/*! Add USB-related driver variables with addvar() and dstate_setinfo().
 * This removes some code duplication across the USB drivers.
 */
void nut_usb_addvars(void)
{
	const struct libusb_version	*v = libusb_get_version();

	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");

	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");

	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
	addvar(VAR_VALUE, "device", "Regular expression to match USB device name");
	addvar(VAR_VALUE, "busport", "Regular expression to match USB bus port name"
#if (!defined WITH_USB_BUSPORT) || (!WITH_USB_BUSPORT)
		/* Not supported by this version of libusb1,
		 * but let's not crash config parsing on
		 * unknown keywords due to such nuances! :)
		 */
		" (tolerated but ignored in this build)"
#endif
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

#ifdef LIBUSB_API_VERSION
	dstate_setinfo("driver.version.usb", "libusb-%u.%u.%u (API: 0x%x)", v->major, v->minor, v->micro, LIBUSB_API_VERSION);
#else  /* no LIBUSB_API_VERSION */
	dstate_setinfo("driver.version.usb", "libusb-%u.%u.%u", v->major, v->minor, v->micro);
#endif /* LIBUSB_API_VERSION */

	upsdebugx(1, "Using USB implementation: %s", dstate_getinfo("driver.version.usb"));
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
static int nut_usb_set_altinterface(libusb_device_handle *udev)
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
		upsdebugx(2, "%s: calling libusb_set_interface_alt_setting(udev, %d, %d)",
			__func__, usb_subdriver.hid_rep_index, altinterface);
		ret = libusb_set_interface_alt_setting(udev, usb_subdriver.hid_rep_index, altinterface);
		if(ret != 0) {
			upslogx(LOG_WARNING, "%s: libusb_set_interface_alt_setting(udev, %d, %d) returned %d (%s)",
				__func__, usb_subdriver.hid_rep_index, altinterface, ret, libusb_strerror((enum libusb_error)ret) );
		}
		upslogx(LOG_NOTICE, "%s: libusb_set_interface_alt_setting() should not be necessary - "
			"please email the nut-upsdev list with information about your UPS.", __func__);
	} else {
		upsdebugx(3, "%s: skipped libusb_set_interface_alt_setting(udev, %d, 0)",
			__func__, usb_subdriver.hid_rep_index);
	}
	return ret;
}

/* On success, fill in the curDevice structure and return the report
 * descriptor length. On failure, return -1.
 * Note: When callback is not NULL, the report descriptor will be
 * passed to this function together with the udev and USBDevice_t
 * information. This callback should return a value > 0 if the device
 * is accepted, or < 1 if not. If it isn't accepted, the next device
 * (if any) will be tried, until there are no more devices left.
 */
static int nut_libusb_open(libusb_device_handle **udevp,
	USBDevice_t *curDevice, USBDeviceMatcher_t *matcher,
	int (*callback)(libusb_device_handle *udev,
		USBDevice_t *hd, usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen)
	)
{
	int retries;
	/* libusb-1.0 usb_ctrl_charbufsize is uint16_t and we
	 * want the rdlen vars signed - so taking a wider type */
	int32_t rdlen1, rdlen2; /* report descriptor length, method 1+2 */
	USBDeviceMatcher_t *m;
	libusb_device **devlist;
	ssize_t	devcount = 0;
	size_t	devnum;
	struct libusb_device_descriptor dev_desc;
	struct libusb_config_descriptor *conf_desc = NULL;
	const struct libusb_interface_descriptor *if_desc;
	libusb_device_handle *udev;
	uint8_t bus_num, device_addr;
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	uint8_t bus_port;
#endif
	int ret, res;
	unsigned char buf[20];
	const unsigned char *p;
	char string[256];
	int i;
	int count_open_EACCESS = 0;
	int count_open_errors = 0;

	/* report descriptor */
	unsigned char	rdbuf[MAX_REPORT_SIZE];
	int32_t		rdlen;

	static int	usb_hid_number_opts_parsed = 0;
	if (!usb_hid_number_opts_parsed) {
		const char	*s;
		unsigned short	us = 0;

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
	if (libusb_init(NULL) < 0) {
		libusb_exit(NULL);
		fatal_with_errno(EXIT_FAILURE, "Failed to init libusb 1.0");
	}

/* TODO: Find a place for this, from Windows branch made for libusb0.c */
/*
#ifdef WIN32
	struct usb_bus *busses;
	busses = usb_get_busses();
#endif
 */

#ifndef __linux__ /* SUN_LIBUSB (confirmed to work on Solaris and FreeBSD) */
	/* Causes a double free corruption in linux if device is detached! */
	/* nut_libusb_close(*udevp); */
	if (*udevp)
		libusb_close(*udevp);
#endif

	devcount = libusb_get_device_list(NULL, &devlist);

	/* devcount may be < 0, loop will get skipped;
	 * its SSIZE_MAX < SIZE_MAX for devnum */
	for (devnum = 0; (ssize_t)devnum < devcount; devnum++) {
		/* int		if_claimed = 0; */
		libusb_device	*device = devlist[devnum];

		libusb_get_device_descriptor(device, &dev_desc);
		upsdebugx(2, "Checking device %" PRIuSIZE " of %" PRIuSIZE " (%04X/%04X)",
			devnum + 1, devcount,
			dev_desc.idVendor, dev_desc.idProduct);

		/* supported vendors are now checked by the supplied matcher */

		/* open the device */
		ret = libusb_open(device, udevp);
		if (ret != 0) {
			upsdebugx(1, "Failed to open device (%04X/%04X), skipping: %s",
				dev_desc.idVendor,
				dev_desc.idProduct,
				libusb_strerror((enum libusb_error)ret));
			count_open_errors++;
			if (ret == LIBUSB_ERROR_ACCESS) {
				count_open_EACCESS++;
			}
			continue;
		}
		udev = *udevp;

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
		 * drivers/libusb0.c and tools/nut-scanner/scan_usb.c:
		 */
		bus_num = libusb_get_bus_number(device);
		curDevice->Bus = (char *)malloc(4);
		if (curDevice->Bus == NULL) {
			libusb_free_device_list(devlist, 1);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		}
		sprintf(curDevice->Bus, "%03d", bus_num);

		device_addr = libusb_get_device_address(device);
		curDevice->Device = (char *)malloc(4);
		if (curDevice->Device == NULL) {
			libusb_free_device_list(devlist, 1);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		}
		if (device_addr > 0) {
			/* 0 means not available, e.g. lack of platform support */
			sprintf(curDevice->Device, "%03d", device_addr);
		} else {
			if (devnum <= 999) {
				/* Log visibly so users know their number discovered
				 * from `lsusb` or `dmesg` (if any) was ignored */
				upsdebugx(0, "%s: invalid libusb device address %" PRIu8 ", "
					"falling back to enumeration order counter %" PRIuSIZE,
					__func__, device_addr, devnum);
				sprintf(curDevice->Device, "%03d", (int)devnum);
			} else {
				upsdebugx(1, "%s: invalid libusb device address %" PRIu8,
					__func__, device_addr);
				free(curDevice->Device);
				curDevice->Device = NULL;
			}
		}

#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
		bus_port = libusb_get_port_number(device);
		curDevice->BusPort = (char *)malloc(4);
		if (curDevice->BusPort == NULL) {
			libusb_free_device_list(devlist, 1);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		}
		if (bus_port > 0) {
			sprintf(curDevice->BusPort, "%03d", bus_port);
		} else {
			upsdebugx(1, "%s: invalid libusb bus number %i",
				__func__, bus_port);
			free(curDevice->BusPort);
			curDevice->BusPort = NULL;
		}
#endif

		curDevice->VendorID = dev_desc.idVendor;
		curDevice->ProductID = dev_desc.idProduct;
		curDevice->bcdDevice = dev_desc.bcdDevice;

		if (dev_desc.iManufacturer) {
			retries = MAX_RETRY;
			while (retries > 0) {
				ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iManufacturer,
					(unsigned char*)string, sizeof(string));
				if (ret > 0) {
					curDevice->Vendor = strdup(string);
					if (curDevice->Vendor == NULL) {
						libusb_free_device_list(devlist, 1);
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
					break;
				}
				retries--;
				upsdebugx(1, "%s get iManufacturer failed, retrying...", __func__);
			}
		}

		if (dev_desc.iProduct) {
			retries = MAX_RETRY;
			while (retries > 0) {
				ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iProduct,
					(unsigned char*)string, sizeof(string));
				if (ret > 0) {
					curDevice->Product = strdup(string);
					if (curDevice->Product == NULL) {
						libusb_free_device_list(devlist, 1);
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
					break;
				}
				retries--;
				upsdebugx(1, "%s get iProduct failed, retrying...", __func__);
			}
		}

		if (dev_desc.iSerialNumber) {
			retries = MAX_RETRY;
			while (retries > 0) {
				ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iSerialNumber,
					(unsigned char*)string, sizeof(string));
				if (ret > 0) {
					curDevice->Serial = strdup(string);
					if (curDevice->Serial == NULL) {
						libusb_free_device_list(devlist, 1);
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
					break;
				}
				retries--;
				upsdebugx(1, "%s get iSerialNumber failed, retrying...", __func__);
			}
		}

		upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
		upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
		upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor ? curDevice->Vendor : "unknown");
		upsdebugx(2, "- Product: %s", curDevice->Product ? curDevice->Product : "unknown");
		upsdebugx(2, "- Serial Number: %s", curDevice->Serial ? curDevice->Serial : "unknown");
		upsdebugx(2, "- Bus: %s", curDevice->Bus ? curDevice->Bus : "unknown");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
		upsdebugx(2, "- Bus Port: %s", curDevice->BusPort ? curDevice->BusPort : "unknown");
#endif
		upsdebugx(2, "- Device: %s", curDevice->Device ? curDevice->Device : "unknown");
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
				libusb_free_device_list(devlist, 1);
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

		upsdebugx(2, "Reading configuration descriptor %d of %d",
			usb_subdriver.usb_config_index+1, dev_desc.bNumConfigurations);
		ret = libusb_get_config_descriptor(device,
			(uint8_t)usb_subdriver.usb_config_index,
			&conf_desc);
		/*ret = libusb_get_active_config_descriptor(device, &conf_desc);*/
		if (ret < 0)
			upsdebugx(2, "result: %i (%s)",
				ret, libusb_strerror((enum libusb_error)ret));

		/* Now we have matched the device we wanted. Claim it. */

#if defined(HAVE_LIBUSB_KERNEL_DRIVER_ACTIVE) && defined(HAVE_LIBUSB_SET_AUTO_DETACH_KERNEL_DRIVER)
		/* Due to the way FreeBSD implements libusb_set_auto_detach_kernel_driver(),
		 * check to see if the kernel driver is active before setting
		 * the auto-detach flag. Otherwise, libusb_claim_interface()
		 * with the auto-detach flag only works if the driver is
		 * running as root.
		 *
		 * Is the kernel driver active? Consider the unimplemented
		 * return code to be equivalent to inactive here.
		 */
		if((ret = libusb_kernel_driver_active(udev, usb_subdriver.hid_rep_index)) == 1) {
			upsdebugx(3, "libusb_kernel_driver_active() returned 1 (driver active)");
			/* Try the auto-detach kernel driver method.
			 * This function is not available on FreeBSD 10.1-10.3 */
			if ((ret = libusb_set_auto_detach_kernel_driver (udev, 1)) != LIBUSB_SUCCESS) {
				upsdebugx(1, "failed to set kernel driver auto-detach "
					"driver flag for USB device: %s",
					libusb_strerror((enum libusb_error)ret));
			} else {
				upsdebugx(2, "successfully set kernel driver auto-detach flag");
			}
		} else {
			upsdebugx(3, "libusb_kernel_driver_active() returned %d: %s",
				ret, libusb_strerror((enum libusb_error)ret));
		}
#endif

#if (defined HAVE_LIBUSB_DETACH_KERNEL_DRIVER) || (defined HAVE_LIBUSB_DETACH_KERNEL_DRIVER_NP)
		/* Then, try the explicit detach method.
		 * This function is available on FreeBSD 10.1-10.3 */
		retries = MAX_RETRY;
#ifdef WIN32
		/* TODO: Align with libusb1 - initially from Windows branch made against libusb0 */
		libusb_set_configuration(udev, 1);
#endif

		while ((ret = libusb_claim_interface(udev, usb_subdriver.hid_rep_index)) != LIBUSB_SUCCESS) {
			upsdebugx(2, "failed to claim USB device: %s",
				libusb_strerror((enum libusb_error)ret));

			if (ret == LIBUSB_ERROR_BUSY && testvar("allow_duplicates")) {
				upsdebugx(2, "Configured to allow_duplicates so looking for another similar device");
				goto next_device;
			}

# ifdef HAVE_LIBUSB_DETACH_KERNEL_DRIVER
			if ((ret = libusb_detach_kernel_driver(udev, usb_subdriver.hid_rep_index)) != LIBUSB_SUCCESS) {
# else /* if defined HAVE_LIBUSB_DETACH_KERNEL_DRIVER_NP) */
			if ((ret = libusb_detach_kernel_driver_np(udev, usb_subdriver.hid_rep_index)) != LIBUSB_SUCCESS) {
# endif
				if (ret == LIBUSB_ERROR_NOT_FOUND) {
					/* logged as "Entity not found" if this persists */
					upsdebugx(2, "Kernel driver already detached");
				} else {
					upsdebugx(1, "failed to detach kernel driver from USB device: %s",
						libusb_strerror((enum libusb_error)ret));
				}
			} else {
				upsdebugx(2, "detached kernel driver from USB device...");
			}

			if (retries-- > 0) {
				continue;
			}

			libusb_free_config_descriptor(conf_desc);
			libusb_free_device_list(devlist, 1);
			fatalx(EXIT_FAILURE,
				"Can't claim USB device [%04x:%04x]@%d/%d/%d: %s",
				curDevice->VendorID, curDevice->ProductID,
				usb_subdriver.usb_config_index,
				usb_subdriver.hid_rep_index,
				usb_subdriver.hid_desc_index,
				libusb_strerror((enum libusb_error)ret));
		}
#else
		if ((ret = libusb_claim_interface(udev, usb_subdriver.hid_rep_index)) != LIBUSB_SUCCESS ) {
			if (ret == LIBUSB_ERROR_BUSY && testvar("allow_duplicates")) {
				upsdebugx(2, "Configured to allow_duplicates so looking for another similar device");
				goto next_device;
			}

			libusb_free_config_descriptor(conf_desc);
			libusb_free_device_list(devlist, 1);
			fatalx(EXIT_FAILURE,
				"Can't claim USB device [%04x:%04x]@%d/%d/%d: %s",
				curDevice->VendorID, curDevice->ProductID,
				usb_subdriver.usb_config_index,
				usb_subdriver.hid_rep_index,
				usb_subdriver.hid_desc_index,
				libusb_strerror((enum libusb_error)ret));
		}
#endif
		/* if_claimed = 1; */
		upsdebugx(2, "Claimed interface %d successfully",
			usb_subdriver.hid_rep_index);

		nut_usb_set_altinterface(udev);

		if (!callback) {
			libusb_free_config_descriptor(conf_desc);
			libusb_free_device_list(devlist, 1);
			return 1;
		}

		if (!conf_desc) { /* ?? this should never happen */
			upsdebugx(2, "  Couldn't retrieve config descriptor [%04x:%04x]@%d",
				curDevice->VendorID, curDevice->ProductID,
				usb_subdriver.usb_config_index
			);
			goto next_device;
		}

		rdlen1 = -1;
		rdlen2 = -1;

		/* Get HID descriptor */

		/* FIRST METHOD: ask for HID descriptor directly. */
		/* libusb0: USB_ENDPOINT_IN + 1 */
		res = libusb_control_transfer(udev,
			LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_INTERFACE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			(LIBUSB_DT_HID << 8) + usb_subdriver.hid_desc_index,
			usb_subdriver.hid_rep_index,
			buf, 0x9, USB_TIMEOUT);

		if (res < 0) {
			upsdebugx(2, "Unable to get HID descriptor (%s)",
				libusb_strerror((enum libusb_error)res));
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
		upsdebugx(3, "HID descriptor length (method 1) %d", rdlen1);

		/* SECOND METHOD: find HID descriptor among "extra" bytes of
		   interface descriptor, i.e., bytes tucked onto the end of
		   descriptor 2. */

		/* Note: on some broken UPS's (e.g. Tripp Lite Smart1000LCD),
			only this second method gives the correct result */

		/* for now, we always assume configuration 0, interface 0,
		   altsetting 0, as above. */

		if_desc = &(conf_desc->interface[usb_subdriver.hid_rep_index].altsetting[0]);
		for (i = 0; i < if_desc->extra_length; i += if_desc->extra[i]) {
			upsdebugx(4, "i=%d, extra[i]=%02x, extra[i+1]=%02x", i,
				if_desc->extra[i], if_desc->extra[i+1]);
			if (i+9 <= if_desc->extra_length && if_desc->extra[i] >= 9 && if_desc->extra[i+1] == 0x21) {
				p = &if_desc->extra[i];
				upsdebug_hex(3, "HID descriptor, method 2", p, 9);
				rdlen2 = ((uint8_t)p[7]) | (((uint8_t)p[8]) << 8);
				break;
			}
		}

		/* we can now free the config descriptor */
		libusb_free_config_descriptor(conf_desc);

		if (rdlen2 < -1) {
			upsdebugx(2, "Warning: HID descriptor, method 2 failed");
		}
		upsdebugx(3, "HID descriptor length (method 2) %d", rdlen2);

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
				"(Reportlen = %d vs. %d)", rdlen1, rdlen2);
		}

		upsdebugx(2, "HID descriptor length %d", rdlen);

		if (rdlen > (int)sizeof(rdbuf)) {
			upsdebugx(2, "HID descriptor too long %d (max %d)",
				rdlen, (int)sizeof(rdbuf));
			goto next_device;
		}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
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
		if ((uintmax_t)rdlen > UINT16_MAX) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif
			upsdebugx(2, "HID descriptor too long %d (max %u)",
				rdlen, UINT16_MAX);
			goto next_device;
		}

		/* libusb0: USB_ENDPOINT_IN + 1 */
		res = libusb_control_transfer(udev,
			LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_INTERFACE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			(LIBUSB_DT_REPORT << 8) + usb_subdriver.hid_desc_index,
			usb_subdriver.hid_rep_index,
			rdbuf, (uint16_t)rdlen, USB_TIMEOUT);

		if (res < 0)
		{
			upsdebug_with_errno(2, "Unable to get Report descriptor");
			goto next_device;
		}

		if (res < rdlen)
		{
#ifndef WIN32
			upsdebugx(2, "Warning: report descriptor too short "
				"(expected %d, got %d)", rdlen, res);
#else
			/* https://github.com/networkupstools/nut/issues/1690#issuecomment-1455206002 */
			upsdebugx(0, "Warning: report descriptor too short "
				"(expected %d, got %d)", rdlen, res);
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

		if (rdlen < USB_CTRL_CHARBUFSIZE_MIN
		||  (uintmax_t)rdlen > (uintmax_t)USB_CTRL_CHARBUFSIZE_MAX
		) {
			upsdebugx(2,
				"Report descriptor length is out of range on this device: "
				"should be %" PRIdMAX " < %d < %" PRIuMAX,
					(intmax_t)USB_CTRL_CHARBUFSIZE_MIN, rdlen,
					(uintmax_t)USB_CTRL_CHARBUFSIZE_MAX);
			goto next_device;
		}

		res = callback(udev, curDevice, rdbuf, (usb_ctrl_charbufsize)rdlen);
		if (res < 1) {
			upsdebugx(2, "Caller doesn't like this device");
			goto next_device;
		}

		upsdebugx(2, "Report descriptor retrieved (Reportlen = %d)", rdlen);
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
		libusb_free_device_list(devlist, 1);

		return rdlen;

		next_device:
			/* usb_release_interface() sometimes blocks and goes
			into uninterruptible sleep.  So don't do it. */
			/* if (if_claimed)
				libusb_release_interface(udev, usb_subdriver.hid_rep_index); */
			libusb_close(udev);
	}

	*udevp = NULL;
	libusb_free_device_list(devlist, 1);
	upsdebugx(2, "libusb1: No appropriate HID device found");
	fflush(stdout);

	if (devcount < 1) {
		upslogx(LOG_WARNING,
			"libusb1: Could not open any HID devices: "
			"no USB buses found");
	}
	else
	if (count_open_errors > 0
	||  count_open_errors == count_open_EACCESS
	) {
		upslogx(LOG_WARNING,
			"libusb1: Could not open any HID devices: "
			"insufficient permissions on everything");
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
#if 0
	/* FIXME: not sure how to map these ones! */
	case LIBUSB_ERROR_INVALID_PARAM: /** Invalid parameter */
	/** System call interrupted (perhaps due to signal) */
	case LIBUSB_ERROR_INTERRUPTED:
	/** Insufficient memory */
	case LIBUSB_ERROR_NO_MEM:
#endif

	case LIBUSB_ERROR_BUSY:	     /** Resource busy */
	case LIBUSB_ERROR_NO_DEVICE: /** No such device (it may have been disconnected) */
	case LIBUSB_ERROR_ACCESS:    /** Access denied (insufficient permissions) */
	case LIBUSB_ERROR_IO:        /** Input/output error */
	case LIBUSB_ERROR_NOT_FOUND: /** Entity not found */
	case LIBUSB_ERROR_PIPE:	     /** Pipe error */
	/** Operation not supported or unimplemented on this platform */
	case LIBUSB_ERROR_NOT_SUPPORTED:
		upslogx(LOG_DEBUG, "%s: %s", desc, libusb_strerror((enum libusb_error)ret));
		return ret;

	case LIBUSB_ERROR_TIMEOUT:	 /** Operation timed out */
		upsdebugx(2, "%s: Connection timed out", desc);
		return 0;

#ifndef WIN32
	case LIBUSB_ERROR_OVERFLOW:	 /** Overflow */
# ifdef EPROTO
/* FIXME: not sure how to map this one! */
	case -EPROTO:	/* Protocol error */
# endif
		upsdebugx(2, "%s: %s", desc, libusb_strerror((enum libusb_error)ret));
		return 0;
#endif /* WIN32 */

	case LIBUSB_ERROR_OTHER:     /** Other error */
	default:                     /** Undetermined, log only */
		upslogx(LOG_DEBUG, "%s: %s", desc, libusb_strerror((enum libusb_error)ret));
		return 0;
	}
}

/* return the report of ID=type in report
 * return -1 on failure, report length on success
 */

/* Expected evaluated types for the API:
 * static int nut_libusb_get_report(libusb_device_handle *udev,
 *	int ReportId, unsigned char *raw_buf, int ReportSize)
 */
static int nut_libusb_get_report(
	libusb_device_handle *udev,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	int	ret;

	upsdebugx(4, "Entering libusb_get_report");

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
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
	if (!udev
	|| ReportId < 0 || (uintmax_t)ReportId > UINT16_MAX
	|| ReportSize < 0 || (uintmax_t)ReportSize > UINT16_MAX
	) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif
		return 0;
	}

	/* libusb0: USB_ENDPOINT_IN + USB_TYPE_CLASS + USB_RECIP_INTERFACE */
	ret = libusb_control_transfer(udev,
		LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE,
		0x01, /* HID_REPORT_GET */
		(uint16_t)ReportId + (0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		usb_subdriver.hid_rep_index,
		raw_buf, (uint16_t)ReportSize, USB_TIMEOUT);

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == LIBUSB_ERROR_PIPE) {
		return 0;
	}

	return nut_libusb_strerror(ret, __func__);
}

/* Expected evaluated types for the API:
 * static int nut_libusb_set_report(libusb_device_handle *udev,
 *	int ReportId, unsigned char *raw_buf, int ReportSize)
 */
static int nut_libusb_set_report(
	libusb_device_handle *udev,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	int	ret;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
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
	if (!udev
	|| ReportId < 0 || (uintmax_t)ReportId > UINT16_MAX
	|| ReportSize < 0 || (uintmax_t)ReportSize > UINT16_MAX
	) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif
		return 0;
	}

	/* libusb0: USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE */
	ret = libusb_control_transfer(udev,
		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE,
		0x09, /* HID_REPORT_SET = 0x09*/
		(uint16_t)ReportId + (0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		usb_subdriver.hid_rep_index,
		raw_buf, (uint16_t)ReportSize, USB_TIMEOUT);

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == LIBUSB_ERROR_PIPE) {
		return 0;
	}

	return nut_libusb_strerror(ret, __func__);
}

/* Expected evaluated types for the API:
 * static int nut_libusb_get_string(libusb_device_handle *udev,
 *	int StringIdx, char *buf, int buflen)
 */
static int nut_libusb_get_string(
	libusb_device_handle *udev,
	usb_ctrl_strindex StringIdx,
	char *buf,
	usb_ctrl_charbufsize buflen)
{
	int ret;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
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
	if (!udev
	|| StringIdx < 0 || (uintmax_t)StringIdx > UINT8_MAX
	|| buflen < 0 || (uintmax_t)buflen > (uintmax_t)INT_MAX
	) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif
		return -1;
	}

	ret = libusb_get_string_descriptor_ascii(udev, (uint8_t)StringIdx,
		(unsigned char*)buf, (int)buflen);

	return nut_libusb_strerror(ret, __func__);
}

/* Expected evaluated types for the API:
 * static int nut_libusb_get_interrupt(libusb_device_handle *udev,
 *	unsigned char *buf, int bufsize, int timeout)
 */
static int nut_libusb_get_interrupt(
	libusb_device_handle *udev,
	usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize,
	usb_ctrl_timeout_msec timeout)
{
	int ret, tmpbufsize;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
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
	if (!udev
	||  bufsize < 0 || (uintmax_t)bufsize > (uintmax_t)INT_MAX
	) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif
		return -1;
	}

	/* NOTE: With all the fuss about word sized arguments,
	 * the libusb_interrupt_transfer() lengths are about ints:
	 * int LIBUSB_CALL libusb_interrupt_transfer(libusb_device_handle *dev_handle,
	 *	unsigned char endpoint, unsigned char *data, int length,
	 *	int *actual_length, unsigned int timeout);
	 */
	tmpbufsize = (int)bufsize;

	/* FIXME: hardcoded interrupt EP => need to get EP descr for IF descr */
	/* ret = libusb_interrupt_transfer(udev, 0x81, buf, bufsize, &bufsize, timeout); */
	/* libusb0: ret = usb_interrupt_read(udev, USB_ENDPOINT_IN + usb_subdriver.hid_ep_in, (char *)buf, bufsize, timeout); */
	/* Interrupt EP is LIBUSB_ENDPOINT_IN with offset defined in hid_ep_in, which is 0 by default, unless overridden in subdriver. */
	ret = libusb_interrupt_transfer(udev,
		LIBUSB_ENDPOINT_IN + usb_subdriver.hid_ep_in,
		(unsigned char *)buf, tmpbufsize, &tmpbufsize, timeout);

	/* Clear stall condition */
	if (ret == LIBUSB_ERROR_PIPE) {
		ret = libusb_clear_halt(udev, 0x81);
	}

	/* In case of success, return the operation size, as done with libusb 0.1 */
	if (ret == LIBUSB_SUCCESS) {
		if (tmpbufsize < 0
		||  (uintmax_t)tmpbufsize > (uintmax_t)USB_CTRL_CHARBUFSIZE_MAX
		) {
			return -1;
		}
		ret = (usb_ctrl_charbufsize)bufsize;
	}

	return nut_libusb_strerror(ret, __func__);
}

static void nut_libusb_close(libusb_device_handle *udev)
{
	if (!udev) {
		return;
	}

	/* usb_release_interface() sometimes blocks and goes
	 * into uninterruptible sleep.  So don't do it.
	 */
	/* libusb_release_interface(udev, usb_subdriver.hid_rep_index); */
	libusb_close(udev);
	libusb_exit(NULL);
}

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
