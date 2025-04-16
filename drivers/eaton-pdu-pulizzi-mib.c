/*  eaton-pdu-pulizzi-mib.c - data to monitor Eaton ePDUs branded as:
 *                G1 Pulizzi Monitored and Switched ePDUs
 *
 *  Copyright (C) 2008 - 2017
 * 		Arnaud Quette <arnaud.quette@gmail.com>
 * 		Arnaud Quette <ArnaudQuette@Eaton.com>
 *  Copyright (C) 2015 - 2017
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

#include "eaton-pdu-pulizzi-mib.h"

/* Pulizzi Monitored ePDU (Basic model, SNMP only)
 * FIXME: to be completed
 *
 * Warning: there are 2 versions:
 * - SA built MI.mib (old MIB)
 * 		#define PULIZZI1_OID_MIB			".1.3.6.1.4.1.20677.3.1.1"
 * 		#define PULIZZI1_OID_MODEL_NAME		".1.3.6.1.4.1.20677.3.1.1.1.2.0"
 * - Eaton-Powerware-Monitored-ePDU_1.0.E.mib (new MIB) Vertical SW
 */


/* Pulizzi Switched ePDU */

#define EATON_PULIZZI_SW_MIB_VERSION	"0.50"

#define PULIZZI_SW_OID_MIB			".1.3.6.1.4.1.20677.3.1.1"
#define PULIZZI_SW_OID_MODEL_NAME		".1.3.6.1.4.1.20677.2.1.1.0"

/* Some buggy FW also report sysOID = ".1.3.6.1.4.1.20677.1" */
#define EATON_PULIZZI_SWITCHED1_SYSOID			".1.3.6.1.4.1.20677.1"
#define EATON_PULIZZI_SWITCHED2_SYSOID			".1.3.6.1.4.1.20677.2"


static info_lkp_t pulizzi_sw_outlet_status_info[] = {
	info_lkp_default(1, "on"),
	info_lkp_default(2, "off"),
	info_lkp_sentinel
};

/* simply remap the above status to "yes" */
static info_lkp_t pulizzi_sw_outlet_switchability_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "yes"),
	info_lkp_sentinel
};

/* Snmp2NUT lookup table for Eaton Pulizzi Switched ePDU MIB */
static snmp_info_t eaton_pulizzi_switched_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* Device page */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, PULIZZI_SW_OID_MODEL_NAME,
		"Switched ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.20677.2.2.6.0",
		"unknown", 0, NULL),

	/* UPS page */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, PULIZZI_SW_OID_MODEL_NAME,
		"Switched ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	 /* FIXME: to be moved to the device collection! */
	snmp_info_default("ups.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.20677.2.1.4.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.time", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.20677.2.1.3.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	/* Outlet page */
	/* Note: outlet.count is deduced, with guesstimate_outlet_count() */
	snmp_info_default("outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),

	snmp_info_default("outlet.current", 0, 1.0, ".1.3.6.1.4.1.20677.2.8.6.4.2.0", NULL, 0, NULL),
	snmp_info_default("outlet.voltage", 0, 1.0, ".1.3.6.1.4.1.20677.2.8.6.4.1.0", NULL, 0, NULL),
	snmp_info_default("outlet.power", 0, 1.0, ".1.3.6.1.4.1.20677.2.8.6.4.3.0", NULL, 0, NULL),

	/* outlet template definition
	 * Notes:
	 * - indexes start from 1, ie outlet.1 => <OID>.1
	 * - the first definition is used to determine the base index (ie 0 or 1)
	 * - outlet.count is estimated, based on the below OID iteration capabilities */
	snmp_info_default("outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.20677.2.6.1.%i.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET, NULL),
	snmp_info_default("outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.20677.2.6.3.%i.0",
		NULL, SU_FLAG_OK | SU_OUTLET, &pulizzi_sw_outlet_status_info[0]),
	snmp_info_default("outlet.%i.id", 0, 1, NULL, "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, NULL),
	/* we use the same OID as outlet.n.status..., to expose switchability */
	snmp_info_default("outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.20677.2.6.3.%i.0", "yes", SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET, &pulizzi_sw_outlet_switchability_info[0]),
	/* FIXME: need to be added to the namespace! */
	snmp_info_default("outlet.%i.delay.reboot", ST_FLAG_RW, 1, ".1.3.6.1.4.1.20677.2.6.1.%i.5.0", NULL, SU_OUTLET, NULL),
	/* "outlet1SequenceTime" is used for global sequence */
	snmp_info_default("outlet.%i.delay.start", ST_FLAG_RW, 1, ".1.3.6.1.4.1.20677.2.6.1.%i.4.0", NULL, SU_OUTLET, NULL),

	/* instant commands. */
	/* FIXME: not exposed as "outlet.load...", or otherwise specific processing applies (template instanciation) */
	snmp_info_default("load.on", 0, 1, ".1.3.6.1.4.1.20677.2.6.2.1.0", "1", SU_TYPE_CMD, NULL),
	snmp_info_default("load.off", 0, 1, ".1.3.6.1.4.1.20677.2.6.2.1.0", "2", SU_TYPE_CMD, NULL),
	snmp_info_default("load.on.delay", 0, 1, ".1.3.6.1.4.1.20677.2.6.2.1.0", "3", SU_TYPE_CMD, NULL),
	snmp_info_default("load.off.delay", 0, 1, ".1.3.6.1.4.1.20677.2.6.2.1.0", "4", SU_TYPE_CMD, NULL),

	/* WARNING: outlet 1 => index 2, so SU_CMD_OFFSET! */
	snmp_info_default("outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.20677.2.6.2.%i.0", "1", SU_TYPE_CMD | SU_OUTLET | SU_CMD_OFFSET, NULL),
	snmp_info_default("outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.20677.2.6.2.%i.0", "2", SU_TYPE_CMD | SU_OUTLET | SU_CMD_OFFSET, NULL),
	snmp_info_default("outlet.%i.load.cycle", 0, 1, ".1.3.6.1.4.1.20677.2.6.2.%i.0", "3", SU_TYPE_CMD | SU_OUTLET | SU_CMD_OFFSET, NULL),

	/* end of structure. */
	snmp_info_sentinel
};


/*mib2nut_info_t	pulizzi_monitored = { "pulizzi_monitored", EATON_PULIZZI_MIB_VERSION, NULL, PULIZZI1_OID_MODEL_NAME, eaton_pulizzi_monitored_mib, PULIZZI1_OID_MIB };*/
mib2nut_info_t	pulizzi_switched1 = { "pulizzi_switched1", EATON_PULIZZI_SW_MIB_VERSION, NULL, EATON_PULIZZI_SWITCHED1_SYSOID, eaton_pulizzi_switched_mib, EATON_PULIZZI_SWITCHED1_SYSOID, NULL };
mib2nut_info_t	pulizzi_switched2 = { "pulizzi_switched2", EATON_PULIZZI_SW_MIB_VERSION, NULL, EATON_PULIZZI_SWITCHED1_SYSOID, eaton_pulizzi_switched_mib, EATON_PULIZZI_SWITCHED2_SYSOID, NULL };
