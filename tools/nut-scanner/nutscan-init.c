/* nutscan-init.c: init functions for nut scanner library
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

#include "common.h"

int nutscan_avail_avahi = 0;
int nutscan_avail_ipmi = 0;
int nutscan_avail_nut = 0;
int nutscan_avail_snmp = 0;
int nutscan_avail_usb = 0;
int nutscan_avail_xml_http = 0;

int nutscan_load_usb_library(void);
int nutscan_load_snmp_library(void);
int nutscan_load_neon_library(void);
int nutscan_load_avahi_library(void);
int nutscan_load_ipmi_library(void);
int nutscan_load_upsclient_library(void);

void nutscan_init(void)
{
#ifdef WITH_USB
	nutscan_avail_usb = nutscan_load_usb_library();
#endif
#ifdef WITH_SNMP
	nutscan_avail_snmp = nutscan_load_snmp_library();
#endif
#ifdef WITH_NEON
	nutscan_avail_xml_http = nutscan_load_neon_library();
#endif
#ifdef WITH_AVAHI
	nutscan_avail_avahi = nutscan_load_avahi_library();
#endif
#ifdef WITH_FREEIPMI
	nutscan_avail_ipmi = nutscan_load_ipmi_library();
#endif
	nutscan_avail_nut = nutscan_load_upsclient_library();
}
