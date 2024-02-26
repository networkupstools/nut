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

#define BESTPOWER_MIB_VERSION		"0.40"
#define BESTPOWER_OID_MODEL_NAME	".1.3.6.1.4.1.2947.1.1.2.0"

/*
 * http://powerquality.eaton.com/Support/Software-Drivers/Downloads/connectivity-firmware/bestpwr2.mib
 */

/* TODO: find the right sysOID for this MIB
 * #define BESTPOWER_SYSOID			".1.3.6.1.4.1.2947???"
 */
#define BESTPOWER_SYSOID	BESTPOWER_OID_MODEL_NAME

static info_lkp_t bestpower_power_status[] = {
	info_lkp_default(1, "OL"),
	info_lkp_default(2, "OB"),
	info_lkp_sentinel
};

/* Snmp2NUT lookup table for Best Power MIB */
static snmp_info_t bestpower_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* Device page */
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ups",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),

	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	/*.1.3.6.1.4.1.2947.1.1.1.0 = STRING: "Ferrups"
	.1.3.6.1.4.1.2947.1.1.2.0 = STRING: "FE850VA"*/
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, BESTPOWER_OID_MODEL_NAME,
		"Best Ferrups", SU_FLAG_STATIC, NULL),

	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.1.5.0",
		"", SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.1.7.0",
		"", SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.power", 0, 1, ".1.3.6.1.4.1.2947.1.1.3.0", "",
		0, NULL),
	snmp_info_default("ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.1.8.0", "",
		0, NULL),

	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2947.1.2.1.0", "",
		0 /*SU_STATUS_PWR*/, &bestpower_power_status[0]),

	/* Battery runtime is expressed in minutes */
	snmp_info_default("battery.runtime", 0, 60.0, ".1.3.6.1.4.1.2947.1.2.3.0", "",
		0, NULL),
	/* The elapsed time in seconds since the
	 * UPS has switched to battery power */
	snmp_info_default("battery.runtime.elapsed", 0, 1.0, ".1.3.6.1.4.1.2947.1.2.2.0", "",
		0, NULL),
	snmp_info_default("battery.voltage", 0, 0.1, ".1.3.6.1.4.1.2947.1.2.4.0", "",
		0, NULL),
	snmp_info_default("battery.current", 0, 0.1, ".1.3.6.1.4.1.2947.1.2.5.0", "",
		0, NULL),

	/* end of structure. */
	snmp_info_sentinel
} ;

mib2nut_info_t	bestpower = { "bestpower", BESTPOWER_MIB_VERSION, NULL,
	BESTPOWER_OID_MODEL_NAME, bestpower_mib, BESTPOWER_SYSOID, NULL };
