/* device.c: manipulation of a container describing a NUT device
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
#include "device.h"
#include <stdlib.h>
#include <string.h>

device_t * new_device()
{
	device_t * device;

	device = malloc(sizeof(device_t));
	if( device==NULL) {
		return NULL;
	}

	memset(device,0,sizeof(device_t));

	return device;
}

static void deep_free_device(device_t * device)
{
	if(device==NULL) {
		return;
	}
	if(device->driver)  {
		free(device->driver);
	}
	if(device->port) {
		free(device->port);
	}
	switch( device->type) {
		case TYPE_USB:
			if(device->opt.usb_opt.vendor_name) {
				free(device->opt.usb_opt.vendor_name);
			}
			if(device->opt.usb_opt.product_name) {
				free(device->opt.usb_opt.product_name);
			}
			if(device->opt.usb_opt.serial_number) {
				free(device->opt.usb_opt.serial_number);
			}
			if(device->opt.usb_opt.bus) {
				free(device->opt.usb_opt.bus);
			}
			break;
		default:
			break;
	}

	if(device->prev) {
		device->prev->next = device->next;
	}
	if(device->next) {
		device->next->prev = device->prev;
	}

	free(device);
}

void free_device(device_t * device)
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

	free(device);
}
