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

#define PW_MIB_VERSION "0.5"

/* SNMP OIDs set */
#define PW_OID_MFR_NAME "1.3.6.1.4.1.534.1.1.1.0" /* XUPS-MIB::xupsIdentManufacturer.0 */
#define PW_OID_MODEL_NAME "1.3.6.1.4.1.534.1.1.2.0" /* XUPS-MIB::xupsIdentModel.0 */
#define PW_OID_FIRMREV "1.3.6.1.4.1.534.1.1.3.0" /* XUPS-MIB::xupsIdentSoftwareVersion.0 */

#define PW_OID_BATT_STATUS "1.3.6.1.4.1.534.1.2.5.0" /* XUPS-MIB::xupsBatteryAbmStatus.0 */
#define PW_OID_BATT_RUNTIME "1.3.6.1.4.1.534.1.2.1.0" /* XUPS-MIB::xupsBatTimeRemaining.0 */
#define PW_OID_BATT_CHARGE "1.3.6.1.4.1.534.1.2.4.0" /* XUPS-MIB::xupsBatCapacity.0 */
#define PW_OID_BATT_VOLTAGE "1.3.6.1.4.1.534.1.2.2.0" /* XUPS-MIB::xupsBatVoltage.0 */
#define PW_OID_BATT_CURRENT "1.3.6.1.4.1.534.1.2.3.0" /* XUPS-MIB::xupsBatCurrent.0 */

#define PW_OID_IN_LINES "1.3.6.1.4.1.534.1.3.3.0" /* XUPS-MIB::xupsInputNumPhases.0 */

#define PW_OID_IN_FREQUENCY "1.3.6.1.4.1.534.1.3.1.0" /* XUPS-MIB::xupsInputFrequency.0 */
#define PW_OID_IN_VOLTAGE "1.3.6.1.4.1.534.1.3.4.1.2" /* XUPS-MIB::xupsInputVoltage */
#define PW_OID_IN_CURRENT "1.3.6.1.4.1.534.1.3.4.1.3" /* XUPS-MIB::xupsInputCurrent */
#define PW_OID_IN_POWER "1.3.6.1.4.1.534.1.3.4.1.4" /* XUPS-MIB::xupsInputWatts */

#define PW_OID_POWER_STATUS "1.3.6.1.4.1.534.1.4.5.0" /* XUPS-MIB::xupsOutputSource.0 */
#define PW_OID_OUT_FREQUENCY "1.3.6.1.4.1.534.1.4.2.0" /* XUPS-MIB::xupsOutputFrequency.0 */
#define PW_OID_OUT_LINES "1.3.6.1.4.1.534.1.4.3.0" /* XUPS-MIB::xupsOutputNumPhases.0 */

#define PW_OID_OUT_VOLTAGE "1.3.6.1.4.1.534.1.4.4.1.2" /* XUPS-MIB::xupsOutputVoltage */
#define PW_OID_OUT_CURRENT "1.3.6.1.4.1.534.1.4.4.1.3" /* XUPS-MIB::xupsOutputCurrent */
#define PW_OID_OUT_POWER "1.3.6.1.4.1.534.1.4.4.1.4" /* XUPS-MIB::xupsOutputWatts */

#define PW_OID_CONF_VOLTAGE "1.3.6.1.4.1.534.1.10.1.0" /* XUPS-MIB::xupsConfigOutputVoltage.0 */
#define PW_OID_AMBIENT_TEMP "1.3.6.1.4.1.534.1.6.1.0" /* XUPS-MIB::xupsEnvAmbientTemp.0 */


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
	{ "ups.load", 0, 1.0, IETF_OID_LOAD_LEVEL ".0", "",
		SU_OUTPUT_1, NULL },
	{ "ups.power", 0, 1.0, PW_OID_OUT_POWER ".0", "",
		0, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "OFF",
		SU_STATUS_PWR, &ietf_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_BATT_STATUS, "",
		SU_STATUS_BATT, &ietf_batt_info[0] },

	/* Battery page */
	{ "battery.charge", 0, 1.0, PW_OID_BATT_CHARGE, "",
		0, NULL },
	{ "battery.runtime", 0, 1.0, PW_OID_BATT_RUNTIME, "",
		0, NULL },
	{ "battery.voltage", 0, 1.0, PW_OID_BATT_VOLTAGE, "",
		0, NULL },
	{ "battery.current", 0, 0.1, PW_OID_BATT_CURRENT, "",
		0, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, PW_OID_OUT_LINES, "",
		SU_FLAG_SETINT, NULL, &output_phases },
	{ "output.frequency", 0, 0.1, PW_OID_OUT_FREQUENCY, "",
		0, NULL },
	{ "output.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".0", "",
		SU_OUTPUT_1, NULL },
	{ "output.current", 0, 0.1, PW_OID_OUT_CURRENT, "",
		SU_OUTPUT_1, NULL },
	{ "output.realpower", 0, 0.1, PW_OID_OUT_POWER ".1", "",
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
	{ "output.L1.realpower", 0, 0.1, PW_OID_OUT_POWER ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.realpower", 0, 0.1, PW_OID_OUT_POWER ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.realpower", 0, 0.1, PW_OID_OUT_POWER ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".3", "",
		SU_OUTPUT_3, NULL },

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
	{ "input.L1.realpower", 0, 0.1, PW_OID_IN_POWER ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2.realpower", 0, 0.1, PW_OID_IN_POWER ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3.realpower", 0, 0.1, PW_OID_IN_POWER ".3", "",
		SU_INPUT_3, NULL },

	/* Ambient page */
	{ "ambient.temperature", 0, 1.0, PW_OID_AMBIENT_TEMP, "",
		0, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;
