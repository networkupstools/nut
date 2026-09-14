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
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include "getopt/getopt.h"
#else
#include <getopt.h>
#endif
#include "libwdi.h"
#include "nutscan-usb.h"

#define oprintf(...) do {if (!opt_silent) printf(__VA_ARGS__);} while(0)

#define DESC        "NUT USB driver"
#define INF_NAME    "usb_device.inf"
#define DEFAULT_DIR "usb_driver"


int __cdecl main(int argc, char** argv)
{
	char desc[128] = DESC;
	struct wdi_device_info *ldev, *ldev_start, dev = {NULL, 0, 0, false, 0, DESC, NULL, NULL, NULL};
	struct wdi_options_create_list ocl = { 0 };
	struct wdi_options_prepare_driver opd = { 0 };
	struct wdi_options_install_driver oid = { 0 };
	int c, r;
	int opt_silent = 0, opt_extract = 0, log_level = WDI_LOG_LEVEL_WARNING;
/*
	int opt_silent = 0, opt_extract = 0, log_level = WDI_LOG_LEVEL_DEBUG;
*/
	char *inf_name = INF_NAME;
	char *ext_dir = DEFAULT_DIR;
	bool matching_device_found;
	int index = 0;

	ocl.list_all = true;
	ocl.list_hubs = true;
	ocl.trim_whitespaces = true;
	opd.driver_type = WDI_LIBUSB0;
/*
	opd.driver_type = WDI_WINUSB;
*/

	wdi_set_log_level(log_level);

	oprintf("NUT UPS driver installer.\n");
	oprintf("-------------------------\n\n");
	oprintf("Searching for known UPS...\n");

	/* Try to match against a plugged device */
	matching_device_found = false;
	if (wdi_create_list(&ldev, &ocl) == WDI_SUCCESS) {
		r = WDI_SUCCESS;
		ldev_start = ldev;
		while( usb_device_table[index].vendorID != 0xFFFF || usb_device_table[index].productID != 0xFFFF) {
			dev.next = NULL;
			dev.vid = usb_device_table[index].vendorID;
			dev.pid = usb_device_table[index].productID;
			dev.is_composite = false;
			dev.mi = 0;
			dev.desc = desc;
			dev.driver = NULL;
			dev.device_id = NULL;
			dev.hardware_id = NULL;
/*
			oprintf("NUT device : vid :  %0X - pid : %0X\n",dev.vid, dev.pid);
*/

			for (ldev = ldev_start; (ldev != NULL) && (r == WDI_SUCCESS); ldev = ldev->next) {
/*
				oprintf("trying vid :  %0X - pid : %0X\n",ldev->vid, ldev->pid);
*/
				if ( (ldev->vid == dev.vid) && (ldev->pid == dev.pid) && (ldev->mi == dev.mi) ) {
					oprintf("Found UPS : vendor ID = %0X - Product ID = %0X\n",ldev->vid, ldev->pid, ldev->mi);
					dev.hardware_id = ldev->hardware_id;
					dev.device_id = ldev->device_id;
					matching_device_found = true;

					oprintf("Extracting driver files...\n");
					r = wdi_prepare_driver(&dev, ext_dir, inf_name, &opd);
					oprintf("  %s\n", wdi_strerror(r));
					if ((r != WDI_SUCCESS) || (opt_extract))
						return r;

					oprintf("  %s: ", dev.hardware_id);
					fflush(stdout);
					oprintf("Installing driver\n");
					r = wdi_install_driver(&dev, ext_dir, inf_name, &oid);
					oprintf("%s\n", wdi_strerror(r));
					if( r == WDI_SUCCESS ) {
						oprintf("You should now unplug and re-plug your device to finish driver's installation.\nHit enter when it's done.\n");
					}
					else {
						oprintf("An error occured while installing driver.\nTry installing libUSB manually.\nHit enter to continue\n");
					}
					getc(stdin);
				}
			}
			index++;
		}
	}

	/* No plugged USB device matches */
	if (!matching_device_found) {
		oprintf("No known UPS device found.\nTry installing libUSB manually.\nHit enter to continue\n");
		getc(stdin);
	}

	return r;
}
