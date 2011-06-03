/* device.h: definition of a container describing a NUT device
 * 
 *  Copyright (C) 2011 - Frederic Bohe <fredericbohe@eaton.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef SCAN_DEVICE
#define SCAN_DEVICE

typedef enum device_type {
	TYPE_NONE=0,
	TYPE_USB,
	TYPE_SNMP,
	TYPE_XML,
	TYPE_NUT_OLD,
	TYPE_NUT_AVAHI,
	TYPE_IPMI
} device_type_t;

typedef struct usb_options {
	int	vendorid;
	int	productid;
	char *	vendor_name;
	char *	product_name;
	char *	serial_number;
	char *	bus;
} usb_options_t;

typedef struct snmp_options {
} snmp_options_t;

typedef struct xml_options {
} xml_options_t;

typedef struct nut_old_options {
} nut_old_options_t;

typedef struct nut_avahi_options {
} nut_avahi_options_t;

typedef struct ipmi_options {
} ipmi_options_t;

typedef union options {
	usb_options_t           usb_opt;
	snmp_options_t          snmp_opt;
	xml_options_t           xml_opt;
	nut_old_options_t       nut_old_opt;
	nut_avahi_options_t     nut_avahi_opt;
	ipmi_options_t          ipmi_opt;
} options_t;

typedef struct device {
	device_type_t	type;
	char *		driver;
	char *		port;
	options_t	opt;
	struct device * prev;
	struct device * next;
} device_t;

device_t * new_device();
void free_device(device_t * device);
#endif
