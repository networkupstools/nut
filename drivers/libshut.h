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
#include "nut_stdint.h"	/* for uint16_t */

extern upsdrv_info_t comm_upsdrv_info;

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
} SHUTDevice_t;

/*!
 * shut_communication_subdriver_s: structure to describe the communication routines
 */
typedef struct shut_communication_subdriver_s {
	const char *name;				/* name of this subdriver		*/
	const char *version;				/* version of this subdriver		*/
	int (*open)(int *upsfd,			/* try to open the next available	*/
		SHUTDevice_t *curDevice,	/* device matching USBDeviceMatcher_t	*/
		char *device_path,
		int (*callback)(int upsfd, SHUTDevice_t *hd, unsigned char *rdbuf, int rdlen));
	void (*close)(int upsfd);
	int (*get_report)(int upsfd, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*set_report)(int upsfd, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*get_string)(int upsfd,
	int StringIdx, char *buf, size_t buflen);
	int (*get_interrupt)(int upsfd,
	unsigned char *buf, int bufsize, int timeout);
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
