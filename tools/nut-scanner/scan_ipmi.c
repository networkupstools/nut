/* scan_ipmi.c: detect NUT supported Power Supply Units
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
#include "common.h"
#include "nut-scan.h"

#ifdef WITH_IPMI

/* Return 0 on error */
int nutscan_load_ipmi_library()
{
	return 0;
}

/* TODO */
nutscan_device_t *  nutscan_scan_ipmi()
{
        if( !nutscan_avail_ipmi ) {
                return NULL;
        }

	return NULL;
}
#else /* WITH_IPMI */
/* stub function */
nutscan_device_t *  nutscan_scan_ipmi()
{
	return NULL;
}
#endif /* WITH_IPMI */
