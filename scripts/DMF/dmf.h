/* dmf.h - Header for dmf.c - the Network UPS Tools XML-driver-loader
 *
 * This file implements procedures to manipulate and load MIB structures
 * for NUT snmp-ups drivers dynamically, rather than as statically linked
 * files of the past. See below for "The big theory" details.
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

#ifndef DMF_H
#define DMF_H

/* THE BIG THEORY COMMENT
 *
 * The dynamic DMF supports adds a way to load and populate at run-time the
 * C structures with OIDs, flags and other information that was previously
 * statically compiled into "snmp-ups" driver and "nut-scanner" application.
 *
 * For snmp-ups this architecture involved linking with multiple *-mib.c files
 * which defined some structures per supported device type/vendor:
 * - info_lkp_t = lookup tables with arbitrary dictionaries to map numeric
 *   IDs to string keywords, usually for deciphering components in an OID
 * - snmp_info_t = mapping of NUT attribute keywords to SNMP OIDs, maybe the
 *   lookup tables or default values for this attribute, etc.
 * - alarms_info_t = optional mapping between SNMP OIDs and alarms
 * - mib2nut_info_t = each entry is a higher-level mapping between some
 *   identification strings and pointers to an instance of snmp_info_t and
 *   optionally an alarms_info_t - this mapping allows NUT to decide which
 *   one of the known snmp_info_t's to use against a particular device.
 *
 * Each typical *-mib.c and accompanying *-mib.h defines one (rarely more)
 * array of snmp_info_t items and one (rarely more) structure instance of
 * mib2nut_info_t, usually several arrays of info_lkp_t's and maybe an array
 * of recently introduced alarms_info_t (at this time only one driver has it).
 *
 * After including all the headers and linking with the *-mib.c files, the
 * snmp-ups.c file defined "mib2nut" - an array of mib2nut_info_t entries
 * with fixed references to all those known higher-level entries (exported
 * via header files).
 *
 * The nut-scanner defines an snmp_device_table with a subset of that
 * information as an array of snmp_device_id_t tuples. The table comes from
 * nutscan-snmp.h which is generated from the *-mib.c files during NUT build.
 *
 * The DMF code allows to populate equivalent hierarchy of structures by
 * parsing an XML file during driver startup, rather than pre-compiling
 * it statically.
 *
 * For that, first you dynamically instantiate the auxiliary list:
 * `alist_t * list = alist_new(NULL,(void (*)(void **))alist_destroy, NULL );`
 * This list hides the complexity of a dynamically allocated array of arrays,
 * ultimately storing the lookup tables, alarms, etc.
 * Then you populate it with data from XML files, using `parse_file()` or
 * `parse_dir()` calls. Whenever compatible markup is found in the XML input,
 * the parser (or rather our callback function called at each tag closure)
 * places the discovered information into a new entry under the "list" tree,
 * or into dynamically grown arrays "mib2nut_info_t *mib2nut_table" (snmp-ups)
 * and "snmp_device_id_t *device_table" (for nut-scanner), as appropriate.
 * References to these tables can be received with `get_mib2nut_table()` and
 * `get_device_table()` methods. You can also `get_device_table_counter()` to
 * look up the two tables' lengths (they were last `realloc()`ed to this size)
 * including the zeroed-out sentinel last entries, but historically the NUT
 * way consisted of looking through the tables until hitting the sentinel
 * entry and so determining its size or otherwise end of loop - and this
 * remains the official and reliable manner of length determination (not
 * a copy of the counter from implementation detail).
 *
 * In the end, these two tables can be used same as the static tables of the
 * old days, referencing information maintained in the trees behind "list".
 * In particular, note that you `dmf_parser_destroy(); alist_destroy(&list);`
 * only together and typically only when you tear down the executing program.
 * There is also a `dmf_parser_init()` that can be called to initialize empty
 * tables (with just one zeroed-out entry in each), but technically it is not
 * required (the parser can start to grow from unallocated table pointers);
 * this routine is there mostly for experiments with dynamic re-initialization
 * of the lists which is not a major goal or use-case.
 *
 * For the initial code-drop, all or most of the routines defined in the dmf.c
 * are declared in this header. The actual consumer API consists of the init,
 * destroy and parse functions, and getters for reference to the two tables.
 * The rest of declarations are here to aid development of other types of
 * consumers (e.g. DMF not for SNMP drivers, alists for something else enirely)
 * and may later be hidden or rearranged.
 *
 * @devs: You can also search for entries in the hierarchy behind "list" with
 * `alist_get_element_by_name()`, and dump contents with debug methods, e.g.:
 *      print_mib2nut_memory_struct((mib2nut_info_t *)
 *              alist_get_element_by_name(list, "powerware")->values[0]);
 * See dmf-test.c for some example usage.
 */

#ifdef WITH_DMF_LUA
/* NOTE: This code uses deprecated lua_open() that is removed since lua5.2 */
# include <lua.h>
# include <lauxlib.h>
# include <lualib.h>
#endif

#include "extstate.h"
#include "snmp-ups.h"
#include "nutscan-snmp.h"

/*
 *      HEADER FILE
 *
 */
#define YES "yes"
#define DEFAULT_CAPACITY 16

#define MIB2NUT "mib2nut"
#define LOOKUP "lookup"
#define SNMP "snmp"
#define ALARM "alarm"
#ifdef WITH_DMF_LUA
#define FUNCTION "function"
#endif

//#define MIB2NUT_NAME "name"
#define MIB2NUT_VERSION "version"
#define MIB2NUT_OID "oid"
#define MIB2NUT_MIB_NAME "mib_name"
#define MIB2NUT_AUTO_CHECK "auto_check"
#define MIB2NUT_POWER_STATUS "power_status"
#define MIB2NUT_SNMP "snmp_info"
#define MIB2NUT_ALARMS "alarms_info"

#define INFO_MIB2NUT_MAX_ATTRS 14
#define INFO_LOOKUP_MAX_ATTRS 4
#define INFO_SNMP_MAX_ATTRS 14
#define INFO_ALARM_MAX_ATTRS 6

#define INFO_LOOKUP "lookup_info"
#define LOOKUP_OID "oid"
#define LOOKUP_VALUE "value"

#define INFO_SNMP "snmp_info"
#define SNMP_NAME "name"
#define SNMP_MULTIPLIER "multiplier"
#define SNMP_OID "oid"
#define SNMP_DEFAULT "default"
#define SNMP_LOOKUP "lookup"
#define SNMP_SETVAR "setvar"
//Info_flags
#define SNMP_INFOFLAG_WRITABLE "writable"
#define SNMP_INFOFLAG_STRING "string"
//Flags
#define SNMP_FLAG_OK "flag_ok"
#define SNMP_FLAG_STATIC "static"
#define SNMP_FLAG_ABSENT "absent"
#define SNMP_FLAG_NEGINVALID "positive"
#define SNMP_FLAG_UNIQUE "unique"
#define SNMP_STATUS_PWR "power_status"
#define SNMP_STATUS_BATT "battery_status"
#define SNMP_STATUS_CAL "calibration"
#define SNMP_STATUS_RB "replace_battery"
#define SNMP_TYPE_CMD "command"
#define SNMP_OUTLET_GROUP "outlet_group"
#define SNMP_OUTLET "outlet"
#define SNMP_OUTPUT_1 "output_1_phase"
#define SNMP_OUTPUT_3 "output_3_phase"
#define SNMP_INPUT_1 "input_1_phase"
#define SNMP_INPUT_3 "input_3_phase"
#define SNMP_BYPASS_1 "bypass_1_phase"
#define SNMP_BYPASS_3 "bypass_3_phase"
//Setvar
#define SETVAR_INPUT_PHASES "input_phases"
#define SETVAR_OUTPUT_PHASES "output_phases"
#define SETVAR_BYPASS_PHASES "bypass_phases"

#define INFO_ALARM "info_alarm"
#define ALARM_OID "oid"
#define ALARM_STATUS "status"
#define ALARM_ALARM "alarm"

typedef struct {
	void **values;
	int size;
	int capacity;
	char *name;
	void (*destroy)(void **self_p);
	void (*new_element)(void);
} alist_t;

typedef enum {
	ERR = -1,
	OK
} state_t;


// Initialize the data for dmf.c
int
	dmf_parser_init();
int
	dmf_parser_destroy();

// Create and initialize info_lkp_t, a lookup element
info_lkp_t *
	info_lkp_new (int oid, const char *value);

// Destroy and NULLify the reference to alist_t, list of collections
void
	info_lkp_destroy (void **self_p);



// Create alarm element
alarms_info_t *
	info_alarm_new (const char *oid,
		const char *status, const char *alarm);

// Destroy full array of alarm elements
void
	info_alarm_destroy (void **self_p);

void
	alarm_info_node_handler (alist_t *list, const char **attrs);



// Same for snmp structure instances
snmp_info_t *
	info_snmp_new (const char *name, int info_flags, double multiplier,
		const char *oid, const char *dfl, unsigned long flags,
		info_lkp_t *lookup, int *setvar);

void
	info_snmp_destroy (void **self_p);

void
	snmp_info_node_handler (alist_t *list, const char **attrs);

snmp_device_id_t *
	get_device_table();

int
	get_device_table_counter();



// Same for MIB2NUT mappers
mib2nut_info_t *
	info_mib2nut_new (const char *name, const char *version,
		const char *oid_power_status, const char *oid_auto_check,
		snmp_info_t *snmp, const char *sysOID, alarms_info_t *alarms
#ifdef WITH_DMF_LUA
		, lua_State **functions
#endif
	);

void
	info_mib2nut_destroy (void **self_p);

void
	mib2nut_info_node_handler (alist_t *list, const char **attrs);

mib2nut_info_t *
	get_mib2nut_table();



// Create new instance of alist_t with LOOKUP type,
// for storage a list of collections
//alist_t *
//	alist_new ();

// New generic list element (can be the root element)
alist_t *
	alist_new (
		const char *name,
		void (*destroy)(void **self_p),
		void (*new_element)(void)
	);

// Destroy full array of generic list elements
void
	alist_destroy (alist_t **self_p);

// Add a generic element at the end of the list
void
	alist_append (alist_t *self, void *element);

// Return the last element of the list
alist_t *
	alist_get_last_element (alist_t *self);

alist_t *
	alist_get_element_by_name (alist_t *self, char *name);

void
	lookup_info_node_handler (alist_t *list, const char **attrs);

char *
	get_param_by_name (const char *name, const char **items);


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

#ifdef WITH_DMF_LUA
int
	xml_cdata_cb(
		void *userdata, int state, const char *cdata, size_t len
	);

lua_State *
	compile_lua_functionFrom_array (char **array, char *name);
#endif



unsigned long
	compile_flags (const char **attrs);

int
	compile_info_flags (const char **attrs);

// Load DMF XML file into structure tree at *list (precreate with alist_new)
int
	parse_file (char *file_name, alist_t *list);

// Load all `*.dmf` DMF XML files from specified directory
int
	parse_dir (char *dir_name, alist_t *list);




// Debugging dumpers
void
	print_snmp_memory_struct (snmp_info_t *self);

void
	print_alarm_memory_struct (alarms_info_t *self);

void
	print_mib2nut_memory_struct (mib2nut_info_t *self);


#endif
