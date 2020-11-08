/*  compaq-mib.c - data to monitor SNMP UPS with NUT
 *
 *  Copyright (C)
 *    2002-2012 Arnaud Quette <arnaud.quette@free.fr>
 *    2002-2006 Niels Baggesen <niels@baggesen.net>
 *    2002-2006 Philip Ward <p.g.ward@stir.ac.uk>
 *
 *  This program was sponsored by MGE UPS SYSTEMS, and now Eaton
 *
 *  This version has been tested using:
 *    HP R5500XR UPS with management card AF401A and a single phase input
 *    HP R/T3000 UPS with management card AF465A and a single phase input
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

#include "compaq-mib.h"

#define CPQPOWER_MIB_VERSION	"1.64"

#define DEFAULT_ONDELAY		"30"
#define DEFAULT_OFFDELAY	"20"

/* Note: RFC-1628 (UPS MIB) is also supported on these devices! */

/* SNMP OIDs set */
#define CPQPOWER_OID_UPS_MIB			".1.3.6.1.4.1.232.165.3"
/* FIXME: to be verified */
#define CPQPOWER_SYSOID					CPQPOWER_OID_UPS_MIB

#define CPQPOWER_OID_MFR_NAME         ".1.3.6.1.4.1.232.165.3.1.1.0"	/* UPS-MIB::upsIdentManufacturer */
#define CPQPOWER_OID_MODEL_NAME       ".1.3.6.1.4.1.232.165.3.1.2.0"	/* UPS-MIB::upsIdentModel */
#define CPQPOWER_OID_FIRMREV          ".1.3.6.1.4.1.232.165.3.1.3.0"	/* UPS-MIB::upsIdentUPSSoftwareVersion */
#define CPQPOWER_OID_OEMCODE          ".1.3.6.1.4.1.232.165.3.1.4.0"	/* UPS-MIB::upsIdentAgentSoftwareVersion */

#define CPQPOWER_OID_BATT_RUNTIME     ".1.3.6.1.4.1.232.165.3.2.1.0"	/* UPS-MIB::upsEstimatedMinutesRemaining */
#define CPQPOWER_OID_BATT_VOLTAGE     ".1.3.6.1.4.1.232.165.3.2.2.0"	/* UPS-MIB::upsBatteryVoltage */
#define CPQPOWER_OID_BATT_CURRENT     ".1.3.6.1.4.1.232.165.3.2.3.0"	/* UPS-MIB::upsBatteryCurrent */
#define CPQPOWER_OID_BATT_CHARGE      ".1.3.6.1.4.1.232.165.3.2.4.0"	/* UPS-MIB::upsBattCapacity */
#define CPQPOWER_OID_BATT_STATUS      ".1.3.6.1.4.1.232.165.3.2.5.0"	/* UPS-MIB::upsBatteryAbmStatus */

#define CPQPOWER_OID_IN_FREQ          ".1.3.6.1.4.1.232.165.3.3.1.0"	/* UPS-MIB::upsInputFrequency */
#define CPQPOWER_OID_IN_LINEBADS      ".1.3.6.1.4.1.232.165.3.3.2.0"	/* UPS-MIB::upsInputLineBads */
#define CPQPOWER_OID_IN_LINES         ".1.3.6.1.4.1.232.165.3.3.3.0"	/* UPS-MIB::upsInputNumPhases */

#define CPQPOWER_OID_IN_PHASE         ".1.3.6.1.4.1.232.165.3.3.4.1.1"	/* UPS-MIB::upsInputPhase */
#define CPQPOWER_OID_IN_VOLTAGE       ".1.3.6.1.4.1.232.165.3.3.4.1.2"	/* UPS-MIB::upsInputVoltage */
#define CPQPOWER_OID_IN_CURRENT       ".1.3.6.1.4.1.232.165.3.3.4.1.3"	/* UPS-MIB::upsInputCurrent */
#define CPQPOWER_OID_IN_POWER         ".1.3.6.1.4.1.232.165.3.3.4.1.4"	/* UPS-MIB::upsInputWatts */

#define CPQPOWER_OID_LOAD_LEVEL       ".1.3.6.1.4.1.232.165.3.4.1.0"     /* UPS-MIB::upsOutputLoad */
#define CPQPOWER_OID_OUT_FREQUENCY    ".1.3.6.1.4.1.232.165.3.4.2.0"	/* UPS-MIB::upsOutputFrequency */
#define CPQPOWER_OID_OUT_LINES        ".1.3.6.1.4.1.232.165.3.4.3.0"	/* UPS-MIB::upsOutputNumPhases */

#define CPQPOWER_OID_OUT_PHASE        ".1.3.6.1.4.1.232.165.3.4.4.1.1"	/* UPS-MIB::upsOutputPhase */
#define CPQPOWER_OID_OUT_VOLTAGE      ".1.3.6.1.4.1.232.165.3.4.4.1.2"	/* UPS-MIB::upsOutputVoltage */
#define CPQPOWER_OID_OUT_CURRENT      ".1.3.6.1.4.1.232.165.3.4.4.1.3"	/* UPS-MIB::upsOutputCurrent */
#define CPQPOWER_OID_OUT_POWER        ".1.3.6.1.4.1.232.165.3.4.4.1.4"	/* UPS-MIB::upsOutputWatts */

#define CPQPOWER_OID_POWER_STATUS     ".1.3.6.1.4.1.232.165.3.4.5.0"	/* UPS-MIB::upsOutputSource */

#define CPQPOWER_OID_AMBIENT_TEMP     ".1.3.6.1.4.1.232.165.3.6.1.0"     /* UPS-MIB::upsEnvAmbientTemp */

#define CPQPOWER_OID_UPS_TEST_BATT    ".1.3.6.1.4.1.232.165.3.7.1.0"     /* UPS-MIB::upsTestBattery */
#define CPQPOWER_OID_UPS_TEST_RES     ".1.3.6.1.4.1.232.165.3.7.2.0"     /* UPS-MIB::upsTestBatteryStatus */
#define CPQPOWER_OID_ALARM_OB         ".1.3.6.1.4.1.232.165.3.7.3.0"     /* UPS-MIB::upsOnBattery */
#define CPQPOWER_OID_ALARM_LB         ".1.3.6.1.4.1.232.165.3.7.4.0"     /* UPS-MIB::upsLowBattery */

#define IETF_OID_AGENTREV             ".1.3.6.1.2.1.33.1.1.4.0"          /* UPS-MIB::upsIdentAgentSoftwareVersion.0 */

/* Not used, as no longer supported by MIB ver. 1.76 (Github issue 118)
static info_lkp_t cpqpower_alarm_ob[] = {
	{ 1, "OB", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
*/

/* Not used, as no longer supported by MIB ver. 1.76 (Github issue 118)
static info_lkp_t cpqpower_alarm_lb[] = {
	{ 1, "LB", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
*/

/* Defines for CPQPOWER_OID_POWER_STATUS (1) */
static info_lkp_t cpqpower_pwr_info[] = {
	{ 1, ""       /* other */, NULL, NULL },
	{ 2, "OFF"    /* none */, NULL, NULL },
	{ 3, "OL"     /* normal */, NULL, NULL },
	{ 4, "OL BYPASS" /* bypass */, NULL, NULL },
	{ 5, "OB"     /* battery */, NULL, NULL },
	{ 6, "OL BOOST"  /* booster */, NULL, NULL },
	{ 7, "OL TRIM"   /* reducer */, NULL, NULL },
	{ 8, "OL"   /* parallelCapacity */, NULL, NULL },
	{ 9, "OL"   /* parallelRedundant */, NULL, NULL },
	{ 10, "OL" /* HighEfficiencyMode */, NULL, NULL },
	{ 0, NULL, NULL, NULL }
} ;

static info_lkp_t cpqpower_mode_info[] = {
	{ 1, "", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 3, "normal", NULL, NULL },
	{ 4, "", NULL, NULL },
	{ 5, "", NULL, NULL },
	{ 6, "", NULL, NULL },
	{ 7, "", NULL, NULL },
	{ 8, "parallel capacity", NULL, NULL },
	{ 9, "parallel redundancy", NULL, NULL },
	{10, "high efficiency", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t cpqpower_battery_abm_status[] = {
	{ 1, "CHRG", NULL, NULL },
	{ 2, "DISCHRG", NULL, NULL },
/*	{ 3, "Floating", NULL, NULL }, */
/*	{ 4, "Resting", NULL, NULL }, */
/*	{ 5, "Unknown", NULL, NULL }, */
	{ 0, NULL, NULL, NULL }
} ;

/* Defines for CPQPOWER_OID_UPS_TEST_RES */
static info_lkp_t cpqpower_test_res_info[] = {
	{ 1, "Unknown", NULL, NULL },
	{ 2, "Done and passed", NULL, NULL },
	{ 3, "Done and error", NULL, NULL },
	{ 4, "In progress", NULL, NULL },
	{ 5, "Not supported", NULL, NULL },
	{ 6, "Inhibited", NULL, NULL },
	{ 7, "Scheduled", NULL, NULL },
	{ 0, NULL, NULL, NULL }
} ;

#define CPQPOWER_START_TEST		"1"

static info_lkp_t cpqpower_outlet_status_info[] = {
	{ 1, "on", NULL, NULL },
	{ 2, "off", NULL, NULL },
	{ 3, "pendingOff", NULL, NULL }, /* transitional status */
	{ 4, "pendingOn", NULL, NULL }, /* transitional status */
	{ 5, "unknown", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* Ugly hack: having the matching OID present means that the outlet is
 * switchable. So, it should not require this value lookup */
static info_lkp_t cpqpower_outlet_switchability_info[] = {
	{ 1, "yes", NULL, NULL },
	{ 2, "yes", NULL, NULL },
	{ 3, "yes", NULL, NULL },
	{ 4, "yes", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

#define CPQPOWER_OID_SD_AFTER_DELAY	 ".1.3.6.1.4.1.232.165.3.8.1.0"	/* UPS-MIB::upsControlOutputOffDelay */
#define CPQPOWER_OFF_DO		0

/* Snmp2NUT lookup table */

static snmp_info_t cpqpower_mib[] = {
	/* UPS page */
	/* info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_MFR_NAME, "HP/Compaq", SU_FLAG_STATIC, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_MODEL_NAME, "SNMP UPS", SU_FLAG_STATIC, NULL },
	/* { "ups.model.aux", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_OEMCODE, "", SU_FLAG_STATIC, NULL },*/
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.1.2.7.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* FIXME: split between firmware and firmware.aux ("00.01.0019;00.01.0004")
	 * UPS Firmware Revision :	00.01.0004
	 * Communication Board Firmware Revision :	00.01.0019 */
	/* FIXME: the 2 "firmware" entries below should be SU_FLAG_SEMI_STATIC */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_FIRMREV, "", 0, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_AGENTREV, "", 0, NULL },
	{ "ups.load", 0, 1.0, CPQPOWER_OID_LOAD_LEVEL, "", 0, NULL },
	{ "ups.realpower", 0, 1.0, CPQPOWER_OID_OUT_POWER, "", SU_OUTPUT_1, NULL },
	{ "ups.realpower", 0, 1.0, ".1.3.6.1.4.1.232.165.3.9.3.0", "", SU_OUTPUT_1, NULL },
	{ "ups.L1.realpower", 0, 0.1, CPQPOWER_OID_OUT_POWER ".1", "", SU_OUTPUT_3, NULL },
	{ "ups.L2.realpower", 0, 0.1, CPQPOWER_OID_OUT_POWER ".2", "", SU_OUTPUT_3, NULL },
	{ "ups.L3.realpower", 0, 0.1, CPQPOWER_OID_OUT_POWER ".3", "", SU_OUTPUT_3, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_POWER_STATUS, "OFF", SU_STATUS_PWR, cpqpower_pwr_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_BATT_STATUS, "", SU_STATUS_PWR, cpqpower_battery_abm_status },
	/* The next two lines are no longer supported by MIB ver. 1.76 (Github issue 118)
	 * { "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_ALARM_OB, "", SU_STATUS_BATT, cpqpower_alarm_ob },
	 * { "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_ALARM_LB, "", SU_STATUS_BATT, cpqpower_alarm_lb }, */
	/* { "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_BATT_STATUS, "", SU_STATUS_BATT, ietf_batt_info }, */
	/* FIXME: this should use either .1.3.6.1.4.1.232.165.3.11.1.0 (upsTopologyType)
	 * or .1.3.6.1.4.1.232.165.3.11.2.0 (upsTopoMachineCode) */
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_POWER_STATUS, "", SU_STATUS_PWR, cpqpower_mode_info },
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_UPS_TEST_RES, "", 0, cpqpower_test_res_info },
	/* FIXME: handle ups.date and ups.time
	 * - OID: .1.3.6.1.4.1.232.165.3.9.5.0
	 * - format MM/DD/YYYY HH:MM:SS */
	/* FIXME: handle upsInputSource.0 (".1.3.6.1.4.1.232.165.3.3.5.0")
	 * other(1)
	 * none(2)
	 * primaryUtility(3)
	 * bypassFeed(4)
	 * secondaryUtility(5)
	 * generator(6)
	 * flywheel(7)
	 * fuelcell(8) */

	{ "ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 6, ".1.3.6.1.4.1.232.165.3.8.1.0", DEFAULT_OFFDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.delay.start", ST_FLAG_STRING | ST_FLAG_RW, 6, ".1.3.6.1.4.1.232.165.3.8.2.0", DEFAULT_ONDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.timer.shutdown", 0, 1, ".1.3.6.1.4.1.232.165.3.8.1.0", "", SU_FLAG_OK, NULL },
	{ "ups.timer.start", 0, 1, ".1.3.6.1.4.1.232.165.3.8.2.0", "", SU_FLAG_OK, NULL },

	/* Ambient page */
	{ "ambient.temperature", 0, 1.0, CPQPOWER_OID_AMBIENT_TEMP, "", 0, NULL },
	{ "ambient.temperature.low", 0, 1.0, ".1.3.6.1.4.1.232.165.3.6.2.0", "", 0, NULL },
	{ "ambient.temperature.high", 0, 1.0, ".1.3.6.1.4.1.232.165.3.6.3.0", "", 0, NULL },

	/* Battery page */
	{ "battery.charge", 0, 1.0, CPQPOWER_OID_BATT_CHARGE, "", 0, NULL },
	{ "battery.runtime", 0, 1.0, CPQPOWER_OID_BATT_RUNTIME, "", 0, NULL },
	{ "battery.voltage", 0, 0.1, CPQPOWER_OID_BATT_VOLTAGE, "", 0, NULL },
	{ "battery.current", 0, 0.1, CPQPOWER_OID_BATT_CURRENT, "", 0, NULL },
	/* FIXME: need the new variable (for ABM)
	{ "battery.status", 0, 0.1, ".1.3.6.1.4.1.232.165.3.2.5.0", "", 0, NULL }, */

	/* Input page */
	{ "input.phases", 0, 1.0, CPQPOWER_OID_IN_LINES, "", 0, NULL },
/*	{ "input.phase", 0, 1.0, CPQPOWER_OID_IN_PHASE, "", SU_OUTPUT_1, NULL }, */
	{ "input.frequency", 0, 0.1, CPQPOWER_OID_IN_FREQ , "", 0, NULL },
	{ "input.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE, "", SU_OUTPUT_1, NULL },
	{ "input.voltage", 0, 1.0, ".1.3.6.1.4.1.232.165.3.3.4.1.2.1", "", SU_OUTPUT_1, NULL },
	{ "input.voltage.nominal", ST_FLAG_RW | ST_FLAG_STRING, 3, ".1.3.6.1.4.1.232.165.3.9.2.0", "", SU_OUTPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE ".1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE ".2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE ".3", "", SU_INPUT_3, NULL },
	{ "input.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT, "", SU_OUTPUT_1, NULL },
	{ "input.current", 0, 0.1, ".1.3.6.1.4.1.232.165.3.3.4.1.3.1", "", SU_OUTPUT_1, NULL },

	{ "input.L1.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT ".1", "", SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT ".2", "", SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT ".3", "", SU_INPUT_3, NULL },
	{ "input.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER, "", SU_OUTPUT_1, NULL },
	{ "input.L1.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER ".1", "", SU_INPUT_3, NULL },
	{ "input.L2.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER ".2", "", SU_INPUT_3, NULL },
	{ "input.L3.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER ".3", "", SU_INPUT_3, NULL },
	{ "input.quality", 0, 1.0, CPQPOWER_OID_IN_LINEBADS, "", 0, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, CPQPOWER_OID_OUT_LINES, "", 0, NULL },
/*	{ "output.phase", 0, 1.0, CPQPOWER_OID_OUT_PHASE, "", SU_OUTPUT_1, NULL }, */
	{ "output.frequency", 0, 0.1, CPQPOWER_OID_OUT_FREQUENCY, "", 0, NULL },
	/* FIXME: handle multiplier (0.1 there) */
	{ "output.frequency.nominal", ST_FLAG_RW | ST_FLAG_STRING, 3, ".1.3.6.1.4.1.232.165.3.9.4.0", "", SU_OUTPUT_1, NULL },
	{ "output.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE, "", SU_OUTPUT_1, NULL },
	{ "output.voltage", 0, 1.0, ".1.3.6.1.4.1.232.165.3.4.4.1.2.1", "", SU_OUTPUT_1, NULL },
	{ "output.voltage.nominal", ST_FLAG_RW | ST_FLAG_STRING, 3, ".1.3.6.1.4.1.232.165.3.9.1.0", "", SU_OUTPUT_1, NULL },
	{ "output.L1-N.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE ".1", "", SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE ".2", "", SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE ".3", "", SU_OUTPUT_3, NULL },
	{ "output.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT, "", SU_OUTPUT_1, NULL },
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.232.165.3.4.4.1.3.1", "", SU_OUTPUT_1, NULL },
	/* { "output.realpower", 0, 1.0, ".1.3.6.1.4.1.232.165.3.4.4.1.4", "", SU_OUTPUT_1, NULL }, */
	{ "output.L1.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT ".1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT ".2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT ".3", "", SU_OUTPUT_3, NULL },

	/* FIXME: what to map with these?
	 * Name/OID: upsConfigLowOutputVoltageLimit.0; Value (Integer): 160
	 * => input.transfer.low?
	 * Name/OID: upsConfigHighOutputVoltageLimit.0; Value (Integer): 288
	 * => input.transfer.high? */

	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.232.165.3.10.1.0", "0", 0, NULL }, /* upsNumReceptacles */

/*	{ "outlet.current", 0, 0.001, AR_OID_UNIT_CURRENT ".0", NULL, 0, NULL, NULL },
	{ "outlet.voltage", 0, 0.001, AR_OID_UNIT_VOLTAGE ".0", NULL, 0, NULL, NULL },
	{ "outlet.realpower", 0, 1.0, AR_OID_UNIT_ACTIVEPOWER ".0", NULL, 0, NULL, NULL },
	{ "outlet.power", 0, 1.0, AR_OID_UNIT_APPARENTPOWER ".0", NULL, 0, NULL, NULL }, */

	/* outlet template definition */
	/* FIXME always true? */
	{ "outlet.%i.switchable", ST_FLAG_STRING, 3, ".1.3.6.1.4.1.232.165.3.10.2.1.1.%i", "yes", SU_FLAG_STATIC | SU_OUTLET, &cpqpower_outlet_switchability_info[0] },
	{ "outlet.%i.id", 0, 1, ".1.3.6.1.4.1.232.165.3.10.2.1.1.%i", "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, NULL },
	/* { "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, AR_OID_OUTLET_NAME ".%i", NULL, SU_OUTLET, NULL }, */
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.3.10.2.1.2.%i", NULL, SU_FLAG_OK | SU_OUTLET, &cpqpower_outlet_status_info[0] },
	/* FIXME: come up with a suitable varname!
	 * - The delay after going On Battery until the Receptacle is automatically turned Off.
	 * A value of -1 means that this Output should never be turned Off automatically, but must be turned Off only by command.
	 * { "outlet.%i.autoswitch.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 6, ".1.3.6.1.4.1.232.165.3.10.2.1.5.%i", DEFAULT_OFFDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL }, // upsRecepAutoOffDelay
	 * - Seconds delay after the Outlet is signaled to turn On before the Output is Automatically turned ON.
	 * A value of -1 means that this Output should never be turned On automatically, but only when specifically commanded to do so.
	 * { "outlet.%i.autoswitch.delay.start", ST_FLAG_STRING | ST_FLAG_RW, 6, ".1.3.6.1.4.1.232.165.3.10.2.1.5.%i", DEFAULT_OFFDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL }, // upsRecepAutoOnDelay
	 */
	/* FIXME: also define .stop (as for 'shutdown.reboot')
	 * and .delay */
	{ "outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.232.165.3.10.2.1.3.%i", "0", SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.232.165.3.10.2.1.4.%i", "0", SU_TYPE_CMD | SU_OUTLET, NULL },
	/* FIXME: also define a .delay or map to "outlet.%i.delay.shutdown" */
	{ "outlet.%i.load.cycle", 0, 1, ".1.3.6.1.4.1.232.165.3.10.2.1.7.%i", "0", SU_TYPE_CMD | SU_OUTLET, NULL },

	/* instant commands. */
	/* We need to duplicate load.{on,off} Vs load.{on,off}.delay, since
	 * "0" cancels the shutdown, so we put "1" (second) for immediate off! */
	{ "load.off", 0, 1, ".1.3.6.1.4.1.232.165.3.8.1.0", "1", SU_TYPE_CMD, NULL },
	{ "load.on", 0, 1, ".1.3.6.1.4.1.232.165.3.8.2.0", "1", SU_TYPE_CMD, NULL },
	{ "shutdown.stop", 0, 1, ".1.3.6.1.4.1.232.165.3.8.1.0", "0", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* FIXME: need ups.{timer,delay}.{start,shutdown} param counterparts! */

	{ "load.off.delay", 0, 1, ".1.3.6.1.4.1.232.165.3.8.1.0", DEFAULT_OFFDELAY, SU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 1, ".1.3.6.1.4.1.232.165.3.8.2.0", DEFAULT_ONDELAY, SU_TYPE_CMD, NULL },
	/*	{ CMD_SHUTDOWN, 0, CPQPOWER_OFF_GRACEFUL, CPQPOWER_OID_OFF, "", 0, NULL }, */
	{ "shutdown.reboot", 0, 1, ".1.3.6.1.4.1.232.165.3.8.6.0", "0", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "test.battery.start", 0, 1, ".1.3.6.1.4.1.232.165.3.7.1.0", CPQPOWER_START_TEST, SU_TYPE_CMD | SU_FLAG_OK, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	compaq = { "cpqpower", CPQPOWER_MIB_VERSION, NULL, CPQPOWER_OID_MFR_NAME, cpqpower_mib, CPQPOWER_SYSOID };

