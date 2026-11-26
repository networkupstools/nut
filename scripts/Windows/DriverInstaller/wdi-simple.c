/*
   wdi-simple.c: Console Driver Installer for NUT USB devices
   Copyright (c) 2010 Pete Batard <pbatard@gmail.com>
   Copyright (c) 2011 Frederic Bohe <fredericbohe@eaton.com>

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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include "getopt/getopt.h"
#else
#include <getopt.h>
#endif
#include "libwdi.h"
#include "nutscan-usb.h"

#define DESC        "NUT USB driver"
#define INF_NAME    "usb_device.inf"
#define DEFAULT_DIR "usb_driver"

static int opt_silent = 0;

static void oprintf(const char *fmt, ...)
{
	va_list ap;

	if (opt_silent) {
		return;
	}

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static void usage(void)
{
	printf("Usage: wdi-simple [options]\n\n");
	printf("Install a USB driver for the UPS devices known to NUT,\n");
	printf("so that NUT drivers can open them.\n\n");
	printf("-t, --type <driver>   driver to install: winusb (default) or libusbk\n");
	printf("-d, --dest <dir>      set the extraction directory (default: %s)\n", DEFAULT_DIR);
	printf("-x, --extract         extract driver files only, do not install\n");
	printf("-s, --silent          silent mode, do not wait for user input\n");
	printf("-l, --log <level>     set libwdi log level (0=debug, 4=none)\n");
	printf("-h, --help            display this help\n");
}

/* Is this device (interface) one that a NUT driver would open? */
static int is_known_ups(const struct wdi_device_info *ldev)
{
	size_t i;

	/* NUT drivers use the first interface of composite devices */
	if (ldev->is_composite && ldev->mi != 0) {
		return 0;
	}

	/* The generated table ends with an all-zero entry */
	for (i = 0; usb_device_table[i].vendorID != 0 || usb_device_table[i].productID != 0; i++) {
		if (ldev->vid == usb_device_table[i].vendorID
		 && ldev->pid == usb_device_table[i].productID
		) {
			return 1;
		}
	}

	return 0;
}

int __cdecl main(int argc, char** argv)
{
	char desc[128] = DESC;
	struct wdi_device_info *ldev, *ldev_start = NULL, dev;
	struct wdi_options_create_list ocl;
	struct wdi_options_prepare_driver opd;
	struct wdi_options_install_driver oid;
	static struct option long_options[] = {
		{"type", required_argument, 0, 't'},
		{"dest", required_argument, 0, 'd'},
		{"extract", no_argument, 0, 'x'},
		{"silent", no_argument, 0, 's'},
		{"log", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int c, r;
	int opt_extract = 0, log_level = WDI_LOG_LEVEL_WARNING;
	int found = 0;
	char *inf_name = INF_NAME;
	char *ext_dir = DEFAULT_DIR;

	memset(&ocl, 0, sizeof(ocl));
	memset(&opd, 0, sizeof(opd));
	memset(&oid, 0, sizeof(oid));

	ocl.list_all = TRUE;
	ocl.list_hubs = TRUE;
	ocl.trim_whitespaces = TRUE;
	/* libusb-1.0 (used by NUT on Windows) works best with WinUSB */
	opd.driver_type = WDI_WINUSB;

	while ((c = getopt_long(argc, argv, "d:hl:st:x", long_options, NULL)) != -1) {
		switch (c) {
		case 'd':
			ext_dir = optarg;
			break;
		case 'h':
			usage();
			return 0;
		case 'l':
			log_level = (int)strtol(optarg, NULL, 0);
			break;
		case 's':
			opt_silent = 1;
			log_level = WDI_LOG_LEVEL_NONE;
			break;
		case 't':
			if (!strcmp(optarg, "winusb")) {
				opd.driver_type = WDI_WINUSB;
			} else if (!strcmp(optarg, "libusbk")) {
				opd.driver_type = WDI_LIBUSBK;
			} else {
				fprintf(stderr, "Unknown driver type: %s\n", optarg);
				usage();
				return 1;
			}
			break;
		case 'x':
			opt_extract = 1;
			break;
		default:
			usage();
			return 1;
		}
	}

	wdi_set_log_level(log_level);

	oprintf("NUT UPS driver installer.\n");
	oprintf("-------------------------\n\n");
	oprintf("Searching for known UPS...\n");

	r = wdi_create_list(&ldev_start, &ocl);
	if (r != WDI_SUCCESS && r != WDI_ERROR_NO_DEVICE) {
		oprintf("Could not list USB devices: %s\n", wdi_strerror(r));
		ldev_start = NULL;
	}

	for (ldev = ldev_start; ldev != NULL; ldev = ldev->next) {
		if (!is_known_ups(ldev)) {
			continue;
		}
		found++;

		/* Work on a copy: the list owns its strings */
		memcpy(&dev, ldev, sizeof(dev));
		dev.next = NULL;
		if (dev.desc == NULL) {
			dev.desc = desc;
		}

		oprintf("Found UPS: vendor ID = %04X - product ID = %04X (%s)\n",
			dev.vid, dev.pid, dev.desc);

		oprintf("Extracting driver files...\n");
		r = wdi_prepare_driver(&dev, ext_dir, inf_name, &opd);
		oprintf("  %s\n", wdi_strerror(r));
		if (r != WDI_SUCCESS || opt_extract) {
			break;
		}

		oprintf("Installing driver for %s...\n",
			dev.hardware_id ? dev.hardware_id : "device");
		r = wdi_install_driver(&dev, ext_dir, inf_name, &oid);
		oprintf("  %s\n", wdi_strerror(r));
		if (r != WDI_SUCCESS) {
			break;
		}
	}

	if (ldev_start != NULL) {
		wdi_destroy_list(ldev_start);
	}

	if (!found) {
		oprintf("No known UPS device found.\n"
			"Check that the UPS is plugged in, or install a driver\n"
			"for it manually (e.g. WinUSB with Zadig).\n");
		r = WDI_ERROR_NOT_FOUND;
	} else if (r != WDI_SUCCESS) {
		oprintf("An error occurred while installing the driver.\n"
			"Try installing a driver for the UPS manually\n"
			"(e.g. WinUSB with Zadig).\n");
	} else if (!opt_extract) {
		oprintf("You should now unplug and re-plug your device\n"
			"to finish the driver installation.\n");
	}

	if (!opt_silent) {
		oprintf("Hit enter to continue\n");
		getc(stdin);
	}

	return r;
}
