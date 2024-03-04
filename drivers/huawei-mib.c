/* huawei-mib.c - subdriver to monitor Huawei SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *  2015	Stuart Henderson <stu@spacehopper.org>
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

#include "huawei-mib.h"

#define HUAWEI_MIB_VERSION  "0.40"

#define HUAWEI_SYSOID       ".1.3.6.1.4.1.8072.3.2.10"
#define HUAWEI_UPSMIB       ".1.3.6.1.4.1.2011"
#define HUAWEI_OID_MODEL_NAME ".1.3.6.1.4.1.2011.6.174.1.2.100.1.2.1"

/* To create a value lookup structure (as needed on the 2nd line of the example
 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
 * static info_lkp_t huawei_onbatt_info[] = {
 * 	info_lkp_default(1, "OB"),
 * 	info_lkp_default(2, "OL"),
 * 	info_lkp_sentinel
 * };
 */

static info_lkp_t huawei_supplymethod_info[] = {
	info_lkp_default(1, ""),	/* no supply */
	info_lkp_default(2, "OL BYPASS"),
	info_lkp_default(3, "OL"),
	info_lkp_default(4, "OB"),
	info_lkp_default(5, ""),	/* combined */
	info_lkp_default(6, "OL ECO"),
	info_lkp_default(7, "OB ECO"),
	info_lkp_sentinel
};

static info_lkp_t huawei_battstate_info[] = {
	info_lkp_default(1, ""),	/* not connected */
	info_lkp_default(2, ""),	/* not charging or discharging */
	info_lkp_default(3, ""),	/* hibernation */
	info_lkp_default(4, ""),	/* float */
	info_lkp_default(5, "CHRG"),	/* equalized charging */
	info_lkp_default(6, "DISCHRG"),
	info_lkp_sentinel
};

static info_lkp_t huawei_phase_info[] = {
	info_lkp_default(1, "1"),
	info_lkp_default(2, "3"),
	info_lkp_sentinel
};

static info_lkp_t huawei_voltrating_info[] = {
	info_lkp_default(1, "200"),
	info_lkp_default(2, "208"),
	info_lkp_default(3, "220"),
	info_lkp_default(4, "380"),
	info_lkp_default(5, "400"),
	info_lkp_default(6, "415"),
	info_lkp_default(7, "480"),
	info_lkp_default(8, "600"),
	info_lkp_default(9, "690"),
	info_lkp_sentinel
};

static info_lkp_t huawei_freqrating_info[] = {
	info_lkp_default(1, "50"),
	info_lkp_default(2, "60"),
	info_lkp_sentinel
};

static info_lkp_t huawei_pwrrating_info[] = {
	info_lkp_default(1, "80000"),
	info_lkp_default(2, "100000"),
	info_lkp_default(3, "120000"),
	info_lkp_default(4, "160000"),
	info_lkp_default(5, "200000"),
	info_lkp_default(6, "30000"),
	info_lkp_default(7, "40000"),
	info_lkp_default(8, "60000"),
	info_lkp_default(9, "2400000"),
	info_lkp_default(10, "2500000"),
	info_lkp_default(11, "2800000"),
	info_lkp_default(12, "3000000"),
	info_lkp_sentinel
};

/* Note: This is currently identical to ietf_test_result_info from IETF MIB
 * We rename it here to a) allow evolution that may become incompatible;
 * b) avoid namespace conflicts, especially with DMF loader of named objects
 */
static info_lkp_t huawei_test_result_info[] = {
	info_lkp_default(1, "done and passed"),
	info_lkp_default(2, "done and warning"),
	info_lkp_default(3, "done and error"),
	info_lkp_default(4, "aborted"),
	info_lkp_default(5, "in progress"),
	info_lkp_default(6, "no test initiated"),
	info_lkp_sentinel
};


/* HUAWEI Snmp2NUT lookup table */
static snmp_info_t huawei_mib[] = {

	/* Data format:
	 * snmp_info_default(info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar),
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
	 * snmp_info_default("input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL),
	 * snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, huawei_onbatt_info),
	 *
	 * To create a value lookup structure (as needed on the 2nd line), use the
	 * following kind of declaration, outside of the present snmp_info_t[]:
	 * static info_lkp_t huawei_onbatt_info[] = {
	 * 	info_lkp_default(1, "OB"),
	 * 	info_lkp_default(2, "OL"),
	 * 	info_lkp_sentinel
	 * };
	 */

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* UPS page */

	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Huawei", SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.2.1", "Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.1.1.2.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	snmp_info_default("ups.time", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.11.1.0", NULL, SU_FLAG_OK, NULL), /* seconds since epoch */

	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.3.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.5.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	snmp_info_default("ups.status", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.2.101.1.1.1", NULL, SU_FLAG_OK, huawei_supplymethod_info),
	snmp_info_default("ups.status", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.2.101.1.3.1", NULL, SU_STATUS_BATT | SU_FLAG_OK, huawei_battstate_info),

	snmp_info_default("ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.33.1.7.3.0", "", 0, huawei_test_result_info),


	/* Input page */

	/* hwUpsCtrlInputStandard listed in MIB but not present on tested UPS5000-E */
	snmp_info_default("input.phases", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.102.100.1.8", "3", SU_FLAG_ABSENT | SU_FLAG_OK, huawei_phase_info),

	snmp_info_default("input.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.1.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.2.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.3.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("input.frequency", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.4.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("input.L1.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.5.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.L2.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.6.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.L3.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.7.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("input.L1.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.8.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.L2.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.9.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.L3.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.10.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("input.bypass.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.1.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.bypass.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.2.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.bypass.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.3.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("input.bypass.frequency", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.4.1", NULL, SU_FLAG_OK, NULL),


	/* Output page */

	/* hwUpsCtrlOutputStandard listed in MIB but not present on tested UPS5000-E */
	snmp_info_default("output.phases", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.102.100.1.9", "3", SU_FLAG_ABSENT | SU_FLAG_OK, huawei_phase_info),

	snmp_info_default("output.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.1.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.2.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.3.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("output.L1.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.4.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L2.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.5.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L3.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.6.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("output.frequency", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.7.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("output.L1.realpower", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.8.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L1.realpower", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.9.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L1.realpower", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.10.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("output.L1.power", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.11.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L2.power", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.12.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L3.power", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.13.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("output.L1.power.percent", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.14.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L2.power.percent", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.15.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L3.power.percent", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.16.1", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("output.voltage.nominal", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.17.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, huawei_voltrating_info),
	snmp_info_default("output.frequency.nominal", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.18.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, huawei_freqrating_info),
	snmp_info_default("output.power.nominal", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.6.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, huawei_pwrrating_info),

	snmp_info_default("output.L1.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.19.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L2.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.20.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.L2.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.21.1", NULL, SU_FLAG_OK, NULL),


	/* Battery page */

	snmp_info_default("battery.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.1.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("battery.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.2.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("battery.charge", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.3.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("battery.runtime", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.4.1", NULL, SU_FLAG_OK, NULL),

#if WITH_UNMAPPED_DATA_POINTS
	snmp_info_default("unmapped.hwUpsBattTest", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.103.101.1.6.1", NULL, SU_FLAG_OK, NULL),
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	huawei = { "huawei", HUAWEI_MIB_VERSION, NULL, HUAWEI_OID_MODEL_NAME, huawei_mib, HUAWEI_SYSOID, NULL };
