/*  ietf-mib.c - data to monitor SNMP UPS (RFC 1628 compliant) with NUT
 *
 *  Copyright (C) 2002-2006
 *	2002-2012	Arnaud Quette <arnaud.quette@free.fr>
 *	2002-2006	Niels Baggesen <niels@baggesen.net>
 *	2002-2006	Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#include "ietf-mib.h"

#define IETF_MIB_VERSION	"1.52"

/* SNMP OIDs set */
#define IETF_OID_UPS_MIB	"1.3.6.1.2.1.33.1."
#define IETF_SYSOID			".1.3.6.1.2.1.33"

/* NOTE: Currently the Tripplite UPSes await user-validation of their
 * real SNMP OID tree, so temporarily the IETF tree is used as "tripplite"
 * for the mapping purposes; the devices ave their entry point OID though.
 * For more details see:
 *    https://github.com/networkupstools/nut/issues/309
 *    https://github.com/networkupstools/nut/issues/171
 * Also related to:
 *    https://github.com/networkupstools/nut/issues/270
 */
#define TRIPPLITE_SYSOID	".1.3.6.1.4.1.850.1"

/* #define DEBUG */

static info_lkp_t ietf_battery_info[] = {
	{ 1, ""   /* unknown */, NULL, NULL },
	{ 2, ""   /* batteryNormal */, NULL, NULL },
	{ 3, "LB" /* batteryLow */, NULL, NULL },
	{ 4, "LB" /* batteryDepleted */, NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ietf_power_source_info[] = {
	{ 1, "" /* other */, NULL, NULL },
	{ 2, "OFF" /* none */, NULL, NULL },
	{ 3, "OL" /* normal */, NULL, NULL },
	{ 4, "OL BYPASS" /* bypass */, NULL, NULL },
	{ 5, "OB" /* battery */, NULL, NULL },
	{ 6, "OL BOOST" /* booster */, NULL, NULL },
	{ 7, "OL TRIM" /* reducer */, NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ietf_overload_info[] = {
	{ 1, "OVER", NULL, NULL },	/* output overload */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ietf_test_active_info[] = {
	{ 1, "", NULL, NULL },	/* upsTestNoTestsInitiated */
	{ 2, "", NULL, NULL },	/* upsTestAbortTestInProgress */
	{ 3, "TEST", NULL, NULL },	/* upsTestGeneralSystemsTest */
	{ 4, "TEST", NULL, NULL },	/* upsTestQuickBatteryTest */
	{ 5, "CAL", NULL, NULL },	/* upsTestDeepBatteryCalibration */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ietf_test_result_info[] = {
	{ 1, "done and passed", NULL, NULL },
	{ 2, "done and warning", NULL, NULL },
	{ 3, "done and error", NULL, NULL },
	{ 4, "aborted", NULL, NULL },
	{ 5, "in progress", NULL, NULL },
	{ 6, "no test initiated", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

#ifdef DEBUG
static info_lkp_t ietf_shutdown_type_info[] = {
	{ 1, "output", NULL, NULL },
	{ 2, "system", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
#endif

static info_lkp_t ietf_yes_no_info[] = {
	{ 1, "yes", NULL, NULL },
	{ 2, "no", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ietf_beeper_status_info[] = {
	{ 1, "disabled", NULL, NULL },
	{ 2, "enabled", NULL, NULL },
	{ 3, "muted", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
static snmp_info_t ietf_mib[] = {
	/* The Device Identification group */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.1.0", "Generic", SU_FLAG_STATIC, NULL }, /* upsIdentManufacturer */
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.2.0", "Generic SNMP UPS", SU_FLAG_STATIC, NULL }, /* upsIdentModel */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.3.0", "", SU_FLAG_STATIC, NULL }, /* upsIdentUPSSoftwareVersion */
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.4.0", "", SU_FLAG_STATIC, NULL }, /* upsIdentAgentSoftwareVersion */
#ifdef DEBUG
	{ "debug.upsIdentName", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.5.0", "", 0, NULL }, /* upsIdentName */
	{ "debug.upsIdentAttachedDevices", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.6.0", "", 0, NULL }, /* upsIdentAttachedDevices */
#endif
	/* Battery Group */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "2.1.0", "", SU_STATUS_BATT, ietf_battery_info }, /* upsBatteryStatus */
#ifdef DEBUG
	{ "debug.upsSecondsOnBattery", 0, 1.0, IETF_OID_UPS_MIB "2.2.0", "", 0, NULL }, /* upsSecondsOnBattery */
#endif
	{ "battery.runtime", 0, 60.0, IETF_OID_UPS_MIB "2.3.0", "", 0, NULL }, /* upsEstimatedMinutesRemaining */
	{ "battery.charge", 0, 1, IETF_OID_UPS_MIB "2.4.0", "", 0, NULL }, /* upsEstimatedChargeRemaining */
	{ "battery.voltage", 0, 0.1, IETF_OID_UPS_MIB "2.5.0", "", 0, NULL }, /* upsBatteryVoltage */
	{ "battery.current", 0, 0.1, IETF_OID_UPS_MIB "2.6.0", "", 0, NULL }, /* upsBatteryCurrent */
	{ "battery.temperature", 0, 1.0, IETF_OID_UPS_MIB "2.7.0", "", 0, NULL }, /* upsBatteryTemperature */

	/* Input Group */
#ifdef DEBUG
	{ "debug.upsInputLineBads", 0, 1.0, IETF_OID_UPS_MIB "3.1.0", "", 0, NULL }, /* upsInputLineBads */
#endif
	{ "input.phases", 0, 1.0, IETF_OID_UPS_MIB "3.2.0", "", 0, NULL }, /* upsInputNumLines */
#ifdef DEBUG
	{ "debug.upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.1", "", SU_INPUT_1, NULL }, /* upsInputLineIndex */
	{ "debug.[1].upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.1", "", SU_INPUT_3, NULL },
	{ "debug.[2].upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.2", "", SU_INPUT_3, NULL },
	{ "debug.[3].upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.3", "", SU_INPUT_3, NULL },
#endif
	{ "input.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.1", "", SU_INPUT_1, NULL }, /* upsInputFrequency */
	{ "input.L1.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.1", "", SU_INPUT_3, NULL },
	{ "input.L2.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.2", "", SU_INPUT_3, NULL },
	{ "input.L3.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.3", "", SU_INPUT_3, NULL },
	{ "input.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.1", "", SU_INPUT_1, NULL }, /* upsInputVoltage */
	{ "input.L1-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.3", "", SU_INPUT_3, NULL },
	{ "input.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.1", "", SU_INPUT_1, NULL }, /* upsInputCurrent */
	{ "input.L1.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.1", "", SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.2", "", SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.3", "", SU_INPUT_3, NULL },
	{ "input.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.1", "", SU_INPUT_1, NULL }, /* upsInputTruePower */
	{ "input.L1.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.1", "", SU_INPUT_3, NULL },
	{ "input.L2.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.2", "", SU_INPUT_3, NULL },
	{ "input.L3.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.3", "", SU_INPUT_3, NULL },

	/* Output Group */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "4.1.0", "", SU_STATUS_PWR, ietf_power_source_info }, /* upsOutputSource */
	{ "output.frequency", 0, 0.1, IETF_OID_UPS_MIB "4.2.0", "", 0, NULL }, /* upsOutputFrequency */
	{ "output.phases", 0, 1.0, IETF_OID_UPS_MIB "4.3.0", "", 0, NULL }, /* upsOutputNumLines */
#ifdef DEBUG
	{ "debug.upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.1", "", SU_OUTPUT_1, NULL }, /* upsOutputLineIndex */
	{ "debug.[1].upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.1", "", SU_OUTPUT_3, NULL },
	{ "debug.[2].upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.2", "", SU_OUTPUT_3, NULL },
	{ "debug.[3].upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.3", "", SU_OUTPUT_3, NULL },
#endif
	{ "output.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.1", "", SU_OUTPUT_1, NULL }, /* upsOutputVoltage */
	{ "output.L1-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.3", "", SU_OUTPUT_3, NULL },
	{ "output.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.1", "", SU_OUTPUT_1, NULL }, /* upsOutputCurrent */
	{ "output.L1.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.3", "", SU_OUTPUT_3, NULL },
	{ "output.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.1", "", SU_OUTPUT_1, NULL }, /* upsOutputPower */
	{ "output.L1.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.3", "", SU_OUTPUT_3, NULL },
	{ "ups.load", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.1", "", SU_OUTPUT_1, NULL }, /* upsOutputPercentLoad */
	{ "output.L1.power.percent", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.power.percent", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.power.percent", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.3", "", SU_OUTPUT_3, NULL },

	/* Bypass Group */
	{ "input.bypass.phases", 0, 1.0, IETF_OID_UPS_MIB "5.2.0", "", 0, NULL }, /* upsBypassNumLines */
	{ "input.bypass.frequency", 0, 0.1, IETF_OID_UPS_MIB "5.1.0", "", SU_BYPASS_1 | SU_BYPASS_3, NULL }, /* upsBypassFrequency */
#ifdef DEBUG
	{ "debug.upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.1", "", SU_BYPASS_1, NULL }, /* upsBypassLineIndex */
	{ "debug.[1].upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.1", "", SU_BYPASS_3, NULL },
	{ "debug.[2].upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.2", "", SU_BYPASS_3, NULL },
	{ "debug.[3].upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.3", "", SU_BYPASS_3, NULL },
#endif
	{ "input.bypass.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.1", "", SU_BYPASS_1, NULL }, /* upsBypassVoltage */
	{ "input.bypass.L1-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.1", "", SU_BYPASS_3, NULL },
	{ "input.bypass.L2-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.2", "", SU_BYPASS_3, NULL },
	{ "input.bypass.L3-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.3", "", SU_BYPASS_3, NULL },
	{ "input.bypass.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.1", "", SU_BYPASS_1, NULL }, /* upsBypassCurrent */
	{ "input.bypass.L1.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.1", "", SU_BYPASS_3, NULL },
	{ "input.bypass.L2.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.2", "", SU_BYPASS_3, NULL },
	{ "input.bypass.L3.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.3", "", SU_BYPASS_3, NULL },
	{ "input.bypass.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.1", "", SU_BYPASS_1, NULL }, /* upsBypassPower */
	{ "input.bypass.L1.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.1", "", SU_BYPASS_3, NULL },
	{ "input.bypass.L2.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.2", "", SU_BYPASS_3, NULL },
	{ "input.bypass.L3.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.3", "", SU_BYPASS_3, NULL },

	/* Alarm Group */
#ifdef DEBUG
	{ "debug.upsAlarmsPresent", 0, 1.0, IETF_OID_UPS_MIB "6.1.0", "", 0, NULL }, /* upsAlarmsPresent */
	{ "debug.upsAlarmBatteryBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.1", "", 0, NULL }, /* upsAlarmBatteryBad */
	{ "debug.upsAlarmOnBattery", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.2", "", 0, NULL }, /* upsAlarmOnBattery */
	{ "debug.upsAlarmLowBattery", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.3", "", 0, NULL }, /* upsAlarmLowBattery */
	{ "debug.upsAlarmDepletedBattery", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.4", "", 0, NULL }, /* upsAlarmDepletedBattery */
	{ "debug.upsAlarmTempBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.5", "", 0, NULL }, /* upsAlarmTempBad */
	{ "debug.upsAlarmInputBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.6", "", 0, NULL }, /* upsAlarmInputBad */
	{ "debug.upsAlarmOutputBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.7", "", 0, NULL }, /* upsAlarmOutputBad */
#endif
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.8", "", 0, ietf_overload_info }, /* upsAlarmOutputOverload */
#ifdef DEBUG
	{ "debug.upsAlarmOnBypass", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.9", "", 0, NULL }, /* upsAlarmOnBypass */
	{ "debug.upsAlarmBypassBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.10", "", 0, NULL }, /* upsAlarmBypassBad */
	{ "debug.upsAlarmOutputOffAsRequested", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.11", "", 0, NULL }, /* upsAlarmOutputOffAsRequested */
	{ "debug.upsAlarmUpsOffAsRequested", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.12", "", 0, NULL }, /* upsAlarmUpsOffAsRequested */
	{ "debug.upsAlarmChargerFailed", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.13", "", 0, NULL }, /* upsAlarmChargerFailed */
	{ "debug.upsAlarmUpsOutputOff", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.14", "", 0, NULL }, /* upsAlarmUpsOutputOff */
	{ "debug.upsAlarmUpsSystemOff", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.15", "", 0, NULL }, /* upsAlarmUpsSystemOff */
	{ "debug.upsAlarmFanFailure", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.16", "", 0, NULL }, /* upsAlarmFanFailure */
	{ "debug.upsAlarmFuseFailure", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.17", "", 0, NULL }, /* upsAlarmFuseFailure */
	{ "debug.upsAlarmGeneralFault", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.18", "", 0, NULL }, /* upsAlarmGeneralFault */
	{ "debug.upsAlarmDiagnosticTestFailed", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.19", "", 0, NULL }, /* upsAlarmDiagnosticTestFailed */
	{ "debug.upsAlarmCommunicationsLost", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.20", "", 0, NULL }, /* upsAlarmCommunicationsLost */
	{ "debug.upsAlarmAwaitingPower", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.21", "", 0, NULL }, /* upsAlarmAwaitingPower */
	{ "debug.upsAlarmShutdownPending", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.22", "", 0, NULL }, /* upsAlarmShutdownPending */
	{ "debug.upsAlarmShutdownImminent", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.23", "", 0, NULL }, /* upsAlarmShutdownImminent */
	{ "debug.upsAlarmTestInProgress", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.24", "", 0, NULL }, /* upsAlarmTestInProgress */
#endif

	/* Test Group */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.1.0", "", 0, ietf_test_active_info }, /* upsTestId */
	{ "test.battery.stop", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL }, /* upsTestAbortTestInProgress */
	{ "test.battery.start", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL }, /* upsTestGeneralSystemsTest */
	{ "test.battery.start.quick", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL }, /* upsTestQuickBatteryTest */
	{ "test.battery.start.deep", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL }, /* upsTestDeepBatteryCalibration */
#ifdef DEBUG
	{ "debug.upsTestSpinLock", 0, 1.0, IETF_OID_UPS_MIB "7.2.0", "", 0, NULL }, /* upsTestSpinLock */
#endif
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.3.0", "", 0, ietf_test_result_info }, /* upsTestResultsSummary */
#ifdef DEBUG
	{ "debug.upsTestResultsDetail", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.4.0", "", 0, NULL }, /* upsTestResultsDetail */
	{ "debug.upsTestStartTime", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.5.0", "", 0, NULL }, /* upsTestStartTime */
	{ "debug.upsTestElapsedTime", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.6.0", "", 0, NULL }, /* upsTestElapsedTime */
#endif

	/* Control Group */
#ifdef DEBUG
	{ "debug.upsShutdownType", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "8.1.0", "", 0, ietf_shutdown_type_info }, /* upsShutdownType */
#endif
	{ "ups.timer.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 8, IETF_OID_UPS_MIB "8.2.0", "", 0, NULL }, /* upsShutdownAfterDelay*/
	{ "load.off", 0, 1, IETF_OID_UPS_MIB "8.2.0", "0", SU_TYPE_CMD, NULL },
	{ "ups.timer.start", ST_FLAG_STRING | ST_FLAG_RW, 8, IETF_OID_UPS_MIB "8.3.0", "", 0, NULL }, /* upsStartupAfterDelay */
	{ "load.on", 0, 1, IETF_OID_UPS_MIB "8.3.0", "0", SU_TYPE_CMD, NULL },
	{ "ups.timer.reboot", ST_FLAG_STRING | ST_FLAG_RW, 8, IETF_OID_UPS_MIB "8.4.0", "", 0, NULL }, /* upsRebootWithDuration */
	{ "ups.start.auto", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "8.5.0", "", 0, ietf_yes_no_info }, /* upsAutoRestart */

	/* Configuration Group */
	{ "input.voltage.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.1.0", "", 0, NULL }, /* upsConfigInputVoltage */
	{ "input.frequency.nominal", 0, 0.1, IETF_OID_UPS_MIB "9.2.0", "", 0, NULL }, /* upsConfigInputFreq */
	{ "output.voltage.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.3.0", "", 0, NULL }, /* upsConfigOutputVoltage */
	{ "output.frequency.nominal", 0, 0.1, IETF_OID_UPS_MIB "9.4.0", "", 0, NULL }, /* upsConfigOutputFreq */
	{ "output.power.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.5.0", "", 0, NULL }, /* upsConfigOutputVA */
	{ "output.realpower.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.6.0", "", 0, NULL }, /* upsConfigOutputPower */
	{ "battery.runtime.low", 0, 60.0, IETF_OID_UPS_MIB "9.7.0", "", 0, NULL }, /* upsConfigLowBattTime */
	{ "ups.beeper.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "9.8.0", "", 0, ietf_beeper_status_info }, /* upsConfigAudibleStatus */
	{ "beeper.disable", 0, 1, IETF_OID_UPS_MIB "9.8.0", "1", SU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 1, IETF_OID_UPS_MIB "9.8.0", "2", SU_TYPE_CMD, NULL },
	{ "beeper.mute", 0, 1, IETF_OID_UPS_MIB "9.8.0", "3", SU_TYPE_CMD, NULL },
	{ "input.transfer.low", 0, 1.0, IETF_OID_UPS_MIB "9.9.0", "", 0, NULL }, /* upsConfigLowVoltageTransferPoint */
	{ "input.transfer.high", 0, 1.0, IETF_OID_UPS_MIB "9.10.0", "", 0, NULL }, /* upsConfigHighVoltageTransferPoint */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

/* FIXME: Rename the structure here (or even relocate to new file)
 * and in snmp-ups.c when the real TrippLite mappings get defined. */
/* FIXME: Duplicate the line below to fix an issue with the code generator (nut-snmpinfo.py -> line is discarding) */
/*mib2nut_info_t	tripplite_ietf = { "tripplite", IETF_MIB_VERSION, NULL, NULL, ietf_mib, TRIPPLITE_SYSOID, NULL };*/
mib2nut_info_t	tripplite_ietf = { "tripplite", IETF_MIB_VERSION, NULL, NULL, ietf_mib, TRIPPLITE_SYSOID, NULL };

/* FIXME: Duplicate the line below to fix an issue with the code generator (nut-snmpinfo.py -> line is discarding) */
/*mib2nut_info_t	ietf = { "ietf", IETF_MIB_VERSION, IETF_OID_UPS_MIB "4.1.0", IETF_OID_UPS_MIB "1.1.0", ietf_mib, IETF_SYSOID, NULL };*/
mib2nut_info_t	ietf = { "ietf", IETF_MIB_VERSION, IETF_OID_UPS_MIB "4.1.0", IETF_OID_UPS_MIB "1.1.0", ietf_mib, IETF_SYSOID, NULL };
