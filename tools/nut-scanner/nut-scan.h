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

#include <nutscan-init.h>
#include <nutscan-device.h>
#include <nutscan-ip.h>

/* SNMP structure */
typedef struct nutscan_snmp {
	char * community;
	char * secLevel;
	char * secName;
	char * authPassword;
	char * privPassword;
	char * authProtocol;
	char * privProtocol;
	char * peername;
	void * handle;
} nutscan_snmp_t;

/* Scanning */
nutscan_device_t * nutscan_scan_snmp(const char * start_ip,const char * stop_ip,long usec_timeout, nutscan_snmp_t * sec);

nutscan_device_t * nutscan_scan_usb();

nutscan_device_t * nutscan_scan_xml_http(long usec_timeout);

nutscan_device_t * nutscan_scan_nut(const char * startIP, const char * stopIP, const char * port, long usec_timeout);

nutscan_device_t * nutscan_scan_avahi(long usec_timeout);

nutscan_device_t *  nutscan_scan_ipmi(void);

/* Display functions */
void nutscan_display_ups_conf(nutscan_device_t * device);
void nutscan_display_parsable(nutscan_device_t * device);

#endif
