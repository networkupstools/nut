/* nutdrv_qx_qs.h - Subdriver for QS protocol based UPSes
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * NOTE:
 * This subdriver implements the same protocol as the one used by the 'mustek' subdriver minus the vendor (I) and ratings (F) queries.
 * In the claim function:
 * - it doesn't even try to get 'vendor' informations (I)
 * - it checks only status (QS), through 'input.voltage' variable
 * Therefore it should be able to work even if the UPS doesn't support vendor/ratings *and* the user doesn't use the 'novendor'/'norating' flags, as long as:
 * - the UPS replies a QS-compliant answer (i.e. not necessary filled with all of the QS-required data, but at least of the right length and with not available data filled with some replacement character)
 * - the UPS reports a valid input.voltage (used in the claim function)
 * - the UPS reports valid status bits (1st, 2nd, 3rd, 6th, 7th are the mandatory ones)
 *
 */

#ifndef NUTDRV_QX_QS_H
#define NUTDRV_QX_QS_H

#include "nutdrv_qx.h"

extern subdriver_t	qs_subdriver;

#endif /* NUTDRV_QX_QS_H */
