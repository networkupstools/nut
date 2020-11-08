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

#define EATON_ATS30_MIB_VERSION  "0.02"

#define EATON_ATS30_SYSOID       ".1.3.6.1.4.1.534.10.1"
#define EATON_ATS30_MODEL        ".1.3.6.1.4.1.534.10.1.2.1.0"

static info_lkp_t eaton_ats30_source_info[] = {
	{ 1, "init", NULL, NULL },
	{ 2, "diagnosis", NULL, NULL },
	{ 3, "off", NULL, NULL },
	{ 4, "1", NULL, NULL },
	{ 5, "2", NULL, NULL },
	{ 6, "safe", NULL, NULL },
	{ 7, "fault", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t eaton_ats30_input_sensitivity[] = {
	{ 1, "high", NULL, NULL },
	{ 2, "low", NULL, NULL },
	{ 0, NULL, NULL, NULL }
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
	{ 0, "OL", NULL, NULL },
	{ 1, "OL", NULL, NULL }, /* SwitchFault */
	{ 2, "OFF", NULL, NULL }, /* NoOutput */
	{ 3, "OFF", NULL, NULL }, /* SwitchFault + NoOutput */
	{ 4, "OL OVER", NULL, NULL }, /* OutputOC */
	{ 5, "OL OVER", NULL, NULL }, /* OutputOC + SwitchFault */
	{ 6, "OFF OVER", NULL, NULL }, /* OutputOC + NoOutput */
	{ 7, "OFF OVER", NULL, NULL }, /* OutputOC + SwitchFault + NoOutput */
	{ 8, "OL", NULL, NULL }, /* OverTemperature */
	{ 9, "OL", NULL, NULL }, /* OverTemperature + SwitchFault */
	{ 10, "OFF", NULL, NULL }, /* OverTemperature + NoOutput */
	{ 11, "OFF", NULL, NULL }, /* OverTemperature + SwitchFault + NoOutput */
	{ 12, "OL OVER", NULL, NULL }, /* OverTemperature + OutputOC */
	{ 13, "OL OVER", NULL, NULL }, /* OverTemperature + OutputOC + SwitchFault */
	{ 14, "OFF OVER", NULL, NULL }, /* OverTemperature + OutputOC + NoOutput */
	{ 15, "OFF OVER", NULL, NULL }, /* OverTemperature + OutputOC + SwitchFault + NoOutput */
	{ 0, NULL, NULL, NULL }
};

/* EATON_ATS30 Snmp2NUT lookup table */
static snmp_info_t eaton_ats30_mib[] = {
	/* device type: ats */
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ats", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },

	/* standard MIB items */
	{ "device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL },
	{ "device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL },
	{ "device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL },

	/* enterprises.534.10.1.1.1.0 = STRING: "Eaton" */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.1.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.1.2.0 = STRING: "01.12.13b" -- SNMP agent version */
	/* { "device.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.1.2.0", NULL, SU_FLAG_OK, NULL }, */
	/* enterprises.534.10.1.1.3.1.0 = INTEGER: 1 */
	/* { "unmapped.enterprise", 0, 1, ".1.3.6.1.4.1.534.10.1.1.3.1.0", NULL, SU_FLAG_OK, NULL }, */
	/* enterprises.534.10.1.2.1.0 = STRING: "STS30002SR10019 " */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.2.2.0 = STRING: "1A0003AR00.00.00" -- Firmware */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.2.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.2.3.0 = STRING: "2014-09-17      "  -- Release date */
	/* { "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.3.0", NULL, SU_FLAG_OK, NULL }, */
	/* enterprises.534.10.1.2.4.0 = STRING: "JA00E52021          " */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.4.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.2.5.0 = STRING: "                    " -- Device ID codes */
	/* { "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.2.5.0", NULL, SU_FLAG_OK, NULL }, */

	/* ats measure */
	/* =========== */
	/* enterprises.534.10.1.3.1.1.1.1 = INTEGER: 1 */
	{ "input.1.id", 0, 1, ".1.3.6.1.4.1.534.10.1.3.1.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.1.1.1.2 = INTEGER: 2 */
	{ "input.2.id", 0, 1, ".1.3.6.1.4.1.534.10.1.3.1.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.1.1.2.1 = INTEGER: 2379 */
	{ "input.1.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.1.1.2.2 = INTEGER: 0 */
	{ "input.2.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.1.1.3.1 = INTEGER: 500 */
	{ "input.1.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.1.1.3.2 = INTEGER: 0 */
	{ "input.2.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.1.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.2.1.0 = INTEGER: 2375 */
	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.2.1.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.2.2.0 = INTEGER: 0 */
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.534.10.1.3.2.2.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.3.0 = INTEGER: 25 -- internal temperature in celsius */
	{ "ups.temperature", 0, 1, ".1.3.6.1.4.1.534.10.1.3.3.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.4.0 = INTEGER: 77 -- internal temperature in  fahrenheit */
	/* { "ups.temperatureF", 0, 1, ".1.3.6.1.4.1.534.10.1.3.4.0", NULL, SU_FLAG_OK, NULL }, */
	/* enterprises.534.10.1.3.5.0 = INTEGER: 37937541 */
	{ "device.uptime", 0, 1, ".1.3.6.1.4.1.534.10.1.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.3.6.0 = INTEGER: 284 */
	/* { "unmapped.atsMessureTransferedTimes", 0, 1, ".1.3.6.1.4.1.534.10.1.3.6.0", NULL, SU_FLAG_OK, NULL }, */
	/* enterprises.534.10.1.3.7.0 = INTEGER: 4 */
	{ "input.source", 0, 1, ".1.3.6.1.4.1.534.10.1.3.7.0", NULL, SU_FLAG_OK, eaton_ats30_source_info },

	/* atsStatus */
	/* ========= */
#if 0
	/* NOTE: Unused OIDs are left as comments for potential future improvements */
	/* enterprises.534.10.1.4.1.0 = INTEGER: 7 */
	{ "unmapped.atsInputFlowIndicator", 0, 1, ".1.3.6.1.4.1.534.10.1.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.1.1 = INTEGER: 1 -- atsInputFlowTable start */
	{ "unmapped.atsInputFlowIndex.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.1.2 = INTEGER: 2 */
	{ "unmapped.atsInputFlowIndex.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.2.1 = INTEGER: 1 */
	{ "unmapped.atsInputFlowRelay.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.2.2 = INTEGER: 2 */
	{ "unmapped.atsInputFlowRelay.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.3.1 = INTEGER: 1 */
	{ "unmapped.atsInputFlowSCR.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.3.2 = INTEGER: 2 */
	{ "unmapped.atsInputFlowSCR.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.4.1 = INTEGER: 1 */
	{ "unmapped.atsInputFlowParallelRelay.1", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.2.1.4.2 = INTEGER: 2 */
	{ "unmapped.atsInputFlowParallelRelay.2", 0, 1, ".1.3.6.1.4.1.534.10.1.4.2.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.3.0 = INTEGER: 58720256 */
	{ "unmapped.atsInputFailureIndicator", 0, 1, ".1.3.6.1.4.1.534.10.1.4.3.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.1.1 = INTEGER: 1 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.1.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.2.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.2.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.3.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.3.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.4.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.4.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.5.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.5.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.5.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.6.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.6.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.6.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.7.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.7.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.7.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.7.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.8.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.8.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.8.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.8.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.9.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.9.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.9.2 = INTEGER: 1 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.9.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.10.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.10.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.10.2 = INTEGER: 1 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.10.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.11.1 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.11.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.4.1.11.2 = INTEGER: 1 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.4.1.11.2", NULL, SU_FLAG_OK, NULL },
#endif /* 0 */

	/* enterprises.atsFailureIndicator = INTEGER: 0 */
	{ "ups.status", 0, 1, ".1.3.6.1.4.1.534.10.1.4.5.0", NULL, SU_FLAG_OK, eaton_ats30_status_info },

#if 0
	/* enterprises.534.10.1.4.6.1.0 = INTEGER: 2 -- atsFailure start */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.1.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.6.2.0 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.2.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.6.3.0 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.3.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.4.6.4.0 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.4.6.4.0", NULL, SU_FLAG_OK, NULL },
#endif /* 0 */

	/* atsLog */
	/* ====== */
#if 0
	/* We are not interested in log */
	/* enterprises.534.10.1.5.1.0 = INTEGER: 272 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.1.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.1 = INTEGER: 1 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.3 = INTEGER: 3 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.3", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.4 = INTEGER: 4 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.5 = INTEGER: 5 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.6 = INTEGER: 6 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.7 = INTEGER: 7 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.8 = INTEGER: 8 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.9 = INTEGER: 9 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.9", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.1.10 = INTEGER: 10 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.1.10", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.1 = INTEGER: 1482323677 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.2 = INTEGER: 1480076955 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.3 = INTEGER: 1480069128 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.3", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.4 = INTEGER: 1480069093 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.4", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.5 = INTEGER: 1478693745 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.5", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.6 = INTEGER: 1478693741 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.6", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.7 = INTEGER: 1466604406 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.7", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.8 = INTEGER: 1466604386 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.8", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.9 = INTEGER: 1466604386 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.9", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.2.10 = INTEGER: 1463038288 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.2.10", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.1 = INTEGER: 41 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.2 = INTEGER: 41 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.3 = INTEGER: 44 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.3", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.4 = INTEGER: 44 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.4", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.5 = INTEGER: 44 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.5", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.6 = INTEGER: 41 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.6", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.7 = INTEGER: 41 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.7", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.8 = INTEGER: 46 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.8", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.9 = INTEGER: 45 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.9", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.3.10 = INTEGER: 41 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.5.2.1.3.10", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.1 = STRING: "12:34:37 12/21/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.2 = STRING: "12:29:15 11/25/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.3 = STRING: "10:18:48 11/25/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.3", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.4 = STRING: "10:18:13 11/25/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.4", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.5 = STRING: "12:15:45 11/09/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.5", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.6 = STRING: "12:15:41 11/09/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.6", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.7 = STRING: "14:06:46 06/22/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.7", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.8 = STRING: "14:06:26 06/22/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.8", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.9 = STRING: "14:06:26 06/22/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.9", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.5.2.1.4.10 = STRING: "07:31:28 05/12/2016" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.5.2.1.4.10", NULL, SU_FLAG_OK, NULL },
#endif /* 0 */

	/* atsConfig */
	/* ========= */
#if 0
	/* enterprises.534.10.1.6.1.1.0 = INTEGER: 538562409 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.1.2.0 = STRING: "01/24/2017" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.6.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.1.3.0 = STRING: "08:40:09" */
	{ "unmapped.enterprises", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.1.6.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.1.1 = INTEGER: 1 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.1.2 = INTEGER: 2 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.2.1 = INTEGER: 1700 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.2.2 = INTEGER: 1700 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.3.1 = INTEGER: 1800 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.3.2 = INTEGER: 1800 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.4.1 = INTEGER: 2640 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.4.2 = INTEGER: 2640 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.5.1 = INTEGER: 3000 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.5.2 = INTEGER: 3000 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.5.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.6.1 = INTEGER: 50 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.6.2 = INTEGER: 50 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.6.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.7.1 = INTEGER: 40 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.7.1", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.2.1.7.2 = INTEGER: 40 */
	{ "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.6.2.1.7.2", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.3.0 = INTEGER: 2640 */
	{ "unmapped.atsConfigInputVoltageRating", 0, 1, ".1.3.6.1.4.1.534.10.1.6.3.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.4.0 = INTEGER: 26 */
	{ "unmapped.atsConfigRandomTime", 0, 1, ".1.3.6.1.4.1.534.10.1.6.4.0", NULL, SU_FLAG_OK, NULL },
#endif /* 0 */

	/* enterprises.534.10.1.6.5.0 = INTEGER: 1 */
	{ "input.source.preferred", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.1.6.5.0", NULL, SU_FLAG_OK, NULL },
	/* enterprises.534.10.1.6.6.0 = INTEGER: 2 */
	{ "input.sensitivity", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.1.6.6.0", NULL, SU_FLAG_OK, eaton_ats30_input_sensitivity },

	/* enterprises.534.10.1.6.7.0 = INTEGER: 2 */
	/* { "unmapped.atsConfigTest", 0, 1, ".1.3.6.1.4.1.534.10.1.6.7.0", NULL, SU_FLAG_OK, NULL }, */

	/* atsUpgrade */
	/* ========== */
#if 0
	/* We are not interested in atsUpgrade */
	/* enterprises.534.10.1.7.1.0 = INTEGER: 1 */
	/* { "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.7.1.0", NULL, SU_FLAG_OK, NULL }, */
	/* enterprises.534.10.1.7.2.0 = INTEGER: 1 */
	/* { "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.7.2.0", NULL, SU_FLAG_OK, NULL }, */
	/* enterprises.534.10.1.7.3.0 = INTEGER: 0 */
	/* { "unmapped.enterprises", 0, 1, ".1.3.6.1.4.1.534.10.1.7.3.0", NULL, SU_FLAG_OK, NULL }, */
#endif /* 0 */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	eaton_ats30 = { "eaton_ats30", EATON_ATS30_MIB_VERSION, NULL, EATON_ATS30_MODEL, eaton_ats30_mib, EATON_ATS30_SYSOID, NULL };
