/* eaton-ats16-nmc-mib.c - subdriver to monitor Eaton ATS16 NMC SNMP devices with NUT
 * using legacy NMC cards
 *
 *  Copyright (C)
 *    2011-2012 Arnaud Quette <arnaud.quette@free.fr>
 *    2016-2020 Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
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

#include "eaton-ats16-nmc-mib.h"

#define EATON_ATS16_NMC_MIB_VERSION  "0.21"

#define EATON_ATS16_NMC_SYSOID  ".1.3.6.1.4.1.705.1"    /* legacy NMC */
#define EATON_ATS16_NMC_MODEL   ".1.3.6.1.4.1.534.10.2.1.2.0"

static info_lkp_t eaton_ats16_nmc_source_info[] = {
	info_lkp_default(1, "init"),
	info_lkp_default(2, "diagnosis"),
	info_lkp_default(3, "off"),
	info_lkp_default(4, "1"),
	info_lkp_default(5, "2"),
	info_lkp_default(6, "safe"),
	info_lkp_default(7, "fault"),
	info_lkp_sentinel
};

static info_lkp_t eaton_ats16_nmc_sensitivity_info[] = {
	info_lkp_default(1, "normal"),
	info_lkp_default(2, "high"),
	info_lkp_default(3, "low"),
	info_lkp_sentinel
};

static info_lkp_t eaton_ats16_nmc_input_frequency_status_info[] = {
	info_lkp_default(1, "good"),	/* No threshold triggered */
	info_lkp_default(2, "out-of-range"),	/* Frequency out of range triggered */
	info_lkp_sentinel
};

static info_lkp_t eaton_ats16_nmc_input_voltage_status_info[] = {
	info_lkp_default(1, "good"),	/* No threshold triggered */
	info_lkp_default(2, "derated-range"),	/* Voltage derated */
	info_lkp_default(3, "out-of-range"),	/* Voltage out of range triggered */
	info_lkp_default(4, "unknown"),	/* "missing" */
	info_lkp_sentinel
};

static info_lkp_t eaton_ats16_nmc_test_result_info[] = {
	info_lkp_default(1, "done and passed"),
	info_lkp_default(2, "done and warning"),
	info_lkp_default(3, "done and error"),
	info_lkp_default(4, "aborted"),
	info_lkp_default(5, "in progress"),
	info_lkp_default(6, "no test initiated"),
	info_lkp_sentinel
};

static info_lkp_t eaton_ats16_nmc_output_status_info[] = {
	info_lkp_default(1, "OFF"),	/* Output not powered */
	info_lkp_default(2, "OL"),	/* Output powered */
	info_lkp_sentinel
};

static info_lkp_t eaton_ats16_nmc_ambient_drycontacts_info[] = {
	info_lkp_default(-1, "unknown"),
	info_lkp_default(1, "opened"),
	info_lkp_default(2, "closed"),
	info_lkp_default(3, "opened"),	/* openWithNotice   */
	info_lkp_default(4, "closed"),	/* closedWithNotice */
	info_lkp_sentinel
};

/* EATON_ATS_NMC Snmp2NUT lookup table */
static snmp_info_t eaton_ats16_nmc_mib[] = {
	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* Device collection */
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ats", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	/* ats2IdentManufacturer.0 = STRING: EATON */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.1.0", "Eaton", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ats2IdentModel.0 = STRING: Eaton ATS */
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.2.0", "ATS", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* FIXME: RFC for device.firmware! */
	/* FIXME: the 2 "firmware" entries below should be SU_FLAG_SEMI_STATIC */
	/* ats2IdentFWVersion.0 = STRING: 00.00.0009 */
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* FIXME: RFC for device.firmware.aux! */
	/* ats2IdentRelease.0 = STRING: JF */
	snmp_info_default("ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.4.0", NULL, SU_FLAG_OK, NULL),
	/* ats2IdentSerialNumber.0 = STRING: GA04F23009 */
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ats2IdentPartNumber.0 = STRING: EATS16N */
	snmp_info_default("device.part", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.6.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ats2IdentAgentVersion.0 = STRING: 301F23C28 */
	/* { "unmapped.ats2IdentAgentVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.7.0", NULL, SU_FLAG_OK, NULL, NULL }, */
	/* ats2InputDephasing.0 = INTEGER: 2 degrees */
	/* { "unmapped.ats2InputDephasing", 0, 1, ".1.3.6.1.4.1.534.10.2.2.1.1.0", NULL, SU_FLAG_OK, NULL, NULL }, */

	/* Input collection */
	/* ats2InputIndex.source1 = INTEGER: source1(1) */
	snmp_info_default("input.1.id", 0, 1, ".1.3.6.1.4.1.534.10.2.2.2.1.1.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ats2InputIndex.source2 = INTEGER: source2(2) */
	snmp_info_default("input.2.id", 0, 1, ".1.3.6.1.4.1.534.10.2.2.2.1.1.2", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ats2InputVoltage.source1 = INTEGER: 2292 0.1 V */
	snmp_info_default("input.1.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* ats2InputVoltage.source2 = INTEGER: 2432 0.1 V */
	snmp_info_default("input.2.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusVoltage.source1 = INTEGER: normalRange(1) */
	snmp_info_default("input.1.voltage.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.5.1", NULL, SU_FLAG_OK, eaton_ats16_nmc_input_voltage_status_info),
	/* ats2InputStatusVoltage.source2 = INTEGER: normalRange(1) */
	snmp_info_default("input.2.voltage.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.5.2", NULL, SU_FLAG_OK, eaton_ats16_nmc_input_voltage_status_info),
	/* ats2InputFrequency.source1 = INTEGER: 500 0.1 Hz */
	snmp_info_default("input.1.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* ats2InputFrequency.source2 = INTEGER: 500 0.1 Hz */
	snmp_info_default("input.2.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusFrequency.source1 = INTEGER: good(1) */
	snmp_info_default("input.1.frequency.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.2.1", NULL, SU_FLAG_OK, eaton_ats16_nmc_input_frequency_status_info),
	/* ats2InputStatusFrequency.source2 = INTEGER: good(1) */
	snmp_info_default("input.2.frequency.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.2.2", NULL, SU_FLAG_OK, eaton_ats16_nmc_input_frequency_status_info),
	/* ats2ConfigSensitivity.0 = INTEGER: normal(1) */
	snmp_info_default("input.sensitivity", ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.6.0", NULL, SU_FLAG_OK, &eaton_ats16_nmc_sensitivity_info[0]),
	/* ats2OperationMode.0 = INTEGER: source1(4) */
	snmp_info_default("input.source", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.2.4.0", NULL, SU_FLAG_OK, eaton_ats16_nmc_source_info),
	/* ats2ConfigPreferred.0 = INTEGER: source1(1) */
	snmp_info_default("input.source.preferred", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.4.5.0", NULL, SU_FLAG_OK, NULL),
	/* ats2InputDephasing = INTEGER: 181 */
	snmp_info_default("input.phase.shift", 0, 1, ".1.3.6.1.4.1.534.10.2.2.1.1.0", NULL, SU_FLAG_OK, NULL),

	/* Output collection */
	/* ats2OutputVoltage.0 = INTEGER: 2304 0.1 V */
	snmp_info_default("output.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.3.1.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigOutputVoltage.0 = INTEGER: 230 1 V */
	snmp_info_default("output.voltage.nominal", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.4.4.0", NULL, SU_FLAG_OK, NULL),
	/* ats2OutputCurrent.0 = INTEGER: 5 0.1 A */
	snmp_info_default("output.current", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.3.2.0", NULL, SU_FLAG_OK, NULL),

	/* UPS collection */
	/* FIXME: RFC for device.test.result! */
	/* ats2ConfigTransferTest.0 = INTEGER: noTestInitiated(6) */
	snmp_info_default("ups.test.result", 0, 1, ".1.3.6.1.4.1.534.10.2.4.8.0", NULL, SU_FLAG_OK, eaton_ats16_nmc_test_result_info),
	/* FIXME: RFC for device.status! */
	/* ats2StatusOutput.0 = INTEGER: outputPowered(2) */
	snmp_info_default("ups.status", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.2.0", NULL, SU_FLAG_OK, eaton_ats16_nmc_output_status_info),

	/* Ambient collection */
	/* ats2EnvRemoteTemp.0 = INTEGER: 0 degrees Centigrade */
	snmp_info_default("ambient.temperature", 0, 0.1, ".1.3.6.1.4.1.534.10.2.5.1.0", NULL, SU_FLAG_OK, NULL),
	/* ats2EnvRemoteTempLowerLimit.0 = INTEGER: 5 degrees Centigrade */
	snmp_info_default("ambient.temperature.low", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.5.0", NULL, SU_FLAG_OK, NULL),
	/* ats2EnvRemoteTempUpperLimit.0 = INTEGER: 40 degrees Centigrade */
	snmp_info_default("ambient.temperature.high", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.6.0", NULL, SU_FLAG_OK, NULL),
	/* ats2EnvRemoteHumidity.0 = INTEGER: 0 percent */
	snmp_info_default("ambient.humidity", 0, 0.1, ".1.3.6.1.4.1.534.10.2.5.2.0", NULL, SU_FLAG_OK, NULL),
	/* ats2EnvRemoteHumidityLowerLimit.0 = INTEGER: 5 percent */
	snmp_info_default("ambient.humidity.low", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.7.0", NULL, SU_FLAG_OK, NULL),
	/* ats2EnvRemoteHumidityUpperLimit.0 = INTEGER: 90 percent */
	snmp_info_default("ambient.humidity.high", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.8.0", NULL, SU_FLAG_OK, NULL),

	/* Dry contacts on EMP001 TH module */
	/* ats2ContactState.1 = INTEGER: open(1) */
	snmp_info_default("ambient.contacts.1.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.10.2.5.4.1.3.1",
		NULL, SU_FLAG_OK, &eaton_ats16_nmc_ambient_drycontacts_info[0]),
	/* ats2ContactState.2 = INTEGER: open(1) */
	snmp_info_default("ambient.contacts.2.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.10.2.5.4.1.3.2",
		NULL, SU_FLAG_OK, &eaton_ats16_nmc_ambient_drycontacts_info[0]),

#if WITH_UNMAPPED_DATA_POINTS /* FIXME: Remaining data to be processed */
	/* ats2InputStatusDephasing.0 = INTEGER: normal(1) */
	snmp_info_default("unmapped.ats2InputStatusDephasing", 0, 1, ".1.3.6.1.4.1.534.10.2.3.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusIndex.source1 = INTEGER: source1(1) */
	snmp_info_default("unmapped.ats2InputStatusIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusIndex.source2 = INTEGER: source2(2) */
	snmp_info_default("unmapped.ats2InputStatusIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.1.2", NULL, SU_FLAG_OK, NULL),

	/* ats2InputStatusGood.source1 = INTEGER: voltageAndFreqNormalRange(2) */
	snmp_info_default("unmapped.ats2InputStatusGood", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusGood.source2 = INTEGER: voltageAndFreqNormalRange(2) */
	snmp_info_default("unmapped.ats2InputStatusGood", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusInternalFailure.source1 = INTEGER: good(1) */
	snmp_info_default("unmapped.ats2InputStatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusInternalFailure.source2 = INTEGER: good(1) */
	snmp_info_default("unmapped.ats2InputStatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.4.2", NULL, SU_FLAG_OK, NULL),

	/* ats2InputStatusUsed.source1 = INTEGER: poweringLoad(2) */
	snmp_info_default("unmapped.ats2InputStatusUsed", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* ats2InputStatusUsed.source2 = INTEGER: notPoweringLoad(1) */
	snmp_info_default("unmapped.ats2InputStatusUsed", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.6.2", NULL, SU_FLAG_OK, NULL),
	/* ats2StatusInternalFailure.0 = INTEGER: good(1) */
	snmp_info_default("unmapped.ats2StatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.1.0", NULL, SU_FLAG_OK, NULL),

	/* ats2StatusOverload.0 = INTEGER: noOverload(1) */
	snmp_info_default("unmapped.ats2StatusOverload", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.3.0", NULL, SU_FLAG_OK, NULL),
	/* ats2StatusOverTemperature.0 = INTEGER: noOverTemperature(1) */
	snmp_info_default("unmapped.ats2StatusOverTemperature", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.4.0", NULL, SU_FLAG_OK, NULL),
	/* ats2StatusShortCircuit.0 = INTEGER: noShortCircuit(1) */
	snmp_info_default("unmapped.ats2StatusShortCircuit", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.5.0", NULL, SU_FLAG_OK, NULL),
	/* ats2StatusCommunicationLost.0 = INTEGER: good(1) */
	snmp_info_default("unmapped.ats2StatusCommunicationLost", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.6.0", NULL, SU_FLAG_OK, NULL),
	/* ats2StatusConfigurationFailure.0 = INTEGER: good(1) */
	snmp_info_default("unmapped.ats2StatusConfigurationFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.7.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigTimeRTC.0 = Wrong Type (should be Counter32): Gauge32: 19191036 */
	snmp_info_default("unmapped.ats2ConfigTimeRTC", 0, 1, ".1.3.6.1.4.1.534.10.2.4.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigTimeTextDate.0 = STRING: 08/11/1970 */
	snmp_info_default("unmapped.ats2ConfigTimeTextDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigTimeTextTime.0 = STRING: 02/50/36 */
	snmp_info_default("unmapped.ats2ConfigTimeTextTime", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigInputVoltageRating.0 = INTEGER: 1 1 V */
	snmp_info_default("unmapped.ats2ConfigInputVoltageRating", 0, 1, ".1.3.6.1.4.1.534.10.2.4.2.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigInputFrequencyRating.0 = INTEGER: 50 Hz */
	snmp_info_default("unmapped.ats2ConfigInputFrequencyRating", 0, 1, ".1.3.6.1.4.1.534.10.2.4.3.0", NULL, SU_FLAG_OK, NULL),

	/* ats2ConfigTransferMode.0 = INTEGER: standard(1) */
	snmp_info_default("unmapped.ats2ConfigTransferMode", 0, 1, ".1.3.6.1.4.1.534.10.2.4.7.0", NULL, SU_FLAG_OK, NULL),

	/* ats2ConfigBrownoutLow.0 = INTEGER: 202 1 V */
	snmp_info_default("unmapped.ats2ConfigBrownoutLow", 0, 1, ".1.3.6.1.4.1.534.10.2.4.9.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigBrownoutLowDerated.0 = INTEGER: 189 1 V */
	snmp_info_default("unmapped.ats2ConfigBrownoutLowDerated", 0, 1, ".1.3.6.1.4.1.534.10.2.4.10.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigBrownoutHigh.0 = INTEGER: 258 1 V */
	snmp_info_default("unmapped.ats2ConfigBrownoutHigh", 0, 1, ".1.3.6.1.4.1.534.10.2.4.11.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ConfigHysteresisVoltage.0 = INTEGER: 5 1 V */
	snmp_info_default("unmapped.ats2ConfigHysteresisVoltage", 0, 1, ".1.3.6.1.4.1.534.10.2.4.12.0", NULL, SU_FLAG_OK, NULL),

	/* Ambient collection */
	/* ats2EnvNumContacts.0 = INTEGER: 2 */
	snmp_info_default("unmapped.ats2EnvNumContacts", 0, 1, ".1.3.6.1.4.1.534.10.2.5.3.0", NULL, SU_FLAG_OK, NULL),
	/* ats2ContactIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.ats2ContactIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* ats2ContactIndex.2 = INTEGER: 2 */
	snmp_info_default("unmapped.ats2ContactIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* ats2ContactType.1 = INTEGER: notUsed(4) */
	snmp_info_default("unmapped.ats2ContactType", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* ats2ContactType.2 = INTEGER: notUsed(4) */
	snmp_info_default("unmapped.ats2ContactType", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* ats2ContactDescr.1 = STRING: Input #1 */
	snmp_info_default("unmapped.ats2ContactDescr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.5.4.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* ats2ContactDescr.2 = STRING: Input #2 */
	snmp_info_default("unmapped.ats2ContactDescr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.5.4.1.4.2", NULL, SU_FLAG_OK, NULL),
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	snmp_info_sentinel
};

/* Note: keep the legacy definition intact, to avoid breaking compatibility */

/* FIXME: The lines below are duplicated to fix an issue with the code generator (nut-snmpinfo.py -> line is discarding) */
/* Note:
 *   due to a bug in tools/nut-snmpinfo.py, prepending a 2nd mib2nut_info_t
 *   declaration with a comment line results in data extraction not being
 *   done for all entries in the file. Hence the above comment line being
 *   after its belonging declaration! */

/*mib2nut_info_t  eaton_ats16_nmc = { "eaton_ats16_nmc", EATON_ATS16_NMC_MIB_VERSION, NULL, EATON_ATS16_NMC_MODEL, eaton_ats16_nmc_mib, EATON_ATS16_NMC_SYSOID, NULL }; */
mib2nut_info_t	eaton_ats16_nmc = { "eaton_ats16_nmc", EATON_ATS16_NMC_MIB_VERSION, NULL, EATON_ATS16_NMC_MODEL, eaton_ats16_nmc_mib, EATON_ATS16_NMC_SYSOID, NULL };
