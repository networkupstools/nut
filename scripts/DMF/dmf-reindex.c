/* dmf-reindex.c - Network UPS Tools XML-driver-loader: validate large DMFs
 * by importing them, and print a concise DMF with bits just for nut-scanner
 * to initialize its `snmp_device_id_t snmp_device_table[]` array of strings.
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

#include "dmf.h"

int
main ()
{
	char *dir_name = "./"; // TODO: Make configurable the dir and/or list of files
// TODO: Usage (help), Command-line args
// option to append just a few (new) files to existing (large) index

	mibdmf_parser_t * dmp = mibdmf_parser_new();
	if (!dmp) {
		fprintf(stderr,"=== DMF-Reindex: FATAL: Can not allocate the DMF parsing structures\n");
		return ENOMEM;
	}

	fprintf(stderr, "=== DMF-Reindex: Loading DMF structures from directory '%s':\n\n",
		dir_name);
	mibdmf_parse_dir(dir_name, dmp);

	// Loop through discovered device_table and print it back as DMF markup
	fprintf(stderr, "=== DMF-Reindex: Print DMF subset for snmp_device_table[]...\n\n");
	snmp_device_id_t *devtab = mibdmf_get_device_table(dmp);
	if (!devtab)
	{
		fprintf(stderr,"=== DMF-Reindex: FATAL: Can not access the parsed device_table\n");
		return ENOMEM;
	}

	// TODO: sprintf the index into a memory string, parse the result as a DMF
	// with a new alist and tables (to validate) and test the same data is found.
	// And only then output the stdout text.
	// Needs fully dynamic alists (no global tables) and a version of parse_file
	// for full in-memory strings.
	// TODO: uniquify output, so that an index that was read in does not pollute?
	size_t i;
	printf("<nut>\n");
	for (i=0; devtab[i].oid != NULL || devtab[i].mib != NULL || devtab[i].sysoid != NULL ; i++)
	{
		//fprintf(stderr,"[%d] ",i);
		//printf("[%d]\t<mib2nut ", i);
		printf("\t<mib2nut ");
		printf("auto_check=\"%s\" ", devtab[i].oid ? devtab[i].oid : ""); // [3 oid_auto_check] oid
		if (devtab[i].mib != NULL) printf("mib_name=\"%s\" ", devtab[i].mib); // [0 mib_name] mib
		if (devtab[i].sysoid != NULL) printf("oid=\"%s\" ", devtab[i].sysoid);  // [5 sysOID] sysoid/NULL
		printf("/>\n");
	}
	printf("</nut>\n");
	fprintf(stderr, "\n=== DMF-Reindex: Indexed %d entries...\n\n", i);

	fprintf(stderr, "=== DMF-Reindex: Freeing data...\n\n");
	mibdmf_parser_destroy(&dmp);

	fprintf(stderr, "=== DMF-Reindex: All done\n\n");

	return 0;
// TODO: do we have and return fatal errors?
}
