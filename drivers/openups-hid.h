/* openups-hid.h - subdriver to monitor Minibox openUPS USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2009	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *         2012	Nicu Pavel <npavel@mini-box.com>
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

#ifndef OPENUPS_HID_H
#define OPENUPS_HID_H

#include "usbhid-ups.h"

/* constants for converting HID read values to real values */
static const float vin_scale = 0.03545 * 100;
static const float vout_scale = 0.02571 * 100;
static const float vbat_scale = 0.00857 * 100;
static const float ccharge_scale = 0.8274 / 10;
static const float cdischarge_scale = 16.113 / 10;

extern subdriver_t openups_subdriver;

#endif /* OPENUPS_HID_H */
