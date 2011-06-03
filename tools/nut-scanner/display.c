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
		nutdev_num++;

		switch(current_dev->type) {
			case TYPE_USB:
				printf("\tvendorid=%04x\n",current_dev->opt.usb_opt.vendorid);
				printf("\tproductid=%04x\n",current_dev->opt.usb_opt.productid);
				if (current_dev->opt.usb_opt.vendor_name != NULL )
				{
					printf("\tvendor=%s\n", current_dev->opt.usb_opt.vendor_name);
				}
				if (current_dev->opt.usb_opt.product_name != NULL )
				{
					printf("\tproduct=%s\n", current_dev->opt.usb_opt.product_name);
				}
				if (current_dev->opt.usb_opt.serial_number != NULL )
				{
					printf("\tserial=%s\n", current_dev->opt.usb_opt.serial_number);
				}
				if (current_dev->opt.usb_opt.bus != NULL )
				{
					printf("\tbus=%s\n", current_dev->opt.usb_opt.bus);
				}
				break;

				/*XML*/
				//        while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {

				/* UPS <upsname> <description> */
				//                if (numa < 3) {
				//                        fprintf(stderr,"Error: insufficient data (got %d args, need at least 3)\n", numa);
				//                        return;
				//                }
				/* FIXME: check for duplication by getting driver.port and device.serial
				 * for comparison with other busses results */
				/* FIXME:
				 * - also print answer[2] if != "Unavailable"?
				 * - for upsmon.conf or ups.conf (using dummy-ups)? */
				//                printf("\t%s@%s\n", answer[1], hostname);
				//        }
			default:
				fprintf(stderr,"Unknown device type %d\n",current_dev->type);
				break;
		}

		current_dev = current_dev->next;
	}
	while( current_dev != NULL );
}

