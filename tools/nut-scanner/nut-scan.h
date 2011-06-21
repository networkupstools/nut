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
#ifndef NUT_SCAN_H
#define NUT_SCAN_H

#include "device.h"
#include "ip.h"

/* SNMP structure */
typedef struct snmp_security {
	char * community;
	char * secLevel;
	char * secName;
	char * authPassword;
	char * privPassword;
	char * authProtocol;
	char * privProtocol;
	char * peername;
	void * handle;
} snmp_security_t;

/* Scanning */
void scan_avahi();

void scan_ipmi();

void scan_nut();

device_t * scan_snmp(char * start_ip, char * stop_ip,long usec_timeout, snmp_security_t * sec);

device_t * scan_usb();

device_t * scan_xml_http(long usec_timeout);

/* Displaying */
void display_ups_conf(device_t * device);

#endif
