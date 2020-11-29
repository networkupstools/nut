/* nutdrv_qx_mecer.c - Subdriver for Mecer/Voltronic Power P98 UPSes
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

#include "nutdrv_qx_mecer.h"

#define MECER_VERSION "Mecer 0.07"

/* Support functions */
static int	mecer_claim(void);
static void	mecer_initups(void);

/* Preprocess functions */
static int	voltronic_p98_protocol(item_t *item, char *value, const size_t valuelen);
static int	mecer_process_test_battery(item_t *item, char *value, const size_t valuelen);


/* == qx2nut lookup table == */
static item_t	mecer_qx2nut[] = {

	/* Query UPS for protocol (Voltronic Power UPSes)
	 * > [QPI\r]
	 * < [(PI98\r]
	 *    012345
	 *    0
	 */

	{ "ups.firmware.aux",		0,	NULL,	"QPI\r",	"",	6,	'(',	"",	1,	4,	"%s",	QX_FLAG_STATIC,	NULL,	NULL,	voltronic_p98_protocol },

	/*
	 * > [Q1\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */

	{ "input.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.voltage.fault",	0,	NULL,	"Q1\r",	"",	47,	'(',	"",	7,	11,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	13,	17,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	19,	21,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	23,	26,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	28,	31,	"%.2f",	0,	NULL,	NULL,	NULL },
	{ "ups.temperature",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	33,	36,	"%.1f",	0,	NULL,	NULL,	NULL },
	/* Status bits */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			NULL,	NULL,	blazer_process_status_bits },	/* Beeper status */

	/*
	 * > [F\r]
	 * < [#220.0 000 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "input.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	5,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.current.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	7,	9,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	11,	15,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.frequency.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	17,	20,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/*
	 * > [I\r]
	 * < [#-------------   ------     VT12046Q  \r]
	 *    012345678901234567890123456789012345678
	 *    0         1         2         3
	 */

	{ "device.mfr",			0,	NULL,	"I\r",	"",	39,	'#',	"",	1,	15,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "device.model",		0,	NULL,	"I\r",	"",	39,	'#',	"",	17,	26,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "ups.firmware",		0,	NULL,	"I\r",	"",	39,	'#',	"",	28,	37,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },

	/* Instant commands
	 * The UPS will reply '(ACK\r' in case of success, '(NAK\r' if the command is rejected or invalid */

	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start",		0,	NULL,	"T%s\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	mecer_process_test_battery },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};


/* == Testing table == */
#ifdef TESTING
static testing_t	mecer_testing[] = {
	{ "Q1\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00000000\r",	-1 },
	{ "QPI\r",	"(PI98\r",	-1 },
	{ "F\r",	"#230.0 000 024.0 50.0\r",	-1 },
	{ "I\r",	"#NOT_A_LIVE_UPS  TESTING    TESTING   \r",	-1 },
	{ "Q\r",	"(ACK\r",	-1 },
	{ "S03\r",	"(NAK\r",	-1 },
	{ "C\r",	"(NAK\r",	-1 },
	{ "S02R0005\r",	"(ACK\r",	-1 },
	{ "S.5R0000\r",	"(ACK\r",	-1 },
	{ "T04\r",	"(NAK\r",	-1 },
	{ "TL\r",	"(ACK\r",	-1 },
	{ "T\r",	"(NAK\r",	-1 },
	{ "CT\r",	"(ACK\r",	-1 },
	{ NULL }
};
#endif	/* TESTING */


/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int	mecer_claim(void)
{
	/* Apart from status (Q1), try to identify protocol (QPI, for Voltronic Power P98 units) or whether the UPS uses '(ACK\r'/'(NAK\r' replies */

	item_t	*item = find_nut_info("input.voltage", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value */
	if (ups_infoval_set(item) != 1)
		return 0;

	/* UPS Protocol */
	item = find_nut_info("ups.firmware.aux", 0, 0);

	/* Don't know what happened */
	if (!item) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	/* No reply/Unable to get value/Command rejected */
	if (qx_process(item, NULL)) {

		/* No reply/Command echoed back or rejected with something other than '(NAK\r' -> Not a '(ACK/(NAK' unit */
		if (!strlen(item->answer) || strcasecmp(item->answer, "(NAK\r")) {
			dstate_delinfo("input.voltage");
			return 0;
		}

		/* Command rejected with '(NAK\r' -> '(ACK/(NAK' unit */

		/* Skip protocol query from now on */
		item->qxflags |= QX_FLAG_SKIP;

	} else {

		/* Unable to process value/Command echoed back or rejected with something other than '(NAK\r'/Protocol not supported */
		if (ups_infoval_set(item) != 1) {
			dstate_delinfo("input.voltage");
			return 0;
		}

		/* Voltronic Power P98 unit */

	}

	return 1;
}

/* Subdriver-specific initups */
static void	mecer_initups(void)
{
	blazer_initups(mecer_qx2nut);
}


/* == Preprocess functions == */

/* Protocol used by the UPS */
static int	voltronic_p98_protocol(item_t *item, char *value, const size_t valuelen)
{
	if (strcasecmp(item->value, "PI98")) {
		upslogx(LOG_ERR, "Protocol [%s] is not supported by this driver", item->value);
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
	snprintf(value, valuelen, item->dfl, "Voltronic Power P98");
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}

/* *CMD* Preprocess 'test.battery.start' instant command */
static int	mecer_process_test_battery(item_t *item, char *value, const size_t valuelen)
{
	const char	*protocol = dstate_getinfo("ups.firmware.aux");
	char		buf[SMALLBUF] = "";
	int		min, test_time;

	/* Voltronic P98 units -> Accepted values for test time: .2 -> .9 (.2=12sec ..), 01 -> 99 (minutes) -> range = [12..5940] */
	if (protocol && !strcasecmp(protocol, "Voltronic Power P98"))
		min = 12;
	/* Other units: 01 -> 99 (minutes) -> [60..5940] */
	else
		min = 60;

	if (strlen(value) != strspn(value, "0123456789")) {
		upslogx(LOG_ERR, "%s: non numerical value [%s]", item->info_type, value);
		return -1;
	}

	test_time = strlen(value) > 0 ? strtol(value, NULL, 10) : 600;

	if ((test_time < min) || (test_time > 5940)) {
		upslogx(LOG_ERR, "%s: battery test time '%d' out of range [%d..5940] seconds", item->info_type, test_time, min);
		return -1;
	}

	/* test time < 1 minute */
	if (test_time < 60) {

		test_time = test_time / 6;
		snprintf(buf, sizeof(buf), ".%d", test_time);

	/* test time > 1 minute */
	} else {

		test_time = test_time / 60;
		snprintf(buf, sizeof(buf), "%02d", test_time);

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
	snprintf(value, valuelen, item->command, buf);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}


/* == Subdriver interface == */
subdriver_t	mecer_subdriver = {
	MECER_VERSION,
	mecer_claim,
	mecer_qx2nut,
	mecer_initups,
	NULL,
	blazer_makevartable,
	"ACK",
	"(NAK\r",
#ifdef TESTING
	mecer_testing,
#endif	/* TESTING */
};
