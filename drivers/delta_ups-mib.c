/* delta_ups-mib.c - subdriver to monitor delta_ups SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-snmp-subdriver.sh script. It must be customized!
 *
 *  MIB reference: https://www.networkupstools.org/ups-protocols/snmp/DeltaUPSv4.mib
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

#define DELTA_UPS_MIB_VERSION  "0.51"

#define DELTA_UPS_SYSOID       ".1.3.6.1.4.1.2254.2.4"

/* To create a value lookup structure (as needed on the 2nd line of the example
 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
 * static info_lkp_t delta_onbatt_info[] = {
 * 	info_lkp_default(1, "OB"),
 * 	info_lkp_default(2, "OL"),
 * 	info_lkp_sentinel
 * };
 */

static info_lkp_t delta_ups_upstype_info[] = {
	info_lkp_default(1, "on-line"),
	info_lkp_default(2, "off-line"),
	info_lkp_default(3, "line-interactive"),
	info_lkp_default(4, "3phase"),
	info_lkp_default(5, "split-phase"),
	info_lkp_sentinel
};

static info_lkp_t delta_ups_pwr_info[] = {
	info_lkp_default(0, "OL"),	/* normal  */
	info_lkp_default(1, "OB"),	/* battery  */
	info_lkp_default(2, "BYPASS"),	/* bypass */
	info_lkp_default(3, "TRIM"),	/* reducing */
	info_lkp_default(4, "BOOST"),	/* boosting */
	info_lkp_default(5, "BYPASS"),	/* manualBypass */

/*
	info_lkp_default(6, "NULL"),
*/	/* other  */

	info_lkp_default(7, "OFF"),	/* none */
	info_lkp_sentinel
};

/* DELTA_UPS Snmp2NUT lookup table */
static snmp_info_t delta_ups_mib[] = {

	/* Data format:
	 * snmp_info_default(info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar),
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
	 * snmp_info_default("input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL),
	 * snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, delta_ups_onbatt_info),
	 *
	 * To create a value lookup structure (as needed on the 2nd line), use the
	 * following kind of declaration, outside of the present snmp_info_t[]:
	 * static info_lkp_t delta_ups_onbatt_info[] = {
	 * 	info_lkp_default(1, "OB"),
	 * 	info_lkp_default(2, "OL"),
	 * 	info_lkp_sentinel
	 * };
	 */

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* dupsIdentManufacturer.0 = STRING: "Socomec" */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsIdentModel.0 = STRING: "NETYS RT 1/1 UPS" */
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsIdentAgentSoftwareVersion.0 = STRING: "2.0h " */
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsIdentUPSSoftwareVersion.0 = STRING: "1.1" */
	snmp_info_default("ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsType.0 = INTEGER: on-line(1) */
	snmp_info_default("ups.type", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.19.0", NULL, SU_FLAG_OK, delta_ups_upstype_info),
	/* dupsOutputLoad1.0 = INTEGER: 29 */
	snmp_info_default("ups.load", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRatingOutputVA.0 = INTEGER: 2200 */
	snmp_info_default("ups.power", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRatingOutputVoltage.0 = INTEGER: 230 */
	snmp_info_default("output.voltage.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputVoltage1.0 = INTEGER: 2300 */
	snmp_info_default("output.voltage", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.5.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRatingOutputFrequency.0 = INTEGER: 50 */
	snmp_info_default("output.frequency.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.9.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputCurrent1.0 = INTEGER: 23 */
	snmp_info_default("output.current", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.5.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRatingInputVoltage.0 = INTEGER: 230 */
	snmp_info_default("input.voltage.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.10.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputVoltage1.0 = INTEGER: 2280 */
	snmp_info_default("input.voltage", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.4.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRatingInputFrequency.0 = INTEGER: 50 */
	snmp_info_default("input.frequency.nominal", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.11.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputFrequency1.0 = INTEGER: 499 */
	snmp_info_default("input.frequency", 0, 0.1, ".1.3.6.1.4.1.2254.2.4.4.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputSource.0 = INTEGER: normal(0) */
	snmp_info_default("ups.status", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.1.0", NULL, SU_FLAG_OK, delta_ups_pwr_info),

	/* Remaining unmapped variables.
	 * Mostly the first field (string) is to be changed
	 * Check docs/nut-names.txt for the right variable names
	 */
#if WITH_UNMAPPED_DATA_POINTS
	/* dupsIdentName.0 = "" */
	snmp_info_default("unmapped.dupsIdentName", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAttachedDevices.0 = "" */
	snmp_info_default("unmapped.dupsAttachedDevices", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRatingBatteryVoltage.0 = INTEGER: 0 */
	snmp_info_default("unmapped.dupsRatingBatteryVoltage", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.12.0", NULL, SU_FLAG_OK, NULL),
	/* dupsLowTransferVoltUpBound.0 = INTEGER: 0 Volt */
	snmp_info_default("unmapped.dupsLowTransferVoltUpBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.13.0", NULL, SU_FLAG_OK, NULL),
	/* dupsLowTransferVoltLowBound.0 = INTEGER: 0 Volt */
	snmp_info_default("unmapped.dupsLowTransferVoltLowBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.14.0", NULL, SU_FLAG_OK, NULL),
	/* dupsHighTransferVoltUpBound.0 = INTEGER: 0 Volt */
	snmp_info_default("unmapped.dupsHighTransferVoltUpBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.15.0", NULL, SU_FLAG_OK, NULL),
	/* dupsHighTransferVoltLowBound.0 = INTEGER: 0 Volt */
	snmp_info_default("unmapped.dupsHighTransferVoltLowBound", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.16.0", NULL, SU_FLAG_OK, NULL),
	/* dupsLowBattTime.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsLowBattTime", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.17.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutletRelays.0 = INTEGER: 2 */
	snmp_info_default("unmapped.dupsOutletRelays", 0, 1, ".1.3.6.1.4.1.2254.2.4.1.18.0", NULL, SU_FLAG_OK, NULL),
	/* dupsShutdownType.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsShutdownType", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAutoReboot.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAutoReboot", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsShutdownAction.0 = INTEGER: 0 */
	snmp_info_default("unmapped.dupsShutdownAction", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRestartAction.0 = INTEGER: 0 */
	snmp_info_default("unmapped.dupsRestartAction", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsSetOutletRelay.0 = INTEGER: 1 */
	snmp_info_default("unmapped.dupsSetOutletRelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRelayOffDelay.0 = INTEGER: 0 */
	snmp_info_default("unmapped.dupsRelayOffDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsRelayOnDelay.0 = INTEGER: 0 */
	snmp_info_default("unmapped.dupsRelayOnDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.2.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigBuzzerAlarm.0 = INTEGER: alarm(1) */
	snmp_info_default("unmapped.dupsConfigBuzzerAlarm", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigBuzzerState.0 = INTEGER: disable(2) */
	snmp_info_default("unmapped.dupsConfigBuzzerState", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigSensitivity.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsConfigSensitivity", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigLowVoltageTransferPoint.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsConfigLowVoltageTransferPoint", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigHighVoltageTransferPoint.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsConfigHighVoltageTransferPoint", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigShutdownOSDelay.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsConfigShutdownOSDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigUPSBootDelay.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsConfigUPSBootDelay", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsConfigExternalBatteryPack.0 = INTEGER: 0 */
	snmp_info_default("unmapped.dupsConfigExternalBatteryPack", 0, 1, ".1.3.6.1.4.1.2254.2.4.3.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputNumLines.0 = INTEGER: 1 */
	snmp_info_default("unmapped.dupsInputNumLines", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputCurrent1.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsInputCurrent1", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputFrequency2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsInputFrequency2", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputVoltage2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsInputVoltage2", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputCurrent2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsInputCurrent2", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputFrequency3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsInputFrequency3", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputVoltage3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsInputVoltage3", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.9.0", NULL, SU_FLAG_OK, NULL),
	/* dupsInputCurrent3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsInputCurrent3", 0, 1, ".1.3.6.1.4.1.2254.2.4.4.10.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputFrequency.0 = INTEGER: 499 0.1 Hertz */
	snmp_info_default("unmapped.dupsOutputFrequency", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputNumLines.0 = INTEGER: 1 */
	snmp_info_default("unmapped.dupsOutputNumLines", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputPower1.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputPower1", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputVoltage2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputVoltage2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputCurrent2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputCurrent2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.9.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputPower2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputPower2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.10.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputLoad2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputLoad2", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.11.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputVoltage3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputVoltage3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.12.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputCurrent3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputCurrent3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.13.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputPower3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputPower3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.14.0", NULL, SU_FLAG_OK, NULL),
	/* dupsOutputLoad3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsOutputLoad3", 0, 1, ".1.3.6.1.4.1.2254.2.4.5.15.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassFrequency.0 = INTEGER: 499 0.1 Hertz */
	snmp_info_default("unmapped.dupsBypassFrequency", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassNumLines.0 = INTEGER: 1 */
	snmp_info_default("unmapped.dupsBypassNumLines", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassVoltage1.0 = INTEGER: 2280 */
	snmp_info_default("unmapped.dupsBypassVoltage1", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassCurrent1.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassCurrent1", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassPower1.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassPower1", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassVoltage2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassVoltage2", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassCurrent2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassCurrent2", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassPower2.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassPower2", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassVoltage3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassVoltage3", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.9.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassCurrent3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassCurrent3", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.10.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypassPower3.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBypassPower3", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.11.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypass.12.0 = NULL */
	snmp_info_default("unmapped.dupsBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.12.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypass.13.0 = NULL */
	snmp_info_default("unmapped.dupsBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.13.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBypass.14.0 = NULL */
	snmp_info_default("unmapped.dupsBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.6.14.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBatteryCondiction.0 = INTEGER: good(0) */
	snmp_info_default("unmapped.dupsBatteryCondiction", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBatteryStatus.0 = INTEGER: ok(0) */
	snmp_info_default("unmapped.dupsBatteryStatus", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBatteryCharge.0 = INTEGER: charging(1) */
	snmp_info_default("unmapped.dupsBatteryCharge", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsSecondsOnBattery.0 = INTEGER: 0 seconds */
	snmp_info_default("unmapped.dupsSecondsOnBattery", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBatteryEstimatedTime.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBatteryEstimatedTime", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBatteryVoltage.0 = INTEGER: 550 0.1 Volt DC */
	snmp_info_default("unmapped.dupsBatteryVoltage", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBatteryCurrent.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsBatteryCurrent", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsBatteryCapacity.0 = INTEGER: 100 percent */
	snmp_info_default("unmapped.dupsBatteryCapacity", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsTemperature.0 = INTEGER: 32 degrees Centigrade */
	snmp_info_default("unmapped.dupsTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.7.9.0", NULL, SU_FLAG_OK, NULL),
	/* dupsLastReplaceDate.0 = Wrong Type (should be OCTET STRING): NULL */
	snmp_info_default("unmapped.dupsLastReplaceDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.7.10.0", NULL, SU_FLAG_OK, NULL),
	/* dupsNextReplaceDate.0 = Wrong Type (should be OCTET STRING): NULL */
	snmp_info_default("unmapped.dupsNextReplaceDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.7.11.0", NULL, SU_FLAG_OK, NULL),
	/* dupsTestType.0 = INTEGER: abort(0) */
	snmp_info_default("unmapped.dupsTestType", 0, 1, ".1.3.6.1.4.1.2254.2.4.8.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsTestResultsSummary.0 = INTEGER: noTestsInitiated(0) */
	snmp_info_default("unmapped.dupsTestResultsSummary", 0, 1, ".1.3.6.1.4.1.2254.2.4.8.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsTestResultsDetail.0 = Wrong Type (should be OCTET STRING): NULL */
	snmp_info_default("unmapped.dupsTestResultsDetail", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.2254.2.4.8.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmDisconnect.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmDisconnect", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmPowerFail.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmPowerFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmBatteryLow.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmBatteryLow", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmLoadWarning.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAlarmLoadWarning", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmLoadSeverity.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAlarmLoadSeverity", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmLoadOnBypass.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmLoadOnBypass", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmUPSFault.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmUPSFault", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmBatteryGroundFault.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAlarmBatteryGroundFault", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmTestInProgress.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmTestInProgress", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.9.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmBatteryTestFail.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmBatteryTestFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.10.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmFuseFailure.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmFuseFailure", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.11.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmOutputOverload.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmOutputOverload", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.12.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmOutputOverCurrent.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAlarmOutputOverCurrent", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.13.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmInverterAbnormal.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmInverterAbnormal", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.14.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmRectifierAbnormal.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAlarmRectifierAbnormal", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.15.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmReserveAbnormal.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAlarmReserveAbnormal", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.16.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmLoadOnReserve.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsAlarmLoadOnReserve", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.17.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmOverTemperature.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmOverTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.18.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmOutputBad.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmOutputBad", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.19.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmBypassBad.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmBypassBad", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.20.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmUPSOff.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmUPSOff", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.21.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmChargerFail.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmChargerFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.22.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmFanFail.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmFanFail", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.23.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmEconomicMode.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmEconomicMode", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.24.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmOutputOff.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmOutputOff", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.25.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmSmartShutdown.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmSmartShutdown", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.26.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmEmergencyPowerOff.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmEmergencyPowerOff", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.27.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmUPSShutdown.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmUPSShutdown", 0, 1, ".1.3.6.1.4.1.2254.2.4.9.28.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvTemperature.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsEnvTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.1.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvHumidity.0 = Wrong Type (should be INTEGER): NULL */
	snmp_info_default("unmapped.dupsEnvHumidity", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.2.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvSetTemperatureLimit.0 = INTEGER: 40 degrees Centigrade */
	snmp_info_default("unmapped.dupsEnvSetTemperatureLimit", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.3.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvSetHumidityLimit.0 = INTEGER: 90 percentage */
	snmp_info_default("unmapped.dupsEnvSetHumidityLimit", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.4.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvSetEnvRelay1.0 = INTEGER: normalOpen(0) */
	snmp_info_default("unmapped.dupsEnvSetEnvRelay1", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.5.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvSetEnvRelay2.0 = INTEGER: normalOpen(0) */
	snmp_info_default("unmapped.dupsEnvSetEnvRelay2", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.6.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvSetEnvRelay3.0 = INTEGER: normalOpen(0) */
	snmp_info_default("unmapped.dupsEnvSetEnvRelay3", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.7.0", NULL, SU_FLAG_OK, NULL),
	/* dupsEnvSetEnvRelay4.0 = INTEGER: normalOpen(0) */
	snmp_info_default("unmapped.dupsEnvSetEnvRelay4", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.8.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmOverEnvTemperature.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmOverEnvTemperature", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.9.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmOverEnvHumidity.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmOverEnvHumidity", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.10.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmEnvRelay1.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmEnvRelay1", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.11.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmEnvRelay2.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmEnvRelay2", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.12.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmEnvRelay3.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmEnvRelay3", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.13.0", NULL, SU_FLAG_OK, NULL),
	/* dupsAlarmEnvRelay4.0 = INTEGER: off(0) */
	snmp_info_default("unmapped.dupsAlarmEnvRelay4", 0, 1, ".1.3.6.1.4.1.2254.2.4.10.14.0", NULL, SU_FLAG_OK, NULL),
#endif	/* #if WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	delta_ups = { "delta_ups", DELTA_UPS_MIB_VERSION, NULL, NULL, delta_ups_mib, DELTA_UPS_SYSOID, NULL };
