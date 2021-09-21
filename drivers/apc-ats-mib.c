/* apcats-mib.c - subdriver to monitor apcats SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *  2016 Arnaud Quette <ArnaudQuette@Eaton.com>
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

#include "apc-ats-mib.h"

#define APC_ATS_MIB_VERSION  "0.4"

#define APC_ATS_SYSOID       ".1.3.6.1.4.1.318.1.3.11"
#define APC_ATS_OID_MODEL_NAME ".1.3.6.1.4.1.318.1.1.8.1.5.0"

static info_lkp_t apc_ats_sensitivity_info[] = {
	{ 1, "high", NULL, NULL },
	{ 2, "low", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t apc_ats_output_status_info[] = {
	{ 1, "OFF", NULL, NULL }, /* fail */
	{ 2, "OL", NULL, NULL },  /* ok */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t apc_ats_outletgroups_name_info[] = {
	{ 1, "total", NULL, NULL },
	{ 2, "bank1", NULL, NULL },
	{ 3, "bank2", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t apc_ats_outletgroups_status_info[] = {
	{ 1, "OL", NULL, NULL },   /* normal */
	{ 2, "", NULL, NULL },     /* lowload */
	{ 3, "", NULL, NULL },     /* nearoverload */
	{ 4, "OVER", NULL, NULL }, /* overload */
	{ 0, NULL, NULL, NULL }
};

/* APC ATS Snmp2NUT lookup table */
static snmp_info_t apc_ats_mib[] = {

	/* Device collection */
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ats", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* ats2IdentManufacturer.0 = STRING: EATON */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "APC", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* atsIdentModelNumber.0 = STRING: "AP7724" */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, APC_ATS_OID_MODEL_NAME, NULL, SU_FLAG_OK, NULL },
	/* FIXME: RFC for device.firmware! */
	/* atsIdentHardwareRev.0 = STRING: "R01" */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* FIXME: RFC for device.firmware.aux! */
	/* atsIdentFirmwareRev.0 = STRING: "3.0.5" */
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* atsIdentFirmwareDate.0 = STRING: "09/13/11" */
	/*{ "unmapped.atsIdentFirmwareDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.1.3.0", NULL, SU_FLAG_OK, NULL },*/
	/* atsIdentSerialNumber.0 = STRING: "5A1516T15268" */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.1.6.0", NULL, SU_FLAG_OK, NULL },
	/* FIXME: RFC for device.mfr.date! */
	/* atsIdentDateOfManufacture.0 = STRING: "04/18/2015" */
	{ "ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.1.4.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigProductName.0 = STRING: "m-ups-04" */
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.4.1.0", NULL, SU_FLAG_OK, NULL },

	/* Input collection */
	/* atsIdentNominalLineVoltage.0 = INTEGER: 230 */
	{ "input.voltage.nominal", 0, 1, ".1.3.6.1.4.1.318.1.1.8.1.7.0", NULL, SU_FLAG_OK, NULL },
	/* atsIdentNominalLineFrequency.0 = INTEGER: 50 */
	{ "input.frequency.nominal", 0, 1, ".1.3.6.1.4.1.318.1.1.8.1.8.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusSelectedSource.0 = INTEGER: sourceB(2) */
	{ "input.source", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigPreferredSource.0 = INTEGER: sourceB(2) */
	{ "input.source.preferred", ST_FLAG_RW, 1, ".1.3.6.1.4.1.318.1.1.8.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* atsInputVoltage.1.1.1 = INTEGER: 216 */
	{ "input.1.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.3.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputVoltage.2.1.1 = INTEGER: 215 */
	{ "input.2.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.3.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputFrequency.1 = INTEGER: 50 */
	{ "input.1.frequency", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputFrequency.2 = INTEGER: 50 */
	{ "input.2.frequency", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* atsConfigVoltageSensitivity.0 = INTEGER: high(1) */
	{ "input.sensitivity", ST_FLAG_RW, 1, ".1.3.6.1.4.1.318.1.1.8.4.4.0", NULL, SU_FLAG_OK, &apc_ats_sensitivity_info[0] },
	/* FIXME: RFC for input.count! */
	/* atsNumInputs.0 = INTEGER: 2 */
	{ "input.count", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.1.0", NULL, SU_FLAG_OK, NULL },

	/* Output collection */
	/* atsOutputFrequency.1 = INTEGER: 50 */
	{ "output.frequency", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankOutputVoltage.1 = INTEGER: 215 */
	{ "output.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.6.1", NULL, SU_FLAG_OK, NULL },

	/* UPS collection */
	/* FIXME: RFC for device.status! */
	/* atsStatusVoltageOutStatus.0 = INTEGER: ok(2) */
	{ "ups.status", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.15.0", NULL, SU_FLAG_OK, &apc_ats_output_status_info[0] },

	/* Outlet groups collection */
	/* Note: prefer the OutputBank data to the ConfigBank ones */
	/* atsConfigBankTableSize.0 = INTEGER: 3 */
	/*{ "outlet.group.count", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.13.0", NULL, SU_FLAG_OK, NULL },*/
	/* atsOutputBankTableSize.0 = INTEGER: 3 */
	{ "outlet.group.count", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.4.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankTableIndex.%i = INTEGER: %i */
	/*{ "outlet.group.%i.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.4.14.1.1.%i", NULL, SU_FLAG_OK, NULL },*/
	/* atsOutputBankTableIndex.%i = INTEGER: %i */
	{ "outlet.group.%i.id", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.1.%i", NULL, SU_FLAG_OK | SU_OUTLET_GROUP, NULL },
	/* atsConfigBank.%i = INTEGER: total(1) */
	/*{ "outlet.group.%i.name", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.2.%i", NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP, &apc_ats_group_name_info[0] },*/
	/* atsOutputBank.1 = INTEGER: total(1) */
	{ "outlet.group.%i.name", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.3.%i", NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP, &apc_ats_outletgroups_name_info[0] },
	/* atsOutputBankCurrent.%i = Gauge32: 88 */
	{ "outlet.group.%i.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.4.%i", NULL, SU_OUTLET_GROUP, NULL },
	/* atsOutputBankState.%i = INTEGER: normal(1) */
	{ "outlet.group.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.5.%i", NULL, SU_OUTLET_GROUP, &apc_ats_outletgroups_status_info[0] },
	/* atsOutputBankOutputVoltage.%i = INTEGER: 215 */
	{ "outlet.group.%i.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.6.%i", NULL, SU_OUTLET_GROUP, NULL },
	/* atsOutputBankPower.1 = INTEGER: 1883 */
	{ "outlet.group.%i.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.15.%i", NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP, NULL },


#if 0 /* FIXME: Remaining data to be processed */
	/* atsIdentDeviceRating.0 = INTEGER: 32 */
	{ "unmapped.atsIdentDeviceRating", 0, 1, ".1.3.6.1.4.1.318.1.1.8.1.9.0", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationNumInputs.0 = INTEGER: 2 */
	{ "unmapped.atsCalibrationNumInputs", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationNumInputPhases.0 = INTEGER: 1 */
	{ "unmapped.atsCalibrationNumInputPhases", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationInputTableIndex.1.1.1 = INTEGER: 1 */
	{ "unmapped.atsCalibrationInputTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.3.1.1.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationInputTableIndex.2.1.1 = INTEGER: 2 */
	{ "unmapped.atsCalibrationInputTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.3.1.1.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationInputPhaseTableIndex.1.1.1 = INTEGER: 1 */
	{ "unmapped.atsCalibrationInputPhaseTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.3.1.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationInputPhaseTableIndex.2.1.1 = INTEGER: 1 */
	{ "unmapped.atsCalibrationInputPhaseTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.3.1.2.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsLineVoltageCalibrationFactor.1.1.1 = INTEGER: 487 */
	{ "unmapped.atsLineVoltageCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.3.1.3.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsLineVoltageCalibrationFactor.2.1.1 = INTEGER: 488 */
	{ "unmapped.atsLineVoltageCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.1.3.1.3.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltages.0 = INTEGER: 5 */
	{ "unmapped.atsCalibrationPowerSupplyVoltages", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.1.0", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltageTableIndex.1 = INTEGER: 1 */
	{ "unmapped.atsCalibrationPowerSupplyVoltageTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltageTableIndex.2 = INTEGER: 2 */
	{ "unmapped.atsCalibrationPowerSupplyVoltageTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltageTableIndex.3 = INTEGER: 3 */
	{ "unmapped.atsCalibrationPowerSupplyVoltageTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.1.3", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltageTableIndex.4 = INTEGER: 4 */
	{ "unmapped.atsCalibrationPowerSupplyVoltageTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltageTableIndex.5 = INTEGER: 5 */
	{ "unmapped.atsCalibrationPowerSupplyVoltageTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltage.1 = INTEGER: powerSupply24V(1) */
	{ "unmapped.atsCalibrationPowerSupplyVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltage.2 = INTEGER: powerSupply12V(2) */
	{ "unmapped.atsCalibrationPowerSupplyVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltage.3 = INTEGER: powerSupply(3) */
	{ "unmapped.atsCalibrationPowerSupplyVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.2.3", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltage.4 = INTEGER: powerSupply24VSourceB(4) */
	{ "unmapped.atsCalibrationPowerSupplyVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.2.4", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationPowerSupplyVoltage.5 = INTEGER: powerSupplyMinus12V(5) */
	{ "unmapped.atsCalibrationPowerSupplyVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.2.5", NULL, SU_FLAG_OK, NULL },
	/* atsPowerSupplyVoltageCalibrationFactor.1 = INTEGER: 521 */
	{ "unmapped.atsPowerSupplyVoltageCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* atsPowerSupplyVoltageCalibrationFactor.2 = INTEGER: 1076 */
	{ "unmapped.atsPowerSupplyVoltageCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* atsPowerSupplyVoltageCalibrationFactor.3 = INTEGER: 2560 */
	{ "unmapped.atsPowerSupplyVoltageCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.3.3", NULL, SU_FLAG_OK, NULL },
	/* atsPowerSupplyVoltageCalibrationFactor.4 = INTEGER: 521 */
	{ "unmapped.atsPowerSupplyVoltageCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.3.4", NULL, SU_FLAG_OK, NULL },
	/* atsPowerSupplyVoltageCalibrationFactor.5 = INTEGER: 975 */
	{ "unmapped.atsPowerSupplyVoltageCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.2.2.1.3.5", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationNumOutputs.0 = INTEGER: 1 */
	{ "unmapped.atsCalibrationNumOutputs", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationNumOutputPhases.0 = INTEGER: 1 */
	{ "unmapped.atsCalibrationNumOutputPhases", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.3.2.0", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationOutputTableIndex.1.phase1.1 = INTEGER: 1 */
	{ "unmapped.atsCalibrationOutputTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.3.3.1.1.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsCalibrationOutputPhasesTableIndex.1.phase1.1 = INTEGER: phase1(1) */
	{ "unmapped.atsCalibrationOutputPhasesTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.3.3.1.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputCurrentCalibrationFactor.1.phase1.1 = INTEGER: 487 */
	{ "unmapped.atsOutputCurrentCalibrationFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.8.2.3.3.1.3.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsControlResetATS.0 = INTEGER: none(1) */
	{ "unmapped.atsControlResetATS", 0, 1, ".1.3.6.1.4.1.318.1.1.8.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* atsControlClearAllAlarms.0 = INTEGER: -1 */
	{ "unmapped.atsControlClearAllAlarms", 0, 1, ".1.3.6.1.4.1.318.1.1.8.3.2.0", NULL, SU_FLAG_OK, NULL },

	/* atsConfigFrontPanelLockout.0 = INTEGER: enableFrontPanel(2) */
	{ "unmapped.atsConfigFrontPanelLockout", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.3.0", NULL, SU_FLAG_OK, NULL },

	/* atsConfigTransferVoltageRange.0 = INTEGER: medium(2) */
	{ "unmapped.atsConfigTransferVoltageRange", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.5.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigCurrentLimit.0 = INTEGER: 32 */
	{ "unmapped.atsConfigCurrentLimit", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.6.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigResetValues.0 = INTEGER: -1 */
	{ "unmapped.atsConfigResetValues", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.7.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigLineVRMS.0 = INTEGER: 230 */
	{ "unmapped.atsConfigLineVRMS", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.8.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigLineVRMSNarrowLimit.0 = INTEGER: 16 */
	{ "unmapped.atsConfigLineVRMSNarrowLimit", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.9.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigLineVRMSMediumLimit.0 = INTEGER: 23 */
	{ "unmapped.atsConfigLineVRMSMediumLimit", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.10.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigLineVRMSWideLimit.0 = INTEGER: 30 */
	{ "unmapped.atsConfigLineVRMSWideLimit", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.11.0", NULL, SU_FLAG_OK, NULL },
	/* atsConfigFrequencyDeviation.0 = INTEGER: two(2) */
	{ "unmapped.atsConfigFrequencyDeviation", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.12.0", NULL, SU_FLAG_OK, NULL },

	/* Outlet groups collection */
	/* atsConfigBankLowLoadThreshold.1 = INTEGER: 0 */
	{ "unmapped.atsConfigBankLowLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankLowLoadThreshold.2 = INTEGER: 0 */
	{ "unmapped.atsConfigBankLowLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankLowLoadThreshold.3 = INTEGER: 0 */
	{ "unmapped.atsConfigBankLowLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.3.3", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankNearOverLoadThreshold.1 = INTEGER: 28 */
	{ "unmapped.atsConfigBankNearOverLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankNearOverLoadThreshold.2 = INTEGER: 12 */
	{ "unmapped.atsConfigBankNearOverLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankNearOverLoadThreshold.3 = INTEGER: 12 */
	{ "unmapped.atsConfigBankNearOverLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.4.3", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankOverLoadThreshold.1 = INTEGER: 32 */
	{ "unmapped.atsConfigBankOverLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankOverLoadThreshold.2 = INTEGER: 16 */
	{ "unmapped.atsConfigBankOverLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.5.2", NULL, SU_FLAG_OK, NULL },
	/* atsConfigBankOverLoadThreshold.3 = INTEGER: 16 */
	{ "unmapped.atsConfigBankOverLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.14.1.5.3", NULL, SU_FLAG_OK, NULL },
	/* atsConfigPhaseTableSize.0 = INTEGER: 0 */
	{ "unmapped.atsConfigPhaseTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.8.4.15.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusCommStatus.0 = INTEGER: atsCommEstablished(2) */
	{ "unmapped.atsStatusCommStatus", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.1.0", NULL, SU_FLAG_OK, NULL },

	/* atsStatusRedundancyState.0 = INTEGER: atsFullyRedundant(2) */
	{ "unmapped.atsStatusRedundancyState", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusOverCurrentState.0 = INTEGER: atsCurrentOK(2) */
	{ "unmapped.atsStatusOverCurrentState", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.4.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatus5VPowerSupply.0 = INTEGER: atsPowerSupplyOK(2) */
	{ "unmapped.atsStatus5VPowerSupply", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.5.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatus24VPowerSupply.0 = INTEGER: atsPowerSupplyOK(2) */
	{ "unmapped.atsStatus24VPowerSupply", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.6.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatus24VSourceBPowerSupply.0 = INTEGER: atsPowerSupplyOK(2) */
	{ "unmapped.atsStatus24VSourceBPowerSupply", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.7.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusPlus12VPowerSupply.0 = INTEGER: atsPowerSupplyOK(2) */
	{ "unmapped.atsStatusPlus12VPowerSupply", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.8.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusMinus12VPowerSupply.0 = INTEGER: atsPowerSupplyOK(2) */
	{ "unmapped.atsStatusMinus12VPowerSupply", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.9.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusSwitchStatus.0 = INTEGER: ok(2) */
	{ "unmapped.atsStatusSwitchStatus", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.10.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusFrontPanel.0 = INTEGER: unlocked(2) */
	{ "unmapped.atsStatusFrontPanel", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.11.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusSourceAStatus.0 = INTEGER: ok(2) */
	{ "unmapped.atsStatusSourceAStatus", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.12.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusSourceBStatus.0 = INTEGER: ok(2) */
	{ "unmapped.atsStatusSourceBStatus", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.13.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusPhaseSyncStatus.0 = INTEGER: inSync(1) */
	{ "unmapped.atsStatusPhaseSyncStatus", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.14.0", NULL, SU_FLAG_OK, NULL },

	/* atsStatusHardwareStatus.0 = INTEGER: ok(2) */
	{ "unmapped.atsStatusHardwareStatus", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.1.16.0", NULL, SU_FLAG_OK, NULL },
	/* atsStatusResetMaxMinValues.0 = INTEGER: -1 */
	{ "unmapped.atsStatusResetMaxMinValues", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.2.1.0", NULL, SU_FLAG_OK, NULL },

	/* atsInputTableIndex.1 = INTEGER: 1 */
	{ "unmapped.atsInputTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputTableIndex.2 = INTEGER: 2 */
	{ "unmapped.atsInputTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* atsNumInputPhases.1 = INTEGER: 1 */
	{ "unmapped.atsNumInputPhases", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* atsNumInputPhases.2 = INTEGER: 1 */
	{ "unmapped.atsNumInputPhases", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* atsInputVoltageOrientation.1 = INTEGER: singlePhase(2) */
	{ "unmapped.atsInputVoltageOrientation", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputVoltageOrientation.2 = INTEGER: singlePhase(2) */
	{ "unmapped.atsInputVoltageOrientation", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.3.2", NULL, SU_FLAG_OK, NULL },

	/* atsInputType.1 = INTEGER: main(2) */
	{ "unmapped.atsInputType", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputType.2 = INTEGER: main(2) */
	{ "unmapped.atsInputType", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.5.2", NULL, SU_FLAG_OK, NULL },
	/* atsInputName.1 = STRING: "Source A" */
	{ "unmapped.atsInputName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputName.2 = STRING: "Source B" */
	{ "unmapped.atsInputName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.8.5.3.2.1.6.2", NULL, SU_FLAG_OK, NULL },
	/* atsInputPhaseTableIndex.1.1.1 = INTEGER: 1 */
	{ "unmapped.atsInputPhaseTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.1.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputPhaseTableIndex.2.1.1 = INTEGER: 2 */
	{ "unmapped.atsInputPhaseTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.1.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputPhaseIndex.1.1.1 = INTEGER: 1 */
	{ "unmapped.atsInputPhaseIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputPhaseIndex.2.1.1 = INTEGER: 1 */
	{ "unmapped.atsInputPhaseIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.2.2.1.1", NULL, SU_FLAG_OK, NULL },

	/* atsInputMaxVoltage.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMaxVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMaxVoltage.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMaxVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.4.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMinVoltage.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMinVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.5.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMinVoltage.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMinVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.5.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputCurrent.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.6.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputCurrent.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.6.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMaxCurrent.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMaxCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.7.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMaxCurrent.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMaxCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.7.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMinCurrent.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMinCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.8.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMinCurrent.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMinCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.8.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputPower.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.9.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputPower.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.9.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMaxPower.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMaxPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.10.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMaxPower.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMaxPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.10.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMinPower.1.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMinPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.11.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsInputMinPower.2.1.1 = INTEGER: -1 */
	{ "unmapped.atsInputMinPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.3.3.1.11.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsNumOutputs.0 = INTEGER: 1 */
	{ "unmapped.atsNumOutputs", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* atsOutputTableIndex.1 = INTEGER: 1 */
	{ "unmapped.atsOutputTableIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* atsNumOutputPhases.1 = INTEGER: 1 */
	{ "unmapped.atsNumOutputPhases", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputVoltageOrientation.1 = INTEGER: singlePhase(2) */
	{ "unmapped.atsOutputVoltageOrientation", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.2.1.3.1", NULL, SU_FLAG_OK, NULL },

	/* atsOutputPhase.1 = INTEGER: phase1(1) */
	{ "unmapped.atsOutputPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputPhase.2 = INTEGER: phase1(1) */
	{ "unmapped.atsOutputPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputPhase.3 = INTEGER: phase1(1) */
	{ "unmapped.atsOutputPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.2.3", NULL, SU_FLAG_OK, NULL },

	/* atsOutputBankMaxCurrent.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.7.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxCurrent.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.7.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxCurrent.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.7.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinCurrent.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.8.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinCurrent.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.8.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinCurrent.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinCurrent", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.8.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankLoad.1 = INTEGER: 1883 */
	{ "unmapped.atsOutputBankLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.9.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankLoad.2 = INTEGER: 984 */
	{ "unmapped.atsOutputBankLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.9.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankLoad.3 = INTEGER: 898 */
	{ "unmapped.atsOutputBankLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.9.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxLoad.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.10.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxLoad.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.10.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxLoad.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.10.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinLoad.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.11.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinLoad.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.11.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinLoad.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.11.3", NULL, SU_FLAG_OK, NULL },

	/* atsOutputBankPercentLoad.1 = INTEGER: 25 */
	{ "unmapped.atsOutputBankPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.12.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankPercentLoad.2 = INTEGER: 13 */
	{ "unmapped.atsOutputBankPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.12.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankPercentLoad.3 = INTEGER: 12 */
	{ "unmapped.atsOutputBankPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.12.3", NULL, SU_FLAG_OK, NULL },

	/* atsOutputBankMaxPercentLoad.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.13.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxPercentLoad.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.13.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxPercentLoad.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.13.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPercentLoad.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.14.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPercentLoad.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.14.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPercentLoad.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPercentLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.14.3", NULL, SU_FLAG_OK, NULL },

	/* atsOutputBankMaxPower.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.16.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxPower.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.16.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxPower.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.16.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPower.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.17.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPower.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.17.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPower.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.17.3", NULL, SU_FLAG_OK, NULL },

	/* atsOutputBankPercentPower.1 = INTEGER: 25 */
	{ "unmapped.atsOutputBankPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.18.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankPercentPower.2 = INTEGER: 13 */
	{ "unmapped.atsOutputBankPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.18.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankPercentPower.3 = INTEGER: 12 */
	{ "unmapped.atsOutputBankPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.18.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxPercentPower.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.19.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxPercentPower.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.19.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMaxPercentPower.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMaxPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.19.3", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPercentPower.1 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.20.1", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPercentPower.2 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.20.2", NULL, SU_FLAG_OK, NULL },
	/* atsOutputBankMinPercentPower.3 = INTEGER: -1 */
	{ "unmapped.atsOutputBankMinPercentPower", 0, 1, ".1.3.6.1.4.1.318.1.1.8.5.4.5.1.20.3", NULL, SU_FLAG_OK, NULL },
#endif /* 0 */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	apc_ats = { "apc_ats", APC_ATS_MIB_VERSION, NULL, APC_ATS_OID_MODEL_NAME, apc_ats_mib, APC_ATS_SYSOID, NULL };
