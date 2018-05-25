/**
 * @file
 * @brief	Detect NUT supported USB devices.
 * @copyright	@parblock
 * Copyright (C):
 * - 2011-2013 -- Frédéric Bohe (<fredericbohe@eaton.com>), for Eaton
 * - 2011-2017 -- Arnaud Quette (<ArnaudQuette@Eaton.com>), for Eaton
 * - 2018      -- Daniele Pezzini (<hyouko@gmail.com>)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *		@endparblock
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

/** @brief Tell which driver listed in *usb_device_id_list* supports the device *dev_VendorID*:*dev_ProductID*.
 * @return the name of the first driver in *usb_device_id_list* that supports the device,
 * @return NULL, if the device is not supported. */
static char	*is_usb_device_supported(
	usb_device_id_t	*usb_device_id_list,	/**< List of USB VID:PID pairs coupled with the name of the driver by which that device should be supported. */
	int		 dev_VendorID,		/**< Vendor ID of the device that has to be checked. */
	int		 dev_ProductID		/**< Product ID of the device that has to be checked. */
) {
	usb_device_id_t	*usbdev;

	for (usbdev = usb_device_id_list; usbdev->driver_name != NULL; usbdev++) {
		if (
			(usbdev->vendorID == dev_VendorID) &&
			(usbdev->productID == dev_ProductID)
		)
			return usbdev->driver_name;
	}

	return NULL;
}

nutscan_device_t	*nutscan_scan_usb(void)
{
	int			  ret;
	libusb_device		**devlist;
	ssize_t			  devcount,
				  devnum;
	nutscan_device_t	 *current_nut_dev = NULL;

	if (!nutscan_avail_usb)
		return NULL;

	/* libusb base init */
	if ((ret = libusb_init(NULL)) != LIBUSB_SUCCESS) {
		libusb_exit(NULL);
		fatalx(EXIT_FAILURE, "Failed to init libusb (%s).", libusb_strerror(ret));
	}

	devcount = libusb_get_device_list(NULL, &devlist);
	if (devcount <= 0) {
		libusb_exit(NULL);
		fatalx(EXIT_FAILURE, "No USB device found (%s).", devcount ? libusb_strerror(devcount) : "no error");
	}

	for (devnum = 0; devnum < devcount; devnum++) {

		libusb_device			*dev = devlist[devnum];
		libusb_device_handle		*udev = NULL;

		char				 string[256];
		uint8_t				 bus;
		char				*busname = NULL;
		char				*serialnumber = NULL;
		char				*device_name = NULL;
		char				*vendor_name = NULL;
		char				*driver_name = NULL;
		struct libusb_device_descriptor	 dev_desc;

		nutscan_device_t		*nut_dev = NULL;

		ret = libusb_get_device_descriptor(dev, &dev_desc);
		if (ret != LIBUSB_SUCCESS) {
			fprintf(stderr, "Unable to get DEVICE descriptor (%s).\n", libusb_strerror(ret));
			continue;
		}

		/* Let's see if this device is supported by a NUT driver... */
		driver_name = is_usb_device_supported(usb_device_table, dev_desc.idVendor, dev_desc.idProduct);
		if (driver_name == NULL)
			continue;

		/* Open the device */
		ret = libusb_open(dev, &udev);
		if (ret != LIBUSB_SUCCESS) {
			fprintf(stderr, "Failed to open device %04X:%04X (%s), skipping.\n", dev_desc.idVendor, dev_desc.idProduct, libusb_strerror(ret));
			continue;
		}

		/* Get device info */

		/* Get bus */
		bus = libusb_get_bus_number(dev);
		if ((busname = (char *)malloc(4)) == NULL)
			goto oom_error;
		snprintf(busname, 4, "%03d", bus);

		/* Get serial number */
		if (dev_desc.iSerialNumber) {
			ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iSerialNumber, (unsigned char *)string, sizeof(string));
			if (ret > 0 && (serialnumber = strdup(str_rtrim(string, ' '))) == NULL)
				goto oom_error;
		}

		/* Get product name */
		if (dev_desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iProduct, (unsigned char *)string, sizeof(string));
			if (ret > 0 && (device_name = strdup(str_rtrim(string, ' '))) == NULL)
				goto oom_error;
		}

		/* Get vendor name */
		if (dev_desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iManufacturer, (unsigned char *)string, sizeof(string));
			if (ret > 0 && (vendor_name = strdup(str_rtrim(string, ' '))) == NULL)
				goto oom_error;
		}

		/* Done with libusb, for this device */
		libusb_close(udev);
		udev = NULL;

		/* Store device info, now */

		nut_dev = nutscan_new_device();
		if (nut_dev == NULL)
			goto oom_error;

		nut_dev->type = TYPE_USB;
		if ((nut_dev->driver = strdup(driver_name)) == NULL)
			goto oom_error;
		if ((nut_dev->port = strdup("auto")) == NULL)
			goto oom_error;
		snprintf(string, sizeof(string), "%04X", dev_desc.idVendor);
		nutscan_add_option_to_device(nut_dev,"vendorid", string);
		snprintf(string, sizeof(string), "%04X", dev_desc.idProduct);
		nutscan_add_option_to_device(nut_dev,"productid", string);
		if (device_name) {
			nutscan_add_option_to_device(nut_dev, "product", device_name);
			free(device_name);
		}
		if (serialnumber) {
			nutscan_add_option_to_device(nut_dev, "serial", serialnumber);
			free(serialnumber);
		}
		if (vendor_name) {
			nutscan_add_option_to_device(nut_dev, "vendor", vendor_name);
			free(vendor_name);
		}
		nutscan_add_option_to_device(nut_dev, "bus", busname);
		free(busname);

		current_nut_dev = nutscan_add_device_to_device(current_nut_dev, nut_dev);
		continue;

	oom_error:
		if (udev)
			libusb_close(udev);
		libusb_free_device_list(devlist, 1);
		libusb_exit(NULL);
		nutscan_free_device(current_nut_dev);
		nutscan_free_device(nut_dev);
		free(busname);
		free(serialnumber);
		free(device_name);
		free(vendor_name);
		fatal_with_errno(EXIT_FAILURE, "Out of memory");

	}

	libusb_free_device_list(devlist, 1);
	libusb_exit(NULL);

	return nutscan_rewind_device(current_nut_dev);
}
#else	/* WITH_USB */
nutscan_device_t	*nutscan_scan_usb(void)
{
	return NULL;
}
#endif	/* WITH_USB */
