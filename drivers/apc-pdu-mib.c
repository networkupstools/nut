/* apc-pdu-mib.c - subdriver to monitor APC PDU using PowerNet-MIB SNMP with NUT
 *
 *  Copyright (C) 2016 - Eaton
 *  Authors:	Tomas Halman <TomasHalman@Eaton.com>
 *  			Arnaud Quette <ArnaudQuette@Eaton.com>
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

#include "apc-pdu-mib.h"

#define APC_PDU_MIB_VERSION  "0.40"

#define APC_PDU_MIB_SYSOID_RPDU      ".1.3.6.1.4.1.318.1.3.4.4"
#define APC_PDU_MIB_SYSOID_RPDU2     ".1.3.6.1.4.1.318.1.3.4.5"
#define APC_PDU_MIB_SYSOID_MSP       ".1.3.6.1.4.1.318.1.3.4.6"
#define APC_PDU_DEVICE_MODEL         ".1.3.6.1.4.1.318.1.1.4.1.4.0"


static info_lkp_t apc_pdu_sw_outlet_status_info[] = {
	info_lkp_default(1, "on"),
	info_lkp_default(2, "off"),
	info_lkp_sentinel
};

static info_lkp_t apc_pdu_sw_outlet_switchability_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "yes"),
	info_lkp_sentinel
};

/* POWERNET-MIB Snmp2NUT lookup table */
static snmp_info_t apc_pdu_mib[] = {

	/* Device page */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "APC",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	/* sPDUIdentModelNumber.0 = STRING: "AP7900" */
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.4.0",
		"Switched ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL,
		SU_FLAG_STALE | SU_FLAG_OK, NULL),
	snmp_info_default("device.description", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.5.0", NULL,
		SU_FLAG_STALE | SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL,
		SU_FLAG_STALE | SU_FLAG_OK, NULL),
	/* FIXME: to be RFC'ed */
	snmp_info_default("device.uptime", 0, 1, ".1.3.6.1.2.1.1.3.0", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	/* sPDUIdentSerialNumber.0 = STRING: "5A1234E00874" */
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* sPDUIdentModelNumber.0 = STRING: "AP7900" */
	snmp_info_default("device.part", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.4.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* sPDUIdentHardwareRev.0 = STRING: "B2" */
	snmp_info_default("device.version", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	/* sPDUIdentFirmwareRev.0 = STRING: "v3.7.3" */
	/* FIXME: to be moved to device.firmware */
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUIdentDateOfManufacture.0 = STRING: "08/13/2012" */
	/* FIXME: to be moved to the device collection! */
	snmp_info_default("ups.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.3.0", NULL, SU_FLAG_OK, NULL),

	/* Input */
	/* rPDUIdentDevicePowerWatts.0 = INTEGER: 0 */
	snmp_info_default("input.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.16.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadStatusLoad.1 = Gauge32: 0 */
	snmp_info_default("input.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceLinetoLineVoltage.0 = INTEGER: 120 */
	snmp_info_default("input.voltage.nominal", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.15.0", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),

	/* Outlets */
	snmp_info_default("outlet.count", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("outlet.%i.id", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.%i", "%i",
		SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET, NULL),
	/* sPDUOutletCtlName.%i = STRING: "Testing Name" */
	snmp_info_default("outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.4.2.1.4.%i", NULL,
		SU_FLAG_STALE | SU_FLAG_OK | SU_OUTLET, NULL),
	/* sPDUOutletCtl.1 = INTEGER: outletOn(1) */
	snmp_info_default("outlet.%i.status", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.4.2.1.3.%i", NULL,
		SU_FLAG_OK | SU_OUTLET, &apc_pdu_sw_outlet_status_info[0]),
	/* Also use this OID to determine switchability ; its presence means "yes" */
	/* sPDUOutletCtl.1 = INTEGER: outletOn(1) */
	snmp_info_default("outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.4.2.1.3.%i", "yes",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, &apc_pdu_sw_outlet_switchability_info[0]),

#if WITH_UNMAPPED_DATA_POINTS /* keep following scan for future development */
	/* sPDUMasterControlSwitch.0 = INTEGER: noCommand(6) */
	snmp_info_default("unmapped.sPDUMasterControlSwitch", 0, 1, ".1.3.6.1.4.1.318.1.1.4.2.1.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUMasterState.0 = STRING: "On  On  On  On  On  On  On  On  " */
	snmp_info_default("unmapped.sPDUMasterState", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.2.2.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUMasterPending.0 = STRING: "No  No  No  No  No  No  No  No  " */
	snmp_info_default("unmapped.sPDUMasterPending", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.2.3.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUMasterConfigPowerOn.0 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUMasterConfigPowerOn", 0, 1, ".1.3.6.1.4.1.318.1.1.4.3.1.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUMasterConfigReboot.0 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUMasterConfigReboot", 0, 1, ".1.3.6.1.4.1.318.1.1.4.3.2.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUMasterConfigPDUName.0 = STRING: "RackPDU" */
	snmp_info_default("unmapped.sPDUMasterConfigPDUName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.3.3.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlTableSize.0 = INTEGER: 8 */

	/* sPDUOutletControlIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlIndex.2 = INTEGER: 2 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlIndex.3 = INTEGER: 3 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlIndex.4 = INTEGER: 4 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlIndex.5 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlIndex.6 = INTEGER: 6 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlIndex.7 = INTEGER: 7 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletControlIndex.8 = INTEGER: 8 */
	snmp_info_default("unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.1 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.2 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.3 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.4 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.5 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.6 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.7 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPending.8 = INTEGER: noCommandPending(2) */
	snmp_info_default("unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigTableSize.0 = INTEGER: 8 */
	snmp_info_default("unmapped.sPDUOutletConfigTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.1.0", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.2 = INTEGER: 2 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.3 = INTEGER: 3 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.4 = INTEGER: 4 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.5 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.6 = INTEGER: 6 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.7 = INTEGER: 7 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletConfigIndex.8 = INTEGER: 8 */
	snmp_info_default("unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.1 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.2 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.3 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.4 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.5 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.6 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.7 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOnTime.8 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.1 = STRING: "Testing Name" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.2 = STRING: "Testing 2" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.3 = STRING: "Outlet 3" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.3", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.4 = STRING: "Outlet 4" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.4", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.5 = STRING: "Outlet 5" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.5", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.6 = STRING: "Outlet 6" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.6", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.7 = STRING: "Outlet 7" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.7", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletName.8 = STRING: "Outlet 8" */
	snmp_info_default("unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.8", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.1 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.2 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.3 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.3", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.4 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.4", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.5 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.5", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.6 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.6", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.7 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.7", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletPowerOffTime.8 = INTEGER: 0 */
	snmp_info_default("unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.8", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.1 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.2 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.2", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.3 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.3", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.4 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.4", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.5 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.5", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.6 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.6", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.7 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.7", NULL, SU_FLAG_OK, NULL),
	/* sPDUOutletRebootDuration.8 = INTEGER: 5 */
	snmp_info_default("unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentName.0 = STRING: "RackPDU" */
	snmp_info_default("unmapped.rPDUIdentName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentHardwareRev.0 = STRING: "B2" */
	snmp_info_default("unmapped.rPDUIdentHardwareRev", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentFirmwareRev.0 = STRING: "v3.7.3" */
	snmp_info_default("unmapped.rPDUIdentFirmwareRev", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDateOfManufacture.0 = STRING: "08/13/2012" */
	snmp_info_default("unmapped.rPDUIdentDateOfManufacture", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.4.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentModelNumber.0 = STRING: "AP7900" */
	snmp_info_default("unmapped.rPDUIdentModelNumber", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.5.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentSerialNumber.0 = STRING: "5A1234E00874" */
	snmp_info_default("unmapped.rPDUIdentSerialNumber", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.6.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceRating.0 = INTEGER: 12 */
	snmp_info_default("unmapped.rPDUIdentDeviceRating", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.7.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceNumOutlets.0 = INTEGER: 8 */
	snmp_info_default("unmapped.rPDUIdentDeviceNumOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.8.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceNumPhases.0 = INTEGER: 1 */
	snmp_info_default("input.phases", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.9.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceNumBreakers.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUIdentDeviceNumBreakers", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.10.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceBreakerRating.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUIdentDeviceBreakerRating", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.11.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceOrientation.0 = INTEGER: orientHorizontal(1) */
	snmp_info_default("unmapped.rPDUIdentDeviceOrientation", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.12.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceOutletLayout.0 = INTEGER: seqPhaseToNeutral(1) */
	snmp_info_default("unmapped.rPDUIdentDeviceOutletLayout", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.13.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceDisplayOrientation.0 = INTEGER: displayNormal(1) */
	snmp_info_default("unmapped.rPDUIdentDeviceDisplayOrientation", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.14.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDeviceLinetoLineVoltage.0 = INTEGER: 120 */
	snmp_info_default("unmapped.rPDUIdentDeviceLinetoLineVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.15.0", NULL, SU_FLAG_OK, NULL),

	/* rPDUIdentDevicePowerFactor.0 = INTEGER: 1000 */
	snmp_info_default("unmapped.rPDUIdentDevicePowerFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.17.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUIdentDevicePowerVA.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUIdentDevicePowerVA", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.18.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadDevMaxPhaseLoad.0 = INTEGER: 12 */
	snmp_info_default("unmapped.rPDULoadDevMaxPhaseLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadDevNumPhases.0 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDULoadDevNumPhases", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadDevMaxBankLoad.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDULoadDevMaxBankLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadDevNumBanks.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDULoadDevNumBanks", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.4.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadDevBankTableSize.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDULoadDevBankTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.5.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadDevMaxOutletTableSize.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDULoadDevMaxOutletTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.7.0", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadPhaseConfigIndex.phase1 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDULoadPhaseConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadPhaseConfigLowLoadThreshold.phase1 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDULoadPhaseConfigLowLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadPhaseConfigNearOverloadThreshold.phase1 = INTEGER: 8 */
	snmp_info_default("unmapped.rPDULoadPhaseConfigNearOverloadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadPhaseConfigOverloadThreshold.phase1 = INTEGER: 12 */
	snmp_info_default("unmapped.rPDULoadPhaseConfigOverloadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadPhaseConfigAlarm.phase1 = INTEGER: noLoadAlarm(1) */
	snmp_info_default("unmapped.rPDULoadPhaseConfigAlarm", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadStatusIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDULoadStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadStatusLoadState.1 = INTEGER: phaseLoadNormal(1) */
	snmp_info_default("unmapped.rPDULoadStatusLoadState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadStatusPhaseNumber.1 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDULoadStatusPhaseNumber", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* rPDULoadStatusBankNumber.1 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDULoadStatusBankNumber", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletDevCommand.0 = INTEGER: noCommandAll(1) */
	snmp_info_default("unmapped.rPDUOutletDevCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletDevColdstartDelay.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletDevColdstartDelay", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletDevNumCntrlOutlets.0 = INTEGER: 8 */
	snmp_info_default("unmapped.rPDUOutletDevNumCntrlOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletDevNumTotalOutlets.0 = INTEGER: 8 */
	snmp_info_default("unmapped.rPDUOutletDevNumTotalOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.4.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletDevMonitoredOutlets.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletDevMonitoredOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.5.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletPhaseIndex.phase1 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletPhaseIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletPhaseOverloadRestriction.phase1 = INTEGER: alwaysAllowTurnON(1) */
	snmp_info_default("unmapped.rPDUOutletPhaseOverloadRestriction", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.2.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.2 = INTEGER: 2 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.3 = INTEGER: 3 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.4 = INTEGER: 4 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.5 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.6 = INTEGER: 6 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.7 = INTEGER: 7 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlIndex.8 = INTEGER: 8 */
	snmp_info_default("unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.1 = STRING: "Testing Name" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.2 = STRING: "Testing 2" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.3 = STRING: "Outlet 3" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.4 = STRING: "Outlet 4" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.5 = STRING: "Outlet 5" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.6 = STRING: "Outlet 6" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.7 = STRING: "Outlet 7" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletName.8 = STRING: "Outlet 8" */
	snmp_info_default("unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.1 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.2 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.3 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.4 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.5 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.6 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.7 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletPhase.8 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.1 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.2 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.3 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.4 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.5 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.6 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.7 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletCommand.8 = INTEGER: immediateOn(1) */
	snmp_info_default("unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.1 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.2 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.3 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.4 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.5 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.6 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.7 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletControlOutletBank.8 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.2 = INTEGER: 2 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.3 = INTEGER: 3 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.4 = INTEGER: 4 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.5 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.6 = INTEGER: 6 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.7 = INTEGER: 7 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigIndex.8 = INTEGER: 8 */
	snmp_info_default("unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.1 = STRING: "Testing Name" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.2 = STRING: "Testing 2" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.3 = STRING: "Outlet 3" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.4 = STRING: "Outlet 4" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.5 = STRING: "Outlet 5" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.6 = STRING: "Outlet 6" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.7 = STRING: "Outlet 7" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletName.8 = STRING: "Outlet 8" */
	snmp_info_default("unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.1 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.2 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.3 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.4 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.5 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.6 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.7 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletPhase.8 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.1 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.2 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.3 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.4 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.5 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.6 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.7 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOnTime.8 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.1 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.2 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.3 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.4 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.5 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.6 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.7 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigPowerOffTime.8 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.1 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.2 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.3 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.4 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.5 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.6 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.7 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigRebootDuration.8 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.1 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.2 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.3 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.4 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.5 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.6 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.7 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigOutletBank.8 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletConfigMonitoredTableSize.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletConfigMonitoredTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.2.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.2 = INTEGER: 2 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.3 = INTEGER: 3 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.4 = INTEGER: 4 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.5 = INTEGER: 5 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.6 = INTEGER: 6 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.7 = INTEGER: 7 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusIndex.8 = INTEGER: 8 */
	snmp_info_default("unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.1 = STRING: "Testing Name" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.2 = STRING: "Testing 2" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.3 = STRING: "Outlet 3" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.4 = STRING: "Outlet 4" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.5 = STRING: "Outlet 5" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.6 = STRING: "Outlet 6" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.7 = STRING: "Outlet 7" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletName.8 = STRING: "Outlet 8" */
	snmp_info_default("unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.1 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.2 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.3 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.4 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.5 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.6 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.7 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletPhase.8 = INTEGER: phase1(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.1 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.2 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.3 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.4 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.5 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.6 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.7 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletState.8 = INTEGER: outletStatusOn(1) */
	snmp_info_default("unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.1 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.2 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.3 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.4 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.5 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.6 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.7 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusCommandPending.8 = INTEGER: outletStatusNoCommandPending(2) */
	snmp_info_default("unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.1 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.2 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.3 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.4 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.5 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.6 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.7 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusOutletBank.8 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.1 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.2 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.2", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.3 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.3", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.4 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.4", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.5 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.5", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.6 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.6", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.7 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.7", NULL, SU_FLAG_OK, NULL),
	/* rPDUOutletStatusLoad.8 = Gauge32: 0 */
	snmp_info_default("unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.8", NULL, SU_FLAG_OK, NULL),
	/* rPDUPowerSupply1Status.0 = INTEGER: powerSupplyOneOk(1) */
	snmp_info_default("unmapped.rPDUPowerSupply1Status", 0, 1, ".1.3.6.1.4.1.318.1.1.12.4.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUPowerSupply2Status.0 = INTEGER: powerSupplyTwoOk(1) */
	snmp_info_default("unmapped.rPDUPowerSupply2Status", 0, 1, ".1.3.6.1.4.1.318.1.1.12.4.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUPowerSupplyAlarm.0 = INTEGER: allAvailablePowerSuppliesOK(1) */
	snmp_info_default("unmapped.rPDUPowerSupplyAlarm", 0, 1, ".1.3.6.1.4.1.318.1.1.12.4.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUStatusBankTableSize.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUStatusBankTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.1.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUStatusPhaseTableSize.0 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDUStatusPhaseTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.3.0", NULL, SU_FLAG_OK, NULL),
	/* rPDUStatusPhaseIndex.1 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDUStatusPhaseIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.4.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUStatusPhaseNumber.1 = INTEGER: 1 */
	snmp_info_default("unmapped.rPDUStatusPhaseNumber", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.4.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUStatusPhaseState.1 = INTEGER: phaseLoadNormal(1) */
	snmp_info_default("unmapped.rPDUStatusPhaseState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.4.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* rPDUStatusOutletTableSize.0 = INTEGER: 0 */
	snmp_info_default("unmapped.rPDUStatusOutletTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.5.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.1.0 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.1.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.2.1 = STRING: "Rack PDU_ISX" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.3.1 = STRING: "5A1234E00874" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.4.1 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.5.1 = STRING: "Rack PDU" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.6.1 = STRING: "1" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.7.1 = STRING: "Unknown" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.8.1 = INTEGER: 255 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.9.1 = STRING: "RackPDU" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.10.1 = INTEGER: 255 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.11.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.11.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.12.1 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.12.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.13.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.13.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.2.1.14.1 = STRING: "SB-1" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.14.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.3.0 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.3.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.4.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.4.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.2.1 = STRING: "5A1234E00874" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.2.2 = STRING: "5A1234E00874" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.3.1 = STRING: "apc_hw02_aos_373.bin" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.3.2 = STRING: "apc_hw02_rpdu_373.bin" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.4.1 = STRING: "v3.7.3" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.4.1.4.2 = STRING: "v3.7.3" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.5.0 = INTEGER: 19 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.5.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.3 = INTEGER: 3 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.4 = INTEGER: 4 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.5 = INTEGER: 5 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.6 = INTEGER: 6 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.7 = INTEGER: 7 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.8 = INTEGER: 8 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.9 = INTEGER: 9 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.10 = INTEGER: 10 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.11 = INTEGER: 11 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.11", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.12 = INTEGER: 12 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.12", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.13 = INTEGER: 13 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.13", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.14 = INTEGER: 14 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.14", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.15 = INTEGER: 15 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.15", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.16 = INTEGER: 16 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.16", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.17 = INTEGER: 17 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.17", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.18 = INTEGER: 18 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.18", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.1.19 = INTEGER: 19 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.19", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.2 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.3 = INTEGER: 3 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.4 = INTEGER: 4 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.5 = INTEGER: 5 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.6 = INTEGER: 6 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.7 = INTEGER: 7 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.8 = INTEGER: 8 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.9 = INTEGER: 9 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.9", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.10 = INTEGER: 10 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.10", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.11 = INTEGER: 11 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.11", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.12 = INTEGER: 12 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.12", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.13 = INTEGER: 13 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.13", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.14 = INTEGER: 14 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.14", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.15 = INTEGER: 15 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.15", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.16 = INTEGER: 16 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.16", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.17 = INTEGER: 17 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.17", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.18 = INTEGER: 18 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.18", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.2.19 = INTEGER: 19 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.19", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.1 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.2 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.3 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.3", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.4 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.4", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.5 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.5", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.6 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.6", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.7 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.7", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.8 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.8", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.9 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.9", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.10 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.10", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.11 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.11", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.12 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.12", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.13 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.13", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.14 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.14", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.15 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.15", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.16 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.16", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.17 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.17", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.18 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.18", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.3.19 = STRING: "0" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.19", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.1 = STRING: "MTYx" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.2 = STRING: "MjM=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.3 = STRING: "ODA=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.3", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.4 = STRING: "OTk1MA==" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.4", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.5 = STRING: "OTk1MA==" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.5", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.6 = STRING: "NDQz" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.6", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.7 = STRING: "MjI=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.7", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.8 = STRING: "MjI=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.8", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.9 = STRING: "MTIz" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.9", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.10 = STRING: "MjU=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.10", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.11 = STRING: "MjE=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.11", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.12 = STRING: "MjE=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.12", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.13 = STRING: "Njg=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.13", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.14 = STRING: "NTQ2" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.14", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.15 = STRING: "MTgxMg==" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.15", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.16 = STRING: "MTYx" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.16", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.17 = STRING: "MjE=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.17", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.18 = STRING: "MjE=" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.18", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.4.19 = STRING: "OTk1MQ==" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.19", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.1 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.2 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.3 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.3", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.4 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.4", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.5 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.5", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.6 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.6", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.7 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.7", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.8 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.8", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.9 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.9", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.10 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.10", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.11 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.11", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.12 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.12", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.13 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.13", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.14 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.14", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.15 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.15", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.16 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.16", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.17 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.17", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.18 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.18", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.6.1.5.19 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.19", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.7.0 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.7.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.8.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.8.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.8.1.2.1 = STRING: "power" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.8.1.3.1 = STRING: "pdu" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.8.1.4.1 = STRING: "rpdu" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.8.1.5.1 = STRING: "version7" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.8.1.6.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.8.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.9.0 = INTEGER: 0 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.9.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.10.0 = INTEGER: 0 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.10.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.2.12.0 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.12.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.4.1.0 = INTEGER: 0 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.4.1.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.4.3.0 = STRING: "<Host Name or IP>,<1 digit type identifier>" */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.4.3.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.4.4.0 = INTEGER: 0 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.4.4.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.1.0 = INTEGER: 12 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.1.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.3 = INTEGER: 3 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.4 = INTEGER: 4 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.5 = INTEGER: 5 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.6 = INTEGER: 6 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.7 = INTEGER: 7 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.8 = INTEGER: 8 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.9 = INTEGER: 9 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.10 = INTEGER: 10 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.11 = INTEGER: 11 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.11", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.1.12 = INTEGER: 12 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.12", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.1 = INTEGER: 3841 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.2 = INTEGER: 3843 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.3 = INTEGER: 3845 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.4 = INTEGER: 3848 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.5 = INTEGER: 3862 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.6 = INTEGER: 3864 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.7 = INTEGER: 3856 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.8 = INTEGER: 3858 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.9 = INTEGER: 3860 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.9", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.10 = INTEGER: 3871 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.10", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.11 = INTEGER: 3873 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.11", NULL, SU_FLAG_OK, NULL),
	/* experimental.5.2.1.2.12 = INTEGER: 3875 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.12", NULL, SU_FLAG_OK, NULL),
	/* experimental.6.1.1.0 = Hex-STRING: 07 00 00 00  */
	snmp_info_default("unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.6.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.7.1.0 = INTEGER: 4 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.1.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.7.3.0 = INTEGER: 4 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.3.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.7.4.0 = INTEGER: 3 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.4.0", NULL, SU_FLAG_OK, NULL),
	/* experimental.7.5.0 = INTEGER: 3 */
	snmp_info_default("unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.5.0", NULL, SU_FLAG_OK, NULL),
#endif	/* if WITH_UNMAPPED_DATA_POINTS */


	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t apc_pdu_rpdu = { "apc_pdu", APC_PDU_MIB_VERSION, NULL, APC_PDU_DEVICE_MODEL, apc_pdu_mib, APC_PDU_MIB_SYSOID_RPDU, NULL };
mib2nut_info_t apc_pdu_rpdu2 = { "apc_pdu", APC_PDU_MIB_VERSION, NULL, APC_PDU_DEVICE_MODEL, apc_pdu_mib, APC_PDU_MIB_SYSOID_RPDU2, NULL };
mib2nut_info_t apc_pdu_msp = { "apc_pdu", APC_PDU_MIB_VERSION, NULL, APC_PDU_DEVICE_MODEL, apc_pdu_mib, APC_PDU_MIB_SYSOID_MSP, NULL };
