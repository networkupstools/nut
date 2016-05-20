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

#include "config.h"
#include "dmf.h"

// These strings are embedded into <nut> tags to show their schema version
// Strings must verbatim match the XSD (no trailing slash etc.)
#ifndef XSD_DMFNUTSCAN_VERSION
#define XSD_DMFNUTSCAN_VERSION  "1.0.0"
#endif
#ifndef XSD_DMFNUTSCAN_XMLNS
#define XSD_DMFNUTSCAN_XMLNS    "http://www.networkupstools.org/dmf/snmp/nutscan"
#endif

int
main ()
{
	int result = 0;
	char *dir_name = NULL; // TODO: Make configurable the dir and/or list of files
	// TODO: Consider DEFAULT_DMFNUTSCAN_DIR for automatic output mode into a file?
#ifdef DEFAULT_DMFSNMP_DIR_OVERRIDE
#ifdef DEFAULT_DMFSNMP_DIR
#undef DEFAULT_DMFSNMP_DIR
#endif
#define DEFAULT_DMFSNMP_DIR DEFAULT_DMFSNMP_DIR_OVERRIDE
#endif

#ifdef DEFAULT_DMFSNMP_DIR
	dir_name = DEFAULT_DMFSNMP_DIR;
#else
	dir_name = "./";
#endif

// TODO: Usage (help), Command-line args
// option to append just a few (new) files to existing (large) index

	mibdmf_parser_t * dmp = mibdmf_parser_new();
	if (!dmp) {
		fprintf(stderr,"=== DMF-Reindex: FATAL: Can not allocate the DMF parsing structures\n");
		return ENOMEM;
	}

	fprintf(stderr, "=== DMF-Reindex: Loading DMF structures from directory '%s':\n\n",
		dir_name);
	result = mibdmf_parse_dir(dir_name, dmp);
	// TODO: Error-checking? Faults in some parses should be fatal or not?

	// Loop through discovered device_table and print it back as DMF markup
	fprintf(stderr, "=== DMF-Reindex: Print DMF subset for snmp_device_table[]...\n\n");
	snmp_device_id_t *devtab = mibdmf_get_device_table(dmp);
	if (!devtab)
	{
		fprintf(stderr,"=== DMF-Reindex: FATAL: Can not access the parsed device_table\n");
		return ENOMEM;
	}

	// Below we sprintf the index into a memory string, parse the result as
	// a DMF with a new alist and tables (to validate) and test the same data
	// is found. And only then output the stdout text.
	// TODO: uniquify output, so that an old index that was read in does not
	// pollute the parsed results (at least not for completely same items as
	// already exist in the table)? What to do about partial hits ~ updates?
	size_t i;
	size_t newdmf_len=0, newdmf_size=1024;
	char *newdmf = (char*)calloc(newdmf_size, sizeof(char));
	if (!newdmf) {
		fprintf(stderr,"=== DMF-Reindex: FATAL: Can not allocate the buffer for parsed DMF\n");
		return ENOMEM;
	}
	newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
                "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
                "<nut version=\"%s\" xmlns=\"%s\">\n",
                XSD_DMFNUTSCAN_VERSION, XSD_DMFNUTSCAN_XMLNS);
	for (i=0; devtab[i].oid != NULL || devtab[i].mib != NULL || devtab[i].sysoid != NULL ; i++)
	{
#ifdef DEBUG
		fprintf(stderr,"[num=%zu (lenbefore=%zu)] ", i, newdmf_len);
#endif

		// ASSUMPTION: String increments would not exceed these few bytes
		if ( (newdmf_size - newdmf_len) < 256)
		{
			newdmf_size += 1024;
			newdmf = (char*)realloc(newdmf, newdmf_size * sizeof(char));
			if (!newdmf) {
				fprintf(stderr,"=== DMF-Reindex: FATAL: Can not extend the buffer for parsed DMF\n");
				return ENOMEM;
			}
#ifdef DEBUG
			fprintf(stderr, "\nExtended the buffer to %zu bytes\n", newdmf_size);
#endif
		}

		newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
			"\t<mib2nut ");

		// This attr is always present, even if as an empty string:
		newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
			"auto_check=\"%s\" ", devtab[i].oid ? devtab[i].oid : ""); // [3 oid_auto_check] oid

		if (devtab[i].mib != NULL)
			newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
				"mib_name=\"%s\" ", devtab[i].mib); // [0 mib_name] mib

		if (devtab[i].sysoid != NULL)
			newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
				"oid=\"%s\" ", devtab[i].sysoid);  // [5 sysOID] sysoid/NULL

		newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
			"/>\n");
	}
	newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
		"</nut>\n");
#ifdef DEBUG
	fprintf(stderr,"[LAST: num=%zu (lenafter=%zu)] ", i, newdmf_len);
#endif
	fprintf(stderr, "\n=== DMF-Reindex: Indexed %zu entries...\n\n", i);

	mibdmf_parser_t * newdmp = mibdmf_parser_new();
	if (!newdmp) {
		fprintf(stderr,"=== DMF-Reindex: FATAL: Can not allocate the DMF verification parsing structures\n\n");
		return ENOMEM;
	}

	fprintf(stderr, "=== DMF-Reindex: Loading DMF structures from prepared string (verification)\n\n");
	result = mibdmf_parse_str(newdmf, newdmp);
	// Error checking for one (just made) document makes sense and is definite
	if ( result != 0 ) {
		fprintf(stderr, "=== DMF-Reindex: The generated document FAILED syntax verification\n\n");
		return result;
	}

	// Loop through reparsed device_table and compare to original one
	fprintf(stderr, "=== DMF-Reindex: Verify reparsed content for snmp_device_table[]...\n\n");
	snmp_device_id_t *newdevtab = mibdmf_get_device_table(newdmp);
	if (!newdevtab)
	{
		fprintf(stderr,"=== DMF-Reindex: FATAL: Can not access the reparsed device_table\n");
		return ENOMEM;
	}

	size_t j=-1, k=-1;
	result=0;
	// Make sure that all values we've considered are present in re-parse
	for (k=0; devtab[k].oid != NULL || devtab[k].mib != NULL || devtab[k].sysoid != NULL ; k++)
	{
		int r = 0;
		for (j=0; newdevtab[j].oid != NULL || newdevtab[j].mib != NULL || newdevtab[j].sysoid != NULL ; j++)
		{ // Note: OID attribute may be empty or NULL, these are assumed equal
			if ( (dmf_streq(newdevtab[j].oid, devtab[k].oid, false)
			    ||dmf_streq(newdevtab[j].oid, "", false)
			    ||dmf_streq(newdevtab[j].oid, NULL, false) )
			 && dmf_streq(newdevtab[j].mib, devtab[k].mib, false)
			 && dmf_streq(newdevtab[j].sysoid, devtab[k].sysoid, false) )
			{
				r = 1;
				break;
			}
		}

		if ( r==0 )
		{
			fprintf(stderr,"=== DMF-Reindex: mismatch in line %zu of the old table (no hits in new table)\n", k);
			result++;
		}
	}

	for (j=0; newdevtab[j].oid != NULL || newdevtab[j].mib != NULL || newdevtab[j].sysoid != NULL ; j++) ;

	if ( i!=j )
	{
		fprintf(stderr,"=== DMF-Reindex: mismatch in amount of lines of old(%zu) and new(%zu) tables\n", i, j);
		result++;
	}

	if ( i<=1 )
	{
		fprintf(stderr,"=== DMF-Reindex: empty table was generated\n");
		result++;
	}

	if ( result != 0 )
	{
		fprintf(stderr,"=== DMF-Reindex: The generated document FAILED content verification (%d issues)\n\n", result);
		return result;
	}

	fprintf(stderr, "=== DMF-Reindex: Checks succeeded - printing generated DMF to stdout...\n\n");
	printf("%s", newdmf);

	fprintf(stderr, "=== DMF-Reindex: Freeing data...\n\n");
	mibdmf_parser_destroy(&newdmp);
	mibdmf_parser_destroy(&dmp);
	free(newdmf);

	fprintf(stderr, "=== DMF-Reindex: All done\n\n");

	return 0;
// TODO: do we have and return fatal errors?
}
