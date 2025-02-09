/* nutdrv_qx_innovart31.c - Subdriver for INNOVA RT 3/1 UPSes
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

#include "nutdrv_qx_innovart31.h"

#define INNOVART31_VERSION "INNOVART31 0.02"

/* Support functions */
static int	innovart31_claim(void);
static void	innovart31_initups(void);
static void	innovart31_initinfo(void);

/* qx2nut lookup table */
static item_t	innovart31_qx2nut[] = {

	/*
	 * > [Q6\r]
	 * < [(227.0 225.6 230.0 50.0 229.9 000.0 000.0 49.9 007 000 000 327.8 000.0 23.0 06932 100 32 00000000 00000000 11\r]
	 *    01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
	 *    0         1         2         3         4         5         6         7         8         9         10
	 */

	/* Common parameters */
	{ "input.L1-N.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.L2-N.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	7,	11,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.L3-N.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	13,	17,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	19,	22,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	24,	28,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.frequency",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	42,	45,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	47,	49,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "ups.temperature",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	71,	74,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	59,	63,	"%.1f",	0,	NULL,	NULL,	qx_multiply_battvolt },
	{ "battery.runtime",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	76,	80,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "battery.charge",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	82,	84,	"%.0f",	0,	NULL,	NULL,	NULL },

	/*
	 * > [WA\r]
	 * < [(001.4 000.0 000.0 001.4 000.0 000.0 001.4 001.4 006.5 000.0 000.0 007 00000000\r]
	 *    01234567890123456789012345678901234567890123456789012345678901234567890123456789
	 *    0         1         2         3         4         5         6         7
	 */

	/* Output consumption parameters */
	{ "output.current",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	49,	53,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.realpower",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	37,	41,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "ups.power",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	43,	47,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },

	/*
	 * > [BPS\r]
	 * < [(230.4 000.0 000.0 49.9\r]
	 *    012345678901234567890123
	 *    0         1         2
	 */

	/* Bypass parameters */
	{ "input.bypass.voltage",		0,	NULL,	"BPS\r",	"",	24,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.bypass.frequency",		0,	NULL,	"BPS\r",	"",	24,	'(',	"",	19,	22,	"%.1f",	0,	NULL,	NULL,	NULL },

	/*
	 * > [F\r]
	 * < [#230.0 087 288.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	/* Nominal parameters (ratings) */
	{ "input.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	5,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.current.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	7,	9,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	11,	15,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.frequency.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	17,	20,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/*
	 * > [FW?\r]
	 * < [05516-1100\r]
	 *    01234567890
	 *    0         1
	 */

	/* Firmware version */
	{ "ups.firmware",	0,	NULL,	"FW?\r",	"",	0,	0,	"",	0,	0,	"%s",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/*
	 * > [SASV07?\r]
	 * < [GASV07DN05K10KB100004      \r]
	 *    0123456789012345678901234567
	 *    0         1         2
	 */

	/* UPS serial number */
	{ "ups.serial",	0,	NULL,	"SASV07?\r",	"",	28,	0,	"",	7,	0,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },

	/*
	 * > [Q1\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */

	/* Status bits */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			NULL,	NULL,	blazer_process_status_bits },	/* Beeper status */

	/* Instant commands */
	/* NB: seems like beeper toggling doesn't work for Innova RT 3/1 by Ippon - UPS simply doesn't respond  */
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start",		0,	NULL,	"T%02d\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};

/* Testing table */
#ifdef TESTING
static testing_t	innovart31_testing[] = {
	{ "Q6\r",	"(227.0 225.6 230.0 50.0 229.9 000.0 000.0 49.9 007 000 000 327.8 000.0 23.0 06932 100 32 00000000 00000000 11\r",	-1 },
	{ "Q1\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00000000\r",	-1 },
	{ "WA\r",	"(001.4 000.0 000.0 001.4 000.0 000.0 001.4 001.4 006.5 000.0 000.0 007 00000000\r",	-1 },
	{ "BPS\r",	"(230.4 000.0 000.0 49.9\r",	-1 },
	{ "F\r",	"#230.0 087 288.0 50.0\r",	-1 },
	{ "FW?\r",	"05516-1100\r",	-1 },
	{ "SASV07?\r",	"GASV07DN05K10KB100004      \r",	-1 },
	{ "S03\r",	"",	-1 },
	{ "C\r",	"",	-1 },
	{ "S02R0005\r",	"",	-1 },
	{ "S.5R0000\r",	"",	-1 },
	{ "T04\r",	"ACK",	-1 },
	{ "TL\r",	"ACK",	-1 },
	{ "T\r",	"ACK",	-1 },
	{ "CT\r",	"ACK",	-1 },
	{ NULL }
};
#endif	/* TESTING */

/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0 */
static int	innovart31_claim(void)
{
	/* We need Q6, Q1, WA, BPS, F, FW? and SASV07? to use this subdriver */
	struct {
		char	*var;
		char	*cmd;
	} mandatory[] = {
		{ "input.L1-N.voltage", "Q6" },
		{ "ups.type", "Q1" },
		{ "output.current", "WA" },
		{ "input.bypass.voltage", "BPS" },
		{ "input.voltage.nominal", "F" },
		{ "ups.firmware", "FW?" },
		{ "ups.serial", "SASV07?" },
		{ NULL, NULL}
	};
	int vari;
	char *sp;
	item_t *item;

	for (vari = 0; mandatory[vari].var; vari++) {
		sp = mandatory[vari].var;
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
static void	innovart31_initups(void)
{
	blazer_initups_light(innovart31_qx2nut);
}

/* Subdriver-specific initinfo */
static void	innovart31_initinfo(void)
{
	dstate_setinfo("input.phases", "%d", 3);
	dstate_setinfo("input.bypass.phases", "%d", 1);
	dstate_setinfo("output.phases", "%d", 1);
}

/* Subdriver interface */
subdriver_t	innovart31_subdriver = {
	INNOVART31_VERSION,
	innovart31_claim,
	innovart31_qx2nut,
	innovart31_initups,
	innovart31_initinfo,
	blazer_makevartable_light,
	"ACK",
	"NAK\r",
#ifdef TESTING
	innovart31_testing,
#endif	/* TESTING */
};
