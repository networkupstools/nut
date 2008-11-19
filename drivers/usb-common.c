/* usb-common.c - common useful USB functions

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

#include <stdlib.h>
#include "usb-common.h"

int is_usb_device_supported(usb_device_id_t *usb_device_id_list, 
							int dev_VendorID, int dev_ProductID)
{
	int retval = NOT_SUPPORTED;
	usb_device_id_t *usbdev;

	for (usbdev = usb_device_id_list; usbdev->vendorID != -1; usbdev++) {
		if (usbdev->vendorID == dev_VendorID) {
			/* mark as possibly supported if we see a known vendor */
			retval = POSSIBLY_SUPPORTED;
			if (usbdev->productID == dev_ProductID) {
				/* call the specific handler, if exists */
				if (usbdev->fun != NULL)
					usbdev->fun();
				return SUPPORTED;
			}
		}
	}
	return retval;
}
