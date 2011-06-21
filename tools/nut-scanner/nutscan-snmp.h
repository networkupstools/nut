/* nutscan-snmp
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

#ifndef DEVSCAN_SNMP_H
#define DEVSCAN_SNMP_H

typedef struct {
        char *          oid;
        char *          mib;
} snmp_device_id_t;

/* SNMP IDs device table */
static snmp_device_id_t snmp_device_table[] = {
	{ "1.3.6.1.4.1.534.1.1.2.0" ,  "pw"},
	{ "1.3.6.1.4.1.232.165.3.1.1.0" ,  "cpqpower"},
	{ ".1.3.6.1.4.1.13742.1.1.12.0" ,  "raritan"},
	{ "1.3.6.1.2.1.33.1.1.1.0" ,  "ietf"},
	{ ".1.3.6.1.4.1.318.1.1.1.1.1.1.0" ,  "apcc"},
	{ ".1.3.6.1.4.1.4779.1.3.5.2.1.24.1" ,  "baytech"},
	{ ".1.3.6.1.4.1.4555.1.1.1.1.1.1.0" ,  "netvision"},
	{ ".1.3.6.1.4.1.17373.3.1.1.0" ,  "aphel_genesisII"},
	{ ".1.3.6.1.4.1.534.6.6.6.1.1.12.0" ,  "aphel_revelation"},
	{ ".1.3.6.1.4.1.2947.1.1.2.0" ,  "bestpower"},
	{ ".1.3.6.1.4.1.705.1.1.1.0" ,  "mge"},
        /* Terminating entry */
        { NULL, NULL }
};
#endif /* DEVSCAN_SNMP_H */
