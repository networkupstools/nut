/* eaton-ats30-mib.c - subdriver to monitor eaton_ats30 SNMP devices with NUT
 *
 *  Copyright (C) 2017 Eaton
 *  Author: Tomas Halman <TomasHalman@eaton.com>
 *    2011-2012 Arnaud Quette <arnaud.quette@free.fr>
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

#include "eaton-ats30-mib.h"

#define EATON_ATS30_MIB_VERSION  "0.03"

#define EATON_ATS30_SYSOID       ".1.3.6.1.4.1.534.10.1"
#define EATON_ATS30_MODEL        ".1.3.6.1.4.1.534.10.1.2.1.0"

static info_lkp_t eaton_ats30_source_info[] = {
	info_lkp_default(1, "init"),
	info_lkp_default(2, "diagnosis"),
	info_lkp_default(3, "off"),
	info_lkp_default(4, "1"),
	info_lkp_default(5, "2"),
	info_lkp_default(6, "safe"),
	info_lkp_default(7, "fault"),
	info_lkp_sentinel
};

static info_lkp_t eaton_ats30_input_sensitivity[] = {
	info_lkp_default(1, "high"),
	info_lkp_default(2, "low"),
	info_lkp_sentinel
};

/*
 * bitmap values of atsStatus.atsFailureIndicator
 *
 * 1 atsFailureSwitchFault N/A
 * 2 atsFailureNoOutput OFF
 * 3 atsFailureOutputOC OVER
 * 4 atsFailureOverTemperature N/A
 */
static info_lkp_t eaton_ats30_status_info[] = {
	info_lkp_default(0, "OL"),
	info_lkp_default(1, "OL"),	/* SwitchFault */
	info_lkp_default(2, "OFF"),	/* NoOutput */
	info_lkp_default(3, "OFF"),	/* SwitchFault + NoOutput */
	info_lkp_default(4, "OL OVER"),	/* OutputOC */
	info_lkp_default(5, "OL OVER"),	/* OutputOC + SwitchFault */
	info_lkp_default(6, "OFF OVER"),	/* OutputOC + NoOutput */
	info_lkp_default(7, "OFF OVER"),	/* OutputOC + SwitchFault + NoOutput */
	info_lkp_default(8, "OL"),	/* OverTemperature */
	info_lkp_default(9, "OL"),	/* OverTemperature + SwitchFault */
	info_lkp_default(10, "OFF"),	/* OverTemperature + NoOutput */
	info_lkp_default(11, "OFF"),	/* OverTemperature + SwitchFault + NoOutput */
	info_lkp_default(12, "OL OVER"),	/* OverTemperature + OutputOC */
	info_lkp_default(13, "OL OVER"),	/* OverTemperature + OutputOC + SwitchFault */
	info_lkp_default(14, "OFF OVER"),	/* OverTemperature + OutputOC + NoOutput */
	info_lkp_default(15, "OFF OVER"),	/* OverTemperature + OutputOC + SwitchFault + NoOutput */
	info_lkp_sentinel
};

/* EATON_ATS30 Snmp2NUT lookup table */
static snmp_info_t eaton_ats30_mib[] = {
	/* device type: ats */
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ats", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* enterprises.534.10.1.1.1.0 = STRING: "Eaton" */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.1.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.1.2.0 = STRING: "01.12.13b" -- SNMP agent version */
	/* snmp_info_default("device.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.1.2.0", NULL, SU_FLAG_OK, NULL), */
	/* enterprises.534.10.1.1.3.1.0 = INTEGER: 1 */
	/* snmp_info_default("unmapped.enterprise", 0, 1, ".1.3.6.1.4.1.534.10.1.1.3.1.0", NULL, SU_FLAG_OK, NULL), */
	/* enterprises.534.10.1.2.1.0 = STRING: "STS30002SR10019 " */
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.2.2.0 = STRING: "1A0003AR00.00.00" -- Firmware */
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.2.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.2.3.0 = STRING: "2014-09-17      "  -- Release date */
	/* snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.3.0", NULL, SU_FLAG_OK, NULL), */
	/* enterprises.534.10.1.2.4.0 = STRING: "JA00E52021          " */
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.4.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.2.5.0 = STRING: "                    " -- Device ID codes */
	/* snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.5.0", NULL, SU_FLAG_OK, NULL), */

	/* ats measure */
	/* =========== */
	/* enterprises.534.10.1.3.1.1.1.1 = INTEGER: 1 */
	snmp_info_default("input.1.id", 0, 1, ".1.3.6.1.4.1.534.10.1.3.1.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.1.1.1.2 = INTEGER: 2 */
	snmp_info_default("input.2.id", 0, 1, ".1.3.6.1.4.1.534.10.1.3.1.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.1.1.2.1 = INTEGER: 2379 */
	snmp_info_default("input.1.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.1.1.2.2 = INTEGER: 0 */
	snmp_info_default("input.2.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.1.1.3.1 = INTEGER: 500 */
	snmp_info_default("input.1.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.1.1.3.2 = INTEGER: 0 */
	snmp_info_default("input.2.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.2.1.0 = INTEGER: 2375 */
	snmp_info_default("output.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.2.1.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.2.2.0 = INTEGER: 0 */
	snmp_info_default("output.current", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.2.2.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.3.0 = INTEGER: 25 -- internal temperature in celsius */
	snmp_info_default("ups.temperature", 0, 1, ".1.3.6.1.4.1.534.10.1.3.3.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.4.0 = INTEGER: 77 -- internal temperature in  fahrenheit */
	/* snmp_info_default("ups.temperatureF", 0, 1, ".1.3.6.1.4.1.534.10.1.3.4.0", NULL, SU_FLAG_OK, NULL), */
	/* enterprises.534.10.1.3.5.0 = INTEGER: 37937541 */
	snmp_info_default("device.uptime", 0, 1, ".1.3.6.1.4.1.534.10.1.3.5.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.3.6.0 = INTEGER: 284 */
	/* snmp_info_default("unmapped.atsMessureTransferedTimes", 0, 1, ".1.3.6.1.4.1.534.10.1.3.6.0", NULL, SU_FLAG_OK, NULL), */
	/* enterprises.534.10.1.3.7.0 = INTEGER: 4 */
	snmp_info_default("input.source", 0, 1, ".1.3.6.1.4.1.534.10.1.3.7.0", NULL, SU_FLAG_OK, eaton_ats30_source_info),

	/* atsStatus */
	/* ========= */
#if WITH_UNMAPPED_DATA_POINTS
	/* NOTE: Unused OIDs are left as comments for potential future improvements */
	/* enterprises.534.10.1.4.1.0 = INTEGER: 7 */
	snmp_info_default("unmapped.atsInputFlowIndicator", 0, 1, ".1.3.6.1.4.1.534.10.1.4.1.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.1.1 = INTEGER: 1 -- atsInputFlowTable start */
	snmp_info_default("unmapped.atsInputFlowIndex.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.atsInputFlowIndex.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.2.1 = INTEGER: 1 */
	snmp_info_default("unmapped.atsInputFlowRelay.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.2.2 = INTEGER: 2 */
	snmp_info_default("unmapped.atsInputFlowRelay.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.3.1 = INTEGER: 1 */
	snmp_info_default("unmapped.atsInputFlowSCR.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.3.2 = INTEGER: 2 */
	snmp_info_default("unmapped.atsInputFlowSCR.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.4.1 = INTEGER: 1 */
	snmp_info_default("unmapped.atsInputFlowParallelRelay.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.2.1.4.2 = INTEGER: 2 */
	snmp_info_default("unmapped.atsInputFlowParallelRelay.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.3.0 = INTEGER: 58720256 */
	snmp_info_default("unmapped.atsInputFailureIndicator", 0, 1, ".1.3.6.1.4.1.534.10.1.4.3.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.2.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.2.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.3.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.3.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.4.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.4.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.5.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.5.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.5.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.6.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.6.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.6.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.7.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.7.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.7.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.8.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.8.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.8.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.8.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.9.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.9.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.9.2 = INTEGER: 1 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.9.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.10.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.10.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.10.2 = INTEGER: 1 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.10.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.11.1 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.11.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.4.1.11.2 = INTEGER: 1 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.11.2", NULL, SU_FLAG_OK, NULL),
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* enterprises.atsFailureIndicator = INTEGER: 0 */
	snmp_info_default("ups.status", 0, 1, ".1.3.6.1.4.1.534.10.1.4.5.0", NULL, SU_FLAG_OK, eaton_ats30_status_info),

#if WITH_UNMAPPED_DATA_POINTS
	/* enterprises.534.10.1.4.6.1.0 = INTEGER: 2 -- atsFailure start */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.1.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.6.2.0 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.2.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.6.3.0 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.3.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.4.6.4.0 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.4.0", NULL, SU_FLAG_OK, NULL),
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* atsLog */
	/* ====== */
#if WITH_UNMAPPED_DATA_POINTS
	/* We are not interested in log */
	/* enterprises.534.10.1.5.1.0 = INTEGER: 272 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.1.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.3 = INTEGER: 3 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.3", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.4 = INTEGER: 4 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.4", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.5 = INTEGER: 5 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.5", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.6 = INTEGER: 6 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.6", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.7 = INTEGER: 7 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.7", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.8 = INTEGER: 8 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.8", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.9 = INTEGER: 9 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.9", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.1.10 = INTEGER: 10 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.10", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.1 = INTEGER: 1482323677 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.2 = INTEGER: 1480076955 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.3 = INTEGER: 1480069128 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.3", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.4 = INTEGER: 1480069093 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.4", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.5 = INTEGER: 1478693745 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.5", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.6 = INTEGER: 1478693741 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.6", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.7 = INTEGER: 1466604406 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.7", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.8 = INTEGER: 1466604386 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.8", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.9 = INTEGER: 1466604386 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.9", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.2.10 = INTEGER: 1463038288 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.10", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.1 = INTEGER: 41 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.2 = INTEGER: 41 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.3 = INTEGER: 44 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.3", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.4 = INTEGER: 44 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.4", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.5 = INTEGER: 44 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.5", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.6 = INTEGER: 41 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.6", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.7 = INTEGER: 41 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.7", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.8 = INTEGER: 46 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.8", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.9 = INTEGER: 45 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.9", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.3.10 = INTEGER: 41 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.10", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.1 = STRING: "12:34:37 12/21/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.2 = STRING: "12:29:15 11/25/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.3 = STRING: "10:18:48 11/25/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.3", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.4 = STRING: "10:18:13 11/25/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.4", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.5 = STRING: "12:15:45 11/09/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.5", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.6 = STRING: "12:15:41 11/09/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.6", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.7 = STRING: "14:06:46 06/22/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.7", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.8 = STRING: "14:06:26 06/22/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.8", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.9 = STRING: "14:06:26 06/22/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.9", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.5.2.1.4.10 = STRING: "07:31:28 05/12/2016" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.10", NULL, SU_FLAG_OK, NULL),
#endif	/* WITH_UNMAPPED_DATA_POINTS */

	/* atsConfig */
	/* ========= */
#if WITH_UNMAPPED_DATA_POINTS
	/* enterprises.534.10.1.6.1.1.0 = INTEGER: 538562409 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.1.1.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.1.2.0 = STRING: "01/24/2017" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.6.1.2.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.1.3.0 = STRING: "08:40:09" */
	snmp_info_default("unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.6.1.3.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.1.1 = INTEGER: 1 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.1.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.1.2 = INTEGER: 2 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.1.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.2.1 = INTEGER: 1700 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.2.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.2.2 = INTEGER: 1700 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.2.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.3.1 = INTEGER: 1800 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.3.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.3.2 = INTEGER: 1800 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.3.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.4.1 = INTEGER: 2640 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.4.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.4.2 = INTEGER: 2640 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.4.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.5.1 = INTEGER: 3000 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.5.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.5.2 = INTEGER: 3000 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.5.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.6.1 = INTEGER: 50 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.6.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.6.2 = INTEGER: 50 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.6.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.7.1 = INTEGER: 40 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.7.1", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.2.1.7.2 = INTEGER: 40 */
	snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.7.2", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.3.0 = INTEGER: 2640 */
	snmp_info_default("unmapped.atsConfigInputVoltageRating", 0, 1, ".1.3.6.1.4.1.534.10.1.6.3.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.4.0 = INTEGER: 26 */
	snmp_info_default("unmapped.atsConfigRandomTime", 0, 1, ".1.3.6.1.4.1.534.10.1.6.4.0", NULL, SU_FLAG_OK, NULL),
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* enterprises.534.10.1.6.5.0 = INTEGER: 1 */
	snmp_info_default("input.source.preferred", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.1.6.5.0", NULL, SU_FLAG_OK, NULL),
	/* enterprises.534.10.1.6.6.0 = INTEGER: 2 */
	snmp_info_default("input.sensitivity", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.1.6.6.0", NULL, SU_FLAG_OK, eaton_ats30_input_sensitivity),

	/* enterprises.534.10.1.6.7.0 = INTEGER: 2 */
	/* snmp_info_default("unmapped.atsConfigTest", 0, 1, ".1.3.6.1.4.1.534.10.1.6.7.0", NULL, SU_FLAG_OK, NULL), */

	/* atsUpgrade */
	/* ========== */
#if WITH_UNMAPPED_DATA_POINTS
	/* We are not interested in atsUpgrade */
	/* enterprises.534.10.1.7.1.0 = INTEGER: 1 */
	/* snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.7.1.0", NULL, SU_FLAG_OK, NULL), */
	/* enterprises.534.10.1.7.2.0 = INTEGER: 1 */
	/* snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.7.2.0", NULL, SU_FLAG_OK, NULL), */
	/* enterprises.534.10.1.7.3.0 = INTEGER: 0 */
	/* snmp_info_default("unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.7.3.0", NULL, SU_FLAG_OK, NULL), */
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	eaton_ats30 = { "eaton_ats30", EATON_ATS30_MIB_VERSION, NULL, EATON_ATS30_MODEL, eaton_ats30_mib, EATON_ATS30_SYSOID, NULL };
