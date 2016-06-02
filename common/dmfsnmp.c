/* dmf.c - Network UPS Tools XML-driver-loader
 *
 * This file implements procedures to manipulate and load MIB structures
 * for NUT snmp-ups drivers dynamically, rather than as statically linked
 * files of the past. See dmf.h for "The big theory" details.
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

#include <neon/ne_xml.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>

#include "dmfsnmp.h"
#include "str.h"

/*
 *
 *  C FILE
 *
 */
#ifdef WITH_DMF_LUA
        lua_State *functions_aux = NULL;
        char *luatext = NULL;
#endif
//DEBUGGING
void
print_snmp_memory_struct(snmp_info_t *self)
{
	int i = 0;

	printf("SNMP: --> Info_type: %s //   Info_len: %f"
		" //   OID:  %s //   Default: %s",
		self->info_type, self->info_len,
		self->OID, self->dfl);
	if(self->setvar)
		printf(" //   Setvar: %d", *self->setvar);
	printf("\n");

	if (self->oid2info)
	{
		while ( !( (self->oid2info[i].oid_value == 0)
		        && (!self->oid2info[i].info_value)
		) ) {
			printf("Info_lkp_t-----------> %d",
				self->oid2info[i].oid_value);
			if(self->oid2info[i].info_value)
				printf("  value---> %s\n",
					self->oid2info[i].info_value);
			i++;
		}
	}
	printf("*-*-*-->Info_flags %d\n", self->info_flags);
	printf("*-*-*-->Flags %lu\n", self->flags);
}

void
print_alarm_memory_struct(alarms_info_t *self)
{
	printf("Alarm: -->  OID: %s //   Status: %s //   Value: %s\n",
		self->OID, self->status_value, self->alarm_value);
}

void
print_mib2nut_memory_struct(mib2nut_info_t *self)
{
	int i = 0;
	printf("\n");
	printf("       MIB2NUT: --> Mib_name: %s //   Version: %s"
		" //   Power_status: %s //   Auto_check: %s"
		" //   SysOID: %s\n",
		self->mib_name, self->mib_version,
		self->oid_pwr_status, self->oid_auto_check,
		self->sysOID);

	if (self->snmp_info)
	{
		while ( !( (!self->snmp_info[i].info_type)
		        && (self->snmp_info[i].info_len == 0)
		        && (!self->snmp_info[i].OID)
		        && (!self->snmp_info[i].dfl)
		        && (self->snmp_info[i].flags == 0)
		        && (!self->snmp_info[i].oid2info)
		) ) {
			print_snmp_memory_struct(self->snmp_info+i);
			i++;
		}
	}

	i = 0;
	if (self->alarms_info)
	{
		while ( (self->alarms_info[i].alarm_value)
		     || (self->alarms_info[i].OID)
		     || (self->alarms_info[i].status_value)
		) {
			print_alarm_memory_struct(self->alarms_info+i);
			i++;
		}
	}
#ifdef WITH_DMF_LUA
if(self->functions){
  lua_getglobal(self->functions, "ups.mfr");
  printf("++++++++++++++------>Executing LUA\n");
  printf("Return of LUA: %d\n in code:\n%s\n", lua_pcall(self->functions,0,0,0), lua_tostring(self->functions, -1));
}
#endif
}
//END DEBUGGING

char *
get_param_by_name (const char *name, const char **items)
{
	int iname;

	if (!items || !name) return NULL;
	iname = 0;
	while (items[iname]) {
		if (strcmp (items[iname],name) == 0) {
			return strdup(items[iname+1]);
		}
		iname += 2;
	}
	return NULL;
}

//Create a lookup element
info_lkp_t *
info_lkp_new (int oid, const char *value)
{
	info_lkp_t *self = (info_lkp_t*) calloc (1, sizeof (info_lkp_t));
	assert (self);
	self->oid_value = oid;
	if (value)
		self->info_value = strdup (value);
	return self;
}

//Create alarm element
alarms_info_t *
info_alarm_new (const char *oid, const char *status, const char *alarm)
{
	alarms_info_t *self = (alarms_info_t*) calloc(1, sizeof (alarms_info_t));
	assert (self);
	if(oid)
		self->OID = strdup (oid);
	if(status)
		self->status_value = strdup (status);
	if(alarm)
		self->alarm_value = strdup (alarm);
	return self;
}

snmp_info_t *
info_snmp_new (const char *name, int info_flags, double multiplier,
	const char *oid, const char *dfl, unsigned long flags,
	info_lkp_t *lookup, int *setvar)
{
	snmp_info_t *self = (snmp_info_t*) calloc (1, sizeof (snmp_info_t));
	assert (self);
	if(name)
		self->info_type = strdup (name);
	self->info_len = multiplier;
	if(oid)
		self->OID = strdup (oid);
	if(dfl)
		self->dfl = strdup (dfl);
	self->info_flags = info_flags;
	self->flags = flags;
	self->oid2info = lookup;
	self->setvar = setvar;
	return self;
}

mib2nut_info_t *
info_mib2nut_new (const char *name, const char *version,
	const char *oid_power_status, const char *oid_auto_check,
	snmp_info_t *snmp, const char *sysOID, alarms_info_t *alarms
#ifdef WITH_DMF_LUA
, lua_State **functions
#endif
)
{
	mib2nut_info_t *self = (mib2nut_info_t*) calloc(1, sizeof(mib2nut_info_t));
	assert (self);
	if(name)
		self->mib_name = strdup (name);
	if(version)
		self->mib_version = strdup (version);
	if(oid_power_status)
		self->oid_pwr_status = strdup (oid_power_status);
	if(oid_auto_check)
		self->oid_auto_check = strdup (oid_auto_check);
	if(sysOID)
		self->sysOID = strdup (sysOID);
	self->snmp_info = snmp;
	self->alarms_info = alarms;
#ifdef WITH_DMF_LUA
self->functions = *functions;
#endif
	return self;
}

//Destroy full array of lookup elements
void
info_lkp_destroy (void **self_p)
{
	if (*self_p)
	{
		info_lkp_t *self = (info_lkp_t*) *self_p;
		if (self->info_value)
		{
			free ((char*)self->info_value);
			self->info_value = NULL;
		}
		free (self);
		*self_p = NULL;
	}
}

//Destroy full array of alarm elements
void
info_alarm_destroy (void **self_p)
{
	if (*self_p)
	{
		alarms_info_t *self = (alarms_info_t*) *self_p;
		if (self->OID)
		{
			free ((char*)self->OID);
			self->OID = NULL;
		}
		if (self->status_value)
		{
			free ((char*)self->status_value);
			self->status_value = NULL;
		}
		if (self->alarm_value)
		{
			free ((char*)self->alarm_value);
			self->alarm_value = NULL;
		}
		free (self);
		*self_p = NULL;
	}
}

void
info_snmp_destroy (void **self_p)
{
	int i = 0;
	if (*self_p) {
		snmp_info_t *self = (snmp_info_t*) *self_p;

		if (self->info_type)
		{
			free ((char*)self->info_type);
			self->info_type = NULL;
		}

		if (self->OID)
		{
			free ((char*)self->OID);
			self->OID = NULL;
		}

		if (self->dfl)
		{
			free ((char*)self->dfl);
			self->dfl = NULL;
		}

		if (self->oid2info)
		{
			while ( !( (self->oid2info[i].oid_value == 0)
			        && (!self->oid2info[i].info_value)
			) ) {
				if(self->oid2info[i].info_value)
				{
					free((void*)self->oid2info[i].info_value);
					self->oid2info[i].info_value = NULL;
				}
				i++;
			}
			free ((info_lkp_t*)self->oid2info);
			self->oid2info = NULL;
		}

		free (self);
		*self_p = NULL;
	}
}

void
info_mib2nut_destroy (void **self_p)
{
	int i = 0;
	//int j = 0;
	if (*self_p) {
		mib2nut_info_t *self = (mib2nut_info_t*) *self_p;
		if (self->mib_name)
		{
			free ((char*)self->mib_name);
			self->mib_name = NULL;
		}
		if (self->mib_version)
		{
			free ((char*)self->mib_version);
			self->mib_version = NULL;
		}
		if (self->oid_pwr_status)
		{
			free ((char*)self->oid_pwr_status);
			self->oid_pwr_status = NULL;
		}
		if (self->oid_auto_check)
		{
			free ((char*)self->oid_auto_check);
			self->oid_auto_check = NULL;
		}
		if (self->sysOID)
		{
			free ((char*)self->sysOID);
			self->sysOID = NULL;
		}

		if (self->snmp_info)
		{
			while( !( (!self->snmp_info[i].info_type)
			       && (self->snmp_info[i].info_len == 0)
			       && (!self->snmp_info[i].OID)
			       && (!self->snmp_info[i].dfl)
			       && (self->snmp_info[i].flags == 0)
			       && (!self->snmp_info[i].oid2info)
			) ) {
				if(self->snmp_info[i].info_type)
				{
					free((void*)self->snmp_info[i].info_type);
					self->snmp_info[i].info_type = NULL;
				}
				if(self->snmp_info[i].OID)
				{
					free((void*)self->snmp_info[i].OID);
					self->snmp_info[i].OID = NULL;
				}
				if(self->snmp_info[i].dfl)
				{
					free((void*)self->snmp_info[i].dfl);
					self->snmp_info[i].dfl = NULL;
				}
				i++;
			}
			free ((snmp_info_t*)self->snmp_info);
			self->snmp_info = NULL;
		}

		i = 0;
		if (self->alarms_info)
		{
			while ( (self->alarms_info[i].alarm_value)
			     || (self->alarms_info[i].OID)
			     || (self->alarms_info[i].status_value)
			) {
				if(self->alarms_info[i].alarm_value)
				{
					free((void*)self->alarms_info[i].alarm_value);
					self->alarms_info[i].alarm_value = NULL;
				}
				if(self->alarms_info[i].OID)
				{
					free((void*)self->alarms_info[i].OID);
					self->alarms_info[i].OID = NULL;
				}
				if(self->alarms_info[i].status_value)
				{
					free((void*)self->alarms_info[i].status_value);
					self->alarms_info[i].status_value = NULL;
				}
				i++;
			}
			free ((alarms_info_t*)self->alarms_info);
			self->alarms_info = NULL;
		}
#ifdef WITH_DMF_LUA
if(self->functions){
  lua_close(self->functions);
}
#endif
		free (self);
		*self_p = NULL;
	}
}

//New generic list element (can be the root element)
alist_t *
alist_new ( const char *name,
	void (*destroy)(void **self_p),
	void (*new_element)(void) )
{
	alist_t *self = (alist_t*) calloc (1, sizeof (alist_t));
	assert (self);
	self->size = 0;
	self->capacity = DEFAULT_CAPACITY;
	self->values = (void**) calloc (self->capacity, sizeof (void*));
	assert (self->values);
	self->destroy = destroy;
	self->new_element = new_element;
	if(name)
		self->name = strdup(name);
	else
		self->name = NULL;
	return self;
}

//Destroy full array of generic list elements
void
alist_destroy (alist_t **self_p)
{
	if (*self_p)
	{
		alist_t *self = *self_p;
		for (;self->size > 0; self->size--)
		{
			if (self->destroy)
				self->destroy(& self->values [self->size-1]);
			else
				free(self->values[self->size-1]);
		}
		if (self->name)
			free(self->name);
		free (self->values);
		free (self);
		*self_p = NULL;
	}
}

//Add a generic element at the end of the list
void
alist_append (alist_t *self, void *element)
{
	if (self->size + 1 == self->capacity)
	{
		self->capacity += DEFAULT_CAPACITY;
		self->values = (void**) realloc (
			self->values,
			self->capacity * sizeof(void*) );
	}
	self->values[self->size] = element;
	self->size++;
	self->values[self->size] = NULL;
}

//Return the last element of the list
alist_t *
alist_get_last_element (alist_t *self)
{
	if(self)
		return (alist_t*)self->values[self->size-1];
	return NULL;
}

alist_t *
alist_get_element_by_name (alist_t *self, char *name)
{
	int i;
	if (self)
		for (i = 0; i < self->size; i++)
			if ( ((alist_t*)self->values[i])->name )
				if (strcmp(((alist_t*)self->values[i])->name, name) == 0)
					return (alist_t*)self->values[i];
	return NULL;
}

// Accessors and lifecycle management for the structure that marries DMF and MIB
snmp_device_id_t *
mibdmf_get_device_table(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return NULL;
	return dmp->device_table;
}

snmp_device_id_t **
mibdmf_get_device_table_ptr(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return NULL;
	return &(dmp->device_table);
}

int
mibdmf_get_device_table_counter(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return -1;
	return dmp->device_table_counter;
}

int *
mibdmf_get_device_table_counter_ptr(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return NULL;
	return &(dmp->device_table_counter);
}

mib2nut_info_t **
mibdmf_get_mib2nut_table(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return NULL;
	return dmp->mib2nut_table;
}

mib2nut_info_t ***
mibdmf_get_mib2nut_table_ptr(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return NULL;
	return &(dmp->mib2nut_table);
}

alist_t *
mibdmf_get_aux_list(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return NULL;
	return dmp->list;
}

alist_t **
mibdmf_get_aux_list_ptr(mibdmf_parser_t *dmp)
{
	if (dmp==NULL) return NULL;
	return &(dmp->list);
}

// Properly destroy the object hierarchy and NULLify the caller's pointer
void
mibdmf_parser_destroy(mibdmf_parser_t **self_p)
{
	if (*self_p)
	{
		mibdmf_parser_t *self = (mibdmf_parser_t *) *self_p;
		// First we destroy the index tables that reference data in the list...
		if (self->device_table)
		{
			free(self->device_table);
			self->device_table = NULL;
		}
		if (self->mib2nut_table)
		{
			free(self->mib2nut_table);
			self->mib2nut_table = NULL;
		}
		if (self->list)
		{
			alist_destroy( &(self->list) );
			self->list = NULL;
		}
		self->device_table_counter = 0;
		free (self);
		*self_p = NULL;
	}
}

mibdmf_parser_t *
mibdmf_parser_new()
{
	mibdmf_parser_t *self = (mibdmf_parser_t *) calloc (1, sizeof (mibdmf_parser_t));
	assert (self);
	// Preallocate the sentinel in tables
	self->device_table_counter = 1;
	self->device_table = (snmp_device_id_t *)calloc(
		self->device_table_counter, sizeof(snmp_device_id_t));
	self->mib2nut_table = (mib2nut_info_t **)calloc(
		self->device_table_counter, sizeof(mib2nut_info_t*));
	assert (self->device_table);
	assert (self->mib2nut_table);
	assert (self->device_table_counter >= 1);
	self->list = alist_new( NULL,(void (*)(void **))alist_destroy, NULL );
	assert (self->list);
	return self;
}


//I splited because with the error control is going a grow a lot
void
mib2nut_info_node_handler (alist_t *list, const char **attrs)
{
	alist_t *element = alist_get_last_element(list);
	int i=0;
	snmp_info_t *snmp = NULL;
	alarms_info_t *alarm = NULL;

	char **arg = (char**) calloc (
		(INFO_MIB2NUT_MAX_ATTRS + 1), sizeof (void**) );
	assert (arg);

	arg[0] = get_param_by_name(MIB2NUT_MIB_NAME, attrs);
	arg[1] = get_param_by_name(MIB2NUT_VERSION, attrs);
	arg[2] = get_param_by_name(MIB2NUT_OID, attrs);
	arg[3] = get_param_by_name(MIB2NUT_POWER_STATUS, attrs);
	arg[4] = get_param_by_name(MIB2NUT_AUTO_CHECK, attrs);
	arg[5] = get_param_by_name(MIB2NUT_SNMP, attrs);
	arg[6] = get_param_by_name(MIB2NUT_ALARMS, attrs);

	if (arg[5])
	{
		alist_t *lkp = alist_get_element_by_name(list, arg[5]);
		snmp = (snmp_info_t*) calloc(
			(lkp->size + 1), sizeof(snmp_info_t) );
		for(i = 0; i < lkp->size; i++)
		{
			snmp[i].info_flags = ((snmp_info_t*)
				lkp->values[i])->info_flags;
			snmp[i].info_len = ((snmp_info_t*)
				lkp->values[i])->info_len;
			snmp[i].flags = ((snmp_info_t*)
				lkp->values[i])->flags;

			if( ((snmp_info_t*) lkp->values[i])->info_type )
				snmp[i].info_type = strdup(((snmp_info_t*)
					lkp->values[i])->info_type);
			else	snmp[i].info_type = NULL;

			if( ((snmp_info_t*) lkp->values[i])->OID )
				snmp[i].OID = strdup(((snmp_info_t*)
					lkp->values[i])->OID);
			else	snmp[i].OID = NULL;

			if( ((snmp_info_t*) lkp->values[i])->dfl )
				snmp[i].dfl = strdup(((snmp_info_t*)
					lkp->values[i])->dfl);
			else	snmp[i].dfl = NULL;

			if( ((snmp_info_t*) lkp->values[i])->setvar )
				snmp[i].setvar = ((snmp_info_t*)
					lkp->values[i])->setvar;
			else	snmp[i].setvar = NULL;

			if( ((snmp_info_t*) lkp->values[i])->oid2info )
				snmp[i].oid2info = ((snmp_info_t*)
					lkp->values[i])->oid2info;
			else	snmp[i].oid2info = NULL;

		} // for

		/* To be safe, do the sentinel entry explicitly */
		snmp[i].info_flags = 0;
		snmp[i].info_type = NULL;
		snmp[i].info_len = 0;
		snmp[i].OID = NULL;
		snmp[i].flags = 0;
		snmp[i].dfl = NULL;
		snmp[i].setvar = NULL;
		snmp[i].oid2info = NULL;
	} // arg[5]

	if(arg[6])
	{
		alist_t *alm = alist_get_element_by_name(list, arg[6]);
		alarm = (alarms_info_t*) calloc(
			alm->size + 1, sizeof(alarms_info_t) );
		for(i = 0; i < alm->size; i++)
		{
			if( ((alarms_info_t*) alm->values[i])->OID )
				alarm[i].OID = strdup( ((alarms_info_t*)
					alm->values[i])->OID );
			else	alarm[i].OID = NULL;

			if( ((alarms_info_t*) alm->values[i])->status_value )
				alarm[i].status_value = strdup( ((alarms_info_t*)
					alm->values[i])->status_value);
			else alarm[i].status_value = NULL;

			if( ((alarms_info_t*) alm->values[i])->alarm_value )
				alarm[i].alarm_value = strdup(((alarms_info_t*)
					alm->values[i])->alarm_value);
			else alarm[i].alarm_value = NULL;
		}
		alarm[i].OID = NULL;
		alarm[i].status_value = NULL;
		alarm[i].alarm_value = NULL;
	} // arg[6]

	if(arg[0])
	{
		alist_append(element, ((mib2nut_info_t *(*) (
			const char *, const char *, const char *,
			const char *, snmp_info_t *, const char *,
			alarms_info_t *
#ifdef WITH_DMF_LUA
, lua_State **
#endif
                )) element->new_element)
			(arg[0], arg[1], arg[3], arg[4],
			 snmp, arg[2], alarm
#ifdef WITH_DMF_LUA
, &functions_aux
#endif
));
	} // arg[0]

	for (i = 0; i < (INFO_MIB2NUT_MAX_ATTRS + 1); i++)
		free (arg[i]);
#ifdef WITH_DMF_LUA
functions_aux = NULL;
#endif
	free (arg);
}

void
alarm_info_node_handler(alist_t *list, const char **attrs)
{
	alist_t *element = alist_get_last_element(list);
	int i=0;
	char **arg = (char**) calloc (
		(INFO_ALARM_MAX_ATTRS + 1), sizeof (void**) );
	assert (arg);

	arg[0] = get_param_by_name(ALARM_ALARM, attrs);
	arg[1] = get_param_by_name(ALARM_STATUS, attrs);
	arg[2] = get_param_by_name(ALARM_OID, attrs);

	if(arg[0])
		alist_append(element, ( (alarms_info_t *(*)
			(const char *, const char *, const char *) )
			element->new_element) (arg[0], arg[1], arg[2]));

	for(i = 0; i < (INFO_ALARM_MAX_ATTRS + 1); i++)
		free (arg[i]);

	free (arg);
}

void
lookup_info_node_handler(alist_t *list, const char **attrs)
{
	alist_t *element = alist_get_last_element(list);
	int i = 0;
	char **arg = (char**) calloc ((INFO_LOOKUP_MAX_ATTRS + 1), sizeof (void**));
	assert (arg);

	arg[0] = get_param_by_name(LOOKUP_OID, attrs);
	arg[1] = get_param_by_name(LOOKUP_VALUE, attrs);

	if(arg[0])
	alist_append(element, ((info_lkp_t *(*) (int, const char *)) element->new_element) (atoi(arg[0]), arg[1]));

	for(i = 0; i < (INFO_LOOKUP_MAX_ATTRS + 1); i++)
		free (arg[i]);

	free (arg);
}

void
snmp_info_node_handler(alist_t *list, const char **attrs)
{
	//temporal
	double multiplier = 128;
	//end tremporal
	unsigned long flags;
	int info_flags;
	info_lkp_t *lookup = NULL;
	alist_t *element = alist_get_last_element(list);
	int i = 0;
	char **arg = (char**) calloc (
		(INFO_SNMP_MAX_ATTRS + 1), sizeof (void**) );
	assert (arg);

	arg[0] = get_param_by_name(SNMP_NAME, attrs);
	arg[1] = get_param_by_name(SNMP_MULTIPLIER, attrs);
	arg[2] = get_param_by_name(SNMP_OID, attrs);
	arg[3] = get_param_by_name(SNMP_DEFAULT, attrs);
	arg[4] = get_param_by_name(SNMP_LOOKUP, attrs);
	arg[5] = get_param_by_name(SNMP_SETVAR, attrs);
	// TODO: Anything here for arg[7] for LUA?

	//Info_flags
	info_flags = compile_info_flags(attrs);
	//Flags
	flags = compile_flags(attrs);

	if(arg[4])
	{
		alist_t *lkp = alist_get_element_by_name(list, arg[4]);
		lookup = (info_lkp_t*) calloc(
			(lkp->size + 1), sizeof(info_lkp_t) );
		for (i = 0; i < lkp->size; i++)
		{
			lookup[i].oid_value = ((info_lkp_t*)
				lkp->values[i])->oid_value;
			if( ((info_lkp_t*) lkp->values[i])->info_value )
				lookup[i].info_value = strdup(((info_lkp_t*)
					lkp->values[i])->info_value);
			else	lookup[i].info_value = NULL;
		}
		lookup[i].oid_value = 0;
		lookup[i].info_value = NULL;
	}

	if(arg[1])
		multiplier = atof(arg[1]);

	if(arg[5])
	{
		flags |= SU_FLAG_SETINT;
		if(strcmp(arg[5], SETVAR_INPUT_PHASES) == 0)
			alist_append(element, ((snmp_info_t *(*)
				(const char *, int, double, const char *,
				 const char *, unsigned long, info_lkp_t *,
				 int *)) element->new_element)
				(arg[0], info_flags, multiplier, arg[2],
				 arg[3], flags, lookup, &input_phases));
		else if(strcmp(arg[5], SETVAR_OUTPUT_PHASES) == 0)
			alist_append(element, ((snmp_info_t *(*)
				(const char *, int, double, const char *,
				 const char *, unsigned long, info_lkp_t *,
				 int *)) element->new_element)
				(arg[0], info_flags, multiplier, arg[2],
				 arg[3], flags, lookup, &output_phases));
		else if(strcmp(arg[5], SETVAR_BYPASS_PHASES) == 0)
			alist_append(element, ((snmp_info_t *(*)
				(const char *, int, double, const char *,
				 const char *, unsigned long, info_lkp_t *,
				 int *)) element->new_element)
				(arg[0], info_flags, multiplier, arg[2],
				 arg[3], flags, lookup, &bypass_phases));
	} else
		alist_append(element, ((snmp_info_t *(*)
			(const char *, int, double, const char *,
			 const char *, unsigned long, info_lkp_t *, int *))
			element->new_element)
			(arg[0], info_flags, multiplier, arg[2],
			 arg[3], flags, lookup, NULL));

	for(i = 0; i < (INFO_SNMP_MAX_ATTRS + 1); i++)
		free (arg[i]);

	free (arg);
}

unsigned long
compile_flags(const char **attrs)
{
	unsigned long flags = 0;
	char *aux_flags = NULL;
	aux_flags = get_param_by_name(SNMP_FLAG_OK, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_FLAG_OK;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_FLAG_STATIC, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_FLAG_STATIC;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_FLAG_ABSENT, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_FLAG_ABSENT;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_FLAG_NEGINVALID, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_FLAG_NEGINVALID;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_FLAG_UNIQUE, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_FLAG_UNIQUE;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_STATUS_PWR, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_STATUS_PWR;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_STATUS_BATT, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_STATUS_BATT;
		}
	if(aux_flags)free(aux_flags);
		aux_flags = get_param_by_name(SNMP_STATUS_CAL, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_STATUS_CAL;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_STATUS_RB, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_STATUS_RB;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_TYPE_CMD, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_TYPE_CMD;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_OUTLET_GROUP, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_OUTLET_GROUP;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_OUTLET, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_OUTLET;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_OUTPUT_1, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_OUTPUT_1;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_OUTPUT_3, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_OUTPUT_3;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_INPUT_1, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_INPUT_1;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_INPUT_3, attrs);
		if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_INPUT_3;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_BYPASS_1, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_BYPASS_1;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_BYPASS_3, attrs);
	if(aux_flags)if(strcmp(aux_flags, YES) == 0){
			flags = flags | SU_BYPASS_3;
		}
	if(aux_flags)free(aux_flags);
        aux_flags = get_param_by_name(TYPE_DAISY, attrs);
        if(aux_flags)if(strcmp(aux_flags, YES) == 0){
                        flags = flags | SU_TYPE_DAISY_1;
                }
        if(aux_flags)free(aux_flags);
#ifdef WITH_DMF_LUA
        aux_flags = get_param_by_name(TYPE_FUNCTION, attrs);
        if(aux_flags)if(strcmp(aux_flags, YES) == 0){
                        flags = flags | SU_FLAG_FUNCTION;
                }
        if(aux_flags)free(aux_flags);
#endif
	return flags;
}

int
compile_info_flags(const char **attrs)
{
	int info_flags = 0;
	char *aux_flags = NULL;
	aux_flags = get_param_by_name(SNMP_INFOFLAG_WRITABLE, attrs);
	if(aux_flags)
		if(strcmp(aux_flags, YES) == 0)
		{
			info_flags = info_flags | ST_FLAG_RW;
		}
	if(aux_flags)free(aux_flags);
	aux_flags = get_param_by_name(SNMP_INFOFLAG_STRING, attrs);
	if(aux_flags)
		if(strcmp(aux_flags, YES) == 0)
		{
			info_flags = info_flags | ST_FLAG_STRING;
		}
	if(aux_flags)free(aux_flags);

	return info_flags;
}

int
xml_dict_start_cb(void *userdata, int parent,
                      const char *nspace, const char *name,
                      const char **attrs)
{
	if(!userdata)return ERR;

	char *auxname = get_param_by_name("name",attrs);
	mibdmf_parser_t *dmp = (mibdmf_parser_t*) userdata;
	alist_t *list = *(mibdmf_get_aux_list_ptr(dmp));

	if(strcmp(name,DMFTAG_MIB2NUT) == 0)
	{
		alist_append(list, alist_new(auxname, info_mib2nut_destroy,
			(void (*)(void)) info_mib2nut_new));
		mib2nut_info_node_handler(list,attrs);
	}
	else if(strcmp(name,DMFTAG_LOOKUP) == 0)
	{
		alist_append(list, alist_new(auxname, info_lkp_destroy,
			(void (*)(void)) info_lkp_new));
	}
	else if(strcmp(name,DMFTAG_ALARM) == 0)
	{
		alist_append(list, alist_new(auxname, info_alarm_destroy,
			(void (*)(void)) info_alarm_new));
	}
	else if(strcmp(name,DMFTAG_SNMP) == 0)
	{
		alist_append(list, alist_new(auxname, info_snmp_destroy,
			(void (*)(void)) info_snmp_new));
	}
	else if(strcmp(name,DMFTAG_INFO_LOOKUP) == 0)
	{
		lookup_info_node_handler(list,attrs);
	}
	else if(strcmp(name,DMFTAG_INFO_ALARM) == 0)
	{
		alarm_info_node_handler(list,attrs);
	}
	else if(strcmp(name,DMFTAG_INFO_SNMP) == 0)
	{
		snmp_info_node_handler(list,attrs);
	}
	else if(strcmp(name,DMFTAG_FUNCTIONS) == 0)
	{
#ifdef WITH_DMF_LUA
          functions_aux = lua_open();
#else
          printf("NUT was not compiled with this feature.\n");
#endif
	}
	else if(strcmp(name,DMFTAG_NUT) != 0)
	{
		fprintf(stderr, "WARN: The '%s' tag in DMF is not recognized!\n", name);
	}
	free(auxname);
	return DMF_NEON_CALLBACK_OK;
}

int
xml_end_cb(void *userdata, int state, const char *nspace, const char *name)
{
	if(!userdata)return ERR;

	mibdmf_parser_t *dmp = (mibdmf_parser_t*) userdata;
	alist_t *list = *(mibdmf_get_aux_list_ptr(dmp));
	alist_t *element = alist_get_last_element(list);

	/* Currently, special handling in the DMF tag closure is for "mib2nut"
	 * tags that are last in the file according to schema - so we know we
	 * have all needed info at this time to populate an instance of the
	 * mib2nut_table index (there may be several such entries in one DMF).
	 */
	if(strcmp(name,DMFTAG_MIB2NUT) == 0)
	{
		int device_table_counter = mibdmf_get_device_table_counter(dmp);

		*mibdmf_get_device_table_ptr(dmp) = (snmp_device_id_t *) realloc(*mibdmf_get_device_table_ptr(dmp),
			device_table_counter * sizeof(snmp_device_id_t));
		*mibdmf_get_mib2nut_table_ptr(dmp) = (mib2nut_info_t **) realloc(*mibdmf_get_mib2nut_table_ptr(dmp),
			device_table_counter * sizeof(mib2nut_info_t*));

		snmp_device_id_t *device_table = mibdmf_get_device_table(dmp);
		assert (device_table);

		/* Make sure the new last entry in the table is zeroed-out */
		memset (device_table + device_table_counter - 1, 0,
			sizeof (snmp_device_id_t));

		(*mibdmf_get_mib2nut_table_ptr(dmp))[device_table_counter - 1] =
			(mib2nut_info_t *) element->values[0];

		if(((mib2nut_info_t *) element->values[0])->oid_auto_check)
			device_table[device_table_counter - 1].oid =
			(char *)((mib2nut_info_t *) element->values[0])->oid_auto_check;

		if(((mib2nut_info_t *) element->values[0])->mib_name)
			device_table[device_table_counter - 1].mib
			= (char *)((mib2nut_info_t *) element->values[0])->mib_name;

		if(((mib2nut_info_t *) element->values[0])->sysOID)
			device_table[device_table_counter - 1].sysoid =
			(char *)((mib2nut_info_t *) element->values[0])->sysOID;

		(*mibdmf_get_device_table_counter_ptr(dmp))++;
	}
#ifdef WITH_DMF_LUA
	else if(strcmp(name,DMFTAG_FUNCTIONS) == 0)
        {
          if(luaL_loadbuffer(functions_aux, luatext, strlen(luatext),"")){
                  printf("Error loading LUA functions:\n%s\n", luatext);
          }
          free(luatext);
        }
#endif
	return OK;
}

int
xml_cdata_cb(void *userdata, int state, const char *cdata, size_t len)
{
	if(!userdata)
		return ERR;

	if(len > 2){
// NOTE: Child-tags are also CDATA when parent-tag processing starts,
// so we do not report "unsupported" errors when we it a CDATA process.
#ifdef WITH_DMF_LUA
          if(functions_aux){
            if(!luatext){
		luatext = (char*) calloc(len + 1, sizeof(char));
		strncpy(luatext, cdata, len);
            }else{
              luatext = (char*) realloc(luatext, (strlen(luatext) + len + 1) * sizeof(char));
              
              strncat(luatext, cdata, len);
              
              //printf("***************--> Lua code %d : %s",(int) strlen(luatext), luatext);
            }
          }
#endif
	}
	return OK;
}

// Load DMF XML file into structure tree at *list (precreate with alist_new)
// Returns 0 on success, or an <errno> code on system or parsing errors
int
mibdmf_parse_file(char *file_name, mibdmf_parser_t *dmp)
{
	char buffer[4096]; /* Align with common cluster/FSblock size nowadays */
	FILE *f;
	int result = 0;

	assert (file_name);
	assert (dmp);
	assert (mibdmf_get_aux_list(dmp)!=NULL);

	if ( (file_name == NULL ) || \
	     ( (f = fopen(file_name, "r")) == NULL ) )
	{
		fprintf(stderr, "ERROR: DMF file '%s' not found or not readable\n",
			file_name ? file_name : "<NULL>");
		return ENOENT;
	}

	ne_xml_parser *parser = ne_xml_create ();
	ne_xml_push_handler (parser, xml_dict_start_cb,
		xml_cdata_cb
		, xml_end_cb, dmp);

	/* The neon XML parser would get blocks from the DMF file and build
	   the in-memory representation with our xml_dict_start_cb() callback.
	   Any hiccup (FS, neon, callback) is failure. */
	while (!feof (f))
	{
		size_t len = fread(buffer, sizeof(char), sizeof(buffer), f);
		if (len == 0) /* Should not zero-read from a non-EOF file */
		{
			fprintf(stderr, "ERROR parsing DMF from '%s'"
				"(unexpected short read)\n", file_name);
			result = EIO;
			break;
		} else {
			if ((result = ne_xml_parse (parser, buffer, len)))
			{
				fprintf(stderr, "ERROR parsing DMF from '%s'"
					"(unexpected markup?)\n", file_name);
				result = ENOMSG;
				break;
			}
		}
	}
	fclose (f);
	if (!result) /* no errors, complete the parse with len==0 call */
		ne_xml_parse (parser, buffer, 0);
	ne_xml_destroy (parser);

#ifdef DEBUG
	fprintf(stderr, "%s DMF acquired from '%s' (result = %d) %s\n",
		( result == 0 ) ? "[--OK--]" : "[-FAIL-]", file_name, result,
		( result == 0 ) ? "" : strerror(result)
	);
#endif

	/* Extend or truncate the tables to the current amount of known entries
	   To be on the safe side, we do this even if current file hiccuped. */
	assert (mibdmf_get_device_table_counter(dmp)>=1); /* Avoid underflow in memset below */
	*mibdmf_get_device_table_ptr(dmp) = (snmp_device_id_t *) realloc(*mibdmf_get_device_table_ptr(dmp),
		mibdmf_get_device_table_counter(dmp) * sizeof(snmp_device_id_t));
	*mibdmf_get_mib2nut_table_ptr(dmp) = (mib2nut_info_t **) realloc(*mibdmf_get_mib2nut_table_ptr(dmp),
		mibdmf_get_device_table_counter(dmp) * sizeof(mib2nut_info_t *));
	assert (mibdmf_get_device_table(dmp));
	assert (mibdmf_get_mib2nut_table(dmp));

	/* Make sure the last entry in the table is the zeroed-out sentinel */
	memset (*mibdmf_get_device_table_ptr(dmp) + mibdmf_get_device_table_counter(dmp) - 1, 0,
		sizeof (snmp_device_id_t));
	*(*mibdmf_get_mib2nut_table_ptr(dmp) + mibdmf_get_device_table_counter(dmp) - 1) = NULL;

	return result;
}

// Parse a buffer with complete DMF XML (from <nut> to </nut>)
int
mibdmf_parse_str (const char *dmf_string, mibdmf_parser_t *dmp)
{
	int result = 0;
	size_t len;

	assert (dmf_string);
	assert (dmp);
	assert (mibdmf_get_aux_list(dmp)!=NULL);

	if ( (dmf_string == NULL ) || \
	     ( (len = strlen(dmf_string)) == 0 ) )
	{
		fprintf(stderr, "ERROR: DMF passed in a string is empty or NULL\n");
		return ENOENT;
	}

	ne_xml_parser *parser = ne_xml_create ();
	ne_xml_push_handler (parser, xml_dict_start_cb,
		xml_cdata_cb
		, xml_end_cb, dmp);

	if ((result = ne_xml_parse (parser, dmf_string, len)))
	{
		fprintf(stderr, "ERROR parsing DMF from string "
			"(unexpected markup?)\n");
		result = ENOMSG;
	}

	if (!result) /* no errors, complete the parse with len==0 call */
		ne_xml_parse (parser, dmf_string, 0);
	ne_xml_destroy (parser);

#ifdef DEBUG
	fprintf(stderr, "%s DMF acquired from string (result = %d) %s\n",
		( result == 0 ) ? "[--OK--]" : "[-FAIL-]", result,
		( result == 0 ) ? "" : strerror(result)
	);
#endif

	/* Extend or truncate the tables to the current amount of known entries
	   To be on the safe side, we do this even if current file hiccuped. */
	assert (mibdmf_get_device_table_counter(dmp)>=1); /* Avoid underflow in memset below */
	*mibdmf_get_device_table_ptr(dmp) = (snmp_device_id_t *) realloc(*mibdmf_get_device_table_ptr(dmp),
		mibdmf_get_device_table_counter(dmp) * sizeof(snmp_device_id_t));
	*mibdmf_get_mib2nut_table_ptr(dmp) = (mib2nut_info_t **) realloc(*mibdmf_get_mib2nut_table_ptr(dmp),
		mibdmf_get_device_table_counter(dmp) * sizeof(mib2nut_info_t *));
	assert (mibdmf_get_device_table(dmp));
	assert (mibdmf_get_mib2nut_table(dmp));

	/* Make sure the last entry in the table is the zeroed-out sentinel */
	memset (*mibdmf_get_device_table_ptr(dmp) + mibdmf_get_device_table_counter(dmp) - 1, 0,
		sizeof (snmp_device_id_t));
	*(*mibdmf_get_mib2nut_table_ptr(dmp) + mibdmf_get_device_table_counter(dmp) - 1) = NULL;

	return result;
}

// Load all `*.dmf` DMF XML files from specified directory into aux list tree
// NOTE: Technically by current implementation, this is `*.dmf*`
int
mibdmf_parse_dir (char *dir_name, mibdmf_parser_t *dmp)
{
	DIR *dir;
	struct dirent *dir_ent;
	int i = 0, x = 0, result = 0;

	assert (dir_name);
	assert (dmp);

	if ( (dir_name == NULL ) || \
	     ( (dir = opendir(dir_name)) == NULL ) )
	{
		fprintf(stderr, "ERROR: DMF directory '%s' not found or not readable\n",
			dir_name ? dir_name : "<NULL>");
		return ENOENT;
	}

	while ( (dir_ent = readdir(dir)) != NULL )
	{
		if ( strstr(dir_ent->d_name, ".dmf") )
		{
			i++;
			char *file_path = str_concat(3, dir_name, "/", dir_ent->d_name);
			assert(file_path);
			int res = mibdmf_parse_file(file_path, dmp);
			free(file_path);
			if ( res != 0 )
			{
				x++;
				result = res;
				// No debug: parse_file() did it if enabled
			}
		}
	}
	closedir(dir);

#ifdef DEBUG
	if (i==0) {
		fprintf(stderr, "WARN: No DMF files were found or readable in directory '%s'\n",
			dir_name ? dir_name : "<NULL>");
	} else {
		fprintf(stderr, "INFO: %d DMF files were inspected in directory '%s'\n",
			i, dir_name ? dir_name : "<NULL>");
	}
	if (result!=0 || x>0) {
		fprintf(stderr, "WARN: Some %d DMF files were not readable in directory '%s' (last bad result %d)\n",
			x, dir_name ? dir_name : "<NULL>", result);
	}
#endif

	return result;
}

bool
dmf_streq (const char* x, const char* y, bool verbose)
{
	if (!x && !y)
		return true;
	if (!x || !y) {
		if (verbose)
			fprintf(stderr, "\nDEBUG: strEQ(): One compared string (but not both) is NULL:\n\t%s\n\t%s\n\n", x ? x : "<NULL>" , y ? y : "<NULL>");
		return false;
		}
	int cmp = strcmp (x, y);
	if (cmp != 0) {
		if (verbose)
			fprintf(stderr, "\nDEBUG: strEQ(): Strings not equal (%i):\n\t%s\n\t%s\n\n", cmp, x, y);
	}
	return (cmp == 0);
}

bool
dmf_strneq (const char* x, const char* y, bool verbose)
{
	if (!x && !y) {
		if (verbose)
			fprintf(stderr, "\nDEBUG: strNEQ(): Both compared strings are NULL\n");
		return false;
		}
	if (!x || !y) {
		return true;
		}
	int cmp = strcmp (x, y);
	if (cmp == 0) {
		if (verbose)
			fprintf(stderr, "\nDEBUG: strNEQ(): Strings are equal (%i):\n\t%s\n\t%s\n\n", cmp, x, y);
	}
	return (cmp != 0);
}
