/*
 *  Copyright (C) 2011 - EATON
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*! \file nutscan-device.c
    \brief manipulation of a container describing a NUT device
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#include "nutscan-device.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const char * nutscan_device_type_strings[TYPE_END - 1] = {
	"USB",
	"SNMP",
	"XML",
	"NUT",
	"IPMI",
	"Avahi",
	"serial",
	};

nutscan_device_t * nutscan_new_device()
{
	nutscan_device_t * device;

	device = malloc(sizeof(nutscan_device_t));
	if( device==NULL) {
		return NULL;
	}

	memset(device,0,sizeof(nutscan_device_t));

	return device;
}

static void deep_free_device(nutscan_device_t * device)
{
	nutscan_options_t * current;

	if(device==NULL) {
		return;
	}
	if(device->driver)  {
		free(device->driver);
	}
	if(device->port) {
		free(device->port);
	}

	while (device->opt != NULL) {
		current     = device->opt;
		device->opt = current->next;

		if(current->option != NULL) {
			free(current->option);
		}

		if(current->value != NULL) {
			free(current->value);
		}

		free(current);
	};

	if(device->prev) {
		device->prev->next = device->next;
	}
	if(device->next) {
		device->next->prev = device->prev;
	}

	free(device);
}

void nutscan_free_device(nutscan_device_t * device)
{
	if(device==NULL) {
		return;
	}
	while(device->prev != NULL) {
		deep_free_device(device->prev);
	}
	while(device->next != NULL) {
		deep_free_device(device->next);
	}

	deep_free_device(device);
}

void nutscan_add_option_to_device(nutscan_device_t * device, char * option, char * value)
{
	nutscan_options_t **opt;

	/* search for last entry */
	opt = &device->opt;

	while (NULL != *opt)
		opt = &(*opt)->next;

	*opt = (nutscan_options_t *)malloc(sizeof(nutscan_options_t));

	/* TBD: A gracefull way to propagate memory failure would be nice */
	assert(NULL != *opt);

	memset(*opt, 0, sizeof(nutscan_options_t));

	if( option != NULL ) {
		(*opt)->option = strdup(option);
	}
	else {
		(*opt)->option = NULL;
	}

	if( value != NULL ) {
		(*opt)->value = strdup(value);
	}
	else {
		(*opt)->value = NULL;
	}
}

nutscan_device_t * nutscan_add_device_to_device(nutscan_device_t * first, nutscan_device_t * second)
{
	nutscan_device_t * dev1=NULL;
	nutscan_device_t * dev2=NULL;

	/* Get end of first device */
	if( first != NULL) {
		dev1 = first;
		while(dev1->next != NULL) {
			dev1 = dev1->next;
		}
	}
	else {
		if( second == NULL ) {
			return NULL;
		}
		/* return end of second */
		dev2 = second;
		while(dev2->next != NULL) {
			dev2 = dev2->next;
		}
		return dev2;
	}

	/* Get start of second */
	if( second != NULL ) {
		dev2 = second;
		while(dev2->prev != NULL) {
			dev2 = dev2->prev;
		}
	}
	else {
		/* return end of first */
		dev1 = first;
		while(dev1->next != NULL) {
			dev1 = dev1->next;

		}
		return dev1;
	}

	/* join both */
	dev1->next = dev2;
	dev2->prev = dev1;

	/* return end of both */
        while(dev2->next != NULL) {
                dev2 = dev2->next;
	}

	return dev2;
}

nutscan_device_t * nutscan_rewind_device(nutscan_device_t * device)
{
	if (NULL == device)
		return NULL;

	while (NULL != device->prev)
		device = device->prev;

	return device;
}
