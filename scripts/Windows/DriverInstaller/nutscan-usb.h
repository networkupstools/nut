/* nutscan-usb
 *  Copyright (C) 2011 - Arnaud Quette <arnaud.quette@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef DEVSCAN_USB_H
#define DEVSCAN_USB_H

typedef struct {
	unsigned short	vendorID;
	unsigned short	productID;
	char*	driver_name;
} usb_device_id_t;

/* USB IDs device table */
static usb_device_id_t usb_device_table[] = {

	{ 0x0001, 0x0000, "blazer_usb" },
	{ 0x03f0, 0x1f01, "bcmxcp_usb" },
	{ 0x03f0, 0x1f02, "bcmxcp_usb" },
	{ 0x03f0, 0x1f06, "usbhid-ups" },
	{ 0x03f0, 0x1f08, "usbhid-ups" },
	{ 0x03f0, 0x1f09, "usbhid-ups" },
	{ 0x03f0, 0x1f0a, "usbhid-ups" },
	{ 0x03f0, 0x1fe0, "usbhid-ups" },
	{ 0x03f0, 0x1fe1, "usbhid-ups" },
	{ 0x0463, 0x0001, "usbhid-ups" },
	{ 0x0463, 0xffff, "usbhid-ups" },
	{ 0x047c, 0xffff, "usbhid-ups" },
	{ 0x050d, 0x0375, "usbhid-ups" },
	{ 0x050d, 0x0551, "usbhid-ups" },
	{ 0x050d, 0x0750, "usbhid-ups" },
	{ 0x050d, 0x0751, "usbhid-ups" },
	{ 0x050d, 0x0900, "usbhid-ups" },
	{ 0x050d, 0x0910, "usbhid-ups" },
	{ 0x050d, 0x0912, "usbhid-ups" },
	{ 0x050d, 0x0980, "usbhid-ups" },
	{ 0x050d, 0x1100, "usbhid-ups" },
	{ 0x051d, 0x0002, "usbhid-ups" },
	{ 0x051d, 0x0003, "usbhid-ups" },
	{ 0x0592, 0x0002, "bcmxcp_usb" },
	{ 0x05b8, 0x0000, "blazer_usb" },
	{ 0x0665, 0x5161, "blazer_usb" },
	{ 0x06da, 0x0002, "bcmxcp_usb" },
	{ 0x06da, 0x0003, "blazer_usb" },
	{ 0x06da, 0xffff, "usbhid-ups" },
	{ 0x075d, 0x0300, "usbhid-ups" },
	{ 0x0764, 0x0005, "usbhid-ups" },
	{ 0x0764, 0x0501, "usbhid-ups" },
	{ 0x0764, 0x0601, "usbhid-ups" },
	{ 0x0925, 0x1234, "richcomm_usb" },
	{ 0x09ae, 0x0001, "tripplite_usb" },
	{ 0x09ae, 0x1003, "usbhid-ups" },
	{ 0x09ae, 0x1007, "usbhid-ups" },
	{ 0x09ae, 0x1008, "usbhid-ups" },
	{ 0x09ae, 0x1009, "usbhid-ups" },
	{ 0x09ae, 0x1010, "usbhid-ups" },
	{ 0x09ae, 0x2005, "usbhid-ups" },
	{ 0x09ae, 0x2007, "usbhid-ups" },
	{ 0x09ae, 0x2008, "usbhid-ups" },
	{ 0x09ae, 0x2009, "usbhid-ups" },
	{ 0x09ae, 0x2010, "usbhid-ups" },
	{ 0x09ae, 0x2011, "usbhid-ups" },
	{ 0x09ae, 0x2012, "usbhid-ups" },
	{ 0x09ae, 0x2013, "usbhid-ups" },
	{ 0x09ae, 0x2014, "usbhid-ups" },
	{ 0x09ae, 0x3008, "usbhid-ups" },
	{ 0x09ae, 0x3009, "usbhid-ups" },
	{ 0x09ae, 0x3010, "usbhid-ups" },
	{ 0x09ae, 0x3011, "usbhid-ups" },
	{ 0x09ae, 0x3012, "usbhid-ups" },
	{ 0x09ae, 0x3013, "usbhid-ups" },
	{ 0x09ae, 0x3014, "usbhid-ups" },
	{ 0x09ae, 0x3015, "usbhid-ups" },
	{ 0x09ae, 0x4001, "usbhid-ups" },
	{ 0x09ae, 0x4002, "usbhid-ups" },
	{ 0x09ae, 0x4003, "usbhid-ups" },
	{ 0x09ae, 0x4004, "usbhid-ups" },
	{ 0x09ae, 0x4005, "usbhid-ups" },
	{ 0x09ae, 0x4006, "usbhid-ups" },
	{ 0x09ae, 0x4007, "usbhid-ups" },
	{ 0x09ae, 0x4008, "usbhid-ups" },
	{ 0x0d9f, 0x00a2, "usbhid-ups" },
	{ 0x0d9f, 0x00a3, "usbhid-ups" },
	{ 0x0d9f, 0x00a4, "usbhid-ups" },
	{ 0x0d9f, 0x00a5, "usbhid-ups" },
	{ 0x0d9f, 0x00a6, "usbhid-ups" },
	{ 0x0f03, 0x0001, "blazer_usb" },
	{ 0x10af, 0x0001, "usbhid-ups" },
	{ 0xffff, 0x0000, "blazer_usb" },
	/* Terminating entry */
	{ -1, -1, NULL }
};
#endif /* DEVSCAN_USB_H */

