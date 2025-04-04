/*  baytech-rpc3nc-mib.c - data to monitor BayTech RPC3-NC PDUs
 *
 *	Copied and modified from baytech-mib.c. The Snmp2NUT lookup table is
 *  the same, excluding changing the OID for ups.model and device.model.
 *
 *  Copyright (C) 2009 from baytech-mib.c
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

#include "baytech-rpc3nc-mib.h"

/* NOTE: last badly versioned release was "4032" but should be "X.Y[Z]"! */
#define BAYTECH_RPC3NC_MIB_VERSION	"0.4035"

/* Baytech RPC3NC MIB */
#define BAYTECH_RPC3NC_OID_MIB			".1.3.6.1.4.1.4779"
#define BAYTECH_RPC3NC_OID_MODEL_NAME	".1.3.6.1.4.1.4779.1.3.5.5.1.3.2.1"

static info_lkp_t baytech_outlet_status_info[] = {
	info_lkp_default(-1, "error"),
	info_lkp_default(0, "off"),
	info_lkp_default(1, "on"),
	info_lkp_default(2, "cycling"),	/* transitional status, "reboot" in MIB comments */
	info_lkp_default(3, "lockon"),
	info_lkp_default(4, "lockoff"),
	info_lkp_default(5, "unlock"),
	info_lkp_default(6, "unknown"),
	info_lkp_sentinel
};

/* Snmp2NUT lookup table for BayTech RPC3NC MIBs */
static snmp_info_t baytech_rpc3nc_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* Device page */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "BayTech",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.5.1.3.2.1",
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.2.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.2.2.1.6.2",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL),

	/* UPS page */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Baytech",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.5.1.3.2.1",
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.id", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.3.0",
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.2.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.1.1.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.temperature", 0, 0.1, ".1.3.6.1.4.1.4779.1.3.5.5.1.10.2.1", NULL, 0, NULL),

	/* Outlet page */
	snmp_info_default("outlet.id", 0, 1, NULL, "0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("outlet.count", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.2.1.15.1", "0", 0, NULL),
	snmp_info_default("outlet.current", 0, 0.1, ".1.3.6.1.4.1.4779.1.3.5.5.1.6.2.1", NULL, 0, NULL),
	snmp_info_default("outlet.voltage", 0, 0.1, ".1.3.6.1.4.1.4779.1.3.5.5.1.8.2.1", NULL, 0, NULL),

	/* outlet template definition */
	snmp_info_default("outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.3.1.3.1.%i", NULL, SU_OUTLET, &baytech_outlet_status_info[0]),
	snmp_info_default("outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.4779.1.3.5.3.1.4.1.%i", NULL, SU_OUTLET, NULL),
	snmp_info_default("outlet.%i.id", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.6.1.3.2.1.%i", "%i", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_OUTLET | SU_FLAG_OK, NULL),
	snmp_info_default("outlet.%i.switchable", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.3.1.1.1.%i", "yes", SU_FLAG_STATIC | SU_OUTLET, NULL),

	/* instant commands. */
	snmp_info_default("outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.3.1.3.1.%i", "0", SU_TYPE_CMD | SU_OUTLET, NULL),
	snmp_info_default("outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.4779.1.3.5.3.1.3.1.%i", "1", SU_TYPE_CMD | SU_OUTLET, NULL),

	/* end of structure. */
	snmp_info_sentinel
};

mib2nut_info_t	baytech_rpc3nc = { "baytech_rpc3nc", BAYTECH_RPC3NC_MIB_VERSION, NULL, BAYTECH_RPC3NC_OID_MODEL_NAME, baytech_rpc3nc_mib, BAYTECH_RPC3NC_OID_MIB, NULL };
