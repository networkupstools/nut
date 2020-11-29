/* nutdrv_qx_voltronic-qs.c - Subdriver for Voltronic Power UPSes with QS protocol
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
 */

#include "main.h"
#include "nutdrv_qx.h"
#include "nutdrv_qx_blazer-common.h"

#include "nutdrv_qx_voltronic-qs.h"

#define VOLTRONIC_QS_VERSION "Voltronic-QS 0.07"

/* Support functions */
static int	voltronic_qs_claim(void);
static void	voltronic_qs_initups(void);

/* Preprocess functions */
static int	voltronic_qs_protocol(item_t *item, char *value, const size_t valuelen);


/* == Ranges == */

/* Range for ups.delay.start */
static info_rw_t	voltronic_qs_r_ondelay[] = {
	{ "60", 0 },
	{ "599940", 0 },
	{ "", 0 }
};

/* Range for ups.delay.shutdown */
static info_rw_t	voltronic_qs_r_offdelay[] = {
	{ "12", 0 },
	{ "540", 0 },
	{ "", 0 }
};


/* == qx2nut lookup table == */
static item_t	voltronic_qs_qx2nut[] = {

	/* Query UPS for protocol
	 * > [M\r]
	 * < [V\r]
	 *    01
	 *    0
	 */

	{ "ups.firmware.aux",		0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"PM-%s",	QX_FLAG_STATIC,	NULL,	NULL,	voltronic_qs_protocol },

	/* Query UPS for status
	 * > [QS\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */

	{ "input.voltage",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.voltage.fault",	0,	NULL,	"QS\r",	"",	47,	'(',	"",	7,	11,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	13,	17,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	19,	21,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "output.frequency",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	23,	26,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	28,	31,	"%.2f",	0,	NULL,	NULL,	NULL },
	{ "ups.temperature",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	33,	36,	"%.1f",	0,	NULL,	NULL,	NULL },
	/* Status bits */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"QS\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			NULL,	NULL,	blazer_process_status_bits },	/* Beeper status */

	/* Query UPS for ratings
	 * > [F\r]
	 * < [#220.0 003 12.00 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "output.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	5,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "output.current.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	7,	9,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	11,	15,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "output.frequency.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	17,	20,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/* Instant commands */
	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	0,	0,	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	0,	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	0,	0,	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	voltronic_qs_r_ondelay,		NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	voltronic_qs_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};


/* == Testing table == */
#ifdef TESTING
static testing_t	voltronic_qs_testing[] = {
	{ "QS\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00000000\r",	-1 },
	{ "F\r",	"#220.0 003 12.00 50.0\r",	-1 },
	{ "M\r",	"V\r",	-1 },
	{ "Q\r",	"",	-1 },
	{ "C\r",	"",	-1 },
	{ "S02R0005\r",	"",	-1 },
	{ "S.5R0000\r",	"",	-1 },
	{ "T\r",	"",	-1 },
	{ NULL }
};
#endif	/* TESTING */


/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int	voltronic_qs_claim(void)
{
	/* We need at least M and QS to run this subdriver */

	/* UPS Protocol */
	item_t	*item = find_nut_info("ups.firmware.aux", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value/Protocol not supported */
	if (ups_infoval_set(item) != 1)
		return 0;

	item = find_nut_info("input.voltage", 0, 0);

	/* Don't know what happened */
	if (!item) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	/* No reply/Unable to get value */
	if (qx_process(item, NULL)) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	/* Unable to process value */
	if (ups_infoval_set(item) != 1) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	return 1;
}

/* Subdriver-specific initups */
static void	voltronic_qs_initups(void)
{
	blazer_initups_light(voltronic_qs_qx2nut);
}


/* == Preprocess functions == */

/* Protocol used by the UPS */
static int	voltronic_qs_protocol(item_t *item, char *value, const size_t valuelen)
{
	if (strcasecmp(item->value, "V")) {
		upsdebugx(2, "%s: invalid protocol [%s]", __func__, item->value);
		return -1;
	}

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	snprintf(value, valuelen, item->dfl, item->value);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}


/* == Subdriver interface == */
subdriver_t	voltronic_qs_subdriver = {
	VOLTRONIC_QS_VERSION,
	voltronic_qs_claim,
	voltronic_qs_qx2nut,
	voltronic_qs_initups,
	NULL,
	blazer_makevartable_light,
	NULL,
	NULL,
#ifdef TESTING
	voltronic_qs_testing,
#endif	/* TESTING */
};
