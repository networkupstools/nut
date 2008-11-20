/*  pwmib.h - data to monitor Powerware UPS with NUT
 *  (using MIBs described in stdupsv1.mib and Xups.mib)
 *
 *  Copyright (C) 2005-2006
 *  			Olli Savia <ops@iki.fi>
 *  			Niels Baggesen <niels@baggesen.net>
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

#define PW_MIB_VERSION "0.6.1"

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

#define PW_OID_AMBIENT_TEMP	"1.3.6.1.4.1.534.1.6.1.0"	/* XUPS-MIB::xupsEnvAmbientTemp.0 */
#define PW_OID_AMBIENT_LOW	"1.3.6.1.4.1.534.1.6.2.0"	/* XUPS-MIB::xupsEnvAmbientLowerLimit.0 */
#define PW_OID_AMBIENT_HIGH	"1.3.6.1.4.1.534.1.6.3.0"	/* XUPS-MIB::xupsEnvAmbientUpperLimit.0 */

#define PW_OID_BATTEST_START	"1.3.6.1.4.1.534.1.8.1"		/* XUPS-MIB::xupsTestBattery   set to startTest(1) to initiate test*/
#define PW_OID_BATTEST_RES	"1.3.6.1.4.1.534.1.8.2"		/* XUPS-MIB::xupsTestBatteryStatus */

#define PW_OID_CONT_OFFDELAY	"1.3.6.1.4.1.534.1.9.1"		/* XUPS-MIB::xupsControlOutputOffDelay */
#define PW_OID_CONT_ONDELAY	"1.3.6.1.4.1.534.1.9.2"		/* XUPS-MIB::xupsControlOutputOnDelay */
#define PW_OID_CONT_OFFT_DEL	"1.3.6.1.4.1.534.1.9.3"		/* XUPS-MIB::xupsControlOutputOffTrapDelay */
#define PW_OID_CONT_ONT_DEL	"1.3.6.1.4.1.534.1.9.4"		/* XUPS-MIB::xupsControlOutputOnTrapDelay */

#define PW_OID_CONF_OVOLTAGE	"1.3.6.1.4.1.534.1.10.1.0"	/* XUPS-MIB::xupsConfigOutputVoltage.0 */
#define PW_OID_CONF_IVOLTAGE	"1.3.6.1.4.1.534.1.10.2.0"	/* XUPS-MIB::xupsConfigInputVoltage.0 */
#define PW_OID_CONF_POWER	"1.3.6.1.4.1.534.1.10.3.0"	/* XUPS-MIB::xupsConfigOutputWatts.0 */
#define PW_OID_CONF_FREQ	"1.3.6.1.4.1.534.1.10.4.0"	/* XUPS-MIB::xupsConfigOutputFreq.0 */

#define PW_OID_ALARMS		"1.3.6.1.4.1.534.1.7.1"		/* XUPS-MIB::xupsAlarms */
#define PW_OID_ALARM_OB		"1.3.6.1.4.1.534.1.7.3"		/* XUPS-MIB::xupsOnBattery */

info_lkp_t pw_alarm_ob[] = {
	{ 1, "OB" },
	{ 0, "NULL" }
} ;

#define PW_OID_ALARM_LB		"1.3.6.1.4.1.534.1.7.4"		/* XUPS-MIB::xupsLowBattery */

info_lkp_t pw_alarm_lb[] = {
	{ 1, "LB" },
	{ 0, "NULL" }
} ;

info_lkp_t pw_pwr_info[] = {
	{ 1, ""		/* other */ },
	{ 2, "OFF"       /* none */ },
	{ 3, "OL"        /* normal */ },
	{ 4, "BYPASS"    /* bypass */ },
	{ 5, "OB"        /* battery */ },
	{ 6, "OL BOOST"  /* booster */ },
	{ 7, "OL TRIM"   /* reducer */ },
	{ 8, "OL"        /* parallel capacity */ },
	{ 9, "OL"        /* parallel redundancy */ },
	{10, "OL"        /* high efficiancy */ },
	{ 0, "NULL" }
};

info_lkp_t pw_mode_info[] = {
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

info_lkp_t pw_batt_info[] = {
	{ 1, "Battery Charging" }, 
	{ 2, "Battery Discharging" },
	{ 3, "Battery Floating" },	/* battery floating  - can we put that stuff somewhere so one actually access that information? */
	{ 4, "Battery Resting" },	/* battery resting   - could come handy if support asks what 
			   state the batteries are in... pw_batt_info doesn't get used */
	{ 5, "unknown" },	/* unknown */
	{ 0, "NULL" }
};

info_lkp_t pw_batt_test_info[] = {
	{ 1, "" },			/* unknown */
	{ 2, "Done and passed" },
	{ 3, "Done and error" }, 
	{ 4, "In progress" },
	{ 5, "Not supported" },
	{ 6, "Inhibited" },
	{ 7, "Scheduled" },
	{ 0, "NULL" }
};


/* Snmp2NUT lookup table */

snmp_info_t pw_mib[] = {
	/* UPS page */
	/* info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MFR_NAME, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MODEL_NAME, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_FIRMREV, "",
		SU_FLAG_STATIC,   NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_AGENTREV, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_IDENT, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.load", 0, 1.0, PW_OID_OUT_LOAD, "",
		SU_OUTPUT_1, NULL },
	{ "ups.power", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		0, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "OFF",
		SU_STATUS_PWR, &pw_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_BATT_STATUS, "",
		SU_STATUS_BATT, &ietf_batt_info[0] },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "",
		SU_FLAG_STATIC | SU_FLAG_OK, &pw_mode_info[0] },
	{ "ups.realpower.nominal", 0, 1.0, PW_OID_CONF_POWER, "",
		0, NULL },
	{ "ups.power.nominal", 0, 1.0, IETF_OID_CONF_OUT_VA, "",
		0, NULL },
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_BATTEST_RES, "",
		0, &pw_batt_test_info[0] },
	{ "vendor.specific.abmstatus", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_BATT_STATUS, "",
	  SU_STATUS_BATT, &pw_batt_info[0] },

	/* Battery page */
	{ "battery.charge", 0, 1.0, PW_OID_BATT_CHARGE, "",
		0, NULL },
	{ "battery.runtime", 0, 1.0, PW_OID_BATT_RUNTIME, "",
		0, NULL },
	{ "battery.voltage", 0, 1.0, PW_OID_BATT_VOLTAGE, "",
		0, NULL },
	{ "battery.current", 0, 0.1, PW_OID_BATT_CURRENT, "",
		0, NULL },
	{ "battery.runtime.low", ST_FLAG_RW, 1.0, IETF_OID_CONF_RUNTIME_LOW, "",
		0, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, PW_OID_OUT_LINES, "",
		SU_FLAG_SETINT, NULL, &output_phases },
	{ "output.frequency", 0, 0.1, PW_OID_OUT_FREQUENCY, "",
		0, NULL },
	{ "output.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".1", "",
		SU_OUTPUT_1, NULL },
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
	{ "output.L1.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.voltage.nominal", 0, 1.0, PW_OID_CONF_OVOLTAGE, "",
		0, NULL },
	{ "output.frequency.nominal", 0, 0.1, PW_OID_CONF_FREQ, "",
		0, NULL },

	/* Input page */
	{ "input.phases", 0, 1.0, PW_OID_IN_LINES, "",
		SU_FLAG_SETINT, NULL, &input_phases },
	{ "input.frequency", 0, 0.1, PW_OID_IN_FREQUENCY, "",
		0, NULL },
	{ "input.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".0", "",
		SU_INPUT_1, NULL },
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
	{ "input.voltage.nominal", 0, 1.0, PW_OID_CONF_IVOLTAGE, "",
		0, NULL },

	/* this segfaults? do we assume the same number of bypass phases as input phases?
	{ "input.bypass.phases", 0, 1.0, PW_OID_BY_LINES, "",
		SU_FLAG_SETINT, NULL }, */
	{ "input.bypass.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".0", "",
		SU_INPUT_1, NULL },
	{ "input.bypass.L1-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".1", "",
		SU_INPUT_3, NULL },
	{ "input.bypass.L2-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".2", "",
		SU_INPUT_3, NULL },
	{ "input.bypass.L3-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".3", "",
		SU_INPUT_3, NULL },

	/* Ambient page */
	{ "ambient.temperature", 0, 1.0, PW_OID_AMBIENT_TEMP, "",
		0, NULL },
	{ "ambient.temperature.low", 0, 1.0, PW_OID_AMBIENT_LOW, "",
		0, NULL },
	{ "ambient.temperature.high", 0, 1.0, PW_OID_AMBIENT_HIGH, "",
		0, NULL },

	/* instant commands */
	{ "test.battery.start.quick", 0, 1, PW_OID_BATTEST_START, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	 /* Cancel output off, by writing 0 to xupsControlOutputOffDelay */
	{ "shutdown.stop", 0, 0, PW_OID_CONT_OFFDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* load off after 1 sec, shortest possible delay */
	{ "load.off", 0, 1, PW_OID_CONT_OFFDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* load on after 1 sec, shortest possible delay */
	{ "load.on", 0, 1, PW_OID_CONT_ONDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;
