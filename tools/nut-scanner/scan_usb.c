/* scan_usb.c: detect NUT supported USB devices
 * 
 *  Copyright (C) 2011 - Frederic Bohe <fredericbohe@eaton.com>
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

#include "common.h"
#ifdef HAVE_USB_H
#include "upsclient.h"
#include "nutscan-usb.h"
#include <stdio.h>
#include <string.h>
#include "nutscan-device.h"

static char* is_usb_device_supported(usb_device_id_t *usb_device_id_list,
					int dev_VendorID, int dev_ProductID)
{
	usb_device_id_t *usbdev;

	for (usbdev=usb_device_id_list; usbdev->driver_name != NULL; usbdev++) {
		if ( (usbdev->vendorID == dev_VendorID)
				&& (usbdev->productID == dev_ProductID) ) {

			return usbdev->driver_name;
		}
	}

	return NULL;
}

/* return NULL if error */
nutscan_device_t * nutscan_scan_usb()
{
	int ret;
	char string[256];
	char *driver_name = NULL;
	char *serialnumber = NULL;
	char *device_name = NULL;
	char *vendor_name = NULL;
	struct usb_device *dev;
	struct usb_bus *bus;
	usb_dev_handle *udev;

	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;

	/* libusb base init */
	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if ((driver_name =
				is_usb_device_supported(usb_device_table,
					dev->descriptor.idVendor,
					dev->descriptor.idProduct)) != NULL) {

				/* open the device */
				udev = usb_open(dev);
				if (!udev) {
					fprintf(stderr,"Failed to open device, \
						skipping. (%s)\n",
						usb_strerror());
					continue;
				}

				/* get serial number */
				if (dev->descriptor.iSerialNumber) {
					ret = usb_get_string_simple(udev,
						dev->descriptor.iSerialNumber,
						string, sizeof(string));
					if (ret > 0) {
						serialnumber = strdup(string);
					}
				}
				/* get product name */
				if (dev->descriptor.iProduct) {
					ret = usb_get_string_simple(udev,
						dev->descriptor.iProduct,
						string, sizeof(string));
					if (ret > 0) {
						device_name = strdup(string);
					}
				}

				/* get vendor name */
				if (dev->descriptor.iManufacturer) {
					ret = usb_get_string_simple(udev,
						dev->descriptor.iManufacturer, 
						string, sizeof(string));
					if (ret > 0) {
						vendor_name = strdup(string);
					}
				}

				nut_dev = nutscan_new_device();
				if(nut_dev == NULL) {
					fprintf(stderr,"Memory allocation \
					error\n");
					nutscan_free_device(current_nut_dev);
					free(serialnumber);
					free(device_name);
					free(vendor_name);
					return NULL;
				}

				nut_dev->type = TYPE_USB;
				if(driver_name) {
					nut_dev->driver = strdup(driver_name);
				}
				nut_dev->port = strdup("auto");
				sprintf(string,"%04X",dev->descriptor.idVendor);
				nutscan_add_option_to_device(nut_dev,"vendorid",
								string);
				sprintf(string,"%04X",
					dev->descriptor.idProduct);
				nutscan_add_option_to_device(nut_dev,"productid",
							string);
				if(device_name) {
					nutscan_add_option_to_device(nut_dev,
								"product",
								device_name);
					free(device_name);
				}
				if(serialnumber) {
					nutscan_add_option_to_device(nut_dev,
								"serial",
								serialnumber);
					free(serialnumber);
				}
				if(vendor_name) {
					nutscan_add_option_to_device(nut_dev,
								"vendor",
								vendor_name);
					free(vendor_name);
				}
				nutscan_add_option_to_device(nut_dev,"bus",
							bus->dirname);

				current_nut_dev = nutscan_add_device_to_device(
								current_nut_dev,
								nut_dev);

				memset (string, 0, sizeof(string));

				usb_close(udev);
			}
		}
	}

	return current_nut_dev;
}
#endif /* HAVE_USB_H */

