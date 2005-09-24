/*!
 * @file libusb.c
 * @brief HID Library - Generic USB backend for Generic HID Access (using MGE HIDParser)
 *
 * @author Copyright (C) 2003
 *	Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *	Philippe Marzouk <philm@users.sourceforge.net> (dump_hex())
 *      2005 Peter Selinger <selinger@users.sourceforge.net>
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "libhid.h"

#include "libusb.h"
#include "config.h" /* for LIBUSB_HAS_DETACH_KRNL_DRV flag */

#include "common.h" /* for xmalloc prototype */

/* #define USB_TIMEOUT 5000 */
#define USB_TIMEOUT 4000

/* TODO: rework all that */
void upsdebugx(int level, const char *fmt, ...);
#define TRACE upsdebugx

/* HID descriptor, completed with desc{type,len} */
struct my_usb_hid_descriptor {
        u_int8_t  bLength;
        u_int8_t  bDescriptorType;
        u_int16_t bcdHID;
        u_int8_t  bCountryCode;
        u_int8_t  bNumDescriptors;
        u_int8_t  bReportDescriptorType;
        u_int16_t wDescriptorLength;
};

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

#define usb_control_msg         typesafe_control_msg

/* return report descriptor on success, NULL otherwise */
/* mode: MODE_OPEN for the 1rst time, MODE_REOPEN to skip getting
    report descriptor (the longer part). On success, fill in the
    curDevice structure and return the report descriptor length. On
    failure, return -1. Note: ReportDesc must point to a large enough
    buffer. There's no way to know the size ahead of time. Matcher is
    a linked list of matchers (see libhid.h), and the opened device
    must match all of them. */
int libusb_open(usb_dev_handle **udevp, HIDDevice *curDevice, HIDDeviceMatcher_t *matcher, unsigned char *ReportDesc, int mode)
{
	int found = 0;
#if LIBUSB_HAS_DETACH_KRNL_DRV
	int retries;
#endif
	struct my_usb_hid_descriptor *desc;
	HIDDeviceMatcher_t *m;
	struct usb_device *dev;                                                
	struct usb_bus *bus;                                                   
	usb_dev_handle *udevx;
	
	int ret, res; 
	unsigned char buf[20];
	char string[256];

	/* libusb base init */
	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus && !found; bus = bus->next) {
		for (dev = bus->devices; dev && !found; dev = dev->next) {
			TRACE(2, "Checking device (%04X/%04X) (%s/%s)", dev->descriptor.idVendor, dev->descriptor.idProduct, bus->dirname, dev->filename);
			
			/* supported vendors are now checked by the
			   supplied matcher */

			/* open the device */
			*udevp = udevx = usb_open(dev);
			if (!udevx) {
				TRACE(2, "Failed to open device, skipping. (%s)", usb_strerror());
				continue;
			} 

			/* collect the identifying information of this
			   device. Note that this is safe, because
			   there's no need to claim an interface for
			   this (and therefore we do not yet need to
			   detach any kernel drivers). */
			
			curDevice->VendorID = dev->descriptor.idVendor;
			curDevice->ProductID = dev->descriptor.idProduct;
			curDevice->Vendor = NULL;
			curDevice->Product = NULL;
			curDevice->Serial = NULL;
			curDevice->Bus = bus->dirname;
			
			if (dev->descriptor.iManufacturer) {
				ret = usb_get_string_simple(udevx, dev->descriptor.iManufacturer, string, sizeof(string));
				if (ret > 0) {
					curDevice->Vendor = strdup(string);
				}
			}

			if (dev->descriptor.iProduct) {
				ret = usb_get_string_simple(udevx, dev->descriptor.iProduct, string, sizeof(string));
				if (ret > 0) {
					curDevice->Product = strdup(string);
				}
			}

			if (dev->descriptor.iSerialNumber) {
				ret = usb_get_string_simple(udevx, dev->descriptor.iSerialNumber, string, sizeof(string));
				if (ret > 0) {
					curDevice->Serial = strdup(string);
				}
			}

			TRACE(2, "- VendorID: %04x", curDevice->VendorID);
			TRACE(2, "- ProductID: %04x", curDevice->ProductID);
			TRACE(2, "- Manufacturer: %s", curDevice->Vendor ? curDevice->Vendor : "unknown");
			TRACE(2, "- Product: %s", curDevice->Product ? curDevice->Product : "unknown");
			TRACE(2, "- Serial Number: %s", curDevice->Serial ? curDevice->Serial : "unknown");
			TRACE(2, "- Bus: %s", curDevice->Bus ? curDevice->Bus : "unknown");

			TRACE(2, "Trying to match device");
			for (m = matcher; m; m=m->next) {
				ret = matches(m, curDevice);
				if (ret==0) {
					TRACE(2, "Device does not match - skipping");
					goto next_device;
				} else if (ret==-1) {
					fatalx("matcher: %s", strerror(errno));
					goto next_device;
				} else if (ret==-2) {
					TRACE(2, "matcher: unspecified error");
					goto next_device;
				}
			}
			TRACE(2, "Device matches");
			
			/* Now we have matched the device we wanted. Claim it. */

#if LIBUSB_HAS_DETACH_KRNL_DRV
			/* this method requires at least libusb 0.1.8:
			 * it force device claiming by unbinding
			 * attached driver... From libhid */
			retries = 3;
			while (usb_claim_interface(udevx, 0) != 0 && retries-- > 0) {
				
				TRACE(2, "failed to claim USB device, trying %d more time(s)...", retries);
				
				TRACE(2, "detaching kernel driver from USB device...");
				if (usb_detach_kernel_driver_np(udevx, 0) < 0) {
					TRACE(2, "failed to detach kernel driver from USB device...");
				}
				
				TRACE(2, "trying again to claim USB device...");
			}
#else
			if (usb_claim_interface(udevx, 0) < 0)
				TRACE(2, "failed to claim USB device...");
#endif
			
			/* set default interface */
			usb_set_altinterface(udevx, 0);
			
			if (mode == MODE_REOPEN) {
				return 1; 
			}

			/* Get HID descriptor */
			desc = (struct my_usb_hid_descriptor *)buf;
			/* res = usb_get_descriptor(udev, USB_DT_HID, 0, buf, 0x9); */
			res = usb_control_msg(udevx, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
					      (USB_DT_HID << 8) + 0, 0, buf, 0x9, USB_TIMEOUT);
			
			if (res < 0) {
				TRACE(2, "Unable to get HID descriptor (%s)", usb_strerror());
				goto next_device;
			} else if (res < 9) {
				TRACE(2, "HID descriptor too short (expected %d, got %d)", 8, res);
				goto next_device;
			} 
			
                        /* USB_LE16_TO_CPU(desc->wDescriptorLength); */
			desc->wDescriptorLength = buf[7] | (buf[8] << 8);
			TRACE(2, "HID descriptor retrieved (Reportlen = %u)", desc->wDescriptorLength);
			
			if (!dev->config) {
				TRACE(2, "  Couldn't retrieve descriptors");
				goto next_device;
			}
			
			/* res = usb_get_descriptor(udev, USB_DT_REPORT, 0, bigbuf, desc->wDescriptorLength); */
			res = usb_control_msg(udevx, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
					      (USB_DT_REPORT << 8) + 0, 0, ReportDesc, 
					      desc->wDescriptorLength, USB_TIMEOUT);
			if (res >= desc->wDescriptorLength) 
			{
				TRACE(2, "Report descriptor retrieved (Reportlen = %u)", desc->wDescriptorLength);
				TRACE(2, "Found HID device");
				fflush(stdout);

				return desc->wDescriptorLength;
			}
			if (res < 0)
			{
				TRACE(2, "Unable to get Report descriptor (%d)", res);
			}
			else
			{
				TRACE(2, "Report descriptor too short (expected %d, got %d)", desc->wDescriptorLength, res);
			}
		next_device:
			usb_close(udevx);
			udevx = NULL;
		}
	}
	TRACE(2, "No appropriate HID device found");
	fflush(stdout);

	return -1;
}

/* return the report of ID=type in report 
 * return -1 on failure, report length on success
 */

int libusb_get_report(usb_dev_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize )
{
	TRACE(4, "Entering libusb_get_report");

	if (udev != NULL)
	{
		return usb_control_msg(udev, 
			 USB_ENDPOINT_IN + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			 0x01, /* HID_REPORT_GET */
			 ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
			 0, raw_buf, ReportSize, USB_TIMEOUT);
	}
	else
		return 0;
}


int libusb_set_report(usb_dev_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize )
{
	if (udev != NULL)
	{
		return usb_control_msg(udev, 
			 USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			 0x09, /* HID_REPORT_SET = 0x09*/
			 ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
			 0, raw_buf, ReportSize, USB_TIMEOUT);
	}
	else
		return 0;
}

int libusb_get_string(usb_dev_handle *udev, int StringIdx, char *string)
{
  int ret = -1;	

  if (udev != NULL)
	{
	  ret = usb_get_string_simple(udev, StringIdx, string, 20); /* sizeof(string)); */
	  if (ret > 0)
		{
		  TRACE(2, "-> String: %s (len = %i/%i)", string, ret, sizeof(string));
		}
	  else
		TRACE(2, "- Unable to fetch string");
	}

  return ret;
}

int libusb_get_interrupt(usb_dev_handle *udev, unsigned char *buf, int bufsize, int timeout)
{
  int ret = -1;

  if (udev != NULL)
	{
	  /* FIXME: hardcoded interrupt EP => need to get EP descr for IF descr */
	  ret = usb_interrupt_read(udev, 0x81, buf, bufsize, timeout);
	  if (ret > 0)
		TRACE(5, " ok");
	  else
		TRACE(5, " none (%i)", ret);
	}

  return ret;
}

void libusb_close(usb_dev_handle **udevp)
{
	if (*udevp != NULL)
	{
	        /* usb_release_interface() sometimes blocks and goes
	           into uninterruptible sleep.  So don't do it. */
	        /* usb_release_interface(udev, 0); */
		usb_close(*udevp);
	}
	*udevp = NULL;
}
