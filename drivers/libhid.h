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

#include "config.h"

#include <sys/types.h>
#include <regex.h>
#include "hidtypes.h"

#ifdef HAVE_STDINT_H
#include <stdint.h> /* for uint16_t */
#endif

#include "timehead.h"
#ifdef SHUT_MODE
	struct shut_dev_handle_t_s {
		int upsfd;		/* point to main.c/upsfd */
		char *device_path;
	};
	typedef struct shut_dev_handle_t_s shut_dev_handle_t;
	typedef shut_dev_handle_t hid_dev_handle_t;
#else
	#include <usb.h>
	typedef usb_dev_handle hid_dev_handle_t;
#endif

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE, TRUE } bool_t;
#else
typedef int bool_t;
#endif

/* Device open modes */
#define MODE_OPEN 0   /* open a HID device for the first time */
#define MODE_REOPEN 1 /* reopen a HID device that was opened before */
#define MODE_NOHID 2  /* open a non-HID device; only used by libusb_open() */

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
 * HIDDevice_t: Describe a USB device. This structure contains exactly
 * the 5 pieces of information by which a USB device identifies
 * itself, so it serves as a kind of "fingerprint" of the device. This
 * information must be matched exactly when reopening a device, and
 * therefore must not be "improved" or updated by a client
 * program. Vendor, Product, and Serial can be NULL if the
 * corresponding string did not exist or could not be retrieved.
 *
 * (Note: technically, there is no such thing as a "HID device", but
 * only a "USB device", which can have zero or one or more HID and
 * non-HID interfaces. The HIDDevice_t structure describes a device, not
 * an interface, and it should really be called USBDevice).
 */
typedef struct
{
	uint16_t VendorID; /*!< Device's Vendor ID */
	uint16_t ProductID; /*!< Device's Product ID */
	char*     Vendor; /*!< Device's Vendor Name */
	char*     Product; /*!< Device's Product Name */
	char*     Serial; /* Product serial number */
	char*     Bus;    /* Bus name, e.g. "003" */
} HIDDevice_t;

/*!
 * HIDDeviceMatcher_t: A "USB matcher" is a callback function that
 * inputs a HIDDevice_t structure, and returns 1 for a match and 0
 * for a non-match.  Thus, a matcher provides a criterion for
 * selecting a USB device.  The callback function further is
 * expected to return -1 on error with errno set, and -2 on other
 * errors. Matchers can be connected in a linked list via the
 * "next" field. This is used e.g. in libusb_open() and
 * HIDOpenDevice().
 */

struct HIDDeviceMatcher_s {
	int (*match_function)(HIDDevice_t *d, void *privdata);
	void *privdata;
	struct HIDDeviceMatcher_s *next;
};
typedef struct HIDDeviceMatcher_s HIDDeviceMatcher_t;

/* invoke matcher against device */
static inline int matches(HIDDeviceMatcher_t *matcher, HIDDevice_t *device) {
	if (!matcher) {
		return 1;
	}
	return matcher->match_function(device, matcher->privdata);
}

/* constructors and destructors for specific types of matchers. An
   exact matcher matches a specific HIDDevice_t structure (except for
   the Bus component, which is ignored). A regex matcher matches
   devices based on a set of regular expressions. The new_* functions
   return a matcher on success, or NULL on error with errno set. Note
   that the "free_*" functions only free the current matcher, not any
   others that are linked via "next" fields. */
HIDDeviceMatcher_t *new_exact_matcher(HIDDevice_t *d);
int new_regex_matcher(HIDDeviceMatcher_t **matcher, char *regex_array[6], int cflags);
void free_exact_matcher(HIDDeviceMatcher_t *matcher);
void free_regex_matcher(HIDDeviceMatcher_t *matcher);

/*!
 * Describe a linked list of HID Events (nodes in the HID tree with
 * values). 
 */
struct HIDEvent_s
{
	char*   Path;		/*!< HID Object's fully qualified HID path (allocated) */
	long    Value;		/*!< HID Object Value				*/
	struct HIDEvent_s *next;  /* linked list */
};
typedef struct HIDEvent_s HIDEvent_t;

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

/*!
 * communication_subdriver_s: structure to describe the communication routines
 * @name: can be either "shut" for Serial HID UPS Transfer (from MGE) or "usb"
 */
struct communication_subdriver_s {
	char *name;				/* name of this subdriver		*/
	char *version;				/* version of this subdriver		*/
	int (*open)(hid_dev_handle_t **sdevp,	/* try to open the next available	*/
		HIDDevice_t *curDevice,		/* device matching HIDDeviceMatcher_t	*/
		HIDDeviceMatcher_t *matcher,
		unsigned char *ReportDesc,
		int mode);
	void (*close)(hid_dev_handle_t *sdev);
	int (*get_report)(hid_dev_handle_t *sdev, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*set_report)(hid_dev_handle_t *sdev, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*get_string)(hid_dev_handle_t *sdev,
	int StringIdx, char *string);
	int (*get_interrupt)(hid_dev_handle_t *sdev,
	unsigned char *buf, int bufsize, int timeout);
};
typedef struct communication_subdriver_s communication_subdriver_t;
extern communication_subdriver_t *comm_driver;

/* ---------------------------------------------------------------------- */

/*
 * HIDOpenDevice
 * -------------------------------------------------------------------------- */
HIDDevice_t *HIDOpenDevice(hid_dev_handle_t **udevp, HIDDevice_t *hd, HIDDeviceMatcher_t *matcher, int mode);

/*
 * HIDGetItemValue
 * -------------------------------------------------------------------------- */
int HIDGetItemValue(hid_dev_handle_t *udev, char *path, float *Value, usage_tables_t *utab);

/*
 * HIDGetItemString
 * -------------------------------------------------------------------------- */
char *HIDGetItemString(hid_dev_handle_t *udev, char *path, char *rawbuf, usage_tables_t *utab);

/*
 * HIDSetItemValue
 * -------------------------------------------------------------------------- */
bool_t HIDSetItemValue(hid_dev_handle_t *udev, char *path, float value, usage_tables_t *utab);


/*
 * HIDFreeEvents
 * -------------------------------------------------------------------------- */
void HIDFreeEvents(HIDEvent_t *events);

/*
 * HIDGetEvents
 * -------------------------------------------------------------------------- */
int HIDGetEvents(hid_dev_handle_t *udev, HIDDevice_t *dev, HIDEvent_t **eventsListp, usage_tables_t *utab);

/*
 * HIDCloseDevice
 * -------------------------------------------------------------------------- */
void HIDCloseDevice(hid_dev_handle_t *udev);

/*
 * Support functions
 * -------------------------------------------------------------------------- */
void HIDDumpTree(hid_dev_handle_t *udev, usage_tables_t *utab);

#endif /* _LIBHID_H */
