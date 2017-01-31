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

#define APC_PDU_MIB_VERSION  "0.2"

#define APC_PDU_MIB_SYSOID_RPDU      ".1.3.6.1.4.1.318.1.3.4.4"
#define APC_PDU_MIB_SYSOID_RPDU2     ".1.3.6.1.4.1.318.1.3.4.5"
#define APC_PDU_MIB_SYSOID_MSP       ".1.3.6.1.4.1.318.1.3.4.6"


static info_lkp_t apc_pdu_sw_outlet_status_info[] = {
	{ 1, "on" },
	{ 2, "off" },
	{ 0, NULL }
};

static info_lkp_t apc_pdu_sw_outlet_switchability_info[] = {
	{ 1, "yes" },
	{ 2, "yes" },
	{ 0, NULL }
};

/* POWERNET-MIB Snmp2NUT lookup table */
static snmp_info_t apc_pdu_mib[] = {

	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "APC",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	/* sPDUIdentModelNumber.0 = STRING: "AP7900" */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.4.0",
		"Switched ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },
	{ "device.contact", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL,
		SU_FLAG_STALE | SU_FLAG_OK, NULL, NULL },
	{ "device.description", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.5.0", NULL,
		SU_FLAG_STALE | SU_FLAG_OK, NULL, NULL },
	{ "device.location", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL,
		SU_FLAG_STALE | SU_FLAG_OK, NULL, NULL },
	/* FIXME: to be RFC'ed */
	{ "device.uptime", 0, 1, ".1.3.6.1.2.1.1.3.0", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL, NULL },
	/* sPDUIdentSerialNumber.0 = STRING: "5A1234E00874" */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	/* sPDUIdentModelNumber.0 = STRING: "AP7900" */
	{ "device.part", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.4.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	/* sPDUIdentHardwareRev.0 = STRING: "B2" */
	{ "device.version", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },

	/* sPDUIdentFirmwareRev.0 = STRING: "v3.7.3" */
	/* FIXME: to be moved to device.firmware */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUIdentDateOfManufacture.0 = STRING: "08/13/2012" */
	/* FIXME: to be moved to the device collection! */
	{ "ups.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.1.3.0", NULL, SU_FLAG_OK, NULL, NULL },

	/* Input */
	/* rPDUIdentDevicePowerWatts.0 = INTEGER: 0 */
	{ "input.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.16.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadStatusLoad.1 = Gauge32: 0 */
	{ "input.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceLinetoLineVoltage.0 = INTEGER: 120 */
	{ "input.voltage.nominal", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.15.0", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL, NULL },

	/* Outlets */
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	{ "outlet.%i.id", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.%i", "%i",
		SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET, NULL, NULL },
	/* sPDUOutletCtlName.%i = STRING: "Testing Name" */
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.4.2.1.4.%i", NULL,
		SU_FLAG_STALE | SU_FLAG_OK | SU_OUTLET, NULL, NULL },
	/* sPDUOutletCtl.1 = INTEGER: outletOn(1) */
	{ "outlet.%i.status", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.4.2.1.3.%i", NULL,
		SU_FLAG_OK | SU_OUTLET, &apc_pdu_sw_outlet_status_info[0], NULL },
	/* Also use this OID to determine switchability ; its presence means "yes" */
	/* sPDUOutletCtl.1 = INTEGER: outletOn(1) */
	{ "outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.4.2.1.3.%i", "yes",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, &apc_pdu_sw_outlet_switchability_info[0], NULL },



#if 0 /* keep following scan for future development */

	/* sPDUMasterControlSwitch.0 = INTEGER: noCommand(6) */
	{ "unmapped.sPDUMasterControlSwitch", 0, 1, ".1.3.6.1.4.1.318.1.1.4.2.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUMasterState.0 = STRING: "On  On  On  On  On  On  On  On  " */
	{ "unmapped.sPDUMasterState", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.2.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUMasterPending.0 = STRING: "No  No  No  No  No  No  No  No  " */
	{ "unmapped.sPDUMasterPending", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.2.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUMasterConfigPowerOn.0 = INTEGER: 0 */
	{ "unmapped.sPDUMasterConfigPowerOn", 0, 1, ".1.3.6.1.4.1.318.1.1.4.3.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUMasterConfigReboot.0 = INTEGER: 0 */
	{ "unmapped.sPDUMasterConfigReboot", 0, 1, ".1.3.6.1.4.1.318.1.1.4.3.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUMasterConfigPDUName.0 = STRING: "RackPDU" */
	{ "unmapped.sPDUMasterConfigPDUName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.3.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlTableSize.0 = INTEGER: 8 */

	/* sPDUOutletControlIndex.1 = INTEGER: 1 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlIndex.2 = INTEGER: 2 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlIndex.3 = INTEGER: 3 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlIndex.4 = INTEGER: 4 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlIndex.5 = INTEGER: 5 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlIndex.6 = INTEGER: 6 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlIndex.7 = INTEGER: 7 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletControlIndex.8 = INTEGER: 8 */
	{ "unmapped.sPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.1.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.1 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.2 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.3 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.4 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.5 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.6 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.7 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPending.8 = INTEGER: noCommandPending(2) */
	{ "unmapped.sPDUOutletPending", 0, 1, ".1.3.6.1.4.1.318.1.1.4.4.2.1.2.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigTableSize.0 = INTEGER: 8 */
	{ "unmapped.sPDUOutletConfigTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.1 = INTEGER: 1 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.2 = INTEGER: 2 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.3 = INTEGER: 3 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.4 = INTEGER: 4 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.5 = INTEGER: 5 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.6 = INTEGER: 6 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.7 = INTEGER: 7 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletConfigIndex.8 = INTEGER: 8 */
	{ "unmapped.sPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.1.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.1 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.2 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.3 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.4 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.5 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.6 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.7 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOnTime.8 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.2.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.1 = STRING: "Testing Name" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.2 = STRING: "Testing 2" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.3 = STRING: "Outlet 3" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.4 = STRING: "Outlet 4" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.5 = STRING: "Outlet 5" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.6 = STRING: "Outlet 6" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.7 = STRING: "Outlet 7" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletName.8 = STRING: "Outlet 8" */
	{ "unmapped.sPDUOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.4.5.2.1.3.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.1 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.2 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.3 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.4 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.5 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.6 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.7 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletPowerOffTime.8 = INTEGER: 0 */
	{ "unmapped.sPDUOutletPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.4.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.1 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.2 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.3 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.4 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.5 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.6 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.7 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* sPDUOutletRebootDuration.8 = INTEGER: 5 */
	{ "unmapped.sPDUOutletRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.4.5.2.1.5.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentName.0 = STRING: "RackPDU" */
	{ "unmapped.rPDUIdentName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentHardwareRev.0 = STRING: "B2" */
	{ "unmapped.rPDUIdentHardwareRev", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentFirmwareRev.0 = STRING: "v3.7.3" */
	{ "unmapped.rPDUIdentFirmwareRev", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDateOfManufacture.0 = STRING: "08/13/2012" */
	{ "unmapped.rPDUIdentDateOfManufacture", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.4.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentModelNumber.0 = STRING: "AP7900" */
	{ "unmapped.rPDUIdentModelNumber", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.5.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentSerialNumber.0 = STRING: "5A1234E00874" */
	{ "unmapped.rPDUIdentSerialNumber", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.1.6.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceRating.0 = INTEGER: 12 */
	{ "unmapped.rPDUIdentDeviceRating", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.7.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceNumOutlets.0 = INTEGER: 8 */
	{ "unmapped.rPDUIdentDeviceNumOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.8.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceNumPhases.0 = INTEGER: 1 */
	{ "input.phases", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.9.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceNumBreakers.0 = INTEGER: 0 */
	{ "unmapped.rPDUIdentDeviceNumBreakers", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.10.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceBreakerRating.0 = INTEGER: 0 */
	{ "unmapped.rPDUIdentDeviceBreakerRating", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.11.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceOrientation.0 = INTEGER: orientHorizontal(1) */
	{ "unmapped.rPDUIdentDeviceOrientation", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.12.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceOutletLayout.0 = INTEGER: seqPhaseToNeutral(1) */
	{ "unmapped.rPDUIdentDeviceOutletLayout", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.13.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceDisplayOrientation.0 = INTEGER: displayNormal(1) */
	{ "unmapped.rPDUIdentDeviceDisplayOrientation", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.14.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDeviceLinetoLineVoltage.0 = INTEGER: 120 */
	{ "unmapped.rPDUIdentDeviceLinetoLineVoltage", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.15.0", NULL, SU_FLAG_OK, NULL, NULL },

	/* rPDUIdentDevicePowerFactor.0 = INTEGER: 1000 */
	{ "unmapped.rPDUIdentDevicePowerFactor", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.17.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUIdentDevicePowerVA.0 = INTEGER: 0 */
	{ "unmapped.rPDUIdentDevicePowerVA", 0, 1, ".1.3.6.1.4.1.318.1.1.12.1.18.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadDevMaxPhaseLoad.0 = INTEGER: 12 */
	{ "unmapped.rPDULoadDevMaxPhaseLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadDevNumPhases.0 = INTEGER: 1 */
	{ "unmapped.rPDULoadDevNumPhases", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadDevMaxBankLoad.0 = INTEGER: 0 */
	{ "unmapped.rPDULoadDevMaxBankLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadDevNumBanks.0 = INTEGER: 0 */
	{ "unmapped.rPDULoadDevNumBanks", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.4.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadDevBankTableSize.0 = INTEGER: 0 */
	{ "unmapped.rPDULoadDevBankTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.5.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadDevMaxOutletTableSize.0 = INTEGER: 0 */
	{ "unmapped.rPDULoadDevMaxOutletTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.1.7.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadPhaseConfigIndex.phase1 = INTEGER: phase1(1) */
	{ "unmapped.rPDULoadPhaseConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadPhaseConfigLowLoadThreshold.phase1 = INTEGER: 0 */
	{ "unmapped.rPDULoadPhaseConfigLowLoadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadPhaseConfigNearOverloadThreshold.phase1 = INTEGER: 8 */
	{ "unmapped.rPDULoadPhaseConfigNearOverloadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadPhaseConfigOverloadThreshold.phase1 = INTEGER: 12 */
	{ "unmapped.rPDULoadPhaseConfigOverloadThreshold", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadPhaseConfigAlarm.phase1 = INTEGER: noLoadAlarm(1) */
	{ "unmapped.rPDULoadPhaseConfigAlarm", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.2.1.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadStatusIndex.1 = INTEGER: 1 */
	{ "unmapped.rPDULoadStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadStatusLoadState.1 = INTEGER: phaseLoadNormal(1) */
	{ "unmapped.rPDULoadStatusLoadState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadStatusPhaseNumber.1 = INTEGER: 1 */
	{ "unmapped.rPDULoadStatusPhaseNumber", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDULoadStatusBankNumber.1 = INTEGER: 0 */
	{ "unmapped.rPDULoadStatusBankNumber", 0, 1, ".1.3.6.1.4.1.318.1.1.12.2.3.1.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletDevCommand.0 = INTEGER: noCommandAll(1) */
	{ "unmapped.rPDUOutletDevCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletDevColdstartDelay.0 = INTEGER: 0 */
	{ "unmapped.rPDUOutletDevColdstartDelay", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletDevNumCntrlOutlets.0 = INTEGER: 8 */
	{ "unmapped.rPDUOutletDevNumCntrlOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletDevNumTotalOutlets.0 = INTEGER: 8 */
	{ "unmapped.rPDUOutletDevNumTotalOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.4.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletDevMonitoredOutlets.0 = INTEGER: 0 */
	{ "unmapped.rPDUOutletDevMonitoredOutlets", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.1.5.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletPhaseIndex.phase1 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletPhaseIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.2.1.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletPhaseOverloadRestriction.phase1 = INTEGER: alwaysAllowTurnON(1) */
	{ "unmapped.rPDUOutletPhaseOverloadRestriction", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.2.1.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.1 = INTEGER: 1 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.2 = INTEGER: 2 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.3 = INTEGER: 3 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.4 = INTEGER: 4 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.5 = INTEGER: 5 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.6 = INTEGER: 6 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.7 = INTEGER: 7 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlIndex.8 = INTEGER: 8 */
	{ "unmapped.rPDUOutletControlIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.1.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.1 = STRING: "Testing Name" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.2 = STRING: "Testing 2" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.3 = STRING: "Outlet 3" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.4 = STRING: "Outlet 4" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.5 = STRING: "Outlet 5" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.6 = STRING: "Outlet 6" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.7 = STRING: "Outlet 7" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletName.8 = STRING: "Outlet 8" */
	{ "unmapped.rPDUOutletControlOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.2.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.1 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.2 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.3 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.4 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.5 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.6 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.7 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletPhase.8 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletControlOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.3.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.1 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.2 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.3 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.4 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.5 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.6 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.7 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletCommand.8 = INTEGER: immediateOn(1) */
	{ "unmapped.rPDUOutletControlOutletCommand", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.4.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.1 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.2 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.3 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.4 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.5 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.6 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.7 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletControlOutletBank.8 = INTEGER: 0 */
	{ "unmapped.rPDUOutletControlOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.3.1.1.5.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.1 = INTEGER: 1 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.2 = INTEGER: 2 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.3 = INTEGER: 3 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.4 = INTEGER: 4 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.5 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.6 = INTEGER: 6 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.7 = INTEGER: 7 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigIndex.8 = INTEGER: 8 */
	{ "unmapped.rPDUOutletConfigIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.1.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.1 = STRING: "Testing Name" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.2 = STRING: "Testing 2" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.3 = STRING: "Outlet 3" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.4 = STRING: "Outlet 4" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.5 = STRING: "Outlet 5" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.6 = STRING: "Outlet 6" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.7 = STRING: "Outlet 7" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletName.8 = STRING: "Outlet 8" */
	{ "unmapped.rPDUOutletConfigOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.2.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.1 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.2 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.3 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.4 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.5 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.6 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.7 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletPhase.8 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletConfigOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.3.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.1 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.2 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.3 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.4 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.5 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.6 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.7 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOnTime.8 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOnTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.4.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.1 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.2 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.3 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.4 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.5 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.6 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.7 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigPowerOffTime.8 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigPowerOffTime", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.5.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.1 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.2 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.3 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.4 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.5 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.6 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.7 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigRebootDuration.8 = INTEGER: 5 */
	{ "unmapped.rPDUOutletConfigRebootDuration", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.6.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.1 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.2 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.3 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.4 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.5 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.6 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.7 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigOutletBank.8 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.1.1.7.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletConfigMonitoredTableSize.0 = INTEGER: 0 */
	{ "unmapped.rPDUOutletConfigMonitoredTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.4.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.1 = INTEGER: 1 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.2 = INTEGER: 2 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.3 = INTEGER: 3 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.4 = INTEGER: 4 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.5 = INTEGER: 5 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.6 = INTEGER: 6 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.7 = INTEGER: 7 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusIndex.8 = INTEGER: 8 */
	{ "unmapped.rPDUOutletStatusIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.1.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.1 = STRING: "Testing Name" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.2 = STRING: "Testing 2" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.3 = STRING: "Outlet 3" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.4 = STRING: "Outlet 4" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.5 = STRING: "Outlet 5" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.6 = STRING: "Outlet 6" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.7 = STRING: "Outlet 7" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletName.8 = STRING: "Outlet 8" */
	{ "unmapped.rPDUOutletStatusOutletName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.2.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.1 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.2 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.3 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.4 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.5 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.6 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.7 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletPhase.8 = INTEGER: phase1(1) */
	{ "unmapped.rPDUOutletStatusOutletPhase", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.3.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.1 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.2 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.3 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.4 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.5 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.6 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.7 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletState.8 = INTEGER: outletStatusOn(1) */
	{ "unmapped.rPDUOutletStatusOutletState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.4.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.1 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.2 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.3 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.4 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.5 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.6 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.7 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusCommandPending.8 = INTEGER: outletStatusNoCommandPending(2) */
	{ "unmapped.rPDUOutletStatusCommandPending", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.5.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.1 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.2 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.3 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.4 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.5 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.6 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.7 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusOutletBank.8 = INTEGER: 0 */
	{ "unmapped.rPDUOutletStatusOutletBank", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.6.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.1 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.2 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.3 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.4 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.5 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.6 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.7 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUOutletStatusLoad.8 = Gauge32: 0 */
	{ "unmapped.rPDUOutletStatusLoad", 0, 1, ".1.3.6.1.4.1.318.1.1.12.3.5.1.1.7.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUPowerSupply1Status.0 = INTEGER: powerSupplyOneOk(1) */
	{ "unmapped.rPDUPowerSupply1Status", 0, 1, ".1.3.6.1.4.1.318.1.1.12.4.1.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUPowerSupply2Status.0 = INTEGER: powerSupplyTwoOk(1) */
	{ "unmapped.rPDUPowerSupply2Status", 0, 1, ".1.3.6.1.4.1.318.1.1.12.4.1.2.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUPowerSupplyAlarm.0 = INTEGER: allAvailablePowerSuppliesOK(1) */
	{ "unmapped.rPDUPowerSupplyAlarm", 0, 1, ".1.3.6.1.4.1.318.1.1.12.4.1.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUStatusBankTableSize.0 = INTEGER: 0 */
	{ "unmapped.rPDUStatusBankTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUStatusPhaseTableSize.0 = INTEGER: 1 */
	{ "unmapped.rPDUStatusPhaseTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUStatusPhaseIndex.1 = INTEGER: 1 */
	{ "unmapped.rPDUStatusPhaseIndex", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.4.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUStatusPhaseNumber.1 = INTEGER: 1 */
	{ "unmapped.rPDUStatusPhaseNumber", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.4.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUStatusPhaseState.1 = INTEGER: phaseLoadNormal(1) */
	{ "unmapped.rPDUStatusPhaseState", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.4.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* rPDUStatusOutletTableSize.0 = INTEGER: 0 */
	{ "unmapped.rPDUStatusOutletTableSize", 0, 1, ".1.3.6.1.4.1.318.1.1.12.5.5.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.1.0 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.1.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.2.1 = STRING: "Rack PDU_ISX" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.3.1 = STRING: "5A1234E00874" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.4.1 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.5.1 = STRING: "Rack PDU" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.6.1 = STRING: "1" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.6.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.7.1 = STRING: "Unknown" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.7.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.8.1 = INTEGER: 255 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.8.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.9.1 = STRING: "RackPDU" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.9.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.10.1 = INTEGER: 255 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.10.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.11.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.11.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.12.1 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.12.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.13.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.2.1.13.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.2.1.14.1 = STRING: "SB-1" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.2.1.14.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.3.0 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.1.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.4.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.1.2 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.4.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.2.1 = STRING: "5A1234E00874" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.2.2 = STRING: "5A1234E00874" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.3.1 = STRING: "apc_hw02_aos_373.bin" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.3.2 = STRING: "apc_hw02_rpdu_373.bin" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.3.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.4.1 = STRING: "v3.7.3" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.4.1.4.2 = STRING: "v3.7.3" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.4.1.4.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.5.0 = INTEGER: 19 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.5.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.2 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.3 = INTEGER: 3 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.4 = INTEGER: 4 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.5 = INTEGER: 5 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.6 = INTEGER: 6 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.7 = INTEGER: 7 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.8 = INTEGER: 8 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.9 = INTEGER: 9 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.9", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.10 = INTEGER: 10 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.10", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.11 = INTEGER: 11 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.11", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.12 = INTEGER: 12 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.12", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.13 = INTEGER: 13 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.13", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.14 = INTEGER: 14 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.14", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.15 = INTEGER: 15 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.15", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.16 = INTEGER: 16 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.16", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.17 = INTEGER: 17 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.17", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.18 = INTEGER: 18 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.18", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.1.19 = INTEGER: 19 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.1.19", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.2 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.3 = INTEGER: 3 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.4 = INTEGER: 4 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.5 = INTEGER: 5 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.6 = INTEGER: 6 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.7 = INTEGER: 7 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.8 = INTEGER: 8 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.9 = INTEGER: 9 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.9", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.10 = INTEGER: 10 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.10", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.11 = INTEGER: 11 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.11", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.12 = INTEGER: 12 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.12", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.13 = INTEGER: 13 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.13", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.14 = INTEGER: 14 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.14", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.15 = INTEGER: 15 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.15", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.16 = INTEGER: 16 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.16", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.17 = INTEGER: 17 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.17", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.18 = INTEGER: 18 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.18", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.2.19 = INTEGER: 19 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.2.19", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.1 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.2 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.3 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.4 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.5 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.6 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.7 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.8 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.9 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.9", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.10 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.10", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.11 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.11", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.12 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.12", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.13 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.13", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.14 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.14", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.15 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.15", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.16 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.16", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.17 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.17", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.18 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.18", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.3.19 = STRING: "0" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.3.19", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.1 = STRING: "MTYx" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.2 = STRING: "MjM=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.3 = STRING: "ODA=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.4 = STRING: "OTk1MA==" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.5 = STRING: "OTk1MA==" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.6 = STRING: "NDQz" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.7 = STRING: "MjI=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.8 = STRING: "MjI=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.9 = STRING: "MTIz" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.9", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.10 = STRING: "MjU=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.10", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.11 = STRING: "MjE=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.11", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.12 = STRING: "MjE=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.12", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.13 = STRING: "Njg=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.13", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.14 = STRING: "NTQ2" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.14", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.15 = STRING: "MTgxMg==" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.15", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.16 = STRING: "MTYx" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.16", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.17 = STRING: "MjE=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.17", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.18 = STRING: "MjE=" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.18", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.4.19 = STRING: "OTk1MQ==" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.6.1.4.19", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.1 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.2 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.3 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.4 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.5 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.6 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.7 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.8 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.9 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.9", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.10 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.10", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.11 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.11", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.12 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.12", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.13 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.13", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.14 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.14", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.15 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.15", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.16 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.16", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.17 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.17", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.18 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.18", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.6.1.5.19 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.6.1.5.19", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.7.0 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.7.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.8.1.1.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.8.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.8.1.2.1 = STRING: "power" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.8.1.3.1 = STRING: "pdu" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.3.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.8.1.4.1 = STRING: "rpdu" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.4.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.8.1.5.1 = STRING: "version7" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.2.8.1.5.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.8.1.6.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.8.1.6.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.9.0 = INTEGER: 0 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.9.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.10.0 = INTEGER: 0 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.10.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.2.12.0 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.2.12.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.4.1.0 = INTEGER: 0 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.4.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.4.3.0 = STRING: "<Host Name or IP>,<1 digit type identifier>" */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.4.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.4.4.0 = INTEGER: 0 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.4.4.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.1.0 = INTEGER: 12 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.1 = INTEGER: 1 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.2 = INTEGER: 2 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.3 = INTEGER: 3 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.4 = INTEGER: 4 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.5 = INTEGER: 5 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.6 = INTEGER: 6 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.7 = INTEGER: 7 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.8 = INTEGER: 8 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.9 = INTEGER: 9 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.9", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.10 = INTEGER: 10 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.10", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.11 = INTEGER: 11 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.11", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.1.12 = INTEGER: 12 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.1.12", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.1 = INTEGER: 3841 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.1", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.2 = INTEGER: 3843 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.2", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.3 = INTEGER: 3845 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.3", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.4 = INTEGER: 3848 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.4", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.5 = INTEGER: 3862 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.5", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.6 = INTEGER: 3864 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.6", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.7 = INTEGER: 3856 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.7", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.8 = INTEGER: 3858 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.8", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.9 = INTEGER: 3860 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.9", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.10 = INTEGER: 3871 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.10", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.11 = INTEGER: 3873 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.11", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.5.2.1.2.12 = INTEGER: 3875 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.5.2.1.2.12", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.6.1.1.0 = Hex-STRING: 07 00 00 00  */
	{ "unmapped.experimental", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.4.6.1.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.7.1.0 = INTEGER: 4 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.1.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.7.3.0 = INTEGER: 4 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.3.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.7.4.0 = INTEGER: 3 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.4.0", NULL, SU_FLAG_OK, NULL, NULL },
	/* experimental.7.5.0 = INTEGER: 3 */
	{ "unmapped.experimental", 0, 1, ".1.3.6.1.4.1.318.1.4.7.5.0", NULL, SU_FLAG_OK, NULL, NULL },
#endif /* scan result */


	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL, NULL }
};

#define APC_PDU_DEVICE_MODEL ".1.3.6.1.4.1.318.1.1.4.1.4.0"
mib2nut_info_t apc_pdu_rpdu = { "apc_pdu", APC_PDU_MIB_VERSION, NULL, APC_PDU_DEVICE_MODEL, apc_pdu_mib, APC_PDU_MIB_SYSOID_RPDU };
mib2nut_info_t apc_pdu_rpdu2 = { "apc_pdu", APC_PDU_MIB_VERSION, NULL, APC_PDU_DEVICE_MODEL, apc_pdu_mib, APC_PDU_MIB_SYSOID_RPDU2 };
mib2nut_info_t apc_pdu_msp = { "apc_pdu", APC_PDU_MIB_VERSION, NULL, APC_PDU_DEVICE_MODEL, apc_pdu_mib, APC_PDU_MIB_SYSOID_MSP };
