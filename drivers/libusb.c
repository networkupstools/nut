/*!
 * @file libusb.c
 * @brief HID Library - Generic USB communication sub driver (using libusb)
 *
 * @author Copyright (C)
 *  2003 - 2007 Arnaud Quette <aquette.dev@gmail.com>
 *  2005 - 2007 Peter Selinger <selinger@users.sourceforge.net>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 *
 *      The logic of this file is ripped from mge-shut driver (also from
 *      Arnaud Quette), which is a "HID over serial link" UPS driver for
 *      Network UPS Tools <http://www.networkupstools.org/>
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

#include "config.h" /* for LIBUSB_HAS_DETACH_KRNL_DRV flag */
#include "common.h" /* for xmalloc, upsdebugx prototypes */
#include "usb-common.h"
#include "libusb.h"

/* USB standard state 5000, but we've decreased it to
 * improve reactivity */
#define USB_TIMEOUT 4000

#define USB_DRIVER_NAME		"USB communication driver"
#define USB_DRIVER_VERSION	"0.31"

/* driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	USB_DRIVER_NAME,
	USB_DRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

#define MAX_REPORT_SIZE         0x1800

static void libusb_close(usb_dev_handle *udev);

/* From usbutils: workaround libusb API goofs:  "byte" should never be sign extended;
 * using "char" is trouble.  Likewise, sizes should never be negative.
 */

static inline int typesafe_control_msg(usb_dev_handle *dev,
        unsigned char requesttype, unsigned char request,
        int value, int index,
        unsigned char *bytes, unsigned size, int timeout)
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

#define usb_control_msg         typesafe_control_msg

/* On success, fill in the curDevice structure and return the report
 * descriptor length. On failure, return -1.
 * Note: When callback is not NULL, the report descriptor will be
 * passed to this function together with the udev and USBDevice_t
 * information. This callback should return a value > 0 if the device
 * is accepted, or < 1 if not. If it isn't accepted, the next device
 * (if any) will be tried, until there are no more devices left.
 */
static int libusb_open(usb_dev_handle **udevp, USBDevice_t *curDevice, USBDeviceMatcher_t *matcher,
	int (*callback)(usb_dev_handle *udev, USBDevice_t *hd, unsigned char *rdbuf, int rdlen))
{
#if LIBUSB_HAS_DETACH_KRNL_DRV
	int retries;
#endif
	int rdlen1, rdlen2; /* report descriptor length, method 1+2 */
	USBDeviceMatcher_t *m;
	struct usb_device *dev;
	struct usb_bus *bus;
	usb_dev_handle *udev;
	struct usb_interface_descriptor *iface;
	
	int ret, res; 
	unsigned char buf[20];
	unsigned char *p;
	char string[256];
	int i;

	/* report descriptor */
	unsigned char	rdbuf[MAX_REPORT_SIZE];
	int		rdlen;

	/* libusb base init */
	usb_init();
	usb_find_busses();
	usb_find_devices();

#ifndef __linux__ /* SUN_LIBUSB (confirmed to work on Solaris and FreeBSD) */
	/* Causes a double free corruption in linux if device is detached! */
	libusb_close(*udevp);
#endif

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			upsdebugx(2, "Checking device (%04X/%04X) (%s/%s)", dev->descriptor.idVendor,
				dev->descriptor.idProduct, bus->dirname, dev->filename);
			
			/* supported vendors are now checked by the
			   supplied matcher */

			/* open the device */
			*udevp = udev = usb_open(dev);
			if (!udev) {
				upsdebugx(2, "Failed to open device, skipping. (%s)", usb_strerror());
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
			memset(curDevice, '\0', sizeof(*curDevice));

			curDevice->VendorID = dev->descriptor.idVendor;
			curDevice->ProductID = dev->descriptor.idProduct;
			curDevice->Bus = strdup(bus->dirname);
			
			if (dev->descriptor.iManufacturer) {
				ret = usb_get_string_simple(udev, dev->descriptor.iManufacturer,
					string, sizeof(string));
				if (ret > 0) {
					curDevice->Vendor = strdup(string);
				}
			}

			if (dev->descriptor.iProduct) {
				ret = usb_get_string_simple(udev, dev->descriptor.iProduct,
					string, sizeof(string));
				if (ret > 0) {
					curDevice->Product = strdup(string);
				}
			}

			if (dev->descriptor.iSerialNumber) {
				ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber,
					string, sizeof(string));
				if (ret > 0) {
					curDevice->Serial = strdup(string);
				}
			}

			upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
			upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
			upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor ? curDevice->Vendor : "unknown");
			upsdebugx(2, "- Product: %s", curDevice->Product ? curDevice->Product : "unknown");
			upsdebugx(2, "- Serial Number: %s", curDevice->Serial ? curDevice->Serial : "unknown");
			upsdebugx(2, "- Bus: %s", curDevice->Bus ? curDevice->Bus : "unknown");

			upsdebugx(2, "Trying to match device");
			for (m = matcher; m; m=m->next) {
				ret = matches(m, curDevice);
				if (ret==0) {
					upsdebugx(2, "Device does not match - skipping");
					goto next_device;
				} else if (ret==-1) {
					fatal_with_errno(EXIT_FAILURE, "matcher");
					goto next_device;
				} else if (ret==-2) {
					upsdebugx(2, "matcher: unspecified error");
					goto next_device;
				}
			}
			upsdebugx(2, "Device matches");
			
			/* Now we have matched the device we wanted. Claim it. */

#if LIBUSB_HAS_DETACH_KRNL_DRV
			/* this method requires at least libusb 0.1.8:
			 * it force device claiming by unbinding
			 * attached driver... From libhid */
			retries = 3;
			while (usb_claim_interface(udev, 0) < 0) {
				
				upsdebugx(2, "failed to claim USB device: %s", usb_strerror());
				
				if (usb_detach_kernel_driver_np(udev, 0) < 0) {
					upsdebugx(2, "failed to detach kernel driver from USB device: %s", usb_strerror());
				} else {
					upsdebugx(2, "detached kernel driver from USB device...");
				}

				if (retries-- > 0) {
					continue;
				}

				fatalx(EXIT_FAILURE, "Can't claim USB device [%04x:%04x]: %s", curDevice->VendorID, curDevice->ProductID, usb_strerror());
			}
#else
			if (usb_claim_interface(udev, 0) < 0) {
				fatalx(EXIT_FAILURE, "Can't claim USB device [%04x:%04x]: %s", curDevice->VendorID, curDevice->ProductID, usb_strerror());
			}
#endif
			
			/* set default interface */
			usb_set_altinterface(udev, 0);
			
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
			/* res = usb_get_descriptor(udev, USB_DT_HID, 0, buf, 0x9); */
			res = usb_control_msg(udev, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
					      (USB_DT_HID << 8) + 0, 0, buf, 0x9, USB_TIMEOUT);
			
			if (res < 0) {
				upsdebugx(2, "Unable to get HID descriptor (%s)", usb_strerror());
			} else if (res < 9) {
				upsdebugx(2, "HID descriptor too short (expected %d, got %d)", 8, res);
			} else {

				upsdebug_hex(3, "HID descriptor, method 1", buf, 9);

				rdlen1 = buf[7] | (buf[8] << 8);
			}			

			if (rdlen1 < -1) {
				upsdebugx(2, "Warning: HID descriptor, method 1 failed");
			}

			/* SECOND METHOD: find HID descriptor among "extra" bytes of
			   interface descriptor, i.e., bytes tucked onto the end of
			   descriptor 2. */

			/* Note: on some broken UPS's (e.g. Tripp Lite Smart1000LCD),
				only this second method gives the correct result */
			
			/* for now, we always assume configuration 0, interface 0,
			   altsetting 0, as above. */
			iface = &dev->config[0].interface[0].altsetting[0];
			for (i=0; i<iface->extralen; i+=iface->extra[i]) {
				upsdebugx(4, "i=%d, extra[i]=%02x, extra[i+1]=%02x", i,
					iface->extra[i], iface->extra[i+1]);
				if (i+9 <= iface->extralen && iface->extra[i] >= 9 && iface->extra[i+1] == 0x21) {
					p = &iface->extra[i];
					upsdebug_hex(3, "HID descriptor, method 2", p, 9);
					rdlen2 = p[7] | (p[8] << 8);
					break;
				}
			}

			if (rdlen2 < -1) {
				upsdebugx(2, "Warning: HID descriptor, method 2 failed");
			}

			/* when available, always choose the second value, as it
				seems to be more reliable (it is the one reported e.g. by
				lsusb). Note: if the need arises, can change this to use
				the maximum of the two values instead. */
			rdlen = rdlen2 >= 0 ? rdlen2 : rdlen1;

			if (rdlen < 0) {
				upsdebugx(2, "Unable to retrieve any HID descriptor");
				goto next_device;
			}
			if (rdlen1 >= 0 && rdlen2 >= 0 && rdlen1 != rdlen2) {
				upsdebugx(2, "Warning: two different HID descriptors retrieved (Reportlen = %u vs. %u)", rdlen1, rdlen2);
			}

			upsdebugx(2, "HID descriptor length %d", rdlen);

			if (rdlen > (int)sizeof(rdbuf)) {
				upsdebugx(2, "HID descriptor too long %d (max %d)", rdlen, sizeof(rdbuf));
				goto next_device;
			}

			/* res = usb_get_descriptor(udev, USB_DT_REPORT, 0, bigbuf, rdlen); */
			res = usb_control_msg(udev, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
				(USB_DT_REPORT << 8) + 0, 0, rdbuf, rdlen, USB_TIMEOUT);

			if (res < 0)
			{
				upsdebug_with_errno(2, "Unable to get Report descriptor");
				goto next_device;
			}

			if (res < rdlen)
			{
				upsdebugx(2, "Warning: report descriptor too short (expected %d, got %d)", rdlen, res);
				rdlen = res; /* correct rdlen if necessary */
			}

			res = callback(udev, curDevice, rdbuf, rdlen);
			if (res < 1) {
				upsdebugx(2, "Caller doesn't like this device");
				goto next_device;
			}

			upsdebugx(2, "Report descriptor retrieved (Reportlen = %u)", rdlen);
			upsdebugx(2, "Found HID device");
			fflush(stdout);

			return rdlen;

		next_device:
			usb_close(udev);
		}
	}

	*udevp = NULL;
	upsdebugx(2, "No appropriate HID device found");
	fflush(stdout);

	return -1;
}

/* return the report of ID=type in report 
 * return -1 on failure, report length on success
 */

static int libusb_get_report(usb_dev_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize )
{
	upsdebugx(4, "Entering libusb_get_report");

	if (!udev) {
		return 0;
	}

	return usb_control_msg(udev,
		 USB_ENDPOINT_IN + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
		 0x01, /* HID_REPORT_GET */
		 ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		 0, raw_buf, ReportSize, USB_TIMEOUT);
}


static int libusb_set_report(usb_dev_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize )
{
	if (!udev) {
		return 0;
	}

	return usb_control_msg(udev,
		 USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
		 0x09, /* HID_REPORT_SET = 0x09*/
		 ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		 0, raw_buf, ReportSize, USB_TIMEOUT);
}

static int libusb_get_string(usb_dev_handle *udev, int StringIdx, char *buf, size_t buflen)
{
	int ret;

	if (!udev) {
		return -1;
	}

	ret = usb_get_string_simple(udev, StringIdx, buf, buflen);
	if (ret > 0) {
		upsdebugx(5, "-> String: %s (len = %i/%i)", buf, ret, buflen);
	} else {
		upsdebugx(2, "- Unable to fetch string %d", StringIdx);
	}

	return ret;
}

static int libusb_get_interrupt(usb_dev_handle *udev, unsigned char *buf, int bufsize, int timeout)
{
	int ret;

	if (!udev) {
		return -1;
	}
#ifdef SUN_LIBUSB
/*
	usleep(timeout * 1000);
 */
	return 0;
#else
	/* FIXME: hardcoded interrupt EP => need to get EP descr for IF descr */
	ret = usb_interrupt_read(udev, 0x81, (char *)buf, bufsize, timeout);
	if (ret > 0) {
		upsdebugx(6, " ok");
	} else {
		upsdebugx(6, " none (%i)", ret);
	}
	return ret;
#endif
}

static void libusb_close(usb_dev_handle *udev)
{
	if (!udev) {
		return;
	}

	/* usb_release_interface() sometimes blocks and goes
	into uninterruptible sleep.  So don't do it. */
	/* usb_release_interface(udev, 0); */
	usb_close(udev);
}

usb_communication_subdriver_t usb_subdriver = {
	USB_DRIVER_VERSION,
	USB_DRIVER_NAME,
	libusb_open,
	libusb_close,
	libusb_get_report,
	libusb_set_report,
	libusb_get_string,
	libusb_get_interrupt
};
