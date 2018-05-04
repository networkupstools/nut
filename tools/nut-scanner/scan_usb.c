/*
 *  Copyright (C) 2011-2016 EATON
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

#ifdef WITH_LIBUSB_1_0
#include "dl_libusb-1.0.h"
#endif	/* WITH_LIBUSB_1_0 */

/* return 0 on error */
int	nutscan_load_usb_library(void)
{
#ifdef WITH_LIBUSB_1_0
	char	error[SMALLBUF];

	if (!dl_libusb10_init(error, sizeof(error))) {
		fprintf(stderr, "%s USB search disabled.\n", error);
		return 0;
	}
	return 1;
#else	/* WITH_LIBUSB_1_0 */
	if (libusb_init(NULL) != LIBUSB_SUCCESS) {
		fprintf(stderr, "Cannot load USB library. USB search disabled.\n");
		return 0;
	}
	return 1;
#endif	/* WITH_LIBUSB_1_0 */
}

void	nutscan_unload_usb_library(void)
{
#ifdef WITH_LIBUSB_1_0
	dl_libusb10_exit();
#else	/* WITH_LIBUSB_1_0 */
	libusb_exit(NULL);
#endif	/* WITH_LIBUSB_1_0 */
}

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
	uint8_t iManufacturer = 0, iProduct = 0, iSerialNumber = 0;
	uint16_t VendorID;
	uint16_t ProductID;
	char *busname;
	libusb_device *dev;
	libusb_device **devlist;
	uint8_t bus;
	libusb_device_handle *udev;
	ssize_t devcount = 0;
	struct libusb_device_descriptor dev_desc;
	int i;

	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;

	if( !nutscan_avail_usb ) {
		return NULL;
	}

	/* libusb base init */
	/* Initialize Libusb */
	if ((ret = libusb_init(NULL)) != LIBUSB_SUCCESS) {
		libusb_exit(NULL);
		fatalx(EXIT_FAILURE, "Failed to init libusb (%s).", libusb_strerror(ret));
	}

	devcount = libusb_get_device_list(NULL, &devlist);
	if (devcount <= 0) {
		libusb_exit(NULL);
		fatalx(EXIT_FAILURE, "No USB device found (%s).", devcount ? libusb_strerror(devcount) : "no error");
	}

	for (i = 0; i < devcount; i++) {

		dev = devlist[i];

		ret = libusb_get_device_descriptor(dev, &dev_desc);
		if (ret != LIBUSB_SUCCESS) {
			fprintf(stderr, "Unable to get DEVICE descriptor (%s).\n", libusb_strerror(ret));
			continue;
		}

		VendorID = dev_desc.idVendor;
		ProductID = dev_desc.idProduct;

		iManufacturer = dev_desc.iManufacturer;
		iProduct = dev_desc.iProduct;
		iSerialNumber = dev_desc.iSerialNumber;
		bus = libusb_get_bus_number(dev);
		busname = (char *)malloc(4);
		if (busname == NULL) {
			libusb_free_device_list(devlist, 1);
			libusb_exit(NULL);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		}
		snprintf(busname, 4, "%03d", bus);

		if ((driver_name =
			is_usb_device_supported(usb_device_table,
				VendorID, ProductID)) != NULL) {

			/* open the device */
			ret = libusb_open(dev, &udev);
			if (ret != LIBUSB_SUCCESS) {
				fprintf(stderr,"Failed to open device, skipping. (%s)\n", libusb_strerror(ret));
				continue;
			}

			/* get serial number */
			if (iSerialNumber) {
				ret = libusb_get_string_descriptor_ascii(udev, iSerialNumber, (unsigned char *)string, sizeof(string));
				if (ret > 0) {
					serialnumber = strdup(str_rtrim(string, ' '));
					if (serialnumber == NULL) {
						libusb_close(udev);
						free(busname);
						libusb_free_device_list(devlist, 1);
						libusb_exit(NULL);
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
				}
			}
			/* get product name */
			if (iProduct) {
				ret = libusb_get_string_descriptor_ascii(udev, iProduct, (unsigned char *)string, sizeof(string));
				if (ret > 0) {
					device_name = strdup(str_rtrim(string, ' '));
					if (device_name == NULL) {
						free(serialnumber);
						libusb_close(udev);
						free(busname);
						libusb_free_device_list(devlist, 1);
						libusb_exit(NULL);
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
				}
			}

			/* get vendor name */
			if (iManufacturer) {
				ret = libusb_get_string_descriptor_ascii(udev, iManufacturer, (unsigned char *)string, sizeof(string));
				if (ret > 0) {
					vendor_name = strdup(str_rtrim(string, ' '));
					if (vendor_name == NULL) {
						free(serialnumber);
						free(device_name);
						libusb_close(udev);
						free(busname);
						libusb_free_device_list(devlist, 1);
						libusb_exit(NULL);
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
				}
			}

			nut_dev = nutscan_new_device();
			if(nut_dev == NULL) {
				fprintf(stderr,"Memory allocation error\n");
				nutscan_free_device(current_nut_dev);
				free(serialnumber);
				free(device_name);
				free(vendor_name);
				libusb_close(udev);
				free(busname);
				libusb_free_device_list(devlist, 1);
				libusb_exit(NULL);
				return NULL;
			}

			nut_dev->type = TYPE_USB;
			if(driver_name) {
				nut_dev->driver = strdup(driver_name);
			}
			nut_dev->port = strdup("auto");
			sprintf(string,"%04X", VendorID);
			nutscan_add_option_to_device(nut_dev,"vendorid", string);
			sprintf(string,"%04X", ProductID);
			nutscan_add_option_to_device(nut_dev,"productid", string);
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
							busname);

			current_nut_dev = nutscan_add_device_to_device(
							current_nut_dev,
							nut_dev);

			memset (string, 0, sizeof(string));

			libusb_close(udev);
		}
		free(busname);
	}

	libusb_free_device_list(devlist, 1);
	libusb_exit(NULL);

	return nutscan_rewind_device(current_nut_dev);
}
#else /* WITH_USB */
nutscan_device_t * nutscan_scan_usb()
{
	return NULL;
}
#endif /* WITH_USB */

