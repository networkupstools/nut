/* hpe-pdu3-cis-mib.c - subdriver to monitor HPE_PDU_CIS SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2016	Arnaud Quette <arnaud.quette@free.fr>
 *  2022		Eaton (author: Arnaud Quette <arnaudquette@eaton.com>)
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

#include "hpe-pdu3-cis-mib.h"

#define HPE_PDU3_CIS_MIB_VERSION  "0.10"

#define HPE_PDU3_CIS_SYSOID       ".1.3.6.1.4.1.232.165.11"
#define HPE_PDU3_OID_MODEL_NAME	".1.3.6.1.4.1.232.165.11.1.2.1.3.1"

static info_lkp_t hpe_cis_unit_switchability_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "no"),
	info_lkp_sentinel
};

static info_lkp_t hpe_cis_outlet_group_type_info[] = {
	info_lkp_default(2, "breaker1pole"),
	info_lkp_default(3, "breaker2pole"),
	info_lkp_default(4, "breaker3pole"),
	info_lkp_default(5, "outlet-section"),
	info_lkp_sentinel
};

/* Note: same as marlin_outlet_type_info + i5-20R */
/* and to eaton_nlogic_outlet_type_info - few entries */
static info_lkp_t hpe_cis_outlet_type_info[] = {
	info_lkp_default(1, "iecC13"),
	info_lkp_default(2, "iecC19"),
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

/* Same as eaton_nlogic_outlet_status_info */
static info_lkp_t hpe_cis_outlet_status_info[] = {
	info_lkp_default(1, "off"),
	info_lkp_default(2, "on"),
	info_lkp_default(3, "pendingOff"),	/* transitional status */
	info_lkp_default(4, "pendingOn"),	/* transitional status */
	info_lkp_sentinel
};

static info_lkp_t hpe_cis_outlet_switchability_info[] = {
	info_lkp_default(1, "yes"),	/* switchable */
	info_lkp_default(2, "no"),	/* notSwitchable */
	info_lkp_sentinel
};

/* HPE_PDU_CIS Snmp2NUT lookup table */
static snmp_info_t hpe_pdu3_cis_mib[] = {

/* standard MIB items; if the vendor MIB contains better OIDs for
 * this (e.g. with daisy-chain support), consider adding those here
 */
	/* Device collection */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),
	/* pdu3NumberPDU.0 = INTEGER: 1 (for daisychain support) */
/*
	snmp_info_default("device.count", 0, 1, ".1.3.6.1.4.1.232.165.11.1.1.0", NULL, SU_FLAG_OK, NULL),
*/
	/* pdu3Manufacturer.1 = STRING: "HPE" */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.1.2.1.4.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* pdu3Model.1 = STRING: "230V, 32A, 7.4kVA, 50/60Hz" */
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.1.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3FirmwareVersion.1 = STRING: "2.0.0.J" */
	snmp_info_default("device.firmware", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.1.2.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3PartNumber.1 = STRING: "P9S18A" */
	snmp_info_default("device.part", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.1.2.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3SerialNumber.1 = STRING: "CN09416708" */
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.1.2.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3MACAddress.1 = Hex-STRING: EC EB B8 3D 78 6D */
	snmp_info_default("device.macaddr", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.1.2.1.14.1", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),

	/* Input collection */
	/* pdu3InputPhaseCount.1 = INTEGER: 1 */
	snmp_info_default("input.phases", 0, 1,
		".1.3.6.1.4.1.232.165.11.1.2.1.11.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPowerVA.1 = INTEGER: 922 */
	snmp_info_default("input.power", 0, 0.001,
		".1.3.6.1.4.1.232.165.11.2.1.1.4.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL),
	/* pdu3InputPowerWatts.1 = INTEGER: 900 */
	snmp_info_default("input.realpower", 0, 0.001,
		".1.3.6.1.4.1.232.165.11.2.1.1.5.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentRating.1.%i = INTEGER: 3200 */
	/* (can be instanciated by phase) */
	snmp_info_default("input.current.nominal", 0, 0.01,
		".1.3.6.1.4.1.232.165.11.2.2.1.10.1.1",
		NULL, 0, NULL),
	/* pdu3InputPhaseCurrent.1.%i = INTEGER: 398 */
	/* (can be instanciated by phase) */
	snmp_info_default("input.current", 0, 0.01,
		".1.3.6.1.4.1.232.165.11.2.2.1.11.1.1",
		NULL, 0, NULL),
	/* pdu3InputPhaseVoltage.1.%i = INTEGER: 2286 */
	/* (can be instanciated by phase) */
	snmp_info_default("input.voltage", 0, 0.1,
		".1.3.6.1.4.1.232.165.11.2.2.1.3.1.1",
		NULL, 0, NULL),
	/* pdu3InputFrequency.%i = INTEGER: 500 */
	/* (can be instanciated by phase) */
	snmp_info_default("input.frequency", 0, 0.1,
		".1.3.6.1.4.1.232.165.11.2.1.1.2.1",
		NULL, 0, NULL),

	/* Outlet groups collection */
	/* pdu3GroupCount.1 = INTEGER: 2 */
	snmp_info_default("outlet.group.count", 0, 1,
		".1.3.6.1.4.1.232.165.11.1.2.1.12.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupName.1.%i = STRING: "B01" */
	snmp_info_default("outlet.group.%i.name", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.3.1.1.2.1.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP /*| SU_TYPE_DAISY_1*/,
		NULL),
	/* pdu3GroupType.1.%i = INTEGER: 2 */
	snmp_info_default("outlet.group.%i.type", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.3.1.1.3.1.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP /*| SU_TYPE_DAISY_1*/,
		&hpe_cis_outlet_group_type_info[0]),
	/* pdu3GroupCurrentPercentLoad.1.%i = INTEGER: 11 */
	snmp_info_default("outlet.group.%i.load", 0, 1.0,
		".1.3.6.1.4.1.232.165.11.3.1.1.18.1.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP /*| SU_TYPE_DAISY_1*/, NULL),
	/* pdu3GroupCurrent.1.1 = INTEGER: 187 */
	snmp_info_default("outlet.group.%i.current", 0, 0.01,
		".1.3.6.1.4.1.232.165.11.3.1.1.12.1.%i",
		NULL, SU_OUTLET_GROUP /*| SU_TYPE_DAISY_1*/, NULL),
	/* pdu3GroupVoltage.1.%i = INTEGER: 2286 */
	snmp_info_default("outlet.group.%i.voltage", 0, 0.1,
		".1.3.6.1.4.1.232.165.11.3.1.1.5.1.%i",
		NULL, SU_OUTLET_GROUP, NULL),
	/* pdu3GroupOutletCount.1.1 = INTEGER: 12 */
	snmp_info_default("outlet.group.%i.count", 0, 1,
		".1.3.6.1.4.1.232.165.11.3.1.1.25.1.1",
		NULL, SU_OUTLET_GROUP /* | SU_TYPE_DAISY_1 */, NULL),

	/* Outlet collection */
	/* pdu3OutletCount.1 = INTEGER: 24 */
	snmp_info_default("outlet.count", 0, 1,
		".1.3.6.1.4.1.232.165.11.1.2.1.13.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3Controllable.1 = INTEGER: 1 */
	snmp_info_default("outlet.switchable", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.1.2.1.10.1",
		"no", SU_FLAG_STATIC,
		&hpe_cis_unit_switchability_info[0]),

	/* pdu3OutletControlStatus.1.1 = INTEGER: 2 */
	snmp_info_default("outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.5.2.1.1.1.%i",
		NULL, SU_OUTLET,
		&hpe_cis_outlet_status_info[0]),
	/* pdu3OutletName.1.%i = STRING: "CABNIET A FAN DOOR" */
	snmp_info_default("outlet.%i.name", ST_FLAG_RW |ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.5.1.1.2.1.%i",
		NULL, SU_OUTLET, NULL),
	/* pdu3OutletType.1.%i = INTEGER: 2 */
	snmp_info_default("outlet.%i.type", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.5.1.1.3.1.%i",
		"unknown", SU_FLAG_STATIC | SU_OUTLET,
		&hpe_cis_outlet_type_info[0]),
	/* pdu3OutletCurrentRating.1.%i = INTEGER: 2000 */
	snmp_info_default("outlet.%i.current.nominal", 0, 0.01,
		".1.3.6.1.4.1.232.165.11.5.1.1.4.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pdu3OutletCurrent.1.%i = INTEGER: 36 */
	snmp_info_default("outlet.%i.current", 0, 0.01,
		".1.3.6.1.4.1.232.165.11.5.1.1.5.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pdu3OutletCurrentPercentLoad.1.%i = INTEGER: 18 */
	snmp_info_default("outlet.%i.load", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.1.1.11.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pdu3OutletVA.1.%i = INTEGER: 84 */
	snmp_info_default("outlet.%i.power", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.1.1.12.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pdu3OutletWatts.1.%i = INTEGER: 84 */
	snmp_info_default("outlet.%i.realpower", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.1.1.13.1.%i",
		NULL, SU_OUTLET | SU_FLAG_NEGINVALID, NULL),
	/* pdu3OutletControlSwitchable.1.%i = INTEGER: 1 */
	snmp_info_default("outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.11.5.2.1.8.1.%i",
		"no", SU_OUTLET | SU_FLAG_UNIQUE | SU_TYPE_DAISY_1,
		&hpe_cis_outlet_switchability_info[0]),

	/* instant commands. */
	/* Delays handling:
	 * 0-n :Time in seconds until the group command is issued
	 * -1:Cancel a pending group-level Off/On/Reboot command */
	/* pdu3OutletControlOffCmd.1.%i = INTEGER: -1 */
	snmp_info_default("outlet.%i.load.off", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.2.1.2.1.%i",
		"0", SU_TYPE_CMD | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
	/* pdu3OutletControlOnCmd.1.%i = INTEGER: -1 */
	snmp_info_default("outlet.%i.load.on", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.2.1.3.1.%i",
		"0", SU_TYPE_CMD | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
	/* pdu3OutletControlRebootCmd.1.%i = INTEGER: -1 */
	snmp_info_default("outlet.%i.load.cycle", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.2.1.4.1.%i",
		"0", SU_TYPE_CMD | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),


#if 0
	/* Per-outlet shutdown / startup delay (configuration point, not the timers)
	 * outletControlShutoffDelay.0.3 = INTEGER: 120
	 * outletControlSequenceDelay.0.8 = INTEGER: 8
	 * (by default each output socket startup is delayed by its number in seconds)
	 */
	snmp_info_default("outlet.%i.delay.shutdown", ST_FLAG_RW, 1,
		".1.3.6.1.4.1.534.6.6.7.6.6.1.10.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
	snmp_info_default("outlet.%i.delay.start", ST_FLAG_RW, 1,
		".1.3.6.1.4.1.534.6.6.7.6.6.1.7.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
#endif
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	snmp_info_default("outlet.%i.load.off.delay", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.2.1.2.1.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
	snmp_info_default("outlet.%i.load.on.delay", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.2.1.3.1.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
	snmp_info_default("outlet.%i.load.cycle.delay", 0, 1,
		".1.3.6.1.4.1.232.165.11.5.2.1.4.1.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
#if 0
	/* Per-outlet shutdown / startup timers
	 * outletControlOffCmd.0.1 = INTEGER: -1
	 * outletControlOnCmd.0.1 = INTEGER: -1
	 */
	snmp_info_default("outlet.%i.timer.shutdown", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.6.6.1.3.%i.%i",
		NULL, SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
	snmp_info_default("outlet.%i.timer.start", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.6.6.1.4.%i.%i",
		NULL, SU_OUTLET /*| SU_TYPE_DAISY_1*/, NULL),
#endif

#if WITH_UNMAPPED_DATA_POINTS

	/* pdu3IdentIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3IdentIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.1.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3Name.1 = "" */
	snmp_info_default("unmapped.pdu3Name", 0, 1, ".1.3.6.1.4.1.232.165.11.1.2.1.2.1", NULL, SU_FLAG_OK, NULL),

	/* pdu3FirmwareVersionTimeStamp.1 = Hex-STRING: 32 30 31 36 2F 31 31 2F 30 31 20 32 30 3A 30 38 3A 33 39 00 */
	snmp_info_default("unmapped.pdu3FirmwareVersionTimeStamp", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.1.2.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3Status.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3Status", 0, 1, ".1.3.6.1.4.1.232.165.11.1.2.1.9.1", NULL, SU_FLAG_OK, NULL),

	/* pdu3IPv4Address.1 = IpAddress: [PDU_IP] */
	snmp_info_default("unmapped.pdu3IPv4Address", 0, 1, ".1.3.6.1.4.1.232.165.11.1.2.1.15.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3IPv6Address.1 = STRING: "FE80::EEEB:B8FF:FE3D:786D" */
	snmp_info_default("unmapped.pdu3IPv6Address", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.1.2.1.16.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigSsh.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ConfigSsh", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigFtps.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ConfigFtps", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigHttp.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ConfigHttp", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigHttps.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ConfigHttps", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigIPv4IPv6Switch.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ConfigIPv4IPv6Switch", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigRedfishAPI.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ConfigRedfishAPI", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigOledDispalyOrientation.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ConfigOledDispalyOrientation", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigEnergyReset.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ConfigEnergyReset", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigNetworkManagementCardReset.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ConfigNetworkManagementCardReset", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ConfigDaisyChainStatus.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ConfigDaisyChainStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.1.3.1.11.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputType.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputFrequencyStatus.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputFrequencyStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.2.1.1.3.1", NULL, SU_FLAG_OK, NULL),

	/* pdu3InputTotalEnergy.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputTotalEnergy", 0, 1, ".1.3.6.1.4.1.232.165.11.2.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPowerWattHourTimer.1 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3InputPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.2.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputResettableEnergy.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputResettableEnergy", 0, 1, ".1.3.6.1.4.1.232.165.11.2.1.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPowerFactor.1 = INTEGER: 97 */
	snmp_info_default("unmapped.pdu3InputPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.2.1.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPowerVAR.1 = INTEGER: 197 */
	snmp_info_default("unmapped.pdu3InputPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.2.1.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseIndex.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputPhaseIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseIndex.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageMeasType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.2.1.3", NULL, SU_FLAG_OK, NULL),

	/* pdu3InputPhaseVoltageThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThLowerWarning.1.1 = INTEGER: 1900 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThLowerCritical.1.1 = INTEGER: 1800 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThUpperWarning.1.1 = INTEGER: 2500 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThUpperCritical.1.1 = INTEGER: 2600 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseVoltageThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentMeasType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.9.1.3", NULL, SU_FLAG_OK, NULL),

	/* pdu3InputPhaseCurrentThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.12.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.12.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.12.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.13.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.13.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.13.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.14.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.14.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.14.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThUpperWarning.1.1 = INTEGER: 2200 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.15.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.15.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.15.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThUpperCritical.1.1 = INTEGER: 2800 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.16.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.16.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.16.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentPercentLoad.1.1 = INTEGER: 124 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.17.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentPercentLoad.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.17.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhaseCurrentPercentLoad.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhaseCurrentPercentLoad", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.17.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3InputPhasePowerMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.18.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerMeasType.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.18.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.18.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerVA.1.1 = INTEGER: 921 */
	snmp_info_default("unmapped.pdu3InputPhasePowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.19.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerVA.1.2 = INTEGER: 921 */
	snmp_info_default("unmapped.pdu3InputPhasePowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.19.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerVA.1.3 = INTEGER: 921 */
	snmp_info_default("unmapped.pdu3InputPhasePowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.19.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWatts.1.1 = INTEGER: 899 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.20.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWatts.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.20.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWatts.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.20.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWattHour.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.21.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWattHour.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.21.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWattHour.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.21.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWattHourTimer.1.1 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.2.2.1.22.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWattHourTimer.1.2 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.2.2.1.22.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerWattHourTimer.1.3 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3InputPhasePowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.2.2.1.22.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerFactor.1.1 = INTEGER: 97 */
	snmp_info_default("unmapped.pdu3InputPhasePowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.23.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerFactor.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.23.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerFactor.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.23.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerVAR.1.1 = INTEGER: 195 */
	snmp_info_default("unmapped.pdu3InputPhasePowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.24.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerVAR.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.24.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3InputPhasePowerVAR.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3InputPhasePowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.2.2.1.24.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.3 = INTEGER: 3 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.4 = INTEGER: 4 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.5 = INTEGER: 5 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.6 = INTEGER: 6 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.7 = INTEGER: 7 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.8 = INTEGER: 8 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.9 = INTEGER: 9 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.10 = INTEGER: 10 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.11 = INTEGER: 11 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupIndex.1.12 = INTEGER: 12 */
	snmp_info_default("unmapped.pdu3GroupIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.1.1.12", NULL, SU_FLAG_OK, NULL),




	/* pdu3GroupVoltageMeasType.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageMeasType.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageMeasType", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.4.1.12", NULL, SU_FLAG_OK, NULL),

	/* pdu3GroupVoltageThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThStatus.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.6.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.7.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThLowerCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.8.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperWarning.1.%i = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.9.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3GroupVoltageThUpperCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupVoltageThUpperCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupVoltageThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.10.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.1 = INTEGER: 1600 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.2 = INTEGER: 1600 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3groupCurrentRating.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3groupCurrentRating", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.11.1.12", NULL, SU_FLAG_OK, NULL),

	/* pdu3GroupCurrentThStatus.1.%i = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3GroupCurrentThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.13.1.%i", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerWarning.1.%i = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.14.1.%i", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThLowerCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.15.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.1 = INTEGER: 1100 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.2 = INTEGER: 1100 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.16.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.1 = INTEGER: 1400 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.2 = INTEGER: 1400 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupCurrentThUpperCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupCurrentThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.17.1.12", NULL, SU_FLAG_OK, NULL),

	/* pdu3GroupPowerVA.1.1 = INTEGER: 430 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.2 = INTEGER: 530 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVA.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVA", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.19.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.1 = INTEGER: 422 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.2 = INTEGER: 512 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWatts.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWatts", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.20.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHour.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.21.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.1 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.2 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.3 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.4 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.5 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.6 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.7 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.8 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.9 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.10 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.11 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerWattHourTimer.1.12 = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.3.1.1.22.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.1 = INTEGER: 98 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.2 = INTEGER: 97 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerFactor.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.23.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.1 = INTEGER: 43 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.2 = INTEGER: 36 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupPowerVAR.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupPowerVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.24.1.12", NULL, SU_FLAG_OK, NULL),

	/* pdu3GroupBreakerStatus.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3GroupBreakerStatus.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3GroupBreakerStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.3.1.1.26.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureScale.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3TemperatureScale", 0, 1, ".1.3.6.1.4.1.232.165.11.4.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureCount.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3TemperatureCount", 0, 1, ".1.3.6.1.4.1.232.165.11.4.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityCount.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3HumidityCount", 0, 1, ".1.3.6.1.4.1.232.165.11.4.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ContactCount", 0, 1, ".1.3.6.1.4.1.232.165.11.4.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureIndex.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3TemperatureIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3TemperatureIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureName.1.1 = STRING: "T" */
	snmp_info_default("unmapped.pdu3TemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.2.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pdu3TemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.2.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.2.1.2.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.2.1.2.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.2.1.2.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.2.1.2.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureProbeStatus.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3TemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3TemperatureProbeStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.3.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.3.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.3.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureValue.1.1 = INTEGER: 27 */
	snmp_info_default("unmapped.pdu3TemperatureValue", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureValue.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3TemperatureValue", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3TemperatureThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3TemperatureThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.5.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.5.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.5.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThLowerWarning.1.1 = INTEGER: 15 */
	snmp_info_default("unmapped.pdu3TemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3TemperatureThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThLowerCritical.1.1 = INTEGER: 10 */
	snmp_info_default("unmapped.pdu3TemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3TemperatureThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThUpperWarning.1.1 = INTEGER: 30 */
	snmp_info_default("unmapped.pdu3TemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3TemperatureThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThUpperCritical.1.1 = INTEGER: 35 */
	snmp_info_default("unmapped.pdu3TemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3TemperatureThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3TemperatureThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.9.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.9.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.2.1.9.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityIndex.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3HumidityIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3HumidityIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityName.1.1 = STRING: "RH" */
	snmp_info_default("unmapped.pdu3HumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.3.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pdu3HumidityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.3.1.2.1.2", NULL, SU_FLAG_OK, NULL),
/* FIXME: missing sub MIB for ambient sensor? */
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.3.1.2.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.3.1.2.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.3.1.2.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = STRING: "                 " */
	snmp_info_default("unmapped. = STRING: "                 "", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.3.1.2.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityProbeStatus.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3HumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3HumidityProbeStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.3.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.3.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.3.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 1 */
	snmp_info_default("unmapped. = INTEGER: 1", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.3.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityValue.1.1 = INTEGER: 27 */
	snmp_info_default("unmapped.pdu3HumidityValue", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityValue.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3HumidityValue", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.4.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.4.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.4.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.4.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThStatus.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3HumidityThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThStatus.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3HumidityThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.5.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.5.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.5.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.5.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.5.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThLowerWarning.1.1 = INTEGER: 30 */
	snmp_info_default("unmapped.pdu3HumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3HumidityThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThLowerCritical.1.1 = INTEGER: 10 */
	snmp_info_default("unmapped.pdu3HumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3HumidityThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThUpperWarning.1.1 = INTEGER: 60 */
	snmp_info_default("unmapped.pdu3HumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3HumidityThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThUpperCritical.1.1 = INTEGER: 80 */
	snmp_info_default("unmapped.pdu3HumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3HumidityThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3HumidityThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.9.1.4", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.9.1.5", NULL, SU_FLAG_OK, NULL),
	/*  = INTEGER: 0 */
	snmp_info_default("unmapped. = INTEGER: 0", 0, 1, ".1.3.6.1.4.1.232.165.11.4.3.1.9.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactIndex.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ContactIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.4.4.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactIndex.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ContactIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.4.4.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactName.1.1 = STRING: "                 " */
	snmp_info_default("unmapped.pdu3ContactName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.4.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactName.1.2 = STRING: "                 " */
	snmp_info_default("unmapped.pdu3ContactName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.4.4.1.2.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactProbeStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ContactProbeStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.4.1.3.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactProbeStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3ContactProbeStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.4.4.1.3.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactState.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ContactState", 0, 1, ".1.3.6.1.4.1.232.165.11.4.4.1.4.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3ContactState.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3ContactState", 0, 1, ".1.3.6.1.4.1.232.165.11.4.4.1.4.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.3 = INTEGER: 3 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.4 = INTEGER: 4 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.5 = INTEGER: 5 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.6 = INTEGER: 6 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.7 = INTEGER: 7 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.8 = INTEGER: 8 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.9 = INTEGER: 9 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.10 = INTEGER: 10 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.11 = INTEGER: 11 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.12 = INTEGER: 12 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.13 = INTEGER: 13 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.13", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.14 = INTEGER: 14 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.14", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.15 = INTEGER: 15 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.15", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.16 = INTEGER: 16 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.16", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.17 = INTEGER: 17 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.17", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.18 = INTEGER: 18 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.18", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.19 = INTEGER: 19 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.19", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.20 = INTEGER: 20 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.20", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.21 = INTEGER: 21 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.21", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.22 = INTEGER: 22 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.22", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.23 = INTEGER: 23 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.23", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.24 = INTEGER: 24 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.24", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.25", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.26", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.27", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.28", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.29", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.30", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.31", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.32", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.33", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.34", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.35", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.36", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.37", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.38", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.39", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.40", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.41", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.42", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.43", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.44", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.45", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.46", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.47", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletIndex.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletIndex", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.1.1.48", NULL, SU_FLAG_OK, NULL),


	/* pdu3OutletActivePowerThStatus.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.2 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.3 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.4 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.5 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.6 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.7 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.8 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.9 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.10 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.11 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.12 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.13 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.13", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.14 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.14", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.15 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.15", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.16 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.16", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.17 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.17", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.18 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.18", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.19 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.19", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.20 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.20", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.21 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.21", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.22 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.22", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.23 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.23", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.24 = INTEGER: 1 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.24", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.25", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.26", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.27", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.28", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.29", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.30", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.31", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.32", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.33", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.34", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.35", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.36", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.37", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.38", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.39", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.40", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.41", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.42", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.43", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.44", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.45", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.46", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.47", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThStatus.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThStatus", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.6.1.48", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.13", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.14", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.15", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.16", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.17", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.18", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.19", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.20", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.21", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.22", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.23", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.24", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.25", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.26", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.27", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.28", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.29", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.30", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.31", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.32", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.33", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.34", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.35", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.36", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.37", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.38", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.39", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.40", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.41", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.42", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.43", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.44", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.45", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.46", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.47", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerWarning.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.7.1.48", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.13", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.14", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.15", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.16", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.17", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.18", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.19", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.20", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.21", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.22", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.23", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.24", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.25", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.26", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.27", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.28", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.29", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.30", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.31", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.32", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.33", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.34", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.35", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.36", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.37", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.38", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.39", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.40", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.41", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.42", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.43", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.44", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.45", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.46", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.47", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThLowerCritical.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThLowerCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.8.1.48", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.13", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.14", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.15", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.16", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.17", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.18", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.19", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.20", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.21", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.22", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.23", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.24", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.25", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.26", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.27", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.28", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.29", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.30", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.31", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.32", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.33", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.34", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.35", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.36", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.37", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.38", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.39", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.40", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.41", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.42", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.43", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.44", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.45", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.46", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.47", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperWarning.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperWarning", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.9.1.48", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.1 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.2 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.2", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.3 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.3", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.4 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.4", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.5 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.5", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.6 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.6", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.7 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.7", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.8 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.8", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.9 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.9", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.10 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.10", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.11 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.11", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.12 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.12", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.13 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.13", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.14 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.14", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.15 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.15", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.16 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.16", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.17 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.17", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.18 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.18", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.19 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.19", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.20 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.20", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.21 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.21", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.22 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.22", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.23 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.23", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.24 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.24", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.25 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.25", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.26 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.26", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.27 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.27", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.28 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.28", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.29 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.29", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.30 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.30", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.31 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.31", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.32 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.32", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.33 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.33", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.34 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.34", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.35 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.35", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.36 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.36", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.37 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.37", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.38 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.38", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.39 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.39", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.40 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.40", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.41 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.41", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.42 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.42", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.43 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.43", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.44 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.44", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.45 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.45", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.46 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.46", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.47 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.47", NULL, SU_FLAG_OK, NULL),
	/* pdu3OutletActivePowerThUpperCritical.1.48 = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletActivePowerThUpperCritical", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.10.1.48", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletWh.1.%i = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletWh", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.14.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletWhTimer.1.%i = Hex-STRING: 32 30 31 36 2F 31 30 2F 31 31 20 30 32 3A 34 36 3A 35 30 00 */
	snmp_info_default("unmapped.pdu3OutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.11.5.1.1.15.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletPowerFactor.1.%i = INTEGER: 88 */
	snmp_info_default("unmapped.pdu3OutletPowerFactor", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.16.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletVAR.1.%i = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3OutletVAR", 0, 1, ".1.3.6.1.4.1.232.165.11.5.1.1.17.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletControlPowerOnState.1.%i = INTEGER: 2 */
	snmp_info_default("unmapped.pdu3OutletControlPowerOnState", 0, 1, ".1.3.6.1.4.1.232.165.11.5.2.1.5.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletControlSequenceDelay.1.%i = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletControlSequenceDelay", 0, 1, ".1.3.6.1.4.1.232.165.11.5.2.1.6.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletControlRebootOffTime.1.%i = INTEGER: 5 */
	snmp_info_default("unmapped.pdu3OutletControlRebootOffTime", 0, 1, ".1.3.6.1.4.1.232.165.11.5.2.1.7.1.%i", NULL, SU_FLAG_OK, NULL),

	/* pdu3OutletControlShutoffDelay.1.%i = INTEGER: 0 */
	snmp_info_default("unmapped.pdu3OutletControlShutoffDelay", 0, 1, ".1.3.6.1.4.1.232.165.11.5.2.1.9.1.%i", NULL, SU_FLAG_OK, NULL),
#endif	/* WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t  hpe_pdu3_cis = { "hpe_pdu3_cis", HPE_PDU3_CIS_MIB_VERSION, NULL, HPE_PDU3_OID_MODEL_NAME, hpe_pdu3_cis_mib, HPE_PDU3_CIS_SYSOID, NULL };
