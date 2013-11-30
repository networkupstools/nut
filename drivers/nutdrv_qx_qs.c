/* nutdrv_qx_qs.c - Subdriver for QS protocol based UPSes
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * NOTE:
 * This subdriver implements the same protocol as the one used by the 'mustek' subdriver minus the vendor (I) and ratings (F) queries.
 * In the claim function:
 * - it doesn't even try to get 'vendor' informations (I)
 * - it checks only status (QS), through 'input.voltage' variable
 * Therefore it should be able to work even if the UPS doesn't support vendor/ratings *and* the user doesn't use the 'novendor'/'norating' flags, as long as:
 * - the UPS replies a QS-compliant answer (i.e. not necessary filled with all of the QS-required data, but at least of the right length and with not available data filled with some replacement character)
 * - the UPS reports a valid input.voltage (used in the claim function)
 * - the UPS reports valid status bits (1st, 2nd, 3rd, 6th, 7th are the mandatory ones)
 *
 */

#include "main.h"
#include "nutdrv_qx.h"
#include "nutdrv_qx_blazer-common.h"

#include "nutdrv_qx_qs.h"

#define QS_VERSION "QS 0.01"

/* qx2nut lookup table */
static item_t	qs_qx2nut[] = {

	/*
	 * > [QS\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */

	{ "input.voltage",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	1,	5,	"%.1f",	0,	NULL },
	{ "input.voltage.fault",	0,	NULL,	"QS\r",	"",	47,	'(',	"",	7,	11,	"%.1f",	0,	NULL },
	{ "output.voltage",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	13,	17,	"%.1f",	0,	NULL },
	{ "ups.load",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	19,	21,	"%.0f",	0,	NULL },
	{ "input.frequency",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	23,	26,	"%.1f",	0,	NULL },
	{ "battery.voltage",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	28,	31,	"%.2f",	0,	NULL },
	{ "ups.temperature",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	33,	36,	"%.1f",	0,	NULL },
	/* Status bits */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.alarm",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	44,	44,	NULL,	0,			blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			blazer_process_status_bits },	/* Beeper status */

	/* Instant commands */
	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL },
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL },
	{ "test.battery.start",		0,	NULL,	"T%02d\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	blazer_process_command },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL }
};

/* Testing table */
#ifdef TESTING
static testing_t	qs_testing[] = {
	{ "QS\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00000000\r" },
	{ "Q\r",	"" },
	{ "S03\r",	"" },
	{ "C\r",	"" },
	{ "S02R0005\r",	"" },
	{ "S.5R0000\r",	"" },
	{ "T04\r",	"" },
	{ "TL\r",	"" },
	{ "T\r",	"" },
	{ "CT\r",	"" },
	{ NULL }
};
#endif	/* TESTING */

/* Subdriver interface */
subdriver_t	qs_subdriver = {
	QS_VERSION,
	blazer_claim_light,
	qs_qx2nut,
	NULL,
	NULL,
	NULL,
	"ACK",
	NULL,
#ifdef TESTING
	qs_testing,
#endif	/* TESTING */
};
