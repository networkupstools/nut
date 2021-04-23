/* delta_ups-mib.c - subdriver to monitor delta_ups SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-snmp-subdriver.sh script. It must be customized!
 *
 *  MIB reference: http://www.networkupstools.org/ups-protocols/snmp/DeltaUPSv4.mib
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

#include "delta_ups-mib.h"

#define DELTA_UPS_MIB_VERSION  "0.2"

#define DELTA_UPS_SYSOID       ".1.3.6.1.4.1.2254.2.4"

/* To create a value lookup structure (as needed on the 2nd line of the example
 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
 * static info_lkp_t delta_onbatt_info[] = {
 * 	{ 1, "OB", NULL, NULL },
 * 	{ 2, "OL", NULL, NULL },
 * 	{ 0, NULL, NULL, NULL }
 * };
 */

static info_lkp_t delta_ups_upstype_info[] = {
	{ 1, "on-line", NULL, NULL },
	{ 2, "off-line", NULL, NULL },
	{ 3, "line-interactive", NULL, NULL },
	{ 4, "3phase", NULL, NULL },
	{ 5, "splite-phase", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t delta_ups_pwr_info[] = {
    { 0, "OL", NULL, NULL },        /* normal  */
    { 1, "OB", NULL, NULL },        /* battery  */
    { 2, "BYPASS", NULL, NULL },    /* bypass */
    { 3, "TRIM", NULL, NULL },      /* reducing */
    { 4, "BOOST", NULL, NULL },     /* boosting */
    { 5, "BYPASS", NULL, NULL },    /* manualBypass */
    /*{ 6, "NULL", NULL, NULL },*/      /* other  */
    { 7, "OFF", NULL, NULL },      /* none */
    { 0, NULL, NULL, NULL }
} ;

/* DELTA_UPS Snmp2NUT lookup table */
static snmp_info_t delta_ups_mib[] = {

	/* Data format:
	 * { info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar },
	 *
	 *	info_type:	NUT INFO_ or CMD_ element name
	 *	info_flags:	flags to set in addinfo
	 *	info_len:	length of strings if STR
	 *				cmd value if CMD, multiplier otherwise
	 *	OID: SNMP OID or NULL
	 *	dfl: default value
	 *	flags: snmp-ups internal flags (FIXME: ...)
	 *	oid2info: lookup table between OID and NUT values
	 *
	 * Example:
	 * { "input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL },
	 * { "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, delta_onbatt_info },
	 *
	 * To create a value lookup structure (as needed on the 2nd line), use the
	 * following kind of declaration, outside of the present snmp_info_t[]:
	 * static info_lkp_t delta_onbatt_info[] = {
	 * 	{ 1, "OB" },
	 * 	{ 2, "OL" },
	 * 	{ 0, NULL }
	 * };
	 */

	/* dupsIdentManufacturer.0 = STRING: "Socomec" */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsIdentModel.0 = STRING: "NETYS RT 1/1 UPS" */
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsIdentAgentSoftwareVersion.0 = STRING: "2.0h " */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsIdentUPSSoftwareVersion.0 = STRING: "1.1" */
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsType.0 = INTEGER: on-line(1) */
	{ "ups.type", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.19.0", NULL, SU_FLAG_OK, delta_ups_upstype_info },
	/* dupsOutputLoad1.0 = INTEGER: 29 */
	{ "ups.load", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRatingOutputVA.0 = INTEGER: 2200 */
	{ "ups.power", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRatingOutputVoltage.0 = INTEGER: 230 */
	{ "output.voltage.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputVoltage1.0 = INTEGER: 2300 */
	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.5.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRatingOutputFrequency.0 = INTEGER: 50 */
	{ "output.frequency.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.9.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputCurrent1.0 = INTEGER: 23 */
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.5.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRatingInputVoltage.0 = INTEGER: 230 */
	{ "input.voltage.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.10.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputVoltage1.0 = INTEGER: 2280 */
	{ "input.voltage", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.4.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRatingInputFrequency.0 = INTEGER: 50 */
	{ "input.frequency.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.11.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputFrequency1.0 = INTEGER: 499 */
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputSource.0 = INTEGER: normal(0) */
	{ "ups.status", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.1.0", NULL, SU_FLAG_OK, delta_ups_pwr_info },

	/* Remaining unmapped variables.
	 * Mostly the first field (string) is to be changed
	 * Check docs/nut-names.txt for the right variable names
	 */
#if 0
	/* dupsIdentName.0 = "" */
	{ "unmapped.dupsIdentName", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAttachedDevices.0 = "" */
	{ "unmapped.dupsAttachedDevices", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRatingBatteryVoltage.0 = INTEGER: 0 */
	{ "unmapped.dupsRatingBatteryVoltage", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.12.0", NULL, SU_FLAG_OK, NULL },
	/* dupsLowTransferVoltUpBound.0 = INTEGER: 0 Volt */
	{ "unmapped.dupsLowTransferVoltUpBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.13.0", NULL, SU_FLAG_OK, NULL },
	/* dupsLowTransferVoltLowBound.0 = INTEGER: 0 Volt */
	{ "unmapped.dupsLowTransferVoltLowBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.14.0", NULL, SU_FLAG_OK, NULL },
	/* dupsHighTransferVoltUpBound.0 = INTEGER: 0 Volt */
	{ "unmapped.dupsHighTransferVoltUpBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.15.0", NULL, SU_FLAG_OK, NULL },
	/* dupsHighTransferVoltLowBound.0 = INTEGER: 0 Volt */
	{ "unmapped.dupsHighTransferVoltLowBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.16.0", NULL, SU_FLAG_OK, NULL },
	/* dupsLowBattTime.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsLowBattTime", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.17.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutletRelays.0 = INTEGER: 2 */
	{ "unmapped.dupsOutletRelays", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.18.0", NULL, SU_FLAG_OK, NULL },
	/* dupsShutdownType.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsShutdownType", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAutoReboot.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAutoReboot", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsShutdownAction.0 = INTEGER: 0 */
	{ "unmapped.dupsShutdownAction", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRestartAction.0 = INTEGER: 0 */
	{ "unmapped.dupsRestartAction", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsSetOutletRelay.0 = INTEGER: 1 */
	{ "unmapped.dupsSetOutletRelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRelayOffDelay.0 = INTEGER: 0 */
	{ "unmapped.dupsRelayOffDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsRelayOnDelay.0 = INTEGER: 0 */
	{ "unmapped.dupsRelayOnDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigBuzzerAlarm.0 = INTEGER: alarm(1) */
	{ "unmapped.dupsConfigBuzzerAlarm", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigBuzzerState.0 = INTEGER: disable(2) */
	{ "unmapped.dupsConfigBuzzerState", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigSensitivity.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsConfigSensitivity", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigLowVoltageTransferPoint.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsConfigLowVoltageTransferPoint", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigHighVoltageTransferPoint.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsConfigHighVoltageTransferPoint", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigShutdownOSDelay.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsConfigShutdownOSDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigUPSBootDelay.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsConfigUPSBootDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsConfigExternalBatteryPack.0 = INTEGER: 0 */
	{ "unmapped.dupsConfigExternalBatteryPack", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputNumLines.0 = INTEGER: 1 */
	{ "unmapped.dupsInputNumLines", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputCurrent1.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsInputCurrent1", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputFrequency2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsInputFrequency2", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputVoltage2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsInputVoltage2", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputCurrent2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsInputCurrent2", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputFrequency3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsInputFrequency3", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputVoltage3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsInputVoltage3", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.9.0", NULL, SU_FLAG_OK, NULL },
	/* dupsInputCurrent3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsInputCurrent3", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.10.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputFrequency.0 = INTEGER: 499 0.1 Hertz */
	{ "unmapped.dupsOutputFrequency", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputNumLines.0 = INTEGER: 1 */
	{ "unmapped.dupsOutputNumLines", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputPower1.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputPower1", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputVoltage2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputVoltage2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputCurrent2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputCurrent2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.9.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputPower2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputPower2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.10.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputLoad2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputLoad2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.11.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputVoltage3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputVoltage3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.12.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputCurrent3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputCurrent3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.13.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputPower3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputPower3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.14.0", NULL, SU_FLAG_OK, NULL },
	/* dupsOutputLoad3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsOutputLoad3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.15.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassFrequency.0 = INTEGER: 499 0.1 Hertz */
	{ "unmapped.dupsBypassFrequency", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassNumLines.0 = INTEGER: 1 */
	{ "unmapped.dupsBypassNumLines", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassVoltage1.0 = INTEGER: 2280 */
	{ "unmapped.dupsBypassVoltage1", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassCurrent1.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassCurrent1", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassPower1.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassPower1", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassVoltage2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassVoltage2", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassCurrent2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassCurrent2", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassPower2.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassPower2", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassVoltage3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassVoltage3", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.9.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassCurrent3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassCurrent3", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.10.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypassPower3.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBypassPower3", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.11.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypass.12.0 = NULL */
	{ "unmapped.dupsBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.12.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypass.13.0 = NULL */
	{ "unmapped.dupsBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.13.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBypass.14.0 = NULL */
	{ "unmapped.dupsBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.14.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBatteryCondiction.0 = INTEGER: good(0) */
	{ "unmapped.dupsBatteryCondiction", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBatteryStatus.0 = INTEGER: ok(0) */
	{ "unmapped.dupsBatteryStatus", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBatteryCharge.0 = INTEGER: charging(1) */
	{ "unmapped.dupsBatteryCharge", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsSecondsOnBattery.0 = INTEGER: 0 seconds */
	{ "unmapped.dupsSecondsOnBattery", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBatteryEstimatedTime.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBatteryEstimatedTime", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBatteryVoltage.0 = INTEGER: 550 0.1 Volt DC */
	{ "unmapped.dupsBatteryVoltage", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBatteryCurrent.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsBatteryCurrent", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsBatteryCapacity.0 = INTEGER: 100 percent */
	{ "unmapped.dupsBatteryCapacity", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsTemperature.0 = INTEGER: 32 degrees Centigrade */
	{ "unmapped.dupsTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.9.0", NULL, SU_FLAG_OK, NULL },
	/* dupsLastReplaceDate.0 = Wrong Type (should be OCTET STRING): NULL */
	{ "unmapped.dupsLastReplaceDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.7.10.0", NULL, SU_FLAG_OK, NULL },
	/* dupsNextReplaceDate.0 = Wrong Type (should be OCTET STRING): NULL */
	{ "unmapped.dupsNextReplaceDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.7.11.0", NULL, SU_FLAG_OK, NULL },
	/* dupsTestType.0 = INTEGER: abort(0) */
	{ "unmapped.dupsTestType", 0, 1, ".1.3.6.1.4.1.2254.2.4.8.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsTestResultsSummary.0 = INTEGER: noTestsInitiated(0) */
	{ "unmapped.dupsTestResultsSummary", 0, 1, ".1.3.6.1.4.1.2254.2.4.8.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsTestResultsDetail.0 = Wrong Type (should be OCTET STRING): NULL */
	{ "unmapped.dupsTestResultsDetail", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.8.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmDisconnect.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmDisconnect", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmPowerFail.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmPowerFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmBatteryLow.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmBatteryLow", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmLoadWarning.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAlarmLoadWarning", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmLoadSeverity.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAlarmLoadSeverity", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmLoadOnBypass.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmLoadOnBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmUPSFault.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmUPSFault", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmBatteryGroundFault.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAlarmBatteryGroundFault", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmTestInProgress.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmTestInProgress", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.9.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmBatteryTestFail.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmBatteryTestFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.10.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmFuseFailure.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmFuseFailure", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.11.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmOutputOverload.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmOutputOverload", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.12.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmOutputOverCurrent.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAlarmOutputOverCurrent", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.13.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmInverterAbnormal.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmInverterAbnormal", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.14.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmRectifierAbnormal.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAlarmRectifierAbnormal", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.15.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmReserveAbnormal.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAlarmReserveAbnormal", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.16.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmLoadOnReserve.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsAlarmLoadOnReserve", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.17.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmOverTemperature.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmOverTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.18.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmOutputBad.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmOutputBad", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.19.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmBypassBad.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmBypassBad", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.20.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmUPSOff.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmUPSOff", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.21.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmChargerFail.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmChargerFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.22.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmFanFail.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmFanFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.23.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmEconomicMode.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmEconomicMode", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.24.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmOutputOff.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmOutputOff", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.25.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmSmartShutdown.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmSmartShutdown", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.26.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmEmergencyPowerOff.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmEmergencyPowerOff", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.27.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmUPSShutdown.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmUPSShutdown", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.28.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvTemperature.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsEnvTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.1.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvHumidity.0 = Wrong Type (should be INTEGER): NULL */
	{ "unmapped.dupsEnvHumidity", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.2.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvSetTemperatureLimit.0 = INTEGER: 40 degrees Centigrade */
	{ "unmapped.dupsEnvSetTemperatureLimit", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.3.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvSetHumidityLimit.0 = INTEGER: 90 percentage */
	{ "unmapped.dupsEnvSetHumidityLimit", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.4.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvSetEnvRelay1.0 = INTEGER: normalOpen(0) */
	{ "unmapped.dupsEnvSetEnvRelay1", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.5.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvSetEnvRelay2.0 = INTEGER: normalOpen(0) */
	{ "unmapped.dupsEnvSetEnvRelay2", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.6.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvSetEnvRelay3.0 = INTEGER: normalOpen(0) */
	{ "unmapped.dupsEnvSetEnvRelay3", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.7.0", NULL, SU_FLAG_OK, NULL },
	/* dupsEnvSetEnvRelay4.0 = INTEGER: normalOpen(0) */
	{ "unmapped.dupsEnvSetEnvRelay4", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.8.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmOverEnvTemperature.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmOverEnvTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.9.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmOverEnvHumidity.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmOverEnvHumidity", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.10.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmEnvRelay1.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmEnvRelay1", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.11.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmEnvRelay2.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmEnvRelay2", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.12.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmEnvRelay3.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmEnvRelay3", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.13.0", NULL, SU_FLAG_OK, NULL },
	/* dupsAlarmEnvRelay4.0 = INTEGER: off(0) */
	{ "unmapped.dupsAlarmEnvRelay4", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.14.0", NULL, SU_FLAG_OK, NULL },
#endif /* #if 0 */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	delta_ups = { "delta_ups", DELTA_UPS_MIB_VERSION, NULL, NULL, delta_ups_mib, DELTA_UPS_SYSOID, NULL };
