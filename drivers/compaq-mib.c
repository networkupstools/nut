/*  compaq-mib.c - data to monitor SNMP UPS with NUT
 *
 *  Copyright (C)
 *    2002-2012 Arnaud Quette <arnaud.quette@free.fr>
 *    2002-2006 Niels Baggesen <niels@baggesen.net>
 *    2002-2006 Philip Ward <p.g.ward@stir.ac.uk>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
 *
 *  This version has been tested using an HP R5500XR UPS with AF401A
 *  management card and a single phase input.
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

#define CPQPOWER_MIB_VERSION	"1.1"

/* SNMP OIDs set */
#define CPQPOWER_OID_UPS_MIB			"1.3.6.1.4.1.232.165.3"
#define CPQPOWER_SYSOID					CPQPOWER_OID_UPS_MIB

#define CPQPOWER_OID_MFR_NAME         CPQPOWER_OID_UPS_MIB ".1.1.0"	/* UPS-MIB::upsIdentManufacturer */
#define CPQPOWER_OID_MODEL_NAME       CPQPOWER_OID_UPS_MIB ".1.2.0"	/* UPS-MIB::upsIdentModel */
#define CPQPOWER_OID_FIRMREV          CPQPOWER_OID_UPS_MIB ".1.3.0"	/* UPS-MIB::upsIdentUPSSoftwareVersion */
#define CPQPOWER_OID_OEMCODE          CPQPOWER_OID_UPS_MIB ".1.4.0"	/* UPS-MIB::upsIdentAgentSoftwareVersion */

#define CPQPOWER_OID_BATT_RUNTIME     CPQPOWER_OID_UPS_MIB ".2.1.0"	/* UPS-MIB::upsEstimatedMinutesRemaining */
#define CPQPOWER_OID_BATT_VOLTAGE     CPQPOWER_OID_UPS_MIB ".2.2.0"	/* UPS-MIB::upsBatteryVoltage */
#define CPQPOWER_OID_BATT_CURRENT     CPQPOWER_OID_UPS_MIB ".2.3.0"	/* UPS-MIB::upsBatteryCurrent */
#define CPQPOWER_OID_BATT_CHARGE      CPQPOWER_OID_UPS_MIB ".2.4.0"	/* UPS-MIB::upsBattCapacity */
#define CPQPOWER_OID_BATT_STATUS      CPQPOWER_OID_UPS_MIB ".2.5.0"	/* UPS-MIB::upsBatteryAbmStatus */

#define CPQPOWER_OID_IN_FREQ          CPQPOWER_OID_UPS_MIB ".3.1.0"	/* UPS-MIB::upsInputFrequency */
#define CPQPOWER_OID_IN_LINEBADS      CPQPOWER_OID_UPS_MIB ".3.2.0"	/* UPS-MIB::upsInputLineBads */
#define CPQPOWER_OID_IN_LINES         CPQPOWER_OID_UPS_MIB ".3.3.0"	/* UPS-MIB::upsInputNumPhases */

#define CPQPOWER_OID_IN_PHASE         CPQPOWER_OID_UPS_MIB ".3.4.1.1"	/* UPS-MIB::upsInputPhase */
#define CPQPOWER_OID_IN_VOLTAGE       CPQPOWER_OID_UPS_MIB ".3.4.1.2"	/* UPS-MIB::upsInputVoltage */
#define CPQPOWER_OID_IN_CURRENT       CPQPOWER_OID_UPS_MIB ".3.4.1.3"	/* UPS-MIB::upsInputCurrent */
#define CPQPOWER_OID_IN_POWER         CPQPOWER_OID_UPS_MIB ".3.4.1.4"	/* UPS-MIB::upsInputWatts */

#define CPQPOWER_OID_LOAD_LEVEL       CPQPOWER_OID_UPS_MIB ".4.1.0"     /* UPS-MIB::upsOutputLoad */
#define CPQPOWER_OID_OUT_FREQUENCY    CPQPOWER_OID_UPS_MIB ".4.2.0"	/* UPS-MIB::upsOutputFrequency */
#define CPQPOWER_OID_OUT_LINES        CPQPOWER_OID_UPS_MIB ".4.3.0"	/* UPS-MIB::upsOutputNumPhases */

#define CPQPOWER_OID_OUT_PHASE        CPQPOWER_OID_UPS_MIB ".4.4.1.1"	/* UPS-MIB::upsOutputPhase */
#define CPQPOWER_OID_OUT_VOLTAGE      CPQPOWER_OID_UPS_MIB ".4.4.1.2"	/* UPS-MIB::upsOutputVoltage */
#define CPQPOWER_OID_OUT_CURRENT      CPQPOWER_OID_UPS_MIB ".4.4.1.3"	/* UPS-MIB::upsOutputCurrent */
#define CPQPOWER_OID_OUT_POWER        CPQPOWER_OID_UPS_MIB ".4.4.1.4"	/* UPS-MIB::upsOutputWatts */

#define CPQPOWER_OID_POWER_STATUS     CPQPOWER_OID_UPS_MIB ".4.5.0"	/* UPS-MIB::upsOutputSource */

#define CPQPOWER_OID_AMBIENT_TEMP     CPQPOWER_OID_UPS_MIB ".6.1.0"     /* UPS-MIB::upsEnvAmbientTemp */

#define CPQPOWER_OID_UPS_TEST_BATT    CPQPOWER_OID_UPS_MIB ".7.1.0"     /* UPS-MIB::upsTestBattery */
#define CPQPOWER_OID_UPS_TEST_RES     CPQPOWER_OID_UPS_MIB ".7.2.0"     /* UPS-MIB::upsTestBatteryStatus */
#define CPQPOWER_OID_ALARM_OB         CPQPOWER_OID_UPS_MIB ".7.3.0"     /* UPS-MIB::upsOnBattery */
#define CPQPOWER_OID_ALARM_LB         CPQPOWER_OID_UPS_MIB ".7.4.0"     /* UPS-MIB::upsLowBattery */

static info_lkp_t cpqpower_alarm_ob[] = {
	{ 1, "OB" },
	{ 0, "NULL" }
} ;

static info_lkp_t cpqpower_alarm_lb[] = {
	{ 1, "LB" },
	{ 0, "NULL" }
} ;

/* Defines for CPQPOWER_OID_POWER_STATUS (1) */
static info_lkp_t cpqpower_pwr_info[] = {
	{ 1, ""       /* other */ },
	{ 2, "OFF"    /* none */ },
	{ 3, "OL"     /* normal */ },
	{ 4, "OL BYPASS" /* bypass */ },
	{ 5, "OB"     /* battery */ },
	{ 6, "OL BOOST"  /* booster */ },
	{ 7, "OL TRIM"   /* reducer */ },
	{ 8, "OL"   /* parallelCapacity */ },
	{ 9, "OL"   /* parallelRedundant */ },
	{ 10, "OL" /* HighEfficiencyMode */ },
	{ 0, "NULL" }
} ;

static info_lkp_t cpqpower_mode_info[] = {
	{ 1, ""  },
	{ 2, ""  },
	{ 3, "normal" },
	{ 4, "" },
	{ 5, "" },
	{ 6, "" },
	{ 7, "" },
	{ 8, "parallel capacity" },
	{ 9, "parallel redundancy" },
	{10, "high efficiency" },
	{ 0, "NULL" }
};

static info_lkp_t cpqpower_battery_abm_status[] = {
	{ 1, "CHRG" },
	{ 2, "DISCHRG" },
/*	{ 3, "Floating" }, */
/*	{ 4, "Resting" }, */
/*	{ 5, "Unknown" }, */
	{ 0, "NULL" }
} ;

/* Defines for CPQPOWER_OID_UPS_TEST_RES */
static info_lkp_t cpqpower_test_res_info[] = {
	{ 1, "Unknown" },
	{ 2, "Done and passed" },
	{ 3, "Done and error" },
	{ 4, "In progress" },
	{ 5, "Not supported" },
	{ 6, "Inhibited" },
	{ 7, "Scheduled" },
	{ 0, "NULL" }
} ;

#define CPQPOWER_OID_SD_AFTER_DELAY	 CPQPOWER_OID_UPS_MIB ".8.1.0"	/* UPS-MIB::upsShutdownAfterDelay */
#define CPQPOWER_OFF_DO		0

/* Snmp2NUT lookup table */

static snmp_info_t cpqpower_mib[] = {
	/* UPS page */
	/* info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_MFR_NAME, "HP/Compaq", SU_FLAG_STATIC, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_MODEL_NAME, "SNMP UPS", SU_FLAG_STATIC, NULL },
	{ "ups.model.aux", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_OEMCODE, "", SU_FLAG_STATIC, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_FIRMREV, "", SU_FLAG_STATIC, NULL },
	{ "ups.load", 0, 1.0, CPQPOWER_OID_LOAD_LEVEL, "", 0, NULL },
	{ "ups.realpower", 0, 1.0, CPQPOWER_OID_OUT_POWER, "", SU_OUTPUT_1, NULL },
	{ "ups.L1.realpower", 0, 0.1, CPQPOWER_OID_OUT_POWER ".1", "", SU_OUTPUT_3, NULL },
	{ "ups.L2.realpower", 0, 0.1, CPQPOWER_OID_OUT_POWER ".2", "", SU_OUTPUT_3, NULL },
	{ "ups.L3.realpower", 0, 0.1, CPQPOWER_OID_OUT_POWER ".3", "", SU_OUTPUT_3, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_POWER_STATUS, "OFF", SU_STATUS_PWR, cpqpower_pwr_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_BATT_STATUS, "", SU_STATUS_PWR, cpqpower_battery_abm_status },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_ALARM_OB, "", SU_STATUS_BATT, cpqpower_alarm_ob },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_ALARM_LB, "", SU_STATUS_BATT, cpqpower_alarm_lb },
/*	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_BATT_STATUS, "", SU_STATUS_BATT, ietf_batt_info }, */
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_POWER_STATUS, "", SU_STATUS_PWR, cpqpower_mode_info },
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, CPQPOWER_OID_UPS_TEST_RES, "", 0, cpqpower_test_res_info },

	/* Ambient page */
	{ "ambient.temperature", 0, 1.0, CPQPOWER_OID_AMBIENT_TEMP, "", 0, NULL },

	/* Battery page */
	{ "battery.charge", 0, 1.0, CPQPOWER_OID_BATT_CHARGE, "", 0, NULL },
	{ "battery.runtime", 0, 60.0, CPQPOWER_OID_BATT_RUNTIME, "", 0, NULL },
	{ "battery.voltage", 0, 0.1, CPQPOWER_OID_BATT_VOLTAGE, "", 0, NULL },
	{ "battery.current", 0, 0.1, CPQPOWER_OID_BATT_CURRENT, "", 0, NULL },

	/* Input page */
	{ "input.phases", 0, 1.0, CPQPOWER_OID_IN_LINES, "", SU_FLAG_SETINT, NULL, &input_phases },
/*	{ "input.phase", 0, 1.0, CPQPOWER_OID_IN_PHASE, "", SU_OUTPUT_1, NULL }, */
	{ "input.frequency", 0, 0.1, CPQPOWER_OID_IN_FREQ , "", 0, NULL },
	{ "input.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE, "", SU_OUTPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE ".1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE ".2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 1.0, CPQPOWER_OID_IN_VOLTAGE ".3", "", SU_INPUT_3, NULL },
	{ "input.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT, "", SU_OUTPUT_1, NULL },
	{ "input.L1.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT ".1", "", SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT ".2", "", SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 0.1, CPQPOWER_OID_IN_CURRENT ".3", "", SU_INPUT_3, NULL },
	{ "input.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER, "", SU_OUTPUT_1, NULL },
	{ "input.L1.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER ".1", "", SU_INPUT_3, NULL },
	{ "input.L2.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER ".2", "", SU_INPUT_3, NULL },
	{ "input.L3.realpower", 0, 0.1, CPQPOWER_OID_IN_POWER ".3", "", SU_INPUT_3, NULL },
	{ "input.quality", 0, 1.0, CPQPOWER_OID_IN_LINEBADS, "", 0, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, CPQPOWER_OID_OUT_LINES, "", SU_FLAG_SETINT, NULL, &output_phases },
/*	{ "output.phase", 0, 1.0, CPQPOWER_OID_OUT_PHASE, "", SU_OUTPUT_1, NULL }, */
	{ "output.frequency", 0, 0.1, CPQPOWER_OID_OUT_FREQUENCY, "", 0, NULL },
	{ "output.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE, "", SU_OUTPUT_1, NULL },
	{ "output.L1-N.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE ".1", "", SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE ".2", "", SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 1.0, CPQPOWER_OID_OUT_VOLTAGE ".3", "", SU_OUTPUT_3, NULL },
	{ "output.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT, "", SU_OUTPUT_1, NULL },
	{ "output.L1.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT ".1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT ".2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 0.1, CPQPOWER_OID_OUT_CURRENT ".3", "", SU_OUTPUT_3, NULL },

	/* instant commands. */
	{ "load.off", 0, CPQPOWER_OFF_DO, CPQPOWER_OID_SD_AFTER_DELAY, "", SU_TYPE_CMD, NULL },
/*	{ CMD_SHUTDOWN, 0, CPQPOWER_OFF_GRACEFUL, CPQPOWER_OID_OFF, "", 0, NULL }, */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	compaq = { "cpqpower", CPQPOWER_MIB_VERSION, "", CPQPOWER_OID_MFR_NAME, cpqpower_mib, CPQPOWER_SYSOID };
