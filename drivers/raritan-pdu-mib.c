/*  raritan-mib.c - data to monitor Raritan PDUs (Basic and Complex)
 *
 *  Copyright (C) 2008-2019
 *  			Arnaud Quette <ArnaudQuette@Eaton.com>
 *
 *  Sponsored by Eaton <http://www.eaton.com>
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

#include "raritan-pdu-mib.h"

#define RARITAN_MIB_VERSION	"0.6"

/* Raritan MIB
 * this one uses the same MIB as Eaton Revelation,
 * but with a different entry point */
#define	RARITAN_BASE_OID					".1.3.6.1.4.1.13742"
#define RARITAN_SYSOID						RARITAN_BASE_OID
#define RARITAN_OID_MODEL_NAME				".1.3.6.1.4.1.13742.1.1.12.0"

#define DO_OFF		"0"
#define DO_ON		"1"
#define DO_CYCLE	"2"

static info_lkp_t outlet_status_info[] = {
	{ -1, "error", NULL, NULL },
	{ 0, "off", NULL, NULL },
	{ 1, "on", NULL, NULL },
	{ 2, "cycling", NULL, NULL }, /* transitional status */
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table for Raritan MIB */
static snmp_info_t raritan_mib[] = {
	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Raritan",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.1.12.0",
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.1.2.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.1.6.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Raritan",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.1.12.0",
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.1.13.0",
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.1.2.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.1.1.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.temperature", 0, 1, ".1.3.6.1.4.1.13742.1.3.1.5.0", NULL, 0, NULL },

	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.13742.1.2.1.0", "0", 0, NULL },
	{ "outlet.current", 0, 0.001, ".1.3.6.1.4.1.13742.1.3.1.1" ".0", NULL, 0, NULL },
	{ "outlet.voltage", 0, 0.001, ".1.3.6.1.4.1.13742.1.3.1.2" ".0", NULL, 0, NULL },
	{ "outlet.realpower", 0, 1.0, ".1.3.6.1.4.1.13742.1.3.1.3" ".0", NULL, 0, NULL },
	{ "outlet.power", 0, 1.0, ".1.3.6.1.4.1.13742.1.3.1.4" ".0", NULL, 0, NULL },

	/* outlet template definition
	 * Caution: the index of the data start at 0, while the name is +1
	 * ie outlet.1 => <OID>.0 */
	{ "outlet.%i.switchable", 0, 1, ".1.3.6.1.4.1.13742.1.2.2.1.1.%i", "yes", SU_FLAG_STATIC | SU_OUTLET, NULL },
	{ "outlet.%i.id", 0, 1, NULL, "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET, NULL },
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.2.2.1.2.%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.13742.1.2.2.1.3.%i", NULL, SU_FLAG_OK | SU_OUTLET, &outlet_status_info[0] },
	{ "outlet.%i.current", 0, 0.001, ".1.3.6.1.4.1.13742.1.2.2.1.4.%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.current.maximum", 0, 0.001, ".1.3.6.1.4.1.13742.1.2.2.1.5.%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.realpower", 0, 1.0, ".1.3.6.1.4.1.13742.1.2.2.1.7.%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.voltage", 0, 1.0, ".1.3.6.1.4.1.13742.1.2.2.1.6.%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.powerfactor", 0, 0.01, ".1.3.6.1.4.1.13742.1.2.2.1.9.%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.power", 0, 1.0, ".1.3.6.1.4.1.13742.1.2.2.1.8.%i", NULL, SU_OUTLET, NULL },

	/* FIXME:
	 * - delay for startup/shutdown sequence
	 * - support for Ambient page
		temperatureSensorCount" src="snmp:$sysoid.2.1.0
		ambient.temperature src="snmp:$sysoid.2.2.1.3.$indiceSensor => seems dumb!
		ambient.humidity src="snmp:$sysoid.2.4.1.3.$indiceSensor
	 */

	/* instant commands. */
	/* Note that load.cycle might be replaced by / mapped on shutdown.reboot */
	/* no counterpart found!
	{ "outlet.load.off", 0, 1, ".1.3.6.1.4.1.13742.1.2.2.1.3.0", DO_OFF, SU_TYPE_CMD, NULL },
	{ "outlet.load.on", 0, 1, ".1.3.6.1.4.1.13742.1.2.2.1.3.0", DO_ON, SU_TYPE_CMD, NULL },
	{ "outlet.load.cycle", 0, 1, ".1.3.6.1.4.1.13742.1.2.2.1.3.0", DO_CYCLE, SU_TYPE_CMD, NULL }, */
	{ "outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.13742.1.2.2.1.3.%i", DO_OFF, SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.13742.1.2.2.1.3.%i", DO_ON, SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.cycle", 0, 1, ".1.3.6.1.4.1.13742.1.2.2.1.3.%i", DO_CYCLE, SU_TYPE_CMD | SU_OUTLET, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	raritan = { "raritan", RARITAN_MIB_VERSION, NULL, RARITAN_OID_MODEL_NAME, raritan_mib, RARITAN_SYSOID, NULL };
