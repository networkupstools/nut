/*!
 * @file nut_libusb.h
 * @brief HID Library - Generic USB backend for Generic HID Access (using MGE HIDParser)
 *
 * @author Copyright (C)
 *      2003 - 2016 Arnaud Quette <aquette.dev@gmail.com>
 *      2005 Peter Selinger <selinger@users.sourceforge.net>
 *      2021 Jim Klimov <jimklimov+nut@gmail.com>
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

#ifndef NUT_LIBUSB_H_SEEN
#define NUT_LIBUSB_H_SEEN 1

#include "main.h"	/* for subdrv_info_t */
#include "usb-common.h"	/* for USBDevice_t and USBDeviceMatcher_t,
                         * and for libusb headers and 0.1/1.0 mapping */

/* Used in drivers/libusb*.c sources: */
#define LIBUSB_DEFAULT_INTERFACE        0
#define LIBUSB_DEFAULT_DESC_INDEX       0
#define LIBUSB_DEFAULT_HID_EP_IN        1
#define LIBUSB_DEFAULT_HID_EP_OUT       1

extern upsdrv_info_t comm_upsdrv_info;

/*!
 * usb_communication_subdriver_s: structure to describe the communication routines
 * @name: can be either "shut" for Serial HID UPS Transfer (from MGE) or "usb"
 */
typedef struct usb_communication_subdriver_s {
	const char *name;				/* name of this subdriver		*/
	const char *version;			/* version of this subdriver	*/

	int (*open)(usb_dev_handle **sdevp,	/* try to open the next available	*/
		USBDevice_t *curDevice,		/* device matching USBDeviceMatcher_t	*/
		USBDeviceMatcher_t *matcher,
		int (*callback)(usb_dev_handle *udev, USBDevice_t *hd,
			usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen));

	void (*close)(usb_dev_handle *sdev);

	int (*get_report)(usb_dev_handle *sdev, usb_ctrl_repindex ReportId,
		usb_ctrl_charbuf raw_buf, usb_ctrl_charbufsize ReportSize);

	int (*set_report)(usb_dev_handle *sdev, usb_ctrl_repindex ReportId,
		usb_ctrl_charbuf raw_buf, usb_ctrl_charbufsize ReportSize);

	int (*get_string)(usb_dev_handle *sdev,
		usb_ctrl_strindex StringIdx, char *buf, usb_ctrl_charbufsize buflen);

	int (*get_interrupt)(usb_dev_handle *sdev,
		usb_ctrl_charbuf buf, usb_ctrl_charbufsize bufsize,
		usb_ctrl_timeout_msec timeout);

	/* Used for Powervar UPS or similar cases to make sure
	 * we use the right interface in the Composite device.
	 * In a few cases our libusb*.c sets the value for specific
	 * VID/PID combinations, in others the subdrivers do so.
	 * FIXME: The numeric value here seems to fit and
	 * gets used in several contexts, it may be cleaner
	 * to separate them eventually. Usages in NUT were
	 * seen as following libusb API (1.0 and 0.1) args,
	 * along with some hints from libusb sources/headers:
	 * libusb-1.0:
	 *   libusb_claim_interface - "int interface_number"
	 *   libusb_detach_kernel_driver - "int interface_number"
	 *   libusb_get_config_descriptor - "uint8_t config_index" ("the index of the configuration you wish to retrieve")
	 *   libusb_control_transfer - "uint16_t wIndex" ("the index field for the setup packet"; "should be given in host-endian byte order")
	 * libusb-0.1: // The parameters mirror the types of the same name in the USB specification
	 *   usb_claim_interface - "int interface"
	 *   usb_control_msg - "int index"
	 * (_np = non-portable => only on some systems, like FreeBSD)
	 *   usb_detach_kernel_driver_np - "int interface"
	 */
	usb_ctrl_repindex hid_rep_index;		/* number of the interface we use in a composite USB device; see comments above */

	/* All devices use HID descriptor at index 0.
	 * However, some UPS like newer Eaton units have
	 * a light HID descriptor at index 0, and
	 * the full version is at index 1 (in which
	 * case, bcdDevice == 0x0202)
	 */
	usb_ctrl_descindex hid_desc_index;		/* HID descriptor is at this index (non-trivial for composite USB devices); see comments above */
	usb_ctrl_endpoint hid_ep_in;			/* Input interrupt endpoint. Default is 1	*/
	usb_ctrl_endpoint hid_ep_out;			/* Output interrupt endpoint. Default is 1	*/
} usb_communication_subdriver_t;

extern usb_communication_subdriver_t	usb_subdriver;

#endif /* NUT_LIBUSB_H_SEEN */
