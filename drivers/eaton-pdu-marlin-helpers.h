/* eaton-pdu-marlin-helpers.h - helper for subdriver to monitor certain
 * Eaton ePDU SNMP devices with NUT
 *
 *  Copyright (C)
 *  2017        Arnaud Quette <ArnaudQuette@eaton.com>
 *  2017        Jim Klimov <EvgenyKlimov@eaton.com>
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

#ifndef EATON_EPDU_MARLIN_HELPERS_H
#define EATON_EPDU_MARLIN_HELPERS_H

long marlin_device_count_fun(const char *daisy_dev_list);

#endif /* EATON_EPDU_MARLIN_HELPERS_H */
