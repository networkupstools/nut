/*  bestpower-mib.c - data to monitor Eaton Best Power Ferrups
 *                     using earlier version of the ConnectUPS
 *
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

#include "bestpower-mib.h"

#define BESTPOWER_MIB_VERSION		"0.2"
#define BESTPOWER_OID_MODEL_NAME	".1.3.6.1.4.1.2947.1.1.2.0"

/*
 * http://powerquality.eaton.com/Support/Software-Drivers/Downloads/connectivity-firmware/bestpwr2.mib
 */

/* TODO: find the right sysOID for this MIB
 * #define BESTPOWER_SYSOID			".1.3.6.1.4.1.2947???"
 */

static info_lkp_t bestpower_power_status[] = {
	{ 1, "OL", NULL, NULL },
	{ 2, "OB", NULL, NULL },
	{ 0, NULL, NULL, NULL }
} ;

/* Snmp2NUT lookup table for Best Power MIB */
static snmp_info_t bestpower_mib[] = {
	/* Device page */
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ups",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },

	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/*.1.3.6.1.4.1.2947.1.1.1.0 = STRING: "Ferrups"
	.1.3.6.1.4.1.2947.1.1.2.0 = STRING: "FE850VA"*/
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, BESTPOWER_OID_MODEL_NAME,
		"Best Ferrups", SU_FLAG_STATIC, NULL },

	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.1.5.0",
		"", SU_FLAG_STATIC, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.1.7.0",
		"", SU_FLAG_STATIC, NULL },
	{ "ups.power", 0, 1, ".1.3.6.1.4.1.2947.1.1.3.0", "",
		0, NULL },
	{ "ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.1.8.0", "",
		0, NULL },

	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.2.1.0", "",
		0 /*SU_STATUS_PWR*/, &bestpower_power_status[0] },

	/* Battery runtime is expressed in minutes */
	{ "battery.runtime", 0, 60.0, ".1.3.6.1.4.1.2947.1.2.3.0", "",
		0, NULL },
	/* The elapsed time in seconds since the
	 * UPS has switched to battery power */
	{ "battery.runtime.elapsed", 0, 1.0, ".1.3.6.1.4.1.2947.1.2.2.0", "",
		0, NULL },
	{ "battery.voltage", 0, 0.1, ".1.3.6.1.4.1.2947.1.2.4.0", "",
		0, NULL },
	{ "battery.current", 0, 0.1, ".1.3.6.1.4.1.2947.1.2.5.0", "",
		0, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;

mib2nut_info_t	bestpower = { "bestpower", BESTPOWER_MIB_VERSION, NULL,
	BESTPOWER_OID_MODEL_NAME, bestpower_mib };
