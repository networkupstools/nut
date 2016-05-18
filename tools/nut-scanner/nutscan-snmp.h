/* nutscan-snmp
 *  Copyright (C) 2011 - Frederic Bohe <FredericBohe@Eaton.com>
 *  Copyright (C) 2016 - Arnaud Quette <ArnaudQuette@Eaton.com>
 *  Copyright (C) 2016 - Jim Klimov <EvgenyKlimov@Eaton.com>
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


#endif /* DEVSCAN_SNMP_H */

/* SNMP IDs device table, excerpt generated from our available MIBs */
/* Note: This is commented away with ifdefs, so the consumers who only need
 * the structure definition are not burdened with an external reference to
 * structure instances they would not need.
 */

#if WANT_DEVSCAN_SNMP_BUILTIN == 1
# ifndef DEVSCAN_SNMP_BUILTIN
#  define DEVSCAN_SNMP_BUILTIN
/* Can use a copy of the structure that was pre-compiled into the binary */
    extern snmp_device_id_t *snmp_device_table_builtin;
# endif /* DEVSCAN_SNMP_BUILTIN */
#endif /* WANT_DEVSCAN_SNMP_BUILTIN */
