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
 static uint8_t (*nut_usb_get_device_address)(libusb_device *dev);
 static uint8_t (*nut_usb_get_port_number)(libusb_device *dev);
 static int (*nut_usb_get_device_descriptor)(libusb_device *dev,
	struct libusb_device_descriptor *desc);
#else /* => WITH_LIBUSB_0_1 */
 #define USB_INIT_SYMBOL "usb_init"
 #define USB_OPEN_SYMBOL "usb_open"
 #define USB_CLOSE_SYMBOL "usb_close"
 #define USB_STRERROR_SYMBOL "usb_strerror"
 static libusb_device_handle * (*nut_usb_open)(struct usb_device *dev);
 static void (*nut_usb_init)(void);
 static int (*nut_usb_find_busses)(void);
# ifndef WIN32
 static struct usb_bus * (*nut_usb_busses);
# else
 static struct usb_bus * (*nut_usb_get_busses)(void);
# endif	/* WIN32 */
 static int (*nut_usb_find_devices)(void);
 static char * (*nut_usb_strerror)(void);
#endif /* WITH_LIBUSB_1_0 */

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
	lt_dlerror();      /* Clear any existing error */

	*(void **) (&nut_usb_init) = lt_dlsym(dl_handle, USB_INIT_SYMBOL);
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_open) = lt_dlsym(dl_handle, USB_OPEN_SYMBOL);
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

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

	*(void **) (&nut_usb_get_device_list) = lt_dlsym(dl_handle,
					"libusb_get_device_list");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_free_device_list) = lt_dlsym(dl_handle,
					"libusb_free_device_list");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_get_bus_number) = lt_dlsym(dl_handle,
					"libusb_get_bus_number");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	/* Note: per https://nxmnpg.lemoda.net/3/libusb_get_device_address there
	 * was a libusb_get_port_path() equivalent with different arguments, but
	 * not for too long (libusb-1.0.12...1.0.16) and now it is deprecated.
	 */
	*(void **) (&nut_usb_get_device_address) = lt_dlsym(dl_handle,
					"libusb_get_device_address");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	/* This method may be absent in some libusb versions, and we should
	 * tolerate that! In run-time driver code see also blocks fenced by:
	 *   #if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	 */
	*(void **) (&nut_usb_get_port_number) = lt_dlsym(dl_handle,
					"libusb_get_port_number");
	if ((dl_error = lt_dlerror()) != NULL) {
			fprintf(stderr,
				"While loading USB library (%s), failed to find libusb_get_port_number() : %s. "
				"The \"busport\" USB matching option will be disabled.\n",
				libname_path, dl_error);
			nut_usb_get_port_number = NULL;
	}

	*(void **) (&nut_usb_get_device_descriptor) = lt_dlsym(dl_handle,
					"libusb_get_device_descriptor");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_usb_get_string_simple) = lt_dlsym(dl_handle,
					"libusb_get_string_descriptor_ascii");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}
#else /* for libusb 0.1 */
	*(void **) (&nut_usb_find_busses) = lt_dlsym(dl_handle,
					"usb_find_busses");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

# ifndef WIN32
	*(void **) (&nut_usb_busses) = lt_dlsym(dl_handle,
					"usb_busses");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}
# else
	*(void **) (&nut_usb_get_busses) = lt_dlsym(dl_handle,
					"usb_get_busses");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}
# endif	/* WIN32 */

	*(void **)(&nut_usb_find_devices) = lt_dlsym(dl_handle,
					"usb_find_devices");
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
	fprintf(stderr,
		"Cannot load USB library (%s) : %s. USB search disabled.\n",
		libname_path, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}
/* end of dynamic link library stuff */

static char* is_usb_device_supported(usb_device_id_t *usb_device_id_list,
					int dev_VendorID, int dev_ProductID, char **alt)
{
	usb_device_id_t *usbdev;

	for (usbdev = usb_device_id_list; usbdev->driver_name != NULL; usbdev++) {
		if ((usbdev->vendorID == dev_VendorID)
		 && (usbdev->productID == dev_ProductID)
		) {
			if (alt)
				*alt = usbdev->alt_driver_names;
			return usbdev->driver_name;
		}
	}

	return NULL;
}

/* return NULL if error */
nutscan_device_t * nutscan_scan_usb(nutscan_usb_t * scanopts)
{
	int ret;
	char string[256];
	/* Items below are learned by libusbN version-specific API code
	 * Keep in sync with items matched by drivers/libusb{0,1}.c
	 * (nut)libusb_open methods, and fields of USBDevice_t struct
	 * (drivers/usb-common.h).
	 */
	char *driver_name = NULL;
	char *alt_driver_names = NULL;
	char *serialnumber = NULL;
	char *device_name = NULL;
	char *vendor_name = NULL;
	uint8_t iManufacturer = 0, iProduct = 0, iSerialNumber = 0;
	uint16_t VendorID = 0;
	uint16_t ProductID = 0;
	char *busname = NULL;
	/* device_port physical meaning: connection port on that bus;
	 *   different consumers plugged into same socket should have
	 *   the same port value. However in practice such functionality
	 *   depends on platform and HW involved and may mean logical
	 *   enumeration results.
	 * In libusb1 API: first libusb_get_port_numbers() earlier known
	 *    as libusb_get_port_path() for physical port number on the bus, see
	 *    https://libusb.sourceforge.io/api-1.0/group__libusb__dev.html#ga14879a0ea7daccdcddb68852d86c00c4
	 *    later changed to logical libusb_get_bus_number() (which
	 *    often yields same numeric value, except on systems that
	 *    can not see or tell about physical topology)
	 * In libusb0 API: "device filename"
	 */
	char *device_port = NULL;

	/* bcdDevice: aka "Device release number" - note we currently do not match by it */
	uint16_t bcdDevice = 0;

	nutscan_usb_t	default_scanopts;

#if WITH_LIBUSB_1_0
	libusb_device *dev;
	libusb_device **devlist;
	uint8_t bus_num;
	/* Sort of like device_port above, but different (should be
	 * more closely about physical port number than logical device
	 * enumeration results). Uses libusb_get_port_number() where
	 * available in libusb (and hoping the OS and HW honour it).
	 */
	char *bus_port = NULL;
	ssize_t devcount = 0;
	struct libusb_device_descriptor dev_desc;
	int i;
#else  /* => WITH_LIBUSB_0_1 */
	struct usb_device *dev;
	struct usb_bus *bus;
#endif /* WITH_LIBUSB_1_0 */
	libusb_device_handle *udev;

	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;

	if (!nutscan_avail_usb) {
		return NULL;
	}

	if (!scanopts) {
		default_scanopts.report_bus = 1;
		default_scanopts.report_busport = 1;
		/* AQU note: disabled by default, since it may lead to instabilities
		 * and give more issues than solutions! */
		default_scanopts.report_device = 0;
		/* Generally not useful at the moment, and coded to be commented away
		 * in formats that support it (e.g. ups.conf) or absent in others. */
		default_scanopts.report_bcdDevice = 0;
		scanopts = &default_scanopts;
	}

	/* libusb base init */
	/* Initialize Libusb */
#if WITH_LIBUSB_1_0
	if ((*nut_usb_init)(NULL) < 0) {
		(*nut_usb_exit)(NULL);
		fatal_with_errno(EXIT_FAILURE, "Failed to init libusb 1.0");
	}
#else  /* => WITH_LIBUSB_0_1 */
	(*nut_usb_init)();
	(*nut_usb_find_busses)();
	(*nut_usb_find_devices)();
#endif /* WITH_LIBUSB_1_0 */

#if WITH_LIBUSB_1_0
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
		bus_num = (*nut_usb_get_bus_number)(dev);

		busname = (char *)malloc(4);
		if (busname == NULL) {
			(*nut_usb_free_device_list)(devlist, 1);
			(*nut_usb_exit)(NULL);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		}
		snprintf(busname, 4, "%03d", bus_num);

		device_port = (char *)malloc(4);
		if (device_port == NULL) {
			(*nut_usb_free_device_list)(devlist, 1);
			(*nut_usb_exit)(NULL);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		} else {
			uint8_t device_addr = (*nut_usb_get_device_address)(dev);
			if (device_addr > 0) {
				snprintf(device_port, 4, "%03d", device_addr);
			} else {
				snprintf(device_port, 4, ".*");
			}
		}

		if (nut_usb_get_port_number != NULL) {
			bus_port = (char *)malloc(4);
			if (bus_port == NULL) {
				(*nut_usb_free_device_list)(devlist, 1);
				(*nut_usb_exit)(NULL);
				fatal_with_errno(EXIT_FAILURE, "Out of memory");
			} else {
				uint8_t port_num = (*nut_usb_get_port_number)(dev);
				if (port_num > 0) {
					snprintf(bus_port, 4, "%03d", port_num);
				} else {
					snprintf(bus_port, 4, ".*");
				}
			}
		}

		bcdDevice = dev_desc.bcdDevice;
#else  /* => WITH_LIBUSB_0_1 */
# ifndef WIN32
	for (bus = (*nut_usb_busses); bus; bus = bus->next) {
# else
	for (bus = (*nut_usb_get_busses)(); bus; bus = bus->next) {
# endif	/* WIN32 */
		for (dev = bus->devices; dev; dev = dev->next) {

			VendorID = dev->descriptor.idVendor;
			ProductID = dev->descriptor.idProduct;

			iManufacturer = dev->descriptor.iManufacturer;
			iProduct = dev->descriptor.iProduct;
			iSerialNumber = dev->descriptor.iSerialNumber;
			busname = bus->dirname;
			device_port = dev->filename;
			bcdDevice = dev->descriptor.bcdDevice;
#endif
			if ((driver_name =
				is_usb_device_supported(usb_device_table,
					VendorID, ProductID, &alt_driver_names)) != NULL) {

				/* open the device */
#if WITH_LIBUSB_1_0
				ret = (*nut_usb_open)(dev, &udev);
				if (!udev || ret != LIBUSB_SUCCESS) {
					fprintf(stderr, "Failed to open device "
						"bus '%s' device/port '%s' bus/port '%s', skipping: %s\n",
						busname, device_port, bus_port,
						(*nut_usb_strerror)(ret));

					/* Note: closing is not applicable
					 * it seems, and can even segfault
					 * (even though an udev is not NULL
					 * when e.g. permissions problem)
					 */

					free(busname);
					free(device_port);
					if (bus_port != NULL) {
						free(bus_port);
						bus_port = NULL;
					}

					continue;
				}
#else  /* => WITH_LIBUSB_0_1 */
				udev = (*nut_usb_open)(dev);
				if (!udev) {
					/* TOTHINK: any errno or similar to test? */
					fprintf(stderr, "Failed to open device "
						"bus '%s' device/port '%s', skipping: %s\n",
						busname, device_port,
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
#if WITH_LIBUSB_1_0
							free(busname);
							free(device_port);
							if (bus_port != NULL) {
								free(bus_port);
								bus_port = NULL;
							}
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
#if WITH_LIBUSB_1_0
							free(busname);
							free(device_port);
							if (bus_port != NULL) {
								free(bus_port);
								bus_port = NULL;
							}
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
#if WITH_LIBUSB_1_0
							free(busname);
							free(device_port);
							if (bus_port != NULL) {
								free(bus_port);
								bus_port = NULL;
							}
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
#if WITH_LIBUSB_1_0
					free(busname);
					free(device_port);
					if (bus_port != NULL) {
						free(bus_port);
						bus_port = NULL;
					}
					(*nut_usb_free_device_list)(devlist, 1);
					(*nut_usb_exit)(NULL);
#endif	/* WITH_LIBUSB_1_0 */
					return NULL;
				}

				nut_dev->type = TYPE_USB;
				if (driver_name) {
					nut_dev->driver = strdup(driver_name);
				}
				if (alt_driver_names) {
					nut_dev->alt_driver_names = strdup(alt_driver_names);
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

				nutscan_add_commented_option_to_device(nut_dev,
					"bus",
					busname,
					scanopts->report_bus ? NULL : "");

				nutscan_add_commented_option_to_device(nut_dev,
					"device",
					device_port,
					scanopts->report_device ? NULL : "");

#if WITH_LIBUSB_1_0
				if (bus_port) {
					nutscan_add_commented_option_to_device(nut_dev,
						"busport",
						bus_port,
						scanopts->report_busport ? NULL : "");
					free(bus_port);
					bus_port = NULL;
				}
#endif	/* WITH_LIBUSB_1_0 */

				/* FIXME: Detect and suggest HID index, interface number, etc. */

				if (scanopts->report_bcdDevice) {
					/* Not currently matched by drivers, hence commented
					 * for now even if requested via scanopts */
					sprintf(string, "%04X", bcdDevice);
					nutscan_add_commented_option_to_device(nut_dev,
						"bcdDevice",
						string,
						"NOTMATCHED-YET");
				}

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
		free(device_port);
		if (bus_port != NULL) {
			free(bus_port);
			bus_port = NULL;
		}
	}

	(*nut_usb_free_device_list)(devlist, 1);
	(*nut_usb_exit)(NULL);
#endif	/* WITH_LIBUSB_0_1 */

	return nutscan_rewind_device(current_nut_dev);
}

#else /* not WITH_USB */

/* stub function */
nutscan_device_t * nutscan_scan_usb(nutscan_usb_t * scanopts)
{
	NUT_UNUSED_VARIABLE(scanopts);

	return NULL;
}

#endif /* WITH_USB */
