/* usb-common.h - prototypes for the common useful USB functions

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

#ifndef NUT_USB_COMMON_H
#define NUT_USB_COMMON_H

#include "nut_stdint.h"	/* for uint16_t */

#include <regex.h>

#if (!WITH_LIBUSB_1_0) && (!WITH_LIBUSB_0_1)
#error "configure script error: Neither WITH_LIBUSB_1_0 nor WITH_LIBUSB_0_1 is set"
#endif

#if (WITH_LIBUSB_1_0) && (WITH_LIBUSB_0_1)
#error "configure script error: Both WITH_LIBUSB_1_0 and WITH_LIBUSB_0_1 are set"
#endif

#if WITH_LIBUSB_1_0
#include <libusb.h>
#endif
#if WITH_LIBUSB_0_1
#include <usb.h>
#endif

/* USB standard timeout [ms] */
#define USB_TIMEOUT 5000

/*!
 * USBDevice_t: Describe a USB device. This structure contains exactly
 * the 5 pieces of information by which a USB device identifies
 * itself, so it serves as a kind of "fingerprint" of the device. This
 * information must be matched exactly when reopening a device, and
 * therefore must not be "improved" or updated by a client
 * program. Vendor, Product, and Serial can be NULL if the
 * corresponding string did not exist or could not be retrieved.
 */
typedef struct USBDevice_s {
	/* These 5 data points are common properties of an USB device: */
	uint16_t	VendorID;  /*!< Device's Vendor ID    */
	uint16_t	ProductID; /*!< Device's Product ID   */
	char		*Vendor;   /*!< Device's Vendor Name  */
	char		*Product;  /*!< Device's Product Name */
	char		*Serial;   /*!< Product serial number */
	/* These data points can be determined by the driver for some devices
	   or by libusb to detail its connection topology: */
	char		*Bus;      /*!< Bus name, e.g. "003"  */
	uint16_t	bcdDevice; /*!< Device release number */
	char		*Device;   /*!< Device name on the bus, e.g. "001"  */
} USBDevice_t;

/*!
 * USBDeviceMatcher_t: A "USB matcher" is a callback function that
 * inputs a USBDevice_t structure, and returns 1 for a match and 0
 * for a non-match.  Thus, a matcher provides a criterion for
 * selecting a USB device.  The callback function further is
 * expected to return -1 on error with errno set, and -2 on other
 * errors. Matchers can be connected in a linked list via the
 * "next" field.
 */
typedef struct USBDeviceMatcher_s {
	int	(*match_function)(USBDevice_t *device, void *privdata);
	void	*privdata;
	struct USBDeviceMatcher_s	*next;
} USBDeviceMatcher_t;

/* constructors and destructors for specific types of matchers. An
   exact matcher matches a specific usb_device_t structure (except for
   the Bus component, which is ignored). A regex matcher matches
   devices based on a set of regular expressions. The USBNew* functions
   return a matcher on success, or -1 on error with errno set. Note
   that the "USBFree*" functions only free the current matcher, not
   any others that are linked via "next" fields. */
int USBNewExactMatcher(USBDeviceMatcher_t **matcher, USBDevice_t *hd);
int USBNewRegexMatcher(USBDeviceMatcher_t **matcher, char **regex, int cflags);
void USBFreeExactMatcher(USBDeviceMatcher_t *matcher);
void USBFreeRegexMatcher(USBDeviceMatcher_t *matcher);

/* dummy USB function and macro, inspired from the Linux kernel
 * this allows USB information extraction */
#define USB_DEVICE(vendorID, productID)	vendorID, productID

typedef struct {
	uint16_t	vendorID;
	uint16_t	productID;
	void	*(*fun)(USBDevice_t *);		/* handler for specific processing */
} usb_device_id_t;

#define NOT_SUPPORTED		0
#define POSSIBLY_SUPPORTED	1
#define SUPPORTED			2

/* Function used to match a VendorID/ProductID pair against a list of
 * supported devices. Return values:
 * NOT_SUPPORTED (0), POSSIBLY_SUPPORTED (1) or SUPPORTED (2) */
int is_usb_device_supported(usb_device_id_t *usb_device_id_list,
							USBDevice_t *device);

void nut_usb_addvars(void);

#endif /* NUT_USB_COMMON_H */
