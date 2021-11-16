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

#define MGE_MIB_VERSION	"0.52"

/* TODO:
 * - MGE PDU MIB and sysOID (".1.3.6.1.4.1.705.2") */

/* SNMP OIDs set */
#define MGE_BASE_OID		".1.3.6.1.4.1.705.1"
#define MGE_SYSOID			MGE_BASE_OID
#define MGE_OID_MODEL_NAME	MGE_BASE_OID ".1.1.0"

static info_lkp_t mge_lowbatt_info[] = {
	{ 1, "LB", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_onbatt_info[] = {
	{ 1, "OB", NULL, NULL },
	{ 2, "OL", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_bypass_info[] = {
	{ 1, "BYPASS", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_boost_info[] = {
	{ 1, "BOOST", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_trim_info[] = {
	{ 1, "TRIM", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_overload_info[] = {
	{ 1, "OVER", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};


static info_lkp_t mge_replacebatt_info[] = {
	{ 1, "RB", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_output_util_off_info[] = {
	{ 1, "OFF", NULL, NULL },
	{ 2, "", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_transfer_reason_info[] = {
	{ 1, "", NULL, NULL },
	{ 2, "input voltage out of range", NULL, NULL },
	{ 3, "input frequency out of range", NULL, NULL },
	{ 4, "utility off", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_test_result_info[] = {
	{ 1, "done and passed", NULL, NULL },
	{ 2, "done and warning", NULL, NULL },
	{ 3, "done and error", NULL, NULL },
	{ 4, "aborted", NULL, NULL },
	{ 5, "in progress", NULL, NULL },
	{ 6, "no test initiated", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_beeper_status_info[] = {
	{ 1, "disabled", NULL, NULL },
	{ 2, "enabled", NULL, NULL },
	{ 3, "muted", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t mge_yes_no_info[] = {
	{ 1, "yes", NULL, NULL },
	{ 2, "no", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* FIXME: the below may introduce status redundancy, that needs to be
 * addressed by the driver, as for usbhid-ups! */
static info_lkp_t mge_power_source_info[] = {
	{ 1, "" /* other */, NULL, NULL },
	{ 2, "OFF" /* none */, NULL, NULL },
#if 0
	{ 3, "OL" /* normal */, NULL, NULL },
#endif
	{ 4, "BYPASS" /* bypass */, NULL, NULL },
	{ 5, "OB" /* battery */, NULL, NULL },
	{ 6, "BOOST" /* booster */, NULL, NULL },
	{ 7, "TRIM" /* reducer */, NULL, NULL },
	{ 0, NULL, NULL, NULL }
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

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Eaton", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.1.1.0", "Generic SNMP UPS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.1.7.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.1.4.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.12.12.0", "", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.1", "", SU_OUTPUT_1, NULL },
	{ "ups.beeper.status", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.2.1.33.1.9.8.0", "", 0, mge_beeper_status_info },
	{ "ups.L1.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.1", "", SU_OUTPUT_3, NULL },
	{ "ups.L2.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.2", "", SU_OUTPUT_3, NULL },
	{ "ups.L3.load", 0, 1, ".1.3.6.1.4.1.705.1.7.2.1.4.3", "", SU_OUTPUT_3, NULL },
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.33.1.7.3.0", "", 0, mge_test_result_info },
	{ "ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW, 6, "1.3.6.1.2.1.33.1.8.2.0", DEFAULT_OFFDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.delay.start", ST_FLAG_STRING | ST_FLAG_RW, 6, "1.3.6.1.2.1.33.1.8.3.0", DEFAULT_ONDELAY, SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.timer.shutdown", 0, 1, "1.3.6.1.2.1.33.1.8.2.0", "", SU_FLAG_OK, NULL },
	{ "ups.timer.start", 0, 1, "1.3.6.1.2.1.33.1.8.3.0", "", SU_FLAG_OK, NULL },
	{ "ups.timer.reboot", 0, 1, "1.3.6.1.2.1.33.1.8.4.0", "", SU_FLAG_OK, NULL },
	{ "ups.start.auto", ST_FLAG_RW, 1, "1.3.6.1.2.1.33.1.8.5.0", "", SU_FLAG_OK, mge_yes_no_info },
	/* status data */
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.5.11.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_replacebatt_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.5.14.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_lowbatt_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.5.16.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_lowbatt_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_onbatt_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.4.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_bypass_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.7.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_output_util_off_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.8.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_boost_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.10.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_overload_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.12.0", "", SU_FLAG_OK | SU_STATUS_BATT, mge_trim_info },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.33.1.4.1.0", "", SU_STATUS_PWR | SU_FLAG_OK, mge_power_source_info },

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
	{ "input.phases", 0, 1.0, ".1.3.6.1.4.1.705.1.6.1.0", "", 0, NULL },
	{ "input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.3", "", SU_INPUT_3, NULL },
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.1", "", SU_INPUT_1, NULL },
	{ "input.L1.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.1", "", SU_INPUT_3, NULL },
	{ "input.L2.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.2", "", SU_INPUT_3, NULL },
	{ "input.L3.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.3.3", "", SU_INPUT_3, NULL },
	{ "input.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.1", "", SU_INPUT_1, NULL },
	{ "input.L1-N.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage.minimum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.4.3", "", SU_INPUT_3, NULL },
	{ "input.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.1", "", SU_INPUT_1, NULL },
	{ "input.L1-N.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.1", "", SU_INPUT_3, NULL },
	{ "input.L2-N.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.2", "", SU_INPUT_3, NULL },
	{ "input.L3-N.voltage.maximum", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.5.3", "", SU_INPUT_3, NULL },
	{ "input.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.1", "", SU_INPUT_1, NULL },
	{ "input.L1.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.1", "", SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.2", "", SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.6.3", "", SU_INPUT_3, NULL },
	{ "input.transfer.reason", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.6.4.0", "", SU_FLAG_OK, mge_transfer_reason_info },

	/* Output page */
	{ "output.phases", 0, 1.0, ".1.3.6.1.4.1.705.1.7.1.0", "", 0, NULL },
	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.1", "", SU_OUTPUT_1, NULL },
	{ "output.L1-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.2.3", "", SU_OUTPUT_3, NULL },
	{ "output.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.1", "", SU_OUTPUT_1, NULL },
	{ "output.L1.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.frequency", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.3.3", "", SU_OUTPUT_3, NULL },
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.1", "", SU_OUTPUT_1, NULL },
	{ "output.L1.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.1", "", SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.2", "", SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 0.1, ".1.3.6.1.4.1.705.1.7.2.1.5.3", "", SU_OUTPUT_3, NULL },

	/* Battery page */
	{ "battery.charge", 0, 1, ".1.3.6.1.4.1.705.1.5.2.0", "", SU_FLAG_OK, NULL },
	{ "battery.runtime", 0, 1, ".1.3.6.1.4.1.705.1.5.1.0", "", SU_FLAG_OK, NULL },
	{ "battery.runtime.low", 0, 1, ".1.3.6.1.4.1.705.1.4.7.0", "", SU_FLAG_OK, NULL },
	{ "battery.charge.low", ST_FLAG_STRING | ST_FLAG_RW, 2, ".1.3.6.1.4.1.705.1.4.8.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "battery.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.5.5.0", "", SU_FLAG_OK, NULL },

	/* Ambient page: Environment Sensor (ref 66 846) */
	{ "ambient.temperature", 0, 0.1, ".1.3.6.1.4.1.705.1.8.1.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },
	{ "ambient.humidity", 0, 0.1, ".1.3.6.1.4.1.705.1.8.2.0", "", SU_TYPE_INT | SU_FLAG_OK, NULL },

	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "Main Outlet",  SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },

	/* instant commands. */
	{ "test.battery.start", 0, 1, ".1.3.6.1.4.1.705.1.10.4.0", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* Also use IETF OIDs
	 * { "test.battery.stop", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.2", SU_TYPE_CMD, NULL },
	 * { "test.battery.start", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.3", SU_TYPE_CMD, NULL },
	 * { "test.battery.start.quick", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.4", SU_TYPE_CMD, NULL },
	 * { "test.battery.start.deep", 0, 0, "1.3.6.1.2.1.33.1.7.1.0", "1.3.6.1.2.1.33.1.7.7.5", SU_TYPE_CMD, NULL },
	 */

	/* { "load.off", 0, 1, ".1.3.6.1.4.1.705.1.9.1.1.6.1", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL },
	 * { "load.on", 0, 1, ".1.3.6.1.4.1.705.1.9.1.1.3.1", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL },
	 * { "shutdown.return", 0, 1, ".1.3.6.1.4.1.705.1.9.1.1.9.1", MGE_START_VALUE, SU_TYPE_CMD | SU_FLAG_OK, NULL },
	 */
	/* IETF MIB fallback */
	{ "beeper.disable", 0, 1, "1.3.6.1.2.1.33.1.9.8.0", "1", SU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 1, "1.3.6.1.2.1.33.1.9.8.0", "2", SU_TYPE_CMD, NULL },
	{ "beeper.mute", 0, 1, "1.3.6.1.2.1.33.1.9.8.0", "3", SU_TYPE_CMD, NULL },
	/*{ "load.off", 0, 1, "1.3.6.1.2.1.33.1.8.2.0", DEFAULT_OFFDELAY, SU_TYPE_CMD, NULL },
	{ "load.on", 0, 1, "1.3.6.1.2.1.33.1.8.3.0", DEFAULT_ONDELAY, SU_TYPE_CMD, NULL },*/
	{ "load.off.delay", 0, 1, "1.3.6.1.2.1.33.1.8.2.0", NULL, SU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 1, "1.3.6.1.2.1.33.1.8.3.0", NULL, SU_TYPE_CMD, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	mge = { "mge", MGE_MIB_VERSION, NULL, MGE_OID_MODEL_NAME, mge_mib, MGE_SYSOID, NULL };
