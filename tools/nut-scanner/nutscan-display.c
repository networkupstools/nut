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

/*! \file nutscan-display.c
    \brief format and display scanned devices
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#include "common.h"
#include <stdio.h>
#include "nutscan-device.h"
#include "nut-scan.h"

static char * nutscan_device_type_string[TYPE_END] = {
	"NONE",
	"USB",
	"SNMP",
	"XML",
	"NUT",
	"IPMI",
	"AVAHI",
	"EATON_SERIAL"
};

void nutscan_display_ups_conf(nutscan_device_t * device)
{
	nutscan_device_t * current_dev = device;
	nutscan_options_t * opt;
	static int nutdev_num = 1;

	if(device==NULL) {
		return;
	}

	/* Find start of the list */
	while(current_dev->prev != NULL) {
		current_dev = current_dev->prev;
	}

	/* Display each devices */
	do {
		printf("[nutdev%i]\n\tdriver = \"%s\"\n\tport = \"%s\"\n",
				nutdev_num, current_dev->driver,
				current_dev->port);

		opt = current_dev->opt;

		while (NULL != opt) {
			if( opt->option != NULL ) {
				printf("\t%s",opt->option);
				if( opt->value != NULL ) {
					printf(" = \"%s\"", opt->value);
				}
				printf("\n");
			}
			opt = opt->next;
		}

		nutdev_num++;

		current_dev = current_dev->next;
	}
	while( current_dev != NULL );
}

void nutscan_display_parsable(nutscan_device_t * device)
{
	nutscan_device_t * current_dev = device;
	nutscan_options_t * opt;

	if(device==NULL) {
		return;
	}

	/* Find start of the list */
	while(current_dev->prev != NULL) {
		current_dev = current_dev->prev;
	}

	/* Display each devices */
	do {
		printf("%s:driver=\"%s\",port=\"%s\"",
			nutscan_device_type_string[current_dev->type],
			current_dev->driver,
			current_dev->port);

		opt = current_dev->opt;

		while (NULL != opt) {
			if( opt->option != NULL ) {
				printf(",%s",opt->option);
				if( opt->value != NULL ) {
					printf("=\"%s\"", opt->value);
				}
			}
			opt = opt->next;
		}

		printf("\n");

		current_dev = current_dev->next;
	}
	while( current_dev != NULL );
}
