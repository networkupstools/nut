/* apc-epdu-mib.c - subdriver to monitor apc SNMP easy pdu with NUT
 *
 *  Copyright (C)
 *  2011 - 2022	Eric Clappier <EricClappier@eaton.com>
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

#include "apc-epdu-mib.h"

#define APC_EPDU_MIB_VERSION  "0.10"

#define APC_EPDU_MIB_SYSOID   ".1.3.6.1.4.1.318.1.3.4.9"

static info_lkp_t apc_epdu_sw_outlet_status_info[] = {
	info_lkp_default(1, "off"),
	info_lkp_default(2, "on"),
	info_lkp_sentinel
};

static info_lkp_t apc_epdu_sw_outlet_switchability_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "yes"),
	info_lkp_sentinel
};

/* POWERNET-MIB Snmp2NUT lookup table */
static snmp_info_t apc_epdu_mib[] = {

	/* Device page */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "APC", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	/* ePDUDeviceStatusModelNumber.1 = STRING: "EPDU1016M" */
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.30.2.1.1.4.1", "Easy ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_STALE | SU_FLAG_OK, NULL),
	snmp_info_default("device.description", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.5.0", NULL, SU_FLAG_STALE | SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_STALE | SU_FLAG_OK, NULL),
	/* FIXME: to be RFC'ed */
	snmp_info_default("device.uptime", 0, 1, ".1.3.6.1.2.1.1.3.0", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	/* ePDUDeviceStatusSerialNumber.1 = STRING: "506255604729" */
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE, " .1.3.6.1.4.1.318.1.1.30.2.1.1.5.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ePDUDeviceStatusModelNumber.1 = STRING: "EPDU1016M" */
	snmp_info_default("device.part", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.30.2.1.1.4.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ePDUDeviceStatusVersion.1 = STRING: "Ver16.10" */
	snmp_info_default("device.version", ST_FLAG_STRING, SU_INFOSIZE, " .1.3.6.1.4.1.318.1.1.30.2.1.1.3.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	/* Input */
	/* ePDUPhaseTableSize = INTEGER: 1 */
	snmp_info_default("input.phases", 0, 1, ".1.3.6.1.4.1.318.1.1.30.3.0", NULL, SU_FLAG_OK, NULL),
	/* ePDUDeviceStatusActivePower.1 = INTEGER: 785 */
	snmp_info_default("input.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.7.1", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	/* Take first phase for global if single phase */
	/* ePDUPhaseStatusVoltage.1 = INTEGER: 2304 */
	snmp_info_default("input.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.4.1", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_1, NULL),
	/* Take first phase for global if single phase */
	/* ePDUPhaseStatusCurrent.1 = INTEGER: 355 */
	snmp_info_default("input.current", 0, 0.01, ".1.3.6.1.4.1.318.1.1.30.4.2.1.5.1", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_1, NULL),

	/* Only if tree-phase */
	/* ePDUPhaseStatusVoltage.1 = INTEGER: 2304 */
	snmp_info_default("input.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.4.1", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusVoltage.2 = INTEGER: 2304 */
	snmp_info_default("input.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.4.2", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusVoltage.3 = INTEGER: 2304 */
	snmp_info_default("input.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.4.3", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusCurrent.1 = INTEGER: 355 */
	snmp_info_default("input.L1.current", 0, 0.01, ".1.3.6.1.4.1.318.1.1.30.4.2.1.5.1", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusCurrent.2 = INTEGER: 355 */
	snmp_info_default("input.L2.current", 0, 0.01, ".1.3.6.1.4.1.318.1.1.30.4.2.1.5.2", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusCurrent.3 = INTEGER: 355 */
	snmp_info_default("input.L3.current", 0, 0.01, ".1.3.6.1.4.1.318.1.1.30.4.2.1.5.3", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusActivePower.1 = INTEGER: 785 */
	snmp_info_default("input.L1.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.6.1", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusActivePower.2 = INTEGER: 785 */
	snmp_info_default("input.L2.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.6.2", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),
	/* ePDUPhaseStatusActivePower.3 = INTEGER: 785 */
	snmp_info_default("input.L3.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.6.3", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_INPUT_3, NULL),

	/* Outlets */
	/* ePDUOutletTableSize.0 = INTEGER: 1 */
	snmp_info_default("outlet.count", 0, 1, ".1.3.6.1.4.1.318.1.1.30.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* ePDUOutletStatusIndex.%i = INTEGER: 1 */
	snmp_info_default("outlet.%i.id", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.1.1.1.%i", "%i", SU_FLAG_STATIC | SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_OUTLET, NULL),
	/* ePDUOutletStatusModule.%i= INTEGER: 1 */
	snmp_info_default("outlet.%i.name",  0, 1, ".1.3.6.1.4.1.318.1.1.30.6.2.1.2.%i", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_OUTLET, NULL),
	/* ePDUOutletStatusNumber.%i = INTEGER: 1 */
	snmp_info_default("outlet.%i.desc", 0, 1, NULL, "Outlet %i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_OUTLET, NULL),
	/* ePDUOutletStatusState.%i = INTEGER: off(1) */
	snmp_info_default("outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.30.6.1.1.4.%i", NULL, SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_OUTLET, &apc_epdu_sw_outlet_status_info[0]),
	/* Also use this OID to determine switchability ; its presence means "yes" */
	/* ePDUOutletStatusState.%i = INTEGER: off(1) */
	snmp_info_default("outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.30.6.1.1.4.%i", "yes",  SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, &apc_epdu_sw_outlet_switchability_info[0]),

#if WITH_UNMAPPED_DATA_POINTS /* keep following scan for future development */
	/* iso.3.6.1.4.1.318.1.1.30.1.0 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.1.0", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.3.1 = STRING: "Ver16.10" */
	snmp_info_default("unmapped.iso", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.30.2.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.4.1 = STRING: "EPDU1016M" */
	snmp_info_default("unmapped.iso", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.30.2.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.5.1 = STRING: "506255604717" */
	snmp_info_default("unmapped.iso", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.30.2.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.6.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.7.1 = INTEGER: 785 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.8.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.9.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.10.1 = INTEGER: 965 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.11.1 = INTEGER: 9114157 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.11.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.1.1.12.1 = INTEGER: 49988 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.1.1.12.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.2.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.2.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.2.1.3.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.2.1.4.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.2.2.1.5.1 = INTEGER: 2 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.2.2.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.3.0 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.3.0", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.1.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.1.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.1.1.3.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.1.1.4.1 = INTEGER: 3000 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.1.1.5.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.1.1.6.1 = INTEGER: 3200 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.1.1.7.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.1.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.3.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.4.1 = INTEGER: 2304 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.5.1 = INTEGER: 353 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.6.1 = INTEGER: 785 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.7.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.8.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.9.1 = INTEGER: 965 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.4.2.1.10.1 = INTEGER: 9114157 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.4.2.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.5.0 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.5.0", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.1.1.1.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.1.1.2.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.1.1.3.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.1.1.4.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.2.1.1.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.2.1.2.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.2.1.3.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.6.2.1.4.1 = INTEGER: -1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.6.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.7.0 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.7.0", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.1.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.1.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.1.1.3.1 = INTEGER: 900 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.1.1.4.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.1.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.1.1.5.1 = INTEGER: 900 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.1.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.1.1.6.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.1.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.2.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.2.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.2.1.3.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* iso.3.6.1.4.1.318.1.1.30.8.2.1.4.1 = INTEGER: 0 */
	snmp_info_default("unmapped.iso", 0, 1, ".1.3.6.1.4.1.318.1.1.30.8.2.1.4.1", NULL, SU_FLAG_OK, NULL),
#endif

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t  apc_pdu_epdu = { "apc", APC_EPDU_MIB_VERSION, NULL, NULL, apc_epdu_mib, APC_EPDU_MIB_SYSOID, NULL };
