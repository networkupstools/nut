/* xppc-mib.c - subdriver to monitor XPPC SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *  2014       	Charles Lepple <clepple+nut@gmail.com>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  scripts/subdriver/gen-snmp-subdriver.sh script.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "xppc-mib.h"

#define XPPC_MIB_VERSION  "0.2"

#define XPPC_SYSOID       ".1.3.6.1.4.1.935"

/* To create a value lookup structure (as needed on the 2nd line of the example
 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
 * static info_lkp_t onbatt_info[] = {
 * 	{ 1, "OB", NULL, NULL },
 * 	{ 2, "OL", NULL, NULL },
 * 	{ 0, NULL, NULL, NULL }
 * };
 */

/* upsBaseBatteryStatus */
static info_lkp_t xpcc_onbatt_info[] = {
	{ 1, "", NULL, NULL },	/* unknown */
	{ 2, "", NULL, NULL },	/* batteryNormal */
	{ 3, "LB", NULL, NULL },	/* batteryLow */
	{ 0, NULL, NULL, NULL }
};

/*
upsBaseOutputStatus OBJECT-TYPE
	SYNTAX INTEGER {
			unknown(1),
			onLine(2),
			onBattery(3),
			onBoost(4),
			sleeping(5),
			onBypass(6),
			rebooting(7),
			standBy(8),
			onBuck(9) }
*/
static info_lkp_t xpcc_power_info[] = {
	{ 1, "", NULL, NULL },	/* unknown */
	{ 2, "OL", NULL, NULL },	/* onLine */
	{ 3, "OB", NULL, NULL },	/* onBattery */
	{ 4, "OL BOOST", NULL, NULL },	/* onBoost */
	{ 5, "OFF", NULL, NULL },	/* sleeping */
	{ 6, "BYPASS", NULL, NULL },	/* onBypass */
	{ 7, "", NULL, NULL },	/* rebooting */
	{ 8, "OFF", NULL, NULL },	/* standBy */
	{ 9, "OL TRIM", NULL, NULL },	/* onBuck */
	{ 0, NULL, NULL, NULL }
};

/* XPPC Snmp2NUT lookup table */
static snmp_info_t xppc_mib[] = {

	/* Data format:
	 * { info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar },
	 *
	 *	info_type:	NUT INFO_ or CMD_ element name
	 *	info_flags:	flags to set in addinfo
	 *	info_len:	length of strings if STR
	 *				cmd value if CMD, multiplier otherwise
	 *	OID: SNMP OID or NULL
	 *	dfl: default value
	 *	flags: snmp-ups internal flags (FIXME: ...)
	 *	oid2info: lookup table between OID and NUT values
	 *
	 * Example:
	 * { "input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL },
	 * { "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, onbatt_info },
	 *
	 * To create a value lookup structure (as needed on the 2nd line), use the
	 * following kind of declaration, outside of the present snmp_info_t[]:
	 * static info_lkp_t onbatt_info[] = {
	 * 	{ 1, "OB" },
	 * 	{ 2, "OL" },
	 * 	{ 0, NULL }
	 * };
	 */
        { "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Tripp Lite / Phoenixtec",
                SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },

	/* upsBaseIdentModel.0 = STRING: "Intelligent" */
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.935.1.1.1.1.1.1.0", "Generic Phoenixtec SNMP device", SU_FLAG_OK, NULL },
	/* upsBaseBatteryStatus.0 = INTEGER: batteryNormal(2) */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.935.1.1.1.2.1.1.0", "", SU_STATUS_BATT | SU_TYPE_INT | SU_FLAG_OK, xpcc_onbatt_info },
	/* upsSmartBatteryCapacity.0 = INTEGER: 100 */
	{ "battery.charge", 0, 1, ".1.3.6.1.4.1.935.1.1.1.2.2.1.0", NULL, SU_TYPE_INT | SU_FLAG_OK, NULL },
	/* upsSmartBatteryTemperature.0 = INTEGER: 260 */
	{ "ups.temperature", 0, 0.1, ".1.3.6.1.4.1.935.1.1.1.2.2.3.0", NULL, SU_TYPE_INT | SU_FLAG_OK, NULL },
	/* upsSmartInputLineVoltage.0 = INTEGER: 1998 */
	{ "input.voltage", 0, 0.1, ".1.3.6.1.4.1.935.1.1.1.3.2.1.0", NULL, SU_TYPE_INT | SU_FLAG_OK, NULL },
	/* upsBaseOutputStatus.0 = INTEGER: onLine(2) */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.935.1.1.1.4.1.1.0", "", SU_TYPE_INT | SU_STATUS_PWR, xpcc_power_info },
	/* upsSmartOutputVoltage.0 = INTEGER: 2309 */
	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.935.1.1.1.4.2.1.0", NULL, SU_TYPE_INT | SU_FLAG_OK, NULL },
	/* upsSmartOutputFrequency.0 = INTEGER: 500 */
	{ "output.frequency", 0, 0.1, ".1.3.6.1.4.1.935.1.1.1.4.2.2.0", NULL, SU_TYPE_INT | SU_FLAG_OK, NULL },
	/* upsSmartOutputLoad.0 = INTEGER: 7 */
	{ "ups.load", 0, 1, ".1.3.6.1.4.1.935.1.1.1.4.2.3.0", NULL, SU_TYPE_INT | SU_FLAG_OK, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	xppc = { "xppc", XPPC_MIB_VERSION, NULL, NULL, xppc_mib, XPPC_SYSOID, NULL };
