/*  cyberpower-mib.c - data to monitor Cyberpower RMCARD
 *
 *  Copyright (C) 2010 - Eric Schultz <paradxum@mentaljynx.com>
 *  
 *  derived (i.e. basically copied and modified) of bestpower by:
 *  Copyright (C) 2010 - Arnaud Quette <arnaud.quette@free.fr>
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

#include "cyberpower-mib.h"

#define CYBERPOWER_MIB_VERSION		"0.2"
#define CYBERPOWER_OID_MODEL_NAME	".1.3.6.1.4.1.3808.1.1.1.1.1.1.0"

#define CYBERPOWER_SYSOID			".1.3.6.1.4.1.3808"

static info_lkp_t cyberpower_power_status[] = {
	{ 2, "OL" },
	{ 3, "OB" },
	{ 4, "OL" },
	{ 5, "OL" },
	{ 7, "OL" },
	{ 1, "NULL" },
	{ 6, "NULL" },
	{ 0, NULL }
} ;

/* Snmp2NUT lookup table for CyberPower MIB */
static snmp_info_t cyberpower_mib[] = {
	/* Device page */
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ups",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },

	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "CYBERPOWER",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, CYBERPOWER_OID_MODEL_NAME,
		"CyberPower", SU_FLAG_STATIC, NULL },

	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.1.2.3.0",
		"", SU_FLAG_STATIC, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.1.2.1.0",
		"", SU_FLAG_STATIC, NULL },
	{ "ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.1.2.2.0", "",
		0, NULL },

	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.4.1.1.0", "",
		0 /*SU_STATUS_PWR*/, &cyberpower_power_status[0] },
	{ "ups.load", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.4.2.3.0", "",
		0, NULL },

	/* Battery runtime is expressed in minutes */
	{ "battery.runtime", 0, 60.0, ".1.3.6.1.4.1.3808.1.1.1.2.2.4.0", "",
		0, NULL },
	/* The elapsed time in seconds since the
	 * UPS has switched to battery power */
	{ "battery.runtime.elapsed", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.2.1.2.0", "",
		0, NULL },
	{ "battery.voltage", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.2.2.2.0", "",
		0, NULL },
	{ "battery.current", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.2.2.7.0", "",
		0, NULL },
	{ "battery.charge", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.2.2.1.0", "",
		0, NULL },

	{ "input.voltage", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.3.2.1.0", "",
		0, NULL },
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.3.2.4.0", "",
		0, NULL },

	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.4.2.1.0", "",
		0, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;

mib2nut_info_t	cyberpower = { "cyberpower", CYBERPOWER_MIB_VERSION, NULL,
	CYBERPOWER_OID_MODEL_NAME, cyberpower_mib, CYBERPOWER_SYSOID };
