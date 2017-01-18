/* dmfcore.h - Header for dmfcore.c - The core of generic DMF support,
 * allowing to parse data from DMF documents with LibNEON (possibly
 * LTDLed just for this purpose).
 *
 * This file implements procedures to load the LibNEON library if needed
 * and to request a parse of an in-memory string, a single file, or of
 * all '*.dmf' files in a named directory.
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

#ifndef DMF_CORE_H
/* Note: we #define DMF_CORE_H only in the end of file */

/* LibNEON is currently required to build DMF */
/* Note: code that includes this must be built with $(LIBNEON_CFLAGS) */
#if WITH_NEON
# include <ne_xml.h>
#else
#error "LibNEON is required to build DMF"
#endif

/*
 *      HEADER FILE
 *
 */

#ifndef PATH_MAX_SIZE
#ifdef PATH_MAX
#define PATH_MAX_SIZE PATH_MAX
#else
#define PATH_MAX_SIZE 1024
#endif
#endif

typedef enum {
	ERR = -1,
	OK,
	DMF_NEON_CALLBACK_OK = 1
} dmfparser_state_t;


/* Function callbacks and pointer to the format-specific object
 * needed to interpret a particular DMF markup (e.g. DMF MIB) */
typedef struct {
	void *parsed_data;		/* Pointer to e.g. a mibdmf_parser_t object with
							 * further format-specific data, as used by the
							 * NEON function callbacks during parsing */

	/* Begin the DMF parsing of a file or string in dmfcore_parse_*()
	 * routines below, e.g. initialize storage or assert stuff.
	 * Such routine takes in this DCP object's "parsed_data",
	 * does its magic and returns OK or ERR as defined above. */
	int (*dmf_parse_begin_cb) (void*);

	/* Finish the DMF parsing of a file or string in dmfcore_parse_*()
	 * routines below, e.g. add sentinels in the end of lists or clean up.
	 * Such routine takes in this DCP object's "parsed_data" and current
	 * parse result (0==OK, or nonzero errno), does its magic and returns
	 * a new (maybe same) result. */
	int (*dmf_parse_finish_cb) (void*, int);

	/* LibNEON callbacks used by xml_parse(): */
	ne_xml_startelm_cb *xml_dict_start_cb;
	ne_xml_cdata_cb *xml_cdata_cb;
	ne_xml_endelm_cb *xml_end_cb;
} dmfcore_parser_t;



/* calloc() a new DCP object and set its fields to NULLs.
 * There is no destroyer because this object only holds references to
 * other objects or functions whole lifecycle is managed by caller.
 * However, this DCP object should also be free()d by the caller now.
 * The caller should populate DCP with callback functions and pointer to
 * the format-specific parsed_data structure, and pass to dmfcore_parse*().
 */
dmfcore_parser_t*
	dmfcore_parser_new();

/* Ensure at compile-time tat currently required fields are populated */
dmfcore_parser_t*
	dmfcore_parser_new_init(
		void *parsed_data,
		int (*dmf_parse_begin_cb) (void*),
		int (*dmf_parse_finish_cb) (void*, int),
		ne_xml_startelm_cb* xml_dict_start_cb,
		ne_xml_cdata_cb* xml_cdata_cb,
		ne_xml_endelm_cb* xml_end_cb);


/* Load DMF XML file into structure tree at dmp->list (can append many times) */
int
	dmfcore_parse_file (char *file_name, dmfcore_parser_t *dcp);

/* Parse a buffer with complete DMF XML (from <nut> to </nut>) */
int
	dmfcore_parse_str (const char *dmf_string, dmfcore_parser_t *dcp);

/* Load all `*.dmf` DMF XML files from specified directory */
int
	dmfcore_parse_dir (char *dir_name, dmfcore_parser_t *dcp);


/* The guts of XML parsing: callbacks that act on an instance of parsed_data
 * such as e.g. mibdmf_parser_t for DMF MIB implementation */
/* The callbacks expected above are defined by LibNEON and look like this spec:
int
	xml_dict_start_cb (
		void *userdata, int parent,
		const char *nspace, const char *name,
		const char **attrs
	);

int
	xml_end_cb (
		void *userdata, int state, const char *nspace,
		const char *name
	);

int
	xml_cdata_cb(
		void *userdata, int state, const char *cdata, size_t len
	);
*/

#define DMF_CORE_H
#endif /* DMF_CORE_H */
