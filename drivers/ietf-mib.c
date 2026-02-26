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

#define IETF_MIB_VERSION	"1.55"

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
	info_lkp_default(1, ""),	/* unknown */
	info_lkp_default(2, ""),	/* batteryNormal */
	info_lkp_default(3, "LB"),	/* batteryLow */
	info_lkp_default(4, "LB"),	/* batteryDepleted */
	info_lkp_sentinel
};

static info_lkp_t ietf_power_source_info[] = {
	info_lkp_default(1, ""),	/* other */
	info_lkp_default(2, "OFF"),	/* none */
	info_lkp_default(3, "OL"),	/* normal */
	info_lkp_default(4, "OL BYPASS"),	/* bypass */
	info_lkp_default(5, "OB"),	/* battery */
	info_lkp_default(6, "OL BOOST"),	/* booster */
	info_lkp_default(7, "OL TRIM"),	/* reducer */
	info_lkp_sentinel
};

static info_lkp_t ietf_overload_info[] = {
	info_lkp_default(1, "OVER"),	/* output overload */
	info_lkp_sentinel
};

static info_lkp_t ietf_test_active_info[] = {
	info_lkp_default(1, ""),	/* upsTestNoTestsInitiated */
	info_lkp_default(2, ""),	/* upsTestAbortTestInProgress */
	info_lkp_default(3, "TEST"),	/* upsTestGeneralSystemsTest */
	info_lkp_default(4, "TEST"),	/* upsTestQuickBatteryTest */
	info_lkp_default(5, "CAL"),	/* upsTestDeepBatteryCalibration */
	info_lkp_sentinel
};

static info_lkp_t ietf_test_result_info[] = {
	info_lkp_default(1, "done and passed"),
	info_lkp_default(2, "done and warning"),
	info_lkp_default(3, "done and error"),
	info_lkp_default(4, "aborted"),
	info_lkp_default(5, "in progress"),
	info_lkp_default(6, "no test initiated"),
	info_lkp_sentinel
};

#ifdef DEBUG
static info_lkp_t ietf_shutdown_type_info[] = {
	info_lkp_default(1, "output"),
	info_lkp_default(2, "system"),
	info_lkp_sentinel
};
#endif

static info_lkp_t ietf_yes_no_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "no"),
	info_lkp_sentinel
};

static info_lkp_t ietf_beeper_status_info[] = {
	info_lkp_default(1, "disabled"),
	info_lkp_default(2, "enabled"),
	info_lkp_default(3, "muted"),
	info_lkp_sentinel
};

/* Snmp2NUT lookup table info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
static snmp_info_t ietf_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* The Device Identification group */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.1.0", "Generic", SU_FLAG_STATIC, NULL), /* upsIdentManufacturer */
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.2.0", "Generic SNMP UPS", SU_FLAG_STATIC, NULL), /* upsIdentModel */
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.3.0", "", SU_FLAG_STATIC, NULL), /* upsIdentUPSSoftwareVersion */
	snmp_info_default("ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.4.0", "", SU_FLAG_STATIC, NULL), /* upsIdentAgentSoftwareVersion */
#ifdef DEBUG
	snmp_info_default("debug.upsIdentName", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.5.0", "", 0, NULL), /* upsIdentName */
	snmp_info_default("debug.upsIdentAttachedDevices", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "1.6.0", "", 0, NULL), /* upsIdentAttachedDevices */
#endif
	/* Battery Group */
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "2.1.0", "", SU_STATUS_BATT, ietf_battery_info), /* upsBatteryStatus */
#ifdef DEBUG
	snmp_info_default("debug.upsSecondsOnBattery", 0, 1.0, IETF_OID_UPS_MIB "2.2.0", "", 0, NULL), /* upsSecondsOnBattery */
#endif
	snmp_info_default("battery.runtime", 0, 60.0, IETF_OID_UPS_MIB "2.3.0", "", 0, NULL), /* upsEstimatedMinutesRemaining */
	snmp_info_default("battery.charge", 0, 1, IETF_OID_UPS_MIB "2.4.0", "", 0, NULL), /* upsEstimatedChargeRemaining */
	snmp_info_default("battery.voltage", 0, 0.1, IETF_OID_UPS_MIB "2.5.0", "", 0, NULL), /* upsBatteryVoltage */
	snmp_info_default("battery.current", 0, 0.1, IETF_OID_UPS_MIB "2.6.0", "", SU_FLAG_NEGINVALID, NULL), /* upsBatteryCurrent */
	snmp_info_default("battery.temperature", 0, 1.0, IETF_OID_UPS_MIB "2.7.0", "", 0, NULL), /* upsBatteryTemperature */

	/* Input Group */
#ifdef DEBUG
	snmp_info_default("debug.upsInputLineBads", 0, 1.0, IETF_OID_UPS_MIB "3.1.0", "", 0, NULL), /* upsInputLineBads */
#endif
	snmp_info_default("input.phases", 0, 1.0, IETF_OID_UPS_MIB "3.2.0", "", 0, NULL), /* upsInputNumLines */
#ifdef DEBUG
	snmp_info_default("debug.upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.1", "", SU_INPUT_1, NULL), /* upsInputLineIndex */
	snmp_info_default("debug.[1].upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.1", "", SU_INPUT_3, NULL),
	snmp_info_default("debug.[2].upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.2", "", SU_INPUT_3, NULL),
	snmp_info_default("debug.[3].upsInputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.1.3", "", SU_INPUT_3, NULL),
#endif
	snmp_info_default("input.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.1", "", SU_INPUT_1, NULL), /* upsInputFrequency */
	snmp_info_default("input.L1.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3.frequency", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.2.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.1", "", SU_INPUT_1, NULL), /* upsInputVoltage */
	snmp_info_default("input.L1-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.3.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.1", "", SU_INPUT_1 |SU_FLAG_NEGINVALID, NULL), /* upsInputCurrent */
	snmp_info_default("input.L1.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3.current", 0, 0.1, IETF_OID_UPS_MIB "3.3.1.4.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.1", "", SU_INPUT_1 | SU_FLAG_NEGINVALID, NULL), /* upsInputTruePower */
	snmp_info_default("input.L1.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3.realpower", 0, 1.0, IETF_OID_UPS_MIB "3.3.1.5.3", "", SU_INPUT_3, NULL),

	/* Output Group */
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "4.1.0", "", SU_STATUS_PWR, ietf_power_source_info), /* upsOutputSource */
	snmp_info_default("output.frequency", 0, 0.1, IETF_OID_UPS_MIB "4.2.0", "", 0, NULL), /* upsOutputFrequency */
	snmp_info_default("output.phases", 0, 1.0, IETF_OID_UPS_MIB "4.3.0", "", 0, NULL), /* upsOutputNumLines */
#ifdef DEBUG
	snmp_info_default("debug.upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.1", "", SU_OUTPUT_1, NULL), /* upsOutputLineIndex */
	snmp_info_default("debug.[1].upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("debug.[2].upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("debug.[3].upsOutputLineIndex", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.1.3", "", SU_OUTPUT_3, NULL),
#endif
	snmp_info_default("output.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.1", "", SU_OUTPUT_1, NULL), /* upsOutputVoltage */
	snmp_info_default("output.L1-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.2.3", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.1", "", SU_OUTPUT_1 | SU_FLAG_NEGINVALID, NULL), /* upsOutputCurrent */
	snmp_info_default("output.L1.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.current", 0, 0.1, IETF_OID_UPS_MIB "4.4.1.3.3", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.1", "", SU_OUTPUT_1 | SU_FLAG_NEGINVALID, NULL), /* upsOutputPower */
	snmp_info_default("output.L1.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.realpower", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.4.3", "", SU_OUTPUT_3, NULL),
	snmp_info_default("ups.load", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.1", "", SU_OUTPUT_1, NULL), /* upsOutputPercentLoad */
	snmp_info_default("output.L1.power.percent", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.power.percent", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.power.percent", 0, 1.0, IETF_OID_UPS_MIB "4.4.1.5.3", "", SU_OUTPUT_3, NULL),

	/* Bypass Group */
	snmp_info_default("input.bypass.phases", 0, 1.0, IETF_OID_UPS_MIB "5.2.0", "", SU_FLAG_NEGINVALID, NULL), /* upsBypassNumLines */
	snmp_info_default("input.bypass.frequency", 0, 0.1, IETF_OID_UPS_MIB "5.1.0", "", SU_BYPASS_1 | SU_BYPASS_3 | SU_FLAG_NEGINVALID, NULL), /* upsBypassFrequency */
#ifdef DEBUG
	snmp_info_default("debug.upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.1", "", SU_BYPASS_1, NULL), /* upsBypassLineIndex */
	snmp_info_default("debug.[1].upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.1", "", SU_BYPASS_3, NULL),
	snmp_info_default("debug.[2].upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.2", "", SU_BYPASS_3, NULL),
	snmp_info_default("debug.[3].upsBypassLineIndex", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.1.3", "", SU_BYPASS_3, NULL),
#endif
	snmp_info_default("input.bypass.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.1", "", SU_BYPASS_1, NULL), /* upsBypassVoltage */
	snmp_info_default("input.bypass.L1-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.1", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L2-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.2", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L3-N.voltage", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.2.3", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.1", "", SU_BYPASS_1, NULL), /* upsBypassCurrent */
	snmp_info_default("input.bypass.L1.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.1", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L2.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.2", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L3.current", 0, 0.1, IETF_OID_UPS_MIB "5.3.1.3.3", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.1", "", SU_BYPASS_1, NULL), /* upsBypassPower */
	snmp_info_default("input.bypass.L1.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.1", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L2.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.2", "", SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L3.realpower", 0, 1.0, IETF_OID_UPS_MIB "5.3.1.4.3", "", SU_BYPASS_3, NULL),

	/* Alarm Group */
#ifdef DEBUG
	snmp_info_default("debug.upsAlarmsPresent", 0, 1.0, IETF_OID_UPS_MIB "6.1.0", "", 0, NULL), /* upsAlarmsPresent */
	snmp_info_default("debug.upsAlarmBatteryBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.1", "", 0, NULL), /* upsAlarmBatteryBad */
	snmp_info_default("debug.upsAlarmOnBattery", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.2", "", 0, NULL), /* upsAlarmOnBattery */
	snmp_info_default("debug.upsAlarmLowBattery", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.3", "", 0, NULL), /* upsAlarmLowBattery */
	snmp_info_default("debug.upsAlarmDepletedBattery", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.4", "", 0, NULL), /* upsAlarmDepletedBattery */
	snmp_info_default("debug.upsAlarmTempBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.5", "", 0, NULL), /* upsAlarmTempBad */
	snmp_info_default("debug.upsAlarmInputBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.6", "", 0, NULL), /* upsAlarmInputBad */
	snmp_info_default("debug.upsAlarmOutputBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.7", "", 0, NULL), /* upsAlarmOutputBad */
#endif
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.8", "", 0, ietf_overload_info), /* upsAlarmOutputOverload */
#ifdef DEBUG
	snmp_info_default("debug.upsAlarmOnBypass", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.9", "", 0, NULL), /* upsAlarmOnBypass */
	snmp_info_default("debug.upsAlarmBypassBad", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.10", "", 0, NULL), /* upsAlarmBypassBad */
	snmp_info_default("debug.upsAlarmOutputOffAsRequested", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.11", "", 0, NULL), /* upsAlarmOutputOffAsRequested */
	snmp_info_default("debug.upsAlarmUpsOffAsRequested", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.12", "", 0, NULL), /* upsAlarmUpsOffAsRequested */
	snmp_info_default("debug.upsAlarmChargerFailed", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.13", "", 0, NULL), /* upsAlarmChargerFailed */
	snmp_info_default("debug.upsAlarmUpsOutputOff", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.14", "", 0, NULL), /* upsAlarmUpsOutputOff */
	snmp_info_default("debug.upsAlarmUpsSystemOff", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.15", "", 0, NULL), /* upsAlarmUpsSystemOff */
	snmp_info_default("debug.upsAlarmFanFailure", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.16", "", 0, NULL), /* upsAlarmFanFailure */
	snmp_info_default("debug.upsAlarmFuseFailure", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.17", "", 0, NULL), /* upsAlarmFuseFailure */
	snmp_info_default("debug.upsAlarmGeneralFault", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.18", "", 0, NULL), /* upsAlarmGeneralFault */
	snmp_info_default("debug.upsAlarmDiagnosticTestFailed", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.19", "", 0, NULL), /* upsAlarmDiagnosticTestFailed */
	snmp_info_default("debug.upsAlarmCommunicationsLost", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.20", "", 0, NULL), /* upsAlarmCommunicationsLost */
	snmp_info_default("debug.upsAlarmAwaitingPower", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.21", "", 0, NULL), /* upsAlarmAwaitingPower */
	snmp_info_default("debug.upsAlarmShutdownPending", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.22", "", 0, NULL), /* upsAlarmShutdownPending */
	snmp_info_default("debug.upsAlarmShutdownImminent", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.23", "", 0, NULL), /* upsAlarmShutdownImminent */
	snmp_info_default("debug.upsAlarmTestInProgress", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "6.3.24", "", 0, NULL), /* upsAlarmTestInProgress */
#endif

	/* Test Group */
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.1.0", "", 0, ietf_test_active_info), /* upsTestId */
	snmp_info_default("test.battery.stop", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL), /* upsTestAbortTestInProgress */
	snmp_info_default("test.battery.start", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL), /* upsTestGeneralSystemsTest */
	snmp_info_default("test.battery.start.quick", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL), /* upsTestQuickBatteryTest */
	snmp_info_default("test.battery.start.deep", 0, 1, IETF_OID_UPS_MIB "7.1.0", "0", SU_TYPE_CMD, NULL), /* upsTestDeepBatteryCalibration */
#ifdef DEBUG
	snmp_info_default("debug.upsTestSpinLock", 0, 1.0, IETF_OID_UPS_MIB "7.2.0", "", 0, NULL), /* upsTestSpinLock */
#endif
	snmp_info_default("ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.3.0", "", 0, ietf_test_result_info), /* upsTestResultsSummary */
#ifdef DEBUG
	snmp_info_default("debug.upsTestResultsDetail", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.4.0", "", 0, NULL), /* upsTestResultsDetail */
	snmp_info_default("debug.upsTestStartTime", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.5.0", "", 0, NULL), /* upsTestStartTime */
	snmp_info_default("debug.upsTestElapsedTime", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "7.6.0", "", 0, NULL), /* upsTestElapsedTime */
#endif

	/* Control Group */
#ifdef DEBUG
	snmp_info_default("debug.upsShutdownType", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "8.1.0", "", 0, ietf_shutdown_type_info), /* upsShutdownType */
#endif
	snmp_info_default("ups.timer.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 8, IETF_OID_UPS_MIB "8.2.0", "", 0, NULL), /* upsShutdownAfterDelay*/
	snmp_info_default("load.off", 0, 1, IETF_OID_UPS_MIB "8.2.0", "0", SU_TYPE_CMD, NULL),
	snmp_info_default("ups.timer.start", ST_FLAG_STRING | ST_FLAG_RW, 8, IETF_OID_UPS_MIB "8.3.0", "", 0, NULL), /* upsStartupAfterDelay */
	snmp_info_default("load.on", 0, 1, IETF_OID_UPS_MIB "8.3.0", "0", SU_TYPE_CMD, NULL),
	snmp_info_default("ups.timer.reboot", ST_FLAG_STRING | ST_FLAG_RW, 8, IETF_OID_UPS_MIB "8.4.0", "", 0, NULL), /* upsRebootWithDuration */
	snmp_info_default("ups.start.auto", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "8.5.0", "", 0, ietf_yes_no_info), /* upsAutoRestart */

	/* Configuration Group */
	snmp_info_default("input.voltage.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.1.0", "", SU_FLAG_NEGINVALID, NULL), /* upsConfigInputVoltage */
	snmp_info_default("input.frequency.nominal", 0, 0.1, IETF_OID_UPS_MIB "9.2.0", "", SU_FLAG_NEGINVALID, NULL), /* upsConfigInputFreq */
	snmp_info_default("output.voltage.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.3.0", "", 0, NULL), /* upsConfigOutputVoltage */
	snmp_info_default("output.frequency.nominal", 0, 0.1, IETF_OID_UPS_MIB "9.4.0", "", 0, NULL), /* upsConfigOutputFreq */
	snmp_info_default("output.power.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.5.0", "", SU_FLAG_NEGINVALID, NULL), /* upsConfigOutputVA */
	snmp_info_default("output.realpower.nominal", 0, 1.0, IETF_OID_UPS_MIB "9.6.0", "", SU_FLAG_NEGINVALID, NULL), /* upsConfigOutputPower */
	snmp_info_default("battery.runtime.low", 0, 60.0, IETF_OID_UPS_MIB "9.7.0", "", 0, NULL), /* upsConfigLowBattTime */
	snmp_info_default("ups.beeper.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_UPS_MIB "9.8.0", "", 0, ietf_beeper_status_info), /* upsConfigAudibleStatus */
	snmp_info_default("beeper.disable", 0, 1, IETF_OID_UPS_MIB "9.8.0", "1", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.enable", 0, 1, IETF_OID_UPS_MIB "9.8.0", "2", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.mute", 0, 1, IETF_OID_UPS_MIB "9.8.0", "3", SU_TYPE_CMD, NULL),
	snmp_info_default("input.transfer.low", 0, 1.0, IETF_OID_UPS_MIB "9.9.0", "", 0, NULL), /* upsConfigLowVoltageTransferPoint */
	snmp_info_default("input.transfer.high", 0, 1.0, IETF_OID_UPS_MIB "9.10.0", "", 0, NULL), /* upsConfigHighVoltageTransferPoint */

	/* end of structure. */
	snmp_info_sentinel
};

/* FIXME: Rename the structure here (or even relocate to new file)
 * and in snmp-ups.c when the real TrippLite mappings get defined. */
/* FIXME: Duplicate the line below to fix an issue with the code generator (nut-snmpinfo.py -> line is discarding) */
/*mib2nut_info_t	tripplite_ietf = { "tripplite", IETF_MIB_VERSION, NULL, NULL, ietf_mib, TRIPPLITE_SYSOID, NULL };*/
mib2nut_info_t	tripplite_ietf = { "tripplite", IETF_MIB_VERSION, NULL, NULL, ietf_mib, TRIPPLITE_SYSOID, NULL };

/* FIXME: Duplicate the line below to fix an issue with the code generator (nut-snmpinfo.py -> line is discarding) */
/*mib2nut_info_t	ietf = { "ietf", IETF_MIB_VERSION, IETF_OID_UPS_MIB "4.1.0", IETF_OID_UPS_MIB "1.1.0", ietf_mib, IETF_SYSOID, NULL };*/
mib2nut_info_t	ietf = { "ietf", IETF_MIB_VERSION, IETF_OID_UPS_MIB "4.1.0", IETF_OID_UPS_MIB "1.1.0", ietf_mib, IETF_SYSOID, NULL };
