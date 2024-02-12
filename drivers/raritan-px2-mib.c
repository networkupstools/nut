/* raritan-px2-mib.c - subdriver to monitor RARITAN PX2 SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *  2016 Arnaud Quette <ArnaudQuette@Eaton.com>
 *
 *  Based on initial work and data from Opengear <support@opengear.com>
 *
 *  NOTE: Many readings allow for PDU ID which is hard-coded to ".1" in
 *  the mapping tables below at this time. This should be extended to NUT
 *  support for "daisy-chain" concept which appeared later than this driver.
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

#include "raritan-px2-mib.h"

#define RARITAN_PX2_MIB_VERSION  "0.41"

#define RARITAN_PX2_MIB_SYSOID     ".1.3.6.1.4.1.13742.6"
#define RARITAN_PX2_OID_MODEL_NAME ".1.3.6.1.4.1.13742.6.3.2.1.1.3.1"

/* info elements */
/* FIXME: triage between status and alarms, and make it compliant! */
static info_lkp_t raritanpx2_outlet_status_info[] = {
	info_lkp_default(-1, "unavailable"),
	info_lkp_default(0, "open"),
	info_lkp_default(1, "closed"),
	info_lkp_default(2, "belowLowerCritical"),
	info_lkp_default(3, "belowLowerWarning"),
	info_lkp_default(4, "normal"),
	info_lkp_default(5, "aboveUpperWarning"),
	info_lkp_default(6, "aboveUpperCritical"),
	info_lkp_default(7, "on"),
	info_lkp_default(8, "off"),
	info_lkp_default(9, "detected"),
	info_lkp_default(10, "notDetected"),
	info_lkp_default(11, "alarmed"),
	info_lkp_default(12, "ok"),
	info_lkp_default(13, "marginal"),
	info_lkp_default(14, "fail"),
	info_lkp_default(15, "yes"),
	info_lkp_default(16, "no"),
	info_lkp_default(17, "standby"),
	info_lkp_default(18, "one"),
	info_lkp_default(19, "two"),
	info_lkp_default(20, "inSync"),
	info_lkp_default(21, "outOfSync"),
	info_lkp_default(22, "i1OpenFault"),
	info_lkp_default(23, "i1ShortFault"),
	info_lkp_default(24, "i2OpenFault"),
	info_lkp_default(25, "i2ShortFault"),
	info_lkp_default(26, "fault"),
	info_lkp_default(27, "warning"),
	info_lkp_default(28, "critical"),
	info_lkp_default(29, "selfTest"),
	info_lkp_default(30, "nonRedundant"),
	info_lkp_sentinel
};

static info_lkp_t raritanpx2_outlet_switchability_info[] = {
	info_lkp_default(-1, "yes"),
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "no"),
	info_lkp_sentinel
};

/* PDU2-MIB Snmp2NUT lookup table */
static snmp_info_t raritan_px2_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* pduManufacturer.1 = STRING: Raritan */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.2.1",
		"Raritan", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* pduModel.1 = STRING: PX2-5475 */
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.3.1",
		"Raritan PX2 SNMP PDU device", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* pduSerialNumber.1 = STRING: QFC3950619 */
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.4.1",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	/* pxMACAddress.1 = STRING: 0:d:5d:b:49:0 */
	snmp_info_default("device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.11.1",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	/* boardVersion.1.mainController.1 = STRING: 0x01 */
	/* FIXME: not compliant! to be RFC'ed */
	snmp_info_default("device.revision", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.4.1.1.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* FIXME: move to device collection! */
	/* Wrong OID!
	 * snmp_info_default("ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.6.1.1.1", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL),*/
	/* boardFirmwareVersion.1.mainController.1 = STRING: 2.4.3.5-40298 */
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.6.1.1.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* pduName.1 = STRING: my PX */
	snmp_info_default("ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.13.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.3.1",
		"Raritan PX2 SNMP PDU device", SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	/* Input data:
	 * Units are given in inletSensorUnits.1.1.%i
	 * Value should be scaled by inletSensorDecimalDigits.1.1.%i
	 * For example, if the value is 1 and inletSensorDecimalDigits is 2, then actual value is 0.01. */
	/* measurementsInletSensorValue.1.1.rmsCurrent = Gauge32: 10 (A) */
	snmp_info_default("input.load", 0, 0.1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* measurementsInletSensorValue.1.1.rmsVoltage = Gauge32: 119 (V) */
	snmp_info_default("input.voltage", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* measurementsInletSensorValue.1.1.activePower = Gauge32: 10 (W) */
	snmp_info_default("input.realpower", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* measurementsInletSensorValue.1.1.apparentPower = Gauge32: 122 (VA) */
	snmp_info_default("input.power", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* measurementsInletSensorValue.1.1.powerFactor = Gauge32: 8 (none) */
	/* FIXME: need RFC! */
	snmp_info_default("input.powerfactor", 0, 0.01, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* measurementsInletSensorValue.1.1.activeEnergy = Gauge32: 193359 (wattHour) */
	/* snmp_info_default("unmapped.measurementsInletSensorValue", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.8", NULL, SU_FLAG_OK, NULL), */

	/* inletPlug.1.1 = INTEGER: plugIEC320C20(6) */
	/* FIXME: need RFC (input.type | [input.]inlet.type...) and standardization
	 * snmp_info_default("unmapped.inletPlug", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.4.1.1", NULL, SU_FLAG_OK, NULL),*/

	/* outletCount.1 = INTEGER: 24 */
	snmp_info_default("outlet.count", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.4.1", "0", SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	/* outlet template definition
	 * Indexes start from 1, ie outlet.1 => <OID>.1 */
	/* Note: the first definition is used to determine the base index (ie 0 or 1) */
	/* outletName.1.%i = STRING:  */
	snmp_info_default("outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.3.1.%i", NULL, SU_OUTLET, NULL),
	/* outletSwitchingState.1.%i = INTEGER: on(7) */
	snmp_info_default("outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.4.1.2.1.3.1.%i", NULL, SU_OUTLET, &raritanpx2_outlet_status_info[0]),
	/* outletLabel.1.%i = STRING: 1 */
	snmp_info_default("outlet.%i.id", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.2.1.%i", "%i", SU_FLAG_STATIC | SU_OUTLET | SU_FLAG_OK, NULL),

	/* outletReceptacle.1.1 = INTEGER: receptacleNEMA520R(37) */
	/* FIXME: need RFC and standardization
	 * snmp_info_default("outlet.%i.type", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.4.1.%i", NULL, SU_OUTLET | SU_FLAG_OK, NULL), */

	/* RMS Current (divide by 10). e.g. 5 == 0.5A */
	/* measurementsOutletSensorValue.1.%i.rmsCurrent = Gauge32: 10 */
	snmp_info_default("outlet.%i.current", 0, 0.1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.1", NULL, SU_OUTLET | SU_FLAG_OK, NULL),
	/* measurementsOutletSensorValue.1.%i.rmsVoltage = Gauge32: 119 */
	snmp_info_default("outlet.%i.voltage", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.4", "%i", SU_OUTLET | SU_FLAG_OK, NULL),
	/* measurementsOutletSensorValue.1.%i.activePower = Gauge32: 10 */
	snmp_info_default("outlet.%i.power", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.5", "%i", SU_OUTLET | SU_FLAG_OK, NULL),
	/* measurementsOutletSensorValue.1.%i.apparentPower = Gauge32: 122 */
	snmp_info_default("outlet.%i.realpower", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.6", "%i", SU_OUTLET | SU_FLAG_OK, NULL),
	/* measurementsOutletSensorValue.1.%i.powerFactor = Gauge32: 8 */
	snmp_info_default("outlet.%i.powerfactor", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.7", "%i", SU_OUTLET | SU_FLAG_OK, NULL),
	/* measurementsOutletSensorValue.1.1.activeEnergy = Gauge32: 89890 */
	/* FIXME:
	 * snmp_info_default("unmapped.measurementsOutletSensorValue", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.1.8", NULL, SU_FLAG_OK, NULL), */
	/* measurementsOutletSensorValue.1.1.onOff = Gauge32: 0 */
	/* FIXME:
	 * snmp_info_default("unmapped.measurementsOutletSensorValue", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.1.14", NULL, SU_FLAG_OK, NULL), */

	/* outletSwitchable.1.%i = INTEGER: true(1) */
	snmp_info_default("outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.28.1.%i", "no", SU_FLAG_STATIC | SU_OUTLET | SU_FLAG_OK, &raritanpx2_outlet_switchability_info[0]),

	/* instant commands. */
	/* switchingOperation.1.1 = INTEGER: on(1) */
	snmp_info_default("outlet.%i.load.off",   0, 1, ".1.3.6.1.4.1.13742.6.4.1.2.1.2.1.%i", "0", SU_TYPE_CMD | SU_OUTLET, NULL),
	snmp_info_default("outlet.%i.load.on",    0, 1, ".1.3.6.1.4.1.13742.6.4.1.2.1.2.1.%i", "1", SU_TYPE_CMD | SU_OUTLET, NULL),
	snmp_info_default("outlet.%i.load.cycle", 0, 1, ".1.3.6.1.4.1.13742.6.4.1.2.1.2.1.%i", "2", SU_TYPE_CMD | SU_OUTLET, NULL),

#if WITH_UNMAPPED_DATA_POINTS || (defined DEBUG)
	/* pduCount.0 = INTEGER: 1 */
	/* FIXME: part of daisychain support, RFC device.count */
	snmp_info_default("device.count", 0, 1, ".1.3.6.1.4.1.13742.6.3.1.0", NULL, SU_FLAG_OK, NULL),

#if WITH_UNMAPPED_DATA_POINTS
	/* pduRatedVoltage.1 = STRING: 100-120V */
	snmp_info_default("unmapped.pduRatedVoltage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* pduRatedCurrent.1 = STRING: 16A */
	snmp_info_default("unmapped.pduRatedCurrent", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* pduRatedFrequency.1 = STRING: 50/60Hz */
	snmp_info_default("unmapped.pduRatedFrequency", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pduRatedVA.1 = STRING: 1.6-1.9kVA */
	snmp_info_default("unmapped.pduRatedVA", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* pduImage.1 = STRING:  */
	snmp_info_default("unmapped.pduImage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* inletCount.1 = INTEGER: 1 */
	snmp_info_default("unmapped.inletCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* overCurrentProtectorCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.overCurrentProtectorCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.3.1", NULL, SU_FLAG_OK, NULL),

	/* inletControllerCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.inletControllerCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* outletControllerCount.1 = INTEGER: 6 */
	snmp_info_default("unmapped.outletControllerCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* externalSensorCount.1 = INTEGER: 16 */
	snmp_info_default("unmapped.externalSensorCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* pxIPAddress.1 = IpAddress: 192.168.20.188 */
	snmp_info_default("unmapped.pxIPAddress", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* netmask.1 = IpAddress: 255.255.255.0 */
	snmp_info_default("unmapped.netmask", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* gateway.1 = IpAddress: 192.168.20.254 */
	snmp_info_default("unmapped.gateway", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* utcOffset.1 = STRING: -5:00 */
	snmp_info_default("unmapped.utcOffset", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.12.1", NULL, SU_FLAG_OK, NULL),
	/* externalSensorsZCoordinateUnits.1 = INTEGER: rackUnits(0) */
	snmp_info_default("unmapped.externalSensorsZCoordinateUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.34.1", NULL, SU_FLAG_OK, NULL),
	/* unitDeviceCapabilities.1 = BITS: 00 00 00 00 00 00  */
	snmp_info_default("unmapped.unitDeviceCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.35.1", NULL, SU_FLAG_OK, NULL),
	/* outletSequencingDelay.1 = Gauge32: 200 */
	snmp_info_default("unmapped.outletSequencingDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.36.1", NULL, SU_FLAG_OK, NULL),
	/* globalOutletPowerCyclingPowerOffPeriod.1 = Gauge32: 10 */
	snmp_info_default("unmapped.globalOutletPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.37.1", NULL, SU_FLAG_OK, NULL),
	/* globalOutletStateOnStartup.1 = INTEGER: lastKnownState(2) */
	snmp_info_default("unmapped.globalOutletStateOnStartup", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.38.1", NULL, SU_FLAG_OK, NULL),
	/* outletPowerupSequence.1 = STRING: 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24 */
	snmp_info_default("unmapped.outletPowerupSequence", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.39.1", NULL, SU_FLAG_OK, NULL),
	/* pduPowerCyclingPowerOffPeriod.1 = Gauge32: 3 */
	snmp_info_default("unmapped.pduPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.40.1", NULL, SU_FLAG_OK, NULL),
	/* pduDaisychainMemberType.1 = INTEGER: standalone(0) */
	snmp_info_default("unmapped.pduDaisychainMemberType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.41.1", NULL, SU_FLAG_OK, NULL),
	/* managedExternalSensorCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.managedExternalSensorCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.42.1", NULL, SU_FLAG_OK, NULL),
	/* pxInetAddressType.1 = INTEGER: ipv4(1) */
	snmp_info_default("unmapped.pxInetAddressType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.50.1", NULL, SU_FLAG_OK, NULL),
	/* pxInetIPAddress.1 = Hex-STRING: C0 A8 14 BC  */
	snmp_info_default("unmapped.pxInetIPAddress", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.51.1", NULL, SU_FLAG_OK, NULL),
	/* pxInetNetmask.1 = Hex-STRING: FF FF FF 00  */
	snmp_info_default("unmapped.pxInetNetmask", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.52.1", NULL, SU_FLAG_OK, NULL),
	/* pxInetGateway.1 = Hex-STRING: C0 A8 14 FE  */
	snmp_info_default("unmapped.pxInetGateway", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.53.1", NULL, SU_FLAG_OK, NULL),
	/* loadShedding.1 = INTEGER: false(2) */
	snmp_info_default("unmapped.loadShedding", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.55.1", NULL, SU_FLAG_OK, NULL),
	/* serverCount.1 = INTEGER: 8 */
	snmp_info_default("unmapped.serverCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.56.1", NULL, SU_FLAG_OK, NULL),
	/* inrushGuardDelay.1 = Gauge32: 200 */
	snmp_info_default("unmapped.inrushGuardDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.57.1", NULL, SU_FLAG_OK, NULL),
	/* cascadedDeviceConnected.1 = INTEGER: false(2) */
	snmp_info_default("unmapped.cascadedDeviceConnected", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.58.1", NULL, SU_FLAG_OK, NULL),
	/* synchronizeWithNTPServer.1 = INTEGER: false(2) */
	snmp_info_default("unmapped.synchronizeWithNTPServer", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.59.1", NULL, SU_FLAG_OK, NULL),
	/* useDHCPProvidedNTPServer.1 = INTEGER: true(1) */
	snmp_info_default("unmapped.useDHCPProvidedNTPServer", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.60.1", NULL, SU_FLAG_OK, NULL),
	/* firstNTPServerAddressType.1 = INTEGER: unknown(0) */
	snmp_info_default("unmapped.firstNTPServerAddressType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.61.1", NULL, SU_FLAG_OK, NULL),
	/* firstNTPServerAddress.1 = "" */
	snmp_info_default("unmapped.firstNTPServerAddress", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.62.1", NULL, SU_FLAG_OK, NULL),
	/* secondNTPServerAddressType.1 = INTEGER: unknown(0) */
	snmp_info_default("unmapped.secondNTPServerAddressType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.63.1", NULL, SU_FLAG_OK, NULL),
	/* secondNTPServerAddress.1 = "" */
	snmp_info_default("unmapped.secondNTPServerAddress", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.64.1", NULL, SU_FLAG_OK, NULL),
	/* wireCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.wireCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.65.1", NULL, SU_FLAG_OK, NULL),
	/* transferSwitchCount.1 = INTEGER: 0 */
	snmp_info_default("unmapped.transferSwitchCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.66.1", NULL, SU_FLAG_OK, NULL),

	/* boardVersion.1.outletController.{1-6} = STRING: 60 */
	snmp_info_default("unmapped.boardVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.4.1.3.1", NULL, SU_FLAG_OK, NULL),

	/* boardFirmwareVersion.1.outletController.{1-6} = STRING: 1F */
	snmp_info_default("unmapped.boardFirmwareVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.6.1.3.1", NULL, SU_FLAG_OK, NULL),

	/* boardFirmwareTimeStamp.1.mainController.1 = Gauge32: 0 */
	snmp_info_default("unmapped.boardFirmwareTimeStamp", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.3.1.8.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* dataLogging.1 = INTEGER: true(1) */
	snmp_info_default("unmapped.dataLogging", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* measurementPeriod.1 = INTEGER: 1 */
	snmp_info_default("unmapped.measurementPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* measurementsPerLogEntry.1 = INTEGER: 60 */
	snmp_info_default("unmapped.measurementsPerLogEntry", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* logSize.1 = INTEGER: 120 */
	snmp_info_default("unmapped.logSize", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* dataLoggingEnableForAllSensors.1 = INTEGER: false(2) */
	snmp_info_default("unmapped.dataLoggingEnableForAllSensors", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* inletLabel.1.1 = STRING: I1 */
	snmp_info_default("unmapped.inletLabel", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.2.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletName.1.1 = STRING:  */
	snmp_info_default("unmapped.inletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.3.1.1", NULL, SU_FLAG_OK, NULL),

	/* inletPoleCount.1.1 = INTEGER: 2 */
	snmp_info_default("unmapped.inletPoleCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.5.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletRatedVoltage.1.1 = STRING: 100-120V */
	snmp_info_default("unmapped.inletRatedVoltage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.6.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletRatedCurrent.1.1 = STRING: 16A */
	snmp_info_default("unmapped.inletRatedCurrent", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.7.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletDeviceCapabilities.1.1 = BITS: 9F 00 00 00 00 00 rmsCurrent(0) rmsVoltage(3) activePower(4) apparentPower(5) powerFactor(6) activeEnergy(7)  */
	snmp_info_default("unmapped.inletDeviceCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.10.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletPoleCapabilities.1.1 = BITS: 00 00 00 00 00 00  */
	snmp_info_default("unmapped.inletPoleCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.11.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletPlugDescriptor.1.1 = STRING: IEC 60320 C20 */
	snmp_info_default("unmapped.inletPlugDescriptor", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.12.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLogAvailable.1.1.rmsCurrent = INTEGER: true(1) */
	snmp_info_default("unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLogAvailable.1.1.rmsVoltage = INTEGER: true(1) */
	snmp_info_default("unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLogAvailable.1.1.activePower = INTEGER: true(1) */
	snmp_info_default("unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLogAvailable.1.1.apparentPower = INTEGER: true(1) */
	snmp_info_default("unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLogAvailable.1.1.powerFactor = INTEGER: true(1) */
	snmp_info_default("unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLogAvailable.1.1.activeEnergy = INTEGER: true(1) */
	snmp_info_default("unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUnits.1.1.rmsCurrent = INTEGER: amp(2) */
	snmp_info_default("unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUnits.1.1.rmsVoltage = INTEGER: volt(1) */
	snmp_info_default("unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUnits.1.1.activePower = INTEGER: watt(3) */
	snmp_info_default("unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUnits.1.1.apparentPower = INTEGER: voltamp(4) */
	snmp_info_default("unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUnits.1.1.powerFactor = INTEGER: none(-1) */
	snmp_info_default("unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUnits.1.1.activeEnergy = INTEGER: wattHour(5) */
	snmp_info_default("unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorDecimalDigits.1.1.rmsCurrent = Gauge32: 1 */
	snmp_info_default("unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorDecimalDigits.1.1.rmsVoltage = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorDecimalDigits.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorDecimalDigits.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorDecimalDigits.1.1.powerFactor = Gauge32: 2 */
	snmp_info_default("unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorDecimalDigits.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorAccuracy.1.1.rmsCurrent = Gauge32: 100 */
	snmp_info_default("unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorAccuracy.1.1.rmsVoltage = Gauge32: 100 */
	snmp_info_default("unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorAccuracy.1.1.activePower = Gauge32: 300 */
	snmp_info_default("unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorAccuracy.1.1.apparentPower = Gauge32: 200 */
	snmp_info_default("unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorAccuracy.1.1.powerFactor = Gauge32: 500 */
	snmp_info_default("unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorAccuracy.1.1.activeEnergy = Gauge32: 100 */
	snmp_info_default("unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorResolution.1.1.rmsCurrent = Gauge32: 1 */
	snmp_info_default("unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorResolution.1.1.rmsVoltage = Gauge32: 1 */
	snmp_info_default("unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorResolution.1.1.activePower = Gauge32: 1 */
	snmp_info_default("unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorResolution.1.1.apparentPower = Gauge32: 1 */
	snmp_info_default("unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorResolution.1.1.powerFactor = Gauge32: 1 */
	snmp_info_default("unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorResolution.1.1.activeEnergy = Gauge32: 1 */
	snmp_info_default("unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorTolerance.1.1.rmsCurrent = Gauge32: 120 */
	snmp_info_default("unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorTolerance.1.1.rmsVoltage = Gauge32: 5 */
	snmp_info_default("unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorTolerance.1.1.activePower = Gauge32: 120 */
	snmp_info_default("unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorTolerance.1.1.apparentPower = Gauge32: 120 */
	snmp_info_default("unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorTolerance.1.1.powerFactor = Gauge32: 50 */
	snmp_info_default("unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorTolerance.1.1.activeEnergy = Gauge32: 120 */
	snmp_info_default("unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMaximum.1.1.rmsCurrent = Gauge32: 7680 */
	snmp_info_default("unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMaximum.1.1.rmsVoltage = Gauge32: 264 */
	snmp_info_default("unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMaximum.1.1.activePower = Gauge32: 202752 */
	snmp_info_default("unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMaximum.1.1.apparentPower = Gauge32: 202752 */
	snmp_info_default("unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMaximum.1.1.powerFactor = Gauge32: 100 */
	snmp_info_default("unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMaximum.1.1.activeEnergy = Gauge32: 4294967295 */
	snmp_info_default("unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMinimum.1.1.rmsCurrent = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMinimum.1.1.rmsVoltage = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMinimum.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMinimum.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMinimum.1.1.powerFactor = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorMinimum.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorHysteresis.1.1.rmsCurrent = Gauge32: 10 */
	snmp_info_default("unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorHysteresis.1.1.rmsVoltage = Gauge32: 2 */
	snmp_info_default("unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorHysteresis.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorHysteresis.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorHysteresis.1.1.powerFactor = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorHysteresis.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorStateChangeDelay.1.1.rmsCurrent = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorStateChangeDelay.1.1.rmsVoltage = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorStateChangeDelay.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorStateChangeDelay.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorStateChangeDelay.1.1.powerFactor = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorStateChangeDelay.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.8", NULL, SU_FLAG_OK, NULL),

	/* Inlet thresholds */
	/* inletSensorLowerCriticalThreshold.1.1.rmsCurrent = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerCriticalThreshold.1.1.rmsVoltage = Gauge32: 94 */
	snmp_info_default("unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerCriticalThreshold.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerCriticalThreshold.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerCriticalThreshold.1.1.powerFactor = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerCriticalThreshold.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerWarningThreshold.1.1.rmsCurrent = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerWarningThreshold.1.1.rmsVoltage = Gauge32: 97 */
	snmp_info_default("unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerWarningThreshold.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerWarningThreshold.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerWarningThreshold.1.1.powerFactor = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorLowerWarningThreshold.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperCriticalThreshold.1.1.rmsCurrent = Gauge32: 128 */
	snmp_info_default("unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperCriticalThreshold.1.1.rmsVoltage = Gauge32: 127 */
	snmp_info_default("unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperCriticalThreshold.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperCriticalThreshold.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperCriticalThreshold.1.1.powerFactor = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperCriticalThreshold.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperWarningThreshold.1.1.rmsCurrent = Gauge32: 104 */
	snmp_info_default("unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperWarningThreshold.1.1.rmsVoltage = Gauge32: 124 */
	snmp_info_default("unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperWarningThreshold.1.1.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperWarningThreshold.1.1.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperWarningThreshold.1.1.powerFactor = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorUpperWarningThreshold.1.1.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* inletSensorEnabledThresholds.1.1.rmsCurrent = BITS: 30 upperWarning(2) upperCritical(3)  */
	snmp_info_default("unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* inletSensorEnabledThresholds.1.1.rmsVoltage = BITS: F0 lowerCritical(0) lowerWarning(1) upperWarning(2) upperCritical(3)  */
	snmp_info_default("unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* inletSensorEnabledThresholds.1.1.activePower = BITS: 00  */
	snmp_info_default("unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* inletSensorEnabledThresholds.1.1.apparentPower = BITS: 00  */
	snmp_info_default("unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* inletSensorEnabledThresholds.1.1.powerFactor = BITS: 00  */
	snmp_info_default("unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* inletSensorEnabledThresholds.1.1.activeEnergy = BITS: 00  */
	snmp_info_default("unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.8", NULL, SU_FLAG_OK, NULL),

	/* outletPoleCount.1.{1-24} = INTEGER: 2 */
	snmp_info_default("unmapped.outletPoleCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.5.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletRatedVoltage.1.{1-24} = STRING: 100-120V */
	snmp_info_default("unmapped.outletRatedVoltage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.6.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletRatedCurrent.1.{1-24} = STRING: 16A */
	snmp_info_default("unmapped.outletRatedCurrent", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.7.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletDeviceCapabilities.1.{1-24} = BITS: 9F 04 00 00 00 00 rmsCurrent(0) rmsVoltage(3) activePower(4) apparentPower(5) powerFactor(6) activeEnergy(7) onOff(13)  */
	snmp_info_default("unmapped.outletDeviceCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.10.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletPowerCyclingPowerOffPeriod.1.{1-24} = Gauge32: 10 */
	snmp_info_default("unmapped.outletPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.12.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletStateOnStartup.1.{1-24} = INTEGER: globalOutletStateOnStartup(3) */
	snmp_info_default("unmapped.outletStateOnStartup", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.13.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletUseGlobalPowerCyclingPowerOffPeriod.1.{1-24} = INTEGER: true(1) */
	snmp_info_default("unmapped.outletUseGlobalPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.14.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletReceptacleDescriptor.1.{1-24} = STRING: NEMA 5-20R */
	snmp_info_default("unmapped.outletReceptacleDescriptor", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.29.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletNonCritical.1.{1-24} = INTEGER: false(2) */
	snmp_info_default("unmapped.outletNonCritical", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.30.1.1", NULL, SU_FLAG_OK, NULL),

	/* outletSequenceDelay.1.{1-24} = Gauge32: 0 */
	snmp_info_default("unmapped.outletSequenceDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.32.1.%i", NULL, SU_FLAG_OK, NULL),

	/* outletSensorLogAvailable.1.{1-24}.rmsCurrent = INTEGER: true(1) */
	snmp_info_default("unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.1", NULL, SU_FLAG_OK, NULL),
	/* outletSensorLogAvailable.1.{1-24}.rmsVoltage = INTEGER: true(1) */
	snmp_info_default("unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.4", NULL, SU_FLAG_OK, NULL),
	/* outletSensorLogAvailable.1.{1-24}.activePower = INTEGER: true(1) */
	snmp_info_default("unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.5", NULL, SU_FLAG_OK, NULL),
	/* outletSensorLogAvailable.1.{1-24}.apparentPower = INTEGER: true(1) */
	snmp_info_default("unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.6", NULL, SU_FLAG_OK, NULL),
	/* outletSensorLogAvailable.1.{1-24}.powerFactor = INTEGER: true(1) */
	snmp_info_default("unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.7", NULL, SU_FLAG_OK, NULL),
	/* outletSensorLogAvailable.1.{1-24}.activeEnergy = INTEGER: true(1) */
	snmp_info_default("unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.8", NULL, SU_FLAG_OK, NULL),
	/* outletSensorLogAvailable.1.{1-24}.onOff = INTEGER: true(1) */
	snmp_info_default("unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.14", NULL, SU_FLAG_OK, NULL),

	/* outletSensorUnits.1.{1-24}.rmsCurrent = INTEGER: amp(2) */
	snmp_info_default("unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.1", NULL, SU_FLAG_OK, NULL),
	/* outletSensorUnits.1.{1-24}.rmsVoltage = INTEGER: volt(1) */
	snmp_info_default("unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.4", NULL, SU_FLAG_OK, NULL),
	/* outletSensorUnits.1.{1-24}.activePower = INTEGER: watt(3) */
	snmp_info_default("unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.5", NULL, SU_FLAG_OK, NULL),
	/* outletSensorUnits.1.{1-24}.apparentPower = INTEGER: voltamp(4) */
	snmp_info_default("unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.6", NULL, SU_FLAG_OK, NULL),
	/* outletSensorUnits.1.{1-24}.powerFactor = INTEGER: none(-1) */
	snmp_info_default("unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.7", NULL, SU_FLAG_OK, NULL),
	/* outletSensorUnits.1.{1-24}.activeEnergy = INTEGER: wattHour(5) */
	snmp_info_default("unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.8", NULL, SU_FLAG_OK, NULL),
	/* outletSensorUnits.1.{1-24}.onOff = INTEGER: none(-1) */
	snmp_info_default("unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.14", NULL, SU_FLAG_OK, NULL),

	/* outletSensorDecimalDigits.1.{1-24}.rmsCurrent = Gauge32: 1 */
	snmp_info_default("unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.1", NULL, SU_FLAG_OK, NULL),
	/* outletSensorDecimalDigits.1.{1-24}.rmsVoltage = Gauge32: 0 */
	snmp_info_default("unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.4", NULL, SU_FLAG_OK, NULL),
	/* outletSensorDecimalDigits.1.{1-24}.activePower = Gauge32: 0 */
	snmp_info_default("unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.5", NULL, SU_FLAG_OK, NULL),
	/* outletSensorDecimalDigits.1.{1-24}.apparentPower = Gauge32: 0 */
	snmp_info_default("unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.6", NULL, SU_FLAG_OK, NULL),
	/* outletSensorDecimalDigits.1.{1-24}.powerFactor = Gauge32: 2 */
	snmp_info_default("unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.7", NULL, SU_FLAG_OK, NULL),
	/* outletSensorDecimalDigits.1.{1-24}.activeEnergy = Gauge32: 0 */
	snmp_info_default("unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.8", NULL, SU_FLAG_OK, NULL),
	/* outletSensorDecimalDigits.1.{1-24}.onOff = Gauge32: 0 */
	snmp_info_default("unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.14", NULL, SU_FLAG_OK, NULL),

	/* outletSensorAccuracy.1.{1-24}.rmsCurrent = Gauge32: 100 */
	snmp_info_default("unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.1", NULL, SU_FLAG_OK, NULL),
	/* outletSensorAccuracy.1.{1-24}.rmsVoltage = Gauge32: 100 */
	snmp_info_default("unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.4", NULL, SU_FLAG_OK, NULL),
	/* outletSensorAccuracy.1.{1-24}.activePower = Gauge32: 300 */
	snmp_info_default("unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.5", NULL, SU_FLAG_OK, NULL),
	/* outletSensorAccuracy.1.{1-24}.apparentPower = Gauge32: 200 */
	snmp_info_default("unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.6", NULL, SU_FLAG_OK, NULL),
	/* outletSensorAccuracy.1.{1-24}.powerFactor = Gauge32: 100 */
	snmp_info_default("unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.7", NULL, SU_FLAG_OK, NULL),
	/* outletSensorAccuracy.1.{1-24}.activeEnergy = Gauge32: 100 */
	snmp_info_default("unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.8", NULL, SU_FLAG_OK, NULL),
	/* outletSensorAccuracy.1.{1-24}.onOff = Gauge32: 0 */
	snmp_info_default("unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.14", NULL, SU_FLAG_OK, NULL),

	/* outletSensorResolution.1.{1-24}.rmsCurrent = Gauge32: 1 */
	snmp_info_default("unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.1", NULL, SU_FLAG_OK, NULL),
	/* outletSensorResolution.1.{1-24}.rmsVoltage = Gauge32: 1 */
	snmp_info_default("unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.4", NULL, SU_FLAG_OK, NULL),
	/* outletSensorResolution.1.{1-24}.activePower = Gauge32: 1 */
	snmp_info_default("unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.5", NULL, SU_FLAG_OK, NULL),
	/* outletSensorResolution.1.{1-24}.apparentPower = Gauge32: 1 */
	snmp_info_default("unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.6", NULL, SU_FLAG_OK, NULL),
	/* outletSensorResolution.1.{1-24}.powerFactor = Gauge32: 1 */
	snmp_info_default("unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.7", NULL, SU_FLAG_OK, NULL),
	/* outletSensorResolution.1.{1-24}.activeEnergy = Gauge32: 1 */
	snmp_info_default("unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.8", NULL, SU_FLAG_OK, NULL),
	/* outletSensorResolution.1.{1-24}.onOff = Gauge32: 0 */
	snmp_info_default("unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.14", NULL, SU_FLAG_OK, NULL),

	/* end of interesting data
	 * the rest is 18MB of verbose log and satellite data */

	/* Note: All reliabilityXXX data were removed */
#endif /* WITH_UNMAPPED_DATA_POINTS */
#endif /* DEBUG || WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	raritan_px2 = { "raritan-px2", RARITAN_PX2_MIB_VERSION, NULL, RARITAN_PX2_OID_MODEL_NAME, raritan_px2_mib, RARITAN_PX2_MIB_SYSOID, NULL };
