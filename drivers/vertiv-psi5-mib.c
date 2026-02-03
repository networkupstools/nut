/*	vertiv-psi5-mib.h - Driver for Vertiv Liebert PSI5 UPS (maybe other Vertiv too)
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
 */

#include "vertiv-psi5-mib.h"

#define VERTIV_PSI5_MIB_VERSION "0.01"

/* SysOID for Vertiv/Liebert */
#define VERTIV_BASEOID	".1.3.6.1.4.1.476.1.42"
#define VERTIV_PSI5_SYSOID	VERTIV_BASEOID
#define VERTIV_PSI5_OID_MODEL	VERTIV_BASEOID ".2.1.4.1"
#define VERTIV_PSI5_OID_PWR	VERTIV_BASEOID ".3.5.3"

/* Vertiv PSI5-750MT120 Mapping
 * Monitoring: Based on integer branch at .1.3.6.1.4.1.476.1.42.3.9.30...
 * Control: Based on standard Liebert Environmental MIB at .1.3.6.1.4.1.476.1.42.3.3.5...
 */

static snmp_info_t vertiv_psi5_mib[] = {
	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* Load and Temperature */
	snmp_info_default("ups.load", 0, 1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.5861", "", SU_OUTPUT_1, NULL),
	snmp_info_default("ups.temperature", 0, 0.1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.10.1.2.1.4291", NULL, 0, NULL),

	/* Battery */
	snmp_info_default("battery.charge", 0, 0.1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4153", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("battery.runtime", 0, 1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4150", "", SU_FLAG_OK, NULL),
	snmp_info_default("battery.voltage", 0, 0.1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4148", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),

	/* Input */
	snmp_info_default("input.voltage", 0, 0.1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4096", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("input.frequency", 0, 0.1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4105", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),

	/* Output */
	snmp_info_default("output.voltage", 0, 0.1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4385", "", SU_FLAG_OK|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("output.current", 0, 0.1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4204", "", SU_FLAG_OK|SU_FLAG_NEGINVALID|SU_FLAG_UNIQUE, NULL),
	snmp_info_default("output.power", 0, 1, ".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4208", "", SU_FLAG_OK|SU_FLAG_NEGINVALID, NULL),

	/* Shutdown / Restart Control
	 * Derived from lgpEnvControl (LIEBERT-GP-ENVIRONMENTAL-MIB)
	 */
	snmp_info_default("ups.delay.shutdown", ST_FLAG_RW, 3, ".1.3.6.1.4.1.476.1.42.3.3.5.1.0", "", SU_TYPE_TIME | SU_FLAG_OK, NULL),
	snmp_info_default("ups.delay.start", ST_FLAG_RW, 3, ".1.3.6.1.4.1.476.1.42.3.3.5.2.0", "", SU_TYPE_TIME | SU_FLAG_OK, NULL),

	/* Status Flags: TBD */

	/* end of structure. */
	snmp_info_sentinel
};

static alarms_info_t vertiv_psi5_alarms[] = {
	/* Replace Battery */
	{ ".1.3.6.1.4.1.476.1.42.3.9.20.1.20.1.2.100.6182", "RB", "Replace Battery" },
	/* Inverter Failure */
	{ ".1.3.6.1.4.1.476.1.42.3.9.20.1.20.1.2.100.4233", NULL, "Inverter failure!" },
	/* Overload */
	{ ".1.3.6.1.4.1.476.1.42.3.9.20.1.20.1.2.100.5806", "OVER", "Output overload!" },
};

/* SysOID for Vertiv/Liebert */

mib2nut_info_t vertiv_psi5_subdriver = {
	"vertiv-psi5",
	VERTIV_PSI5_MIB_VERSION,
	VERTIV_PSI5_OID_PWR,
	VERTIV_PSI5_OID_MODEL,
	vertiv_psi5_mib,
	VERTIV_PSI5_SYSOID,
	vertiv_psi5_alarms
};
