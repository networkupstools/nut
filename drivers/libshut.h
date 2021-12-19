/*!
 * @file libshut.h
 * @brief HID Library - Generic serial SHUT backend for Generic HID Access (using MGE HIDParser)
 *        SHUT stands for Serial HID UPS Transfer, and was created by MGE UPS SYSTEMS
 *
 * @author Copyright (C) 2006 - 2007
 *	Arnaud Quette <aquette.dev@gmail.com>
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

#ifndef NUT_LIBSHUT_H_SEEN
#define NUT_LIBSHUT_H_SEEN 1

#include "main.h"	/* for subdrv_info_t */
#include "nut_stdint.h"	/* for uint16_t, size_t, PRIsize etc. */

extern upsdrv_info_t comm_upsdrv_info;

/* These typedefs are also named in usb-common.h (=> nut_libusb.h), adhering
 * to one or another libusb API version. For consistency of "ifdef SHUT_MODE"
 * handling in libhid.c and some drivers, these symbolic names are used in
 * all the headers and are expected to match binary code of object files at
 * (monolithic) driver build time.
 *
 * The MIN/MAX definitions here are primarily to generalize range-check
 * code (especially if anything is done outside the libraries).
 * FIXME: It may make sense to constrain the limits to lowest common
 * denominator that should fit alll of libusb-0.1, libusb-1.0 and libshut,
 * so that any build of the practical (driver) code knows to not exceed
 * any use-case.
 *
 * Types below were mined from existing method signatures; see also the
 * my_hid_descriptor struct in libshut.c for practical fixed-size types.
 */

/* Essentially the file descriptor type, "int" - as in ser_get_char() etc.: */
typedef int usb_dev_handle;

/* Originally "int" cast to "uint8_t" in shut_control_msg(),
 * and "unsigned char" in shut_get_descriptor() */
typedef unsigned char usb_ctrl_requesttype;
#define USB_CTRL_REQUESTTYPE_MIN	0
#define USB_CTRL_REQUESTTYPE_MAX	UCHAR_MAX

typedef int usb_ctrl_request;
#define USB_CTRL_REQUEST_MIN	INT_MIN
#define USB_CTRL_REQUEST_MAX	INT_MAX

typedef int usb_ctrl_endpoint;
#define USB_CTRL_ENDPOINT_MIN	INT_MIN
#define USB_CTRL_ENDPOINT_MAX	INT_MAX

typedef int usb_ctrl_msgvalue;
#define USB_CTRL_MSGVALUE_MIN	INT_MIN
#define USB_CTRL_MSGVALUE_MAX	INT_MAX

typedef int usb_ctrl_repindex;
#define USB_CTRL_REPINDEX_MIN	INT_MIN
#define USB_CTRL_REPINDEX_MAX	INT_MAX

typedef int usb_ctrl_strindex;
#define USB_CTRL_STRINDEX_MIN	INT_MIN
#define USB_CTRL_STRINDEX_MAX	INT_MAX

typedef unsigned char usb_ctrl_descindex;
#define USB_CTRL_DESCINDEX_MIN	0
#define USB_CTRL_DESCINDEX_MAX	UCHAR_MAX

/* Here MIN/MAX should not matter much, type mostly used for casting */
typedef unsigned char* usb_ctrl_charbuf;
typedef unsigned char usb_ctrl_char;
#define USB_CTRL_CHAR_MIN	0
#define USB_CTRL_CHAR_MAX	UCHAR_MAX

typedef size_t usb_ctrl_charbufsize;	/*typedef int usb_ctrl_charbufsize;*/
#define USB_CTRL_CHARBUFSIZE_MIN	0
#define USB_CTRL_CHARBUFSIZE_MAX	SIZE_MAX
#define PRI_NUT_USB_CTRL_CHARBUFSIZE PRIsize

typedef int usb_ctrl_timeout_msec;	/* in milliseconds */
#define USB_CTRL_TIMEOUTMSEC_MIN	INT_MIN
#define USB_CTRL_TIMEOUTMSEC_MAX	INT_MAX

/*!
 * SHUTDevice_t: Describe a SHUT device. This structure contains exactly
 * the 5 pieces of information by which a SHUT device identifies
 * itself, so it serves as a kind of "fingerprint" of the device. This
 * information must be matched exactly when reopening a device, and
 * therefore must not be "improved" or updated by a client
 * program. Vendor, Product, and Serial can be NULL if the
 * corresponding string did not exist or could not be retrieved.
 */
typedef struct SHUTDevice_s {
	uint16_t	VendorID;  /*!< Device's Vendor ID    */
	uint16_t	ProductID; /*!< Device's Product ID   */
	char*		Vendor;    /*!< Device's Vendor Name  */
	char*		Product;   /*!< Device's Product Name */
	char*		Serial;    /*!< Product serial number */
	char*		Bus;       /*!< Bus name, e.g. "003"  */
	uint16_t	bcdDevice; /*!< Device release number */
	char		*Device;   /*!< Device name on the bus, e.g. "001"  */
} SHUTDevice_t;

/*!
 * shut_communication_subdriver_s: structure to describe the communication routines
 */
typedef struct shut_communication_subdriver_s {
	const char *name;				/* name of this subdriver		*/
	const char *version;			/* version of this subdriver	*/

	int (*open)(usb_dev_handle *upsfd,			/* try to open the next available	*/
		SHUTDevice_t *curDevice,	/* device matching USBDeviceMatcher_t	*/
		char *device_path,
		int (*callback)(usb_dev_handle upsfd, SHUTDevice_t *hd,
			usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen));

	void (*close)(usb_dev_handle upsfd);

	int (*get_report)(usb_dev_handle upsfd, usb_ctrl_repindex ReportId,
		usb_ctrl_charbuf raw_buf, usb_ctrl_charbufsize ReportSize);

	int (*set_report)(usb_dev_handle upsfd, usb_ctrl_repindex ReportId,
		usb_ctrl_charbuf raw_buf, usb_ctrl_charbufsize ReportSize);

	int (*get_string)(usb_dev_handle upsfd,
		usb_ctrl_strindex StringIdx, char *buf, usb_ctrl_charbufsize buflen);

	int (*get_interrupt)(usb_dev_handle upsfd,
		usb_ctrl_charbuf buf, usb_ctrl_charbufsize bufsize,
		usb_ctrl_timeout_msec timeout);
} shut_communication_subdriver_t;

extern shut_communication_subdriver_t	shut_subdriver;

/*!
 * Notification levels
 * These are however not processed currently
 */
#define OFF_NOTIFICATION	1	/* notification off */
#define LIGHT_NOTIFICATION	2	/* light notification */
#define COMPLETE_NOTIFICATION	3	/* complete notification for UPSs which do */
					/* not support disabling it like some early */
					/* Ellipse models */
#define DEFAULT_NOTIFICATION COMPLETE_NOTIFICATION

#endif /* NUT_LIBSHUT_H_SEEN */
