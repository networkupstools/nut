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

#include <errno.h>
#include <dirent.h>
#include <assert.h>

/* For experiments in development of DMF+lookup function support,
 * uncomment this line; for currently stable codebase keep it off...
 */
//#define WITH_SNMP_LKP_FUN 1

#include "dmf.h"

/* The test involves generation of DMF and comparison to existing data.
   As a random pick, we use eaton-pdu-marlin-mib.c "as is" (with structures
   and referenced conversion/lookup functions, if enabled by macros).
   This causes macro-redefinition conflict (and -Werror dies on it) -
   so we undefine a few macros...
*/
#undef PACKAGE_VERSION
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_BUGREPORT
#include "eaton-pdu-marlin-mib.c"

// Replicate what drivers/main.c exports
int do_synchronous = 0;

int
main ()
{
	nut_debug_level = 10;

	int result;
	mibdmf_parser_t * dmp = mibdmf_parser_new();
	if (!dmp) {
		fprintf(stderr,"FATAL: Can not allocate the DMF parsing structures\n");
		return ENOMEM;
	}

#ifdef DEFAULT_DMFSNMP_DIR_OVERRIDE
# ifdef DEFAULT_DMFSNMP_DIR
#  undef DEFAULT_DMFSNMP_DIR
# endif
# define DEFAULT_DMFSNMP_DIR DEFAULT_DMFSNMP_DIR_OVERRIDE
#endif

#ifdef DEFAULT_DMFSNMP_DIR
	printf("=== DMF-Test: Parsing data from %s...\n\n", DEFAULT_DMFSNMP_DIR);
	result = mibdmf_parse_dir(DEFAULT_DMFSNMP_DIR, dmp);
#else
	printf("=== DMF-Test: Parsing data from %s...\n\n", "./");
	result = mibdmf_parse_dir("./", dmp);
#endif

	if (result != 0) {
		printf("=== DMF-Test: Error parsing data: %i\n\n", result);
		mibdmf_parser_destroy(&dmp);
		return result;
	}

	/*Debugging
	 *mib2nut_info_t *m2n = get_mib2nut_table();
	 *print_mib2nut_memory_struct(m2n + 6);
	 *print_mib2nut_memory_struct(&pxgx_ups); */
	alist_t **aux = mibdmf_get_initial_list_ptr(dmp);
	alist_t *element;
	int iterator = 0;

	/* printf("=== DMF-Test: Loaded C structures (sample for 'eaton_epdu'):\n\n"); */
	if(aux){
		while(!(element = alist_get_element_by_name(aux[iterator], "eaton_marlin"))&&(iterator < mibdmf_get_list_size(dmp)))
			iterator++;

		if(element) {
			printf("=== DMF-Test: Found an eaton_marlin element; iterator == %i \n\n", iterator);
			print_mib2nut_memory_struct((mib2nut_info_t *) element->values[0]);
			result = 0;
		} else {
			printf("=== DMF-Test: Error, did not find an eaton_marlin element; iterator == %i\n\n", iterator);
			result = 1;
		}

		/* printf("\n\n");
		 * printf("=== DMF-Test: Original C structures (sample for 'eaton_epdu'):\n\n");
		 * print_mib2nut_memory_struct(&eaton_marlin);
		 * End debugging */

#if WITH_DMF_FUNCTIONS
#if WITH_DMF_LUA
		// Array of pointers to singular instances of mib2nut_info_t
		mib2nut_info_t **mib2nut = *(mibdmf_get_mib2nut_table_ptr)(dmp);
		if ( mib2nut == NULL ) {
			upsdebugx(1,"FATAL: Could not access the mib2nut index table");
			result = 1;
		} else {
			int i;
/* Note: the tests are lax: they rely on two first items in both test DMFs
 * being function-interpreted. Normally we'd search and match that... */
			for (i = 0; mib2nut[i] != NULL; i++) {
				if (strcmp("mge", mib2nut[i]->mib_name)==0) {
					printf("=== DMF-Test: Found an mge MIB mapping, dumping snmp_info; iterator == %i \n\n", i);
					print_snmp_memory_struct(&(mib2nut[i]->snmp_info[0]));
					if (is_sentinel__snmp_info_t( &(mib2nut[i]->snmp_info[0]) )) {
						printf("=== DMF-Test: FAILURE : an mge MIB mapping begins with a sentinel, proceeding for now but will fail the test later! \n\n");
						result = 2;
					} else {
						print_snmp_memory_struct(&(mib2nut[i]->snmp_info[1]));
					}
					break;
				}
			}
			for (i = 0; mib2nut[i] != NULL; i++) {
				if (strcmp("eaton_epdu", mib2nut[i]->mib_name)==0) {
					printf("=== DMF-Test: Found an eaton_epdu MIB mapping, dumping snmp_info (note: the gateway routines manipulate driver state (snmp-ups) and do not make sense here, not linked, return error as expected); iterator == %i \n\n", i);
					if (is_sentinel__snmp_info_t( &(mib2nut[i]->snmp_info[0]) )) {
						printf("=== DMF-Test: FAILURE : an eaton_epdu MIB mapping begins with a sentinel, proceeding for now but will fail the test later! \n\n");
						result = 2;
					} else {
						print_snmp_memory_struct(&(mib2nut[i]->snmp_info[1]));
					}
					break;
				}
			}
		}
#endif
#endif

		printf("=== DMF-Test: Freeing data...\n\n");
		mibdmf_parser_destroy(&dmp);

		printf("=== DMF-Test: All done (code %d)\n\n", result);
		return result;
	}

	printf("=== DMF-Test: Error, no DMF data loaded\n");
	mibdmf_parser_destroy(&dmp);
	return -1;
}
