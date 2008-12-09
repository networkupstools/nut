
/*  raritan-mib.h - data to monitor Raritan PDUs (Basic and Complex)
 *
 *  Copyright (C) 2008
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

#define RARITAN_MIB_VERSION	"0.3"

/* Raritan MIB 
 * this one uses the Revelation MIB, with a different entry point.
 * so we use a trick to have only 1 set of OIDs for 2 set of nut variables */
#undef	AR_BASE_OID
#define	AR_BASE_OID						".1.3.6.1.4.1.13742"

#define	RARITAN_OID_MODEL_NAME					AR_OID_MODEL_NAME

/* Snmp2NUT lookup table for Revelation and Raritan MIBs */
snmp_info_t raritan_mib[] = {
	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_MODEL_NAME,
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_SERIAL, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Raritan",
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

	/* outlet #1 */
	{ "outlet.1.id", 0, 1, NULL, "1", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "outlet.1.switchable", 0, 1, AR_OID_OUTLET_INDEX ".0", "yes", SU_FLAG_STATIC, NULL, NULL },
	{ "outlet.1.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_NAME ".0", NULL, 0, NULL, NULL },
	{ "outlet.1.status", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_STATUS ".0", NULL, 0, &outlet_status_info[0], NULL },
	{ "outlet.1.current", 0, 0.001, AR_OID_OUTLET_CURRENT ".0", NULL, 0, NULL, NULL },
	{ "outlet.1.current.maximum", 0, 0.001, AR_OID_OUTLET_MAXCURRENT ".0", NULL, 0, NULL, NULL },
	{ "outlet.1.realpower", 0, 1.0, AR_OID_OUTLET_ACTIVEPOWER ".0", NULL, 0, NULL, NULL },
	{ "outlet.1.voltage", 0, 1.0, AR_OID_OUTLET_VOLTAGE ".0", NULL, 0, NULL, NULL },
	{ "outlet.1.powerfactor", 0, 0.01, AR_OID_OUTLET_POWERFACTOR ".0", NULL, 0, NULL, NULL },
	{ "outlet.1.power", 0, 1.0, AR_OID_OUTLET_APPARENTPOWER ".0", NULL, 0, NULL, NULL },
	
	/* outlet #2 */
	{ "outlet.2.id", 0, 1, NULL, "2", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "outlet.2.switchable", 0, 1, AR_OID_OUTLET_INDEX ".1", "yes", SU_FLAG_STATIC, NULL, NULL },
	{ "outlet.2.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_NAME ".1", NULL, 0, NULL, NULL },
	{ "outlet.2.status", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_STATUS ".1", NULL, 0, &outlet_status_info[0], NULL },
	{ "outlet.2.current", 0, 0.001, AR_OID_OUTLET_CURRENT ".1", NULL, 0, NULL, NULL },
	{ "outlet.2.current.maximum", 0, 0.001, AR_OID_OUTLET_MAXCURRENT ".1", NULL, 0, NULL, NULL },
	{ "outlet.2.realpower", 0, 1.0, AR_OID_OUTLET_ACTIVEPOWER ".1", NULL, 0, NULL, NULL },
	{ "outlet.2.voltage", 0, 1.0, AR_OID_OUTLET_VOLTAGE ".1", NULL, 0, NULL, NULL },
	{ "outlet.2.powerfactor", 0, 0.01, AR_OID_OUTLET_POWERFACTOR ".1", NULL, 0, NULL, NULL },
	{ "outlet.2.power", 0, 1.0, AR_OID_OUTLET_APPARENTPOWER ".1", NULL, 0, NULL, NULL },
	
	/* outlet #3 */
	{ "outlet.3.id", 0, 1, NULL, "3", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "outlet.3.switchable", 0, 1, AR_OID_OUTLET_INDEX ".2", "yes", SU_FLAG_STATIC, NULL, NULL },
	{ "outlet.3.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_NAME ".2", NULL, 0, NULL, NULL },
	{ "outlet.3.status", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_STATUS ".2", NULL, 0, &outlet_status_info[0], NULL },
	{ "outlet.3.current", 0, 0.001, AR_OID_OUTLET_CURRENT ".2", NULL, 0, NULL, NULL },
	{ "outlet.3.current.maximum", 0, 0.001, AR_OID_OUTLET_MAXCURRENT ".2", NULL, 0, NULL, NULL },
	{ "outlet.3.realpower", 0, 1.0, AR_OID_OUTLET_ACTIVEPOWER ".2", NULL, 0, NULL, NULL },
	{ "outlet.3.voltage", 0, 1.0, AR_OID_OUTLET_VOLTAGE ".2", NULL, 0, NULL, NULL },
	{ "outlet.3.powerfactor", 0, 0.01, AR_OID_OUTLET_POWERFACTOR ".2", NULL, 0, NULL, NULL },
	{ "outlet.3.power", 0, 1.0, AR_OID_OUTLET_APPARENTPOWER ".2", NULL, 0, NULL, NULL },

	/* outlet #4 */
	{ "outlet.4.id", 0, 1, NULL, "2", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "outlet.4.switchable", 0, 1, AR_OID_OUTLET_INDEX ".3", "yes", SU_FLAG_STATIC, NULL, NULL },
	{ "outlet.4.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_NAME ".3", NULL, 0, NULL, NULL },
	{ "outlet.4.status", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_STATUS ".3", NULL, 0, &outlet_status_info[0], NULL },
	{ "outlet.4.current", 0, 0.001, AR_OID_OUTLET_CURRENT ".3", NULL, 0, NULL, NULL },
	{ "outlet.4.current.maximum", 0, 0.001, AR_OID_OUTLET_MAXCURRENT ".3", NULL, 0, NULL, NULL },
	{ "outlet.4.realpower", 0, 1.0, AR_OID_OUTLET_ACTIVEPOWER ".3", NULL, 0, NULL, NULL },
	{ "outlet.4.voltage", 0, 1.0, AR_OID_OUTLET_VOLTAGE ".3", NULL, 0, NULL, NULL },
	{ "outlet.4.powerfactor", 0, 0.01, AR_OID_OUTLET_POWERFACTOR ".3", NULL, 0, NULL, NULL },
	{ "outlet.4.power", 0, 1.0, AR_OID_OUTLET_APPARENTPOWER ".3", NULL, 0, NULL, NULL },


	/* FIXME:
	 * - support for subsequent outlets...
	 * can be up to 20 (infinite when daisy chained!)
	 * - delay for startup/shutdown sequence
	 * - support for Ambient page
		temperatureSensorCount" src="snmp:$sysoid.2.1.0
		ambient.temperature src="snmp:$sysoid.2.2.1.3.$indiceSensor => seems dumb!
		ambient.humidity src="snmp:$sysoid.2.4.1.3.$indiceSensor
	 */

	/* instant commands. */
	/* Note that load.cycle might be replaced by shutdown.reboot */
	/* no counterpart found!	
	{ "outlet.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL }, */
	{ "outlet.1.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.1.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.1.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".0", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.2.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".1", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.2.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".1", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.2.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".1", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.3.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".2", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.3.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".2", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.3.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".2", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.4.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".3", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.4.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".3", NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.4.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".3", NULL, SU_TYPE_CMD, NULL, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL, NULL }
};
