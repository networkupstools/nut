/*  mgemib.h - data to monitor MGE UPS SYSTEMS SNMP devices with NUT
 *
 *  Copyright (C) 2002-2003 
 *  			Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *  			J.W. Hoogervorst <jeroen@hoogervorst.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
 *   and MGE Office Protection Systems <http://www.mgeops.com>
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

#define MGE_MIB_VERSION	"0.4"

/* SNMP OIDs set */
#define MGE_OID_UPS_MIB			".1.3.6.1.4.1.705"
#define MGE_OID_MODEL_NAME		".1.3.6.1.4.1.705.1.1.1.0"
#define MGE_OID_SERIAL			".1.3.6.1.4.1.705.1.1.7.0"
#define MGE_OID_LOBATTPCT		".1.3.6.1.4.1.705.1.4.8.0"
#define MGE_OID_BATT_RUNTIME 	".1.3.6.1.4.1.705.1.5.1.0"
#define MGE_OID_BATT_CHARGE		".1.3.6.1.4.1.705.1.5.2.0"
#define MGE_OID_BATTVOLT		".1.3.6.1.4.1.705.1.5.5.0"
#define MGE_OID_LOWBATT_STATUS	".1.3.6.1.4.1.705.1.5.14.0"
info_lkp_t mge_lowbatt_info[] = {
	{ 1, "LB" },
	{ 2, "" },
	{ 0, "NULL" }
};
#define MGE_OID_INVOLT			".1.3.6.1.4.1.705.1.6.2.1.2.0"
#define MGE_OID_INFREQ			".1.3.6.1.4.1.705.1.6.2.1.3.0"
#define MGE_OID_INVOLTMIN		".1.3.6.1.4.1.705.1.6.2.1.4.0"
#define MGE_OID_INVOLTMAX		".1.3.6.1.4.1.705.1.6.2.1.5.0"
#define MGE_OID_OUTVOLT			".1.3.6.1.4.1.705.1.7.2.1.2.0"
#define MGE_OID_OUTFREQ			".1.3.6.1.4.1.705.1.7.2.1.3.0"
#define MGE_OID_LOADPCT			".1.3.6.1.4.1.705.1.7.2.1.4.0"
#define MGE_OID_ONBATT_STATUS	".1.3.6.1.4.1.705.1.7.3.0"
info_lkp_t mge_onbatt_info[] = {
	{ 1, "OB" },
	{ 2, "OL" },
	{ 0, "NULL" }
};

#define MGE_OID_BYPASS_STATUS	".1.3.6.1.4.1.705.1.7.4.0"
info_lkp_t mge_bypass_info[] = {
	{ 1, "BYPASS" },
	{ 2, "" },
	{ 0, "NULL" }
};

#define MGE_OID_BOOST_STATUS	".1.3.6.1.4.1.705.1.7.8.0"
info_lkp_t mge_boost_info[] = {
	{ 1, "BOOST" },
	{ 2, "" },
	{ 0, "NULL" }
};

#define MGE_OID_OVERBATT_STATUS	".1.3.6.1.4.1.705.1.7.10.0"
info_lkp_t mge_overbatt_info[] = {
	{ 1, "OVER" },
	{ 2, "" },
	{ 0, "NULL" }
};

#define MGE_OID_AMBIENT_TEMP ".1.3.6.1.4.1.705.1.8.1.0"
#define MGE_OID_AMBIENT_HUMIDITY ".1.3.6.1.4.1.705.1.8.2.0"

#define MGE_OID_FIRMREV			".1.3.6.1.4.1.705.1.12.12.0"

#define MGE_OID_BATT_TEST		".1.3.6.1.4.1.705.1.10.4.0"

#define MGE_NOTHING_VALUE		1
#define MGE_START_VALUE			2
#define MGE_STOP_VALUE			3

/* TODO: PowerShare (per plug .1, .2, .3) and deals with delays */
#define MGE_OID_GRACEDELAY 		""
#define MGE_OID_REBOOT_CTRL		"1.3.6.1.4.1.705.1.9.1.1.9.1"
#define MGE_OID_OFF_CTRL 		"1.3.6.1.4.1.705.1.9.1.1.6.1"
#define MGE_OID_ON_CTRL 		"1.3.6.1.4.1.705.1.9.1.1.3.1"

/* Snmp2NUT lookup table */
snmp_info_t mge_mib[] = {
	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "MGE UPS SYSTEMS",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, MGE_OID_MODEL_NAME,
		"Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, MGE_OID_SERIAL, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, MGE_OID_FIRMREV, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_OID_ONBATT_STATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, &mge_onbatt_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_POWER_STATUS, "OFF",
		SU_FLAG_OK | SU_STATUS_PWR, &ietf_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_OID_LOWBATT_STATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, &mge_lowbatt_info[0] },
		
	{ "ups.load", 0, 1, MGE_OID_LOADPCT, "", SU_FLAG_OK, NULL },
	/* Input page */	
	{ "input.voltage", 0, 0.1, MGE_OID_INVOLT, "", SU_FLAG_OK, NULL },
	{ "input.voltage.minimum", 0, 0.1, MGE_OID_INVOLTMIN, "", SU_FLAG_OK, NULL },
	{ "input.voltage.maximum", 0, 0.1, MGE_OID_INVOLTMAX, "", SU_FLAG_OK, NULL },
	{ "input.frequency", 0, 0.1, MGE_OID_INFREQ, "", SU_FLAG_OK, NULL },
	/* Output page */
	{ "output.voltage", 0, 0.1, MGE_OID_OUTVOLT, "", SU_FLAG_OK, NULL },
	{ "output.frequency", 0, 0.1, MGE_OID_OUTFREQ, "", SU_FLAG_OK, NULL },
	/* Battery page */
	{ "battery.charge", 0, 1, MGE_OID_BATT_CHARGE, "", SU_FLAG_OK, NULL },
 	{ "battery.runtime", 0, 1, MGE_OID_BATT_RUNTIME, "", SU_FLAG_OK, NULL },
 	{ "battery.charge.low", ST_FLAG_STRING | ST_FLAG_RW, 2, MGE_OID_LOBATTPCT, "",
 		SU_TYPE_INT | SU_FLAG_OK, NULL },	
 	{ "battery.voltage", 0, 0.1, MGE_OID_BATTVOLT, "", SU_FLAG_OK, NULL },

	/* Ambient page: Environment Sensor (ref 66 846) */
 	{ "ambient.temperature", 0, 0.1, MGE_OID_AMBIENT_TEMP, "", SU_TYPE_INT | SU_FLAG_OK, NULL },
 	{ "ambient.humidity", 0, 0.1, MGE_OID_AMBIENT_HUMIDITY, "", SU_TYPE_INT | SU_FLAG_OK, NULL },

/*	{ "ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 3, MGE_OID_GRACEDELAY, "",
		SU_FLAG_OK, NULL },
*/
	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "Main Outlet", 
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* instant commands. */
	{ "test.battery.start", 0, MGE_START_VALUE, MGE_OID_BATT_TEST, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
/*	{ "load.off", 0, MGE_START_VALUE, MGE_OID_OFF_CTRL, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "load.on", 0, MGE_START_VALUE, MGE_OID_ON_CTRL, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "shutdown.return", 0, MGE_START_VALUE, MGE_OID_REBOOT_CTRL, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
*/	
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};
