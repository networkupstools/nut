/*  apccmib.h - data to monitor APC SNMP devices (Powernet MIB) with NUT
 *
 *  Copyright (C) 2002-2003 
 *  			Dmitry Frolov <frolov@riss-telecom.ru>
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

#define APCC_MIB_VERSION	"1.1"

/* SNMP OIDs set */
#define APCC_OID_POWERNET_MIB	".1.3.6.1.4.1.318"
/* info elements */
#define APCC_OID_MODEL_NAME		".1.3.6.1.4.1.318.1.1.1.1.1.1"
#define APCC_OID_UPSIDEN		".1.3.6.1.4.1.318.1.1.1.1.1.2"
#define APCC_OID_FIRMREV		".1.3.6.1.4.1.318.1.1.1.1.2.1"
#define APCC_OID_MFRDATE		".1.3.6.1.4.1.318.1.1.1.1.2.2"
#define APCC_OID_SERIAL			".1.3.6.1.4.1.318.1.1.1.1.2.3"
#define APCC_OID_BATT_STATUS	".1.3.6.1.4.1.318.1.1.1.2.1.1"
/* Defines for APCC_OID_BATT_STATUS */
info_lkp_t apcc_batt_info[] = {
	{ 1, "" },	/* unknown */
	{ 2, "" },	/* batteryNormal */
	{ 3, "LB" },	/* batteryLow */
	{ 0, "NULL" }
} ;

#define APCC_OID_BATTDATE		".1.3.6.1.4.1.318.1.1.1.2.1.3"
#define APCC_OID_BATT_CHARGE	".1.3.6.1.4.1.318.1.1.1.2.2.1"
#define APCC_OID_UPSTEMP		".1.3.6.1.4.1.318.1.1.1.2.2.2"
#define APCC_OID_BATT_RUNTIME	".1.3.6.1.4.1.318.1.1.1.2.2.3"
#define APCC_OID_INVOLT			".1.3.6.1.4.1.318.1.1.1.3.2.1"
#define APCC_OID_INFREQ			".1.3.6.1.4.1.318.1.1.1.3.2.4"
#define APCC_OID_POWER_STATUS	".1.3.6.1.4.1.318.1.1.1.4.1.1"

/* Defines for APCC_OID_POWER_STATUS */
info_lkp_t apcc_pwr_info[] = {
	{ 1, "" },		/* other  */
	{ 2, "OL" },		/* normal */
	{ 3, "OB" },		/* battery */
	{ 4, "BOOST" },	/* booster */
	{ 5, "OFF" },		/* timedSleeping */
	{ 6, "OFF" },		/* bypass  */
	{ 7, "" },		/* none */
	{ 8, "" },		/* rebooting */
	{ 9, "" },		/* Pwr Hard Bypass */
	{ 10, "" },		/* Pwr Fail Bypass */
	{ 11, "OFF" },	/* sleepingUntilPowerReturn */
	{ 12, "TRIM" },	/* reducer */
	{ 0, "NULL" }
} ;

#define APCC_OID_OUTVOLT		".1.3.6.1.4.1.318.1.1.1.4.2.1"
#define APCC_OID_LOADPCT		".1.3.6.1.4.1.318.1.1.1.4.2.3"

/* PowerNet-MIB::upsAdvConfigHighTransferVolt */
#define APCC_OID_HIGHXFER		".1.3.6.1.4.1.318.1.1.1.5.2.2"

/* PowerNet-MIB::upsAdvConfigLowTransferVolt */
#define APCC_OID_LOWXFER		".1.3.6.1.4.1.318.1.1.1.5.2.3"

/* --- future OIDs which are not mapped to anything yet */

/* PowerNet-MIB::upsAdvInputMaxLineVoltage */
#define FUTURE_OID_MAX_INVOLT		".1.3.6.1.4.1.318.1.1.1.3.2.2"

/* PowerNet-MIB::upsAdvInputMinLineVoltage */
#define FUTURE_OID_MIN_INVOLT		".1.3.6.1.4.1.318.1.1.1.3.2.3"

/* --- */

#define APCC_OID_SLFTSTRES		".1.3.6.1.4.1.318.1.1.1.7.2.3"
/* XXX can't find appropriate OID for INFO_BATTVOLT. */
/*#define APCC_OID_BATT_VOLTAGE	".1.3.6.1.4.1.318.???"*/
/* commands */
#define APCC_OID_OFF			".1.3.6.1.4.1.318.1.1.1.6.2.1"
#define APCC_OFF_DO 2
#define APCC_OFF_GRACEFUL 3
#define APCC_OID_REBOOT			".1.3.6.1.4.1.318.1.1.1.6.2.2"
#define APCC_REBOOT_DO			2
#define APCC_REBOOT_GRACEFUL	3
#if 0	/* not used. */
	#define APCC_OID_SLEEP		".1.3.6.1.4.1.318.1.1.1.6.2.3"
	#define APCC_SLEEP_ON			"2"
	#define APCC_SLEEP_GRACEFUL		"3"
#endif
#define APCC_OID_SIMPWF			".1.3.6.1.4.1.318.1.1.1.6.2.4"
#define APCC_SIMPWF_DO			2
#define APCC_OID_FPTEST			".1.3.6.1.4.1.318.1.1.1.6.2.5"
#define APCC_FPTEST_DO			2
#define APCC_OID_ON				".1.3.6.1.4.1.318.1.1.1.6.2.6"
#define APCC_ON_DO				2
#define APCC_OID_BYPASS			".1.3.6.1.4.1.318.1.1.1.6.2.7"
#define APCC_BYPASS_ON			2
#define APCC_BYPASS_OFF			3
#define APCC_OID_SELFTEST		".1.3.6.1.4.1.318.1.1.1.7.2.2"
#define APCC_SELFTEST_DO		2
#define APCC_OID_CAL			".1.3.6.1.4.1.318.1.1.1.7.2.5"
#define APCC_CAL_DO				2
#define APCC_CAL_CANCEL			3
#define APCC_OID_CAL_RESULTS	".1.3.6.1.4.1.318.1.1.1.7.2.6"
#define APCC_CAL_OK				1
#define APCC_CAL_INVALID		2
#define APCC_CAL_INPROGRESS		3
/*#define APCC_OID_OUTPUT_TAB	"XXX"*/
#define APCC_OID_OUTCURRENT		".1.3.6.1.4.1.318.1.1.1.4.2.4"
#define APCC_OID_REQOUTVOLT		".1.3.6.1.4.1.318.1.1.1.5.2.1"
#define APCC_OID_RETCAPACITY	".1.3.6.1.4.1.318.1.1.1.5.2.6"
#define APCC_OID_CONSERVE		".1.3.6.1.4.1.318.1.1.1.6.1.1"
#define APCC_CONSERVE_DO		2
#define APCC_OID_NEEDREPLBATT	".1.3.6.1.4.1.318.1.1.1.2.2.4"
#define APCC_RB_NONEED			1
#define APCC_RB_NEED			2
#define APCC_OID_SENS			".1.3.6.1.4.1.318.1.1.1.5.2.7"
#define APCC_OID_GRACEDELAY		".1.3.6.1.4.1.318.1.1.1.5.2.10"
#define APCC_OID_RETDELAY		".1.3.6.1.4.1.318.1.1.1.5.2.9"
#define APCC_OID_LOBATTIME		".1.3.6.1.4.1.318.1.1.1.5.2.8"
/* Environmental sensors (AP9612TH and others) */
#define APCC_OID_AMBTEMP		".1.3.6.1.4.1.318.1.1.2.1.1"
#define APCC_OID_AMBHUMID		".1.3.6.1.4.1.318.1.1.2.1.2"

/* IEM: integrated environment monitor probe */

#define APCC_OID_IEM_TEMP       ".1.3.6.1.4.1.318.1.1.10.2.3.2.1.4"
#define APCC_OID_IEM_TEMP_UNIT  ".1.3.6.1.4.1.318.1.1.10.2.3.2.1.5"
#define TEMP_UNIT_FAHRENHEIT	2
#define APCC_OID_IEM_HUMID      ".1.3.6.1.4.1.318.1.1.10.2.3.2.1.6"

snmp_info_t apcc_mib[] = {

	/* info elements. */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "APC",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_MODEL_NAME,
		"Generic Powernet SNMP device", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_SERIAL, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_BATTDATE, "",
		SU_FLAG_OK | SU_FLAG_STATIC, NULL },
	{ "input.voltage", 0, 1, APCC_OID_INVOLT, "", SU_FLAG_OK, NULL },
	{ "battery.charge", 0, 1, APCC_OID_BATT_CHARGE, "", SU_FLAG_OK, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_POWER_STATUS, "OFF",
		SU_FLAG_OK | SU_STATUS_PWR, &apcc_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_BATT_STATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, &apcc_batt_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_CAL_RESULTS, "",
		SU_FLAG_OK | SU_STATUS_CAL, NULL },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_NEEDREPLBATT, "",
		SU_FLAG_OK | SU_STATUS_RB, NULL },
	{ "ups.temperature", 0, 1, APCC_OID_UPSTEMP, "", SU_FLAG_OK, NULL },
	{ "input.frequency", 0, 1, APCC_OID_INFREQ, "", SU_FLAG_OK, NULL },
	{ "ups.load", 0, 1, APCC_OID_LOADPCT, "", SU_FLAG_OK, NULL },
	{ "ups.firmware", ST_FLAG_STRING, 16, APCC_OID_FIRMREV, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "battery.runtime", 0, 1, APCC_OID_BATT_RUNTIME, "", SU_FLAG_OK, NULL },
	/* can't find appropriate OID for "battery.voltage". */
	/*{ "battery.voltage", 0, 1, APCC_OID_BATT_VOLTAGE, "", SU_FLAG_OK, NULL },*/
	{ "output.voltage", 0, 1, APCC_OID_OUTVOLT, "", SU_FLAG_OK, NULL },
	{ "ups.id", ST_FLAG_STRING | ST_FLAG_RW, 8, APCC_OID_UPSIDEN, "",
		SU_FLAG_OK | SU_FLAG_STATIC | SU_TYPE_STRING, NULL },
	{ "battery.date", ST_FLAG_STRING | ST_FLAG_RW, 8, APCC_OID_MFRDATE, "",
		SU_FLAG_OK | SU_FLAG_STATIC | SU_TYPE_STRING, NULL },
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_SLFTSTRES, "",
		SU_FLAG_OK, NULL },
	{ "input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW, 3, APCC_OID_LOWXFER, "",
		SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW, 3, APCC_OID_HIGHXFER, "",
		SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "output.current", 0, 0, APCC_OID_OUTCURRENT, "", SU_FLAG_OK, NULL },
	{ "output.voltage.target.battery", ST_FLAG_STRING | ST_FLAG_RW, 3, APCC_OID_REQOUTVOLT, "",
		SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "battery.charge.restart", ST_FLAG_STRING | ST_FLAG_RW, 3, APCC_OID_RETCAPACITY, "",
		SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "input.sensitivity", ST_FLAG_STRING | ST_FLAG_RW, 1, APCC_OID_SENS, "",
		SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 3, APCC_OID_GRACEDELAY, "",
		SU_FLAG_OK, NULL },
	{ "ups.delay.start", ST_FLAG_STRING | ST_FLAG_RW, 3, APCC_OID_RETDELAY, "",
		SU_FLAG_OK, NULL },
	{ "battery.charge.low", ST_FLAG_STRING | ST_FLAG_RW, 3, APCC_OID_LOBATTIME, "",
		SU_FLAG_OK, NULL },

	/* Measure-UPS ambient variables */
	{ "ambient.temperature", 0, 1, APCC_OID_AMBTEMP, "", SU_FLAG_OK, NULL },
	{ "ambient.humidity", 0, 1, APCC_OID_AMBHUMID, "", SU_FLAG_OK, NULL },

	/* IEM ambient variables */
	{ "ambient.temperature", 0, 1, APCC_OID_IEM_TEMP, "", SU_FLAG_OK, NULL },
	{ "ambient.humidity", 0, 1, APCC_OID_IEM_HUMID, "", SU_FLAG_OK, NULL },

	/* instant commands. */
	{ "load.off", 0, APCC_OFF_DO, APCC_OID_OFF, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "load.on", 0, APCC_ON_DO, APCC_OID_ON, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "shutdown.stayoff", 0, APCC_OFF_GRACEFUL, APCC_OID_OFF, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },

/*	{ CMD_SDRET, 0, APCC_REBOOT_GRACEFUL, APCC_OID_REBOOT, "", SU_TYPE_CMD | SU_FLAG_OK, NULL }, */
	
	{ "shutdown.return", 0, APCC_CONSERVE_DO, APCC_OID_CONSERVE, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "test.failure.start", 0, APCC_SIMPWF_DO, APCC_OID_SIMPWF, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "test.panel.start", 0, APCC_FPTEST_DO, APCC_OID_FPTEST, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "bypass.start", 0, APCC_BYPASS_ON, APCC_OID_BYPASS, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "test.battery.start", 0, APCC_SELFTEST_DO, APCC_OID_SELFTEST, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "calibrate.stop", 0, APCC_CAL_CANCEL, APCC_OID_CAL, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "calibrate.start", 0, APCC_CAL_DO, APCC_OID_CAL, "", SU_TYPE_CMD | SU_FLAG_OK, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};
