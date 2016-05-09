/* dmf.h - Header for dmf.c - the Network UPS Tools XML-driver-loader
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

#ifndef DMF_H
#define DMF_H

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
#define SNMP_STATUS_RB "replace_baterry"
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

int
	get_mib2nut_table_counter();


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

int
	parse_file (char *file_name, alist_t *list);




// Debugging dumpers
void
	print_snmp_memory_struct (snmp_info_t *self);

void
	print_alarm_memory_struct (alarms_info_t *self);

void
	print_mib2nut_memory_struct (mib2nut_info_t *self);


#endif
