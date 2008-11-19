/* usb-common.h - prototypes for the common useful USB functions

   Copyright (C) 2008  Arnaud Quette <arnaud.quette@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef NUT_USB_COMMON_H
#define NUT_USB_COMMON_H

/* dummy USB function and macro, inspired from the Linux kernel
 * this allows USB information extraction */
#define USB_DEVICE(vendorID, productID)	vendorID, productID

typedef struct {
	int vendorID;
	int productID;
	void*(*fun)();				/* handler for specific processing */
} usb_device_id_t;

#define NOT_SUPPORTED		0
#define POSSIBLY_SUPPORTED	1
#define SUPPORTED			2

/* Function used to match a VendorID/ProductID pair against a list of
 * supported devices. Return values:
 * NOT_SUPPORTED (0), POSSIBLY_SUPPORTED (1) or SUPPORTED (2) */
int is_usb_device_supported(usb_device_id_t *usb_device_id_list, 
							int dev_VendorID, int dev_ProductID);

#endif /* NUT_USB_COMMON_H */
