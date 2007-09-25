/*  ietfmib.h - data to monitor SNMP UPS (RFC 1628 compliant) with NUT
 *
 *  Copyright (C) 2002-2006 
 *  			Arnaud Quette <arnaud.quette@free.fr>
 *  			Niels Baggesen <niels@baggesen.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
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

#define IETF_MIB_VERSION	"1.3"

/* SNMP OIDs set */
#define IETF_OID_UPS_MIB          "1.3.6.1.2.1.33"
#define IETF_OID_MFR_NAME         "1.3.6.1.2.1.33.1.1.1.0"	/* UPS-MIB::upsIdentManufacturer.0 */
#define IETF_OID_MODEL_NAME       "1.3.6.1.2.1.33.1.1.2.0"	/* UPS-MIB::upsIdentModel.0 */
#define IETF_OID_FIRMREV          "1.3.6.1.2.1.33.1.1.3.0"	/* UPS-MIB::upsIdentUPSSoftwareVersion.0 */
#define IETF_OID_AGENTREV         "1.3.6.1.2.1.33.1.1.4.0"	/* UPS-MIB::upsIdentAgentSoftwareVersion.0 */
#define IETF_OID_IDENT            "1.3.6.1.2.1.33.1.1.5.0"	/* UPS-MIB::upsIdentName.0 */

#define IETF_OID_BATT_STATUS      "1.3.6.1.2.1.33.1.2.1.0"	/* UPS-MIB::upsBatteryStatus.0 */
#define IETF_OID_BATT_RUNTIME     "1.3.6.1.2.1.33.1.2.3.0"	/* UPS-MIB::upsEstimatedMinutesRemaining.0 */
#define IETF_OID_BATT_CHARGE      "1.3.6.1.2.1.33.1.2.4.0"	/* UPS-MIB::upsEstimatedChargeRemaining.0 */
#define IETF_OID_BATT_VOLTAGE     "1.3.6.1.2.1.33.1.2.5.0"	/* UPS-MIB::upsBatteryVoltage.0 */
#define IETF_OID_BATT_CURRENT     "1.3.6.1.2.1.33.1.2.6.0"	/* UPS-MIB::upsBatteryCurrent.0 */
#define IETF_OID_BATT_TEMP        "1.3.6.1.2.1.33.1.2.7.0"	/* UPS-MIB::upsBatteryTemperature.0 */

#define IETF_OID_IN_LINEBADS      "1.3.6.1.2.1.33.1.3.1.0"	/* UPS-MIB::upsInputLineBads.0 */
#define IETF_OID_IN_LINES         "1.3.6.1.2.1.33.1.3.2.0"	/* UPS-MIB::upsInputNumLines.0 */

#define IETF_OID_IN_FREQ          "1.3.6.1.2.1.33.1.3.3.1.2"	/* UPS-MIB::upsInputFrequency */
#define IETF_OID_IN_VOLTAGE       "1.3.6.1.2.1.33.1.3.3.1.3"	/* UPS-MIB::upsInputVoltage */
#define IETF_OID_IN_CURRENT       "1.3.6.1.2.1.33.1.3.3.1.4"	/* UPS-MIB::upsInputCurrent */
#define IETF_OID_IN_POWER         "1.3.6.1.2.1.33.1.3.3.1.5"	/* UPS-MIB::upsInputTruePower */

#define IETF_OID_POWER_STATUS     "1.3.6.1.2.1.33.1.4.1.0"	/* UPS-MIB::upsOutputSource.0 */
#define IETF_OID_OUT_FREQUENCY    "1.3.6.1.2.1.33.1.4.2.0"	/* UPS-MIB::upsOutputFrequency.0 */
#define IETF_OID_OUT_LINES        "1.3.6.1.2.1.33.1.4.3.0"	/* UPS-MIB::upsOutputNumLines.0 */

#define IETF_OID_OUT_VOLTAGE      "1.3.6.1.2.1.33.1.4.4.1.2"	/* UPS-MIB::upsOutputVoltage */
#define IETF_OID_OUT_CURRENT      "1.3.6.1.2.1.33.1.4.4.1.3"	/* UPS-MIB::upsOutputCurrent */
#define IETF_OID_OUT_POWER        "1.3.6.1.2.1.33.1.4.4.1.4"	/* UPS-MIB::upsOutputPower */
#define IETF_OID_LOAD_LEVEL       "1.3.6.1.2.1.33.1.4.4.1.5"	/* UPS-MIB::upsOutputPercentLoad */

#define IETF_OID_UPS_TEST_ID      "1.3.6.1.2.1.33.1.7.1"        /* UPS-MIB::upsTestID */
#define IETF_OID_UPS_TEST_RES     "1.3.6.1.2.1.33.1.7.3"        /* UPS-MIB::upsTestResultsSummary */
#define IETF_OID_UPS_TEST_RESDET  "1.3.6.1.2.1.33.1.7.4"        /* UPS-MIB::upsTestResultsDetail */
#define IETF_OID_UPS_TEST_NOTEST  "1.3.6.1.2.1.33.1.7.7.1"      /* UPS-MIB::upsTestNoTestInitiated */
#define IETF_OID_UPS_TEST_ABORT   "1.3.6.1.2.1.33.1.7.7.2"      /* UPS-MIB::upsTestAbortTestInProgress */
#define IETF_OID_UPS_TEST_GSTEST  "1.3.6.1.2.1.33.1.7.7.3"      /* UPS-MIB::upsTestGeneralSystemsTest */
#define IETF_OID_UPS_TEST_QBATT   "1.3.6.1.2.1.33.1.7.7.4"      /* UPS-MIB::upsTestQuickBatteryTest */
#define IETF_OID_UPS_TEST_DBATT   "1.3.6.1.2.1.33.1.7.7.5"      /* UPS-MIB::upsTestDeepBatteryCalibration */

#define IETF_OID_CONF_VOLTAGE     "1.3.6.1.2.1.33.1.9.3.0"      /* UPS-MIB::upsConfigOutputVoltage.0 */
#define IETF_OID_CONF_OUT_VA      "1.3.6.1.2.1.33.1.9.5.0"      /* UPS-MIB::upsConfigOutputVA.0 */
#define IETF_OID_CONF_RUNTIME_LOW "1.3.6.1.2.1.33.1.9.7.0"	/* UPS-MIB::upsConfigLowBattTime.0 */

/* Defines for IETF_OID_POWER_STATUS (1) */
info_lkp_t ietf_pwr_info[] = {
	{ 1, ""       /* other */ },
	{ 2, "OFF"    /* none */ },
	{ 3, "OL"     /* normal */ },
	{ 4, "BYPASS" /* bypass */ },
	{ 5, "OB"     /* battery */ },
	{ 6, "BOOST"  /* booster */ },
	{ 7, "TRIM"   /* reducer */ },
	{ 0, "NULL" }
} ;

/* Defines for IETF_OID_BATT_STATUS (2) */
info_lkp_t ietf_batt_info[] = {
	{ 1, ""   /* unknown */ },
	{ 2, ""   /* batteryNormal */},
	{ 3, "LB" /* batteryLow */ },
	{ 4, "LB" /* batteryDepleted */ },
	{ 0, "NULL" }
} ;

/* Defines for IETF_OID_TEST_RES */
info_lkp_t ietf_test_res_info[] = {
	{ 1, "Done and passed" },
	{ 2, "Done and warning" },
	{ 3, "Done and error" },
	{ 4, "Aborted" },
	{ 5, "In progress" },
	{ 6, "No test initiated" },
	{ 0, "NULL" }
} ;

#define IETF_OID_SD_AFTER_DELAY	 "1.3.6.1.2.1.33.1.8.2"	/* UPS-MIB::upsShutdownAfterDelay */
#define IETF_OFF_DO		0

#define IETF_OID_ALARM_OB "1.3.6.1.2.1.33.1.6.3.2"	/* UPS-MIB::upsAlarmOnBattery */

info_lkp_t ietf_alarm_ob[] = {
	{ 1, "OB" },
	{ 0, "NULL" }
} ;
		
#define IETF_OID_ALARM_LB "1.3.6.1.2.1.33.1.6.3.3"	/* UPS-MIB::upsAlarmLowBattery */

info_lkp_t ietf_alarm_lb[] = {
	{ 1, "LB" },
	{ 0, "NULL" }
} ;

/* Missing data
   CAL   - UPS is performing calibration
   OVER  - UPS is overloaded
   RB    - UPS battery needs to be replaced
   FSD   - UPS is in forced shutdown state (slaves take note)
*/

/* Snmp2NUT lookup table */

snmp_info_t ietf_mib[] = {
	/* UPS page */
	/* info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_MFR_NAME, "Generic",
		SU_FLAG_STATIC, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_MODEL_NAME, "Generic SNMP UPS",
		SU_FLAG_STATIC, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_FIRMREV, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_AGENTREV, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_IDENT, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.load", 0, 1.0, IETF_OID_LOAD_LEVEL ".0", "",
		SU_OUTPUT_1, NULL },
	{ "ups.power", 0, 1.0, IETF_OID_OUT_POWER ".0", "",
		0, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_POWER_STATUS, "OFF",
		SU_STATUS_PWR, &ietf_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_BATT_STATUS, "",
		SU_STATUS_BATT, &ietf_batt_info[0] },
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_TEST_RESDET, "",
		0, NULL },

	/* Battery page */
	{ "battery.charge", 0, 1.0, IETF_OID_BATT_CHARGE, "",
		0, NULL },
	{ "battery.runtime", 0, 60.0, IETF_OID_BATT_RUNTIME, "",
		0, NULL },
	{ "battery.runtime.low", ST_FLAG_RW, 1, IETF_OID_CONF_RUNTIME_LOW, "",
		0, NULL },
	{ "battery.voltage", 0, 0.1, IETF_OID_BATT_VOLTAGE, "",
		0, NULL },
	{ "battery.current", 0, 0.1, IETF_OID_BATT_CURRENT, "",
		0, NULL },
	{ "battery.temperature", 0, 1.0, IETF_OID_BATT_TEMP, "",
		0, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, IETF_OID_OUT_LINES, "",
		SU_FLAG_SETINT, NULL, &output_phases },
	{ "output.frequency", 0, 0.1, IETF_OID_OUT_FREQUENCY, "",
		0, NULL },
	{ "output.voltage", 0, 1.0, IETF_OID_OUT_VOLTAGE ".0", "",
		SU_OUTPUT_1, NULL },
	{ "output.current", 0, 0.1, IETF_OID_OUT_CURRENT ".0", "",
		SU_OUTPUT_1, NULL },
	{ "output.realpower", 0, 1.0, IETF_OID_OUT_POWER ".0", "",
		SU_OUTPUT_1, NULL },
	{ "output.L1-N.voltage", 0, 1.0, IETF_OID_OUT_VOLTAGE ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 1.0, IETF_OID_OUT_VOLTAGE ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 1.0, IETF_OID_OUT_VOLTAGE ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.current", 0, 0.1, IETF_OID_OUT_CURRENT ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 0.1, IETF_OID_OUT_CURRENT ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 0.1, IETF_OID_OUT_CURRENT ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.realpower", 0, 0.1, IETF_OID_OUT_POWER ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.realpower", 0, 0.1, IETF_OID_OUT_POWER ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.realpower", 0, 0.1, IETF_OID_OUT_POWER ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".3", "",
		SU_OUTPUT_3, NULL },

	/* Input page */
	{ "input.phases", 0, 1.0, IETF_OID_IN_LINES, "",
		SU_FLAG_SETINT, NULL, &input_phases },
	{ "input.frequency", 0, 0.1, IETF_OID_IN_FREQ ".0", "",
		SU_INPUT_1, NULL },
	{ "input.voltage", 0, 1.0, IETF_OID_IN_VOLTAGE ".0", "",
		SU_INPUT_1, NULL },
	{ "input.current", 0, 0.1, IETF_OID_IN_CURRENT ".0", "",
		SU_INPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 1.0, IETF_OID_IN_VOLTAGE ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 1.0, IETF_OID_IN_VOLTAGE ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 1.0, IETF_OID_IN_VOLTAGE ".3", "",
		SU_INPUT_3, NULL },
	{ "input.L1.current", 0, 0.1, IETF_OID_IN_CURRENT ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 0.1, IETF_OID_IN_CURRENT ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 0.1, IETF_OID_IN_CURRENT ".3", "",
		SU_INPUT_3, NULL },
	{ "input.L1.realpower", 0, 0.1, IETF_OID_IN_POWER ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2.realpower", 0, 0.1, IETF_OID_IN_POWER ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3.realpower", 0, 0.1, IETF_OID_IN_POWER ".3", "",
		SU_INPUT_3, NULL },
        { "input.quality", 0, 1.0, IETF_OID_IN_LINEBADS, "",
               0, NULL },

	/* instant commands. */
	{ "load.off", 0, IETF_OFF_DO, IETF_OID_SD_AFTER_DELAY, "",
		SU_TYPE_CMD, NULL },
	/* write the OID of the battery test into the test initiator OID */
	{ "test.battery.start.quick", 0, SU_INFOSIZE, IETF_OID_UPS_TEST_ID, IETF_OID_UPS_TEST_QBATT,
		SU_TYPE_CMD, NULL },
	 /* write the OID of the battery test into the test initiator OID */
	{ "test.battery.start.deep", 0, SU_INFOSIZE, IETF_OID_UPS_TEST_ID, IETF_OID_UPS_TEST_DBATT,
		SU_TYPE_CMD, NULL },
/*	{ CMD_SHUTDOWN, 0, IETF_OFF_GRACEFUL, IETF_OID_OFF, "", 0, NULL }, */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};
