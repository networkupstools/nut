/*  ietfmib.h - data to monitor SNMP UPS (RFC 1628 compliant) with NUT
 *
 *  Copyright (C) 2002-2003 
 *  			Arnaud Quette <arnaud.quette@free.fr>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#define IETF_MIB_VERSION			"1.1"

/* SNMP OIDs set */
#define IETF_OID_UPS_MIB			".1.3.6.1.2.1.33"
#define IETF_OID_MFR_NAME			".1.3.6.1.2.1.33.1.1.1"
#define IETF_OID_MODEL_NAME			".1.3.6.1.2.1.33.1.1.2"
#define IETF_OID_FIRMREV			".1.3.6.1.2.1.33.1.1.4"
#define IETF_OID_BATT_STATUS		".1.3.6.1.2.1.33.1.2.1"     /* INFO_STATUS (2) */
#define IETF_OID_BATT_RUNTIME		".1.3.6.1.2.1.33.1.2.3"
#define IETF_OID_BATT_RUNTIME_LOW	".1.3.6.1.2.1.33.1.9.7"
#define IETF_OID_BATT_CHARGE		".1.3.6.1.2.1.33.1.2.4"
#define IETF_OID_BATT_VOLTAGE		".1.3.6.1.2.1.33.1.2.5"
#define IETF_OID_IN_FREQ			".1.3.6.1.2.1.33.1.3.3.1.2"
#define IETF_OID_IN_VOLTAGE			".1.3.6.1.2.1.33.1.3.3.1.3"
#define IETF_OID_POWER_STATUS		".1.3.6.1.2.1.33.1.4.1"     /* INFO_STATUS (1) */
#define IETF_OID_OUT_VOLTAGE		".1.3.6.1.2.1.33.1.9.3"     /* INFO_UTILITY ? */
#define IETF_OID_UTIL_VOLTAGE		".1.3.6.1.2.1.33.1.4.4.1.2" /* INFO_UTILITY ? */
#define IETF_OID_LOAD_LEVEL			".1.3.6.1.2.1.33.1.4.4.1.5" /* INFO_LOADPCT */
#define IETF_OID_OUTPUT_TAB			".1.3.6.1.2.1.33.1.4.4"

/* Defines for IETF_OID_POWER_STATUS (1) */
info_lkp_t ietf_pwr_info[] = {
	{ 1, "" },
	{ 2, "OFF" },
	{ 3, "OL" },
	{ 4, "BYPASS" },
	{ 5, "OB" },
	{ 6, "BOOST" },
	{ 7, "TRIM" },
	{ 0, "NULL" }
} ;

/* Defines for IETF_OID_BATT_STATUS (2) */
info_lkp_t ietf_batt_info[] = {
	{ 1, "" },
	{ 2, "" },
	{ 3, "LB" },
	{ 4, "" },
	{ 0, "NULL" }
} ;

#define IETF_OID_SD_AFTER_DELAY		".1.3.6.1.2.1.33.1.8.2"
#define IETF_OFF_DO					0

#define IETF_OID_ALARM_OB ".1.3.6.1.2.1.33.1.6.3.2"

info_lkp_t ietf_alarm_ob[] = {
	{ 1, "OB" },
	{ 0, "NULL" }
} ;
		
#define IETF_OID_ALARM_LB ".1.3.6.1.2.1.33.1.6.3.3"

info_lkp_t ietf_alarm_lb[] = {
	{ 1, "LB" },
	{ 0, "NULL" }
} ;

/* Missing data
   CAL   - UPS is performing calibration
   OVER  - UPS is overloaded
   RB    - UPS battery needs to be replaced
   FSD   - UPS is in forced shutdown state (slaves take note)
*/

/* Snmp2NUT lookup table */

snmp_info_t ietf_mib[] = {
	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_MFR_NAME, "Generic",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_MODEL_NAME,
		"Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_FIRMREV, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_POWER_STATUS, "OFF",
		SU_FLAG_OK | SU_STATUS_PWR, &ietf_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_BATT_STATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, &ietf_batt_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_ALARM_OB, "",
		SU_FLAG_OK | SU_STATUS_BATT, &ietf_alarm_ob[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_ALARM_LB, "",
		SU_FLAG_OK | SU_STATUS_BATT, &ietf_alarm_lb[0] },
	{ "ups.load", 0, 1, IETF_OID_LOAD_LEVEL, "", SU_FLAG_OK, NULL },
	/* Battery page */
	{ "battery.charge", 0,1, IETF_OID_BATT_CHARGE, "", SU_FLAG_OK, NULL },
	{ "battery.runtime", 0, 60, IETF_OID_BATT_RUNTIME, "", SU_FLAG_OK, NULL },
	{ "battery.runtime.low", ST_FLAG_RW, 1, IETF_OID_BATT_RUNTIME_LOW, "", SU_FLAG_OK, NULL },
	{ "battery.voltage", 0, 0.1, IETF_OID_BATT_VOLTAGE, "", SU_FLAG_OK, NULL },
	/* Output page */
	{ "output.voltage", 0, 1, IETF_OID_OUT_VOLTAGE, "", SU_FLAG_OK, NULL },
	/* Input page */
	{ "input.voltage", 0, 1, IETF_OID_IN_VOLTAGE, "", SU_FLAG_OK, NULL },
	{ "input.frequency", 0, 0.1, IETF_OID_IN_FREQ, "", SU_FLAG_OK, NULL },
	/* instant commands. */
	{ "load.off", 0, IETF_OFF_DO, IETF_OID_SD_AFTER_DELAY, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
/*	{ CMD_SHUTDOWN, 0, IETF_OFF_GRACEFUL, IETF_OID_OFF, "", SU_FLAG_OK, NULL }, */
	
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};
