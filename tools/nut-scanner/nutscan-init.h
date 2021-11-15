/*
 *  Copyright (C) 2011 - EATON
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

/*! \file nutscan-init.h
    \brief initialisation data
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#ifndef SCAN_INIT
#define SCAN_INIT

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

extern int nutscan_avail_avahi;
extern int nutscan_avail_ipmi;
extern int nutscan_avail_nut;
extern int nutscan_avail_snmp;
extern int nutscan_avail_usb;
extern int nutscan_avail_xml_http;

void nutscan_init(void);
void nutscan_free(void);

#define DEFAULT_THREAD  512

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
