/*  pwmib.h - data to monitor Powerware UPS with NUT
 *  (using MIBs described in stdupsv1.mib and Xups.mib)
 *
 *  Copyright (C) 2005
 *  			Olli Savia <ops@iki.fi>
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

#define PW_MIB_VERSION                   "0.1"

#define PW_OID_MANUFACTURER              ".1.3.6.1.2.1.33.1.1.1"
#define PW_OID_MODEL                     ".1.3.6.1.2.1.33.1.1.2"
#define PW_OID_SOFTWARE_VERSION          ".1.3.6.1.2.1.33.1.1.3"
#define PW_OID_AGENT_SOFTWARE_VERSION    ".1.3.6.1.2.1.33.1.1.4"

#define PW_OID_BATTERY_STATUS            ".1.3.6.1.4.1.534.1.2.5" /* Xups.mib */
#define PW_OID_BATTERY_MINUTES_REMAINING ".1.3.6.1.2.1.33.1.2.3"
#define PW_OID_BATTERY_CHARGE_REMAINING  ".1.3.6.1.2.1.33.1.2.4"
#define PW_OID_BATTERY_VOLTAGE           ".1.3.6.1.2.1.33.1.2.5"

#define PW_OID_IN_FREQUENCY              ".1.3.6.1.2.1.33.1.3.3.1.2"
#define PW_OID_IN_VOLTAGE                ".1.3.6.1.2.1.33.1.3.3.1.3"

#define PW_OID_OUT_FREQUENCY             ".1.3.6.1.2.1.33.1.4.2"
#define PW_OID_OUT_VOLTAGE               ".1.3.6.1.2.1.33.1.4.4.1.2"
#define PW_OID_OUT_CURRENT               ".1.3.6.1.2.1.33.1.4.4.1.3"
#define PW_OID_OUT_POWER                 ".1.3.6.1.2.1.33.1.4.4.1.4"
#define PW_OID_OUT_PERCENTLOAD           ".1.3.6.1.2.1.33.1.4.4.1.5"

#define PW_OID_AMBIENT_TEMP              ".1.3.6.1.4.1.534.1.6.1" /* Xups.mib */


/* Defines for PW_OID_BATTERY_STATUS */
info_lkp_t pw_batt_info[] = {
	{ 1, "CHARGING" },
	{ 2, "DISCHARGING" },
	{ 3, "FLOATING" },
	{ 4, "RESTING" },
	{ 5, "UNKNOWN" },
	{ 0, "NULL" }
} ;


/* Snmp2NUT lookup table */

snmp_info_t pw_mib[] = {
	/* UPS page */
	{ "ups.firmware",        ST_FLAG_STRING, SU_INFOSIZE, PW_OID_SOFTWARE_VERSION,          "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware.aux",    ST_FLAG_STRING, SU_INFOSIZE, PW_OID_AGENT_SOFTWARE_VERSION,    "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.load",            0,              1.0,         PW_OID_OUT_PERCENTLOAD,           "", SU_FLAG_OK,                  NULL },
	{ "ups.mfr",             ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MANUFACTURER,              "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.model",           ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MODEL,                     "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.power",           0,              1.0,         PW_OID_OUT_POWER,                 "", SU_FLAG_OK,                  NULL },
	{ "ups.status",          ST_FLAG_STRING, SU_INFOSIZE, PW_OID_BATTERY_STATUS,            "", SU_FLAG_OK | SU_STATUS_BATT, &pw_batt_info[0] },
	/* Battery page */
	{ "battery.charge",      0,              1.0,         PW_OID_BATTERY_CHARGE_REMAINING,  "", SU_FLAG_OK,                  NULL },
	{ "battery.runtime",     0,              60.0,        PW_OID_BATTERY_MINUTES_REMAINING, "", SU_FLAG_OK,                  NULL },
	{ "battery.voltage",     0,              0.1,         PW_OID_BATTERY_VOLTAGE,           "", SU_FLAG_OK,                  NULL },
	/* Output page */
	{ "output.current",      0,              1.0,         PW_OID_OUT_CURRENT,               "", SU_FLAG_OK,                  NULL },
	{ "output.frequency",    0,              0.1,         PW_OID_OUT_FREQUENCY,             "", SU_FLAG_OK,                  NULL },
	{ "output.voltage",      0,              1.0,         PW_OID_OUT_VOLTAGE,               "", SU_FLAG_OK,                  NULL },
        /* Input page */
	{ "input.frequency",     0,              0.1,         PW_OID_IN_FREQUENCY,              "", SU_FLAG_OK,                  NULL },
	{ "input.voltage",       0,              1.0,         PW_OID_IN_VOLTAGE,                "", SU_FLAG_OK,                  NULL },
	/* Ambient page */
	{ "ambient.temperature", 0,              1.0,         PW_OID_AMBIENT_TEMP,              "", SU_FLAG_OK,                  NULL },
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;
