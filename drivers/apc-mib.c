/*  apc-mib.c - data to monitor APC SNMP devices (Powernet MIB) with NUT
 *
 *  Copyright (C)
 *    2002-2003 - Dmitry Frolov <frolov@riss-telecom.ru>
 *    2002-2012 - Arnaud Quette <arnaud.quette@free.fr>
 *    2012 - Chew Hong Gunn <hglinux@gunnet.org> (high precision values)
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

#include "apc-mib.h"

#define APCC_MIB_VERSION	"1.3"

#define APC_UPS_DEVICE_MODEL	".1.3.6.1.4.1.318.1.1.1.1.1.1.0"

/* Other APC sysOID:
 *
 * examples found on the Net and other sources:
 *   'enterprises.apc.products.system.smartUPS.smartUPS700'
 *  - from fence agents
 *   '.1.3.6.1.4.1.318.1.3.4.5': ApcRPDU,
 *   '.1.3.6.1.4.1.318.1.3.4.4': ApcMSP
 *  - from Bill Seligman
 *   .1.3.6.1.4.1.318.1.3.2.11
 *   .1.3.6.1.4.1.318.1.3.2.12
 *   .1.3.6.1.4.1.318.1.3.27
 *   .1.3.6.1.4.1.318.1.3.2.7
 *   .1.3.6.1.4.1.318.1.3.2.8
 */

/* TODO: find the right sysOID for this MIB
 * Ie ".1.3.6.1.4.1.318.1.1.1" or ".1.3.6.1.4.1.318" or? */

/*     .1.3.6.1.4.1.318.1.1.1
 *       enterprise^
 *       apc ---------^
 *       products ------^
 *       hardware --------^
 *       ups ---------------^
 *  ref: ftp://ftp.apc.com/apc/public/software/pnetmib/mib/404/powernet404.mib
 */

/* info elements */

#define APCC_OID_BATT_STATUS	".1.3.6.1.4.1.318.1.1.1.2.1.1.0"
/* Defines for APCC_OID_BATT_STATUS */
static info_lkp_t apcc_batt_info[] = {
	{ 1, "", NULL, NULL },	/* unknown */
	{ 2, "", NULL, NULL },	/* batteryNormal */
	{ 3, "LB", NULL, NULL },	/* batteryLow */
	{ 0, NULL, NULL, NULL }
} ;

#define APCC_OID_POWER_STATUS	".1.3.6.1.4.1.318.1.1.1.4.1.1.0"
/* Defines for APCC_OID_POWER_STATUS */
static info_lkp_t apcc_pwr_info[] = {
    { 1, "", NULL, NULL },          /* unknown  */
    { 2, "OL", NULL, NULL },        /* onLine */
    { 3, "OB", NULL, NULL },        /* onBattery */
    { 4, "OL BOOST", NULL, NULL },     /* onSmartBoost */
    { 5, "OFF", NULL, NULL },       /* timedSleeping */
    { 6, "OFF", NULL, NULL },       /* softwareBypass  */
    { 7, "OFF", NULL, NULL },       /* off */
    { 8, "", NULL, NULL },          /* rebooting */
    { 9, "BYPASS", NULL, NULL },    /* switchedBypass */
    { 10, "BYPASS", NULL, NULL },   /* hardwareFailureBypass */
    { 11, "OFF", NULL, NULL },      /* sleepingUntilPowerReturn */
    { 12, "OL TRIM", NULL, NULL },     /* onSmartTrim */
    { 0, NULL, NULL, NULL }
} ;

#define APCC_OID_CAL_RESULTS	".1.3.6.1.4.1.318.1.1.1.7.2.6.0"
static info_lkp_t apcc_cal_info[] = {
    { 1, "", NULL, NULL },          /* Calibration Successful */
    { 2, "", NULL, NULL },          /* Calibration not done, battery capacity below 100% */
    { 3, "CAL", NULL, NULL },       /* Calibration in progress */
    { 0, NULL, NULL, NULL }
};

#define APCC_OID_NEEDREPLBATT	".1.3.6.1.4.1.318.1.1.1.2.2.4.0"
static info_lkp_t apcc_battrepl_info[] = {
    { 1, "", NULL, NULL },          /* No battery needs replacing */
    { 2, "RB", NULL, NULL },        /* Batteries need to be replaced */
    { 0, NULL, NULL, NULL }
};

#define APCC_OID_TESTDIAGRESULTS ".1.3.6.1.4.1.318.1.1.1.7.2.3.0"
static info_lkp_t apcc_testdiag_results[] = {
    { 1, "Ok", NULL, NULL },
    { 2, "Failed", NULL, NULL },
    { 3, "InvalidTest", NULL, NULL },
    { 4, "TestInProgress", NULL, NULL },
    { 0, NULL, NULL, NULL }
};

#define APCC_OID_SENSITIVITY ".1.3.6.1.4.1.318.1.1.1.5.2.7.0"
static info_lkp_t apcc_sensitivity_modes[] = {
    { 1, "auto", NULL, NULL },
    { 2, "low", NULL, NULL },
    { 3, "medium", NULL, NULL },
    { 4, "high", NULL, NULL },
    { 0, NULL, NULL, NULL }
};

#define APCC_OID_TRANSFERREASON "1.3.6.1.4.1.318.1.1.1.3.2.5.0"
static info_lkp_t apcc_transfer_reasons[] = {
    { 1, "noTransfer", NULL, NULL },
    { 2, "highLineVoltage", NULL, NULL },
    { 3, "brownout", NULL, NULL },
    { 4, "blackout", NULL, NULL },
    { 5, "smallMomentarySag", NULL, NULL },
    { 6, "deepMomentarySag", NULL, NULL },
    { 7, "smallMomentarySpike", NULL, NULL },
    { 8, "largeMomentarySpike", NULL, NULL },
    { 9, "selfTest", NULL, NULL },
    { 10, "rateOfVoltageChange", NULL, NULL }
};

/* --- */

/* commands */
#define APCC_OID_REBOOT			".1.3.6.1.4.1.318.1.1.1.6.2.2"
#define APCC_REBOOT_DO			2
#define APCC_REBOOT_GRACEFUL	3
#if 0	/* not used. */
	#define APCC_OID_SLEEP		".1.3.6.1.4.1.318.1.1.1.6.2.3"
	#define APCC_SLEEP_ON			"2"
	#define APCC_SLEEP_GRACEFUL		"3"
#endif



static snmp_info_t apcc_mib[] = {

	/* info elements. */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "APC",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.1.1.1.1.0", "Generic Powernet SNMP device", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.1.1.2.3.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.1.1.2.2.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL },
	{ "input.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.1.0", "", SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE, NULL },
	{ "input.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.2.0", "", SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE, NULL },
	{ "input.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.3.0", "", SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE, NULL },
	{ "input.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.1.0", "", SU_FLAG_OK, NULL },
	{ "input.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.2.0", "", SU_FLAG_OK, NULL },
	{ "input.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.3.0", "", SU_FLAG_OK, NULL },
	{ "input.phases", ST_FLAG_STRING, 2, ".1.3.6.1.4.1.318.1.1.1.9.2.2.1.2.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "input.L1-L2.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.3.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L2-L3.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.3.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L3-L1.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.3.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L1-L2.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.4.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L2-L3.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.4.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L3-L1.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.4.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L1-L2.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.5.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L2-L3.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.5.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L3-L1.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.5.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.6.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.6.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.6.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.7.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.7.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.7.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.8.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.8.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.8.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.2.1.4.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.4.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "input.frequency", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.4.0", "", SU_FLAG_OK, NULL },
	{ "input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.3.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.2.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },
    { "input.transfer.reason", ST_FLAG_STRING, 1, APCC_OID_TRANSFERREASON, "", SU_TYPE_INT | SU_FLAG_OK, apcc_transfer_reasons },
	{ "input.sensitivity", ST_FLAG_STRING | ST_FLAG_RW, 1, APCC_OID_SENSITIVITY, "", SU_TYPE_INT | SU_FLAG_OK, apcc_sensitivity_modes },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_POWER_STATUS, "OFF",
		SU_FLAG_OK | SU_STATUS_PWR, apcc_pwr_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_BATT_STATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, apcc_batt_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_CAL_RESULTS, "",
		SU_FLAG_OK | SU_STATUS_CAL, apcc_cal_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_NEEDREPLBATT, "",
		SU_FLAG_OK | SU_STATUS_RB, apcc_battrepl_info },
	{ "ups.temperature", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.2.0", "", SU_FLAG_OK|SU_FLAG_UNIQUE, NULL },
	{ "ups.temperature", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.2.0", "", SU_FLAG_OK, NULL },
	{ "ups.load", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.3.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "ups.load", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.3.0", "", SU_FLAG_OK, NULL },
	{ "ups.firmware", ST_FLAG_STRING, 16, ".1.3.6.1.4.1.318.1.1.1.1.2.1.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.10.0", "", SU_FLAG_OK, NULL },
	{ "ups.delay.start", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.9.0", "", SU_FLAG_OK, NULL },
	{ "battery.charge", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.1.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "battery.charge", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.1.0", "", SU_FLAG_OK, NULL },
	{ "battery.charge.restart", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.6.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "battery.runtime", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.3.0", "", SU_FLAG_OK, NULL },
	{ "battery.runtime.low", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.8.0", "", SU_FLAG_OK, NULL },
	{ "battery.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.4.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "battery.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.8.0", "", SU_FLAG_OK, NULL },
	{ "battery.voltage.nominal", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.7.0", "", SU_FLAG_OK, NULL },
	{ "battery.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.5.0", "", SU_FLAG_OK|SU_FLAG_UNIQUE, NULL },
	{ "battery.current", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.9.0", "", SU_FLAG_OK, NULL },
	{ "battery.current.total", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.6.0", "", SU_FLAG_OK, NULL },
	{ "battery.packs", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.5.0", "", SU_FLAG_OK, NULL },
	{ "battery.packs.bad", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.6.0", "", SU_FLAG_OK, NULL },
	{ "battery.date", ST_FLAG_STRING | ST_FLAG_RW, 8, ".1.3.6.1.4.1.318.1.1.1.2.1.3.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL },
	{ "ups.id", ST_FLAG_STRING | ST_FLAG_RW, 8, ".1.3.6.1.4.1.318.1.1.1.1.1.2.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL },
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_TESTDIAGRESULTS, "", SU_FLAG_OK, apcc_testdiag_results },
	{ "ups.test.date", ST_FLAG_STRING | ST_FLAG_RW, 8, ".1.3.6.1.4.1.318.1.1.1.7.2.4.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL },
	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.1.0", "", SU_FLAG_OK | SU_FLAG_UNIQUE, NULL },
	{ "output.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.1.0", "", SU_FLAG_OK, NULL },
	{ "output.phases", ST_FLAG_STRING, 2, ".1.3.6.1.4.1.318.1.1.1.9.3.2.1.2.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "output.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.2.1.4.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "output.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.2.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "output.frequency", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.2.0", "", SU_FLAG_OK, NULL },
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.4.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL },
	{ "output.current", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.4.0", "", SU_FLAG_OK, NULL },
	{ "output.L1-L2.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.3.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2-L3.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.3.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3-L1.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.3.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.4.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.4.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.4.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.5.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.5.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.5.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.6.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.6.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.6.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.power", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.7.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.power", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.7.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.power", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.7.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.power.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.8.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.power.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.8.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.power.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.8.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.power.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.9.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.power.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.9.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.power.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.9.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.power.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.10.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.power.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.10.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.power.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.10.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.power.maximum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.11.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.power.maximum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.11.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.power.maximum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.11.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L1.power.minimum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.12.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L2.power.minimum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.12.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.L3.power.minimum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.12.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL },
	{ "output.voltage.nominal", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.1.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },

	/* Measure-UPS ambient variables */
/* Environmental sensors (AP9612TH and others) */
	{ "ambient.temperature", 0, 1, ".1.3.6.1.4.1.318.1.1.2.1.1.0", "", SU_FLAG_OK, NULL },
	{ "ambient.1.temperature.alarm.high", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.3.1", "", SU_FLAG_OK, NULL },
	{ "ambient.1.temperature.alarm.low", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.4.1", "", SU_FLAG_OK, NULL },
	{ "ambient.humidity", 0, 1, ".1.3.6.1.4.1.318.1.1.2.1.2.0", "", SU_FLAG_OK, NULL },
	{ "ambient.1.humidity.alarm.high", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.6.1", "", SU_FLAG_OK, NULL },
	{ "ambient.1.humidity.alarm.low", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.7.1", "", SU_FLAG_OK, NULL },

	/* IEM ambient variables */
/* IEM: integrated environment monitor probe */
#define APCC_OID_IEM_TEMP       ".1.3.6.1.4.1.318.1.1.10.2.3.2.1.4.1"
#define APCC_OID_IEM_TEMP_UNIT  ".1.3.6.1.4.1.318.1.1.10.2.3.2.1.5.1"
#define APCC_IEM_FAHRENHEIT	    2
#define APCC_OID_IEM_HUMID      ".1.3.6.1.4.1.318.1.1.10.2.3.2.1.6.1"
	{ "ambient.temperature", 0, 1, APCC_OID_IEM_TEMP, "", SU_FLAG_OK, NULL },
	{ "ambient.humidity", 0, 1, APCC_OID_IEM_HUMID, "", SU_FLAG_OK, NULL },

	/* instant commands. */
	{ "load.off", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "load.on", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.6.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "shutdown.stayoff", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.1.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL },

/*	{ CMD_SDRET, 0, APCC_REBOOT_GRACEFUL, APCC_OID_REBOOT, "", SU_TYPE_CMD | SU_FLAG_OK, NULL }, */

	{ "shutdown.return", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.1.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "test.failure.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.4.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "test.panel.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.5.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "bypass.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.7.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "bypass.stop", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.7.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "test.battery.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.7.2.2.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "calibrate.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.7.2.5.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "calibrate.stop", 0, 1, ".1.3.6.1.4.1.318.1.1.1.7.2.5.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "reset.input.minmax", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.1.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	apc = { "apcc", APCC_MIB_VERSION, APCC_OID_POWER_STATUS, APC_UPS_DEVICE_MODEL, apcc_mib };

/*
vim:ts=4:sw=4:et:
*/
