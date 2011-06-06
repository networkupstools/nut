/* display.c: format and display scanned devices
 * 
 *  Copyright (C) 2011 - Frederic Bohe <fredericbohe@eaton.com>
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


/* Parseable output
 *  bus_type: driver=driver,port=port,[,serial,vendorid,productid,mibs,...]
 *  USB: driver=usbhid-ups,port=auto,serial=XXX,vendorid=0x0463,productid=0xffff,
 *  SNMP: driver=snmp-ups,port=ip_address,mibs=...
 */


#include "config.h"
#include <stdio.h>
#include "device.h"

int nutdev_num = 1;

void display_ups_conf(device_t * device)
{
	device_t * current_dev = device;
	options_t * opt;

	if(device==NULL) {
		return;
	}

	/* Find start of the list */
	while(current_dev->prev != NULL) {
		current_dev = current_dev->prev;
	}

	/* Display each devices */
	do {
		printf("[nutdev%i]\n\tdriver=%s\n\tport=%s\n",
				nutdev_num, current_dev->driver,
				current_dev->port);

		opt = &(current_dev->opt);

		do {
			if( opt->option != NULL ) {
				printf("\t%s",opt->option);
				if( opt->value != NULL ) {
					printf("=%s", opt->value);
				}
				printf("\n");
			}
			opt = opt->next;
		} while( opt != NULL );

		nutdev_num++;

		current_dev = current_dev->next;
	}
	while( current_dev != NULL );
}

