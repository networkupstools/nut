/* nutdrv_qx_bestups.c - Subdriver for Best Power/Sola Australia UPSes
 *
 * Copyright (C)
 *   2014 Daniele Pezzini <hyouko@gmail.com>
 * Based on:
 *  bestups.c - Copyright (C)
 *    1999 Russell Kroll <rkroll@exploits.org>
 *         Jason White <jdwhite@jdwhite.org>
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

#include "nutdrv_qx_bestups.h"

#define BESTUPS_VERSION "BestUPS 0.06"

/* Support functions */
static int	bestups_claim(void);
static void	bestups_initups(void);
static void	bestups_makevartable(void);

/* Answer preprocess functions */
static int	bestups_preprocess_id_answer(item_t *item, const int len);

/* Preprocess functions */
static int	bestups_process_setvar(item_t *item, char *value, const size_t valuelen);
static int	bestups_process_bbb_status_bit(item_t *item, char *value, const size_t valuelen);
static int	bestups_manufacturer(item_t *item, char *value, const size_t valuelen);
static int	bestups_model(item_t *item, char *value, const size_t valuelen);
static int	bestups_batt_runtime(item_t *item, char *value, const size_t valuelen);
static int	bestups_batt_packs(item_t *item, char *value, const size_t valuelen);
static int	bestups_get_pins_shutdown_mode(item_t *item, char *value, const size_t valuelen);
static int	bestups_voltage_settings(item_t *item, char *value, const size_t valuelen);

/* ups.conf settings */
static int	pins_shutdown_mode;

/* General settings */
static int	inverted_bbb_bit = 0;


/* == Ranges/enums == */

/* Range for ups.delay.start */
info_rw_t	bestups_r_ondelay[] = {
	{ "60", 0 },
	{ "599940", 0 },
	{ "", 0 }
};

/* Range for ups.delay.shutdown */
static info_rw_t	bestups_r_offdelay[] = {
	{ "12", 0 },
	{ "5940", 0 },
	{ "", 0 }
};

/* Range for number of battery packs */
static info_rw_t	bestups_r_batt_packs[] = {
	{ "0", 0 },
	{ "5", 0 },
	{ "", 0 }
};

/* Range for pin shutdown mode */
static info_rw_t	bestups_r_pins_shutdown_mode[] = {
	{ "0", 0 },
	{ "6", 0 },
	{ "", 0 }
};


/* == qx2nut lookup table == */
static item_t	bestups_qx2nut[] = {

	/* Query UPS for status
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
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },		/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },		/* Battery Low */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },		/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },		/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },		/* Test in Progress */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },		/* Shutdown Active */
/*	{ "ups.beeper.status",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			NULL,	NULL,	blazer_process_status_bits },		*//* Beeper status: not supported; always 0 */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	bestups_process_bbb_status_bit },	/* Bypass/Boost or Buck Active - keep this one at the end as it needs the processed data from the previous items */

	/* Query UPS for ratings and model infos
	 * > [ID\r]
	 * < [FOR,750,120,120,20.0,27.6\r]	case #1: length = 26
	 * < [FOR,1500,120,120,20.0,27.6\r]	case #2: length = 27
	 * < [FOR,3000,120,120,20.0,100.6\r]	case #3: length = 28
	 * < [FOR, 750,120,120,20.0, 27.6\r]	after being preprocessed: length = 28
	 *    0123456789012345678901234567
	 *    0         1         2
	 */

	{ "device.mfr",			0,	NULL,	"ID\r",	"",	28,	0,	"",	0,	2,	"%s",	QX_FLAG_STATIC,		NULL,	bestups_preprocess_id_answer,	bestups_manufacturer },
	{ "device.model",		0,	NULL,	"ID\r",	"",	28,	0,	"",	0,	2,	"%s",	QX_FLAG_STATIC,		NULL,	bestups_preprocess_id_answer,	bestups_model },
	{ "ups.power.nominal",		0,	NULL,	"ID\r",	"",	28,	0,	"",	4,	7,	"%.0f",	QX_FLAG_STATIC,		NULL,	bestups_preprocess_id_answer,	NULL },
	{ "input.voltage.nominal",	0,	NULL,	"ID\r",	"",	28,	0,	"",	9,	11,	"%.0f",	QX_FLAG_STATIC,		NULL,	bestups_preprocess_id_answer,	NULL },
	{ "output.voltage.nominal",	0,	NULL,	"ID\r",	"",	28,	0,	"",	13,	15,	"%.0f",	QX_FLAG_STATIC,		NULL,	bestups_preprocess_id_answer,	NULL },
	{ "battery.voltage.low",	0,	NULL,	"ID\r",	"",	28,	0,	"",	17,	20,	"%.1f",	QX_FLAG_SEMI_STATIC,	NULL,	bestups_preprocess_id_answer,	NULL },
	{ "battery.voltage.high",	0,	NULL,	"ID\r",	"",	28,	0,	"",	22,	26,	"%.1f",	QX_FLAG_SEMI_STATIC,	NULL,	bestups_preprocess_id_answer,	NULL },

	/* Query UPS for battery runtime (not available on the Patriot Pro/Sola 320 model series)
	 * > [RT\r]
	 * < [025\r]
	 *    0123
	 *    0
	 */

	{ "battery.runtime",	0,	NULL,	"RT\r",	"",	4,	0,	"",	0,	2,	"%.0f",	 QX_FLAG_SKIP,	NULL,	NULL,	bestups_batt_runtime },

	/* Query UPS for number of battery packs (available only on the Axxium/Sola 620 model series)
	 * > [BP?\r]
	 * < [02\r]
	 *    012
	 *    0
	 */

	{ "battery.packs",	ST_FLAG_RW,	bestups_r_batt_packs,	"BP?\r",	"",	3,	0,	"",	0,	1,	"%d",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	bestups_batt_packs },

	/* Set number of battery packs to n (integer, 0-5) (available only on the Axxium/Sola 620 model series)
	 * > [BPn\r]
	 * < []
	 */

	{ "battery.packs",	0,		bestups_r_batt_packs,	"BP%.0f\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,		NULL,	NULL,	bestups_process_setvar },

	/* Query UPS for shutdown mode functionality of Pin 1 and Pin 7 on the UPS DB9 communication port (Per Best Power's EPS-0059)
	 * > [SS?\r]
	 * < [0\r]
	 *    01
	 *    0
	 */

	{ "pins_shutdown_mode",	ST_FLAG_RW,	bestups_r_pins_shutdown_mode,	"SS?\r",	"",	2,	0,	"",	0,	0,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_NONUT,			NULL,	NULL,	bestups_get_pins_shutdown_mode },

	/* Set shutdown mode functionality of Pin 1 and Pin 7 on the UPS DB9 communication port (Per Best Power's EPS-0059) to n (integer, 0-6)
	 * > [SSn\r]
	 * < []
	 */

	{ "pins_shutdown_mode",	0,		bestups_r_pins_shutdown_mode,	"SS%.0f\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,		NULL,	NULL,	bestups_process_setvar },

	/* Query UPS for voltage settings
	 * > [M\r]
	 * < [0\r]
	 *    01
	 *    0
	 */

	{ "input.transfer.low",		0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },
	{ "input.transfer.boost.low",	0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },
	{ "input.transfer.boost.high",	0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },
	{ "input.voltage.nominal",	0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },
	{ "output.voltage.nominal",	0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },
	{ "input.transfer.trim.low",	0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },
	{ "input.transfer.trim.high",	0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },
	{ "input.transfer.high",	0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"%d",	0,	NULL,	NULL,	bestups_voltage_settings },

	/* Instant commands */
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start",		0,	NULL,	"T%02d\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	bestups_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	bestups_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,		0,	NULL,	NULL,	"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};


/* == Testing table == */
#ifdef TESTING
static testing_t	bestups_testing[] = {
	{ "Q1\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00100000\r",	-1 },
	{ "ID\r",	"FOR,750,120,120,20.0,27.6\r",	-1 },
	{ "RT\r",	"015\r",	-1 },
	{ "BP?\r",	"02\r",	-1 },
	{ "BP1\r",	"",	-1 },
	{ "SS?\r",	"0\r",	-1 },
	{ "SS2\r",	"",	-1 },
	{ "M\r",	"0\r",	-1 },
	{ "S03\r",	"",	-1 },
	{ "C\r",	"",	-1 },
	{ "S02R0005\r",	"",	-1 },
	{ "S.5R0001\r",	"",	-1 },
	{ "T04\r",	"",	-1 },
	{ "TL\r",	"",	-1 },
	{ "T\r",	"",	-1 },
	{ "CT\r",	"",	-1 },
	{ NULL }
};
#endif	/* TESTING */


/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int	bestups_claim(void)
{
	/* We need at least Q1 and ID to run this subdriver */

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

	/* UPS Model */
	item = find_nut_info("device.model", 0, 0);

	/* Don't know what happened */
	if (!item) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	/* No reply/Unable to get value */
	if (qx_process(item, NULL)) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	/* Unable to process value */
	if (ups_infoval_set(item) != 1) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	return 1;
}

/* Subdriver-specific initups */
static void	bestups_initups(void)
{
	blazer_initups_light(bestups_qx2nut);
}

/* Subdriver-specific flags/vars */
static void	bestups_makevartable(void)
{
	addvar(VAR_VALUE, "pins_shutdown_mode", "Set shutdown mode functionality of Pin 1 and Pin 7 on the UPS DB9 communication port (Per Best Power's EPS-0059) to n (integer, 0-6)");

	blazer_makevartable_light();
}


/* == Answer preprocess functions == */

/* Preprocess the answer we got back from the UPS when queried with 'ID\r': make data begin always at the same indexes */
static int	bestups_preprocess_id_answer(item_t *item, const int len)
{
	int	i;
	char	refined[SMALLBUF] = "",
		rawval[SMALLBUF] = "",
		*token,
		*saveptr = NULL;

	if (len <= 0)
		return len;

	if (len < 25 || len > 27) {
		upsdebugx(4, "%s: wrong length [%s: %d]", __func__, item->info_type, len);
		return -1;
	}

	/* e.g.:
	 *  1. item->answer = "FOR,750,120,120,20.0,27.6\r";	len = 26
	 *  2. item->answer = "FOR,1500,120,120,20.0,27.6\r";	len = 27
	 *  3. item->answer = "FOR,3000,120,120,20.0,100.6\r";	len = 28 */
	upsdebugx(4, "read: '%.*s'", (int)strcspn(item->answer, "\r"), item->answer);

	snprintf(rawval, sizeof(rawval), "%s", item->answer);

	for (i = 1, token = strtok_r(rawval, ",", &saveptr); token != NULL; i++, token = strtok_r(NULL, ",", &saveptr)) {

		switch (i)
		{
		case 1:
			snprintf(refined, sizeof(refined), "%s", token);
			continue;
		case 2:	/* Output power */
			snprintfcat(refined, sizeof(refined), ",%4s", token);
			continue;
		case 6:	/* Battery voltage at full charge (+ trailing CR) */
			snprintfcat(refined, sizeof(refined), ",%6s", token);
			continue;
		default:
			snprintfcat(refined, sizeof(refined), ",%s", token);
		}

	}

	if (i != 7 || strlen(refined) != 28) {
		upsdebugx(2, "noncompliant reply: '%.*s'", (int)strcspn(refined, "\r"), refined);
		return -1;
	}

	upsdebugx(4, "read: '%.*s'", (int)strcspn(refined, "\r"), refined);

	/* e.g.: item->answer = "FOR, 750,120,120,20.0, 27.6\r"; len = 28 */
	return snprintf(item->answer, sizeof(item->answer), "%s", refined);
}


/* == Preprocess functions == */

/* *SETVAR(/NONUT)* Preprocess setvars */
static int	bestups_process_setvar(item_t *item, char *value, const size_t valuelen)
{
	if (!strlen(value)) {
		upsdebugx(2, "%s: value not given for %s", __func__, item->info_type);
		return -1;
	}

	double	val = strtod(value, NULL);

	if (!strcasecmp(item->info_type, "pins_shutdown_mode")) {

		if (val == pins_shutdown_mode) {
			upslogx(LOG_INFO, "%s is already set to %.0f", item->info_type, val);
			return -1;
		}

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
	snprintf(value, valuelen, item->command, val);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}

/* Bypass/Boost or Buck status */
static int	bestups_process_bbb_status_bit(item_t *item, char *value, const size_t valuelen)
{
	/* Bypass/Boost/Buck bit is not reliable when a battery test, shutdown or on battery condition occurs: always ignore it in these cases */
	if (!(qx_status() & STATUS(OL)) || (qx_status() & (STATUS(CAL) | STATUS(FSD)))) {

		if (item->value[0] == '1')
			item->value[0] = '0';

		return blazer_process_status_bits(item, value, valuelen);

	}

	/* UPSes with inverted bypass/boost/buck bit */
	if (inverted_bbb_bit) {

		if (item->value[0] == '1')
			item->value[0] = '0';

		else if (item->value[0] == '0')
			item->value[0] = '1';

	}

	return blazer_process_status_bits(item, value, valuelen);
}

/* Identify UPS manufacturer */
static int	bestups_manufacturer(item_t *item, char *value, const size_t valuelen)
{
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif

	/* Best Power devices */
	if (
		!strcmp(item->value, "AX1") ||
		!strcmp(item->value, "FOR") ||
		!strcmp(item->value, "FTC") ||
		!strcmp(item->value, "PR2") ||
		!strcmp(item->value, "PRO")
	) {
		snprintf(value, valuelen, item->dfl, "Best Power");
		return 0;
	}

	/* Sola Australia devices */
	if (
		!strcmp(item->value, "325") ||
		!strcmp(item->value, "520") ||
		!strcmp(item->value, "620")
	) {
		snprintf(value, valuelen, item->dfl, "Sola Australia");
		return 0;
	}

	/* Unknown devices */
	snprintf(value, valuelen, item->dfl, "Unknown");

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}

/* Identify UPS model and unskip qx2nut table's items accordingly */
static int	bestups_model(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif

	/* Best Power devices */

	if (!strcmp(item->value, "AX1")) {

		snprintf(value, valuelen, item->dfl, "Axxium Rackmount");

	} else if (!strcmp(item->value, "FOR")) {

		snprintf(value, valuelen, item->dfl, "Fortress");

	} else if (!strcmp(item->value, "FTC")) {

		snprintf(value, valuelen, item->dfl, "Fortress Telecom");

	} else if (!strcmp(item->value, "PR2")) {

		snprintf(value, valuelen, item->dfl, "Patriot Pro II");
		inverted_bbb_bit = 1;

	} else if (!strcmp(item->value, "PRO")) {

		snprintf(value, valuelen, item->dfl, "Patriot Pro");
		inverted_bbb_bit = 1;

	/* Sola Australia devices */
	} else if (
		!strcmp(item->value, "320") ||
		!strcmp(item->value, "325") ||
		!strcmp(item->value, "520") ||
		!strcmp(item->value, "525") ||
		!strcmp(item->value, "620")
	) {

		snprintf(value, valuelen, "Sola %s", item->value);

	/* Unknown devices */
	} else {

		snprintf(value, valuelen, item->dfl, "Unknown (%s)", item->value);
		upslogx(LOG_INFO, "Unknown model detected - please report this ID: '%s'", item->value);

	}

	/* Unskip qx2nut table's items according to the UPS model */

	/* battery.runtime var is not available on the Patriot Pro/Sola 320 model series: leave it skipped in these cases, otherwise unskip it */
	if (strcmp(item->value, "PRO") && strcmp(item->value, "320")) {

		unskip = find_nut_info("battery.runtime", 0, 0);

		/* Don't know what happened */
		if (!unskip)
			return -1;

		unskip->qxflags &= ~QX_FLAG_SKIP;

	}

	/* battery.packs var is available only on the Axxium/Sola 620 model series: unskip it in these cases */
	if (!strcmp(item->value, "AX1") || !strcmp(item->value, "620")) {

		unskip = find_nut_info("battery.packs", 0, QX_FLAG_SETVAR);

		/* Don't know what happened */
		if (!unskip)
			return -1;

		unskip->qxflags &= ~QX_FLAG_SKIP;

	}

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}

/* Battery runtime */
static int	bestups_batt_runtime(item_t *item, char *value, const size_t valuelen)
{
	double	runtime;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* Battery runtime is reported by the UPS in minutes, NUT expects seconds */
	runtime = strtod(item->value, NULL) * 60;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	snprintf(value, valuelen, item->dfl, runtime);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}

/* Battery packs */
static int	bestups_batt_packs(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;

	if (strspn(item->value, "0123456789 ") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
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
	snprintf(value, valuelen, item->dfl, strtol(item->value, NULL, 10));
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	/* Unskip battery.packs setvar */
	unskip = find_nut_info("battery.packs", QX_FLAG_SETVAR, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* *NONUT* Get shutdown mode functionality of Pin 1 and Pin 7 on the UPS DB9 communication port (Per Best Power's EPS-0059) as set in the UPS */
static int	bestups_get_pins_shutdown_mode(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;

	if (strspn(item->value, "0123456789") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	pins_shutdown_mode = strtol(item->value, NULL, 10);

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	snprintf(value, valuelen, item->dfl, pins_shutdown_mode);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	/* We were not asked by the user to change the value */
	if ((item->qxflags & QX_FLAG_NONUT) && !getval(item->info_type))
		return 0;

	/* Unskip setvar */
	unskip = find_nut_info(item->info_type, QX_FLAG_SETVAR, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* Voltage settings */
static int	bestups_voltage_settings(item_t *item, char *value, const size_t valuelen)
{
	int		index, val;
	const char	*nominal_voltage;
	const struct {
		const int	low;		/* Low voltage		->	input.transfer.low / input.transfer.boost.low */
		const int	boost;		/* Boost voltage	->	input.transfer.boost.high */
		const int	nominal;	/* Nominal voltage	->	input.voltage.nominal / output.voltage.nominal */
		const int	buck;		/* Buck voltage		->	input.transfer.trim.low */
		const int	high;		/* High voltage		->	input.transfer.high / input.transfer.trim.high */
	} voltage_settings[] = {
		/* U models voltage limits, for:
		 * - Fortress (750U, 1050U, 1425U, 1800U and 2250U)
		 * - Fortress Rackmount (750, 1050, 1425, 1800, and 2250 VA)
		 * - Patriot Pro II (400U, 750U, and 1000U) */
	/* M	  low	boost	nominal	buck	high */
	/* 0 */	{ 96,	109,	120,	130,	146 },	/* LEDs lit: 2,3,4 (Default) */
	/* 1 */	{ 96,	109,	120,	138,	156 },	/* LEDs lit: 1,3,4 */
	/* 2 */	{ 90,	104,	120,	130,	146 },	/* LEDs lit: 2,3,5 */
	/* 3 */	{ 90,	104,	120,	138,	156 },	/* LEDs lit: 1,3,5 */
	/* 4 */	{ 90,	104,	110,	120,	130 },	/* LEDs lit: 3,4,5 */
	/* 5 */	{ 90,	104,	110,	130,	146 },	/* LEDs lit: 2,4,5 */
	/* 6 */	{ 90,	96,	110,	120,	130 },	/* LEDs lit: 3,4,6 */
	/* 7 */	{ 90,	96,	110,	130,	146 },	/* LEDs lit: 2,4,6 */
	/* 8 */	{ 96,	109,	128,	146,	156 },	/* LEDs lit: 1,2,4 */
	/* 9 */	{ 90,	104,	128,	146,	156 },	/* LEDs lit: 1,2,5 */

		/* E models voltage limits, for:
		 * - Fortress (750E, 1050E, 1425E, and 2250E)
		 * - Fortress Rackmount (750, 1050, 1425, and 2250 VA)
		 * - Patriot Pro II (400E, 750E, and 1000E) */
	/* M	  low	boost	nominal	buck	high */
	/* 0 */	{ 200,	222,	240,	250,	284 },	/* LEDs lit: 2,3,4 */
	/* 1 */	{ 200,	222,	240,	264,	290 },	/* LEDs lit: 1,3,4 */
	/* 2 */	{ 188,	210,	240,	250,	284 },	/* LEDs lit: 2,3,5 */
	/* 3 */	{ 188,	210,	240,	264,	290 },	/* LEDs lit: 1,3,5 */
	/* 4 */	{ 188,	210,	230,	244,	270 },	/* LEDs lit: 3,4,5 (Default) */
	/* 5 */	{ 188,	210,	230,	250,	284 },	/* LEDs lit: 2,4,5 */
	/* 6 */	{ 180,	200,	230,	244,	270 },	/* LEDs lit: 3,4,6 */
	/* 7 */	{ 180,	200,	230,	250,	284 },	/* LEDs lit: 2,4,6 */
	/* 8 */	{ 165,	188,	208,	222,	244 },	/* LEDs lit: 4,5,6 */
	/* 9 */	{ 165,	188,	208,	244,	270 }	/* LEDs lit: 3,5,6 */
	};

	if (strspn(item->value, "0123456789") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	index = strtol(item->value, NULL, 10);

	if (index < 0 || index > 9) {
		upsdebugx(2, "%s: value '%d' out of range [0..9]", __func__, index);
		return -1;
	}

	nominal_voltage = dstate_getinfo("input.voltage.nominal");

	if (!nominal_voltage)
		nominal_voltage = dstate_getinfo("output.voltage.nominal");

	if (!nominal_voltage) {
		upsdebugx(2, "%s: unable to get nominal voltage", __func__);
		return -1;
	}

	/* E models */
	if (strtol(nominal_voltage, NULL, 10) > 160)
		index += 10;

	if (!strcasecmp(item->info_type, "input.transfer.low") || !strcasecmp(item->info_type, "input.transfer.boost.low")) {

		val = voltage_settings[index].low;

	} else if (!strcasecmp(item->info_type, "input.transfer.boost.high")) {

		val = voltage_settings[index].boost;

	} else if (!strcasecmp(item->info_type, "input.voltage.nominal") || !strcasecmp(item->info_type, "output.voltage.nominal")) {

		val = voltage_settings[index].nominal;

	} else if (!strcasecmp(item->info_type, "input.transfer.trim.low")) {

		val = voltage_settings[index].buck;

	} else if (!strcasecmp(item->info_type, "input.transfer.trim.high") || !strcasecmp(item->info_type, "input.transfer.high")) {

		val = voltage_settings[index].high;

	} else {

		/* Don't know what happened */
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
	snprintf(value, valuelen, item->dfl, val);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}


/* == Subdriver interface == */
subdriver_t	bestups_subdriver = {
	BESTUPS_VERSION,
	bestups_claim,
	bestups_qx2nut,
	bestups_initups,
	NULL,
	bestups_makevartable,
	NULL,
	NULL,
#ifdef TESTING
	bestups_testing,
#endif	/* TESTING */
};
