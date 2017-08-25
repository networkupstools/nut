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

static char marlin_scratch_buf[20];

/* Compute the phase to which an outlet group is connected
 * WRT the number of phase(s) and the outlet group number.
 * Note that the group type (marlin_outlet_group_type_info)
 * is not considered since this applies to any kind of group.
 * This trick limits input phase to electrical groups only
 * (not outlet-section nor user-defined!), and for now, there
 * is a maximum of 6 gangs (electrical groups).
 */
const char *marlin_outlet_group_phase_fun(int outlet_group_nb)
{
	const char* str_phases_nb = dstate_getinfo("input.phases");
	int phases_nb = 1;
	if (str_phases_nb) {
		phases_nb = atoi(str_phases_nb);
		if (phases_nb == 1) {
			return "L1";
		}
		else { /* 3ph assumed, 2ph PDU don't exist - at least not in Eaton Marlin lineup! */
			if (outlet_group_nb > 3)
				snprintf(marlin_scratch_buf, 3, "L%i", (outlet_group_nb - 3)); /* FIXME: For more than 6 ports, maybe "nb % 3"? */
			else
				snprintf(marlin_scratch_buf, 3, "L%i", outlet_group_nb);

			return marlin_scratch_buf;
		}
	}
	return NULL;
}
