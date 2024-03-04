/*  eaton-ups-pxg-mib.c - data to monitor Eaton / Powerware PXG UPS with NUT
 *  (using MIBs described in stdupsv1.mib and Xups.mib)
 *  Previously known as powerware-mib.c for "pw" mapping,
 *  later split into several subdrivers
 *
 *  Copyright (C)
 *       2005-2006 Olli Savia <ops@iki.fi>
 *       2005-2006 Niels Baggesen <niels@baggesen.net>
 *       2015-2022 Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
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

#include "eaton-ups-pxg-mib.h"
#if WITH_SNMP_LKP_FUN
/* FIXME: shared helper code, need to be put in common */
#include "eaton-pdu-marlin-helpers.h"
#endif

#define EATON_PXG_MIB_VERSION "0.105"

/* Powerware UPS newer PXGX UPS cards (BladeUPS, ...) */
#define EATON_PXGX_SYSOID	".1.3.6.1.4.1.534.2.12"

/* SNMP OIDs set */
#define PW_OID_MFR_NAME		"1.3.6.1.4.1.534.1.1.1.0"	/* XUPS-MIB::xupsIdentManufacturer.0 */
#define PW_OID_MODEL_NAME	"1.3.6.1.4.1.534.1.1.2.0"	/* XUPS-MIB::xupsIdentModel.0 */
#define PW_OID_FIRMREV		"1.3.6.1.4.1.534.1.1.3.0"	/* XUPS-MIB::xupsIdentSoftwareVersion.0 */

#define PW_OID_BATT_RUNTIME	"1.3.6.1.4.1.534.1.2.1.0"	/* XUPS-MIB::xupsBatTimeRemaining.0 */
#define PW_OID_BATT_VOLTAGE	"1.3.6.1.4.1.534.1.2.2.0"	/* XUPS-MIB::xupsBatVoltage.0 */
#define PW_OID_BATT_CURRENT	"1.3.6.1.4.1.534.1.2.3.0"	/* XUPS-MIB::xupsBatCurrent.0 */
#define PW_OID_BATT_CHARGE	"1.3.6.1.4.1.534.1.2.4.0"	/* XUPS-MIB::xupsBatCapacity.0 */
#define PW_OID_BATT_STATUS	"1.3.6.1.4.1.534.1.2.5.0"	/* XUPS-MIB::xupsBatteryAbmStatus.0 */

#define PW_OID_IN_FREQUENCY	"1.3.6.1.4.1.534.1.3.1.0"	/* XUPS-MIB::xupsInputFrequency.0 */
#define PW_OID_IN_LINE_BADS	"1.3.6.1.4.1.534.1.3.2.0"	/* XUPS-MIB::xupsInputLineBads.0 */
#define PW_OID_IN_LINES		"1.3.6.1.4.1.534.1.3.3.0"	/* XUPS-MIB::xupsInputNumPhases.0 */
#define PW_OID_IN_VOLTAGE	"1.3.6.1.4.1.534.1.3.4.1.2"	/* XUPS-MIB::xupsInputVoltage */
#define PW_OID_IN_CURRENT	"1.3.6.1.4.1.534.1.3.4.1.3"	/* XUPS-MIB::xupsInputCurrent */
#define PW_OID_IN_POWER		"1.3.6.1.4.1.534.1.3.4.1.4"	/* XUPS-MIB::xupsInputWatts */

#define PW_OID_OUT_LOAD		"1.3.6.1.4.1.534.1.4.1.0"	/* XUPS-MIB::xupsOutputLoad.0 */
#define PW_OID_OUT_FREQUENCY	"1.3.6.1.4.1.534.1.4.2.0"	/* XUPS-MIB::xupsOutputFrequency.0 */
#define PW_OID_OUT_LINES	"1.3.6.1.4.1.534.1.4.3.0"	/* XUPS-MIB::xupsOutputNumPhases.0 */
#define PW_OID_OUT_VOLTAGE	"1.3.6.1.4.1.534.1.4.4.1.2"	/* XUPS-MIB::xupsOutputVoltage */
#define PW_OID_OUT_CURRENT	"1.3.6.1.4.1.534.1.4.4.1.3"	/* XUPS-MIB::xupsOutputCurrent */
#define PW_OID_OUT_POWER	"1.3.6.1.4.1.534.1.4.4.1.4"	/* XUPS-MIB::xupsOutputWatts */
#define PW_OID_POWER_STATUS	"1.3.6.1.4.1.534.1.4.5.0"	/* XUPS-MIB::xupsOutputSource.0 */

#define PW_OID_BY_FREQUENCY	"1.3.6.1.4.1.534.1.5.1.0"	/* XUPS-MIB::xupsBypassFrequency.0 */
#define PW_OID_BY_LINES		"1.3.6.1.4.1.534.1.5.2.0"	/* XUPS-MIB::xupsBypassNumPhases.0 */
#define PW_OID_BY_VOLTAGE	"1.3.6.1.4.1.534.1.5.3.1.2"	/* XUPS-MIB::xupsBypassVoltage */

#define PW_OID_BATTEST_START	"1.3.6.1.4.1.534.1.8.1"		/* XUPS-MIB::xupsTestBattery   set to startTest(1) to initiate test*/

#define PW_OID_CONT_OFFDELAY	"1.3.6.1.4.1.534.1.9.1.0"		/* XUPS-MIB::xupsControlOutputOffDelay */
#define PW_OID_CONT_ONDELAY	"1.3.6.1.4.1.534.1.9.2.0"		/* XUPS-MIB::xupsControlOutputOnDelay */
#define PW_OID_CONT_OFFT_DEL	"1.3.6.1.4.1.534.1.9.3"		/* XUPS-MIB::xupsControlOutputOffTrapDelay */
#define PW_OID_CONT_ONT_DEL	"1.3.6.1.4.1.534.1.9.4"		/* XUPS-MIB::xupsControlOutputOnTrapDelay */
#define PW_OID_CONT_LOAD_SHED_AND_RESTART	"1.3.6.1.4.1.534.1.9.6"		/* XUPS-MIB::xupsLoadShedSecsWithRestart */

#define PW_OID_CONF_OVOLTAGE	"1.3.6.1.4.1.534.1.10.1.0"	/* XUPS-MIB::xupsConfigOutputVoltage.0 */
#define PW_OID_CONF_IVOLTAGE	"1.3.6.1.4.1.534.1.10.2.0"	/* XUPS-MIB::xupsConfigInputVoltage.0 */
#define PW_OID_CONF_POWER	"1.3.6.1.4.1.534.1.10.3.0"	/* XUPS-MIB::xupsConfigOutputWatts.0 */
#define PW_OID_CONF_FREQ	"1.3.6.1.4.1.534.1.10.4.0"	/* XUPS-MIB::xupsConfigOutputFreq.0 */

#define PW_OID_ALARMS		"1.3.6.1.4.1.534.1.7.1.0"		/* XUPS-MIB::xupsAlarms */
#define PW_OID_ALARM_OB		"1.3.6.1.4.1.534.1.7.3"		/* XUPS-MIB::xupsOnBattery */
#define PW_OID_ALARM_LB		"1.3.6.1.4.1.534.1.7.4"		/* XUPS-MIB::xupsLowBattery */

#define IETF_OID_AGENTREV	"1.3.6.1.2.1.33.1.1.4.0"	/* UPS-MIB::upsIdentAgentSoftwareVersion.0 */
#define IETF_OID_IDENT		"1.3.6.1.2.1.33.1.1.5.0"	/* UPS-MIB::upsIdentName.0 */
#define IETF_OID_CONF_OUT_VA	"1.3.6.1.2.1.33.1.9.5.0"	/* UPS-MIB::upsConfigOutputVA.0 */
#define IETF_OID_CONF_RUNTIME_LOW	"1.3.6.1.2.1.33.1.9.7.0"	/* UPS-MIB::upsConfigLowBattTime.0 */
#define IETF_OID_LOAD_LEVEL	"1.3.6.1.2.1.33.1.4.4.1.5"	/* UPS-MIB::upsOutputPercentLoad */
#define IETF_OID_AUTO_RESTART	"1.3.6.1.2.1.33.1.8.5.0"	/* UPS-MIB::upsAutoRestart */

/* Delay before powering off in seconds */
#define DEFAULT_OFFDELAY	30
/* Delay before powering on in seconds */
#define DEFAULT_ONDELAY	20
/* Default shutdown.return delay in seconds */
#define DEFAULT_SHUTDOWNDELAY	0

static info_lkp_t eaton_pxg_alarm_ob[] = {
	info_lkp_default(1, "OB"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_alarm_lb[] = {
	info_lkp_default(1, "LB"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_pwr_info[] = {
	info_lkp_default(0, "OFF"),	/* off */
	info_lkp_default(1, "OL"),	/* systemNormal */
	info_lkp_default(2, "OL"),	/* systemNormalUPSRedundant */
	info_lkp_default(3, "OL"),	/* systemNormalNotRedundant */
	info_lkp_default(4, "OB"),	/* systemOnDCSource*/
	info_lkp_default(5, "OB LB"),	/* systemOnDCSourceShutdownImminent */
	info_lkp_default(6, "OL"),	/* systemNormalBypassNotAvailable */
	info_lkp_default(7, "OL"),	/* systemNormalOnLine */
	info_lkp_default(8, "OL"),	/* systemNormalEnergySaverSystem */
	info_lkp_default(9, "OL"),	/* systemNormalVMMS */
	info_lkp_default(10, "OL"),	/* systemNormalHRS */
	info_lkp_default(13, "OL OVER"),	/* outputOverload */
	info_lkp_default(14, "OL TRIM"),	/* systemNormalOnBuck */
	info_lkp_default(15, "OL BOOST"),	/* systemNormalOnBoost */
	info_lkp_default(16, "BYPASS"),	/* onBypass */
	info_lkp_default(17, "BYPASS"),	/* onBypassStarting */
	info_lkp_default(18, "BYPASS"),	/* onBypassReady */
	info_lkp_default(32, "BYPASS"),	/* onMaintenanceBypass */
	info_lkp_default(33, "OL BYPASS"),	/* onMBSUPSOnLine */
	info_lkp_default(34, "BYPASS"),	/* onMBSUPSOnBypass */
	info_lkp_default(35, "OFF BYPASS"),	/* onMBSUPSOff */
	info_lkp_default(36, "OB BYPASS"),	/* onMBSUPSOnBattery */
	info_lkp_default(37, "OL"),	/* onMBSUPSOnLineESS */
	info_lkp_default(38, "OL"),	/* onMBSUPSOnLineVMMS */
	info_lkp_default(39, "OL"),	/* onMBSUPSOnLineHRS */
	info_lkp_default(40, "OL"),	/* onMBSStarting */
	info_lkp_default(41, "OL"),	/* onMBSReady */
	info_lkp_default(48, "OFF"),	/* loadOff */
	info_lkp_default(49, "OFF"),	/* loadOffStarting */
	info_lkp_default(50, "OFF"),	/* loadOffReady */
	info_lkp_default(64, "OL"),	/* supportingLoad */
	info_lkp_default(80, "OL"),	/* systemNormalSP */
	info_lkp_default(81, "OL"),	/* systemNormalEnergySaverSystemSP */
	info_lkp_default(96, "BYPASS"),	/* systemOnBypassSP */
	info_lkp_default(100, "BYPASS"),	/* systemOnManualMaintenanceBypassSP (0x64) */
	info_lkp_default(224, "OL OVER"),	/* loadSegmentOverload (0x64) */
	info_lkp_default(240, "OB"),	/* systemOnDCSourceSP */
	info_lkp_default(241, "OFF"),	/* systemOffSP */
	info_lkp_sentinel
};

/* FIXME: mapped to (experimental.)ups.type, but
 * should be output.source or ups.mode (need RFC)
 * to complement the above ups.status
 * along with having ups.type as described hereafter*/
/* FIXME: should be used by ups.mode or output.source (need RFC);
 * Note: this define is not set via project options; code was hidden with
 * original commit to "snmp-ups: support newer Genepi management cards";
 * un-hidden to make it "experimental.*" namespace during backporting
 */
#ifndef USE_PW_MODE_INFO
# define USE_PW_MODE_INFO 1
#endif

#if USE_PW_MODE_INFO
static info_lkp_t eaton_pxg_mode_info[] = {
	info_lkp_default(1, ""),
	info_lkp_default(2, ""),
	info_lkp_default(3, "normal"),
	info_lkp_default(4, ""),
	info_lkp_default(5, ""),
	info_lkp_default(6, ""),
	info_lkp_default(7, ""),
	info_lkp_default(8, "parallel capacity"),
	info_lkp_default(9, "parallel redundancy"),
	info_lkp_default(10, "high efficiency"),

	/* Extended status values,
	 * FIXME: check for source and completion */
	info_lkp_default(240, ""),	/* battery (0xF0) */
	info_lkp_default(100, ""),	/* maintenanceBypass (0x64) */
	info_lkp_default(96, ""),	/* Bypass (0x60) */
	info_lkp_default(81, "high efficiency"),	/* high efficiency (0x51) */
	info_lkp_default(80, "normal"),	/* normal (0x50) */
	info_lkp_default(64, ""),	/* UPS supporting load, normal degraded mode (0x40) */
	info_lkp_default(16, ""),	/* none (0x10) */
	info_lkp_sentinel
};
#endif /* USE_PW_MODE_INFO */

/* FIXME: may be standardized
 * extracted from bcmxcp.c->BCMXCP_TOPOLOGY_*, Make some common definitions */
static info_lkp_t eaton_pxg_topology_info[] = {
	info_lkp_default(0x0000, ""),	/* None; use the Table of Elements */
	info_lkp_default(0x0010, "Off-line switcher, Single Phase"),
	info_lkp_default(0x0020, "Line-Interactive UPS, Single Phase"),
	info_lkp_default(0x0021, "Line-Interactive UPS, Two Phase"),
	info_lkp_default(0x0022, "Line-Interactive UPS, Three Phase"),
	info_lkp_default(0x0030, "Dual AC Input, On-Line UPS, Single Phase"),
	info_lkp_default(0x0031, "Dual AC Input, On-Line UPS, Two Phase"),
	info_lkp_default(0x0032, "Dual AC Input, On-Line UPS, Three Phase"),
	info_lkp_default(0x0040, "On-Line UPS, Single Phase"),
	info_lkp_default(0x0041, "On-Line UPS, Two Phase"),
	info_lkp_default(0x0042, "On-Line UPS, Three Phase"),
	info_lkp_default(0x0050, "Parallel Redundant On-Line UPS, Single Phase"),
	info_lkp_default(0x0051, "Parallel Redundant On-Line UPS, Two Phase"),
	info_lkp_default(0x0052, "Parallel Redundant On-Line UPS, Three Phase"),
	info_lkp_default(0x0060, "Parallel for Capacity On-Line UPS, Single Phase"),
	info_lkp_default(0x0061, "Parallel for Capacity On-Line UPS, Two Phase"),
	info_lkp_default(0x0062, "Parallel for Capacity On-Line UPS, Three Phase"),
	info_lkp_default(0x0102, "System Bypass Module, Three Phase"),
	info_lkp_default(0x0122, "Hot-Tie Cabinet, Three Phase"),
	info_lkp_default(0x0200, "Outlet Controller, Single Phase"),
	info_lkp_default(0x0222, "Dual AC Input Static Switch Module, 3 Phase"),
	info_lkp_sentinel
};

/* Legacy implementation */
static info_lkp_t eaton_pxg_battery_abm_status[] = {
	info_lkp_default(1, "CHRG"),
	info_lkp_default(2, "DISCHRG"),
/*
	info_lkp_default(3, "Floating"),
	info_lkp_default(4, "Resting"),
	info_lkp_default(5, "Unknown"),
*/
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_abm_status_info[] = {
	info_lkp_default(1, "charging"),
	info_lkp_default(2, "discharging"),
	info_lkp_default(3, "floating"),
	info_lkp_default(4, "resting"),
	info_lkp_default(5, "unknown"),	/* Undefined - ABM is not activated */
	info_lkp_default(6, "disabled"),	/* ABM Charger Disabled */
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_batt_test_info[] = {
	info_lkp_default(1, "Unknown"),
	info_lkp_default(2, "Done and passed"),
	info_lkp_default(3, "Done and error"),
	info_lkp_default(4, "In progress"),
	info_lkp_default(5, "Not supported"),
	info_lkp_default(6, "Inhibited"),
	info_lkp_default(7, "Scheduled"),
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_yes_no_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "no"),
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_outlet_status_info[] = {
	info_lkp_default(1, "on"),
	info_lkp_default(2, "off"),
	info_lkp_default(3, "on"),	/* pendingOff, transitional status */
	info_lkp_default(4, "off"),	/* pendingOn, transitional status */
	/* info_lkp_default(5, ""),	//  unknown */
	/* info_lkp_default(6, ""),	//  reserved */
	info_lkp_default(7, "off"),	/* Failed in Closed position */
	info_lkp_default(8, "on"),	/* Failed in Open position */
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_ambient_drycontacts_info[] = {
	info_lkp_default(-1, "unknown"),
	info_lkp_default(1, "opened"),
	info_lkp_default(2, "closed"),
	info_lkp_default(3, "opened"),	/* openWithNotice   */
	info_lkp_default(4, "closed"),	/* closedWithNotice */
	info_lkp_sentinel
};

#if WITH_SNMP_LKP_FUN
/* Note: eaton_sensor_temperature_unit_fun() is defined in eaton-pdu-marlin-helpers.c
 * and su_temperature_read_fun() is in snmp-ups.c
 * Future work for DMF might provide same-named routines via LUA-C gateway.
 */

# if WITH_SNMP_LKP_FUN_DUMMY
/* Temperature unit consideration */
const char *eaton_sensor_temperature_unit_fun(void *raw_snmp_value) {
	/* snmp_value here would be a (long*) */
	NUT_UNUSED_VARIABLE(raw_snmp_value);
	return "unknown";
}
/* FIXME: please DMF, though this should be in snmp-ups.c or equiv. */
const char *su_temperature_read_fun(void *raw_snmp_value) {
	/* snmp_value here would be a (long*) */
	NUT_UNUSED_VARIABLE(raw_snmp_value);
	return "dummy";
};
# endif /* WITH_SNMP_LKP_FUN_DUMMY */

static info_lkp_t eaton_pxg_sensor_temperature_unit_info[] = {
	info_lkp_fun_vp2s(0, "dummy", eaton_sensor_temperature_unit_fun),
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_sensor_temperature_read_info[] = {
	info_lkp_fun_vp2s(0, "dummy", su_temperature_read_fun),
	info_lkp_sentinel
};

#else /* if not WITH_SNMP_LKP_FUN: */

/* FIXME: For now, DMF codebase falls back to old implementation with static
 * lookup/mapping tables for this, which can easily go into the DMF XML file.
 */
static info_lkp_t eaton_pxg_sensor_temperature_unit_info[] = {
	info_lkp_default(0, "kelvin"),
	info_lkp_default(1, "celsius"),
	info_lkp_default(2, "fahrenheit"),
	info_lkp_sentinel
};

#endif /* WITH_SNMP_LKP_FUN */

static info_lkp_t eaton_pxg_ambient_drycontacts_polarity_info[] = {
	info_lkp_default(0, "normal-opened"),
	info_lkp_default(1, "normal-closed"),
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_ambient_drycontacts_state_info[] = {
	info_lkp_default(0, "inactive"),
	info_lkp_default(1, "active"),
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_emp002_ambient_presence_info[] = {
	info_lkp_default(0, "unknown"),
	info_lkp_default(2, "yes"),	/* communicationOK */
	info_lkp_default(3, "no"),	/* communicationLost */
	info_lkp_sentinel
};

/* extracted from drivers/eaton-pdu-marlin-mib.c -> marlin_threshold_status_info */
static info_lkp_t eaton_pxg_threshold_status_info[] = {
	info_lkp_default(0, "good"),	/* No threshold triggered */
	info_lkp_default(1, "warning-low"),	/* Warning low threshold triggered */
	info_lkp_default(2, "critical-low"),	/* Critical low threshold triggered */
	info_lkp_default(3, "warning-high"),	/* Warning high threshold triggered */
	info_lkp_default(4, "critical-high"),	/* Critical high threshold triggered */
	info_lkp_sentinel
};

/* extracted from drivers/eaton-pdu-marlin-mib.c -> marlin_threshold_xxx_alarms_info */
static info_lkp_t eaton_pxg_threshold_temperature_alarms_info[] = {
	info_lkp_default(0, ""),	/* No threshold triggered */
	info_lkp_default(1, "low temperature warning!"),	/* Warning low threshold triggered */
	info_lkp_default(2, "low temperature critical!"),	/* Critical low threshold triggered */
	info_lkp_default(3, "high temperature warning!"),	/* Warning high threshold triggered */
	info_lkp_default(4, "high temperature critical!"),	/* Critical high threshold triggered */
	info_lkp_sentinel
};

static info_lkp_t eaton_pxg_threshold_humidity_alarms_info[] = {
	info_lkp_default(0, ""),	/* No threshold triggered */
	info_lkp_default(1, "low humidity warning!"),	/* Warning low threshold triggered */
	info_lkp_default(2, "low humidity critical!"),	/* Critical low threshold triggered */
	info_lkp_default(3, "high humidity warning!"),	/* Warning high threshold triggered */
	info_lkp_default(4, "high humidity critical!"),	/* Critical high threshold triggered */
	info_lkp_sentinel
};

/* Snmp2NUT lookup table */

static snmp_info_t eaton_pxg_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* FIXME: miss device page! */
	/* UPS page */
	/* info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MFR_NAME, "",
		SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MODEL_NAME, "",
		SU_FLAG_STATIC, NULL),
	/* FIXME: the 2 "firmware" entries below should be SU_FLAG_SEMI_STATIC */
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_FIRMREV, "",
		0, NULL),
	snmp_info_default("ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_AGENTREV, "",
		0, NULL),
	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_IDENT, "",
		SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.load", 0, 1.0, PW_OID_OUT_LOAD, "",
		0, NULL),
	/* FIXME: should be removed in favor of output.power */
	snmp_info_default("ups.power", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		0, NULL),
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsOutputWatts.1.0; Value (Integer): 300 */
	snmp_info_default("ups.power", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.4.1.0", "",
		0, NULL),

	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "OFF",
		SU_STATUS_PWR, &eaton_pxg_pwr_info[0]),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_ALARM_OB, "",
		SU_STATUS_BATT, &eaton_pxg_alarm_ob[0]),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_ALARM_LB, "",
		SU_STATUS_BATT, &eaton_pxg_alarm_lb[0]),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_BATT_STATUS, "",
		SU_STATUS_BATT, &eaton_pxg_battery_abm_status[0]),

#if USE_PW_MODE_INFO
	/* FIXME: should be ups.mode or output.source (need RFC) */
	/* Note: this define is not set via project options; code hidden with
	 * commit to "snmp-ups: support newer Genepi management cards" */
	snmp_info_default("experimental.ups.type", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "",
		SU_FLAG_STATIC | SU_FLAG_OK, &eaton_pxg_mode_info[0]),
#endif /* USE_PW_MODE_INFO */
	/* xupsTopologyType.0; Value (Integer): 32 */
	snmp_info_default("ups.type", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.13.1.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, &eaton_pxg_topology_info[0]),
	/* FIXME: should be removed in favor of their output. equivalent! */
	snmp_info_default("ups.realpower.nominal", 0, 1.0, PW_OID_CONF_POWER, "",
		0, NULL),
	/* FIXME: should be removed in favor of output.power.nominal */
	snmp_info_default("ups.power.nominal", 0, 1.0, IETF_OID_CONF_OUT_VA, "",
		0, NULL),
	/* XUPS-MIB::xupsEnvAmbientTemp.0 */
	snmp_info_default("ups.temperature", 0, 1.0, "1.3.6.1.4.1.534.1.6.1.0", "", 0, NULL),
	/* FIXME: These 2 data needs RFC! */
	/* XUPS-MIB::xupsEnvAmbientLowerLimit.0 */
	snmp_info_default("ups.temperature.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.2.0", "", 0, NULL),
	/* XUPS-MIB::xupsEnvAmbientUpperLimit.0 */
	snmp_info_default("ups.temperature.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.3.0", "", 0, NULL),
	/* XUPS-MIB::xupsTestBatteryStatus */
	snmp_info_default("ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.8.2.0", "", 0, &eaton_pxg_batt_test_info[0]),
	/* UPS-MIB::upsAutoRestart */
	snmp_info_default("ups.start.auto", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.2.1.33.1.8.5.0", "", SU_FLAG_OK, &eaton_pxg_yes_no_info[0]),
	/* XUPS-MIB::xupsBatteryAbmStatus.0 */
	snmp_info_default("battery.charger.status", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.2.5.0", "", SU_STATUS_BATT, &eaton_pxg_abm_status_info[0]),

	/* Battery page */
	snmp_info_default("battery.charge", 0, 1.0, PW_OID_BATT_CHARGE, "",
		0, NULL),
	snmp_info_default("battery.runtime", 0, 1.0, PW_OID_BATT_RUNTIME, "",
		0, NULL),
	snmp_info_default("battery.voltage", 0, 1.0, PW_OID_BATT_VOLTAGE, "",
		0, NULL),
	snmp_info_default("battery.current", 0, 0.1, PW_OID_BATT_CURRENT, "",
		0, NULL),
	snmp_info_default("battery.runtime.low", 0, 60.0, IETF_OID_CONF_RUNTIME_LOW, "",
		0, NULL),
	snmp_info_default("battery.date", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.1.2.6.0", NULL, SU_FLAG_OK, &su_convert_to_iso_date_info[FUNMAP_USDATE_TO_ISODATE]),

	/* Output page */
	snmp_info_default("output.phases", 0, 1.0, PW_OID_OUT_LINES, "", 0, NULL),
	/* XUPS-MIB::xupsOutputFrequency.0 */
	snmp_info_default("output.frequency", 0, 0.1, "1.3.6.1.4.1.534.1.4.2.0", "", 0, NULL),
	/* XUPS-MIB::xupsConfigOutputFreq.0 */
	snmp_info_default("output.frequency.nominal", 0, 0.1, "1.3.6.1.4.1.534.1.10.4.0", "", 0, NULL),
	/* XUPS-MIB::xupsOutputVoltage.1 */
	snmp_info_default("output.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.2.1", "", SU_OUTPUT_1, NULL),
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsOutputVoltage.1.0; Value (Integer): 230 */
	snmp_info_default("output.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.2.1.0", "", SU_OUTPUT_1, NULL),
	/* XUPS-MIB::xupsConfigOutputVoltage.0 */
	snmp_info_default("output.voltage.nominal", 0, 1.0, "1.3.6.1.4.1.534.1.10.1.0", "", 0, NULL),
	/* XUPS-MIB::xupsConfigLowOutputVoltageLimit.0 */
	snmp_info_default("output.voltage.low", 0, 1.0, ".1.3.6.1.4.1.534.1.10.6.0", "", 0, NULL),
	/* XUPS-MIB::xupsConfigHighOutputVoltageLimit.0 */
	snmp_info_default("output.voltage.high", 0, 1.0, ".1.3.6.1.4.1.534.1.10.7.0", "", 0, NULL),
	snmp_info_default("output.current", 0, 1.0, PW_OID_OUT_CURRENT ".1", "",
		SU_OUTPUT_1, NULL),
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsOutputCurrent.1.0; Value (Integer): 0 */
	snmp_info_default("output.current", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.3.1.0", "",
		SU_OUTPUT_1, NULL),
	snmp_info_default("output.realpower", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		SU_OUTPUT_1, NULL),
	/* Duplicate of the above entry, but pointing at the first index */
	/* Name/OID: xupsOutputWatts.1.0; Value (Integer): 1200 */
	snmp_info_default("output.realpower", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.4.1.0", "",
		0, NULL),
	/* Duplicate of "ups.realpower.nominal"
	 * FIXME: map either ups or output, but not both (or have an auto-remap) */
	snmp_info_default("output.realpower.nominal", 0, 1.0, PW_OID_CONF_POWER, "",
		0, NULL),
	snmp_info_default("output.L1-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".1", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".2", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".3", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L1.current", 0, 1.0, PW_OID_OUT_CURRENT ".1", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.current", 0, 1.0, PW_OID_OUT_CURRENT ".2", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.current", 0, 1.0, PW_OID_OUT_CURRENT ".3", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L1.realpower", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.realpower", 0, 1.0, PW_OID_OUT_POWER ".2", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.realpower", 0, 1.0, PW_OID_OUT_POWER ".3", "",
		SU_OUTPUT_3, NULL),
	/* FIXME: should better be output.Lx.load */
	snmp_info_default("output.L1.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".1", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".2", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".3", "",
		SU_OUTPUT_3, NULL),
	snmp_info_default("output.voltage.nominal", 0, 1.0, PW_OID_CONF_OVOLTAGE, "",
		0, NULL),

	/* Input page */
	snmp_info_default("input.phases", 0, 1.0, PW_OID_IN_LINES, "",
		0, NULL),
	snmp_info_default("input.frequency", 0, 0.1, PW_OID_IN_FREQUENCY, "",
		0, NULL),
	snmp_info_default("input.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".0", "",
		SU_INPUT_1, NULL),
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsInputVoltage.1[.0]; Value (Integer): 245 */
	snmp_info_default("input.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.3.4.1.2.1", "",
		SU_INPUT_1, NULL),

	/* XUPS-MIB::xupsConfigInputVoltage.0 */
	snmp_info_default("input.voltage.nominal", 0, 1.0, "1.3.6.1.4.1.534.1.10.2.0", "", 0, NULL),
	snmp_info_default("input.current", 0, 0.1, PW_OID_IN_CURRENT ".0", "",
		SU_INPUT_1, NULL),
	snmp_info_default("input.L1-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".1", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L2-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".2", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L3-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".3", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L1.current", 0, 1.0, PW_OID_IN_CURRENT ".1", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L2.current", 0, 1.0, PW_OID_IN_CURRENT ".2", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L3.current", 0, 1.0, PW_OID_IN_CURRENT ".3", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L1.realpower", 0, 1.0, PW_OID_IN_POWER ".1", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L2.realpower", 0, 1.0, PW_OID_IN_POWER ".2", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.L3.realpower", 0, 1.0, PW_OID_IN_POWER ".3", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.quality", 0, 1.0, PW_OID_IN_LINE_BADS, "",
		0, NULL),

	/* FIXME: this segfaults! do we assume the same number of bypass phases as input phases?
	snmp_info_default("input.bypass.phases", 0, 1.0, PW_OID_BY_LINES, "", 0, NULL), */
	snmp_info_default("input.bypass.frequency", 0, 0.1, PW_OID_BY_FREQUENCY, "", 0, NULL),
	snmp_info_default("input.bypass.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".0", "",
		SU_INPUT_1, NULL),
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsBypassVoltage.1.0; Value (Integer): 244 */
	snmp_info_default("input.bypass.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.5.3.1.2.1.0", "",
		SU_INPUT_1, NULL),
	snmp_info_default("input.bypass.L1-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".1", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.bypass.L2-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".2", "",
		SU_INPUT_3, NULL),
	snmp_info_default("input.bypass.L3-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".3", "",
		SU_INPUT_3, NULL),

	/* Outlet page */
	/* Master outlet id always equal to 0 */
	snmp_info_default("outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC , NULL),
	/* XUPS-MIB:: xupsSwitchable.0 */
	snmp_info_default("outlet.switchable", 0, 1, ".1.3.6.1.4.1.534.1.9.7.0", NULL, SU_FLAG_STATIC , &eaton_pxg_yes_no_info[0]),
	/* XUPS-MIB::xupsNumReceptacles; Value (Integer): 2 */
	snmp_info_default("outlet.count", 0, 1, ".1.3.6.1.4.1.534.1.12.1.0", NULL, SU_FLAG_STATIC, NULL),
	/* XUPS-MIB::xupsRecepIndex.X; Value (Integer): X */
	snmp_info_default("outlet.%i.id", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.1.%i", NULL, SU_FLAG_STATIC | SU_OUTLET, NULL),
	/* This MIB does not provide outlets switchability info. So map to a nearby
		OID, for data activation, and map all values to "yes" */
	snmp_info_default("outlet.%i.switchable", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.1.%i", NULL, SU_FLAG_STATIC | SU_OUTLET, NULL),
	/* XUPS-MIB::xupsRecepStatus.X; Value (Integer): 1 */
	snmp_info_default("outlet.%i.status", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.2.%i", NULL, SU_OUTLET, &eaton_pxg_outlet_status_info[0]),

	/* Ambient collection */
	/* EMP001 (legacy) mapping */
	/* XUPS-MIB::xupsEnvRemoteTemp.0 */
	snmp_info_default("ambient.temperature", 0, 1.0, "1.3.6.1.4.1.534.1.6.5.0", "", 0, NULL),
	/* XUPS-MIB::xupsEnvRemoteTempLowerLimit.0 */
	snmp_info_default("ambient.temperature.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.9.0", "", 0, NULL),
	/* XUPS-MIB::xupsEnvRemoteTempUpperLimit.0 */
	snmp_info_default("ambient.temperature.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.10.0", "", 0, NULL),
	/* XUPS-MIB::xupsEnvRemoteHumidity.0 */
	snmp_info_default("ambient.humidity", 0, 1.0, "1.3.6.1.4.1.534.1.6.6.0", "", 0, NULL),
	/* XUPS-MIB::xupsEnvRemoteHumidityLowerLimit.0 */
	snmp_info_default("ambient.humidity.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.11.0", "", 0, NULL),
	/* XUPS-MIB::xupsEnvRemoteHumidityUpperLimit.0 */
	snmp_info_default("ambient.humidity.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.12.0", "", 0, NULL),
	/* XUPS-MIB::xupsContactDescr.n */
	snmp_info_default("ambient.contacts.1.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.4.1", "", 0, NULL),
	snmp_info_default("ambient.contacts.2.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.4.2", "", 0, NULL),
	/* XUPS-MIB::xupsContactState.n */
	snmp_info_default("ambient.contacts.1.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.3.1", "", 0, &eaton_pxg_ambient_drycontacts_info[0]),
	snmp_info_default("ambient.contacts.2.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.3.2", "", 0, &eaton_pxg_ambient_drycontacts_info[0]),

	/* EMP002 (EATON EMP MIB) mapping, including daisychain support */
	/* Warning: indexes start at '1' not '0'! */
	/* sensorCount.0 */
	snmp_info_default("ambient.count", ST_FLAG_RW, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.1.0", "", 0, NULL),
	/* CommunicationStatus.n */
	snmp_info_default("ambient.%i.present", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.1.4.1.1.%i",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_pxg_emp002_ambient_presence_info[0]),
	/* sensorName.n: OctetString EMPDT1H1C2 @1 */
	snmp_info_default("ambient.%i.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.3.1.1.%i", "", SU_AMBIENT_TEMPLATE, NULL),
	/* sensorManufacturer.n */
	snmp_info_default("ambient.%i.mfr", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.6.%i", "", SU_AMBIENT_TEMPLATE, NULL),
	/* sensorModel.n */
	snmp_info_default("ambient.%i.model", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.7.%i", "", SU_AMBIENT_TEMPLATE, NULL),
	/* sensorSerialNumber.n */
	snmp_info_default("ambient.%i.serial", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.9.%i", "", SU_AMBIENT_TEMPLATE, NULL),
	/* sensorUuid.n */
	snmp_info_default("ambient.%i.id", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.2.%i", "", SU_AMBIENT_TEMPLATE, NULL),
	/* sensorAddress.n */
	snmp_info_default("ambient.%i.address", 0, 1, ".1.3.6.1.4.1.534.6.8.1.1.2.1.4.%i", "", SU_AMBIENT_TEMPLATE, NULL),
	/* sensorFirmwareVersion.n */
	snmp_info_default("ambient.%i.firmware", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.10.%i", "", SU_AMBIENT_TEMPLATE, NULL),
	/* temperatureUnit.1
	 * MUST be before the temperature data reading! */
	snmp_info_default("ambient.%i.temperature.unit", 0, 1.0, ".1.3.6.1.4.1.534.6.8.1.2.5.0", "", SU_AMBIENT_TEMPLATE, &eaton_pxg_sensor_temperature_unit_info[0]),

	/* temperatureValue.n.1 */
#if WITH_SNMP_LKP_FUN
	snmp_info_default("ambient.%i.temperature", 0, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE,
		&eaton_pxg_sensor_temperature_read_info[0]),
#else
	snmp_info_default("ambient.%i.temperature", 0, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE,
		NULL),
#endif

	snmp_info_default("ambient.%i.temperature.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.2.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_pxg_threshold_status_info[0]),
	snmp_info_default("ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.2.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_pxg_threshold_temperature_alarms_info[0]),
	/* FIXME: ambient.n.temperature.{minimum,maximum} */
	/* temperatureThresholdLowCritical.n.1 */
	snmp_info_default("ambient.%i.temperature.low.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.6.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* temperatureThresholdLowWarning.n.1 */
	snmp_info_default("ambient.%i.temperature.low.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.5.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* temperatureThresholdHighWarning.n.1 */
	snmp_info_default("ambient.%i.temperature.high.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.7.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* temperatureThresholdHighCritical.n.1 */
	snmp_info_default("ambient.%i.temperature.high.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.8.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* humidityValue.n.1 */
	snmp_info_default("ambient.%i.humidity", 0, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	snmp_info_default("ambient.%i.humidity.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.3.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_pxg_threshold_status_info[0]),
	snmp_info_default("ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.3.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_pxg_threshold_humidity_alarms_info[0]),
	/* FIXME: consider ambient.n.humidity.{minimum,maximum} */
	/* humidityThresholdLowCritical.n.1 */
	snmp_info_default("ambient.%i.humidity.low.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.6.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* humidityThresholdLowWarning.n.1 */
	snmp_info_default("ambient.%i.humidity.low.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.5.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* humidityThresholdHighWarning.n.1 */
	snmp_info_default("ambient.%i.humidity.high.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.7.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* humidityThresholdHighCritical.n.1 */
	snmp_info_default("ambient.%i.humidity.high.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.8.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	/* digitalInputName.n.{1,2} */
	snmp_info_default("ambient.%i.contacts.1.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.2.1.1.%i.1", "", SU_AMBIENT_TEMPLATE, NULL),
	snmp_info_default("ambient.%i.contacts.2.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.2.1.1.%i.2", "", SU_AMBIENT_TEMPLATE, NULL),
	/* digitalInputPolarity.n */
	snmp_info_default("ambient.%i.contacts.1.config", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.4.2.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, &eaton_pxg_ambient_drycontacts_polarity_info[0]),
	snmp_info_default("ambient.%i.contacts.2.config", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.4.2.1.3.%i.2", "", SU_AMBIENT_TEMPLATE, &eaton_pxg_ambient_drycontacts_polarity_info[0]),
	/* XUPS-MIB::xupsContactState.n */
	snmp_info_default("ambient.%i.contacts.1.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, &eaton_pxg_ambient_drycontacts_state_info[0]),
	snmp_info_default("ambient.%i.contacts.2.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.3.1.3.%i.2", "", SU_AMBIENT_TEMPLATE, &eaton_pxg_ambient_drycontacts_state_info[0]),

	/* instant commands */
	snmp_info_default("test.battery.start.quick", 0, 1, PW_OID_BATTEST_START, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL),
	/* Shed load and restart when line power back on; cannot be canceled */
	snmp_info_default("shutdown.return", 0, DEFAULT_SHUTDOWNDELAY, PW_OID_CONT_LOAD_SHED_AND_RESTART, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL),
	/* Cancel output off, by writing 0 to xupsControlOutputOffDelay */
	snmp_info_default("shutdown.stop", 0, 0, PW_OID_CONT_OFFDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL),
	/* XUPS-MIB::xupsControlOutputOffDelay */
	/* load off after 1 sec, shortest possible delay; 0 cancels */
	snmp_info_default("load.off", 0, 1, PW_OID_CONT_OFFDELAY, "1",
		SU_TYPE_CMD | SU_FLAG_OK, NULL),
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	snmp_info_default("load.off.delay", 0, 1, PW_OID_CONT_OFFDELAY, NULL,
		SU_TYPE_CMD | SU_FLAG_OK, NULL),
	/* XUPS-MIB::xupsControlOutputOnDelay */
	/* load on after 1 sec, shortest possible delay; 0 cancels */
	snmp_info_default("load.on", 0, 1, PW_OID_CONT_ONDELAY, "1",
		SU_TYPE_CMD | SU_FLAG_OK, NULL),
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	snmp_info_default("load.on.delay", 0, 1, PW_OID_CONT_ONDELAY, NULL,
		SU_TYPE_CMD | SU_FLAG_OK, NULL),

	/* Delays handling:
	 * 0-n :Time in seconds until the command is issued
	 * -1:Cancel a pending Off/On command */
	/* XUPS-MIB::xupsRecepOffDelaySecs.n */
	snmp_info_default("outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.3.%i",
		"0", SU_TYPE_CMD | SU_OUTLET, NULL),
	/* XUPS-MIB::xupsRecepOnDelaySecs.n */
	snmp_info_default("outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.4.%i",
		"0", SU_TYPE_CMD | SU_OUTLET, NULL),
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	snmp_info_default("outlet.%i.load.off.delay", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.3.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET, NULL),
	/* XUPS-MIB::xupsRecepOnDelaySecs.n */
	snmp_info_default("outlet.%i.load.on.delay", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.4.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET, NULL),

	snmp_info_default("ups.alarms", 0, 1.0, PW_OID_ALARMS, "",
		0, NULL),

	/* end of structure. */
	snmp_info_sentinel
} ;

static alarms_info_t eaton_pxg_alarms[] = {
	/* xupsLowBattery */
	{ PW_OID_ALARM_LB, "LB", NULL },
	/* xupsOutputOverload */
	{ ".1.3.6.1.4.1.534.1.7.7", "OVER", "Output overload!" },
	/* xupsInternalFailure */
	{ ".1.3.6.1.4.1.534.1.7.8", NULL, "Internal failure!" },
	/* xupsBatteryDischarged */
	{ ".1.3.6.1.4.1.534.1.7.9", NULL, "Battery discharged!" },
	/* xupsInverterFailure */
	{ ".1.3.6.1.4.1.534.1.7.10", NULL, "Inverter failure!" },
	/* xupsOnBypass
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.11", "BYPASS", "On bypass!" },
	/* xupsBypassNotAvailable
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.12", NULL, "Bypass not available!" },
	/* xupsOutputOff
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.13", "OFF", "Output off!" },
	/* xupsInputFailure
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.14", NULL, "Input failure!" },
	/* xupsBuildingAlarm
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.15", NULL, "Building alarm!" },
	/* xupsShutdownImminent */
	{ ".1.3.6.1.4.1.534.1.7.16", NULL, "Shutdown imminent!" },
	/* xupsOnInverter
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.17", NULL, "On inverter!" },
	/* xupsBreakerOpen
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.20", NULL, "Breaker open!" },
	/* xupsAlarmBatteryBad */
	{ ".1.3.6.1.4.1.534.1.7.23", "RB", "Battery bad!" },
	/* xupsOutputOffAsRequested
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.24", "OFF", "Output off as requested!" },
	/* xupsDiagnosticTestFailed
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.25", NULL, "Diagnostic test failure!" },
	/* xupsCommunicationsLost */
	{ ".1.3.6.1.4.1.534.1.7.26", NULL, "Communication with UPS lost!" },
	/* xupsUpsShutdownPending */
	{ ".1.3.6.1.4.1.534.1.7.27", NULL, "Shutdown pending!" },
	/* xupsAmbientTempBad */
	{ ".1.3.6.1.4.1.534.1.7.29", NULL, "Bad ambient temperature!" },
	/* xupsLossOfRedundancy */
	{ ".1.3.6.1.4.1.534.1.7.30", NULL, "Redundancy lost!" },
	/* xupsAlarmTempBad */
	{ ".1.3.6.1.4.1.534.1.7.31", NULL, "Bad temperature!" },
	/* xupsAlarmChargerFailed */
	{ ".1.3.6.1.4.1.534.1.7.32", NULL, "Charger failure!" },
	/* xupsAlarmFanFailure */
	{ ".1.3.6.1.4.1.534.1.7.33", NULL, "Fan failure!" },
	/* xupsAlarmFuseFailure */
	{ ".1.3.6.1.4.1.534.1.7.34", NULL, "Fuse failure!" },
	/* xupsPowerSwitchBad */
	{ ".1.3.6.1.4.1.534.1.7.35", NULL, "Powerswitch failure!" },
	/* xupsModuleFailure */
	{ ".1.3.6.1.4.1.534.1.7.36", NULL, "Parallel or composite module failure!" },
	/* xupsOnAlternatePowerSource
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.37", NULL, "Using alternative power source!" },
	/* xupsAltPowerNotAvailable
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.38", NULL, "Alternative power source unavailable!" },
	/* xupsRemoteTempBad */
	{ ".1.3.6.1.4.1.534.1.7.40", NULL, "Bad remote temperature!" },
	/* xupsRemoteHumidityBad */
	{ ".1.3.6.1.4.1.534.1.7.41", NULL, "Bad remote humidity!" },
	/* xupsAlarmOutputBad */
	{ ".1.3.6.1.4.1.534.1.7.42", NULL, "Bad output condition!" },
	/* xupsAlarmAwaitingPower
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.43", NULL, "Awaiting power!" },
	/* xupsOnMaintenanceBypass
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event?
	 * FIXME: NUT currently doesn't distinguish between Maintenance and
	 * Automatic Bypass (both published as "ups.alarm: BYPASS)
	 * Should we make the distinction? */
	{ ".1.3.6.1.4.1.534.1.7.44", "BYPASS", "On maintenance bypass!" },


	/* end of structure. */
	{ NULL, NULL, NULL }
} ;

mib2nut_info_t	eaton_pxg_ups = { "eaton_pxg_ups", EATON_PXG_MIB_VERSION, NULL, PW_OID_MODEL_NAME, eaton_pxg_mib, EATON_PXGX_SYSOID , eaton_pxg_alarms };
