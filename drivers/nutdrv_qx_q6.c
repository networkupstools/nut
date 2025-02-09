/* nutdrv_qx_q6.c - Subdriver for Q6 Megatec protocol variant
 *
 * Copyright (C)
 *   2024 Viktor Drobot <linux776@gmail.com>
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

#include "nutdrv_qx_q6.h"

#define Q6_VERSION "Q6 0.01"

/* Support functions */
static int	q6_process_topology_bits(item_t *item, char *value, const size_t valuelen);
static int	q6_claim(void);
static void	q6_initups(void);
static void	q6_initinfo(void);
static void	q6_makevartable(void);

/* qx2nut lookup table */
static item_t	q6_qx2nut[] = {

	/*
	 * > [Q6\r]
	 * < [(MMM.M MMM.M MMM.M NN.N PPP.P PPP.P PPP.P RR.R QQQ QQQ QQQ SSS.S VVV.V TT.T ttttt CCC KB ffffffff wwwwwwww YO\r]
	 *    01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
	 *    0         1         2         3         4         5         6         7         8         9         10
	 */

	/* Common parameters */
	{ "input.L1-N.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.L2-N.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	7,	11,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.L3-N.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	13,	17,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	19,	22,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.L1-N.voltage",	0,	NULL,	"Q6\r",	"",	110,	'(',	"",	24,	28,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.L2-N.voltage",	0,	NULL,	"Q6\r",	"",	110,	'(',	"",	30,	34,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.L3-N.voltage",	0,	NULL,	"Q6\r",	"",	110,	'(',	"",	36,	40,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.frequency",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	42,	45,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.L1.load",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	47,	49,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "output.L2.load",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	51,	53,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "output.L3.load",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	55,	57,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "ups.temperature",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	71,	74,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	59,	63,	"%.1f",	0,	NULL,	NULL,	qx_multiply_battvolt },
	{ "battery.runtime",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	76,	80,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "battery.charge",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	82,	84,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "experimental.battery.voltage.negative",	0,	NULL,	"Q6\r",	"",	110,	'(',	"",	65,	69,	"%.1f",	0,	NULL,	NULL,	qx_multiply_battvolt },	/* Negative battery voltage */
	{ "experimental.input.topology",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	107,	107,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	q6_process_topology_bits },	/* Input transformer topology */
	{ "experimental.output.topology",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	108,	108,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	q6_process_topology_bits },	/* Output transformer topology */
	/* TODO handle KB (86-87) properly: mb use "experimental." vars for that */

	/*
	 * > [WA\r]
	 * < [(WWW.W WWW.W WWW.W VVV.V VVV.V VVV.V TTT.T SSS.S AAA.A AAA.A AAA.A QQQ b7b6b5b4b3b2b1b0\r]
	 *    012345678901234567890123456789012345678901234567890123456789012345678901 2 3 4 5 6 7 8 9
	 *    0         1         2         3         4         5         6         7
	 */

	/* Output consumption parameters */
	{ "output.L1.realpower",	0,	NULL,	"WA\r",	"",	80,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "output.L2.realpower",	0,	NULL,	"WA\r",	"",	80,	'(',	"",	7,	11,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "output.L3.realpower",	0,	NULL,	"WA\r",	"",	80,	'(',	"",	13,	17,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "output.L1.power",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	19,	23,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "output.L2.power",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	25,	29,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "output.L3.power",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	31,	35,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "output.L1.current",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	49,	53,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.L2.current",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	55,	59,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.L3.current",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	61,	65,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.realpower",			0,	NULL,	"WA\r",	"",	80,	'(',	"",	37,	41,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "ups.power",				0,	NULL,	"WA\r",	"",	80,	'(',	"",	43,	47,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "ups.load",				0,	NULL,	"WA\r",	"",	80,	'(',	"",	67,	69,	"%.0f",	0,	NULL,	NULL,	NULL },

	/*
	 * > [Q1\r]
	 * < [(MMM.M NNN.N PPP.P QQQ RR.R S.SS TT.T b7b6b5b4b3b2b1b0\r]
	 *    012345678901234567890123456789012345678 9 0 1 2 3 4 5 6
	 *    0         1         2         3           4
	 */

	/* Status bits */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",	0,	NULL,	"Q1\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			NULL,	NULL,	blazer_process_status_bits },	/* Beeper status */

	/* Instant commands */
	{ "beeper.toggle",				0,	NULL,	"Q\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.off",					0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",					0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "shutdown.return",			0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",			0,	NULL,	"S%sR0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",				0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start",			0,	NULL,	"T%02d\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.stop",			0,	NULL,	"CT\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};

/* Testing table */
#ifdef TESTING
static testing_t	q6_testing[] = {
	{ "Q6\r",	"(227.0 225.6 230.0 50.0 229.9 000.0 000.0 49.9 007 000 000 327.8 000.0 23.0 06932 100 32 00000000 00000000 11\r",	-1 },
	{ "WA\r",	"(001.4 000.0 000.0 001.4 000.0 000.0 001.4 001.4 006.5 000.0 000.0 007 00000000\r",	-1 },
	{ "Q1\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00000000\r",	-1 },
	{ "Q\r",	"",	-1 },
	{ "S03\r",	"",	-1 },
	{ "C\r",	"",	-1 },
	{ "S02R0005\r",	"",	-1 },
	{ "S.5R0000\r",	"",	-1 },
	{ "T04\r",	"",	-1 },
	{ "TL\r",	"",	-1 },
	{ "T\r",	"",	-1 },
	{ "CT\r",	"",	-1 },
	{ NULL }
};
#endif	/* TESTING */

/* == Support functions == */

/* This function allows to process input/output transformers topology bits */
static int	q6_process_topology_bits(item_t *item, char *value, const size_t valuelen)
{
	char	*val = "";

	if (strspn(item->value, "01") != 1) {
		upsdebugx(3, "%s: unexpected value %s@%d->%s", __func__, item->value, item->from, item->value);
		return -1;
	}

	switch (item->from)
	{
	case 107:	/* Input topology - experimental.input.topology */

		if (item->value[0] == '1')
			val = "star";
		else if (item->value[0] == '0')
			val = "delta";
		else
			val = "unknown";
		break;

	case 108:	/* Output topology - experimental.output.topology */

		if (item->value[0] == '1')
			val = "star";
		else if (item->value[0] == '0')
			val = "delta";
		else
			val = "unknown";
		break;

	default:
		/* Don't know what happened */
		return -1;
	}

	snprintf(value, valuelen, "%s", val);

	return 0;
}

/* This function allows the subdriver to "claim" a device:
 * return 1 if the device is supported by this subdriver, else 0
 */
static int	q6_claim(void)
{
	/* We need Q6, Q1, and WA to use this subdriver (WA only if user hasn't requested 'nooutstats' flag) */
	/* TODO think whether we should check for BPS (and bypass-related variables) too */
	struct {
		char	*var;
		char	*cmd;
	} mandatory[] = {
		{ "input.L1-N.voltage", "Q6" },
		{ "ups.type", "Q1" },
		{ "output.L1.current", "WA"},
		{ NULL, NULL }
	};
	int vari;
	char *sp, *cmd;
	item_t *item;

	for (vari = 0; mandatory[vari].var; vari++) {
		sp = mandatory[vari].var;
		cmd = mandatory[vari].cmd;

		/* Ignore checks for 'WA' command support if explicitly asked */
		if (testvar("nooutstats") && (strcasecmp(cmd, "WA") == 0))
			continue;

		item = find_nut_info(sp, 0, 0);

		/* Don't know what happened */
		if (!item)
			return 0;

		/* No reply/Unable to get value */
		if (qx_process(item, NULL))
			return 0;

		/* Unable to process value */
		if (ups_infoval_set(item) != 1)
			return 0;
	}

	return 1;
}

/* Subdriver-specific initups */
static void	q6_initups(void)
{
	int	nr, nos, isb;
	item_t	*item;

	nr = testvar("norating");
	nos = testvar("nooutstats");
	isb = testvar("ignoresab");

	if (!nr && !nos && !isb)
		return;

	for (item = q6_qx2nut; item->info_type != NULL; item++) {

		if (!item->command)
			continue;

		/* norating */
		if (nr && (strcasecmp(item->command, "F\r") == 0)) {
			upsdebugx(2, "%s: skipping %s", __func__, item->info_type);
			item->qxflags |= QX_FLAG_SKIP;
		}

		/* nooutstats */
		if (nos && (strcasecmp(item->command, "WA\r") == 0)) {
			upsdebugx(2, "%s: skipping %s", __func__, item->info_type);
			item->qxflags |= QX_FLAG_SKIP;
		}

		/* ignoresab */
		if (isb && (strcasecmp(item->info_type, "ups.status") == 0) && item->from == 44 && item->to == 44) {
			upsdebugx(2, "%s: skipping %s ('Shutdown Active' bit)", __func__, item->info_type);
			item->qxflags |= QX_FLAG_SKIP;
		}
	}
}

/* Subdriver-specific initinfo */
static void	q6_initinfo(void)
{
	dstate_setinfo("input.phases", "%d", 3);
	dstate_setinfo("output.phases", "%d", 3);
}

/* Subdriver-specific flags/vars */
static void	q6_makevartable(void)
{
	addvar(VAR_FLAG, "norating", "Skip reading rating information from UPS");
	addvar(VAR_FLAG, "nooutstats", "Skip reading output load stats information from UPS");

	blazer_makevartable_light();
}

/* Subdriver interface */
subdriver_t	q6_subdriver = {
	Q6_VERSION,
	q6_claim,
	q6_qx2nut,
	q6_initups,
	q6_initinfo,
	q6_makevartable,
	"ACK",
	NULL,
#ifdef TESTING
	q6_testing,
#endif	/* TESTING */
};
