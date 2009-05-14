/*  eaton-aphel-mib.h - data to monitor Eaton Aphel PDUs (Basic and Complex)
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

#define EATON_APHEL_MIB_VERSION	"0.4"

/* APHEL-GENESIS-II-MIB (monitored ePDU)
 * *************************************
 * Note: we should also be able to support this one using netxml-ups!
 */

#define APHEL1_OID_MIB						".1.3.6.1.4.1.17373"
#define APHEL1_OID_MODEL_NAME				".1.3.6.1.4.1.17373.3.1.1.0"
#define APHEL1_OID_FIRMREV					".1.3.6.1.4.1.17373.3.1.2.0"
#define APHEL1_OID_DEVICE_NAME				".1.3.6.1.4.1.17373.3.1.3.0"
#define APHEL1_OID_UNIT_MACADDR				".1.3.6.1.4.1.17373.3.1.4.0"
/* needs concat .<outlet-index>.0 */
#define APHEL1_OID_OUTLET_CURRENT			".1.3.6.1.4.1.17373.3.2"

/* Snmp2NUT lookup table for GenesisII MIB */
snmp_info_t eaton_aphel_genesisII_mib[] = {
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
	{ "outlet.1.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".1.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.2.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".2.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.3.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".3.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.4.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".4.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.5.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".5.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.6.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".6.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.7.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".7.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },
	{ "outlet.8.current", 0, 0.001, APHEL1_OID_OUTLET_CURRENT ".8.0", NULL, SU_FLAG_NEGINVALID, NULL, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL, NULL }
};


/* APHEL PDU-MIB - Revelation MIB (Managed ePDU)
 * ********************************************* */

#define AR_BASE_OID					".1.3.6.1.4.1.534.6.6.6"

#define APHEL2_OID_MODEL_NAME				AR_OID_MODEL_NAME 

/* Common Aphel / Raritan declaration */

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

info_lkp_t outlet_status_info[] = {
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
snmp_info_t eaton_aphel_revelation_mib[] = {
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
	 * - support for Ambient page
		temperatureSensorCount" src="snmp:$sysoid.2.1.0
		ambient.temperature src="snmp:$sysoid.2.2.1.3.$indiceSensor => seems dumb!
		ambient.humidity src="snmp:$sysoid.2.4.1.3.$indiceSensor
	 */

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
