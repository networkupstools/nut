/*!
 * @file mge-xml.h
 * @brief HID Library - Network MGE XML/HTTP communication sub driver and HID stubs
 *
 * @author Copyright (C) 2007
 *	Arnaud Quette <arnaudquette@eaton.com>
 *
 * This program is sponsored by MGE Office Protection Systems - http://www.mgeops.com
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

#ifndef MGE_XML_H
#define MGE_XML_H

#include "nut_stdint.h" /* for uint16_t */

/*!
 * MGEXMLDevice_t: Describe an MGE XML/HTTP device. This structure contains
 * exactly the 5 pieces of information by which a SHUT device identifies
 * itself, so it serves as a kind of "fingerprint" of the device. This
 * information must be matched exactly when reopening a device, and
 * therefore must not be "improved" or updated by a client
 * program. Vendor, Product, and Serial can be NULL if the
 * corresponding string did not exist or could not be retrieved.
 */
typedef struct MGEXMLDevice_s {
	uint16_t	VendorID; /*!< Device's Vendor ID */
	uint16_t	ProductID; /*!< Device's Product ID */
	char*		Vendor; /*!< Device's Vendor Name */
	char*		Product; /*!< Device's Product Name */
	char*		Serial; /* Product serial number */
	char*		Bus;    /* Bus name, e.g. "003" */
} MGEXMLDevice_t;

/*!
 * mgexml_communication_subdriver_s: structure to describe the communication routines
 */
typedef struct mgexml_communication_subdriver_s {
	char *name;				/* name of this subdriver		*/
	char *version;				/* version of this subdriver		*/
	int (*open)(int *upsfd,			/* try to open the next available	*/
		MGEXMLDevice_t *curDevice,	/* device matching USBDeviceMatcher_t	*/
		char *device_path,
		int (*callback)(int upsfd, MGEXMLDevice_t *hd, unsigned char *rdbuf, int rdlen));
	void (*close)(int upsfd);
	int (*get_report)(int upsfd, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*set_report)(int upsfd, int ReportId,
	unsigned char *raw_buf, int ReportSize );
	int (*get_string)(int upsfd,
	int StringIdx, char *buf, size_t buflen);
	int (*get_interrupt)(int upsfd,
	unsigned char *buf, int bufsize, int timeout);
} mgexml_communication_subdriver_t;

extern mgexml_communication_subdriver_t	mgexml_subdriver;

#endif /* LIBMGE_XML_H */
