/*!
 * @file libusb.h
 * @brief HID Library - Generic USB backend for Generic HID Access (using MGE HIDParser)
 *
 * @author Copyright (C)
 *	2003 - 2006 Arnaud Quette <aquette.dev@gmail.com>
 *      2005 Peter Selinger <selinger@users.sourceforge.net>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 *
 *      The logic of this file is ripped from mge-shut driver (also from
 *      Arnaud Quette), which is a "HID over serial link" UPS driver for
 *      Network UPS Tools <http://www.networkupstools.org/>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

#ifndef LIBUSB_H
#define LIBUSB_H

#include <usb.h> /* libusb header file */
#include <regex.h>

#include "nut_stdint.h" /* for uint16_t */

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
	uint16_t	VendorID; /*!< Device's Vendor ID */
	uint16_t	ProductID; /*!< Device's Product ID */
	char*		Vendor; /*!< Device's Vendor Name */
	char*		Product; /*!< Device's Product Name */
	char*		Serial; /* Product serial number */
	char*		Bus;    /* Bus name, e.g. "003" */
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
	int (*match_function)(USBDevice_t *d, void *privdata);
	void *privdata;
	struct USBDeviceMatcher_s *next;
} USBDeviceMatcher_t;

/* invoke matcher against device */
static inline int matches(USBDeviceMatcher_t *matcher, USBDevice_t *device) {
	if (!matcher) {
		return 1;
	}
	return matcher->match_function(device, matcher->privdata);
}

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

/*!
 * usb_communication_subdriver_s: structure to describe the communication routines
 * @name: can be either "shut" for Serial HID UPS Transfer (from MGE) or "usb"
 */
typedef struct usb_communication_subdriver_s {
	char *name;				/* name of this subdriver		*/
	char *version;				/* version of this subdriver		*/
	int (*open)(usb_dev_handle **sdevp,	/* try to open the next available	*/
		USBDevice_t *curDevice,		/* device matching USBDeviceMatcher_t	*/
		USBDeviceMatcher_t *matcher,
		int (*callback)(usb_dev_handle *udev, USBDevice_t *hd, unsigned char *rdbuf, int rdlen));
	void (*close)(usb_dev_handle *sdev);
	int (*get_report)(usb_dev_handle *sdev, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*set_report)(usb_dev_handle *sdev, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*get_string)(usb_dev_handle *sdev,
	int StringIdx, char *buf, size_t buflen);
	int (*get_interrupt)(usb_dev_handle *sdev,
	unsigned char *buf, int bufsize, int timeout);
} usb_communication_subdriver_t;

extern usb_communication_subdriver_t	usb_subdriver;

#endif /* LIBUSB_H */

