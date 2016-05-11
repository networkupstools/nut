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
#include "powerware-mib.c"

int
main ()
{
#ifdef WITH_DMF_LUA
	// TODO: Verify this for typos/mismerges
	char *luaquery = "print(\"something\")";
	lua_State** lfunction = (lua_State**) malloc(sizeof(lua_State**));
	*lfunction = lua_open();
	luaopen_base(*lfunction);
	luaopen_string(*lfunction);
	luaL_loadbuffer(*lfunction, luaquery, strlen(luaquery), "fn");
	lua_pcall(*lfunction, 0, 0, 0);
#endif

	alist_t * list = alist_new(
		NULL,(void (*)(void **))alist_destroy, NULL );
	if (!list) {
		fprintf(stderr,"FATAL: Can not allocate the auxiliary list\n");
		return ENOMEM;
	}
	dmf_parser_init();

	parse_dir("./", list);

	//Debugging
	//mib2nut_info_t *m2n = get_mib2nut_table();
	//print_mib2nut_memory_struct(m2n + 6);
	//print_mib2nut_memory_struct(&pxgx_ups);
	printf("=== DMF-Test: Loaded C structures (sample for 'powerware'):\n\n");
	print_mib2nut_memory_struct((mib2nut_info_t *)
		alist_get_element_by_name(list, "powerware")->values[0]);
	printf("\n\n");
	printf("=== DMF-Test: Original C structures (sample for 'powerware'):\n\n");
	print_mib2nut_memory_struct(&powerware);
	//End debugging

	// First we destroy the index tables that reference data in the list...
	printf("=== DMF-Test: Freeing data...\n\n");
	dmf_parser_destroy();
	alist_destroy(&list);

#ifdef WITH_DMF_LUA
	lua_close(*lfunction);
	free(lfunction);
#endif
	printf("=== DMF-Test: All done\n\n");
}
