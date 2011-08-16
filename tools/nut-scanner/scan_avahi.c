/* scan_avahi.c: detect NUT avahi services
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

#include "config.h"

void nutscan_scan_avahi()
{
//        printf("Scanning NUT bus (DNS-SD method):\n");

        /* Check avahi-browse code:
         * http://git.0pointer.de/?p=avahi.git;a=tree;f=avahi-utils;h=5655a104964258e7be32ada78794f73beb84e0dd;hb=HEAD
         *
         * Example service publication (counterpart of the above):
         * $ avahi-publish -s nut _upsd._tcp 3493 txtvers=1 protovers=1.0.0 type=standalone
         */
}

