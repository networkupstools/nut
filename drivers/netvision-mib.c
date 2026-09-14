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

#define NETVISION_MIB_VERSION			"0.45"

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
	info_lkp_default(2, ""),	/* battery normal      */
	info_lkp_default(3, "LB"),	/* battery low         */
	info_lkp_default(4, "LB"),	/* battery depleted    */
	info_lkp_default(5, "DISCHRG"),	/* battery discharging */
	info_lkp_default(6, "RB"),	/* battery failure     */
	info_lkp_sentinel
};

/* Battery status: upsAlarmOnBattery */
static info_lkp_t netvision_onbatt_info[] = {
	info_lkp_default(0, "OL"),	/* Online      */
	info_lkp_default(1, "OB"),	/* On battery  */
	info_lkp_sentinel
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

/*
 * some of the output sources below are set to empty string; because we
 * don't know from here if we are online or on batteries.
 * In this case upsAlarmOnBattery will set the appropriate status.
 */
static info_lkp_t netvision_output_info[] = {
#if 0
	/* For reference: from the times before Git until SVN, this mapping
	 * was defined as stashed away in this block of code. It was wrong
	 * at least for MASTERYS 3/3 SYSTEM 60 kVA UPS devices described in
	 * pull request https://github.com/networkupstools/nut/pull/2803
	 */
	info_lkp_default(1, ""),	/* output source other   */
	info_lkp_default(2, ""),	/* output source none    */
	info_lkp_default(3, "OL"),	/* output source normal  */
	info_lkp_default(4, "OL BYPASS"),	/* output source bypass  */
	info_lkp_default(5, "OB"),	/* output source battery */
	info_lkp_default(6, "OL BOOST"),	/* output source booster */
	info_lkp_default(7, "OL TRIM"),	/* output source reducer */
	info_lkp_default(8, "OL"),	/* output source standby */
	info_lkp_default(9, ""),	/* output source ecomode */
#else
	info_lkp_default(1, ""),	/* output source unknown   */
	info_lkp_default(2, ""),	/* output source inverter  */
	info_lkp_default(3, "OL"),	/* output source mains     */
	info_lkp_default(4, ""),	/* output source ecomode   */
	info_lkp_default(5, "OL BYPASS"), /* output source bypass  */
	info_lkp_default(6, "OFF"),	/* output source standby   */
	info_lkp_default(7, "OL BYPASS"), /* output source maintenance bypass */
	info_lkp_default(8, "OFF"),	/* output source off       */
	info_lkp_default(9, ""),	/* output source normal    */
#endif
	info_lkp_sentinel
};

/* Snmp2NUT lookup table */
static snmp_info_t netvision_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTAGENTSWVERSION, "SOCOMEC SICON UPS",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTMODEL,
		"Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTUPSSERIALNUMBER, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_UPSIDENTFWVERSION, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_BATTERYSTATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, &netvision_batt_info[0]),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, NETVISION_OID_OUTPUT_SOURCE, "",
		SU_FLAG_OK | SU_STATUS_PWR, &netvision_output_info[0]),
	/* upsAlarmOnBattery */
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4555.1.1.1.1.6.3.2.0", "",
		SU_FLAG_OK | SU_STATUS_PWR, &netvision_onbatt_info[0]),

	/* ups load */
	snmp_info_default("ups.load", 0, 1, NETVISION_OID_OUT_LOAD_PCT_P1, 0, SU_INPUT_1, NULL),

	/*ups input,output voltage, output frquency phase 1 */
	snmp_info_default("input.phases", 0, 1.0, NETVISION_OID_INPUT_NUM_LINES, NULL, 0, NULL),
	snmp_info_default("input.frequency", 0, 0.1, NETVISION_OID_INPUT_FREQ, NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P1, NULL, SU_INPUT_1, NULL),
	snmp_info_default("input.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P1, NULL, SU_INPUT_1, NULL),
	snmp_info_default("input.L1-N.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P1, NULL, SU_INPUT_3, NULL),
	snmp_info_default("input.L1.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P1, NULL, SU_INPUT_3, NULL),
	snmp_info_default("input.L2-N.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P2, NULL, SU_INPUT_3, NULL),
	snmp_info_default("input.L2.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P2, NULL, SU_INPUT_3, NULL),
	snmp_info_default("input.L3-N.voltage", 0, 0.1, NETVISION_OID_IN_VOLTAGE_P3, NULL, SU_INPUT_3, NULL),
	snmp_info_default("input.L3.current", 0, 0.1, NETVISION_OID_IN_CURRENT_P3, NULL, SU_INPUT_3, NULL),

	snmp_info_default("output.phases", 0, 1.0, NETVISION_OID_OUTPUT_NUM_LINES, NULL, 0, NULL),
	snmp_info_default("output.frequency", 0, 0.1, NETVISION_OID_OUTPUT_FREQ, NULL, SU_FLAG_OK, NULL),
	snmp_info_default("output.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P1, NULL, SU_OUTPUT_1, NULL),
	snmp_info_default("output.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P1, NULL, SU_OUTPUT_1, NULL),
	snmp_info_default("output.load", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P1, NULL, SU_OUTPUT_1, NULL),
	snmp_info_default("output.L1-N.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P1, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L1.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P1, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L1.power.percent", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P1, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2-N.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P2, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P2, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.power.percent", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P2, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3-N.voltage", 0, 0.1, NETVISION_OID_OUT_VOLTAGE_P3, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.current", 0, 0.1, NETVISION_OID_OUT_CURRENT_P3, NULL, SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.power.percent", 0, 1.0, NETVISION_OID_OUT_LOAD_PCT_P3, NULL, SU_OUTPUT_3, NULL),

	snmp_info_default("input.bypass.phases", 0, 1.0, NETVISION_OID_BYPASS_NUM_LINES, NULL, 0, NULL),
	snmp_info_default("input.bypass.frequency", 0, 0.1, NETVISION_OID_BYPASS_FREQ, NULL, SU_FLAG_OK, NULL),
	snmp_info_default("input.bypass.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P1, NULL, SU_BYPASS_1, NULL),
	snmp_info_default("input.bypass.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P1, NULL, SU_BYPASS_1, NULL),
	snmp_info_default("input.bypass.L1-N.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P1, NULL, SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L1.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P1, NULL, SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L2-N.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P2, NULL, SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L2.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P2, NULL, SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L3-N.voltage", 0, 0.1, NETVISION_OID_BY_VOLTAGE_P3, NULL, SU_BYPASS_3, NULL),
	snmp_info_default("input.bypass.L3.current", 0, 0.1, NETVISION_OID_BY_CURRENT_P3, NULL, SU_BYPASS_3, NULL),

	/* battery info */
	snmp_info_default("battery.charge", 0, 1, NETVISION_OID_BATT_CHARGE, "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.voltage", 0, 0.1, NETVISION_OID_BATT_VOLTS, "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.runtime", 0, 60, NETVISION_OID_BATT_RUNTIME_REMAINING, "", SU_FLAG_OK, NULL),

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	netvision = { "netvision", NETVISION_MIB_VERSION, NULL, NETVISION_OID_UPSIDENTMODEL, netvision_mib, NETVISION_SYSOID, NULL };
