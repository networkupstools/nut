/*  powerware-mib.c - data to monitor Powerware UPS with NUT
 *  (using MIBs described in stdupsv1.mib and Xups.mib)
 *
 *  Copyright (C)
 *       2005-2006 Olli Savia <ops@iki.fi>
 *       2005-2006 Niels Baggesen <niels@baggesen.net>
 *       2015-2016 Arnaud Quette <ArnaudQuette@Eaton.com>
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

#include "powerware-mib.h"

#define PW_MIB_VERSION "0.92"

/* TODO: more sysOID and MIBs support:
 *
 * Powerware UPS (Ingrasys X-SLOT and BD-SLOT): ".1.3.6.1.4.1.534.1"
 * Powerware PXGX cards: ".1.3.6.1.4.1.534.2.12"
 *		PXGX 2000 cards (UPS): Get xupsIdentModel (".1.3.6.1.4.1.534.1.1.2.0")
 * 		PXGX 1000 cards (PDU/RPP/RPM): Get pduNumPanels ".1.3.6.1.4.1.534.6.6.4.1.1.1.4.0"
 */

/* Powerware UPS (Ingrasys X-SLOT and BD-SLOT) */
#define POWERWARE_SYSOID	".1.3.6.1.4.1.534.1"
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

#define PW_OID_CONT_OFFDELAY	"1.3.6.1.4.1.534.1.9.1"		/* XUPS-MIB::xupsControlOutputOffDelay */
#define PW_OID_CONT_ONDELAY	"1.3.6.1.4.1.534.1.9.2"		/* XUPS-MIB::xupsControlOutputOnDelay */
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

static info_lkp_t pw_alarm_ob[] = {
	{ 1, "OB", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
} ;

static info_lkp_t pw_alarm_lb[] = {
	{ 1, "LB", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
} ;

static info_lkp_t pw_pwr_info[] = {
	{   1, ""         /* other */, NULL, NULL },
	{   2, "OFF"       /* none */, NULL, NULL },
	{   3, "OL"        /* normal */, NULL, NULL },
	{   4, "BYPASS"    /* bypass */, NULL, NULL },
	{   5, "OB"        /* battery */, NULL, NULL },
	{   6, "OL BOOST"  /* booster */, NULL, NULL },
	{   7, "OL TRIM"   /* reducer */, NULL, NULL },
	{   8, "OL"        /* parallel capacity */, NULL, NULL },
	{   9, "OL"        /* parallel redundancy */, NULL, NULL },
	{  10, "OL"        /* high efficiency */, NULL, NULL },
	/* Extended status values */
	{ 240, "OB"        /* battery (0xF0) */, NULL, NULL },
	{ 100, "BYPASS"    /* maintenanceBypass (0x64) */, NULL, NULL },
	{  96, "BYPASS"    /* Bypass (0x60) */, NULL, NULL },
	{  81, "OL"        /* high efficiency (0x51) */, NULL, NULL },
	{  80, "OL"        /* normal (0x50) */, NULL, NULL },
	{  64, "OL"        /* UPS supporting load, normal degraded mode (0x40) */, NULL, NULL },
	{  16, "OFF"       /* none (0x10) */, NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t pw_mode_info[] = {
	{   1, "", NULL, NULL },
	{   2, "", NULL, NULL },
	{   3, "normal", NULL, NULL },
	{   4, "", NULL, NULL },
	{   5, "", NULL, NULL },
	{   6, "", NULL, NULL },
	{   7, "", NULL, NULL },
	{   8, "parallel capacity", NULL, NULL },
	{   9, "parallel redundancy", NULL, NULL },
	{  10, "high efficiency", NULL, NULL },
	/* Extended status values */
	{ 240, ""                /* battery (0xF0) */, NULL, NULL },
	{ 100, ""                /* maintenanceBypass (0x64) */, NULL, NULL },
	{  96, ""                /* Bypass (0x60) */, NULL, NULL },
	{  81, "high efficiency" /* high efficiency (0x51) */, NULL, NULL },
	{  80, "normal"          /* normal (0x50) */, NULL, NULL },
	{  64, ""                /* UPS supporting load, normal degraded mode (0x40) */, NULL, NULL },
	{  16, ""                /* none (0x10) */, NULL, NULL },
	{   0, NULL, NULL, NULL }
};

/* Legacy implementation */
static info_lkp_t pw_battery_abm_status[] = {
	{ 1, "CHRG", NULL, NULL },
	{ 2, "DISCHRG", NULL, NULL },
/*	{ 3, "Floating", NULL, NULL }, */
/*	{ 4, "Resting", NULL, NULL }, */
/*	{ 5, "Unknown", NULL, NULL }, */
	{ 0, NULL, NULL, NULL }
} ;

static info_lkp_t eaton_abm_status_info[] = {
	{ 1, "charging", NULL, NULL },
	{ 2, "discharging", NULL, NULL },
	{ 3, "floating", NULL, NULL },
	{ 4, "resting", NULL, NULL },
	{ 5, "unknown", NULL, NULL },   /* Undefined - ABM is not activated */
	{ 6, "disabled", NULL, NULL },  /* ABM Charger Disabled */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t pw_batt_test_info[] = {
	{ 1, "Unknown", NULL, NULL },
	{ 2, "Done and passed", NULL, NULL },
	{ 3, "Done and error", NULL, NULL },
	{ 4, "In progress", NULL, NULL },
	{ 5, "Not supported", NULL, NULL },
	{ 6, "Inhibited", NULL, NULL },
	{ 7, "Scheduled", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ietf_yes_no_info[] = {
	{ 1, "yes", NULL, NULL },
	{ 2, "no", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table */

static snmp_info_t pw_mib[] = {
	/* FIXME: miss device page! */
	/* UPS page */
	/* info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MFR_NAME, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MODEL_NAME, "",
		SU_FLAG_STATIC, NULL },
	/* FIXME: the 2 "firmware" entries below should be SU_FLAG_SEMI_STATIC */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_FIRMREV, "",
		0, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_AGENTREV, "",
		0, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_IDENT, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.load", 0, 1.0, PW_OID_OUT_LOAD, "",
		SU_OUTPUT_1, NULL },
	{ "ups.power", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		0, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "OFF",
		SU_STATUS_PWR, &pw_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_ALARM_OB, "",
		SU_STATUS_BATT, &pw_alarm_ob[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_ALARM_LB, "",
		SU_STATUS_BATT, &pw_alarm_lb[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_BATT_STATUS, "",
		SU_STATUS_BATT, &pw_battery_abm_status[0] },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "",
		SU_FLAG_STATIC | SU_FLAG_OK, &pw_mode_info[0] },
	{ "ups.realpower.nominal", 0, 1.0, PW_OID_CONF_POWER, "",
		0, NULL },
	{ "ups.power.nominal", 0, 1.0, IETF_OID_CONF_OUT_VA, "",
		0, NULL },
	/* XUPS-MIB::xupsEnvAmbientTemp.0 */
	{ "ups.temperature", 0, 1.0, "1.3.6.1.4.1.534.1.6.1.0", "", 0, NULL },
	/* FIXME: These 2 data needs RFC! */
	/* XUPS-MIB::xupsEnvAmbientLowerLimit.0 */
	{ "ups.temperature.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.2.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvAmbientUpperLimit.0 */
	{ "ups.temperature.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.3.0", "", 0, NULL },
	/* XUPS-MIB::xupsTestBatteryStatus */
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.8.2.0", "", 0, &pw_batt_test_info[0] },
	/* UPS-MIB::upsAutoRestart */
	{ "ups.start.auto", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.2.1.33.1.8.5.0", "", SU_FLAG_OK, &ietf_yes_no_info[0] },
	/* XUPS-MIB::xupsBatteryAbmStatus.0 */
	{ "battery.charger.status", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.2.5.0", "", SU_STATUS_BATT, &eaton_abm_status_info[0] },

	/* Battery page */
	{ "battery.charge", 0, 1.0, PW_OID_BATT_CHARGE, "",
		0, NULL },
	{ "battery.runtime", 0, 1.0, PW_OID_BATT_RUNTIME, "",
		0, NULL },
	{ "battery.voltage", 0, 1.0, PW_OID_BATT_VOLTAGE, "",
		0, NULL },
	{ "battery.current", 0, 0.1, PW_OID_BATT_CURRENT, "",
		0, NULL },
	{ "battery.runtime.low", 0, 60.0, IETF_OID_CONF_RUNTIME_LOW, "",
		0, NULL },
	{ "battery.date", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.1.2.6.0", NULL, SU_FLAG_OK, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, PW_OID_OUT_LINES, "", 0, NULL },
	/* XUPS-MIB::xupsOutputFrequency.0 */
	{ "output.frequency", 0, 0.1, "1.3.6.1.4.1.534.1.4.2.0", "", 0, NULL },
	/* XUPS-MIB::xupsConfigOutputFreq.0 */
	{ "output.frequency.nominal", 0, 0.1, "1.3.6.1.4.1.534.1.10.4.0", "", 0, NULL },
	/* XUPS-MIB::xupsOutputVoltage.1 */
	{ "output.voltage", 0, 1.0, ".1.3.6.1.4.1.534.1.4.4.1.2.1", "", SU_OUTPUT_1, NULL },
	/* XUPS-MIB::xupsConfigOutputVoltage.0 */
	{ "output.voltage.nominal", 0, 1.0, "1.3.6.1.4.1.534.1.10.1.0", "", 0, NULL },
	/* XUPS-MIB::xupsConfigLowOutputVoltageLimit.0 */
	{ "output.voltage.low", 0, 1.0, ".1.3.6.1.4.1.534.1.10.6.0", "", 0, NULL },
	/* XUPS-MIB::xupsConfigHighOutputVoltageLimit.0 */
	{ "output.voltage.high", 0, 1.0, ".1.3.6.1.4.1.534.1.10.7.0", "", 0, NULL },
	{ "output.current", 0, 1.0, PW_OID_OUT_CURRENT ".1", "",
		SU_OUTPUT_1, NULL },
	{ "output.realpower", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		SU_OUTPUT_1, NULL },
	{ "output.L1-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.current", 0, 1.0, PW_OID_OUT_CURRENT ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 1.0, PW_OID_OUT_CURRENT ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 1.0, PW_OID_OUT_CURRENT ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.realpower", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.realpower", 0, 1.0, PW_OID_OUT_POWER ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.realpower", 0, 1.0, PW_OID_OUT_POWER ".3", "",
		SU_OUTPUT_3, NULL },
	/* FIXME: should better be output.Lx.load */
	{ "output.L1.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.voltage.nominal", 0, 1.0, PW_OID_CONF_OVOLTAGE, "",
		0, NULL },

	/* Input page */
	{ "input.phases", 0, 1.0, PW_OID_IN_LINES, "",
		0, NULL },
	{ "input.frequency", 0, 0.1, PW_OID_IN_FREQUENCY, "",
		0, NULL },
	{ "input.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".0", "",
		SU_INPUT_1, NULL },
	/* XUPS-MIB::xupsConfigInputVoltage.0 */
	{ "input.voltage.nominal", 0, 1.0, "1.3.6.1.4.1.534.1.10.2.0", "", 0, NULL },
	{ "input.current", 0, 0.1, PW_OID_IN_CURRENT ".0", "",
		SU_INPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".3", "",
		SU_INPUT_3, NULL },
	{ "input.L1.current", 0, 1.0, PW_OID_IN_CURRENT ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 1.0, PW_OID_IN_CURRENT ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 1.0, PW_OID_IN_CURRENT ".3", "",
		SU_INPUT_3, NULL },
	{ "input.L1.realpower", 0, 1.0, PW_OID_IN_POWER ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2.realpower", 0, 1.0, PW_OID_IN_POWER ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3.realpower", 0, 1.0, PW_OID_IN_POWER ".3", "",
		SU_INPUT_3, NULL },
	{ "input.quality", 0, 1.0, PW_OID_IN_LINE_BADS, "",
		0, NULL },

	/* FIXME: this segfaults! do we assume the same number of bypass phases as input phases?
	{ "input.bypass.phases", 0, 1.0, PW_OID_BY_LINES, "", 0, NULL }, */
	{ "input.bypass.frequency", 0, 0.1, PW_OID_BY_FREQUENCY, "", 0, NULL },
	{ "input.bypass.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".0", "",
		SU_INPUT_1, NULL },
	{ "input.bypass.L1-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".1", "",
		SU_INPUT_3, NULL },
	{ "input.bypass.L2-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".2", "",
		SU_INPUT_3, NULL },
	{ "input.bypass.L3-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".3", "",
		SU_INPUT_3, NULL },

	/* Ambient page */
	/* XUPS-MIB::xupsEnvRemoteTemp.0 */
	{ "ambient.temperature", 0, 1.0, "1.3.6.1.4.1.534.1.6.5.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteTempLowerLimit.0 */
	{ "ambient.temperature.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.9.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteTempUpperLimit.0 */
	{ "ambient.temperature.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.10.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteHumidity.0 */
	{ "ambient.humidity", 0, 1.0, "1.3.6.1.4.1.534.1.6.6.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteHumidityLowerLimit.0 */
	{ "ambient.humidity.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.11.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteHumidityUpperLimit.0 */
	{ "ambient.humidity.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.12.0", "", 0, NULL },

	/* instant commands */
	{ "test.battery.start.quick", 0, 1, PW_OID_BATTEST_START, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* Shed load and restart when line power back on; cannot be canceled */
	{ "shutdown.return", 0, DEFAULT_SHUTDOWNDELAY, PW_OID_CONT_LOAD_SHED_AND_RESTART, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* Cancel output off, by writing 0 to xupsControlOutputOffDelay */
	{ "shutdown.stop", 0, 0, PW_OID_CONT_OFFDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* load off after 1 sec, shortest possible delay; 0 cancels */
	{ "load.off", 0, 1, PW_OID_CONT_OFFDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "load.off.delay", 0, DEFAULT_OFFDELAY, PW_OID_CONT_OFFDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* load on after 1 sec, shortest possible delay; 0 cancels */
	{ "load.on", 0, 1, PW_OID_CONT_ONDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "load.on.delay", 0, DEFAULT_ONDELAY, PW_OID_CONT_ONDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },

	{ "ups.alarms", 0, 1.0, PW_OID_ALARMS, "",
		0, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;

static alarms_info_t pw_alarms[] = {
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


mib2nut_info_t	powerware = { "pw", PW_MIB_VERSION, NULL, PW_OID_MODEL_NAME, pw_mib, POWERWARE_SYSOID , pw_alarms };
mib2nut_info_t	pxgx_ups = { "pxgx_ups", PW_MIB_VERSION, NULL, PW_OID_MODEL_NAME, pw_mib, EATON_PXGX_SYSOID , pw_alarms };
