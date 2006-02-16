/*!
 * @file libhid.h
 * @brief HID Library - User API
 *
 * @author Copyright (C) 2003
 *      Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *      Charles Lepple <clepple@ghz.cc>
 *      2005 Peter Selinger <selinger@users.sourceforge.net>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
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

#ifndef _LIBHID_H
#define _LIBHID_H

#include <sys/types.h>
#include <regex.h>
#include <usb.h>
#include "timehead.h"
#include "hidtypes.h"

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE, TRUE } bool;
#else
typedef int bool;
#endif

/* Device open modes */
#define MODE_OPEN 0
#define MODE_REOPEN 1

#define MAX_TS			2		/* validity period of a gotten report (2 sec) */

/* ---------------------------------------------------------------------- */

/* structure to describe an item in a usage table */
typedef struct {
	const char *usage_name;
	unsigned int usage_code;
} usage_lkp_t;

extern usage_lkp_t hid_usage_lkp[];

/* an object of type usage_tables_t is a NULL-terminated array of
 * pointers to individual usage tables. */
typedef usage_lkp_t *usage_tables_t;

/*!
 * HIDDevice: Describe a USB device. This structure contains exactly
 * the 5 pieces of information by which a USB device identifies
 * itself, so it serves as a kind of "fingerprint" of the device. This
 * information must be matched exactly when reopening a device, and
 * therefore must not be "improved" or updated by a client
 * program. Vendor, Product, and Serial can be NULL if the
 * corresponding string did not exist or could not be retrieved.
 *
 * (Note: technically, there is no such thing as a "HID device", but
 * only a "USB device", which can have zero or one or more HID and
 * non-HID interfaces. The HIDDevice structure describes a device, not
 * an interface, and it should really be called USBDevice).
 */
typedef struct
{
	u_int16_t VendorID; /*!< Device's Vendor ID */
	u_int16_t ProductID; /*!< Device's Product ID */
	char*     Vendor; /*!< Device's Vendor Name */
	char*     Product; /*!< Device's Product Name */
	char*     Serial; /* Product serial number */
	char*     Bus;    /* Bus name, e.g. "003" */
} HIDDevice;

/* A "USB matcher" is a callback function that inputs a HIDDevice
   structure, and returns 1 for a match and 0 for a non-match.  Thus,
   a matcher provides a criterion for selecting a USB device.  The
   callback function further is expected to return -1 on error with
   errno set, and -2 on other errors. Matchers can be connected in a
   linked list via the "next" field. This is used e.g. in
   libusb_open() and HIDOpenDevice(). */

struct HIDDeviceMatcher_s {
	int (*match_function)(HIDDevice *d, void *privdata);
	void *privdata;
	struct HIDDeviceMatcher_s *next;
};
typedef struct HIDDeviceMatcher_s HIDDeviceMatcher_t;

/* invoke matcher against device */
static inline int matches(HIDDeviceMatcher_t *matcher, HIDDevice *device) {
	if (!matcher) {
		return 1;
	}
	return matcher->match_function(device, matcher->privdata);
}

/* constructors and destructors for specific types of matchers. An
   exact matcher matches a specific HIDDevice structure (except for
   the Bus component, which is ignored). A regex matcher matches
   devices based on a set of regular expressions. The new_* functions
   return a matcher on success, or NULL on error with errno set. Note
   that the "free_*" functions only free the current matcher, not any
   others that are linked via "next" fields. */
HIDDeviceMatcher_t *new_exact_matcher(HIDDevice *d);
int new_regex_matcher(HIDDeviceMatcher_t **matcher, char *regex_array[6], int cflags);
void free_exact_matcher(HIDDeviceMatcher_t *matcher);
void free_regex_matcher(HIDDeviceMatcher_t *matcher);

/*!
 * Describe a HID Item (a node in the HID tree)
 */
 typedef struct
{
	char*   Path;			/*!< HID Object's fully qualified HID path	*/
	long    Value;		/*!< HID Object Value					*/
} HIDItem;

/* Describe a set of values to match for finding a special HID device.
 * This is given by a set of (compiled) regular expressions. If any
 * expression is NULL, it matches anything. The second set of values
 * are the original (not compiled) regular expression strings. They
 * are only used to product human-readable log messages, but not for
 * the actual matching. */

struct MatchFlags_s {
	regex_t *re_Vendor;
	regex_t *re_VendorID;
	regex_t *re_Product;
	regex_t *re_ProductID;
	regex_t *re_Serial;
};
typedef struct MatchFlags_s MatchFlags_t;

/* ---------------------------------------------------------------------- */

/*
 * HIDOpenDevice
 * -------------------------------------------------------------------------- */
HIDDevice *HIDOpenDevice(usb_dev_handle **udevp, HIDDevice *hd, HIDDeviceMatcher_t *matcher, int mode);

/*
 * HIDGetItem
 * -------------------------------------------------------------------------- */
HIDItem *HIDGetItem(const char *ItemPath);

/*
 * HIDGetItemValue
 * -------------------------------------------------------------------------- */
int HIDGetItemValue(usb_dev_handle *udev, char *path, float *Value, usage_tables_t *utab);

/*
 * HIDGetItemString
 * -------------------------------------------------------------------------- */
char *HIDGetItemString(usb_dev_handle *udev, char *path, unsigned char *rawbuf, usage_tables_t *utab);

/*
 * HIDSetItemValue
 * -------------------------------------------------------------------------- */
bool HIDSetItemValue(usb_dev_handle *udev, char *path, float value, usage_tables_t *utab);

/*
 * HIDGetNextEvent
 * -------------------------------------------------------------------------- */
int HIDGetEvents(usb_dev_handle *udev, HIDDevice *dev, HIDItem **eventsList, usage_tables_t *utab);

/*
 * HIDCloseDevice
 * -------------------------------------------------------------------------- */
void HIDCloseDevice(usb_dev_handle *udev);

/*
 * Support functions
 * -------------------------------------------------------------------------- */
int get_current_data_attribute();
void HIDDumpTree(usb_dev_handle *udev, usage_tables_t *utab);

#endif /* _LIBHID_H */
