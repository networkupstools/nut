/* liebert-voyager-mib.c - subdriver to monitor liebert_voyager SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2016	Arnaud Quette <arnaud.quette@free.fr>
 *  2019        EATON (author: Arnaud Quette <ArnaudQuette@eaton.com>)
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

#include "liebert-voyager-mib.h"

#define LIEBERT_VOYAGER_MIB_VERSION  "0.1"

#define LIEBERT_VOYAGER_SYSOID       ".1.3.6.1.4.1.476.1.1.1.14"


static info_lkp_t liebert_voyager_alarm_ob[] = {
	{ 1, "OB" },
	{ 0, NULL }
} ;

static info_lkp_t liebert_voyager_alarm_lb[] = {
	{ 1, "LB" },
	{ 0, NULL }
} ;

static info_lkp_t liebert_voyager_alarm_lb[] = {
	{ 1, "LB" },
	{ 0, NULL }
} ;
static info_lkp_t liebert_voyager_alarm_off[] = {
	{ 1, "OFF" },
	{ 0, NULL }
} ;

static info_lkp_t liebert_voyager_bypass_info[] = {
	/* { 1, "??"}, */
	{ 2, "BYPASS" },
	{ 0, NULL }
} ;

/* LIEBERT_VOYAGER Snmp2NUT lookup table */
static snmp_info_t liebert_voyager_mib[] = {

	/* Device Page */
	/* lcUpsIdentManufacturer.0 = STRING: "Liebert" */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsIdentModel.0 = STRING: "GXT2-1500RT120                " */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsIdentSerialNumber.0 = STRING: "0219100012AF051     " */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.1.6.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsIdentSoftwareVersion.0 = STRING: "2.7.3 " */
	{ "device.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsIdentSpecific.0 = OID: luUPStationGxt */
	/* { "unmapped.lcUpsIdentSpecific", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.1.4.0", NULL, SU_FLAG_OK, NULL }, */
	/* lcUpsIdentFirmwareVersion.0 = STRING: "GXT2MR14        " */
	{ "device.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.1.5.0", NULL, SU_FLAG_OK, NULL },

	/* UPS Page */
	/* lcUpsOutputLoad.0 = INTEGER: 35 */
	{ "ups.load", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsTestBatteryStatus.0 = INTEGER: passed(2) */
	{ "ups.test.result", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.7.2.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsIdentManufactureDate.0 = STRING: "05JUL02 " */
	/* { "unmapped.lcUpsIdentManufactureDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.1.7.0", NULL, SU_FLAG_OK, NULL },*/

	/* OL is the default for the UPS and has no explicit OID in the MIB.
	 * It can be inferred by a lack of the lcUpsAlarmOnBattery alarm, lcUpsAlarmUpsOff
	 * alarm and the lcUpsInverter state of on(2).
	 * OB is indicated by the alarm lcUpsAlarmOnBattery */
	/* FIXME: also check for lcUpsInverter==on */
	/* lcUpsAlarmOnBattery */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.6.3.10", "OL",
		SU_STATUS_BATT, &liebert_voyager_alarm_ob[0] },
	/* lcUpsAlarmLowBatteryWarning */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.6.3.1", "",
		SU_STATUS_BATT, &liebert_voyager_alarm_lb[0] },
	/* lcUpsOnBypass */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.13.1", "",
		SU_STATUS_BATT, &liebert_voyager_bypass_info[0] },
	/* lcUpsAlarmUpsOff */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.476.1.1.1.1.6.3.12", "",
		SU_STATUS_BATT, &liebert_voyager_alarm_off[0] },

	/* Battery Page */
	/* lcUpsBatTimeRemaining.0 = INTEGER: 19 */
	{ "battery.runtime", 0, 60, ".1.3.6.1.4.1.476.1.1.1.1.2.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsBatVoltage.0 = INTEGER: 54 */
	{ "battery.voltage", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.2.3.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsBatCapacity.0 = INTEGER: 100 */
	{ "battery.charge", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.2.6.0", NULL, SU_FLAG_OK, NULL },

	/* Input Page */
	/* lcUpsInputFrequency.0 = INTEGER: 60 */
	{ "input.frequency", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsInputNumLines.0 = INTEGER: 1 */
	{ "input.phases", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsInputLine.1 = INTEGER: 1 */
	/* { "unmapped.lcUpsInputLine", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.3.6.1.1.1", NULL, SU_FLAG_OK, NULL }, */
	/* lcUpsInputVoltage.1 = INTEGER: 121 */
	{ "input.voltage", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.3.6.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* lcUpsNominalInputVoltage.0 = INTEGER: 120 */
	{ "input.voltage.nominal", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.9.2.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsNominalInputFreq.0 = INTEGER: 60 */
	{ "input.frequency.nominal", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.9.6.0", NULL, SU_FLAG_OK, NULL },

	/* Output Page */
	/* lcUpsOutputFrequency.0 = INTEGER: 60 */
	{ "output.frequency", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsOutputNumLines.0 = INTEGER: 1 */
	{ "output.phases", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.4.3.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsOutputLine.1 = INTEGER: 1 */
	/* { "unmapped.lcUpsOutputLine", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.4.4.1.1.1", NULL, SU_FLAG_OK, NULL },*/
	/* lcUpsOutputVoltage.1 = INTEGER: 120 */
	{ "output.voltage", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.4.4.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* lcUpsOutputCurrent.1 = INTEGER: 3 */
	{ "output.current", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.4.4.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* lcUpsOutputWatts.0 = INTEGER: 375 */
	{ "output.realpower", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.4.5.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsNominalOutputVoltage.0 = INTEGER: 120 */
	{ "output.voltage.nominal", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.9.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsNominalOutputFreq.0 = INTEGER: 60 */
	{ "output.frequency.nominal", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.9.5.0", NULL, SU_FLAG_OK, NULL },
	/* FIXME: should be removed in favor of output.power.nominal */
	/* lcUpsNominalOutputVaRating.0 = INTEGER: 1500 */
	{ "ups.power.nominal", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.9.7.0", NULL, SU_FLAG_OK, NULL },
	/* FIXME: should be removed in favor of output.power.nominal */
	/* lcUpsNominalOutputWattsRating.0 = INTEGER: 1050 */
	{ "ups.realpower.nominal", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.9.8.0", NULL, SU_FLAG_OK, NULL },

#if 0
	/* lcUpsInverterStatus.0 = INTEGER: on(2) */
?	{ "unmapped.lcUpsInverterStatus", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.5.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsAlarms.0 = Gauge32: 0 */
?	{ "unmapped.lcUpsAlarms", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.6.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsTestBattery.0 = INTEGER: unknown(1) */
?	{ "unmapped.lcUpsTestBattery", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.7.1.0", NULL, SU_FLAG_OK, NULL },

	/* lcUpsControlOutputOffDelay.0 = INTEGER: 0 */
	{ "unmapped.lcUpsControlOutputOffDelay", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsControlOutputOnDelay.0 = INTEGER: 0 */
	{ "unmapped.lcUpsControlOutputOnDelay", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.2.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsControlOutputOffTrapDelay.0 = INTEGER: 0 */
	{ "unmapped.lcUpsControlOutputOffTrapDelay", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.3.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsControlOutputOnTrapDelay.0 = INTEGER: 0 */
	{ "unmapped.lcUpsControlOutputOnTrapDelay", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.4.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsControlUnixShutdownDelay.0 = INTEGER: 0 */
	{ "unmapped.lcUpsControlUnixShutdownDelay", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.5.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsControlUnixShutdownTrapDelay.0 = INTEGER: 0 */
	{ "unmapped.lcUpsControlUnixShutdownTrapDelay", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.6.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsControlCancelCommands.0 = INTEGER: unknown(1) */
	{ "unmapped.lcUpsControlCancelCommands", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.7.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsControlRebootAgentDelay.0 = INTEGER: 0 */
	{ "unmapped.lcUpsControlRebootAgentDelay", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.8.8.0", NULL, SU_FLAG_OK, NULL },

	/* lcUpsOnBypass.0 = INTEGER: no(3) */
	{ "unmapped.lcUpsOnBypass", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.13.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsBypassFrequency.0 = INTEGER: 60 */
	{ "unmapped.lcUpsBypassFrequency", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.13.2.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsBypassNumLines.0 = INTEGER: 1 */
	{ "unmapped.lcUpsBypassNumLines", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.13.3.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsBypassLine.1 = INTEGER: 1 */
	{ "unmapped.lcUpsBypassLine", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.13.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* lcUpsBypassVoltage.1 = INTEGER: 122 */
	{ "unmapped.lcUpsBypassVoltage", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.13.4.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* lcUpsConfigType.0 = INTEGER: online(2) */
	{ "unmapped.lcUpsConfigType", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.14.1.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsConfigBypassInstalled.0 = INTEGER: yes(2) */
	{ "unmapped.lcUpsConfigBypassInstalled", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.14.2.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsConfigModuleCount.0 = INTEGER: 0 */
	{ "unmapped.lcUpsConfigModuleCount", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.14.3.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsConfigAudibleStatus.0 = INTEGER: enabled(2) */
	{ "unmapped.lcUpsConfigAudibleStatus", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.14.5.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsConfigLowBattTime.0 = INTEGER: 2 */
	{ "unmapped.lcUpsConfigLowBattTime", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.14.6.0", NULL, SU_FLAG_OK, NULL },
	/* lcUpsConfigAutoRestart.0 = INTEGER: 0 */
	{ "unmapped.lcUpsConfigAutoRestart", 0, 1, ".1.3.6.1.4.1.476.1.1.1.1.14.7.0", NULL, SU_FLAG_OK, NULL },
#endif
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	liebert_voyager = { "liebert_voyager", LIEBERT_VOYAGER_MIB_VERSION, NULL, NULL, liebert_voyager_mib, LIEBERT_VOYAGER_SYSOID };
