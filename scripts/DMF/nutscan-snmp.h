/* nutscan-snmp
 *  Copyright (C) 2011 - Frederic Bohe <FredericBohe@Eaton.com>
 *  Copyright (C) 2016 - Arnaud Quette <ArnaudQuette@Eaton.com>
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
        char *       sysoid;
} snmp_device_id_t;

/* SNMP IDs device table */
/*static snmp_device_id_t snmp_device_table[] = {
	{ ".1.3.6.1.4.1.534.10.2.1.2.0",  "eaton_ats", ".1.3.6.1.4.1.705.1"},
	{ ".1.3.6.1.4.1.13742.6.3.2.1.1.3.1",  "raritan-px2", ".1.3.6.1.4.1.13742.6"},
	{ ".1.3.6.1.4.1.2947.1.1.2.0",  "bestpower", NULL},
	{ ".1.3.6.1.4.1.17373.3.1.1.0",  "aphel_genesisII", ".1.3.6.1.4.1.17373"},
	{ ".1.3.6.1.4.1.534.6.6.6.1.1.12.0",  "aphel_revelation", ".1.3.6.1.4.1.534.6.6.6"},
	{ ".1.3.6.1.4.1.534.6.6.7.1.2.1.2.0",  "eaton_epdu", ".1.3.6.1.4.1.534.6.6.7"},
	{ ".1.3.6.1.4.1.20677.1",  "pulizzi_switched1", ".1.3.6.1.4.1.20677.1"},
	{ ".1.3.6.1.4.1.20677.1",  "pulizzi_switched2", ".1.3.6.1.4.1.20677.2"},
	{ ".1.3.6.1.4.1.705.1.1.1.0",  "mge", ".1.3.6.1.4.1.705.1"},
	{ "1.3.6.1.4.1.534.1.1.2.0",  "pw", ".1.3.6.1.4.1.534.1"},
	{ "1.3.6.1.4.1.534.1.1.2.0",  "pxgx_ups", ".1.3.6.1.4.1.534.2.12"},
	{ ".1.3.6.1.4.1.3808.1.1.1.1.1.1.0",  "cyberpower", ".1.3.6.1.4.1.3808"},
	{ ".1.3.6.1.4.1.13742.1.1.12.0",  "raritan", ".1.3.6.1.4.1.13742"},
	{ "1.3.6.1.2.1.33.1.1.1.0",  "ietf", ".1.3.6.1.2.1.33"},
	{ "",  "ietf", ".1.3.6.1.4.1.850.1"},
	{ ".1.3.6.1.4.1.4779.1.3.5.2.1.24.1",  "baytech", NULL},
	{ "",  "huawei", ".1.3.6.1.4.1.8072.3.2.10"},
	{ ".1.3.6.1.4.1.232.165.3.1.1.0",  "cpqpower", ".1.3.6.1.4.1.232.165.3"},
	{ "",  "apc_ats", ".1.3.6.1.4.1.318.1.3.11"},
	{ "",  "xppc", ".1.3.6.1.4.1.935"},
	{ ".1.3.6.1.4.1.4555.1.1.1.1.1.1.0",  "netvision", ".1.3.6.1.4.1.4555.1.1.1"},
	{ "",  "delta_ups", ".1.3.6.1.4.1.2254.2.4"},
	{ ".1.3.6.1.4.1.318.1.1.1.1.1.1.0",  "apcc", NULL},*/
        /* Terminating entry */
        //{ NULL, NULL, NULL}
//};
#endif /* DEVSCAN_SNMP_H */
