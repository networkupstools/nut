/* eaton-pdu-nlogic-mib.c - subdriver to monitor eaton-pdu-nlogic SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2016	Arnaud Quette <arnaud.quette@free.fr>
 *	2022        Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
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

#include "eaton-pdu-nlogic-mib.h"

#define EATON_PDU_NLOGIC_MIB_VERSION  "0.10"

#define EATON_PDU_NLOGIC_SYSOID       ".1.3.6.1.4.1.534.7.1"

static info_lkp_t eaton_nlogic_unit_switchability_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "no"),
	info_lkp_sentinel
};

static info_lkp_t eaton_nlogic_outlet_status_info[] = {
	info_lkp_default(1, "off"),
	info_lkp_default(2, "on"),
	info_lkp_default(3, "pendingOff"),	/* transitional status */
	info_lkp_default(4, "pendingOn"),	/* transitional status */
	info_lkp_sentinel
};

/* Note: same as marlin_outlet_type_info + i5-20R */
static info_lkp_t eaton_nlogic_outlet_type_info[] = {
	info_lkp_default(0, "unknown"),
	info_lkp_default(1, "iecC13"),
	info_lkp_default(2, "iecC19"),
	info_lkp_default(3, "i5-20R"),
	info_lkp_default(10, "uk"),
	info_lkp_default(11, "french"),
	info_lkp_default(12, "schuko"),
	info_lkp_default(20, "nema515"),
	info_lkp_default(21, "nema51520"),
	info_lkp_default(22, "nema520"),
	info_lkp_default(23, "nemaL520"),
	info_lkp_default(24, "nemaL530"),
	info_lkp_default(25, "nema615"),
	info_lkp_default(26, "nema620"),
	info_lkp_default(27, "nemaL620"),
	info_lkp_default(28, "nemaL630"),
	info_lkp_default(29, "nemaL715"),
	info_lkp_default(30, "rf203p277"),
	info_lkp_sentinel
};


/* EATON_PDU_NLOGIC Snmp2NUT lookup table */
static snmp_info_t eaton_pdu_nlogic_mib[] = {

/* Data format:
 * { info_type, info_flags, info_len, OID, dfl, flags, oid2info },
 *
 *	info_type:	NUT INFO_ or CMD_ element name
 *	info_flags:	flags to set in addinfo
 *	info_len:	length of strings if ST_FLAG_STRING, multiplier otherwise
 *	OID: SNMP OID or NULL
 *	dfl: default value
 *	flags: snmp-ups internal flags (FIXME: ...)
 *	oid2info: lookup table between OID and NUT values
 */


/* standard MIB items; if the vendor MIB contains better OIDs for
 * this (e.g. with daisy-chain support), consider adding those here
 */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE,
		".1.3.6.1.2.1.1.1.0",
		NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE,
		".1.3.6.1.2.1.1.4.0",
		NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE,
		".1.3.6.1.2.1.1.6.0",
		NULL, SU_FLAG_OK, NULL),

	/* Device collection */
	/* pduManufacturer.1 = STRING: "EATON" */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.1.2.1.4.1",
		"EATON", SU_FLAG_STATIC, NULL),
	/* pduModel.1 = STRING: "200-240V, 24A, 5.0kVA, 50/60Hz" */
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.1.2.1.3.1",
		"Eaton ePDU", SU_FLAG_STATIC, NULL),
	/* pduSerialNumber.1 = STRING: "WMEL0046" */
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.1.2.1.8.1",
		NULL, SU_FLAG_STATIC, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE,
		NULL,
		"pdu", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	/* pduPartNumber.1 = STRING: "EMSV0001" */
	snmp_info_default("device.part", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.1.2.1.7.1",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* For daisychain, there is only 1 physical interface! */
	/* pduMACAddress.1 = Hex-STRING: 43 38 2D 34 35 2D 34 34 2D 33 30 2D 39 34 2D 31 */
	snmp_info_default("device.macaddr", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.1.2.1.14.1",
		"", SU_FLAG_STATIC, NULL),

	/* Number of daisychained units is processed according to present units
	 * in the chain with new G3 firmware (02.00.0051, since autumn 2017):
	 * Take string "unitsPresent" (ex: "0,3,4,5"), and count the amount
	 * of "," separators+1 using an inline function */
	/* FIXME: inline func */
	/* pduNumberPDU.0 = INTEGER: 1 */
	snmp_info_default("device.count", 0, 1,
		".1.3.6.1.4.1.534.7.1.1.1.0",
		"0", SU_FLAG_STATIC,
		NULL /* &marlin_device_count_info[0] */ /* devices_count */),

	/* FIXME: this entry should be SU_FLAG_SEMI_STATIC */
	/* pduFirmwareVersion.1 = STRING: "1.0.6" */
	snmp_info_default("device.firmware", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.1.2.1.5.1",
		"", SU_FLAG_OK, NULL),
	/* pduFirmwareVersionTimeStamp.1 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	/* snmp_info_default("unmapped.pduFirmwareVersionTimeStamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.1.2.1.6.1", NULL, SU_FLAG_OK, NULL), */
	/* pduIdentIndex.1 = INTEGER: 1 */
	/* snmp_info_default("unmapped.pduIdentIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.1.2.1.1.1", NULL, SU_FLAG_OK, NULL), */
	/* pduName.1 = "" */
	/* FIXME: RFC device.name? */
	snmp_info_default("device.name", ST_FLAG_STRING | ST_FLAG_RW, 63,
		".1.3.6.1.4.1.534.7.1.1.2.1.2.1",
		NULL, SU_FLAG_OK, NULL),
	/* pduStatus.1 = INTEGER: 4 */
	/* snmp_info_default("unmapped.pduStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.1.2.1.9.1", NULL, SU_FLAG_OK, NULL), */

	/* Input collection */
	/* pduInputPhaseCount.1 = INTEGER: 1 */
	snmp_info_default("input.phases", 0, 1,
		".1.3.6.1.4.1.534.7.1.1.2.1.11.1",
		NULL, SU_FLAG_OK, NULL),
	/* pduInputType.1 = INTEGER: 1 */
	/* snmp_info_default("unmapped.pduInputType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.1.1", NULL, SU_FLAG_OK, NULL), */
	/* pduInputFrequency.1 = INTEGER: 499 */
	snmp_info_default("input.frequency", 0, 0.1,
		".1.3.6.1.4.1.534.7.1.2.1.1.2.1",
		NULL, 0, NULL),
	/* pduInputTotalCurrent.1 = INTEGER: 0 */
	snmp_info_default("input.current", 0, 0.01, ".1.3.6.1.4.1.534.7.1.2.1.1.11.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltage.1.1 = INTEGER: 2418 */
	snmp_info_default("input.voltage", 0, 0.1,
		".1.3.6.1.4.1.534.7.1.2.2.1.3.1.1",
		NULL, SU_FLAG_OK, NULL),

	/* Outlet groups collection */
	/* pduGroupCount.1 = INTEGER: 1 */
	snmp_info_default("outlet.group.count", 0, 1,
		".1.3.6.1.4.1.534.7.1.1.2.1.12.1",
		NULL, SU_FLAG_STATIC, NULL),
	/* pduGroupVoltage.1.1 = INTEGER: 2418 */
	snmp_info_default("outlet.group.%i.voltage", 0, 0.1,
		".1.3.6.1.4.1.534.7.1.3.1.1.5.1.%i",
		NULL, SU_OUTLET_GROUP, NULL),
	/* pduGroupCurrent.1.1 = INTEGER: 0 */
	snmp_info_default("outlet.group.%i.current", 0, 1,
		".1.3.6.1.4.1.534.7.1.3.1.1.12.1.%i",
		NULL, SU_OUTLET_GROUP, NULL),

	/* Outlet collection */
	/* pduOutletCount.1 = INTEGER: 6 */
	snmp_info_default("outlet.count", 0, 1,
		".1.3.6.1.4.1.534.7.1.1.2.1.13.1",
		NULL, SU_FLAG_OK, NULL),
	/* pduControllable.1 = INTEGER: 1 */
	snmp_info_default("outlet.switchable", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.1.2.1.10.1",
		"no", SU_FLAG_STATIC,
		&eaton_nlogic_unit_switchability_info[0]),
	/* pduOutletControlSwitchable.1.1 = INTEGER: 2 */
	snmp_info_default("outlet.%i.switchable", ST_FLAG_RW |ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.5.2.1.8.1.%i",
		"no", SU_OUTLET,
		&eaton_nlogic_unit_switchability_info[0]),
	/* pduOutletControlStatus.1.1 = INTEGER: 2 */
	snmp_info_default("outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.5.2.1.1.1.%i",
		NULL, SU_OUTLET,
		&eaton_nlogic_outlet_status_info[0]),
	/* pduOutletName.1.1 = STRING: "OUTLET 1" */
	snmp_info_default("outlet.%i.name", ST_FLAG_RW |ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.5.1.1.2.1.%i",
		NULL, SU_OUTLET, NULL),
	/* pduOutletType.1.1 = INTEGER: 2 */
	snmp_info_default("outlet.%i.type", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.7.1.5.1.1.3.1.%i",
		"unknown", SU_FLAG_STATIC | SU_OUTLET,
		&eaton_nlogic_outlet_type_info[0]),
	/* pduOutletCurrentRating.1.1 = INTEGER: 1600 */
	snmp_info_default("outlet.%i.current.nominal", 0, 0.01,
		".1.3.6.1.4.1.534.7.1.5.1.1.4.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pduOutletCurrent.1.1 = INTEGER: 0 */
	snmp_info_default("outlet.%i.current", 0, 0.01,
		".1.3.6.1.4.1.534.7.1.5.1.1.5.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pduOutletCurrentPercentLoad.1.1 = INTEGER: 0 */
	snmp_info_default("outlet.%i.load", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.1.1.11.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pduOutletVA.1.1 = INTEGER: 0 */
	snmp_info_default("outlet.%i.power", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.1.1.12.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pduOutletWatts.1.1 = INTEGER: 0 */
	snmp_info_default("outlet.%i.realpower", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.1.1.13.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),

	/* instant commands. */
	/* pduOutletControlCommand.1.1 = INTEGER: 2 */
	snmp_info_default("outlet.%i.load.off", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.10.1.%i",
		"1", SU_TYPE_CMD | SU_OUTLET, NULL),
	snmp_info_default("outlet.%i.load.on", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.10.1.%i",
		"2", SU_TYPE_CMD | SU_OUTLET, NULL),
	snmp_info_default("outlet.%i.load.cycle", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.10.1.%i",
		"5", SU_TYPE_CMD | SU_OUTLET, NULL),

	/* Per-outlet shutdown / startup delay (configuration point, not the timers)
	 * (by default each output socket startup is delayed by its number in seconds)
	 */
	/* pduOutletControlShutoffDelay.1.1 = INTEGER: 0 */
	snmp_info_default("outlet.%i.delay.shutdown", ST_FLAG_RW, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.9.1.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET, NULL),
	/* pduOutletControlSequenceDelay.1.1 = INTEGER: 0 */
	snmp_info_default("outlet.%i.delay.start", ST_FLAG_RW, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.6.1.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET, NULL),
	/* FIXME: need RFC! */
	/* pduOutletControlRebootOffTime.1.1 = INTEGER: 5 */
	snmp_info_default("outlet.%i.delay.reboot", ST_FLAG_RW, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.7.1.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET, NULL),


#if 0
	/* FIXME: how to deal with these?
		Delays are in 1 OID and command in another.
		These versions should take the delay, set the related OID (need the outlet index, so miss context!) and then call the command */
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	snmp_info_default("outlet.%i.load.off.delay", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.10.1.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET, NULL), // + set "outlet.%i.delay.shutdown" + set itself to delayedOff (3)
		// &eaton_nlogic_outlet_delayed_off_info[0]
	snmp_info_default("outlet.%i.load.on.delay", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.10.1.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET, NULL), // + set "outlet.%i.delay.start" + set itself to delayedOn (4),
		// &eaton_nlogic_outlet_delayed_on_info[0]
	snmp_info_default("outlet.%i.load.cycle.delay", 0, 1,
		".1.3.6.1.4.1.534.7.1.5.2.1.10.1.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET, NULL), // + set "outlet.%i.delay.start" + set itself to delayedReboot (6),
		// &eaton_nlogic_outlet_delayed_reboot_info[0]
#endif

#if WITH_UNMAPPED_DATA_POINTS
	/* pduIPv4Address.1 = IpAddress: 192.168.1.55 */
	snmp_info_default("unmapped.pduIPv4Address", 0, 1, ".1.3.6.1.4.1.534.7.1.1.2.1.15.1", NULL, SU_FLAG_OK, NULL),
	/* pduIPv6Address.1 = STRING: "FE80::CA45:44FF:FE30:9414" */
	snmp_info_default("unmapped.pduIPv6Address", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.1.2.1.16.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigSsh.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduConfigSsh", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigFtps.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduConfigFtps", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigHttp.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduConfigHttp", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigHttps.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduConfigHttps", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigIPv4IPv6Switch.1 = INTEGER: 3 */
	snmp_info_default("unmapped.pduConfigIPv4IPv6Switch", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigRedfishAPI.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduConfigRedfishAPI", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigOledDispalyOrientation.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduConfigOledDispalyOrientation", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigEnergyReset.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduConfigEnergyReset", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* pduConfigNetworkManagementCardReset.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduConfigNetworkManagementCardReset", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.10.1", NULL, SU_FLAG_OK, NULL),

	/* pduConfigDaisyChainStatus.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduConfigDaisyChainStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.1.3.1.11.1", NULL, SU_FLAG_OK, NULL),

	/* pduInputFrequencyStatus.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduInputFrequencyStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerVA.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerWatts.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputTotalEnergy.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputTotalEnergy", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerWattHourTimer.1 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduInputPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.2.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputResettableEnergy.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputResettableEnergy", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerFactor.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerVAR.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.2.1.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseIndex.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduInputPhaseIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseIndex.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduInputPhaseVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageMeasType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.2.1.3", NULL, SU_FLAG_OK, NULL),

	/* pduInputPhaseVoltage.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltage", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltage.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltage", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThStatus.1.1 = INTEGER: 5 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThLowerWarning.1.1 = INTEGER: 1900 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThLowerCritical.1.1 = INTEGER: 1800 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThUpperWarning.1.1 = INTEGER: 2150 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThUpperCritical.1.1 = INTEGER: 2250 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduInputPhaseCurrentMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentMeasType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentRating.1.1 = INTEGER: 2400 */
	snmp_info_default("unmapped.pduInputPhaseCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentRating.1.2 = INTEGER: 2400 */
	snmp_info_default("unmapped.pduInputPhaseCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.10.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentRating.1.3 = INTEGER: 2400 */
	snmp_info_default("unmapped.pduInputPhaseCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.10.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrent.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrent", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.11.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrent.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrent", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.11.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrent.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrent", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.11.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.12.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.12.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.12.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.13.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.13.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.13.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.14.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.14.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.14.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThUpperWarning.1.1 = INTEGER: 2100 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.15.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.15.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.15.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThUpperCritical.1.1 = INTEGER: 2400 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.16.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.16.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.16.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentPercentLoad.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.17.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentPercentLoad.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.17.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentPercentLoad.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.17.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduInputPhasePowerMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.18.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerMeasType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.18.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.18.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerVA.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.19.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerVA.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.19.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerVA.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.19.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWatts.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.20.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWatts.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.20.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWatts.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.20.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWattHour.1.1 = INTEGER: 30 */
	snmp_info_default("unmapped.pduInputPhasePowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.21.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWattHour.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.21.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWattHour.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.21.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWattHourTimer.1.1 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduInputPhasePowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.2.2.1.22.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWattHourTimer.1.2 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduInputPhasePowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.2.2.1.22.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerWattHourTimer.1.3 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduInputPhasePowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.2.2.1.22.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerFactor.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.23.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerFactor.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.23.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerFactor.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.23.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerVAR.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.24.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerVAR.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.24.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhasePowerVAR.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhasePowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.24.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThResetThld.1.1 = INTEGER: 20 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.25.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThResetThld.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.25.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThResetThld.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.25.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThChangeDelay.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.26.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThChangeDelay.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.26.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThChangeDelay.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.26.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThCtrl.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.27.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThCtrl.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.27.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseVoltageThCtrl.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.27.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThResetThld.1.1 = INTEGER: 100 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.28.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThResetThld.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.28.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThResetThld.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.28.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThChangeDelay.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.29.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThChangeDelay.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.29.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThChangeDelay.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.29.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThCtrl.1.1 = INTEGER: 12 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.30.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThCtrl.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.30.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduInputPhaseCurrentThCtrl.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPhaseCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.30.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerThresholdThLowerWarning.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerThresholdThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.31.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerThresholdThLowerCritical.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerThresholdThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.32.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerThresholdThUpperWarning.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerThresholdThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.33.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerThresholdThUpperCritical.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerThresholdThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.34.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerThresholdThResetThld.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerThresholdThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.35.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerThresholdThChangeDelay.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputPowerThresholdThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.36.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputPowerThresholdThCtrl.1 = INTEGER: 15 */
	snmp_info_default("unmapped.pduInputPowerThresholdThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.37.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputEnergyThresholdThUpperWarning.1 = INTEGER: 2147483 */
	snmp_info_default("unmapped.pduInputEnergyThresholdThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.38.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputEnergyThresholdThUpperCritical.1 = INTEGER: 2147483 */
	snmp_info_default("unmapped.pduInputEnergyThresholdThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.39.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputEnergyThresholdThResetThld.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputEnergyThresholdThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.40.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputEnergyThresholdThChangeDelay.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduInputEnergyThresholdThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.41.1", NULL, SU_FLAG_OK, NULL),
	/* pduInputEnergyThresholdThCtrl.1 = INTEGER: 3 */
	snmp_info_default("unmapped.pduInputEnergyThresholdThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.2.2.1.42.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.3 = INTEGER: 3 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.4 = INTEGER: 4 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.5 = INTEGER: 5 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.6 = INTEGER: 6 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.7 = INTEGER: 7 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.8 = INTEGER: 8 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.9 = INTEGER: 9 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.10 = INTEGER: 10 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.11 = INTEGER: 11 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupIndex.1.12 = INTEGER: 12 */
	snmp_info_default("unmapped.pduGroupIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.1.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.1 = Hex-STRING: 42 31 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.2 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.3 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.4 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.5 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.6 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.7 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.8 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.9 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.10 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.11 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupName.1.12 = Hex-STRING: 00 */
	snmp_info_default("unmapped.pduGroupName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.2.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.1 = INTEGER: 5 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupType.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.3.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageMeasType.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.4.1.12", NULL, SU_FLAG_OK, NULL),

	/* pduGroupVoltageThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThStatus.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.6.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.7.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThLowerCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.8.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.9.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThUpperCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.10.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdugroupCurrentRating.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdugroupCurrentRating", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.11.1.12", NULL, SU_FLAG_OK, NULL),

	/* pduGroupCurrentThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThStatus.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.13.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.14.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThLowerCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.15.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.16.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThUpperCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.17.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentPercentLoad.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.18.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVA.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVA", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.19.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWatts.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWatts", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.20.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHour.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.21.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.1 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.2 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.3 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.4 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.5 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.6 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.7 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.8 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.9 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.10 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.11 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerWattHourTimer.1.12 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduGroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.3.1.1.22.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.1 = INTEGER: 100 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerFactor.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.23.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupPowerVAR.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupPowerVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.24.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.1 = INTEGER: 6 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupOutletCount.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupOutletCount", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.25.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupBreakerStatus.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.26.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupVoltageThCtrl.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupVoltageThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.27.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduGroupCurrentThCtrl.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduGroupCurrentThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.3.1.1.28.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureScale.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduTemperatureScale", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureCount", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityCount", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* pduDoorCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDoorCount", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* pduDryCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDryCount", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* pduSpotCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduSpotCount", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* pduRopeCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduRopeCount", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidCount", 0, 1, ".1.3.6.1.4.1.534.7.1.4.1.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureIndex.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureIndex.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureIndex.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureIndex.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureName.1.1 = STRING: "                 " */
	snmp_info_default("unmapped.pduTemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.2.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pduTemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.2.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureName.1.3 = STRING: "                 " */
	snmp_info_default("unmapped.pduTemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.2.1.2.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureName.1.4 = STRING: "                 " */
	snmp_info_default("unmapped.pduTemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.2.1.2.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureName.1.5 = STRING: "                 " */
	snmp_info_default("unmapped.pduTemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.2.1.2.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureName.1.6 = STRING: "                 " */
	snmp_info_default("unmapped.pduTemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.2.1.2.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureProbeStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduTemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pduTemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureProbeStatus.1.3 = INTEGER: 1 */
	snmp_info_default("unmapped.pduTemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureProbeStatus.1.4 = INTEGER: 1 */
	snmp_info_default("unmapped.pduTemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.3.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureProbeStatus.1.5 = INTEGER: 1 */
	snmp_info_default("unmapped.pduTemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.3.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureProbeStatus.1.6 = INTEGER: 1 */
	snmp_info_default("unmapped.pduTemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.3.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureValue.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureValue.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureValue.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureValue.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureValue.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureValue.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThStatus.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThStatus.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.5.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThStatus.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.5.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThStatus.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.5.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.9.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.9.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.9.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThCtrl.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThCtrl.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.10.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThCtrl.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.10.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThCtrl.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.10.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThCtrl.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.10.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduTemperatureThCtrl.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduTemperatureThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.2.1.10.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityIndex.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityIndex.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityIndex.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityIndex.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityName.1.1 = STRING: "                 " */
	snmp_info_default("unmapped.pduHumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.3.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pduHumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.3.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityName.1.3 = STRING: "                 " */
	snmp_info_default("unmapped.pduHumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.3.1.2.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityName.1.4 = STRING: "                 " */
	snmp_info_default("unmapped.pduHumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.3.1.2.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityName.1.5 = STRING: "                 " */
	snmp_info_default("unmapped.pduHumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.3.1.2.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityName.1.6 = STRING: "                 " */
	snmp_info_default("unmapped.pduHumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.3.1.2.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityProbeStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduHumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pduHumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityProbeStatus.1.3 = INTEGER: 1 */
	snmp_info_default("unmapped.pduHumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityProbeStatus.1.4 = INTEGER: 1 */
	snmp_info_default("unmapped.pduHumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.3.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityProbeStatus.1.5 = INTEGER: 1 */
	snmp_info_default("unmapped.pduHumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.3.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityProbeStatus.1.6 = INTEGER: 1 */
	snmp_info_default("unmapped.pduHumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.3.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityValue.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityValue.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityValue.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityValue.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityValue.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityValue.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityValue", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThStatus.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThStatus.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.5.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThStatus.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.5.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThStatus.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.5.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.9.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.9.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.9.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThCtrl.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThCtrl.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.10.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThCtrl.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.10.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThCtrl.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.10.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThCtrl.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.10.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHumidityThCtrl.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHumidityThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.4.3.1.10.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduDoorIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDoorIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.4.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDoorIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDoorIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.4.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduDoorName.1.1 = STRING: "                 " */
	snmp_info_default("unmapped.pduDoorName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.4.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDoorName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pduDoorName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.4.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduDoorProbeStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduDoorProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.4.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDoorProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pduDoorProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.4.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduDoorState.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDoorState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.4.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDoorState.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDoorState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.4.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduDryIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDryIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.5.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDryIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDryIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.5.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduDryName.1.1 = STRING: "                 " */
	snmp_info_default("unmapped.pduDryName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.5.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDryName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pduDryName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.5.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduDryProbeStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduDryProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.5.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDryProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pduDryProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.5.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduDryState.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDryState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.5.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduDryState.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduDryState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.5.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduSpotIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduSpotIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.6.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduSpotIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduSpotIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.6.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduSpotName.1.1 = STRING: "                 " */
	snmp_info_default("unmapped.pduSpotName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.6.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduSpotName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pduSpotName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.6.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduSpotProbeStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduSpotProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.6.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduSpotProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pduSpotProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.6.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduSpotState.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduSpotState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.6.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduSpotState.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduSpotState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.6.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduRopeIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduRopeIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.7.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduRopeIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduRopeIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.7.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduRopeName.1.1 = STRING: "                 " */
	snmp_info_default("unmapped.pduRopeName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.7.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduRopeName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pduRopeName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.7.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduRopeProbeStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduRopeProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.7.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduRopeProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pduRopeProbeStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.4.7.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduRopeState.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduRopeState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.7.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduRopeState.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduRopeState", 0, 1, ".1.3.6.1.4.1.534.7.1.4.7.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHIDIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHIDIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHIDIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHIDIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidAisle.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidAisle.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidHandleOperation.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pduHidHandleOperation", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidHandleOperation.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pduHidHandleOperation", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHIDVer.1.1 = Hex-STRING: 02 00 00 00 00 */
	snmp_info_default("unmapped.pduHIDVer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.1.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHIDVer.1.2 = Hex-STRING: 02 00 00 00 00 */
	snmp_info_default("unmapped.pduHIDVer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.1.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduMechanicalLock.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pduMechanicalLock", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduMechanicalLock.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pduMechanicalLock", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.1.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlIndex.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.1.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.1 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.2 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.3 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.4 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.5 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.6 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.7 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.8 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.9 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.10 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.11 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.12 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.13 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.14 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.15 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlUserName.1.16 = "" */
	snmp_info_default("unmapped.pduHidControlUserName", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.2.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardID.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardID", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.3.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.1 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.2 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.3 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.4 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.5 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.6 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.7 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.8 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.9 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.10 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.11 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.12 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.13 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.14 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.15 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlTimestamp.1.16 = STRING: "2000/00/00 00:00:00" */
	snmp_info_default("unmapped.pduHidControlTimestamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.4.10.2.1.4.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduHidControlCardAisle.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduHidControlCardAisle", 0, 1, ".1.3.6.1.4.1.534.7.1.4.10.2.1.5.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.3 = INTEGER: 3 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.4 = INTEGER: 4 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.5 = INTEGER: 5 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.6 = INTEGER: 6 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletIndex.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletIndex", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.1.1.48", NULL, SU_FLAG_OK, NULL),

	/* pduOutletActivePowerThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.3 = INTEGER: 1 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.4 = INTEGER: 1 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.5 = INTEGER: 1 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.6 = INTEGER: 1 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThStatus.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.6.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerWarning.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.7.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThLowerCritical.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.8.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperWarning.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.9.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThUpperCritical.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.10.1.48", NULL, SU_FLAG_OK, NULL),

	/* pduOutletWh.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWh.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletWh", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.14.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.1 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.2 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.3 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.4 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.5 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.6 = Hex-STRING: 32 30 32 31 2F 30 35 2F 32 39 20 30 39 3A 30 36 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.7 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.8 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.9 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.10 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.11 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.12 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.13 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.14 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.15 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.16 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.17 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.18 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.19 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.20 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.21 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.22 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.23 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.24 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.25 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.26 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.27 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.28 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.29 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.30 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.31 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.32 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.33 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.34 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.35 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.36 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.37 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.38 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.39 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.40 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.41 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.42 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.43 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.44 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.45 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.46 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.47 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletWhTimer.1.48 = Hex-STRING: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */
	snmp_info_default("unmapped.pduOutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.7.1.5.1.1.15.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.1 = INTEGER: 100 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.2 = INTEGER: 100 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.3 = INTEGER: 100 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.4 = INTEGER: 100 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.5 = INTEGER: 100 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.6 = INTEGER: 100 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletPowerFactor.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletPowerFactor", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.16.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletVAR.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletVAR", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.17.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletBranch.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletBranch", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.18.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThResetThld.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThResetThld", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.19.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThChangeDelay.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletActivePowerThChangeDelay", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.20.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.1 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.2 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.3 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.4 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.5 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.6 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.7 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.8 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.9 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.10 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.11 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.12 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.13 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.14 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.15 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.16 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.17 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.18 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.19 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.20 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.21 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.22 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.23 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.24 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.25 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.26 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.27 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.28 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.29 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.30 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.31 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.32 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.33 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.34 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.35 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.36 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.37 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.38 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.39 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.40 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.41 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.42 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.43 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.44 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.45 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.46 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.47 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletActivePowerThCtrl.1.48 = INTEGER: 15 */
	snmp_info_default("unmapped.pduOutletActivePowerThCtrl", 0, 1, ".1.3.6.1.4.1.534.7.1.5.1.1.21.1.48", NULL, SU_FLAG_OK, NULL),


	/* pduOutletControlOffCmd.1.1 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.2 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.3 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.4 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.5 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.6 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOffCmd.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOffCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.2.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.1 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.2 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.3 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.4 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.5 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.6 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlOnCmd.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlOnCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.3.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.1 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.2 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.3 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.4 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.5 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.6 = INTEGER: -1 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlRebootCmd.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlRebootCmd", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.4.1.48", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.3 = INTEGER: 2 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.4 = INTEGER: 2 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.4", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.5 = INTEGER: 2 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.5", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.6 = INTEGER: 2 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.6", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.7", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.8", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.9", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.10", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.11", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.12", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.13", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.14", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.15", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.16", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.17", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.18", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.19", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.20", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.21", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.22", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.23", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.24", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.25", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.26", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.27", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.28", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.29", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.30", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.31", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.32", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.33", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.34", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.35", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.36", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.37", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.38", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.39", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.40", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.41", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.42", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.43", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.44", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.45", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.46", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.47", NULL, SU_FLAG_OK, NULL),
	/* pduOutletControlPowerOnState.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pduOutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.534.7.1.5.2.1.5.1.48", NULL, SU_FLAG_OK, NULL),
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	eaton_pdu_nlogic = { "eaton_pdu_nlogic", EATON_PDU_NLOGIC_MIB_VERSION, NULL, NULL, eaton_pdu_nlogic_mib, EATON_PDU_NLOGIC_SYSOID, NULL };
