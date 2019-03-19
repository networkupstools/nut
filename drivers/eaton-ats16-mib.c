/* eaton_ats16-mib.c - subdriver to monitor Eaton ATS16 SNMP devices with NUT
 *
 *  Copyright (C)
 *    2011-2012 Arnaud Quette <arnaud.quette@free.fr>
 *    2016-2019 Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-snmp-subdriver script. It must be customized!
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "eaton-ats16-mib.h"

#define EATON_ATS16_MIB_VERSION  "0.16"

#define EATON_ATS16_SYSOID       ".1.3.6.1.4.1.534.10"
#define EATON_ATS16_MODEL        ".1.3.6.1.4.1.534.10.2.1.2.0"

static info_lkp_t eaton_ats16_source_info[] = {
	{ 1, "init" },
	{ 2, "diagnosis" },
	{ 3, "off" },
	{ 4, "1" },
	{ 5, "2" },
	{ 6, "safe" },
	{ 7, "fault" },
	{ 0, NULL }
};

static info_lkp_t eaton_ats16_sensitivity_info[] = {
	{ 1, "normal" },
	{ 2, "high" },
	{ 3, "low" },
	{ 0, NULL }
};

static info_lkp_t eaton_ats16_input_frequency_status_info[] = {
	{ 1, "good" },          /* No threshold triggered */
	{ 2, "out-of-range" },  /* Frequency out of range triggered */
	{ 0, NULL }
};

static info_lkp_t eaton_ats16_input_voltage_status_info[] = {
	{ 1, "good" },          /* No threshold triggered */
	{ 2, "derated-range" }, /* Voltage derated */
	{ 3, "out-of-range" },  /* Voltage out of range triggered */
	{ 4, "unknown" },       /* "missing" */
	{ 0, NULL }
};

static info_lkp_t eaton_ats16_test_result_info[] = {
	{ 1, "done and passed" },
	{ 2, "done and warning" },
	{ 3, "done and error" },
	{ 4, "aborted" },
	{ 5, "in progress" },
	{ 6, "no test initiated" },
	{ 0, NULL }
};

static info_lkp_t eaton_ats16_output_status_info[] = {
	{ 1, "OFF" }, /* Output not powered */
	{ 2, "OL" },  /* Output powered */
	{ 0, NULL }
};

/* EATON_ATS Snmp2NUT lookup table */
static snmp_info_t eaton_ats16_mib[] = {

	/* Device collection */
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ats", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* ats2IdentManufacturer.0 = STRING: EATON */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.1.0", "Eaton", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2IdentModel.0 = STRING: Eaton ATS */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.2.0", "ATS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* FIXME: RFC for device.firmware! */
	/* FIXME: the 2 "firmware" entries below should be SU_FLAG_SEMI_STATIC */
	/* ats2IdentFWVersion.0 = STRING: 00.00.0009 */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* FIXME: RFC for device.firmware.aux! */
	/* ats2IdentRelease.0 = STRING: JF */
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.4.0", NULL, SU_FLAG_OK, NULL },
	/* ats2IdentSerialNumber.0 = STRING: GA04F23009 */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2IdentPartNumber.0 = STRING: EATS16N */
	{ "device.part", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.6.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2IdentAgentVersion.0 = STRING: 301F23C28 */
	/* { "unmapped.ats2IdentAgentVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.7.0", NULL, SU_FLAG_OK, NULL, NULL }, */
	/* ats2InputDephasing.0 = INTEGER: 2 degrees */
	/* { "unmapped.ats2InputDephasing", 0, 1, ".1.3.6.1.4.1.534.10.2.2.1.1.0", NULL, SU_FLAG_OK, NULL, NULL }, */

	/* Input collection */
	/* ats2InputIndex.source1 = INTEGER: source1(1) */
	{ "input.1.id", 0, 1, ".1.3.6.1.4.1.534.10.2.2.2.1.1.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2InputIndex.source2 = INTEGER: source2(2) */
	{ "input.2.id", 0, 1, ".1.3.6.1.4.1.534.10.2.2.2.1.1.2", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2InputVoltage.source1 = INTEGER: 2292 0.1 V */
	{ "input.1.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputVoltage.source2 = INTEGER: 2432 0.1 V */
	{ "input.2.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusVoltage.source1 = INTEGER: normalRange(1) */
	{ "input.1.voltage.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.5.1", NULL, SU_FLAG_OK, eaton_ats16_input_voltage_status_info },
	/* ats2InputStatusVoltage.source2 = INTEGER: normalRange(1) */
	{ "input.2.voltage.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.5.2", NULL, SU_FLAG_OK, eaton_ats16_input_voltage_status_info },
	/* ats2InputFrequency.source1 = INTEGER: 500 0.1 Hz */
	{ "input.1.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputFrequency.source2 = INTEGER: 500 0.1 Hz */
	{ "input.2.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusFrequency.source1 = INTEGER: good(1) */
	{ "input.1.frequency.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.2.1", NULL, SU_FLAG_OK, eaton_ats16_input_frequency_status_info },
	/* ats2InputStatusFrequency.source2 = INTEGER: good(1) */
	{ "input.2.frequency.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.2.2", NULL, SU_FLAG_OK, eaton_ats16_input_frequency_status_info },
	/* ats2ConfigSensitivity.0 = INTEGER: normal(1) */
	{ "input.sensitivity", ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.6.0", NULL, SU_FLAG_OK, &eaton_ats16_sensitivity_info[0] },
	/* ats2OperationMode.0 = INTEGER: source1(4) */
	{ "input.source", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.2.4.0", NULL, SU_FLAG_OK, eaton_ats16_source_info },
	/* ats2ConfigPreferred.0 = INTEGER: source1(1) */
	{ "input.source.preferred", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.4.5.0", NULL, SU_FLAG_OK, NULL },
	/* ats2InputDephasing = INTEGER: 181 */
	{ "input.phase.shift", 0, 1, ".1.3.6.1.4.1.534.10.2.2.1.1.0", NULL, SU_FLAG_OK, NULL },

	/* Output collection */
	/* ats2OutputVoltage.0 = INTEGER: 2304 0.1 V */
	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigOutputVoltage.0 = INTEGER: 230 1 V */
	{ "output.voltage.nominal", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.4.4.0", NULL, SU_FLAG_OK, NULL },
	/* ats2OutputCurrent.0 = INTEGER: 5 0.1 A */
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.3.2.0", NULL, SU_FLAG_OK, NULL },

	/* UPS collection */
	/* FIXME: RFC for device.test.result! */
	/* ats2ConfigTransferTest.0 = INTEGER: noTestInitiated(6) */
	{ "ups.test.result", 0, 1, ".1.3.6.1.4.1.534.10.2.4.8.0", NULL, SU_FLAG_OK, eaton_ats16_test_result_info },
	/* FIXME: RFC for device.status! */
	/* ats2StatusOutput.0 = INTEGER: outputPowered(2) */
	{ "ups.status", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.2.0", NULL, SU_FLAG_OK, eaton_ats16_output_status_info },

	/* Ambient collection */
	/* ats2EnvRemoteTemp.0 = INTEGER: 0 degrees Centigrade */
	{ "ambient.temperature", 0, 0.1, ".1.3.6.1.4.1.534.10.2.5.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteTempLowerLimit.0 = INTEGER: 5 degrees Centigrade */
	{ "ambient.temperature.low", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.5.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteTempUpperLimit.0 = INTEGER: 40 degrees Centigrade */
	{ "ambient.temperature.high", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.6.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteHumidity.0 = INTEGER: 0 percent */
	{ "ambient.humidity", 0, 0.1, ".1.3.6.1.4.1.534.10.2.5.2.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteHumidityLowerLimit.0 = INTEGER: 5 percent */
	{ "ambient.humidity.low", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.7.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteHumidityUpperLimit.0 = INTEGER: 90 percent */
	{ "ambient.humidity.high", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.8.0", NULL, SU_FLAG_OK, NULL },

#if 0 /* FIXME: Remaining data to be processed */
	/* ats2InputStatusDephasing.0 = INTEGER: normal(1) */
	{ "unmapped.ats2InputStatusDephasing", 0, 1, ".1.3.6.1.4.1.534.10.2.3.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusIndex.source1 = INTEGER: source1(1) */
	{ "unmapped.ats2InputStatusIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusIndex.source2 = INTEGER: source2(2) */
	{ "unmapped.ats2InputStatusIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.1.2", NULL, SU_FLAG_OK, NULL },

	/* ats2InputStatusGood.source1 = INTEGER: voltageAndFreqNormalRange(2) */
	{ "unmapped.ats2InputStatusGood", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusGood.source2 = INTEGER: voltageAndFreqNormalRange(2) */
	{ "unmapped.ats2InputStatusGood", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusInternalFailure.source1 = INTEGER: good(1) */
	{ "unmapped.ats2InputStatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusInternalFailure.source2 = INTEGER: good(1) */
	{ "unmapped.ats2InputStatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.4.2", NULL, SU_FLAG_OK, NULL },

	/* ats2InputStatusUsed.source1 = INTEGER: poweringLoad(2) */
	{ "unmapped.ats2InputStatusUsed", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusUsed.source2 = INTEGER: notPoweringLoad(1) */
	{ "unmapped.ats2InputStatusUsed", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.6.2", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusInternalFailure.0 = INTEGER: good(1) */
	{ "unmapped.ats2StatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.1.0", NULL, SU_FLAG_OK, NULL },

	/* ats2StatusOverload.0 = INTEGER: noOverload(1) */
	{ "unmapped.ats2StatusOverload", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.3.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusOverTemperature.0 = INTEGER: noOverTemperature(1) */
	{ "unmapped.ats2StatusOverTemperature", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.4.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusShortCircuit.0 = INTEGER: noShortCircuit(1) */
	{ "unmapped.ats2StatusShortCircuit", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusCommunicationLost.0 = INTEGER: good(1) */
	{ "unmapped.ats2StatusCommunicationLost", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.6.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusConfigurationFailure.0 = INTEGER: good(1) */
	{ "unmapped.ats2StatusConfigurationFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.7.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigTimeRTC.0 = Wrong Type (should be Counter32): Gauge32: 19191036 */
	{ "unmapped.ats2ConfigTimeRTC", 0, 1, ".1.3.6.1.4.1.534.10.2.4.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigTimeTextDate.0 = STRING: 08/11/1970 */
	{ "unmapped.ats2ConfigTimeTextDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigTimeTextTime.0 = STRING: 02/50/36 */
	{ "unmapped.ats2ConfigTimeTextTime", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigInputVoltageRating.0 = INTEGER: 1 1 V */
	{ "unmapped.ats2ConfigInputVoltageRating", 0, 1, ".1.3.6.1.4.1.534.10.2.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigInputFrequencyRating.0 = INTEGER: 50 Hz */
	{ "unmapped.ats2ConfigInputFrequencyRating", 0, 1, ".1.3.6.1.4.1.534.10.2.4.3.0", NULL, SU_FLAG_OK, NULL },

	/* ats2ConfigTransferMode.0 = INTEGER: standard(1) */
	{ "unmapped.ats2ConfigTransferMode", 0, 1, ".1.3.6.1.4.1.534.10.2.4.7.0", NULL, SU_FLAG_OK, NULL },

	/* ats2ConfigBrownoutLow.0 = INTEGER: 202 1 V */
	{ "unmapped.ats2ConfigBrownoutLow", 0, 1, ".1.3.6.1.4.1.534.10.2.4.9.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigBrownoutLowDerated.0 = INTEGER: 189 1 V */
	{ "unmapped.ats2ConfigBrownoutLowDerated", 0, 1, ".1.3.6.1.4.1.534.10.2.4.10.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigBrownoutHigh.0 = INTEGER: 258 1 V */
	{ "unmapped.ats2ConfigBrownoutHigh", 0, 1, ".1.3.6.1.4.1.534.10.2.4.11.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigHysteresisVoltage.0 = INTEGER: 5 1 V */
	{ "unmapped.ats2ConfigHysteresisVoltage", 0, 1, ".1.3.6.1.4.1.534.10.2.4.12.0", NULL, SU_FLAG_OK, NULL },

	/* Ambient collection */
	/* ats2EnvNumContacts.0 = INTEGER: 2 */
	{ "unmapped.ats2EnvNumContacts", 0, 1, ".1.3.6.1.4.1.534.10.2.5.3.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactIndex.1 = INTEGER: 1 */
	{ "unmapped.ats2ContactIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactIndex.2 = INTEGER: 2 */
	{ "unmapped.ats2ContactIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactType.1 = INTEGER: notUsed(4) */
	{ "unmapped.ats2ContactType", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactType.2 = INTEGER: notUsed(4) */
	{ "unmapped.ats2ContactType", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactState.1 = INTEGER: open(1) */
	{ "unmapped.ats2ContactState", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactState.2 = INTEGER: open(1) */
	{ "unmapped.ats2ContactState", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactDescr.1 = STRING: Input #1 */
	{ "unmapped.ats2ContactDescr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.5.4.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactDescr.2 = STRING: Input #2 */
	{ "unmapped.ats2ContactDescr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.5.4.1.4.2", NULL, SU_FLAG_OK, NULL },
#endif /* if 0 */
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	eaton_ats16 = { "eaton_ats16", EATON_ATS16_MIB_VERSION, NULL, EATON_ATS16_MODEL, eaton_ats16_mib, ".1.3.6.1.4.1.705.1" };
/* FIXME: Eaton ATS need to be fixed for the sysOID (currently .1.3.6.1.4.1.705.1!) */
/* mib2nut_info_t	eaton_ats16 = { "eaton_ats16", EATON_ATS16_MIB_VERSION, NULL, EATON_ATS16_MODEL, eaton_ats16_mib, EATON_ATS16_SYSOID }; */
