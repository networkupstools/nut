/*	vertiv-psi5-mib.h - Driver for Vertiv Liebert PSI5 UPS
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

/* drivers/vertiv-psi5-mib.c */
#include "vertiv-psi5-mib.h"

/* Vertiv PSI5-750MT120 Mapping
   Monitoring: Based on integer branch at .1.3.6.1.4.1.476.1.42.3.9.30...
   Control: Based on standard Liebert Environmental MIB at .1.3.6.1.4.1.476.1.42.3.3.5... */

static snmp_info_t vertiv_psi5_mib[] = {
	/* Load and Temperature */
	{ "ups.load", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.5861",
		NULL, NULL },
	{ "ups.temperature", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.10.1.2.1.4291",
		NULL, NULL },

	/* Battery */
	{ "battery.charge", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4153",
		NULL, NULL },
	{ "battery.runtime", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4150",
		NULL, NULL }, /* Raw value is minutes */
	{ "battery.voltage", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4148",
		NULL, NULL },

	/* Input */
	{ "input.voltage", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4096",
		SU_FLAG_OK, NULL }, /* Scale: 0.1 */
	{ "input.frequency", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4105",
		SU_FLAG_OK, NULL }, /* Scale: 0.1 */

	/* Output */
	{ "output.voltage", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4385",
		SU_FLAG_OK, NULL }, /* Scale: 0.1 */
	{ "output.current", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4204",
		SU_FLAG_OK, NULL }, /* Scale: 0.1 */
	{ "output.power", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.30.1.20.1.2.1.4208",
		SU_FLAG_OK, NULL },

	/* Shutdown / Restart Control
	   Derived from lgpEnvControl (LIEBERT-GP-ENVIRONMENTAL-MIB) */
	{ "ups.delay.shutdown", 0, SU_INFOTYPE_INT,
		".1.3.6.1.4.1.476.1.42.3.3.5.1.0",
		SU_FLAG_RW | SU_FLAG_OK, NULL },
	{ "ups.delay.start", 0, SU_INFOTYPE_INT,
		".1.3.6.1.4.1.476.1.42.3.3.5.2.0",
		SU_FLAG_RW | SU_FLAG_OK, NULL },

	/* Status Flags */
	{ "ups.alarm", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.20.1.20.1.2.100.6182",
		NULL, NULL }, /* Replace Battery */
	{ "ups.alarm", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.20.1.20.1.2.100.4233",
		NULL, NULL }, /* Inverter Failure */
	{ "ups.alarm", 0, SU_INFOTYPE_INT, NULL,
		".1.3.6.1.4.1.476.1.42.3.9.20.1.20.1.2.100.5806",
		NULL, NULL }, /* Overload */

	/* End of list */
	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

snmp_subdriver_t vertiv_psi5_subdriver = {
	"vertiv-psi5",
	"1.00",
	".1.3.6.1.4.1.476.1.42", /* SysOID for Vertiv/Liebert */
	vertiv_psi5_mib,
	NULL,
	NULL,
	NULL,
	NULL
};
