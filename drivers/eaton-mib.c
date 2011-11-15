/*  eaton-mib.c - data to monitor Eaton Aphel PDUs (Basic and Complex)
 *
 *  Copyright (C) 2008 - 2010
 *  			Arnaud Quette <ArnaudQuette@Eaton.com>
 *
 *  Sponsored by Eaton <http://www.eaton.com>
 *   and MGE Office Protection Systems <http://www.mgeops.com>
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

#include "eaton-mib.h"

#define EATON_APHEL_MIB_VERSION	"0.46"

/* APHEL-GENESIS-II-MIB (monitored ePDU)
 * *************************************
 * Note: we should also be able to support this one using netxml-ups!
 */

#define APHEL1_OID_MIB						".1.3.6.1.4.1.17373"
#define APHEL1_SYSOID						APHEL1_OID_MIB
#define APHEL1_OID_MODEL_NAME				".1.3.6.1.4.1.17373.3.1.1.0"
#define APHEL1_OID_FIRMREV					".1.3.6.1.4.1.17373.3.1.2.0"
#define APHEL1_OID_DEVICE_NAME				".1.3.6.1.4.1.17373.3.1.3.0"
#define APHEL1_OID_UNIT_MACADDR				".1.3.6.1.4.1.17373.3.1.4.0"
/* needs concat .<outlet-index>.0 */
#define APHEL1_OID_OUTLET_CURRENT			".1.3.6.1.4.1.17373.3.2"

/* Snmp2NUT lookup table for GenesisII MIB */
static snmp_info_t eaton_aphel_genesisII_mib[] = {
	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_MODEL_NAME,
		"Eaton Powerware ePDU Monitored", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_MODEL_NAME,
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_DEVICE_NAME,
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_FIRMREV, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "ups.macaddr", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_UNIT_MACADDR, "unknown",
		0, NULL, NULL },

	/* Outlet page */
	/* we can't use template since there is no counterpart to outlet.count */
	{ "outlet.1.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".1.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.2.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".2.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.3.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".3.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.4.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".4.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.5.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".5.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.6.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".6.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.7.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".7.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.8.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".8.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL, NULL }
};


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

static info_lkp_t outlet_status_info[] = {
	{ -1, "error" },
	{ 0, "off" },
	{ 1, "on" },
	{ 2, "cycling" }, /* transitional status */
	{ 0, NULL }
};

#define DO_OFF		0
#define DO_ON		1
#define DO_CYCLE	2

#define AR_OID_OUTLET_COUNT				AR_BASE_OID ".1.2.1.0"
#define AR_OID_OUTLET_CURRENT			AR_BASE_OID ".1.2.2.1.4"
#define AR_OID_OUTLET_MAXCURRENT		AR_BASE_OID ".1.2.2.1.5"
#define AR_OID_OUTLET_VOLTAGE			AR_BASE_OID ".1.2.2.1.6"
#define AR_OID_OUTLET_ACTIVEPOWER		AR_BASE_OID ".1.2.2.1.7"
#define AR_OID_OUTLET_APPARENTPOWER		AR_BASE_OID ".1.2.2.1.8"
#define AR_OID_OUTLET_POWERFACTOR		AR_BASE_OID ".1.2.2.1.9"

/* Snmp2NUT lookup table for Eaton Revelation MIB */
static snmp_info_t eaton_aphel_revelation_mib[] = {
	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_MODEL_NAME,
		"Eaton Powerware ePDU Managed", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_SERIAL, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_MODEL_NAME,
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_DEVICE_NAME,
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_SERIAL, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_FIRMREV, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "ups.macaddr", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_UNIT_MACADDR, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.temperature", 0, 1, AR_OID_UNIT_CPUTEMPERATURE, NULL, 0, NULL, NULL },

	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.count", 0, 1, AR_OID_OUTLET_COUNT, "0", 0, NULL },
	{ "outlet.current", 0, 0.001, AR_OID_UNIT_CURRENT ".0", NULL, 0, NULL, NULL },
	{ "outlet.voltage", 0, 0.001, AR_OID_UNIT_VOLTAGE ".0", NULL, 0, NULL, NULL },
	{ "outlet.realpower", 0, 1.0, AR_OID_UNIT_ACTIVEPOWER ".0", NULL, 0, NULL, NULL },
	{ "outlet.power", 0, 1.0, AR_OID_UNIT_APPARENTPOWER ".0", NULL, 0, NULL, NULL },

	/* outlet template definition
	 * Caution: the index of the data start at 0, while the name is +1
	 * ie outlet.1 => <OID>.0 */
	{ "outlet.%i.switchable", 0, 1, AR_OID_OUTLET_INDEX ".%i", "yes", SU_FLAG_STATIC | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.id", 0, 1, NULL, "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_NAME ".%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_STATUS ".%i", NULL, SU_FLAG_OK | SU_OUTLET, &outlet_status_info[0], NULL },
	{ "outlet.%i.current", 0, 0.001, AR_OID_OUTLET_CURRENT ".%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.current.maximum", 0, 0.001, AR_OID_OUTLET_MAXCURRENT ".%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.realpower", 0, 1.0, AR_OID_OUTLET_ACTIVEPOWER ".%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.voltage", 0, 1.0, AR_OID_OUTLET_VOLTAGE ".%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.powerfactor", 0, 0.01, AR_OID_OUTLET_POWERFACTOR ".%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.power", 0, 1.0, AR_OID_OUTLET_APPARENTPOWER ".%i", NULL, SU_OUTLET, NULL, NULL },

	/* FIXME:
	 * - delay for startup/shutdown sequence
	 * - support for multiple Ambient sensors ( max. 8), starting at index '0'
	 * 		ambient.%i.temperature => .1.3.6.1.4.1.534.6.6.6.2.2.1.3.%i
	 * 		ambient.%i.humidity => .1.3.6.1.4.1.534.6.6.6.2.4.1.3.%i
	 */

	/* Ambient page */
	/* We use critical levels, for both temperature and humidity,
	 * since warning levels are also available! */
	{ "ambient.temperature", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.2.1.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	{ "ambient.temperature.low", 0, 1.0, "1.3.6.1.4.1.534.6.6.6.2.2.1.6.0", NULL, SU_FLAG_OK, NULL, NULL },
	{ "ambient.temperature.high", 0, 1.0, "1.3.6.1.4.1.534.6.6.6.2.2.1.7.0", NULL, SU_FLAG_OK, NULL, NULL },
	{ "ambient.humidity", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.4.1.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	{ "ambient.humidity.low", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.4.1.6.0", NULL, SU_FLAG_OK, NULL, NULL },
	{ "ambient.humidity.high", 0, 1.0, ".1.3.6.1.4.1.534.6.6.6.2.4.1.7.0", NULL, SU_FLAG_OK, NULL, NULL },

	/* instant commands. */
	/* Note that load.cycle might be replaced by / mapped on shutdown.reboot */
	/* no counterpart found!
	{ "outlet.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL }, */
	{ "outlet.%i.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".%i", NULL, SU_TYPE_CMD | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".%i", NULL, SU_TYPE_CMD | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".%i", NULL, SU_TYPE_CMD | SU_OUTLET, NULL, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL, NULL }
};

/* Eaton PDU-MIB - Marlin MIB
 * ************************** */

#define EATON_MARLIN_MIB_VERSION	"0.05"
#define EATON_MARLIN_SYSOID			".1.3.6.1.4.1.534.6.6.7"
#define EATON_MARLIN_OID_MODEL_NAME	".1.3.6.1.4.1.534.6.6.7.1.2.1.2.0"

static info_lkp_t marlin_outlet_status_info[] = {
	{ 0, "off" },
	{ 1, "on" },
	{ 2, "pendingOff" }, /* transitional status */
	{ 3, "pendingOn" }, /* transitional status */
	{ 0, NULL }
};

/* Snmp2NUT lookup table for Eaton Marlin MIB */
static snmp_info_t eaton_marlin_mib[] = {
	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.1.2.1.2.0",
		"Eaton Powerware ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.1.2.1.4.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	/* FIXME: need RFC validation on this variable
	 * { "device.part", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.1.2.1.3.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL }, */

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.6.6.7.1.2.1.2.0",
		"Eaton Powerware ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },

	/*	FIXME: use unitName.0	(ePDU)?
	 * { "ups.id", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_DEVICE_NAME,
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL }, */
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.1.2.1.4.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.1.2.1.5.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	/* TODO:
	 * The below possibly requires (?) the use of
	 * 	int snprint_hexstring(char *buf, size_t buf_len, const u_char *, size_t);
	 * { "ups.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.2.2.1.6.2",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	 * + date reformating callback
	 *   2011-8-29,16:27:25.0,+1:0
	 *   Hex-STRING: 07 DB 08 1D 10 0C 36 00 2B 01 00 00 
	 * { "ups.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.1.2.1.8.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	 * { "ups.time", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.1.2.1.8.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	 */

	/* Input page */
	{ "input.phases", 0, 1, ".1.3.6.1.4.1.534.6.6.7.1.2.1.20.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	/* inputType.0.1	singlePhase  (1) 		iso.3.6.1.4.1.534.6.6.7.3.1.1.2.0.1 */
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.3.1.1.3.0.1", NULL, 0, NULL, NULL },
	{ "input.voltage", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.3.2.1.3.0.1.1", NULL, 0, NULL, NULL },
	/* FIXME: check multiplier */
	{ "input.current", 0, 0.01, ".1.3.6.1.4.1.534.6.6.7.3.3.1.4.0.1.1", NULL, 0, NULL, NULL },

	/* Ambient page */
	/* We use critical levels, for both temperature and humidity,
	 * since warning levels are also available! */
	{ "ambient.temperature", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.1.1.4.0.1", NULL, SU_FLAG_OK, NULL, NULL },
	{ "ambient.temperature.low", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.1.1.7.0.1", NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL, NULL },
	{ "ambient.temperature.high", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.1.1.9.0.1", NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL, NULL },
	{ "ambient.humidity", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.2.1.4.0.1", NULL, SU_FLAG_OK, NULL, NULL },
	{ "ambient.humidity.low", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.2.1.7.0.1", NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL, NULL },
	{ "ambient.humidity.high", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.2.1.9.0.1", NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL, NULL },

	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.534.6.6.7.1.2.1.22.0", "0", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	/* The below ones are the same as the input.* equivalent */
	{ "outlet.frequency", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.3.1.1.3.0.1", NULL, 0, NULL, NULL },
	{ "outlet.voltage", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.3.2.1.3.0.1.1", NULL, 0, NULL, NULL },
	{ "outlet.current", 0, 0.01, ".1.3.6.1.4.1.534.6.6.7.3.3.1.4.0.1.1", NULL, 0, NULL, NULL },
	/* There is also a .2 available (ie .1.3.6.1.4.1.534.6.6.7.3.4.1.3.0.1.2) */
	{ "outlet.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.4.0.1.2", NULL, 0, NULL, NULL },
	/* There is also a .2 available (ie .1.3.6.1.4.1.534.6.6.7.3.4.1.3.0.1.2) */
	{ "outlet.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.3.0.1.1", NULL, 0, NULL, NULL },

	/* outlet template definition
	 * Indexes start from 1, ie outlet.1 => <OID>.1 */
	{ "outlet.%i.switchable", 0, 1, ".1.3.6.1.4.1.534.6.6.7.6.6.1.3.0.%i", "yes", SU_FLAG_STATIC | SU_OUTLET, NULL, NULL },
	/* Note: the first definition is used to determine the base index (ie 0 or 1) */
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.6.1.1.3.0.%i", NULL, SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.6.7.6.6.1.2.0.%i",
		NULL, SU_FLAG_OK | SU_OUTLET, &marlin_outlet_status_info[0], NULL },
	/* FIXME: or use ".1.3.6.1.4.1.534.6.6.7.6.1.1.2.0.1", though it's related to groups! */
	{ "outlet.%i.id", 0, 1, NULL, "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.current", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.6.4.1.3.0.%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.6.5.1.3.0.%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.voltage", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.6.3.1.2.0.%i", NULL, SU_OUTLET, NULL, NULL },
	{ "outlet.%i.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.6.5.1.2.0.%i", NULL, SU_OUTLET, NULL, NULL },

	/* TODO: handle statistics
	 * outletWh.0.1
	 * outletWhTimer.0.1
	 */

	/* instant commands. */
	/* Notes:
	 * - load.cycle might be replaced by / mapped on shutdown.reboot 
	 * - outletControl{Off,On,Reboot}Cmd values:
	 * 		0-n : Timer
	 * 		-1 : Cancel
	 * 		we currently use "0", so instant On | Off | Reboot... */
	/* no counterpart found!
	{ "outlet.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL }, */

	/* TODO: handle delays */
	{ "outlet.%i.load.off", 0, 0, ".1.3.6.1.4.1.534.6.6.7.6.6.1.3.0.%i", NULL, SU_TYPE_CMD | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.load.on", 0, 0, ".1.3.6.1.4.1.534.6.6.7.6.6.1.4.0.%i", NULL, SU_TYPE_CMD | SU_OUTLET, NULL, NULL },
	{ "outlet.%i.load.cycle", 0, 0, ".1.3.6.1.4.1.534.6.6.7.6.6.1.5.0.%i", NULL, SU_TYPE_CMD | SU_OUTLET, NULL, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL, NULL }
};

mib2nut_info_t	aphel_genesisII = { "aphel_genesisII", EATON_APHEL_MIB_VERSION, "", APHEL1_OID_MODEL_NAME, eaton_aphel_genesisII_mib, APHEL1_SYSOID };
mib2nut_info_t	aphel_revelation = { "aphel_revelation", EATON_APHEL_MIB_VERSION, "", APHEL2_OID_MODEL_NAME, eaton_aphel_revelation_mib, APHEL2_SYSOID };
mib2nut_info_t	eaton_marlin = { "eaton_epdu", EATON_MARLIN_MIB_VERSION, "", EATON_MARLIN_OID_MODEL_NAME, eaton_marlin_mib, EATON_MARLIN_SYSOID };

