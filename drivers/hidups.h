/* hidups.h - prototype HID UPS driver for Network UPS Tools
 
   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
 
   Based on evtest.c v1.10 - Copyright (c) 1999-2000 Vojtech Pavlik
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or 
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "timehead.h"

#include "config.h"

#ifndef HID_MAX_USAGES
#define HID_MAX_USAGES 1024	/* horrible workaround hack */
#endif

#include LINUX_HIDDEV		/* from configure */

#define DEFAULT_ONDELAY		13	/* delay between return of utility power */
								/* and powering up of load (10 seconds units for MGE) */
								/* ondelay > offdelay */
#define DEFAULT_OFFDELAY	120	/* delay befor power off, in SECONDS*/ 

/* power device page: x84 */

#define UPS_USAGE				0x840004
#define POWER_USAGE				0x840020	/* wrong, but needed for MGE */
#define UPS_BATTVOLT			0x840030	/* voltage * 100             */
#define UPS_LOADPCT				0x840035	/* load percentage           */
#define UPS_SHUTDOWN_IMMINENT	0x840069	/* 1 = low battery           */
#define UPS_IMFR				0x8400FD	/* manufacturer name         */
#define UPS_IPRODUCT			0x8400FE	/* model name                */
#define UPS_ISERIAL				0x8400FF	/* serial number             */
#define UPS_WAKEDELAY			0x840056
#define UPS_GRACEDELAY			0x840057

/* battery system page: x85 */

#define BATT_BELOW_RCL			0x850042	/* below remaining cap limit */
#define BATT_CHARGING			0x850044	/* 0 = no longer charging    */
#define BATT_DISCHARGING		0x850045	/* 1 = on battery            */
#define BATT_REMAINING_CAPACITY	0x850066	/* battery percentage        */
#define BATT_RUNTIME_TO_EMPTY	0x850068	/* minutes                   */
#define BATT_MFRDATE			0x850085	/* manufacturer date         */
#define BATT_ICHEMISTRY			0x850089	/* battery type              */
#define BATT_AC_PRESENT			0x8500d0	/* 1 = on line               */
#define BATT_IOEMINFORMATION	0x85008f	/* battery OEM description   */
