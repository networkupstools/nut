/*  eaton-pdu-revelation-mib.c - data to monitor Eaton ePDUs branded as:
 *                G1 Aphel based ePDUs (Complex) - Revelation
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

#include "eaton-pdu-revelation-mib.h"

#define EATON_APHEL_REVELATION_MIB_VERSION	"0.50"

/* APHEL PDU-MIB - Revelation MIB (Managed ePDU)
 * ********************************************* */

#define AR_BASE_OID						".1.3.6.1.4.1.534.6.6.6"
#define APHEL2_SYSOID					AR_BASE_OID
#define APHEL2_OID_MODEL_NAME			AR_OID_MODEL_NAME

#define AR_OID_MODEL_NAME				AR_BASE_OID ".1.1.12.0"
#define AR_OID_DEVICE_NAME				AR_BASE_OID ".1.1.13.0"
#define AR_OID_FIRMREV					AR_BASE_OID ".1.1.1.0"
#define AR_OID_SERIAL					AR_BASE_OID ".1.1.2.0"
#define AR_OID_UNIT_MACADDR				AR_BASE_OID ".1.1.6.0"

#define AR_OID_UNIT_CURRENT				AR_BASE_OID ".1.3.1.1"
#define AR_OID_UNIT_VOLTAGE				AR_BASE_OID ".1.3.1.2"
#define AR_OID_UNIT_ACTIVEPOWER			AR_BASE_OID ".1.3.1.3"
#define AR_OID_UNIT_APPARENTPOWER		AR_BASE_OID ".1.3.1.4"
#define AR_OID_UNIT_CPUTEMPERATURE		AR_BASE_OID ".1.3.1.5.0"

#define AR_OID_OUTLET_INDEX				AR_BASE_OID ".1.2.2.1.1"
#define AR_OID_OUTLET_NAME				AR_BASE_OID ".1.2.2.1.2"
#define AR_OID_OUTLET_STATUS			AR_BASE_OID ".1.2.2.1.3"

static info_lkp_t revelation_outlet_status_info[] = {
	{ -1, "error", NULL, NULL },
	{ 0, "off", NULL, NULL },
	{ 1, "on", NULL, NULL },
	{ 2, "cycling", NULL, NULL }, /* transitional status */
	{ 0, NULL, NULL, NULL }
};

/* Ugly hack: having the matching OID present means that the outlet is
 * switchable. So, it should not require this value lookup */
static info_lkp_t revelation_outlet_switchability_info[] = {
	{ -1, "yes", NULL, NULL },
	{ 0, "yes", NULL, NULL },
	{ 1, "yes", NULL, NULL },
	{ 2, "yes", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

#define DO_OFF		"0"
#define DO_ON		"1"
#define DO_CYCLE	"2"

#define AR_OID_OUTLET_COUNT				AR_BASE_OID ".1.2.1.0"
#define AR_OID_OUTLET_CURRENT			AR_BASE_OID ".1.2.2.1.4"
#define AR_OID_OUTLET_MAXCURRENT		AR_BASE_OID ".1.2.2.1.5"
#define AR_OID_OUTLET_VOLTAGE			AR_BASE_OID ".1.2.2.1.6"
#define AR_OID_OUTLET_ACTIVEPOWER		AR_BASE_OID ".1.2.2.1.7"
#define AR_OID_OUTLET_APPARENTPOWER		AR_BASE_OID ".1.2.2.1.8"
#define AR_OID_OUTLET_POWERFACTOR		AR_BASE_OID ".1.2.2.1.9"

/* Snmp2NUT lookup table for Eaton Revelation MIB */
static snmp_info_t eaton_aphel_revelation_mib[] = {
	/* Device collection */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_MODEL_NAME,
		"Eaton Powerware ePDU Managed", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_SERIAL, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_UNIT_MACADDR, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* hardwareRev.0 = Integer: 26 */
	/* FIXME: not compliant! to be RFC'ed */
	{ "device.revision", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.6.1.1.7.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/* UPS collection */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_MODEL_NAME,
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_DEVICE_NAME,
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_SERIAL, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_FIRMREV, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.temperature", 0, 1, AR_OID_UNIT_CPUTEMPERATURE, NULL, 0, NULL },

	/* Outlet collection */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.count", 0, 1, AR_OID_OUTLET_COUNT, "0", 0, NULL },
	{ "outlet.current", 0, 0.001, AR_OID_UNIT_CURRENT ".0", NULL, 0, NULL },
	{ "outlet.voltage", 0, 0.001, AR_OID_UNIT_VOLTAGE ".0", NULL, 0, NULL },
	{ "outlet.realpower", 0, 1.0, AR_OID_UNIT_ACTIVEPOWER ".0", NULL, 0, NULL },
	{ "outlet.power", 0, 1.0, AR_OID_UNIT_APPARENTPOWER ".0", NULL, 0, NULL },

	/* outlet template definition
	 * Caution: the index of the data start at 0, while the name is +1
	 * ie outlet.1 => <OID>.0 */
	{ "outlet.%i.switchable", 0, 1, AR_OID_OUTLET_STATUS ".%i", "yes", SU_FLAG_STATIC | SU_OUTLET, &revelation_outlet_switchability_info[0] },
	{ "outlet.%i.id", 0, 1, NULL, "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, NULL },
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_NAME ".%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_STATUS ".%i", NULL, SU_FLAG_OK | SU_OUTLET, &revelation_outlet_status_info[0] },
	{ "outlet.%i.current", 0, 0.001, AR_OID_OUTLET_CURRENT ".%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.current.maximum", 0, 0.001, AR_OID_OUTLET_MAXCURRENT ".%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.realpower", 0, 1.0, AR_OID_OUTLET_ACTIVEPOWER ".%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.voltage", 0, 1.0, AR_OID_OUTLET_VOLTAGE ".%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.powerfactor", 0, 0.01, AR_OID_OUTLET_POWERFACTOR ".%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.power", 0, 1.0, AR_OID_OUTLET_APPARENTPOWER ".%i", NULL, SU_OUTLET, NULL },

	/* FIXME:
	 * - delay for startup/shutdown sequence
	 * - support for multiple Ambient sensors ( max. 8), starting at index '0'
	 * 		ambient.%i.temperature => .1.3.6.1.4.1.534.6.6.6.2.2.1.3.%i
	 * 		ambient.%i.humidity => .1.3.6.1.4.1.534.6.6.6.2.4.1.3.%i
	 */

	/* Ambient collection */
	/* We use critical levels, for both temperature and humidity,
	 * since warning levels are also available! */
	{ "ambient.temperature", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.2.1.3.0", NULL, SU_FLAG_OK, NULL },
	{ "ambient.temperature.low", 0, 1.0, "1.3.6.1.4.1.534.6.6.6.2.2.1.6.0", NULL, SU_FLAG_OK, NULL },
	{ "ambient.temperature.high", 0, 1.0, "1.3.6.1.4.1.534.6.6.6.2.2.1.7.0", NULL, SU_FLAG_OK, NULL },
	{ "ambient.humidity", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.4.1.3.0", NULL, SU_FLAG_OK, NULL },
	{ "ambient.humidity.low", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.4.1.6.0", NULL, SU_FLAG_OK, NULL },
	{ "ambient.humidity.high", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.4.1.7.0", NULL, SU_FLAG_OK, NULL },

	/* instant commands. */
	/* Note that load.cycle might be replaced by / mapped on shutdown.reboot */
	/* no counterpart found!
	{ "outlet.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL },
	{ "outlet.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL },
	{ "outlet.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL }, */
	{ "outlet.%i.load.off", 0, 1, AR_OID_OUTLET_STATUS ".%i", DO_OFF, SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.on", 0, 1, AR_OID_OUTLET_STATUS ".%i", DO_ON, SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.cycle", 0, 1, AR_OID_OUTLET_STATUS ".%i", DO_CYCLE, SU_TYPE_CMD | SU_OUTLET, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};


mib2nut_info_t	aphel_revelation = { "aphel_revelation", EATON_APHEL_REVELATION_MIB_VERSION, NULL, APHEL2_OID_MODEL_NAME, eaton_aphel_revelation_mib, APHEL2_SYSOID };
