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

#include <stddef.h> /*define NULL */

typedef struct {
        char *          oid;
        char *          mib;
        char *       sysoid;
} snmp_device_id_t;


#endif /* DEVSCAN_SNMP_H */

/* SNMP IDs device table, excerpt generated from our available MIBs */
/* The consumer defines an instance of this table, either dynamic with DMF
 * or a precompiled legacy binary based on ifdef WITH_DMFMIB compile-time
 * support and real-time DMF availability (as fallback for no/bad/empty DMF),
 * with explicit reference like below (builtin generated into nutscan-snmp.c).
 * Note: This is commented away with ifdefs, so the consumers who only need
 * the structure definition are not burdened with an external reference to
 * structure instances they would not need.
 */

#ifdef __cplusplus
extern "C" {
#endif

#if WANT_DEVSCAN_SNMP_BUILTIN == 1
# ifndef DEVSCAN_SNMP_BUILTIN
#  define DEVSCAN_SNMP_BUILTIN
/* Can use a copy of the structure that was pre-compiled into the binary */
    extern snmp_device_id_t *snmp_device_table_builtin;
# endif /* DEVSCAN_SNMP_BUILTIN */
#endif /* WANT_DEVSCAN_SNMP_BUILTIN */

#if WANT_DEVSCAN_SNMP_DMF == 1
# ifndef DEVSCAN_SNMP_DMF
#  define DEVSCAN_SNMP_DMF
/* Can use a copy of the structure that will be populated dynamically */
    extern snmp_device_id_t *snmp_device_table_dmf;
# endif /* DEVSCAN_SNMP_DMF */
#endif /* WANT_DEVSCAN_SNMP_DMF */

#if WANT_LIBNUTSCAN_SNMP_DMF == 1
# ifndef LIBNUTSCAN_SNMP_DMF
#  ifdef DMF_SNMP_H
#   define LIBNUTSCAN_SNMP_DMF
    /* Note: This requires types defined in "dmf.h" */
    /* Variable implemented in scan_snmp.c */
    extern char *dmfnutscan_snmp_dir;
    extern mibdmf_parser_t *dmfnutscan_snmp_dmp;
    /* Just reference this to NULLify when client quits and frees DMF stuff */
    void uninit_snmp_device_table();
#  endif /* DMF_SNMP_H already included */
# endif /* LIBNUTSCAN_SNMP_DMF */
#endif /* WANT_LIBNUTSCAN_SNMP_DMF */

#ifdef __cplusplus
}
#endif
