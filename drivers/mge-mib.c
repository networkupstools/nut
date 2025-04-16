/*  mge-mib.c - data to monitor MGE UPS SYSTEMS SNMP devices with NUT
 *
 *  Copyright (C)
 *  	2002-2012	Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *  	2002-2003	J.W. Hoogervorst <jeroen@hoogervorst.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
 *   MGE Office Protection Systems <http://www.mgeops.com>
 *   Eaton <http://powerquality.eaton.com>
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

#include "mge-mib.h"

#define MGE_MIB_VERSION	"0.55"

/* TODO:
 * - MGE PDU MIB and sysOID (".1.3.6.1.4.1.705.2") */

/* SNMP OIDs set */
#define MGE_BASE_OID		".1.3.6.1.4.1.705.1"
#define MGE_SYSOID			MGE_BASE_OID
#define MGE_OID_MODEL_NAME	MGE_BASE_OID ".1.1.0"

static info_lkp_t mge_lowbatt_info[] = {
	info_lkp_default(1, "LB"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t mge_onbatt_info[] = {
	info_lkp_default(1, "OB"),
	info_lkp_default(2, "OL"),
	info_lkp_sentinel
};

static info_lkp_t mge_bypass_info[] = {
	info_lkp_default(1, "BYPASS"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t mge_boost_info[] = {
	info_lkp_default(1, "BOOST"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t mge_trim_info[] = {
	info_lkp_default(1, "TRIM"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t mge_overload_info[] = {
	info_lkp_default(1, "OVER"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t mge_replacebatt_info[] = {
	info_lkp_default(1, "RB"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t mge_output_util_off_info[] = {
	info_lkp_default(1, "OFF"),
	info_lkp_default(2, ""),
	info_lkp_sentinel
};

static info_lkp_t mge_transfer_reason_info[] = {
	info_lkp_default(1, ""),
	info_lkp_default(2, "input voltage out of range"),
	info_lkp_default(3, "input frequency out of range"),
	info_lkp_default(4, "utility off"),
	info_lkp_sentinel
};

static info_lkp_t mge_test_result_info[] = {
	info_lkp_default(1, "done and passed"),
	info_lkp_default(2, "done and warning"),
	info_lkp_default(3, "done and error"),
	info_lkp_default(4, "aborted"),
	info_lkp_default(5, "in progress"),
	info_lkp_default(6, "no test initiated"),
	info_lkp_sentinel
};

static info_lkp_t mge_beeper_status_info[] = {
	info_lkp_default(1, "disabled"),
	info_lkp_default(2, "enabled"),
	info_lkp_default(3, "muted"),
	info_lkp_sentinel
};

static info_lkp_t mge_yes_no_info[] = {
	info_lkp_default(1, "yes"),
	info_lkp_default(2, "no"),
	info_lkp_sentinel
};

/* FIXME: the below may introduce status redundancy, that needs to be
 * addressed by the driver, as for usbhid-ups! */
static info_lkp_t mge_power_source_info[] = {
	info_lkp_default(1, ""),	/* other */
	info_lkp_default(2, "OFF"),	/* none */

#if 0
	info_lkp_default(3, "OL"),	/* normal */
#endif

	info_lkp_default(4, "BYPASS"),	/* bypass */
	info_lkp_default(5, "OB"),	/* battery */
	info_lkp_default(6, "BOOST"),	/* booster */
	info_lkp_default(7, "TRIM"),	/* reducer */
	info_lkp_sentinel
};

static info_lkp_t mge_ambient_drycontacts_info[] = {
	info_lkp_default(-1, "unknown"),
	info_lkp_default(1, "closed"),
	info_lkp_default(2, "opened"),
	info_lkp_sentinel
};

/* Parameters default values */
#define DEFAULT_ONDELAY		"30"	/* Delay between return of utility power */
										/* and powering up of load, in seconds */
										/* CAUTION: ondelay > offdelay */
#define DEFAULT_OFFDELAY	"20"	/* Delay before power off, in seconds */

#define MGE_NOTHING_VALUE	"1"
#define MGE_START_VALUE		"2"
#define MGE_STOP_VALUE		"3"

/* TODO: PowerShare (per plug .1, .2, .3) and deals with delays */

/* Snmp2NUT lookup table */
static snmp_info_t mge_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* UPS page */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.1.1.0", "Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.1.7.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.1.4.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.12.12.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.1", "", SU_OUTPUT_1, NULL),
	snmp_info_default("ups.beeper.status", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.2.1.33.1.9.8.0", "", 0, mge_beeper_status_info),
	snmp_info_default("ups.L1.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("ups.L2.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("ups.L3.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.3", "", SU_OUTPUT_3, NULL),
	snmp_info_default("ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.33.1.7.3.0", "", 0, mge_test_result_info),
	snmp_info_default("ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 6, "1.3.6.1.2.1.33.1.8.2.0", DEFAULT_OFFDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.delay.start", ST_FLAG_STRING | ST_FLAG_RW, 6, "1.3.6.1.2.1.33.1.8.3.0", DEFAULT_ONDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.timer.shutdown", 0, 1, "1.3.6.1.2.1.33.1.8.2.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ups.timer.start", 0, 1, "1.3.6.1.2.1.33.1.8.3.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ups.timer.reboot", 0, 1, "1.3.6.1.2.1.33.1.8.4.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("ups.start.auto", ST_FLAG_RW, 1, "1.3.6.1.2.1.33.1.8.5.0", "", SU_FLAG_OK, mge_yes_no_info),
	/* status data */
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.5.11.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_replacebatt_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.5.14.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_lowbatt_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.5.16.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_lowbatt_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_onbatt_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.4.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_bypass_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.7.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_output_util_off_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.8.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_boost_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.10.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_overload_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.12.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_trim_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.33.1.4.1.0", "", SU_STATUS_PWR | SU_FLAG_OK, mge_power_source_info),

	/* FIXME: Alarms
	 * - upsmgBatteryChargerFault (.1.3.6.1.4.1.705.1.5.15.0), yes (1), no (2)
	 * => Battery charger fail!
	 * - upsmgBatteryFaultBattery (.1.3.6.1.4.1.705.1.5.9.0), yes (1), no (2)
	 * => "Battery fault!" or?
	 * - upsmgOutputOverTemp (.1.3.6.1.4.1.705.1.7.11.0), yes (1), no (2)
	 * => "Temperature too high!"
	 * - upsmgOutputInverterOff (.1.3.6.1.4.1.705.1.7.9.0), yes (1), no (2)
	 * => "??"
	 * - upsmgInputBadStatus (.1.3.6.1.4.1.705.1.6.3.0), yes (1), no (2)
	 * => "bad volt or bad freq"
	 */

	/* Input page */
	snmp_info_default("input.phases", 0, 1.0, ".1.3.6.1.4.1.705.1.6.1.0", "", 0, NULL),
	snmp_info_default("input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL),
	snmp_info_default("input.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.1", "", SU_INPUT_1, NULL),
	snmp_info_default("input.L1.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.1", "", SU_INPUT_1, NULL),
	snmp_info_default("input.L1-N.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2-N.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3-N.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.1", "", SU_INPUT_1, NULL),
	snmp_info_default("input.L1-N.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2-N.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3-N.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.1", "", SU_INPUT_1, NULL),
	snmp_info_default("input.L1.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.1", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L2.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.2", "", SU_INPUT_3, NULL),
	snmp_info_default("input.L3.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.3", "", SU_INPUT_3, NULL),
	snmp_info_default("input.transfer.reason", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.6.4.0", "", SU_FLAG_OK, mge_transfer_reason_info),

	/* Output page */
	snmp_info_default("output.phases", 0, 1.0, ".1.3.6.1.4.1.705.1.7.1.0", "", 0, NULL),
	snmp_info_default("output.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.1", "", SU_OUTPUT_1, NULL),
	snmp_info_default("output.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.3", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.1", "", SU_OUTPUT_1, NULL),
	snmp_info_default("output.L1.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.3", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.1", "", SU_OUTPUT_1, NULL),
	snmp_info_default("output.L1.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.1", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L2.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.2", "", SU_OUTPUT_3, NULL),
	snmp_info_default("output.L3.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.3", "", SU_OUTPUT_3, NULL),

	/* Battery page */
	snmp_info_default("battery.charge", 0, 1, ".1.3.6.1.4.1.705.1.5.2.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.runtime", 0, 1, ".1.3.6.1.4.1.705.1.5.1.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.runtime.low", 0, 1, ".1.3.6.1.4.1.705.1.4.7.0", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.charge.low", ST_FLAG_STRING | ST_FLAG_RW, 2, ".1.3.6.1.4.1.705.1.4.8.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL),
	snmp_info_default("battery.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.5.5.0", "", SU_FLAG_OK, NULL),

	/* Ambient page: Environment Sensor (ref 66 846) */
	snmp_info_default("ambient.temperature", 0, 0.1, ".1.3.6.1.4.1.705.1.8.1.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL),
	snmp_info_default("ambient.humidity", 0, 0.1, ".1.3.6.1.4.1.705.1.8.2.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL),
	/* upsmgEnvironmentInput1State.1 */
	snmp_info_default("ambient.contacts.1.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.8.7.1.9.1", "", SU_TYPE_INT | SU_FLAG_OK, mge_ambient_drycontacts_info),
	/* upsmgEnvironmentInput1State.1 */
	snmp_info_default("ambient.contacts.2.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.8.7.1.10.1", "", SU_TYPE_INT | SU_FLAG_OK, mge_ambient_drycontacts_info),

	/* Outlet page */
	snmp_info_default("outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "Main Outlet",  SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),

	/* instant commands. */
	snmp_info_default("test.battery.start", 0, 1, ".1.3.6.1.4.1.705.1.10.4.0", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL),
	/* Also use IETF OIDs
	 * snmp_info_default("test.battery.stop", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.2", SU_TYPE_CMD, NULL),
	 * snmp_info_default("test.battery.start", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.3", SU_TYPE_CMD, NULL),
	 * snmp_info_default("test.battery.start.quick", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.4", SU_TYPE_CMD, NULL),
	 * snmp_info_default("test.battery.start.deep", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.5", SU_TYPE_CMD, NULL),
	 */

	/* snmp_info_default("load.off", 0, 1, ".1.3.6.1.4.1.705.1.9.1.1.6.1", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL),
	 * snmp_info_default("load.on", 0, 1, ".1.3.6.1.4.1.705.1.9.1.1.3.1", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL),
	 * snmp_info_default("shutdown.return", 0, 1, ".1.3.6.1.4.1.705.1.9.1.1.9.1", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL),
	 */
	/* IETF MIB fallback */
	snmp_info_default("beeper.disable", 0, 1, "1.3.6.1.2.1.33.1.9.8.0", "1", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.enable", 0, 1, "1.3.6.1.2.1.33.1.9.8.0", "2", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.mute", 0, 1, "1.3.6.1.2.1.33.1.9.8.0", "3", SU_TYPE_CMD, NULL),
/*
	snmp_info_default("load.off", 0, 1, "1.3.6.1.2.1.33.1.8.2.0", DEFAULT_OFFDELAY, SU_TYPE_CMD, NULL),
	snmp_info_default("load.on", 0, 1, "1.3.6.1.2.1.33.1.8.3.0", DEFAULT_ONDELAY, SU_TYPE_CMD, NULL),
*/
	snmp_info_default("load.off.delay", 0, 1, "1.3.6.1.2.1.33.1.8.2.0", NULL, SU_TYPE_CMD, NULL),
	snmp_info_default("load.on.delay", 0, 1, "1.3.6.1.2.1.33.1.8.3.0", NULL, SU_TYPE_CMD, NULL),

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	mge = { "mge", MGE_MIB_VERSION, NULL, MGE_OID_MODEL_NAME, mge_mib, MGE_SYSOID, NULL };
