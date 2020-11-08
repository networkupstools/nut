/*  netvision-mib.c - data to monitor Socomec Sicon UPS equipped
 *  with Netvision WEB/SNMP card/external box with NUT
 *
 *  Copyright (C)
 *  	2004	Thanos Chatziathanassiou <tchatzi@arx.net>
 *  	2012	Manuel Bouyer <bouyer@NetBSD.org>
 * 		2015	Arnaud Quette <arnaud.quette@gmail.com>
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

#include "netvision-mib.h"

#define NETVISION_MIB_VERSION			"0.42"

#define NETVISION_SYSOID				".1.3.6.1.4.1.4555.1.1.1"

/* SNMP OIDs set */
#define NETVISION_OID_UPS_MIB			".1.3.6.1.4.1.4555.1.1.1.1"
#define NETVISION_OID_UPSIDENTMODEL		".1.3.6.1.4.1.4555.1.1.1.1.1.1.0"
#define NETVISION_OID_UPSIDENTFWVERSION		".1.3.6.1.4.1.4555.1.1.1.1.1.2.0"
#define NETVISION_OID_UPSIDENTAGENTSWVERSION	".1.3.6.1.4.1.4555.1.1.1.1.1.3.0"
#define NETVISION_OID_UPSIDENTUPSSERIALNUMBER	".1.3.6.1.4.1.4555.1.1.1.1.1.4.0"

/* UPS Battery */
#define NETVISION_OID_BATTERYSTATUS		".1.3.6.1.4.1.4555.1.1.1.1.2.1.0"
static info_lkp_t netvision_batt_info[] = {
	{ 2, "", NULL, NULL },        /* battery normal      */
	{ 3, "LB", NULL, NULL },      /* battery low         */
	{ 4, "LB", NULL, NULL },      /* battery depleted    */
	{ 5, "DISCHRG", NULL, NULL }, /* battery discharging */
	{ 6, "RB", NULL, NULL },      /* battery failure     */
	{ 0, NULL, NULL, NULL }
};

/* Battery status: upsAlarmOnBattery */
static info_lkp_t netvision_onbatt_info[] = {
	{ 0, "OL", NULL, NULL },      /* Online      */
	{ 1, "OB", NULL, NULL },      /* On battery  */
	{ 0, NULL, NULL, NULL }
};

#define NETVISION_OID_SECONDSONBATTERY		".1.3.6.1.4.1.4555.1.1.1.1.2.2.0"
#define NETVISION_OID_BATT_RUNTIME_REMAINING	".1.3.6.1.4.1.4555.1.1.1.1.2.3.0"
#define NETVISION_OID_BATT_CHARGE		".1.3.6.1.4.1.4555.1.1.1.1.2.4.0"
#define NETVISION_OID_BATT_VOLTS		".1.3.6.1.4.1.4555.1.1.1.1.2.5.0"

#define NETVISION_OID_INPUT_NUM_LINES		".1.3.6.1.4.1.4555.1.1.1.1.3.1.0" /* 1phase or 3phase UPS input */
#define NETVISION_OID_INPUT_FREQ		".1.3.6.1.4.1.4555.1.1.1.1.3.2.0"
#define NETVISION_OID_OUTPUT_NUM_LINES		".1.3.6.1.4.1.4555.1.1.1.1.4.3.0" /* 1phase or 3phase UPS output */
#define NETVISION_OID_OUTPUT_FREQ		".1.3.6.1.4.1.4555.1.1.1.1.4.2.0"

#define NETVISION_OID_BYPASS_FREQ		".1.3.6.1.4.1.4555.1.1.1.1.5.1.0"
#define NETVISION_OID_BYPASS_NUM_LINES		".1.3.6.1.4.1.4555.1.1.1.1.5.2.0" /* 1phase or 3phase UPS input */
/*
	three phase ups provide input/output/load for each phase
	in case of one-phase output, only _P1 should be used
*/

#define NETVISION_OID_OUT_VOLTAGE_P1	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.2.1"
#define NETVISION_OID_OUT_CURRENT_P1	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.3.1"
#define NETVISION_OID_OUT_LOAD_PCT_P1	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.4.1"
#define NETVISION_OID_IN_VOLTAGE_P1	".1.3.6.1.4.1.4555.1.1.1.1.3.3.1.5.1"
#define NETVISION_OID_IN_CURRENT_P1	".1.3.6.1.4.1.4555.1.1.1.1.3.3.1.3.1"
#define NETVISION_OID_BY_VOLTAGE_P1	".1.3.6.1.4.1.4555.1.1.1.1.5.3.1.2.1"
#define NETVISION_OID_BY_CURRENT_P1	".1.3.6.1.4.1.4555.1.1.1.1.5.3.1.3.1"

#define NETVISION_OID_OUT_VOLTAGE_P2	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.2.2"
#define NETVISION_OID_OUT_CURRENT_P2	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.3.2"
#define NETVISION_OID_OUT_LOAD_PCT_P2	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.4.2"
#define NETVISION_OID_IN_VOLTAGE_P2	".1.3.6.1.4.1.4555.1.1.1.1.3.3.1.5.2"
#define NETVISION_OID_IN_CURRENT_P2	".1.3.6.1.4.1.4555.1.1.1.1.3.3.1.3.2"
#define NETVISION_OID_BY_VOLTAGE_P2	".1.3.6.1.4.1.4555.1.1.1.1.5.3.1.2.2"
#define NETVISION_OID_BY_CURRENT_P2	".1.3.6.1.4.1.4555.1.1.1.1.5.3.1.3.2"

#define NETVISION_OID_OUT_VOLTAGE_P3	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.2.3"
#define NETVISION_OID_OUT_CURRENT_P3	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.3.3"
#define NETVISION_OID_OUT_LOAD_PCT_P3	".1.3.6.1.4.1.4555.1.1.1.1.4.4.1.4.3"
#define NETVISION_OID_IN_VOLTAGE_P3	".1.3.6.1.4.1.4555.1.1.1.1.3.3.1.5.3"
#define NETVISION_OID_IN_CURRENT_P3	".1.3.6.1.4.1.4555.1.1.1.1.3.3.1.3.3"
#define NETVISION_OID_BY_VOLTAGE_P3	".1.3.6.1.4.1.4555.1.1.1.1.5.3.1.2.3"
#define NETVISION_OID_BY_CURRENT_P3	".1.3.6.1.4.1.4555.1.1.1.1.5.3.1.3.3"

#define NETVISION_OID_OUTPUT_SOURCE	".1.3.6.1.4.1.4555.1.1.1.1.4.1.0"

#define NETVISION_OID_CONTROL_STATUS 	     ".1.3.6.1.4.1.4555.1.1.1.1.8.1"
#define NETVISION_OID_CONTROL_SHUTDOWN_DELAY ".1.3.6.1.4.1.4555.1.1.1.1.8.2"

static info_lkp_t netvision_output_info[] = {
	{ 1, "", NULL, NULL },          /* output source other   */
	{ 2, "", NULL, NULL },          /* output source none    */
	{ 3, "OL", NULL, NULL },        /* output source normal  */
	{ 4, "OL BYPASS", NULL, NULL }, /* output source bypass  */
	{ 5, "OB", NULL, NULL },        /* output source battery */
	{ 6, "OL BOOST", NULL, NULL },  /* output source booster */
	{ 7, "OL TRIM", NULL, NULL },   /* output source reducer */
	{ 8, "OL", NULL, NULL },        /* output source standby */
	{ 9, "", NULL, NULL },          /* output source ecomode */
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table */
static snmp_info_t netvision_mib[] = {
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTAGENTSWVERSION, "SOCOMEC SICON UPS",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTMODEL,
		"Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTUPSSERIALNUMBER, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTFWVERSION, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_BATTERYSTATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, &netvision_batt_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_OUTPUT_SOURCE, "",
		SU_FLAG_OK | SU_STATUS_PWR, &netvision_output_info[0] },
	/* upsAlarmOnBattery */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4555.1.1.1.1.6.3.2.0", "",
		SU_FLAG_OK | SU_STATUS_PWR, &netvision_onbatt_info[0] },

	/* ups load */
	{ "ups.load", 0, 1, NETVISION_OID_OUT_LOAD_PCT_P1, 0, SU_INPUT_1, NULL },

	/*ups input,output voltage, output frquency phase 1 */
	{ "input.phases", 0, 1.0, NETVISION_OID_INPUT_NUM_LINES, NULL, 0, NULL },
	{ "input.frequency", 0, 0.1, NETVISION_OID_INPUT_FREQ, NULL, SU_FLAG_OK, NULL },
	{ "input.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P1, NULL, SU_INPUT_1, NULL },
	{ "input.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P1, NULL, SU_INPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P1, NULL, SU_INPUT_3, NULL },
	{ "input.L1.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P1, NULL, SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P2, NULL, SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P2, NULL, SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P3, NULL, SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P3, NULL, SU_INPUT_3, NULL },

	{ "output.phases", 0, 1.0, NETVISION_OID_OUTPUT_NUM_LINES, NULL, 0, NULL },
	{ "output.frequency", 0, 0.1, NETVISION_OID_OUTPUT_FREQ, NULL, SU_FLAG_OK, NULL },
	{ "output.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P1, NULL, SU_OUTPUT_1, NULL },
	{ "output.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P1, NULL, SU_OUTPUT_1, NULL },
	{ "output.load", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P1, NULL, SU_OUTPUT_1, NULL },
	{ "output.L1-N.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P1, NULL, SU_OUTPUT_3, NULL },
	{ "output.L1.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P1, NULL, SU_OUTPUT_3, NULL },
	{ "output.L1.power.percent", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P1, NULL, SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P2, NULL, SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P2, NULL, SU_OUTPUT_3, NULL },
	{ "output.L2.power.percent", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P2, NULL, SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P3, NULL, SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P3, NULL, SU_OUTPUT_3, NULL },
	{ "output.L3.power.percent", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P3, NULL, SU_OUTPUT_3, NULL },

	{ "input.bypass.phases", 0, 1.0, NETVISION_OID_BYPASS_NUM_LINES, NULL, 0, NULL },
	{ "input.bypass.frequency", 0, 0.1, NETVISION_OID_BYPASS_FREQ, NULL, SU_FLAG_OK, NULL },
	{ "input.bypass.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P1, NULL, SU_BYPASS_1, NULL },
	{ "input.bypass.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P1, NULL, SU_BYPASS_1, NULL },
	{ "input.bypass.L1-N.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P1, NULL, SU_BYPASS_3, NULL },
	{ "input.bypass.L1.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P1, NULL, SU_BYPASS_3, NULL },
	{ "input.bypass.L2-N.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P2, NULL, SU_BYPASS_3, NULL },
	{ "input.bypass.L2.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P2, NULL, SU_BYPASS_3, NULL },
	{ "input.bypass.L3-N.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P3, NULL, SU_BYPASS_3, NULL },
	{ "input.bypass.L3.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P3, NULL, SU_BYPASS_3, NULL },

	/* battery info */
	{ "battery.charge", 0, 1, NETVISION_OID_BATT_CHARGE, "", SU_FLAG_OK, NULL },
	{ "battery.voltage", 0, 0.1, NETVISION_OID_BATT_VOLTS, "", SU_FLAG_OK, NULL },
	{ "battery.runtime", 0, 60, NETVISION_OID_BATT_RUNTIME_REMAINING, "", SU_FLAG_OK, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	netvision = { "netvision", NETVISION_MIB_VERSION, NULL, NETVISION_OID_UPSIDENTMODEL, netvision_mib, NETVISION_SYSOID };
