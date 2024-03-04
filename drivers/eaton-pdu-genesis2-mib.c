/*  eaton-pdu-genesis2-mib.c - data to monitor Eaton ePDUs branded as:
 *                G1 Aphel based ePDUs (Basic) - GenesisII
 *
 *  Copyright (C) 2008 - 2019
 * 		Arnaud Quette <arnaud.quette@gmail.com>
 * 		Arnaud Quette <ArnaudQuette@Eaton.com>
 *  Copyright (C) 2015 - 2017
 * 		Jim Klimov <EvgenyKlimov@Eaton.com>
 *
 *  Supported by Eaton <http://www.eaton.com>
 *   and previously MGE Office Protection Systems <http://www.mgeops.com>
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

#include "eaton-pdu-genesis2-mib.h"

#define EATON_APHEL_GENESIS2_MIB_VERSION	"0.52"

/* APHEL-GENESIS-II-MIB (monitored ePDU)
 * *************************************
 * Note: There is also a basic XML interface, but not worth
 * implementing in netxml-ups!
 */

#define APHEL1_OID_MIB						".1.3.6.1.4.1.17373"
#define APHEL1_SYSOID						APHEL1_OID_MIB
#define APHEL1_OID_MODEL_NAME				".1.3.6.1.4.1.17373.3.1.1.0"
#define APHEL1_OID_FIRMREV					".1.3.6.1.4.1.17373.3.1.2.0"
#define APHEL1_OID_DEVICE_NAME				".1.3.6.1.4.1.17373.3.1.3.0"
#define APHEL1_OID_UNIT_MACADDR				".1.3.6.1.4.1.17373.3.1.4.0"
/* needs concat .<outlet-index>.0 */
#define APHEL1_OID_OUTLET_CURRENT			".1.3.6.1.4.1.17373.3.2"

/* Snmp2NUT lookup table for GenesisII MIB */
static snmp_info_t eaton_aphel_genesisII_mib[] = {

	/* standard MIB items */
	snmp_info_default("device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL),
	snmp_info_default("device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL),

	/* Device page */
	snmp_info_default("device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.model", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_MODEL_NAME,
		"Eaton Powerware ePDU Monitored", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_UNIT_MACADDR, "unknown",
		0, NULL),

	/* UPS page */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON | Powerware",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_MODEL_NAME,
		"Generic SNMP PDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.id", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_DEVICE_NAME,
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, APHEL1_OID_FIRMREV, "",
		SU_FLAG_STATIC | SU_FLAG_OK, NULL),
	snmp_info_default("ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL),

	/* Outlet page */
	/* we can't use template since there is no counterpart to outlet.count */
	snmp_info_default("outlet.1.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".1.0", NULL, SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("outlet.2.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".2.0", NULL, SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("outlet.3.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".3.0", NULL, SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("outlet.4.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".4.0", NULL, SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("outlet.5.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".5.0", NULL, SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("outlet.6.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".6.0", NULL, SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("outlet.7.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".7.0", NULL, SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("outlet.8.current", 0, 0.1, APHEL1_OID_OUTLET_CURRENT ".8.0", NULL, SU_FLAG_NEGINVALID, NULL),

	/* end of structure. */
	snmp_info_sentinel
};


mib2nut_info_t	aphel_genesisII = { "aphel_genesisII", EATON_APHEL_GENESIS2_MIB_VERSION, NULL, APHEL1_OID_MODEL_NAME, eaton_aphel_genesisII_mib, APHEL1_SYSOID, NULL };
