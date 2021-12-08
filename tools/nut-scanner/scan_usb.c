/*
 *  Copyright (C) 2011-2016 - EATON
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
    \author Arnaud Quette <ArnaudQuette@Eaton.com>
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
static int (*nut_usb_close)(libusb_device_handle *dev);
static int (*nut_usb_get_string_simple)(libusb_device_handle *dev, int index,
		char *buf, size_t buflen);


/* Compatibility layer between libusb 0.1 and 1.0 */
#if WITH_LIBUSB_1_0
 #define USB_INIT_SYMBOL "libusb_init"
 #define USB_OPEN_SYMBOL "libusb_open"
 #define USB_CLOSE_SYMBOL "libusb_close"
 #define USB_STRERROR_SYMBOL "libusb_strerror"
 static int (*nut_usb_open)(libusb_device *dev, libusb_device_handle **handle);
 static int (*nut_usb_init)(libusb_context **ctx);
 static void (*nut_usb_exit)(libusb_context *ctx);
 static char * (*nut_usb_strerror)(enum libusb_error errcode);
 static ssize_t (*nut_usb_get_device_list)(libusb_context *ctx,	libusb_device ***list);
 static void (*nut_usb_free_device_list)(libusb_device **list, int unref_devices);
 static uint8_t (*nut_usb_get_bus_number)(libusb_device *dev);
 static int (*nut_usb_get_device_descriptor)(libusb_device *dev,
	struct libusb_device_descriptor *desc);
#else
 #define USB_INIT_SYMBOL "usb_init"
 #define USB_OPEN_SYMBOL "usb_open"
 #define USB_CLOSE_SYMBOL "usb_close"
 #define USB_STRERROR_SYMBOL "usb_strerror"
 static libusb_device_handle * (*nut_usb_open)(struct usb_device *dev);
 static void (*nut_usb_init)(void);
static int (*nut_usb_find_busses)(void);
static struct usb_bus * (*nut_usb_busses);
static int (*nut_usb_find_devices)(void);
 static char * (*nut_usb_strerror)(void);
#endif

/* return 0 on error; visible externally */
int nutscan_load_usb_library(const char *libname_path);
int nutscan_load_usb_library(const char *libname_path)
{
	if (dl_handle != NULL) {
			/* if previous init failed */
			if (dl_handle == (void *)1) {
					return 0;
			}
			/* init has already been done */
			return 1;
	}

	if (libname_path == NULL) {
		fprintf(stderr, "USB library not found. USB search disabled.\n");
		return 0;
	}

	if (lt_dlinit() != 0) {
		fprintf(stderr, "Error initializing lt_init\n");
		return 0;
	}

	dl_handle = lt_dlopen(libname_path);
	if (!dl_handle) {
			dl_error = lt_dlerror();
			goto err;
	}

	*(void **) (&nut_usb_init) = lt_dlsym(dl_handle, USB_INIT_SYMBOL);
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_open) = lt_dlsym(dl_handle, USB_OPEN_SYMBOL);
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	lt_dlerror();      /* Clear any existing error */
	*(void **) (&nut_usb_close) = lt_dlsym(dl_handle, USB_CLOSE_SYMBOL);
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_strerror) = lt_dlsym(dl_handle, USB_STRERROR_SYMBOL);
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

#if WITH_LIBUSB_1_0
	*(void **) (&nut_usb_exit) = lt_dlsym(dl_handle, "libusb_exit");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_get_device_list) = lt_dlsym(dl_handle, "libusb_get_device_list");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_free_device_list) = lt_dlsym(dl_handle, "libusb_free_device_list");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_get_bus_number) = lt_dlsym(dl_handle, "libusb_get_bus_number");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_usb_get_device_descriptor) = lt_dlsym(dl_handle, "libusb_get_device_descriptor");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_get_string_simple) = lt_dlsym(dl_handle,
					"libusb_get_string_descriptor_ascii");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}
#else /* for libusb 0.1 */
	*(void **) (&nut_usb_find_busses) = lt_dlsym(dl_handle, "usb_find_busses");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_busses) = lt_dlsym(dl_handle, "usb_busses");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **)(&nut_usb_find_devices) = lt_dlsym(dl_handle,"usb_find_devices");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_get_string_simple) = lt_dlsym(dl_handle,
					"usb_get_string_simple");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}
#endif /* WITH_LIBUSB_1_0 */

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

	for (usbdev = usb_device_id_list; usbdev->driver_name != NULL; usbdev++) {
		if ((usbdev->vendorID == dev_VendorID)
		 && (usbdev->productID == dev_ProductID)
		) {
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
	uint8_t iManufacturer = 0, iProduct = 0, iSerialNumber = 0;
	uint16_t VendorID;
	uint16_t ProductID;
	char *busname;
#if WITH_LIBUSB_1_0
	libusb_device *dev;
	libusb_device **devlist;
	uint8_t bus;
#else
	struct usb_device *dev;
	struct usb_bus *bus;
#endif /* WITH_LIBUSB_1_0 */
	libusb_device_handle *udev;

	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;

	if (!nutscan_avail_usb) {
		return NULL;
	}

	/* libusb base init */
	/* Initialize Libusb */
#if WITH_LIBUSB_1_0
	if ((*nut_usb_init)(NULL) < 0) {
		(*nut_usb_exit)(NULL);
		fatal_with_errno(EXIT_FAILURE, "Failed to init libusb 1.0");
	}
#else
	(*nut_usb_init)();
	(*nut_usb_find_busses)();
	(*nut_usb_find_devices)();
#endif /* WITH_LIBUSB_1_0 */

#if WITH_LIBUSB_1_0
	ssize_t devcount = 0;
	struct libusb_device_descriptor dev_desc;
	int i;

	devcount = (*nut_usb_get_device_list)(NULL, &devlist);
	if (devcount <= 0) {
		(*nut_usb_exit)(NULL);
		fatal_with_errno(EXIT_FAILURE, "No USB device found");
	}

	for (i = 0; i < devcount; i++) {

		dev = devlist[i];
		(*nut_usb_get_device_descriptor)(dev, &dev_desc);

		VendorID = dev_desc.idVendor;
		ProductID = dev_desc.idProduct;

		iManufacturer = dev_desc.iManufacturer;
		iProduct = dev_desc.iProduct;
		iSerialNumber = dev_desc.iSerialNumber;
		bus = (*nut_usb_get_bus_number)(dev);
		busname = (char *)malloc(4);
		if (busname == NULL) {
			(*nut_usb_free_device_list)(devlist, 1);
			(*nut_usb_exit)(NULL);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		}
		snprintf(busname, 4, "%03d", bus);
#else
	for (bus = (*nut_usb_busses); bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {

			VendorID = dev->descriptor.idVendor;
			ProductID = dev->descriptor.idProduct;

			iManufacturer = dev->descriptor.iManufacturer;
			iProduct = dev->descriptor.iProduct;
			iSerialNumber = dev->descriptor.iSerialNumber;
			busname = bus->dirname;
#endif
			if ((driver_name =
				is_usb_device_supported(usb_device_table,
					VendorID, ProductID)) != NULL) {

				/* open the device */
#if WITH_LIBUSB_1_0
				ret = (*nut_usb_open)(dev, &udev);
				if (!udev) {
					fprintf(stderr,"Failed to open device, \
						skipping. (%s)\n",
						(*nut_usb_strerror)(ret));
					continue;
				}
#else
				udev = (*nut_usb_open)(dev);
				if (!udev) {
					fprintf(stderr, "Failed to open device, \
						skipping. (%s)\n",
						(*nut_usb_strerror)());
					continue;
				}
#endif

				/* get serial number */
				if (iSerialNumber) {
					ret = (*nut_usb_get_string_simple)(udev,
						iSerialNumber, string, sizeof(string));
					if (ret > 0) {
						serialnumber = strdup(str_rtrim(string, ' '));
						if (serialnumber == NULL) {
							(*nut_usb_close)(udev);
#ifdef WITH_LIBUSB_1_0
							free(busname);
							(*nut_usb_free_device_list)(devlist, 1);
							(*nut_usb_exit)(NULL);
#endif	/* WITH_LIBUSB_1_0 */
							fatal_with_errno(EXIT_FAILURE, "Out of memory");
						}
					}
				}

				/* get product name */
				if (iProduct) {
					ret = (*nut_usb_get_string_simple)(udev,
						iProduct, string, sizeof(string));
					if (ret > 0) {
						device_name = strdup(str_rtrim(string, ' '));
						if (device_name == NULL) {
							free(serialnumber);
							(*nut_usb_close)(udev);
#ifdef WITH_LIBUSB_1_0
							free(busname);
							(*nut_usb_free_device_list)(devlist, 1);
							(*nut_usb_exit)(NULL);
#endif	/* WITH_LIBUSB_1_0 */
							fatal_with_errno(EXIT_FAILURE, "Out of memory");
						}
					}
				}

				/* get vendor name */
				if (iManufacturer) {
					ret = (*nut_usb_get_string_simple)(udev,
						iManufacturer, string, sizeof(string));
					if (ret > 0) {
						vendor_name = strdup(str_rtrim(string, ' '));
						if (vendor_name == NULL) {
							free(serialnumber);
							free(device_name);
							(*nut_usb_close)(udev);
#ifdef WITH_LIBUSB_1_0
							free(busname);
							(*nut_usb_free_device_list)(devlist, 1);
							(*nut_usb_exit)(NULL);
#endif	/* WITH_LIBUSB_1_0 */
							fatal_with_errno(EXIT_FAILURE, "Out of memory");
						}
					}
				}

				nut_dev = nutscan_new_device();
				if (nut_dev == NULL) {
					fprintf(stderr,
						"Memory allocation error\n");
					nutscan_free_device(current_nut_dev);
					free(serialnumber);
					free(device_name);
					free(vendor_name);
					(*nut_usb_close)(udev);
#ifdef WITH_LIBUSB_1_0
					free(busname);
					(*nut_usb_free_device_list)(devlist, 1);
					(*nut_usb_exit)(NULL);
#endif	/* WITH_LIBUSB_1_0 */
					return NULL;
				}

				nut_dev->type = TYPE_USB;
				if (driver_name) {
					nut_dev->driver = strdup(driver_name);
				}
				nut_dev->port = strdup("auto");

				sprintf(string, "%04X", VendorID);
				nutscan_add_option_to_device(nut_dev,
					"vendorid",
					string);

				sprintf(string, "%04X", ProductID);
				nutscan_add_option_to_device(nut_dev,
					"productid",
					string);

				if (device_name) {
					nutscan_add_option_to_device(nut_dev,
						"product",
						device_name);
					free(device_name);
					device_name = NULL;
				}

				if (serialnumber) {
					nutscan_add_option_to_device(nut_dev,
						"serial",
						serialnumber);
					free(serialnumber);
					serialnumber = NULL;
				}

				if (vendor_name) {
					nutscan_add_option_to_device(nut_dev,
						"vendor",
						vendor_name);
					free(vendor_name);
					vendor_name = NULL;
				}

				nutscan_add_option_to_device(nut_dev,
					"bus",
					busname);

				current_nut_dev = nutscan_add_device_to_device(
					current_nut_dev,
					nut_dev);

				memset (string, 0, sizeof(string));

				(*nut_usb_close)(udev);
			}
#if WITH_LIBUSB_0_1
		}
	}
#else	/* not WITH_LIBUSB_0_1 */
		free(busname);
	}

	(*nut_usb_free_device_list)(devlist, 1);
	(*nut_usb_exit)(NULL);
#endif	/* WITH_LIBUSB_0_1 */

	return nutscan_rewind_device(current_nut_dev);
}
#else /* not WITH_USB */
nutscan_device_t * nutscan_scan_usb()
{
	return NULL;
}
#endif /* WITH_USB */
