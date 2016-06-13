/* dmf-test.c - Network UPS Tools XML-driver-loader self-test program
 *
 * This file implements procedures to manipulate and load MIB structures
 * for NUT snmp-ups drivers dynamically, rather than as statically linked
 * files of the past.
 *
 * Copyright (C) 2016 Carlos Dominguez <CarlosDominguez@eaton.com>
 * Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
 * Copyright (C) 2016 Jim Klimov <EvgenyKlimov@eaton.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

//#include <neon/ne_xml.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>

#include "dmf.h"

/* The test involves generation of DMF and comparison to existing data.
   As a random pick, we use powerware-mib.c "as is" (with structures).
   This causes macro-redefinition conflict (and -Werror dies on it).
*/
#undef PACKAGE_VERSION
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_BUGREPORT
#include "eaton-mib.c"

int
main ()
{
	mibdmf_parser_t * dmp = mibdmf_parser_new();
	if (!dmp) {
		fprintf(stderr,"FATAL: Can not allocate the DMF parsing structures\n");
		return ENOMEM;
	}

#ifdef DEFAULT_DMFSNMP_DIR_OVERRIDE
#ifdef DEFAULT_DMFSNMP_DIR
#undef DEFAULT_DMFSNMP_DIR
#endif
#define DEFAULT_DMFSNMP_DIR DEFAULT_DMFSNMP_DIR_OVERRIDE
#endif

#ifdef DEFAULT_DMFSNMP_DIR
	mibdmf_parse_dir(DEFAULT_DMFSNMP_DIR, dmp);
#else
	mibdmf_parse_dir("./", dmp);
#endif

	//Debugging
	//mib2nut_info_t *m2n = get_mib2nut_table();
	//print_mib2nut_memory_struct(m2n + 6);
	//print_mib2nut_memory_struct(&pxgx_ups);
	printf("=== DMF-Test: Loaded C structures (sample for 'eaton_epdu'):\n\n");
	print_mib2nut_memory_struct((mib2nut_info_t *)
		alist_get_element_by_name(mibdmf_get_aux_list(dmp), "eaton_marlin")->values[0]);
	printf("\n\n");
	printf("=== DMF-Test: Original C structures (sample for 'eaton_epdu'):\n\n");
	print_mib2nut_memory_struct(&eaton_marlin);
	//End debugging

	printf("=== DMF-Test: Freeing data...\n\n");
	mibdmf_parser_destroy(&dmp);

	printf("=== DMF-Test: All done\n\n");
	return 0;
}
