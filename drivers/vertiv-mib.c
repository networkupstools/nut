/*	vertiv-mib.h - Driver for Vertiv Liebert PSI5 UPS (maybe other Vertiv too)
 *
 *	Copyright (C)
 *		2026       jawz101 <jawz101@users.noreply.github.com> + Gemini
 *		2026       Jim Klimov <jimklimov+nut@gmail.com>       - cleanup
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Includes support for:
 * - Basic Monitoring (Voltage, Load, Temperature)
 * - Battery Management (Charge, Runtime)
 * - Alarms (Replace Battery, Overload, etc.)
 * - Beeper Control
 */

#include "vertiv-mib.h"

#define VERTIV_MIB_VERSION "0.01"

/* Base OIDs from IS-UNITY-DP Card */
#define VERTIV_BASEOID        ".1.3.6.1.4.1.476.1.42"
#define VERTIV_ID_OID         VERTIV_BASEOID ".2.4.2.1.4.1" 
#define VERTIV_VAL_OID        VERTIV_BASEOID ".3.9.30.1.20.1.2.1"
#define VERTIV_ALM_OID        VERTIV_BASEOID ".3.9.20.1.10.1.2.100"
#define VERTIV_PWRSTATUS_OID  VERTIV_BASEOID ".3.5.3"
#define VERTIV_BEEPER_OID     VERTIV_VAL_OID ".6188"

static info_lkp_t ietf_beeper_status_info[] = {
	info_lkp_default(1, "disabled"),
	info_lkp_default(2, "enabled"),
	info_lkp_default(3, "muted"),
	info_lkp_sentinel
};

static info_lkp_t vertiv_beeper_status_info[] = {
	info_lkp_default(1, "enabled"),
	info_lkp_default(2, "disabled"),
	info_lkp_sentinel
};

static snmp_info_t vertiv_mib[] = {
	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK | SU_FLAG_SEMI_STATIC, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK | SU_FLAG_SEMI_STATIC, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK | SU_FLAG_SEMI_STATIC, NULL),

	/* Device Identification from vendor MIB */
	snmp_info_default("device.mfr",    0, 1.0, VERTIV_BASEOID ".2.1.1.0", NULL, SU_FLAG_OK | SU_FLAG_STATIC, NULL),
	snmp_info_default("device.model",  0, 1.0, VERTIV_ID_OID, NULL, SU_FLAG_OK | SU_FLAG_STATIC, NULL),
	snmp_info_default("device.serial", 0, 1.0, VERTIV_BASEOID ".2.4.2.1.7.1", NULL, SU_FLAG_OK | SU_FLAG_STATIC, NULL),

	/* UPS Measurements - Scaling verified via Audit */
	snmp_info_default("ups.load",        0, 1.0, VERTIV_VAL_OID ".5861", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("ups.temperature", 0, 1.0, VERTIV_BASEOID ".3.9.30.1.10.1.2.1.4291", NULL, SU_FLAG_NEGINVALID, NULL),

	/* Battery Data */
	snmp_info_default("battery.charge",  0, 1.0, VERTIV_VAL_OID ".4153", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	/* Multiplier 60.0 converts UPS minutes to NUT seconds */
	snmp_info_default("battery.runtime", 0, 60.0, VERTIV_VAL_OID ".4150", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.voltage", 0, 1.0, VERTIV_VAL_OID ".4148", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),

	/* Power Quality - 0.1 multiplier for tenths of Volts/Hz */
	snmp_info_default("input.voltage",   0, 0.1, VERTIV_VAL_OID ".4096", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("input.frequency", 0, 0.1, VERTIV_VAL_OID ".4105", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.voltage",  0, 0.1, VERTIV_VAL_OID ".4385", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.current",  0, 0.1, VERTIV_VAL_OID ".4204", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("output.power",    0, 1.0, VERTIV_VAL_OID ".4208", "", SU_FLAG_OK | SU_FLAG_NEGINVALID, NULL),

	/* UPS Status */
	/* Output Source: 3=Normal(OL), 4/5=Battery(OB) */
	snmp_info_default("ups.status",      0, 1.0, VERTIV_VAL_OID ".4872", "", SU_FLAG_OK, NULL),

	/* Beeper status and commands */
	snmp_info_default("ups.beeper.status", ST_FLAG_STRING, SU_INFOSIZE, VERTIV_BEEPER_OID, "", SU_FLAG_UNIQUE, vertiv_beeper_status_info),

	snmp_info_default("beeper.disable", 0, 1, VERTIV_BEEPER_OID, "2", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.enable",  0, 1, VERTIV_BEEPER_OID, "1", SU_TYPE_CMD, NULL),

	/* IETF MIB fallback */
	snmp_info_default("ups.beeper.status", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.2.1.33.1.9.8.0", "", SU_FLAG_UNIQUE, ietf_beeper_status_info),
#if 0
	snmp_info_default("beeper.disable", 0, 1, "1.3.6.1.2.1.33.1.9.8.0", "1", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.enable",  0, 1, "1.3.6.1.2.1.33.1.9.8.0", "2", SU_TYPE_CMD, NULL),
#endif
	snmp_info_default("beeper.mute",    0, 1, "1.3.6.1.2.1.33.1.9.8.0", "3", SU_TYPE_CMD, NULL),

	/* Shutdown / Restart Control
	 * NOTE: Other sources suggest
	 *   "ups.delay.shutdown" => VERTIV_BASEOID ".3.3.5.1.0"
	 *   "ups.delay.start"    => VERTIV_BASEOID ".3.3.5.2.0"
	 */
	snmp_info_default("ups.delay.shutdown", ST_FLAG_RW, 1.0, VERTIV_VAL_OID ".5814", "", SU_TYPE_TIME | SU_FLAG_OK, NULL),
	snmp_info_default("ups.delay.start",    ST_FLAG_RW, 1.0, VERTIV_VAL_OID ".5816", "", SU_TYPE_TIME | SU_FLAG_OK, NULL),

	/* end of structure. */
	snmp_info_sentinel
};

static alarms_info_t vertiv_alarms[] = {
	/* Event Branch Monitoring */
	{ VERTIV_ALM_OID ".4168", "OB",   "Battery Discharging" },
	{ VERTIV_ALM_OID ".4162", "LB",   "Battery Low" },
	{ VERTIV_ALM_OID ".5806", "OVER", "Output Overload" },
	{ VERTIV_ALM_OID ".6182", "RB",   "Replace Battery" },
	{ VERTIV_ALM_OID ".4233", "FAULT", "Inverter Failure" },
	{ VERTIV_ALM_OID ".4310", "OT",   "Over Temperature" },
	{ VERTIV_ALM_OID ".4215", "OFF",  "UPS Output Off" },
	{ NULL, NULL, NULL }
};

mib2nut_info_t vertiv = {
	"vertiv",
	VERTIV_MIB_VERSION,
	VERTIV_PWRSTATUS_OID,/* Optional Power Status OID */
	VERTIV_ID_OID,      /* Model Name OID */
	vertiv_mib,
	VERTIV_BASEOID,     /* SysOID fingerprint */
	vertiv_alarms
};
