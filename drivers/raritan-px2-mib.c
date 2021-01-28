/* raritan-px2-mib.c - subdriver to monitor RARITAN PX2 SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *  2016 Arnaud Quette <ArnaudQuette@Eaton.com>
 *
 *  Based on initial work and data from Opengear <support@opengear.com>
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

#define RARITAN_PX2_MIB_VERSION  "0.2"

#define RARITAN_PX2_MIB_SYSOID     ".1.3.6.1.4.1.13742.6"
#define RARITAN_PX2_OID_MODEL_NAME ".1.3.6.1.4.1.13742.6.3.2.1.1.3.1"

/* info elements */
/* FIXME: triage between status and alarms, and make it compliant! */
static info_lkp_t raritanpx2_outlet_status_info[] = {
	{ -1, "unavailable", NULL, NULL },
	{  0, "open", NULL, NULL },
	{  1, "closed", NULL, NULL },
	{  2, "belowLowerCritical", NULL, NULL },
	{  3, "belowLowerWarning", NULL, NULL },
	{  4, "normal", NULL, NULL },
	{  5, "aboveUpperWarning", NULL, NULL },
	{  6, "aboveUpperCritical", NULL, NULL },
	{  7, "on", NULL, NULL },
	{  8, "off", NULL, NULL },
	{  9, "detected", NULL, NULL },
	{ 10, "notDetected", NULL, NULL },
	{ 11, "alarmed", NULL, NULL },
	{ 12, "ok", NULL, NULL },
	{ 13, "marginal", NULL, NULL },
	{ 14, "fail", NULL, NULL },
	{ 15, "yes", NULL, NULL },
	{ 16, "no", NULL, NULL },
	{ 17, "standby", NULL, NULL },
	{ 18, "one", NULL, NULL },
	{ 19, "two", NULL, NULL },
	{ 20, "inSync", NULL, NULL },
	{ 21, "outOfSync", NULL, NULL },
	{ 0, "NULL", NULL, NULL }
};

static info_lkp_t raritanpx2_outlet_switchability_info[] = {
	{ -1, "yes", NULL, NULL },
	{ 1, "yes", NULL, NULL },
	{ 2, "no", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* PDU2-MIB Snmp2NUT lookup table */
static snmp_info_t raritan_px2_mib[] = {

	/* pduManufacturer.1 = STRING: Raritan */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.2.1",
		"Raritan", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* pduModel.1 = STRING: PX2-5475 */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.3.1",
		"Raritan PX2 SNMP PDU device", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* pduSerialNumber.1 = STRING: QFC3950619 */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.4.1",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* pxMACAddress.1 = STRING: 0:d:5d:b:49:0 */
	{ "device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.11.1",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/* boardVersion.1.mainController.1 = STRING: 0x01 */
	/* FIXME: not compliant! to be RFC'ed */
	{ "device.revision", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.4.1.1.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* FIXME: move to device collection! */
	/* Wrong OID!
	 * { "ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.6.1.1.1", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL },*/
	/* boardFirmwareVersion.1.mainController.1 = STRING: 2.4.3.5-40298 */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.6.1.1.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* pduName.1 = STRING: my PX */
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.13.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.3.1",
		"Raritan PX2 SNMP PDU device", SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/* Input data:
	 * Units are given in inletSensorUnits.1.1.%i
	 * Value should be scaled by inletSensorDecimalDigits.1.1.%i
	 * For example, if the value is 1 and inletSensorDecimalDigits is 2, then actual value is 0.01. */
	/* measurementsInletSensorValue.1.1.rmsCurrent = Gauge32: 10 (A) */
	{ "input.load", 0, 0.1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* measurementsInletSensorValue.1.1.rmsVoltage = Gauge32: 119 (V) */
	{ "input.voltage", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* measurementsInletSensorValue.1.1.activePower = Gauge32: 10 (W) */
	{ "input.realpower", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* measurementsInletSensorValue.1.1.apparentPower = Gauge32: 122 (VA) */
	{ "input.power", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* measurementsInletSensorValue.1.1.powerFactor = Gauge32: 8 (none) */
	/* FIXME: need RFC! */
	{ "input.powerfactor", 0, 0.01, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* measurementsInletSensorValue.1.1.activeEnergy = Gauge32: 193359 (wattHour) */
	/* { "unmapped.measurementsInletSensorValue", 0, 1, ".1.3.6.1.4.1.13742.6.5.2.3.1.4.1.1.8", NULL, SU_FLAG_OK, NULL }, */

	/* inletPlug.1.1 = INTEGER: plugIEC320C20(6) */
	/* FIXME: need RFC (input.type | [input.]inlet.type...) and standardization
	 * { "unmapped.inletPlug", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.4.1.1", NULL, SU_FLAG_OK, NULL },*/

	/* outletCount.1 = INTEGER: 24 */
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.4.1", "0", SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/* outlet template definition
	 * Indexes start from 1, ie outlet.1 => <OID>.1 */
	/* Note: the first definition is used to determine the base index (ie 0 or 1) */
	/* outletName.1.%i = STRING:  */
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.3.1.%i", NULL, SU_OUTLET, NULL },
	/* outletSwitchingState.1.%i = INTEGER: on(7) */
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.4.1.2.1.3.1.%i", NULL, SU_OUTLET, &raritanpx2_outlet_status_info[0] },
	/* outletLabel.1.%i = STRING: 1 */
	{ "outlet.%i.id", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.2.1.%i", "%i", SU_FLAG_STATIC | SU_OUTLET | SU_FLAG_OK, NULL },

	/* outletReceptacle.1.1 = INTEGER: receptacleNEMA520R(37) */
	/* FIXME: need RFC and standardization
	 * { "outlet.%i.type", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.4.1.%i", NULL, SU_OUTLET | SU_FLAG_OK, NULL }, */

	/* RMS Current (divide by 10). e.g. 5 == 0.5A */
	/* measurementsOutletSensorValue.1.%i.rmsCurrent = Gauge32: 10 */
	{ "outlet.%i.current", 0, 0.1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.1", NULL, SU_OUTLET | SU_FLAG_OK, NULL },
	/* measurementsOutletSensorValue.1.%i.rmsVoltage = Gauge32: 119 */
	{ "outlet.%i.voltage", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.4", "%i", SU_OUTLET | SU_FLAG_OK, NULL },
	/* measurementsOutletSensorValue.1.%i.activePower = Gauge32: 10 */
	{ "outlet.%i.power", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.5", "%i", SU_OUTLET | SU_FLAG_OK, NULL },
	/* measurementsOutletSensorValue.1.%i.apparentPower = Gauge32: 122 */
	{ "outlet.%i.realpower", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.6", "%i", SU_OUTLET | SU_FLAG_OK, NULL },
	/* measurementsOutletSensorValue.1.%i.powerFactor = Gauge32: 8 */
	{ "outlet.%i.powerfactor", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.%i.7", "%i", SU_OUTLET | SU_FLAG_OK, NULL },
	/* measurementsOutletSensorValue.1.1.activeEnergy = Gauge32: 89890 */
	/* FIXME:
	 * { "unmapped.measurementsOutletSensorValue", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.1.8", NULL, SU_FLAG_OK, NULL }, */
	/* measurementsOutletSensorValue.1.1.onOff = Gauge32: 0 */
	/* FIXME:
	 * { "unmapped.measurementsOutletSensorValue", 0, 1, ".1.3.6.1.4.1.13742.6.5.4.3.1.4.1.1.14", NULL, SU_FLAG_OK, NULL }, */

	/* outletSwitchable.1.%i = INTEGER: true(1) */
	{ "outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.28.1.%i", "no", SU_FLAG_STATIC | SU_OUTLET | SU_FLAG_OK, &raritanpx2_outlet_switchability_info[0] },

	/* instant commands. */
	/* switchingOperation.1.1 = INTEGER: on(1) */
	{ "outlet.%i.load.off",   0, 1, ".1.3.6.1.4.1.13742.6.4.1.2.1.2.1.%i", "0", SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.on",    0, 1, ".1.3.6.1.4.1.13742.6.4.1.2.1.2.1.%i", "1", SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.cycle", 0, 1, ".1.3.6.1.4.1.13742.6.4.1.2.1.2.1.%i", "2", SU_TYPE_CMD | SU_OUTLET, NULL },

#ifdef DEBUG
	/* pduCount.0 = INTEGER: 1 */
	/* FIXME: part of daisychain support, RFC device.count */
	{ "device.count", 0, 1, ".1.3.6.1.4.1.13742.6.3.1.0", NULL, SU_FLAG_OK, NULL },

	/* pduRatedVoltage.1 = STRING: 100-120V */
	{ "unmapped.pduRatedVoltage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* pduRatedCurrent.1 = STRING: 16A */
	{ "unmapped.pduRatedCurrent", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* pduRatedFrequency.1 = STRING: 50/60Hz */
	{ "unmapped.pduRatedFrequency", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.7.1", NULL, SU_FLAG_OK, NULL },
	/* pduRatedVA.1 = STRING: 1.6-1.9kVA */
	{ "unmapped.pduRatedVA", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.8.1", NULL, SU_FLAG_OK, NULL },
	/* pduImage.1 = STRING:  */
	{ "unmapped.pduImage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.1.1.9.1", NULL, SU_FLAG_OK, NULL },
	/* inletCount.1 = INTEGER: 1 */
	{ "unmapped.inletCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* overCurrentProtectorCount.1 = INTEGER: 0 */
	{ "unmapped.overCurrentProtectorCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.3.1", NULL, SU_FLAG_OK, NULL },

	/* inletControllerCount.1 = INTEGER: 0 */
	{ "unmapped.inletControllerCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* outletControllerCount.1 = INTEGER: 6 */
	{ "unmapped.outletControllerCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* externalSensorCount.1 = INTEGER: 16 */
	{ "unmapped.externalSensorCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.7.1", NULL, SU_FLAG_OK, NULL },
	/* pxIPAddress.1 = IpAddress: 192.168.20.188 */
	{ "unmapped.pxIPAddress", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.8.1", NULL, SU_FLAG_OK, NULL },
	/* netmask.1 = IpAddress: 255.255.255.0 */
	{ "unmapped.netmask", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.9.1", NULL, SU_FLAG_OK, NULL },
	/* gateway.1 = IpAddress: 192.168.20.254 */
	{ "unmapped.gateway", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.10.1", NULL, SU_FLAG_OK, NULL },
	/* utcOffset.1 = STRING: -5:00 */
	{ "unmapped.utcOffset", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.12.1", NULL, SU_FLAG_OK, NULL },
	/* externalSensorsZCoordinateUnits.1 = INTEGER: rackUnits(0) */
	{ "unmapped.externalSensorsZCoordinateUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.34.1", NULL, SU_FLAG_OK, NULL },
	/* unitDeviceCapabilities.1 = BITS: 00 00 00 00 00 00  */
	{ "unmapped.unitDeviceCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.35.1", NULL, SU_FLAG_OK, NULL },
	/* outletSequencingDelay.1 = Gauge32: 200 */
	{ "unmapped.outletSequencingDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.36.1", NULL, SU_FLAG_OK, NULL },
	/* globalOutletPowerCyclingPowerOffPeriod.1 = Gauge32: 10 */
	{ "unmapped.globalOutletPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.37.1", NULL, SU_FLAG_OK, NULL },
	/* globalOutletStateOnStartup.1 = INTEGER: lastKnownState(2) */
	{ "unmapped.globalOutletStateOnStartup", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.38.1", NULL, SU_FLAG_OK, NULL },
	/* outletPowerupSequence.1 = STRING: 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24 */
	{ "unmapped.outletPowerupSequence", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.39.1", NULL, SU_FLAG_OK, NULL },
	/* pduPowerCyclingPowerOffPeriod.1 = Gauge32: 3 */
	{ "unmapped.pduPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.40.1", NULL, SU_FLAG_OK, NULL },
	/* pduDaisychainMemberType.1 = INTEGER: standalone(0) */
	{ "unmapped.pduDaisychainMemberType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.41.1", NULL, SU_FLAG_OK, NULL },
	/* managedExternalSensorCount.1 = INTEGER: 0 */
	{ "unmapped.managedExternalSensorCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.42.1", NULL, SU_FLAG_OK, NULL },
	/* pxInetAddressType.1 = INTEGER: ipv4(1) */
	{ "unmapped.pxInetAddressType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.50.1", NULL, SU_FLAG_OK, NULL },
	/* pxInetIPAddress.1 = Hex-STRING: C0 A8 14 BC  */
	{ "unmapped.pxInetIPAddress", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.51.1", NULL, SU_FLAG_OK, NULL },
	/* pxInetNetmask.1 = Hex-STRING: FF FF FF 00  */
	{ "unmapped.pxInetNetmask", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.52.1", NULL, SU_FLAG_OK, NULL },
	/* pxInetGateway.1 = Hex-STRING: C0 A8 14 FE  */
	{ "unmapped.pxInetGateway", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.2.1.53.1", NULL, SU_FLAG_OK, NULL },
	/* loadShedding.1 = INTEGER: false(2) */
	{ "unmapped.loadShedding", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.55.1", NULL, SU_FLAG_OK, NULL },
	/* serverCount.1 = INTEGER: 8 */
	{ "unmapped.serverCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.56.1", NULL, SU_FLAG_OK, NULL },
	/* inrushGuardDelay.1 = Gauge32: 200 */
	{ "unmapped.inrushGuardDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.57.1", NULL, SU_FLAG_OK, NULL },
	/* cascadedDeviceConnected.1 = INTEGER: false(2) */
	{ "unmapped.cascadedDeviceConnected", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.58.1", NULL, SU_FLAG_OK, NULL },
	/* synchronizeWithNTPServer.1 = INTEGER: false(2) */
	{ "unmapped.synchronizeWithNTPServer", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.59.1", NULL, SU_FLAG_OK, NULL },
	/* useDHCPProvidedNTPServer.1 = INTEGER: true(1) */
	{ "unmapped.useDHCPProvidedNTPServer", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.60.1", NULL, SU_FLAG_OK, NULL },
	/* firstNTPServerAddressType.1 = INTEGER: unknown(0) */
	{ "unmapped.firstNTPServerAddressType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.61.1", NULL, SU_FLAG_OK, NULL },
	/* firstNTPServerAddress.1 = "" */
	{ "unmapped.firstNTPServerAddress", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.62.1", NULL, SU_FLAG_OK, NULL },
	/* secondNTPServerAddressType.1 = INTEGER: unknown(0) */
	{ "unmapped.secondNTPServerAddressType", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.63.1", NULL, SU_FLAG_OK, NULL },
	/* secondNTPServerAddress.1 = "" */
	{ "unmapped.secondNTPServerAddress", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.64.1", NULL, SU_FLAG_OK, NULL },
	/* wireCount.1 = INTEGER: 0 */
	{ "unmapped.wireCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.65.1", NULL, SU_FLAG_OK, NULL },
	/* transferSwitchCount.1 = INTEGER: 0 */
	{ "unmapped.transferSwitchCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.2.1.66.1", NULL, SU_FLAG_OK, NULL },

	/* boardVersion.1.outletController.{1-6} = STRING: 60 */
	{ "unmapped.boardVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.4.1.3.1", NULL, SU_FLAG_OK, NULL },

	/* boardFirmwareVersion.1.outletController.{1-6} = STRING: 1F */
	{ "unmapped.boardFirmwareVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.2.3.1.6.1.3.1", NULL, SU_FLAG_OK, NULL },

	/* boardFirmwareTimeStamp.1.mainController.1 = Gauge32: 0 */
	{ "unmapped.boardFirmwareTimeStamp", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.3.1.8.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* dataLogging.1 = INTEGER: true(1) */
	{ "unmapped.dataLogging", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* measurementPeriod.1 = INTEGER: 1 */
	{ "unmapped.measurementPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* measurementsPerLogEntry.1 = INTEGER: 60 */
	{ "unmapped.measurementsPerLogEntry", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* logSize.1 = INTEGER: 120 */
	{ "unmapped.logSize", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* dataLoggingEnableForAllSensors.1 = INTEGER: false(2) */
	{ "unmapped.dataLoggingEnableForAllSensors", 0, 1, ".1.3.6.1.4.1.13742.6.3.2.4.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* inletLabel.1.1 = STRING: I1 */
	{ "unmapped.inletLabel", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.2.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletName.1.1 = STRING:  */
	{ "unmapped.inletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.3.1.1", NULL, SU_FLAG_OK, NULL },

	/* inletPoleCount.1.1 = INTEGER: 2 */
	{ "unmapped.inletPoleCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.5.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletRatedVoltage.1.1 = STRING: 100-120V */
	{ "unmapped.inletRatedVoltage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.6.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletRatedCurrent.1.1 = STRING: 16A */
	{ "unmapped.inletRatedCurrent", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.7.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletDeviceCapabilities.1.1 = BITS: 9F 00 00 00 00 00 rmsCurrent(0) rmsVoltage(3) activePower(4) apparentPower(5) powerFactor(6) activeEnergy(7)  */
	{ "unmapped.inletDeviceCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.10.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletPoleCapabilities.1.1 = BITS: 00 00 00 00 00 00  */
	{ "unmapped.inletPoleCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.3.1.11.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletPlugDescriptor.1.1 = STRING: IEC 60320 C20 */
	{ "unmapped.inletPlugDescriptor", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.3.3.1.12.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLogAvailable.1.1.rmsCurrent = INTEGER: true(1) */
	{ "unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLogAvailable.1.1.rmsVoltage = INTEGER: true(1) */
	{ "unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLogAvailable.1.1.activePower = INTEGER: true(1) */
	{ "unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLogAvailable.1.1.apparentPower = INTEGER: true(1) */
	{ "unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLogAvailable.1.1.powerFactor = INTEGER: true(1) */
	{ "unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLogAvailable.1.1.activeEnergy = INTEGER: true(1) */
	{ "unmapped.inletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.4.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUnits.1.1.rmsCurrent = INTEGER: amp(2) */
	{ "unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUnits.1.1.rmsVoltage = INTEGER: volt(1) */
	{ "unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUnits.1.1.activePower = INTEGER: watt(3) */
	{ "unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUnits.1.1.apparentPower = INTEGER: voltamp(4) */
	{ "unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUnits.1.1.powerFactor = INTEGER: none(-1) */
	{ "unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUnits.1.1.activeEnergy = INTEGER: wattHour(5) */
	{ "unmapped.inletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.6.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorDecimalDigits.1.1.rmsCurrent = Gauge32: 1 */
	{ "unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorDecimalDigits.1.1.rmsVoltage = Gauge32: 0 */
	{ "unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorDecimalDigits.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorDecimalDigits.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorDecimalDigits.1.1.powerFactor = Gauge32: 2 */
	{ "unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorDecimalDigits.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.7.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorAccuracy.1.1.rmsCurrent = Gauge32: 100 */
	{ "unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorAccuracy.1.1.rmsVoltage = Gauge32: 100 */
	{ "unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorAccuracy.1.1.activePower = Gauge32: 300 */
	{ "unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorAccuracy.1.1.apparentPower = Gauge32: 200 */
	{ "unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorAccuracy.1.1.powerFactor = Gauge32: 500 */
	{ "unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorAccuracy.1.1.activeEnergy = Gauge32: 100 */
	{ "unmapped.inletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.8.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorResolution.1.1.rmsCurrent = Gauge32: 1 */
	{ "unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorResolution.1.1.rmsVoltage = Gauge32: 1 */
	{ "unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorResolution.1.1.activePower = Gauge32: 1 */
	{ "unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorResolution.1.1.apparentPower = Gauge32: 1 */
	{ "unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorResolution.1.1.powerFactor = Gauge32: 1 */
	{ "unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorResolution.1.1.activeEnergy = Gauge32: 1 */
	{ "unmapped.inletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.9.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorTolerance.1.1.rmsCurrent = Gauge32: 120 */
	{ "unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorTolerance.1.1.rmsVoltage = Gauge32: 5 */
	{ "unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorTolerance.1.1.activePower = Gauge32: 120 */
	{ "unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorTolerance.1.1.apparentPower = Gauge32: 120 */
	{ "unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorTolerance.1.1.powerFactor = Gauge32: 50 */
	{ "unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorTolerance.1.1.activeEnergy = Gauge32: 120 */
	{ "unmapped.inletSensorTolerance", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.10.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMaximum.1.1.rmsCurrent = Gauge32: 7680 */
	{ "unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMaximum.1.1.rmsVoltage = Gauge32: 264 */
	{ "unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMaximum.1.1.activePower = Gauge32: 202752 */
	{ "unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMaximum.1.1.apparentPower = Gauge32: 202752 */
	{ "unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMaximum.1.1.powerFactor = Gauge32: 100 */
	{ "unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMaximum.1.1.activeEnergy = Gauge32: 4294967295 */
	{ "unmapped.inletSensorMaximum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.11.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMinimum.1.1.rmsCurrent = Gauge32: 0 */
	{ "unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMinimum.1.1.rmsVoltage = Gauge32: 0 */
	{ "unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMinimum.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMinimum.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMinimum.1.1.powerFactor = Gauge32: 0 */
	{ "unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorMinimum.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorMinimum", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.12.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorHysteresis.1.1.rmsCurrent = Gauge32: 10 */
	{ "unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorHysteresis.1.1.rmsVoltage = Gauge32: 2 */
	{ "unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorHysteresis.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorHysteresis.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorHysteresis.1.1.powerFactor = Gauge32: 0 */
	{ "unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorHysteresis.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorHysteresis", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.13.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorStateChangeDelay.1.1.rmsCurrent = Gauge32: 0 */
	{ "unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorStateChangeDelay.1.1.rmsVoltage = Gauge32: 0 */
	{ "unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorStateChangeDelay.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorStateChangeDelay.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorStateChangeDelay.1.1.powerFactor = Gauge32: 0 */
	{ "unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorStateChangeDelay.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorStateChangeDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.14.1.1.8", NULL, SU_FLAG_OK, NULL },

/* Inlet thresholds */
	/* inletSensorLowerCriticalThreshold.1.1.rmsCurrent = Gauge32: 0 */
	{ "unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerCriticalThreshold.1.1.rmsVoltage = Gauge32: 94 */
	{ "unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerCriticalThreshold.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerCriticalThreshold.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerCriticalThreshold.1.1.powerFactor = Gauge32: 0 */
	{ "unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerCriticalThreshold.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorLowerCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.21.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerWarningThreshold.1.1.rmsCurrent = Gauge32: 0 */
	{ "unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerWarningThreshold.1.1.rmsVoltage = Gauge32: 97 */
	{ "unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerWarningThreshold.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerWarningThreshold.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerWarningThreshold.1.1.powerFactor = Gauge32: 0 */
	{ "unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorLowerWarningThreshold.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorLowerWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.22.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperCriticalThreshold.1.1.rmsCurrent = Gauge32: 128 */
	{ "unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperCriticalThreshold.1.1.rmsVoltage = Gauge32: 127 */
	{ "unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperCriticalThreshold.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperCriticalThreshold.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperCriticalThreshold.1.1.powerFactor = Gauge32: 0 */
	{ "unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperCriticalThreshold.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorUpperCriticalThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.23.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperWarningThreshold.1.1.rmsCurrent = Gauge32: 104 */
	{ "unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperWarningThreshold.1.1.rmsVoltage = Gauge32: 124 */
	{ "unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperWarningThreshold.1.1.activePower = Gauge32: 0 */
	{ "unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperWarningThreshold.1.1.apparentPower = Gauge32: 0 */
	{ "unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperWarningThreshold.1.1.powerFactor = Gauge32: 0 */
	{ "unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorUpperWarningThreshold.1.1.activeEnergy = Gauge32: 0 */
	{ "unmapped.inletSensorUpperWarningThreshold", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.24.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* inletSensorEnabledThresholds.1.1.rmsCurrent = BITS: 30 upperWarning(2) upperCritical(3)  */
	{ "unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* inletSensorEnabledThresholds.1.1.rmsVoltage = BITS: F0 lowerCritical(0) lowerWarning(1) upperWarning(2) upperCritical(3)  */
	{ "unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* inletSensorEnabledThresholds.1.1.activePower = BITS: 00  */
	{ "unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* inletSensorEnabledThresholds.1.1.apparentPower = BITS: 00  */
	{ "unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* inletSensorEnabledThresholds.1.1.powerFactor = BITS: 00  */
	{ "unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* inletSensorEnabledThresholds.1.1.activeEnergy = BITS: 00  */
	{ "unmapped.inletSensorEnabledThresholds", 0, 1, ".1.3.6.1.4.1.13742.6.3.3.4.1.25.1.1.8", NULL, SU_FLAG_OK, NULL },

	/* outletPoleCount.1.{1-24} = INTEGER: 2 */
	{ "unmapped.outletPoleCount", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.5.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletRatedVoltage.1.{1-24} = STRING: 100-120V */
	{ "unmapped.outletRatedVoltage", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.6.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletRatedCurrent.1.{1-24} = STRING: 16A */
	{ "unmapped.outletRatedCurrent", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.7.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletDeviceCapabilities.1.{1-24} = BITS: 9F 04 00 00 00 00 rmsCurrent(0) rmsVoltage(3) activePower(4) apparentPower(5) powerFactor(6) activeEnergy(7) onOff(13)  */
	{ "unmapped.outletDeviceCapabilities", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.10.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletPowerCyclingPowerOffPeriod.1.{1-24} = Gauge32: 10 */
	{ "unmapped.outletPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.12.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletStateOnStartup.1.{1-24} = INTEGER: globalOutletStateOnStartup(3) */
	{ "unmapped.outletStateOnStartup", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.13.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletUseGlobalPowerCyclingPowerOffPeriod.1.{1-24} = INTEGER: true(1) */
	{ "unmapped.outletUseGlobalPowerCyclingPowerOffPeriod", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.14.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletReceptacleDescriptor.1.{1-24} = STRING: NEMA 5-20R */
	{ "unmapped.outletReceptacleDescriptor", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.6.3.5.3.1.29.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletNonCritical.1.{1-24} = INTEGER: false(2) */
	{ "unmapped.outletNonCritical", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.30.1.1", NULL, SU_FLAG_OK, NULL },

	/* outletSequenceDelay.1.{1-24} = Gauge32: 0 */
	{ "unmapped.outletSequenceDelay", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.3.1.32.1.%i", NULL, SU_FLAG_OK, NULL },

	/* outletSensorLogAvailable.1.{1-24}.rmsCurrent = INTEGER: true(1) */
	{ "unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.1", NULL, SU_FLAG_OK, NULL },
	/* outletSensorLogAvailable.1.{1-24}.rmsVoltage = INTEGER: true(1) */
	{ "unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.4", NULL, SU_FLAG_OK, NULL },
	/* outletSensorLogAvailable.1.{1-24}.activePower = INTEGER: true(1) */
	{ "unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.5", NULL, SU_FLAG_OK, NULL },
	/* outletSensorLogAvailable.1.{1-24}.apparentPower = INTEGER: true(1) */
	{ "unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.6", NULL, SU_FLAG_OK, NULL },
	/* outletSensorLogAvailable.1.{1-24}.powerFactor = INTEGER: true(1) */
	{ "unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.7", NULL, SU_FLAG_OK, NULL },
	/* outletSensorLogAvailable.1.{1-24}.activeEnergy = INTEGER: true(1) */
	{ "unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.8", NULL, SU_FLAG_OK, NULL },
	/* outletSensorLogAvailable.1.{1-24}.onOff = INTEGER: true(1) */
	{ "unmapped.outletSensorLogAvailable", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.4.1.%i.14", NULL, SU_FLAG_OK, NULL },

	/* outletSensorUnits.1.{1-24}.rmsCurrent = INTEGER: amp(2) */
	{ "unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.1", NULL, SU_FLAG_OK, NULL },
	/* outletSensorUnits.1.{1-24}.rmsVoltage = INTEGER: volt(1) */
	{ "unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.4", NULL, SU_FLAG_OK, NULL },
	/* outletSensorUnits.1.{1-24}.activePower = INTEGER: watt(3) */
	{ "unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.5", NULL, SU_FLAG_OK, NULL },
	/* outletSensorUnits.1.{1-24}.apparentPower = INTEGER: voltamp(4) */
	{ "unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.6", NULL, SU_FLAG_OK, NULL },
	/* outletSensorUnits.1.{1-24}.powerFactor = INTEGER: none(-1) */
	{ "unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.7", NULL, SU_FLAG_OK, NULL },
	/* outletSensorUnits.1.{1-24}.activeEnergy = INTEGER: wattHour(5) */
	{ "unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.8", NULL, SU_FLAG_OK, NULL },
	/* outletSensorUnits.1.{1-24}.onOff = INTEGER: none(-1) */
	{ "unmapped.outletSensorUnits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.6.1.%i.14", NULL, SU_FLAG_OK, NULL },

	/* outletSensorDecimalDigits.1.{1-24}.rmsCurrent = Gauge32: 1 */
	{ "unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.1", NULL, SU_FLAG_OK, NULL },
	/* outletSensorDecimalDigits.1.{1-24}.rmsVoltage = Gauge32: 0 */
	{ "unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.4", NULL, SU_FLAG_OK, NULL },
	/* outletSensorDecimalDigits.1.{1-24}.activePower = Gauge32: 0 */
	{ "unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.5", NULL, SU_FLAG_OK, NULL },
	/* outletSensorDecimalDigits.1.{1-24}.apparentPower = Gauge32: 0 */
	{ "unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.6", NULL, SU_FLAG_OK, NULL },
	/* outletSensorDecimalDigits.1.{1-24}.powerFactor = Gauge32: 2 */
	{ "unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.7", NULL, SU_FLAG_OK, NULL },
	/* outletSensorDecimalDigits.1.{1-24}.activeEnergy = Gauge32: 0 */
	{ "unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.8", NULL, SU_FLAG_OK, NULL },
	/* outletSensorDecimalDigits.1.{1-24}.onOff = Gauge32: 0 */
	{ "unmapped.outletSensorDecimalDigits", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.7.1.%i.14", NULL, SU_FLAG_OK, NULL },

	/* outletSensorAccuracy.1.{1-24}.rmsCurrent = Gauge32: 100 */
	{ "unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.1", NULL, SU_FLAG_OK, NULL },
	/* outletSensorAccuracy.1.{1-24}.rmsVoltage = Gauge32: 100 */
	{ "unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.4", NULL, SU_FLAG_OK, NULL },
	/* outletSensorAccuracy.1.{1-24}.activePower = Gauge32: 300 */
	{ "unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.5", NULL, SU_FLAG_OK, NULL },
	/* outletSensorAccuracy.1.{1-24}.apparentPower = Gauge32: 200 */
	{ "unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.6", NULL, SU_FLAG_OK, NULL },
	/* outletSensorAccuracy.1.{1-24}.powerFactor = Gauge32: 100 */
	{ "unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.7", NULL, SU_FLAG_OK, NULL },
	/* outletSensorAccuracy.1.{1-24}.activeEnergy = Gauge32: 100 */
	{ "unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.8", NULL, SU_FLAG_OK, NULL },
	/* outletSensorAccuracy.1.{1-24}.onOff = Gauge32: 0 */
	{ "unmapped.outletSensorAccuracy", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.8.1.%i.14", NULL, SU_FLAG_OK, NULL },

	/* outletSensorResolution.1.{1-24}.rmsCurrent = Gauge32: 1 */
	{ "unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.1", NULL, SU_FLAG_OK, NULL },
	/* outletSensorResolution.1.{1-24}.rmsVoltage = Gauge32: 1 */
	{ "unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.4", NULL, SU_FLAG_OK, NULL },
	/* outletSensorResolution.1.{1-24}.activePower = Gauge32: 1 */
	{ "unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.5", NULL, SU_FLAG_OK, NULL },
	/* outletSensorResolution.1.{1-24}.apparentPower = Gauge32: 1 */
	{ "unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.6", NULL, SU_FLAG_OK, NULL },
	/* outletSensorResolution.1.{1-24}.powerFactor = Gauge32: 1 */
	{ "unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.7", NULL, SU_FLAG_OK, NULL },
	/* outletSensorResolution.1.{1-24}.activeEnergy = Gauge32: 1 */
	{ "unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.8", NULL, SU_FLAG_OK, NULL },
	/* outletSensorResolution.1.{1-24}.onOff = Gauge32: 0 */
	{ "unmapped.outletSensorResolution", 0, 1, ".1.3.6.1.4.1.13742.6.3.5.4.1.9.1.%i.14", NULL, SU_FLAG_OK, NULL },

	/* end of interesting data
	 * the rest is 18MB of verbose log and satellite data */

	/* Note: All reliabilityXXX data were removed */
#endif /* DEBUG */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	raritan_px2 = { "raritan-px2", RARITAN_PX2_MIB_VERSION, NULL, RARITAN_PX2_OID_MODEL_NAME, raritan_px2_mib, RARITAN_PX2_MIB_SYSOID, NULL };
