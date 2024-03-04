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

#include "config.h"	/* must be first */
#include "common.h"
#include "usb-common.h"

int is_usb_device_supported(usb_device_id_t *usb_device_id_list, USBDevice_t *device)
{
	int retval = NOT_SUPPORTED;
	usb_device_id_t *usbdev;

	for (usbdev = usb_device_id_list;
	     (usbdev->vendorID != 0 || usbdev->productID != 0 || usbdev->fun != NULL);
	     usbdev++
	) {
		if (usbdev->vendorID != device->VendorID) {
			continue;
		}

		/* flag as possibly supported if we see a known vendor */
		retval = POSSIBLY_SUPPORTED;

		if (usbdev->productID != device->ProductID) {
			continue;
		}

		/* call the specific handler, if it exists */
		if (usbdev->fun != NULL) {
			(*usbdev->fun)(device);
		}

		return SUPPORTED;
	}

	return retval;
}

/* ---------------------------------------------------------------------- */
/* matchers */

/* private callback function for exact matches
 */
static int match_function_exact(USBDevice_t *hd, void *privdata)
{
	USBDevice_t	*data = (USBDevice_t *)privdata;

	upsdebugx(3, "%s: matching a device...", __func__);

	if (hd->VendorID != data->VendorID) {
		upsdebugx(2, "%s: failed match of %s: %4x != %4x",
		    __func__, "VendorID", hd->VendorID, data->VendorID);
		return 0;
	}

	if (hd->ProductID != data->ProductID) {
		upsdebugx(2, "%s: failed match of %s: %4x != %4x",
		    __func__, "ProductID", hd->ProductID, data->ProductID);
		return 0;
	}

	if (strcmp_null(hd->Vendor, data->Vendor) != 0) {
		upsdebugx(2, "%s: failed match of %s: %s != %s",
		    __func__, "Vendor", hd->Vendor, data->Vendor);
		return 0;
	}

	if (strcmp_null(hd->Product, data->Product) != 0) {
		upsdebugx(2, "%s: failed match of %s: %s != %s",
		    __func__, "Product", hd->Product, data->Product);
		return 0;
	}

	if (strcmp_null(hd->Serial, data->Serial) != 0) {
		upsdebugx(2, "%s: failed match of %s: %s != %s",
		    __func__, "Serial", hd->Serial, data->Serial);
		return 0;
	}
#ifdef DEBUG_EXACT_MATCH_BUS
	if (strcmp_null(hd->Bus, data->Bus) != 0) {
		upsdebugx(2, "%s: failed match of %s: %s != %s",
		    __func__, "Bus", hd->Bus, data->Bus);
		return 0;
	}
#endif
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
  #ifdef DEBUG_EXACT_MATCH_BUSPORT
	if (strcmp_null(hd->BusPort, data->BusPort) != 0) {
		upsdebugx(2, "%s: failed match of %s: %s != %s",
		    __func__, "BusPort", hd->BusPort, data->BusPort);
		return 0;
	}
  #endif
#endif
#ifdef DEBUG_EXACT_MATCH_DEVICE
	if (strcmp_null(hd->Device, data->Device) != 0) {
		upsdebugx(2, "%s: failed match of %s: %s != %s",
		    __func__, "Device", hd->Device, data->Device);
		return 0;
	}
#endif
	return 1;
}

/* constructor: create an exact matcher that matches the device.
 * On success, return 0 and store the matcher in *matcher. On
 * error, return -1 with errno set
 */
int USBNewExactMatcher(USBDeviceMatcher_t **matcher, USBDevice_t *hd)
{
	USBDeviceMatcher_t	*m;
	USBDevice_t		*data;

	m = malloc(sizeof(*m));
	if (!m) {
		return -1;
	}

	data = calloc(1, sizeof(*data));
	if (!data) {
		free(m);
		return -1;
	}

	m->match_function = &match_function_exact;
	m->privdata = (void *)data;
	m->next = NULL;

	data->VendorID = hd->VendorID;
	data->ProductID = hd->ProductID;
	data->Vendor = hd->Vendor ? strdup(hd->Vendor) : NULL;
	data->Product = hd->Product ? strdup(hd->Product) : NULL;
	data->Serial = hd->Serial ? strdup(hd->Serial) : NULL;
#ifdef DEBUG_EXACT_MATCH_BUS
	data->Bus = hd->Bus ? strdup(hd->Bus) : NULL;
#endif
#ifdef DEBUG_EXACT_MATCH_DEVICE
	data->Device = hd->Device ? strdup(hd->Device) : NULL;
#endif

	/* NOTE: Callers must pre-initialize to NULL! */
	if (matcher && *matcher) {
		free(*matcher);
		*matcher = NULL;
	}

	*matcher = m;

	return 0;
}

/* destructor: free matcher previously created with USBNewExactMatcher */
void USBFreeExactMatcher(USBDeviceMatcher_t *matcher)
{
	USBDevice_t	*data;

	if (!matcher) {
		return;
	}

	data = (USBDevice_t *)matcher->privdata;

	free(data->Vendor);
	free(data->Product);
	free(data->Serial);
#ifdef DEBUG_EXACT_MATCH_BUS
	free(data->Bus);
#endif
#ifdef DEBUG_EXACT_MATCH_DEVICE
	free(data->Device);
#endif
	free(data);
	free(matcher);
}

/* private data type: hold a set of compiled regular expressions. */
typedef struct regex_matcher_data_s {
	regex_t	*regex[USBMATCHER_REGEXP_ARRAY_LIMIT];
} regex_matcher_data_t;

/* private callback function for regex matches */
static int match_function_regex(USBDevice_t *hd, void *privdata)
{
	regex_matcher_data_t	*data = (regex_matcher_data_t *)privdata;
	int r;

	upsdebugx(3, "%s: matching a device...", __func__);

	/* NOTE: Here and below the "detailed" logging is commented away
	 * because data->regex[] elements are not strings anymore! */

	r = match_regex_hex(data->regex[0], hd->VendorID);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %4x !~ %s",
		    __func__, "VendorID", hd->VendorID, data->regex[0]);
*/
		upsdebugx(2, "%s: failed match of %s: %4x",
		    __func__, "VendorID", hd->VendorID);
		return r;
	}

	r = match_regex_hex(data->regex[1], hd->ProductID);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %4x !~ %s",
		    __func__, "ProductID", hd->ProductID, data->regex[1]);
*/
		upsdebugx(2, "%s: failed match of %s: %4x",
		    __func__, "ProductID", hd->ProductID);
		return r;
	}

	r = match_regex(data->regex[2], hd->Vendor);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %s !~ %s",
		    __func__, "Vendor", hd->Vendor, data->regex[2]);
*/
		upsdebugx(2, "%s: failed match of %s: %s",
		    __func__, "Vendor", hd->Vendor);
		return r;
	}

	r = match_regex(data->regex[3], hd->Product);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %s !~ %s",
		    __func__, "Product", hd->Product, data->regex[3]);
*/
		upsdebugx(2, "%s: failed match of %s: %s",
		    __func__, "Product", hd->Product);
		return r;
	}

	r = match_regex(data->regex[4], hd->Serial);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %s !~ %s",
		    __func__, "Serial", hd->Serial, data->regex[4]);
*/
		upsdebugx(2, "%s: failed match of %s: %s",
		    __func__, "Serial", hd->Serial);
		return r;
	}

	r = match_regex(data->regex[5], hd->Bus);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %s !~ %s",
		    __func__, "Bus", hd->Bus, data->regex[5]);
*/
		upsdebugx(2, "%s: failed match of %s: %s",
		    __func__, "Bus", hd->Bus);
		return r;
	}

	r = match_regex(data->regex[6], hd->Device);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %s !~ %s",
		    __func__, "Device", hd->Device, data->regex[6]);
*/
		upsdebugx(2, "%s: failed match of %s: %s",
		    __func__, "Device", hd->Device);
		return r;
	}

#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	r = match_regex(data->regex[7], hd->BusPort);
	if (r != 1) {
/*
		upsdebugx(2, "%s: failed match of %s: %s !~ %s",
		    __func__, "Device", hd->Device, data->regex[6]);
*/
		upsdebugx(2, "%s: failed match of %s: %s",
		    __func__, "Bus Port", hd->BusPort);
		return r;
	}
#endif
	return 1;
}

/* constructor: create a regular expression matcher. This matcher is
 * based on seven regular expression strings in regex_array[0..6],
 * corresponding to: vendorid, productid, vendor, product, serial,
 * bus, device. Any of these strings can be NULL, which matches
 * everything. Cflags are as in regcomp(3). Typical values for cflags
 * are REG_ICASE (case insensitive matching) and REG_EXTENDED (use
 * extended regular expressions).  On success, return 0 and store the
 * matcher in *matcher. On error, return -1 with errno set, or return
 * i=1--7 to indicate that the regular expression regex_array[i-1] was
 * ill-formed (an error message can then be retrieved with
 * regerror(3)).
 */
int USBNewRegexMatcher(USBDeviceMatcher_t **matcher, char **regex, int cflags)
{
	int	r, i;
	USBDeviceMatcher_t	*m;
	regex_matcher_data_t	*data;

	m = malloc(sizeof(*m));
	if (!m) {
		return -1;
	}

	data = calloc(1, sizeof(*data));
	if (!data) {
		free(m);
		return -1;
	}

	m->match_function = &match_function_regex;
	m->privdata = (void *)data;
	m->next = NULL;

	for (i=0; i < USBMATCHER_REGEXP_ARRAY_LIMIT; i++) {
		r = compile_regex(&data->regex[i], regex[i], cflags);
		if (r == -2) {
			r = i+1;
		}
		if (r) {
			USBFreeRegexMatcher(m);
			return r;
		}
	}

	/* NOTE: Callers must pre-initialize to NULL! */
	if (matcher && *matcher) {
		free(*matcher);
		*matcher = NULL;
	}

	*matcher = m;

	return 0;
}

void USBFreeRegexMatcher(USBDeviceMatcher_t *matcher)
{
	int	i;
	regex_matcher_data_t	*data;

	if (!matcher) {
		return;
	}

	data = (regex_matcher_data_t *)matcher->privdata;

	for (i = 0; i < USBMATCHER_REGEXP_ARRAY_LIMIT; i++) {
		if (!data->regex[i]) {
			continue;
		}

		regfree(data->regex[i]);
		free(data->regex[i]);
	}

	free(data);
	free(matcher);
}

void warn_if_bad_usb_port_filename(const char *fn) {
	/* USB drivers ignore the 'port' setting - log a notice
	 * if it is not "auto". Note: per se, ignoring the port
	 * (or internally the device_path variable from main.c)
	 * is currently not a bug and is actually documented at
	 * docs/config-notes.txt; however it is not something
	 * evident to users during troubleshooting a device.
	 * Documentation and common practice recommend port=auto
	 * so here we warn during driver start if it has some
	 * other value and users might think it is honoured.
	 */

	if (!fn) {
		upslogx(LOG_WARNING,
			"WARNING: %s(): port argument was not "
			"specified to the driver",
			__func__);
		return;
	}

	if (!strcmp(fn, "auto"))
		return;

	upslogx(LOG_WARNING,
		"WARNING: %s(): port argument specified to\n"
		"  the driver is \"%s\" but USB drivers do "
		"not use it and rely on\n"
		"  libusb walking all devices and matching "
		"their identification metadata.\n"
		"  NUT documentation recommends port=\"auto\" "
		"for USB devices to avoid confusion.",
		__func__, fn);
	return;
}
