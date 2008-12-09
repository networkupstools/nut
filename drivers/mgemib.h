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
#define MGE_BASE_OID		".1.3.6.1.4.1.705.1"
#define MGE_OID_MODEL_NAME	MGE_BASE_OID ".1.1.0"

static info_lkp_t mge_lowbatt_info[] = {
	{ 1, "LB" },
	{ 2, "" },
	{ 0, "NULL" }
};

static info_lkp_t mge_onbatt_info[] = {
	{ 1, "OB" },
	{ 2, "OL" },
	{ 0, "NULL" }
};

static info_lkp_t mge_bypass_info[] = {
	{ 1, "BYPASS" },
	{ 2, "" },
	{ 0, "NULL" }
};

static info_lkp_t mge_boost_info[] = {
	{ 1, "BOOST" },
	{ 2, "" },
	{ 0, "NULL" }
};

static info_lkp_t mge_trim_info[] = {
	{ 1, "TRIM" },
	{ 2, "" },
	{ 0, "NULL" }
};

static info_lkp_t mge_overload_info[] = {
	{ 1, "OVER" },
	{ 2, "" },
	{ 0, "NULL" }
};

#define MGE_NOTHING_VALUE	1
#define MGE_START_VALUE		2
#define MGE_STOP_VALUE		3

/* TODO: PowerShare (per plug .1, .2, .3) and deals with delays */

/* Snmp2NUT lookup table */
snmp_info_t mge_mib[] = {

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "MGE UPS SYSTEMS", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".1.1.0", "Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".1.7.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".12.12.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_POWER_STATUS, "OFF", SU_FLAG_OK | SU_STATUS_PWR, ietf_pwr_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".5.14.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_lowbatt_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_onbatt_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".7.4.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_bypass_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".7.8.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_boost_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".7.10.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_overload_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, MGE_BASE_OID ".7.12.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_trim_info },
	{ "ups.load", 0, 1, MGE_BASE_OID ".7.2.1.4.0", "", SU_OUTPUT_1, NULL },
	{ "ups.L1.load", 0, 1, MGE_BASE_OID ".7.2.1.4.1", "", SU_OUTPUT_3, NULL },
	{ "ups.L2.load", 0, 1, MGE_BASE_OID ".7.2.1.4.2", "", SU_OUTPUT_3, NULL },
	{ "ups.L3.load", 0, 1, MGE_BASE_OID ".7.2.1.4.3", "", SU_OUTPUT_3, NULL },
/*	{ "ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 3, MGE_OID_GRACEDELAY, "", SU_FLAG_OK, NULL }, */

	/* Input page */	
	{ "input.phases", 0, 1.0, MGE_BASE_OID ".6.1.0", "", SU_FLAG_SETINT, NULL, &input_phases },
	{ "input.voltage", 0, 0.1, MGE_BASE_OID ".6.2.1.2.0", "", SU_INPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 0.1, MGE_BASE_OID ".6.2.1.2.1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 0.1, MGE_BASE_OID ".6.2.1.2.2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 0.1, MGE_BASE_OID ".6.2.1.2.3", "", SU_INPUT_3, NULL },
	{ "input.frequency", 0, 0.1, MGE_BASE_OID ".6.2.1.3.0", "", SU_INPUT_1, NULL },
	{ "input.L1.frequency", 0, 0.1, MGE_BASE_OID ".6.2.1.3.1", "", SU_INPUT_3, NULL },
	{ "input.L2.frequency", 0, 0.1, MGE_BASE_OID ".6.2.1.3.2", "", SU_INPUT_3, NULL },
	{ "input.L3.frequency", 0, 0.1, MGE_BASE_OID ".6.2.1.3.3", "", SU_INPUT_3, NULL },
	{ "input.voltage.minimum", 0, 0.1, MGE_BASE_OID ".6.2.1.4.0", "", SU_INPUT_1, NULL },
	{ "input.L1-N.voltage.minimum", 0, 0.1, MGE_BASE_OID ".6.2.1.4.1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage.minimum", 0, 0.1, MGE_BASE_OID ".6.2.1.4.2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage.minimum", 0, 0.1, MGE_BASE_OID ".6.2.1.4.3", "", SU_INPUT_3, NULL },
	{ "input.voltage.maximum", 0, 0.1, MGE_BASE_OID ".6.2.1.5.0", "", SU_INPUT_1, NULL },
	{ "input.L1-N.voltage.maximum", 0, 0.1, MGE_BASE_OID ".6.2.1.5.1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage.maximum", 0, 0.1, MGE_BASE_OID ".6.2.1.5.2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage.maximum", 0, 0.1, MGE_BASE_OID ".6.2.1.5.3", "", SU_INPUT_3, NULL },
	{ "input.current", 0, 0.1, MGE_BASE_OID ".6.2.1.6.0", "", SU_INPUT_1, NULL },
	{ "input.L1.current", 0, 0.1, MGE_BASE_OID ".6.2.1.6.1", "", SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 0.1, MGE_BASE_OID ".6.2.1.6.2", "", SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 0.1, MGE_BASE_OID ".6.2.1.6.3", "", SU_INPUT_3, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, MGE_BASE_OID ".7.1.0", "", SU_FLAG_SETINT, NULL, &output_phases },
	{ "output.voltage", 0, 0.1, MGE_BASE_OID ".7.2.1.2.0", "", SU_OUTPUT_1, NULL },
	{ "output.L1-N.voltage", 0, 0.1, MGE_BASE_OID ".7.2.1.2.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 0.1, MGE_BASE_OID ".7.2.1.2.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 0.1, MGE_BASE_OID ".7.2.1.2.3", "", SU_OUTPUT_3, NULL },
	{ "output.frequency", 0, 0.1, MGE_BASE_OID ".7.2.1.3.0", "", SU_OUTPUT_1, NULL },
	{ "output.L1.frequency", 0, 0.1, MGE_BASE_OID ".7.2.1.3.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.frequency", 0, 0.1, MGE_BASE_OID ".7.2.1.3.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.frequency", 0, 0.1, MGE_BASE_OID ".7.2.1.3.3", "", SU_OUTPUT_3, NULL },
	{ "output.current", 0, 0.1, MGE_BASE_OID ".7.2.1.5.0", "", SU_OUTPUT_1, NULL },
	{ "output.L1.current", 0, 0.1, MGE_BASE_OID ".7.2.1.5.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 0.1, MGE_BASE_OID ".7.2.1.5.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 0.1, MGE_BASE_OID ".7.2.1.5.3", "", SU_OUTPUT_3, NULL },

	/* Battery page */
	{ "battery.charge", 0, 1, MGE_BASE_OID ".5.2.0", "", SU_FLAG_OK, NULL },
	{ "battery.runtime", 0, 1, MGE_BASE_OID ".5.1.0", "", SU_FLAG_OK, NULL },
	{ "battery.charge.low", ST_FLAG_STRING | ST_FLAG_RW, 2, MGE_BASE_OID ".4.8.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },	
	{ "battery.voltage", 0, 0.1, MGE_BASE_OID ".5.5.0", "", SU_FLAG_OK, NULL },

	/* Ambient page: Environment Sensor (ref 66 846) */
	{ "ambient.temperature", 0, 0.1, MGE_BASE_OID ".8.1.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "ambient.humidity", 0, 0.1, MGE_BASE_OID ".8.2.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },

	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "Main Outlet",  SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },

	/* instant commands. */
	{ "test.battery.start", 0, MGE_START_VALUE, MGE_BASE_OID ".10.4.0", "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
/*	{ "load.off", 0, MGE_START_VALUE, MGE_BASE_OID ".9.1.1.6.1", "", SU_TYPE_CMD | SU_FLAG_OK, NULL }, */
/*	{ "load.on", 0, MGE_START_VALUE, MGE_BASE_OID ".9.1.1.3.1", "", SU_TYPE_CMD | SU_FLAG_OK, NULL }, */
/*	{ "shutdown.return", 0, MGE_START_VALUE, MGE_BASE_OID ".9.1.1.9.1", "", SU_TYPE_CMD | SU_FLAG_OK, NULL }, */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};
