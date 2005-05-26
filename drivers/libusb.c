/*!
 * @file libusb.c
 * @brief HID Library - Generic USB backend for Generic HID Access (using MGE HIDParser)
 *
 * @author Copyright (C) 2003
 *	Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *	Philippe Marzouk <philm@users.sourceforge.net> (dump_hex())
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
#include "libhid.h"

#include "hid-usb.h"
#include "config.h" /* for LIBUSB_HAS_DETACH_KRNL_DRV flag */

#include "common.h" /* for xmalloc prototype */

usb_dev_handle *udev = NULL;
struct usb_device *dev;
struct usb_bus *bus;

/* #define USB_TIMEOUT 5000 */
#define USB_TIMEOUT 4000

/* FIXME! */
#define MGE_UPS_SYSTEMS		0x0463		/* All models */
#define APC					0x051d		/* All models */


/* TODO: rework all that */
extern void upsdebugx(int level, const char *fmt, ...);
#define TRACE upsdebugx

extern void usb_fetch_and_parse_descriptors(usb_dev_handle *udev);

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

/* return report descriptor on success, NULL otherwise */
/* mode: MODE_OPEN for the 1rst time, MODE_REOPEN to skip getting
    report descriptor (the longer part) */
int libusb_open(HIDDevice *curDevice, MatchFlags *flg, unsigned char *ReportDesc, int mode)
{
	int found = 0;
#if LIBUSB_HAS_DETACH_KRNL_DRV
	int retries = 3;
#endif
	struct my_usb_hid_descriptor *desc;

	int ret = -1, res; 
	char buf[20];

	/* libusb base init */
	usb_init();
	usb_find_busses();
	usb_find_devices();


	for (bus = usb_busses; bus && !found; bus = bus->next) {
		for (dev = bus->devices; dev && !found; dev = dev->next) {
			char string[256];
      
			TRACE(2, "Opening new device (%04X/%04X)",
				  dev->descriptor.idVendor, dev->descriptor.idProduct);
			udev = usb_open(dev);
			if (udev) {

			  /* Check the VendorID for matching flag */
			  /* FIXME: temporary method, not generic/flexible enough */
			  if ((dev->descriptor.idVendor == MGE_UPS_SYSTEMS)
				  || (dev->descriptor.idVendor == APC))
				{
				  TRACE(2, "Found 0x%x", dev->descriptor.idVendor);
				  if (mode == MODE_REOPEN)
					return 1;

				  curDevice->VendorID = dev->descriptor.idVendor;
				  curDevice->iProduct = dev->descriptor.iProduct;
				}
			  else
				{
				  usb_close(udev);
				  udev = NULL;
				  continue;
				}

			  /* set default interface and claim it */
			  usb_set_altinterface(udev, 0);

#if LIBUSB_HAS_DETACH_KRNL_DRV
			  /* this method requires libusb 0.1.8:
			   * it force device claiming by unbinding
			   * attached driver... From libhid */
			  while ((ret = usb_claim_interface(udev, 0)) != 0 && retries-- > 0) {
	  
				TRACE(2, "failed to claim USB device, trying %d more time(s)...", retries);
	  
				TRACE(2, "detaching kernel driver from USB device...");
				if (usb_detach_kernel_driver_np(udev, 0) < 0) {
				  TRACE(2, "failed to detach kernel driver from USB device...");
				}
	  
				TRACE(2, "trying again to claim USB device...");
			  }
#else
			  usb_claim_interface(udev, 0);
#endif
	
			  if (dev->descriptor.iManufacturer) {
				ret = usb_get_string_simple(udev, dev->descriptor.iManufacturer, 
											string, sizeof(string));
				if (ret > 0)
				  {
					TRACE(2, "- Manufacturer : %s", string);
					curDevice->Vendor = strdup(string);
					curDevice->Name = curDevice->Vendor; /* FIXME: cat Vendor+Prod?! */
				  }
				else
				  {
					TRACE(2, "- Unable to fetch manufacturer string");
					curDevice->Vendor = xmalloc(30);
					snprintf(curDevice->Vendor, 30, "Unknown vendor (0x%04x)",
							 dev->descriptor.idVendor);
				  }
			  }
	
			  if (dev->descriptor.iProduct) {
				ret = usb_get_string_simple(udev, dev->descriptor.iProduct,
											string, sizeof(string));
				if (ret > 0) {
				  TRACE(2, "- Product      : %s", string);
				  curDevice->Product = strdup(string);
				}
				else
				  {
					TRACE(2, "-	 Unable to fetch product string");
					curDevice->Product = xmalloc(30);
					snprintf(curDevice->Product, 30, "Unknown product (0x%04x)",
							 dev->descriptor.idProduct);
				  }
			  }
	
			  if (dev->descriptor.iSerialNumber) {
				ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber,
											string, sizeof(string));
				if (ret > 0)
				  {
					TRACE(2, "- Serial Number: %s", string);
					curDevice->Serial = strdup(string);
				  }
				else
				  TRACE(2, "- Unable to fetch serial number string");
			  } else
				TRACE(2, "- No serial number string");
	
			  /* Get HID descriptor */
			  desc = (struct my_usb_hid_descriptor *)buf;
			  /* res = usb_get_descriptor(udev, USB_DT_HID, 0, buf, 0x9); */
			  res = usb_control_msg(udev, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
									(USB_DT_HID << 8) + 0, 0, buf, 0x9, USB_TIMEOUT);
	
			  if (res < 9) {
				if (res < 0) {
				  TRACE(2, "Unable to get HID descriptor (%s)", usb_strerror());
				  usb_close(udev);
				  udev = NULL;
				  continue;
				}
				else
				  TRACE(2, "HID descriptor too short (expected %d, got %d)", 8, res);
			  }
			  else {
				/* USB_LE16_TO_CPU(desc->wDescriptorLength); */
				desc->wDescriptorLength = buf[7] + (buf[8]<<8);
				TRACE(2, "HID descriptor retrieved (Reportlen = %i)",
					  desc->wDescriptorLength);
			  }
	
			  if (!dev->config) {
				TRACE(2, "  Couldn't retrieve descriptors");
				usb_close(udev);
				udev = NULL;
				continue;
			  }

			  /* Skip getting Report descriptor upon reconnexion */
			  if (mode == MODE_OPEN)
				{      
				  /* res = usb_get_descriptor(udev, USB_DT_REPORT, 0, bigbuf, desc->wDescriptorLength); */
				  res = usb_control_msg(udev, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
										(USB_DT_REPORT << 8) + 0, 0, ReportDesc, 
										desc->wDescriptorLength, USB_TIMEOUT);
				  if (res < desc->wDescriptorLength) {
					if (res < 0)
					  {
						TRACE(2, "Unable to get Report descriptor (%d)", res);
					  }
					else
					  {
						TRACE(2, "Report descriptor too short (expected %d, got %d)", 
							  desc->wDescriptorLength, res);
					  }
					usb_close(udev);
					udev = NULL;
					continue;
				  }
				  else
					{
					  TRACE(2, "Report descriptor retrieved (Reportlen = %i)", 
							desc->wDescriptorLength);
					  ret = desc->wDescriptorLength;
					}
				}
      
			  /* we can break there, and be sure there's a HID device */
			  found = 1;
			  break;
			} /* if udev */
		}
	}
	TRACE(2, "found %i (%i)", found, ret);
	fflush(stdout);

	return ret;
}

/* return the report of ID=type in report 
 * return -1 on failure, report length on success
 */

int libusb_get_report(int ReportId, unsigned char *raw_buf, int ReportSize )
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


int libusb_set_report(int ReportId, unsigned char *raw_buf, int ReportSize )
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

int libusb_get_string(int StringIdx, char *string)
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

int libusb_get_interrupt(unsigned char *buf, int bufsize, int timeout)
{
  int ret = -1;

  if (udev != NULL)
	{
	  /* FIXME: hardcoded interrupt EP => need to get EP descr for IF descr */
	  ret = usb_interrupt_read(udev, 0x81, buf, bufsize, timeout);
	  if (ret > 0)
		TRACE(2, " ok\n");
	  else
		TRACE(2, " none (%i)\n", ret);
	}

  return ret;
}

void libusb_close(HIDDevice *curDevice)
{
	if (udev != NULL)
	{
		usb_release_interface(udev, 0);
		usb_close(udev);
	}
	udev = NULL;
}
