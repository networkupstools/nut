/*  eaton-pdu-marlin-helpers.c - helper routines for eaton-pdu-marlin-mib.c
 *  to monitor Eaton ePDUs branded as:
 *                G2 Marlin SW / MI / MO / MA
 *                G3 Shark SW / MI / MO / MA
 *
 *  Copyright (C) 2017
 * 		Arnaud Quette <ArnaudQuette@Eaton.com>
 *  Copyright (C) 2017
 * 		Jim Klimov <EvgenyKlimov@Eaton.com>
 *
 *  Supported by Eaton <http://www.eaton.com>
 *   and previously MGE Office Protection Systems <http://www.mgeops.com>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "eaton-pdu-marlin-helpers.h"
#include "dstate.h"

/* Take string "unitsPresent" (ex: "0,3,4,5"), and count the amount
 * of "," separators+1 using an inline function */
long marlin_device_count_fun(const char *daisy_dev_list)
{
	long count = 0, i;
	for (i=0; daisy_dev_list[i] != '\0'; i++) {
		if (daisy_dev_list[i] == ',') {
			/* Each comma means a new device in the list */
			count ++;
		}
	}
	if (i>0 && (daisy_dev_list[i-1] != ',') ) {
		/* Non-empty string => at least one device, and no trailing commas */
		count ++;
	}
	return count;
}
