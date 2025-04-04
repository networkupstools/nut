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

#define APCC_MIB_VERSION	"1.60"

#define APC_UPS_DEVICE_MODEL	".1.3.6.1.4.1.318.1.1.1.1.1.1.0"
/* FIXME: Find a better oid_auto_check vs. sysOID for this one? */
#define APC_UPS_SYSOID	APC_UPS_DEVICE_MODEL

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
	info_lkp_default(1, ""),	/* unknown */
	info_lkp_default(2, ""),	/* batteryNormal */
	info_lkp_default(3, "LB"),	/* batteryLow */
	info_lkp_default(4, "LB"),	/* batteryInFaultCondition */
	info_lkp_default(5, "LB"),	/* noBatteryPresent */
	info_lkp_sentinel
};

#define APCC_OID_POWER_STATUS	".1.3.6.1.4.1.318.1.1.1.4.1.1.0"
/* Defines for APCC_OID_POWER_STATUS */
static info_lkp_t apcc_pwr_info[] = {
	info_lkp_default(1, ""),	/* unknown  */
	info_lkp_default(2, "OL"),	/* onLine */
	info_lkp_default(3, "OB"),	/* onBattery */
	info_lkp_default(4, "OL BOOST"),	/* onSmartBoost */
	info_lkp_default(5, "OFF"),	/* timedSleeping */
	info_lkp_default(6, "OFF"),	/* softwareBypass  */
	info_lkp_default(7, "OFF"),	/* off */
	info_lkp_default(8, ""),	/* rebooting */
	info_lkp_default(9, "BYPASS"),	/* switchedBypass */
	info_lkp_default(10, "BYPASS"),	/* hardwareFailureBypass */
	info_lkp_default(11, "OFF"),	/* sleepingUntilPowerReturn */
	info_lkp_default(12, "OL TRIM"),	/* onSmartTrim */
	info_lkp_default(13, "OL ECO"),	/* ecoMode */
	info_lkp_default(14, "OL"),	/* hotStandby */
	info_lkp_default(15, "OL"),	/* onBatteryTest */
	info_lkp_default(16, "BYPASS"),	/* emergencyStaticBypass */
	info_lkp_default(17, "BYPASS"),	/* staticBypassStandby */
	info_lkp_default(18, ""),	/* powerSavingMode */
	info_lkp_default(19, "OL"),	/* spotMode */
	info_lkp_default(20, "OL ECO"),	/* eConversion */
	info_lkp_default(21, "OL"),	/* chargerSpotmode */
	info_lkp_default(22, "OL"),	/* inverterSpotmode */
	info_lkp_default(23, ""),	/* activeLoad */
	info_lkp_default(24, "OL"),	/* batteryDischargeSpotmode */
	info_lkp_default(25, "OL"),	/* inverterStandby */
	info_lkp_default(26, ""),	/* chargerOnly */
	info_lkp_default(27, ""),	/* distributedEnergyReserve */
	info_lkp_default(28, "OL"),	/* selfTest */
	info_lkp_sentinel
} ;

#define APCC_OID_CAL_RESULTS	".1.3.6.1.4.1.318.1.1.1.7.2.6.0"
static info_lkp_t apcc_cal_info[] = {
	info_lkp_default(1, ""),	/* Calibration Successful */
	info_lkp_default(2, ""),	/* Calibration not done, battery capacity below 100% */
	info_lkp_default(3, "CAL"),	/* Calibration in progress */
	info_lkp_default(4, ""),	/* Calibration not done, refused */
	info_lkp_default(5, ""),	/* Calibration canceled by user or error */
	info_lkp_default(6, ""),	/* Calibration pending, about to start */
	info_lkp_sentinel
};

#define APCC_OID_NEEDREPLBATT	".1.3.6.1.4.1.318.1.1.1.2.2.4.0"
static info_lkp_t apcc_battrepl_info[] = {
	info_lkp_default(1, ""),	/* No battery needs replacing */
	info_lkp_default(2, "RB"),	/* Batteries need to be replaced */
	info_lkp_sentinel
};

#define APCC_OID_TESTDIAGRESULTS ".1.3.6.1.4.1.318.1.1.1.7.2.3.0"
static info_lkp_t apcc_testdiag_results[] = {
	info_lkp_default(1, "Ok"),
	info_lkp_default(2, "Failed"),
	info_lkp_default(3, "InvalidTest"),
	info_lkp_default(4, "TestInProgress"),
	info_lkp_sentinel
};

#define APCC_OID_SENSITIVITY ".1.3.6.1.4.1.318.1.1.1.5.2.7.0"
static info_lkp_t apcc_sensitivity_modes[] = {
	info_lkp_default(1, "auto"),
	info_lkp_default(2, "low"),
	info_lkp_default(3, "medium"),
	info_lkp_default(4, "high"),
	info_lkp_sentinel
};

#define APCC_OID_TRANSFERREASON "1.3.6.1.4.1.318.1.1.1.3.2.5.0"
static info_lkp_t apcc_transfer_reasons[] = {
	info_lkp_default(1, "noTransfer"),
	info_lkp_default(2, "highLineVoltage"),
	info_lkp_default(3, "brownout"),
	info_lkp_default(4, "blackout"),
	info_lkp_default(5, "smallMomentarySag"),
	info_lkp_default(6, "deepMomentarySag"),
	info_lkp_default(7, "smallMomentarySpike"),
	info_lkp_default(8, "largeMomentarySpike"),
	info_lkp_default(9, "selfTest"),
	info_lkp_default(10, "rateOfVoltageChange"),
	info_lkp_sentinel
};

/* --- */

/* commands */
#define APCC_OID_REBOOT			".1.3.6.1.4.1.318.1.1.1.6.2.2"
#define APCC_REBOOT_DO			2
#define APCC_REBOOT_GRACEFUL	3
#if 0	/* not used. */
#	define APCC_OID_SLEEP		".1.3.6.1.4.1.318.1.1.1.6.2.3"
#	define APCC_SLEEP_ON			"2"
#	define APCC_SLEEP_GRACEFUL		"3"
#endif



static snmp_info_t apcc_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* info elements. */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "APC",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.1.1.1.1.0", "Generic Powernet SNMP device", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.1.1.2.3.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.318.1.1.1.1.2.2.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL),
	snmp_info_default("input.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.1.0", "", SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE, NULL),
	snmp_info_default("input.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.2.0", "", SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE, NULL),
	snmp_info_default("input.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.3.0", "", SU_FLAG_OK | SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE, NULL),
	snmp_info_default("input.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.1.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("input.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.2.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("input.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.3.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("input.phases", ST_FLAG_STRING, 2, ".1.3.6.1.4.1.318.1.1.1.9.2.2.1.2.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("input.L1-L2.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.3.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L2-L3.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.3.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L3-L1.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.3.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L1-L2.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.4.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L2-L3.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.4.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L3-L1.voltage.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.4.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L1-L2.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.5.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L2-L3.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.5.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L3-L1.voltage.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.5.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L1.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.6.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L2.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.6.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L3.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.6.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L1.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.7.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L2.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.7.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L3.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.7.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L1.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.8.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L2.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.8.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.L3.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.3.1.8.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.2.2.1.4.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("input.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.3.3.4.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("input.frequency", 0, 1, ".1.3.6.1.4.1.318.1.1.1.3.2.4.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.3.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL),
	snmp_info_default("input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.2.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL),
	snmp_info_default("input.transfer.reason", ST_FLAG_STRING, 1, APCC_OID_TRANSFERREASON, "", SU_TYPE_INT | SU_FLAG_OK, apcc_transfer_reasons),
	snmp_info_default("input.sensitivity", ST_FLAG_STRING | ST_FLAG_RW, 1, APCC_OID_SENSITIVITY, "", SU_TYPE_INT | SU_FLAG_OK, apcc_sensitivity_modes),
	snmp_info_default("ups.power", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.9.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ups.realpower", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.8.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_POWER_STATUS, "OFF",
		SU_FLAG_OK | SU_STATUS_PWR, apcc_pwr_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_BATT_STATUS, "",
		SU_FLAG_OK | SU_STATUS_BATT, apcc_batt_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_CAL_RESULTS, "",
		SU_FLAG_OK | SU_STATUS_CAL, apcc_cal_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_NEEDREPLBATT, "",
		SU_FLAG_OK | SU_STATUS_RB, apcc_battrepl_info),
	snmp_info_default("ups.temperature", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.2.0", "", SU_FLAG_OK|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("ups.temperature", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.2.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ups.load", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.3.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("ups.load", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.3.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ups.firmware", ST_FLAG_STRING, 16, ".1.3.6.1.4.1.318.1.1.1.1.2.1.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.delay.shutdown", ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.10.0", "", SU_TYPE_TIME | SU_FLAG_OK, NULL),
	snmp_info_default("ups.delay.start", ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.9.0", "", SU_TYPE_TIME | SU_FLAG_OK, NULL),
	snmp_info_default("battery.charge", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.1.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("battery.charge", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.1.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.charge.restart", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.6.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL),
	snmp_info_default("battery.runtime", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.3.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.runtime.low", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.8.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.4.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("battery.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.8.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.voltage.nominal", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.7.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.5.0", "", SU_FLAG_OK|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("battery.current", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.9.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.current.total", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.2.3.6.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.packs", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.5.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.packs.bad", 0, 1, ".1.3.6.1.4.1.318.1.1.1.2.2.6.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.date", ST_FLAG_STRING | ST_FLAG_RW, 8, ".1.3.6.1.4.1.318.1.1.1.2.1.3.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.id", ST_FLAG_STRING | ST_FLAG_RW, 8, ".1.3.6.1.4.1.318.1.1.1.1.1.2.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, APCC_OID_TESTDIAGRESULTS, "", SU_FLAG_OK, apcc_testdiag_results),
	snmp_info_default("ups.test.date", ST_FLAG_STRING | ST_FLAG_RW, 8, ".1.3.6.1.4.1.318.1.1.1.7.2.4.0", "", SU_FLAG_OK | SU_FLAG_STATIC, NULL),
	snmp_info_default("output.voltage", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.1.0", "", SU_FLAG_OK | SU_FLAG_UNIQUE, NULL),
	snmp_info_default("output.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.1.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("output.phases", ST_FLAG_STRING, 2, ".1.3.6.1.4.1.318.1.1.1.9.3.2.1.2.1", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("output.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.2.1.4.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("output.frequency", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.2.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("output.frequency", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.2.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("output.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.4.3.4.0", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("output.current", 0, 1, ".1.3.6.1.4.1.318.1.1.1.4.2.4.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("output.L1-L2.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.3.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2-L3.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.3.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3-L1.voltage", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.3.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.4.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.4.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.current", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.4.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.5.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.5.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.current.maximum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.5.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.6.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.6.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.current.minimum", 0, 0.1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.6.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.power", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.7.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.power", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.7.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.power", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.7.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.power.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.8.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.power.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.8.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.power.maximum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.8.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.power.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.9.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.power.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.9.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.power.minimum", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.9.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.power.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.10.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.power.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.10.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.power.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.10.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.power.maximum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.11.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.power.maximum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.11.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.power.maximum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.11.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L1.power.minimum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.12.1.1.1", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L2.power.minimum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.12.1.1.2", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.L3.power.minimum.percent", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.3.3.1.12.1.1.3", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.voltage.nominal", ST_FLAG_STRING | ST_FLAG_RW, 3, ".1.3.6.1.4.1.318.1.1.1.5.2.1.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL),

	/* Measure-UPS ambient variables */
	/* Environmental sensors (AP9612TH and others) */
	snmp_info_default("ambient.temperature", 0, 1, ".1.3.6.1.4.1.318.1.1.2.1.1.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ambient.1.temperature.alarm.high", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.3.1", "", SU_FLAG_OK, NULL),
	snmp_info_default("ambient.1.temperature.alarm.low", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.4.1", "", SU_FLAG_OK, NULL),
	snmp_info_default("ambient.humidity", 0, 1, ".1.3.6.1.4.1.318.1.1.2.1.2.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ambient.1.humidity.alarm.high", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.6.1", "", SU_FLAG_OK, NULL),
	snmp_info_default("ambient.1.humidity.alarm.low", 0, 1, ".1.3.6.1.4.1.318.1.1.10.1.2.2.1.7.1", "", SU_FLAG_OK, NULL),

	/* IEM ambient variables */
	/* IEM: integrated environment monitor probe */
	snmp_info_default("ambient.temperature", 0, 1, APCC_OID_IEM_TEMP, "", SU_FLAG_OK, NULL),
	snmp_info_default("ambient.humidity", 0, 1, APCC_OID_IEM_HUMID, "", SU_FLAG_OK, NULL),

	/* instant commands. */
	snmp_info_default("load.off", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("load.on", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.6.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("shutdown.stayoff", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.1.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL),

/*	snmp_info_default(CMD_SDRET, 0, APCC_REBOOT_GRACEFUL, APCC_OID_REBOOT, "", SU_TYPE_CMD | SU_FLAG_OK, NULL), */

	snmp_info_default("shutdown.return", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.1.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("test.failure.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.4.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("test.panel.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.5.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("bypass.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.7.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("bypass.stop", 0, 1, ".1.3.6.1.4.1.318.1.1.1.6.2.7.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("test.battery.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.7.2.2.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("calibrate.start", 0, 1, ".1.3.6.1.4.1.318.1.1.1.7.2.5.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("calibrate.stop", 0, 1, ".1.3.6.1.4.1.318.1.1.1.7.2.5.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL),
	snmp_info_default("reset.input.minmax", 0, 1, ".1.3.6.1.4.1.318.1.1.1.9.1.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL),

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	apc = { "apcc", APCC_MIB_VERSION, APCC_OID_POWER_STATUS, APC_UPS_DEVICE_MODEL, apcc_mib, APC_UPS_SYSOID, NULL };

/*
vim:ts=4:sw=4:et:
*/
