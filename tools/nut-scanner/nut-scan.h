/* nut-scan.h: detect NUT services
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

#include "device.h"

/* Scanning */
void scan_avahi();

void scan_ipmi();

void scan_nut();

void scan_snmp();

device_t * scan_usb();

void scan_xml_http();

/* Displaying */
void display_ups_conf(device_t * device);
