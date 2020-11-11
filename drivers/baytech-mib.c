/*  baytech-mib.c - data to monitor BayTech PDUs
 *
 *  Copyright (C) 2009
 *  			Opengear <support@opengear.com>
 *  			Arnaud Quette <arnaud.quette@free.fr>
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

#include "baytech-mib.h"

/* FIXME: should be "X.Y[Z]"! */
#define BAYTECH_MIB_VERSION	"4032"

/* Baytech MIB */
#define BAYTECH_OID_MIB			".1.3.6.1.4.1.4779"
#define BAYTECH_OID_MODEL_NAME	".1.3.6.1.4.1.4779.1.3.5.2.1.24.1"

static info_lkp_t outlet_status_info[] = {
	{ -1, "error", NULL, NULL },
	{ 0, "off", NULL, NULL },
	{ 1, "on", NULL, NULL },
	{ 2, "cycling", NULL, NULL }, /* transitional status */
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table for BayTech MIBs */
static snmp_info_t baytech_mib[] = {
	/* Device page */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "BayTech",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.2.1.24.1",
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.2.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.2.2.1.6.2",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Baytech",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.2.1.24.1",
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.3.0",
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.2.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.1.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.temperature", 0, 0.1, ".1.3.6.1.4.1.4779.1.3.5.5.1.10.2.1", NULL, 0, NULL },

	/* Outlet page */
	{ "outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.2.1.15.1", "0", 0, NULL },
	{ "outlet.current", 0, 0.1, ".1.3.6.1.4.1.4779.1.3.5.5.1.6.2.1", NULL, 0, NULL },
	{ "outlet.voltage", 0, 0.1, ".1.3.6.1.4.1.4779.1.3.5.5.1.8.2.1", NULL, 0, NULL },

	/* outlet template definition */
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.3.1.3.1.%i", NULL, SU_OUTLET, &outlet_status_info[0] },
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.3.1.4.1.%i", NULL, SU_OUTLET, NULL },
	{ "outlet.%i.id", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.6.1.3.2.1.%i", "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_OUTLET | SU_FLAG_OK, NULL },
	{ "outlet.%i.switchable", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.3.1.1.1.%i", "yes", SU_FLAG_STATIC | SU_OUTLET, NULL },

	/* instant commands. */
	{ "outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.3.1.3.1.%i", "0", SU_TYPE_CMD | SU_OUTLET, NULL },
	{ "outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.3.1.3.1.%i", "1", SU_TYPE_CMD | SU_OUTLET, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	baytech = { "baytech", BAYTECH_MIB_VERSION, NULL, BAYTECH_OID_MODEL_NAME, baytech_mib, BAYTECH_OID_MIB, NULL };
