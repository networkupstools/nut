/*
 *  Copyright (C) 2011 - EATON
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

/*! \file scan_usb.c
    \brief detect NUT supported USB devices
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#include "common.h"
#include "nut-scan.h"

#ifdef WITH_USB
#include "upsclient.h"
#include "nutscan-usb.h"
#include <stdio.h>
#include <string.h>
#include <ltdl.h>

/* dynamic link library stuff */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;
static int (*nut_usb_close)(usb_dev_handle *dev);
static int (*nut_usb_find_busses)(void);
static char * (*nut_usb_strerror)(void);
static void (*nut_usb_init)(void);
static int (*nut_usb_get_string_simple)(usb_dev_handle *dev, int index,
		char *buf, size_t buflen);
static struct usb_bus * (*nut_usb_busses);
static usb_dev_handle * (*nut_usb_open)(struct usb_device *dev);
static int (*nut_usb_find_devices)(void);

/* return 0 on error; visible externally */
int nutscan_load_usb_library(const char *libname_path);
int nutscan_load_usb_library(const char *libname_path)
{
	if( dl_handle != NULL ) {
			/* if previous init failed */
			if( dl_handle == (void *)1 ) {
					return 0;
			}
			/* init has already been done */
			return 1;
	}

	if (libname_path == NULL) {
		fprintf(stderr, "USB library not found. USB search disabled.\n");
		return 0;
	}

	if( lt_dlinit() != 0 ) {
		fprintf(stderr, "Error initializing lt_init\n");
		return 0;
	}

	dl_handle = lt_dlopen(libname_path);
	if (!dl_handle) {
			dl_error = lt_dlerror();
			goto err;
	}
	lt_dlerror();      /* Clear any existing error */
	*(void **) (&nut_usb_close) = lt_dlsym(dl_handle, "usb_close");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_find_busses) = lt_dlsym(dl_handle, "usb_find_busses");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_strerror) = lt_dlsym(dl_handle, "usb_strerror");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_init) = lt_dlsym(dl_handle, "usb_init");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_get_string_simple) = lt_dlsym(dl_handle,
					"usb_get_string_simple");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_busses) = lt_dlsym(dl_handle, "usb_busses");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_open) = lt_dlsym(dl_handle, "usb_open");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **)(&nut_usb_find_devices) = lt_dlsym(dl_handle,"usb_find_devices");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	return 1;
err:
	fprintf(stderr, "Cannot load USB library (%s) : %s. USB search disabled.\n", libname_path, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}
/* end of dynamic link library stuff */

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

        if( !nutscan_avail_usb ) {
                return NULL;
        }

	/* libusb base init */
	(*nut_usb_init)();
	(*nut_usb_find_busses)();
	(*nut_usb_find_devices)();

	for (bus = (*nut_usb_busses); bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if ((driver_name =
				is_usb_device_supported(usb_device_table,
					dev->descriptor.idVendor,
					dev->descriptor.idProduct)) != NULL) {

				/* open the device */
				udev = (*nut_usb_open)(dev);
				if (!udev) {
					fprintf(stderr,"Failed to open device, \
						skipping. (%s)\n",
						(*nut_usb_strerror)());
					continue;
				}

				/* get serial number */
				if (dev->descriptor.iSerialNumber) {
					ret = (*nut_usb_get_string_simple)(udev,
						dev->descriptor.iSerialNumber,
						string, sizeof(string));
					if (ret > 0) {
						serialnumber = strdup(str_rtrim(string, ' '));
					}
				}
				/* get product name */
				if (dev->descriptor.iProduct) {
					ret = (*nut_usb_get_string_simple)(udev,
						dev->descriptor.iProduct,
						string, sizeof(string));
					if (ret > 0) {
						device_name = strdup(str_rtrim(string, ' '));
					}
				}

				/* get vendor name */
				if (dev->descriptor.iManufacturer) {
					ret = (*nut_usb_get_string_simple)(udev,
						dev->descriptor.iManufacturer,
						string, sizeof(string));
					if (ret > 0) {
						vendor_name = strdup(str_rtrim(string, ' '));
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
					device_name = NULL;
				}
				if(serialnumber) {
					nutscan_add_option_to_device(nut_dev,
								"serial",
								serialnumber);
					free(serialnumber);
					serialnumber = NULL;
				}
				if(vendor_name) {
					nutscan_add_option_to_device(nut_dev,
								"vendor",
								vendor_name);
					free(vendor_name);
					vendor_name = NULL;
				}
				nutscan_add_option_to_device(nut_dev,"bus",
							bus->dirname);

				current_nut_dev = nutscan_add_device_to_device(
								current_nut_dev,
								nut_dev);

				memset (string, 0, sizeof(string));

				(*nut_usb_close)(udev);
			}
		}
	}

	return nutscan_rewind_device(current_nut_dev);
}
#else /* WITH_USB */
nutscan_device_t * nutscan_scan_usb()
{
	return NULL;
}
#endif /* WITH_USB */

