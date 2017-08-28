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
#include "common.h"
#if (!DMFREINDEXER_MAKECHECK)
# include "nut_version.h"
#endif
#include "dmf.h"

/* These strings are embedded into <nut> tags to show their schema version */
/* Strings must verbatim match the XSD (no trailing slash etc.) */
#ifndef XSD_DMFNUTSCAN_VERSION
#define XSD_DMFNUTSCAN_VERSION  "1.0.0"
#endif
#ifndef XSD_DMFNUTSCAN_XMLNS
#define XSD_DMFNUTSCAN_XMLNS    "http://www.networkupstools.org/dmf/snmp/nutscan"
#endif

const char optstring[] = "?hDVkKZ:";
#define ERR_BAD_OPTION	(-1)

#ifdef HAVE_GETOPT_LONG
const struct option longopts[] = {
	{ "help",no_argument,NULL,'h' },
	{ "nut_debug_level",no_argument,NULL,'D' },
	{ "version",no_argument,NULL,'V' },
	{ "proceed_on_errors",no_argument,NULL,'k' },
	{ "abort_on_errors",no_argument,NULL,'K' },
	{ "dmf_dir", required_argument, NULL, 'Z' },
	{NULL,0,NULL,0}};
#else
#define getopt_long(a,b,c,d,e)	getopt(a,b,c) 
#endif /* HAVE_GETOPT_LONG */

int main(int argc, char *argv[])
{
	int opt_ret;
	int result = 0;
	int ret_code = EXIT_SUCCESS;
	int proceed_on_errors = 1; /* By default, do as much as we can */
	char *dir_name = NULL; /* TODO: Make configurable the dir and/or list of files */
	int dir_name_dynamic = 0;
	/* TODO: Consider DEFAULT_DMFNUTSCAN_DIR for automatic output mode into a file? */
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

/* TODO: Usage (help), Command-line args */
/* option to append just a few (new) files to existing (large) index */

	while((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL))!=-1) {

		switch(opt_ret) {
			case 'Z':
				if (dir_name_dynamic != 0)
					free(dir_name);
				dir_name = strdup(optarg);
				dir_name_dynamic = 1;
				break;
			case 'D':
				nut_debug_level++;
				break;
			case 'k':
				proceed_on_errors = 1;
				break;
			case 'K':
				proceed_on_errors = 0;
				break;
			case 'V':
#if DMFREINDEXER_MAKECHECK
				printf("Network UPS Tools - %s\n",
					"private build for DMF 'make check'"
					);
#else
				printf("Network UPS Tools - %s\n",
					NUT_VERSION_MACRO
					);
#endif
				exit(EXIT_SUCCESS);
				break;
			case '?':
				ret_code = ERR_BAD_OPTION;
			case 'h':
			default:
#if DMFREINDEXER_MAKECHECK
				puts("dmf-reindex : a private build for DMF 'make check' of the tool to reindex existing DMF files into the subset needed by nut-scanner.\n");
#else
				puts("nut-scanner-reindex-dmfsnmp : a tool to reindex existing DMF files into the subset needed by nut-scanner.\n");
#endif
				puts("OPTIONS:");
				printf("  -Z, --dmf_dir: Directory where multiple DMF MIB mapping files which you want to index reside\n");
				printf("\nMiscellaneous options:\n");
				printf("  -V, --version: Display NUT version\n");
				printf("  -D, --nut_debug_level: Raise the debugging level.  Use this multiple times to see more details.\n");
				printf("  -k, --proceed-on-errors: If some files could not be parsed, process what we have read%s.\n", proceed_on_errors==1?" (default)":"");
				printf("  -K, --abort-on-errors: If some files could not be parsed, do not process anything%s.\n", proceed_on_errors==1?"":" (default)");
				return ret_code;
		}
	}

	mibdmf_parser_t * dmp = mibdmf_parser_new();
	if (!dmp) {
		fatalx(EXIT_FAILURE,"=== DMF-Reindex: FATAL: Can not allocate the DMF parsing structures\n");
		/* TODO: Can we pass this code to fatalx? */
		return ENOMEM;
	}

	upsdebugx(1, "=== DMF-Reindex: Loading DMF structures from directory '%s':\n\n", dir_name);
	result = mibdmf_parse_dir(dir_name, dmp);
	if ( (result != 0) &&
	     (result == ERR || proceed_on_errors != 1)
	) {
		/* TODO: Error-checking? Faults in some parses should be fatal or not? */
		fatalx(EXIT_FAILURE,"=== DMF-Reindex: FATAL: Could not find or parse some files (return code %i)\n", result);
		/* TODO: Can we pass this code to fatalx? */
		return result;
	}

	/* Loop through discovered device_table and print it back as DMF markup */
	upsdebugx(2, "=== DMF-Reindex: Print DMF subset for snmp_device_table[]...\n\n");

	snmp_device_id_t *devtab = mibdmf_get_device_table(dmp);
	if (!devtab)
	{
		fatalx(EXIT_FAILURE,"=== DMF-Reindex: FATAL: Can not access the parsed device_table\n");
		/* TODO: Can we pass this code to fatalx? */
		return ENOMEM;
	}

	/* Below we sprintf the index into a memory string, parse the result as
	 * a DMF with a new alist and tables (to validate) and test the same data
	 * is found. And only then output the stdout text. */
	/* TODO: uniquify output, so that an old index that was read in does not
	 * pollute the parsed results (at least not for completely same items as
	 * already exist in the table)? What to do about partial hits ~ updates? */
	size_t i;
	size_t newdmf_len=0, newdmf_size=1024;
	char *newdmf = (char*)calloc(newdmf_size, sizeof(char));
	if (!newdmf) {
		fatalx(EXIT_FAILURE,"=== DMF-Reindex: FATAL: Can not allocate the buffer for parsed DMF\n");
		/* TODO: Can we pass this code to fatalx? */
		return ENOMEM;
	}
	newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
		"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
		"<nut version=\"%s\" xmlns=\"%s\">\n",
		XSD_DMFNUTSCAN_VERSION, XSD_DMFNUTSCAN_XMLNS);
	for (i=0; !is_sentinel__snmp_device_id_t(&(devtab[i])) ; i++)
	{
		upsdebugx(2,"[num=%zu (lenbefore=%zu)]", i, newdmf_len);

		/* ASSUMPTION: String increments would not exceed these few bytes */
		if ( (newdmf_size - newdmf_len) < 256)
		{
			newdmf_size += 1024;
			newdmf = (char*)realloc(newdmf, newdmf_size * sizeof(char));
			if (!newdmf) {
				fatalx(EXIT_FAILURE,"=== DMF-Reindex: FATAL: Can not extend the buffer for parsed DMF\n");
				/* TODO: Can we pass this code to fatalx? */
				return ENOMEM;
			}
			upsdebugx(2, "\nExtended the buffer to %zu bytes\n", newdmf_size);
		}

		newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
			"\t<mib2nut ");

		/* This attr is always present, even if as an empty string: */
		newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
			"auto_check=\"%s\" ", devtab[i].oid ? devtab[i].oid : ""); /* [3 oid_auto_check] oid */

		if (devtab[i].mib != NULL)
			newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
				"mib_name=\"%s\" ", devtab[i].mib); /* [0 mib_name] mib */

		if (devtab[i].sysoid != NULL)
			newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
				"oid=\"%s\" ", devtab[i].sysoid);  /* [5 sysOID] sysoid/NULL */

		newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
			"/>\n");
	}
	newdmf_len += snprintf(newdmf + newdmf_len, (newdmf_size - newdmf_len),
		"</nut>\n");

	upsdebugx(2,"[LAST: num=%zu (lenafter=%zu)] ", i, newdmf_len);
	upsdebugx(1, "\n=== DMF-Reindex: Indexed %zu entries...\n\n", i);

	mibdmf_parser_t * newdmp = mibdmf_parser_new();
	if (!newdmp) {
		fatalx(EXIT_FAILURE,"=== DMF-Reindex: FATAL: Can not allocate the DMF verification parsing structures\n\n");
		/* TODO: Can we pass this code to fatalx? */
		return ENOMEM;
	}

	upsdebugx(1, "=== DMF-Reindex: Loading DMF structures from prepared string (verification)\n\n");
	ret_code = mibdmf_parse_str(newdmf, newdmp);
	/* Error checking for one (just made) document makes sense and is definite */
	if ( result != 0 ) {
		fatalx(EXIT_FAILURE, "=== DMF-Reindex: The generated document FAILED syntax verification (return code %d)\n\n", ret_code);
		/* TODO: Can we pass this code to fatalx? */
		return ret_code;
	}

	/* Loop through reparsed device_table and compare to original one */
	upsdebugx(1, "=== DMF-Reindex: Verify reparsed content for snmp_device_table[]...\n\n");
	snmp_device_id_t *newdevtab = mibdmf_get_device_table(newdmp);
	if (!newdevtab)
	{
		fatalx(EXIT_FAILURE,"=== DMF-Reindex: FATAL: Can not access the reparsed device_table\n");
		/* TODO: Can we pass this code to fatalx? */
		return ENOMEM;
	}

	size_t j=-1, k=-1;
	result=0;
	/* Make sure that all values we've considered are present in re-parse */
	for (k=0; !is_sentinel__snmp_device_id_t(&(devtab[k])) ; k++)
	{
		int r = 0;
		for (j=0; !is_sentinel__snmp_device_id_t(&(newdevtab[j])) ; j++)
		{ /* Note: OID attribute may be empty or NULL, these are assumed equal */
			if ( (dmf_streq(newdevtab[j].oid, devtab[k].oid)
			    ||dmf_streq(newdevtab[j].oid, "")
			    ||dmf_streq(newdevtab[j].oid, NULL) )
			 && dmf_streq(newdevtab[j].mib, devtab[k].mib)
			 && dmf_streq(newdevtab[j].sysoid, devtab[k].sysoid) )
			{
				r = 1;
				break;
			}
		}

		if ( r==0 )
		{
			upsdebugx(2,"=== DMF-Reindex: mismatch in line %zu of the old table (no hits in new table)\n", k);
			result++;
		}
	}

	/* Count them */
	for (j=0; !is_sentinel__snmp_device_id_t(&(newdevtab[j])) ; j++) ;

	if ( i!=j )
	{
		upsdebugx(1,"=== DMF-Reindex: mismatch in amount of lines of old(%zu) and new(%zu) tables\n", i, j);
		result++;
	}

	if ( i<=1 )
	{
		upsdebugx(1,"=== DMF-Reindex: empty table was generated\n");
		result++;
	}

	if ( result != 0 )
	{
		fatalx(EXIT_FAILURE,"=== DMF-Reindex: The generated document FAILED content verification (%d issues)\n\n", result);
		/* TODO: Can we pass this code to fatalx? */
		return result;
	}

	upsdebugx(1, "=== DMF-Reindex: Checks succeeded - printing generated DMF to stdout...\n\n");
	printf("%s", newdmf);
	fflush(stdout);

	upsdebugx(2, "=== DMF-Reindex: Freeing data...\n\n");
	mibdmf_parser_destroy(&newdmp);
	mibdmf_parser_destroy(&dmp);
	free(newdmf);

	upsdebugx(1, "=== DMF-Reindex: All done\n\n");

	if (dir_name_dynamic != 0)
		free (dir_name);

	return ret_code;
}
