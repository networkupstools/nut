/* emerson-avocent-pdu-mib.c - subdriver to monitor Emerson Avocent PDUs with NUT
 *
 *  Copyright (C)
 *    2008-2018 Arnaud Quette <arnaud.quette@gmail.com>
 *    2009 Opengear <support@opengear.com>
 *    2017 Eaton (Arnaud Quette <ArnaudQuette@Eaton.com>)
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
#define OPENGEAR_MULTIPLE_BANKS 1

#include "emerson-avocent-pdu-mib.h"

#define EMERSON_AVOCENT_MIB_VERSION		"1.0"
#define EMERSON_AVOCENT_SYSOID			".1.3.6.1.4.1.10418.17.1.7"
#define EMERSON_AVOCENT_OID_MODEL_NAME	".1.3.6.1.4.1.10418.17.2.1.2.0"

/* FIXME: Avocent PM's seem to have 3 temperature sensors (index 1, 2, 3)
 * for the embedded temperature (equivalent to ups.temperature) */
#define AVOCENT_OID_UNIT_TEMPERATURE ".1.3.6.1.4.1.10418.17.2.5.3.1.17.1.1"

/* Same as above for humidity... */
#define AVOCENT_OID_UNIT_HUMIDITY ".1.3.6.1.4.1.10418.17.2.5.3.1.24.1"

#define AVOCENT_OID_OUTLET_COUNT	".1.3.6.1.4.1.10418.17.2.5.3.1.8.%i.%i"

/* FIXME: This is actually pmPowerMgmtPDUTableCurrent1Value */
#define AVOCENT_OID_UNIT_CURRENT	".1.3.6.1.4.1.10418.17.2.5.3.1.10.1.1"
/* FIXME: This is actually pmPowerMgmtPDUTableVoltage1Value */
#define AVOCENT_OID_UNIT_VOLTAGE	".1.3.6.1.4.1.10418.17.2.5.3.1.31.1.1"
#define AVOCENT_OID_UNIT_MACADDR        ".1.3.6.1.2.1.2.2.1.6.1"

#ifdef OPENGEAR_MULTIPLE_BANKS
#define AVOCENT_OID_OUTLET_ID		".1.3.6.1.4.1.10418.17.2.5.5.1.3"
#define AVOCENT_OID_OUTLET_NAME		".1.3.6.1.4.1.10418.17.2.5.5.1.4"
#define AVOCENT_OID_OUTLET_STATUS	".1.3.6.1.4.1.10418.17.2.5.5.1.5"
/* This the actual value for the Current of the sensor. */
#define AVOCENT_OID_OUTLET_LOAD		".1.3.6.1.4.1.10418.17.2.5.5.1.50"
#define AVOCENT_OID_OUTLET_CONTROL	".1.3.6.1.4.1.10418.17.2.5.5.1.6"
#else
#define AVOCENT_OID_OUTLET_ID		".1.3.6.1.4.1.10418.17.2.5.5.1.3.1.1"
#define AVOCENT_OID_OUTLET_NAME		".1.3.6.1.4.1.10418.17.2.5.5.1.4.1.1"
#define AVOCENT_OID_OUTLET_STATUS	".1.3.6.1.4.1.10418.17.2.5.5.1.5.1.1"
#define AVOCENT_OID_OUTLET_LOAD		".1.3.6.1.4.1.10418.17.2.5.5.1.50.1.1"
#define AVOCENT_OID_OUTLET_CONTROL	".1.3.6.1.4.1.10418.17.2.5.5.1.6.1.1"
#endif

info_lkp_t avocent_outlet_status_info[] = {
	{ 1, "off" },
	{ 2, "on" },
/*	{ 3, "offLocked" },
	{ 4, "onLocked" },
	{ 5, "offCycle" },
	{ 6, "onPendingOff" },
	{ 7, "offPendingOn" },
	{ 8, "onPendingCycle" },
	{ 9, "notSet" },
	{ 10, "onFixed" },
	{ 11, "offShutdown" },
	{ 12, "tripped" },*/
	{ 0, NULL }
};

snmp_info_t emerson_avocent_pdu_mib[] = {
	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Avocent",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.10418.17.2.5.3.1.5.1.%i", /* EMERSON_AVOCENT_OID_MODEL_NAME */
		"Avocent SNMP PDU", SU_FLAG_ABSENT | SU_FLAG_OK | SU_FLAG_NAINVALID, NULL, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.10418.17.2.1.4.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	/* Daisychained devices support
	 * Notes: this definition is used to:
	 * - estimate the number of devices, based on the below OID iteration capabilities
	 * - determine the base index of the SNMP OID (ie 0 or 1) */
	{ "device.count", 0, 1, ".1.3.6.1.4.1.10418.17.2.5.2.1.4.1",
		"1", SU_FLAG_STATIC, NULL, NULL /* devices_count */ },

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Avocent",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, EMERSON_AVOCENT_OID_MODEL_NAME,
		"Avocent SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.10418.17.2.1.1.0",
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.10418.17.2.1.4.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.10418.17.2.1.7.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "ups.macaddr", ST_FLAG_STRING, SU_INFOSIZE, AVOCENT_OID_UNIT_MACADDR,
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.10418.17.2.5.3.1.8.1.%i", "0", SU_FLAG_STATIC | SU_FLAG_ZEROINVALID | SU_FLAG_OK, NULL },

	/* outlets */
	/* NOTE: there is a bug in Avocent FW:
	 * index '0' should not respond (and is not in subtree mode) but answers
	 * to unitary get, since OIDs start at index '1'.
	 *Use the status data below to test since '0' is not a supported value */
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.10418.17.2.5.5.1.5.1.%i.%i", NULL, SU_OUTLET | SU_TYPE_DAISY_1 | SU_FLAG_ZEROINVALID, &avocent_outlet_status_info[0], NULL },
	{ "outlet.%i.id", 0, 1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.3.1.%i.%i", NULL, SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.10418.17.2.5.5.1.4.1.%i.%i", NULL, SU_OUTLET | SU_TYPE_DAISY_1 | SU_FLAG_NAINVALID, NULL, NULL },
	/* pmPowerMgmtOutletsTableCurrentValue.1.1.1; Value (Integer): 0 */
	{ "outlet.%i.current", 0, 0.1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.50.1.%i.%i", NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	/* pmPowerMgmtOutletsTableCurrentHighCritical.1.1.1; Value (Integer): 160 */
	{ "outlet.%i.current.high.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.100.1.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	/* pmPowerMgmtOutletsTableCurrentHighWarning.1.1.1; Value (Integer): 120 */
	{ "outlet.%i.current.high.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.101.1.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	/* pmPowerMgmtOutletsTableCurrentLowWarning.1.1.1; Value (Integer): 0 */
	{ "outlet.%i.current.low.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.102.1.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	/* pmPowerMgmtOutletsTableCurrentLowCritical.1.1.1; Value (Integer): 0 */
	{ "outlet.%i.current.low.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.103.1.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	/* pmPowerMgmtOutletsTablePowerValue.1.1.1; Value (Integer): 0 */
	{ "outlet.%i.realpower", 0, 0.1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.60.1.%i.%i", NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	/* pmPowerMgmtOutletsTableVoltageValue.1.1.1; Value (Integer): 238 */
	{ "outlet.%i.voltage", 0, 1,
		".1.3.6.1.4.1.10418.17.2.5.5.1.70.1.%i.%i", NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },

	/* TODO: handle statistics
	 * pmPowerMgmtOutletsTableEnergyValue.1.1.1; Value (Integer): 0 (Wh)
	 * pmPowerMgmtOutletsTableEnergyStartTime.1.1.1; Value (OctetString): 2018-02-13 10:40:09
	 * pmPowerMgmtOutletsTableEnergyReset.1.1.1; Value (Integer): noAction (1)
	 */

	/* Outlet groups collection */
	/* pmPowerMgmtNumberOfOutletGroup.0; Value (Integer): 0 */
	{ "outlet.group.count", 0, 1, ".1.3.6.1.4.1.10418.17.2.5.6.%i",
		"0", SU_FLAG_STATIC | SU_TYPE_DAISY_1, NULL, NULL },

	/* TODO: support for "Banks" (not sure to understand what is this?!)
	 * pmPowerMgmtTotalNumberOfBanks.0; Value (Integer): 6
	 * pmPowerMgmtBanksTableName.1.1.1; Value (OctetString): 18-bf-ffP0_1_A
	 */

	/* According to MIB Power Control values are:
	 * noAction(1),
	 * powerOn(2),
	 * powerOff(3),
	 * powerCycle(4),
	 * powerLock(5),
	 * powerUnlock(6)
	 */
	{ "outlet.%i.load.cycle", 0, 4, ".1.3.6.1.4.1.10418.17.2.5.5.1.6.1.%i.%i", NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	{ "outlet.%i.load.off", 0, 3, ".1.3.6.1.4.1.10418.17.2.5.5.1.6.1.%i.%i", NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },
	{ "outlet.%i.load.on", 0, 2, ".1.3.6.1.4.1.10418.17.2.5.5.1.6.1.%i.%i", NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL, NULL }
};

mib2nut_info_t	emerson_avocent_pdu = { "emerson_avocent_pdu", EMERSON_AVOCENT_MIB_VERSION, NULL, EMERSON_AVOCENT_OID_MODEL_NAME, emerson_avocent_pdu_mib, EMERSON_AVOCENT_SYSOID };
