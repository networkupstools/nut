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

#define HUAWEI_MIB_VERSION  "0.1"

#define HUAWEI_SYSOID       ".1.3.6.1.4.1.8072.3.2.10"
#define HUAWEI_UPSMIB       ".1.3.6.1.4.1.2011"

/* To create a value lookup structure (as needed on the 2nd line of the example
 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
 * static info_lkp_t onbatt_info[] = {
 * 	{ 1, "OB" },
 * 	{ 2, "OL" },
 * 	{ 0, NULL }
 * };
 */

static info_lkp_t huawei_supplymethod_info[] = {
	{ 1, "" },		/* no supply */
	{ 2, "OL BYPASS" },
	{ 3, "OL" },
	{ 4, "OB" },
	{ 5, "" },		/* combined */
	{ 6, "OL ECO" },
	{ 7, "OB ECO" },
	{ 0, NULL }
};

static info_lkp_t huawei_battstate_info[] = {
	{ 1, "" },		/* not connected */
	{ 2, "" },		/* not charging or discharging */
	{ 3, "" },		/* hibernation */
	{ 4, "" },		/* float */
	{ 5, "CHRG" },		/* equalized charging */
	{ 6, "DISCHRG" },
	{ 0, NULL }
};

static info_lkp_t huawei_phase_info[] = {
	{ 1, "1" },
	{ 2, "3" },
	{ 0, NULL }
};

static info_lkp_t huawei_voltrating_info[] = {
	{ 1, "200" },
	{ 2, "208" },
	{ 3, "220" },
	{ 4, "380" },
	{ 5, "400" },
	{ 6, "415" },
	{ 7, "480" },
	{ 8, "600" },
	{ 9, "690" },
	{ 0, NULL }
};

static info_lkp_t huawei_freqrating_info[] = {
	{ 1, "50" },
	{ 2, "60" },
	{ 0, NULL }
};

static info_lkp_t huawei_pwrrating_info[] = {
	{ 1, "80000" },
	{ 2, "100000" },
	{ 3, "120000" },
	{ 4, "160000" },
	{ 5, "200000" },
	{ 6, "30000" },
	{ 7, "40000" },
	{ 8, "60000" },
	{ 9, "2400000" },
	{ 10, "2500000" },
	{ 11, "2800000" },
	{ 12, "3000000" },
	{ 0, NULL }
};

/* Note: This is currently identical to ietf_test_result_info from IETF MIB
 * We rename it here to a) allow evolution that may become incompatible;
 * b) avoid namespace conflicts, especially with DMF loader of named objects
 */
static info_lkp_t huawei_test_result_info[] = {
	{ 1, "done and passed" },
	{ 2, "done and warning" },
	{ 3, "done and error" },
	{ 4, "aborted" },
	{ 5, "in progress" },
	{ 6, "no test initiated" },
	{ 0, NULL }
};


/* HUAWEI Snmp2NUT lookup table */
static snmp_info_t huawei_mib[] = {

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
	 *	setvar: variable to set for SU_FLAG_SETINT
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

	/* UPS page */

	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Huawei", SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.2.1", "Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.1.1.2.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	{ "ups.time", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.11.1.0", NULL, SU_FLAG_OK, NULL }, /* seconds since epoch */

	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.3.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.5.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	{ "ups.status", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.2.101.1.1.1", NULL, SU_FLAG_OK, huawei_supplymethod_info },
	{ "ups.status", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.2.101.1.3.1", NULL, SU_STATUS_BATT | SU_FLAG_OK, huawei_battstate_info },

	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.33.1.7.3.0", "", 0, huawei_test_result_info },


	/* Input page */

	/* hwUpsCtrlInputStandard listed in MIB but not present on tested UPS5000-E */
	{ "input.phases", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.102.100.1.8", "3", SU_FLAG_ABSENT | SU_FLAG_OK, huawei_phase_info },

	{ "input.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.1.1", NULL, SU_FLAG_OK, NULL },
	{ "input.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.2.1", NULL, SU_FLAG_OK, NULL },
	{ "input.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.3.1", NULL, SU_FLAG_OK, NULL },

	{ "input.frequency", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.4.1", NULL, SU_FLAG_OK, NULL },

	{ "input.L1.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.5.1", NULL, SU_FLAG_OK, NULL },
	{ "input.L2.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.6.1", NULL, SU_FLAG_OK, NULL },
	{ "input.L3.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.7.1", NULL, SU_FLAG_OK, NULL },

	{ "input.L1.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.8.1", NULL, SU_FLAG_OK, NULL },
	{ "input.L2.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.9.1", NULL, SU_FLAG_OK, NULL },
	{ "input.L3.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.3.100.1.10.1", NULL, SU_FLAG_OK, NULL },

	{ "input.bypass.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.1.1", NULL, SU_FLAG_OK, NULL },
	{ "input.bypass.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.2.1", NULL, SU_FLAG_OK, NULL },
	{ "input.bypass.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.3.1", NULL, SU_FLAG_OK, NULL },

	{ "input.bypass.frequency", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.5.100.1.4.1", NULL, SU_FLAG_OK, NULL },


	/* Output page */

	/* hwUpsCtrlOutputStandard listed in MIB but not present on tested UPS5000-E */
	{ "output.phases", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.102.100.1.9", "3", SU_FLAG_ABSENT | SU_FLAG_OK, huawei_phase_info },

	{ "output.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.1.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.2.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.3.1", NULL, SU_FLAG_OK, NULL },

	{ "output.L1.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.4.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L2.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.5.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L3.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.6.1", NULL, SU_FLAG_OK, NULL },

	{ "output.frequency", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.7.1", NULL, SU_FLAG_OK, NULL },

	{ "output.L1.realpower", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.8.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L1.realpower", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.9.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L1.realpower", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.10.1", NULL, SU_FLAG_OK, NULL },

	{ "output.L1.power", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.11.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L2.power", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.12.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L3.power", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.13.1", NULL, SU_FLAG_OK, NULL },

	{ "output.L1.power.percent", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.14.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L2.power.percent", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.15.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L3.power.percent", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.16.1", NULL, SU_FLAG_OK, NULL },

	{ "output.voltage.nominal", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.17.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, huawei_voltrating_info },
	{ "output.frequency.nominal", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.18.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, huawei_freqrating_info },
	{ "output.power.nominal", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2011.6.174.1.2.100.1.6.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, huawei_pwrrating_info },

	{ "output.L1.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.19.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L2.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.20.1", NULL, SU_FLAG_OK, NULL },
	{ "output.L2.powerfactor", 0, 0.01, ".1.3.6.1.4.1.2011.6.174.1.4.100.1.21.1", NULL, SU_FLAG_OK, NULL },


	/* Battery page */

	{ "battery.voltage", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.1.1", NULL, SU_FLAG_OK, NULL },
	{ "battery.current", 0, 0.1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.2.1", NULL, SU_FLAG_OK, NULL },
	{ "battery.charge", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.3.1", NULL, SU_FLAG_OK, NULL },
	{ "battery.runtime", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.6.100.1.4.1", NULL, SU_FLAG_OK, NULL },


	/* { "unmapped.hwUpsBattTest", 0, 1, ".1.3.6.1.4.1.2011.6.174.1.103.101.1.6.1", NULL, SU_FLAG_OK, NULL }, */


	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	huawei = { "huawei", HUAWEI_MIB_VERSION, NULL, NULL, huawei_mib, HUAWEI_SYSOID };
