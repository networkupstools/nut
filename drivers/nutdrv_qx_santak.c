/* nutdrv_qx_santak.c - Subdriver for single-phase Santak Castle C*K UPSes
 *
 * Copyright (C)
 *   2026 NUT community contribution
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

/* NOTE:
 * This subdriver speaks the same Q6 dialect as 'innovart31' and 'q6', but for a
 * SINGLE-PHASE device. Developed and tested against a Santak Castle C1K
 * (online, 1 kVA, two 12 V batteries) over RS232 at 2400 8N1.
 *
 * Differences from 'innovart31', which is otherwise the closest sibling:
 *
 *   1. Single-phase input. The Q6, WA and BPS replies carry the placeholder
 *      "---.-" in every L2/L3 slot, so those items are omitted entirely and
 *      'input.voltage' is reported unqualified rather than as
 *      'input.L1-N.voltage'. input/output/bypass phases are all 1.
 *
 *   2. The 'F' (ratings) query is NAKed by this device, so the nominal-rating
 *      items are absent and claim() must not require them. 'innovart31' lists
 *      'input.voltage.nominal' as mandatory and therefore rejects this UPS
 *      outright with "Device not supported!".
 *
 *   (An earlier revision claimed the 'SASV07?' reply was not space-padded here
 *   and dropped its answer_len. That was wrong: a capture harness had stripped
 *   the trailing spaces. The reply is 'GASV07D<serial>    ' = 27 bytes + CR,
 *   exactly the 28 'innovart31' expects, so that item is unchanged.)
 *
 * Also NAKed by this device, for the record: I, F, Q2, Q3, Q5, QS, QMOD, QFLAG,
 * QGS, QMD, QID, QRI, QBV, QBYV, QBYF, QPI, QVFW, QDI, QBT, QT, QMF, QENF,
 * QFET, QOPF, QFRE, QCHT, QLDL, SS?, BP?. In particular there is no QFLAG, so
 * this unit exposes no ECO / energy-save flag to query or set.
 *
 * Two further commands DO answer but are deliberately left unmapped, because
 * their meaning could only be guessed at without vendor documentation:
 *
 *   > [Q4\r]
 *   < 07 08 03 06 00 09 FF 27 01 07 01 03 00 00 00 00 02 0D   (binary, 17 B + CR)
 *     Byte-identical across repeated samples, so it is static configuration
 *     rather than telemetry.
 *
 *   > [SASV08?\r]
 *   < [GASV080001\r]
 *     A second register alongside SASV07 (the serial). Sweeping SASV01? through
 *     SASV24? showed only 07 and 08 exist.
 *
 * Mapping either onto NUT variables would be speculation, so they are recorded
 * here for whoever gets protocol documentation rather than reported as data.
 *
 * 'ups.model' is read from the device (WH), but 'device.mfr' is deliberately
 * NOT hardcoded. This is an OEM Q6 platform sold under several brands, and a
 * subdriver that stamps a vendor name onto every device its claim() accepts is
 * how 'innovatae' ends up labelling non-Ippon hardware as Ippon. Users who want
 * a manufacturer string can set 'override.ups.mfr' in ups.conf.
 */

#include "main.h"
#include "nutdrv_qx.h"
#include "nutdrv_qx_blazer-common.h"

#include "nutdrv_qx_santak.h"

#define SANTAK_VERSION "SANTAK 0.01"

/* Offsets of the input L2/L3 voltage fields inside the Q6 reply, used by
 * claim() to tell a single-phase device from a three-phase one. */
#define SANTAK_Q6_L2_OFFSET	7
#define SANTAK_Q6_L3_OFFSET	13
#define SANTAK_Q6_PLACEHOLDER	"---.-"

static int	santak_claim(void);
static void	santak_initups(void);
static void	santak_initinfo(void);

/* qx2nut lookup table */
static item_t	santak_qx2nut[] = {

	/*
	 * > [Q6\r]
	 * < [(225.4 ---.- ---.- 49.9 226.3 ---.- ---.- 49.9 013 --- --- 027.1 ---.- 26.5 02293 100 82 00000000 00000000 11\r]
	 *    01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
	 *    0         1         2         3         4         5         6         7         8         9         10
	 *
	 * L2/L3 input (7-11, 13-17) and output (30-34, 36-40) read "---.-" on a
	 * single-phase unit, as do the per-phase load fields (51-53, 55-57), so
	 * none of them are mapped.
	 */

	/* Common parameters */
	{ "input.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	19,	22,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	24,	28,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.frequency",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	42,	45,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",			0,	NULL,	"Q6\r",	"",	110,	'(',	"",	47,	49,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	59,	63,	"%.1f",	0,	NULL,	NULL,	qx_multiply_battvolt },
	{ "ups.temperature",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	71,	74,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.runtime",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	76,	80,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "battery.charge",		0,	NULL,	"Q6\r",	"",	110,	'(',	"",	82,	84,	"%.0f",	0,	NULL,	NULL,	NULL },

	/*
	 * > [WA\r]
	 * < [(000.1 ---.- ---.- 000.1 ---.- ---.- 000.1 000.1 000.6 ---.- ---.- 014 00000001\r]
	 *    01234567890123456789012345678901234567890123456789012345678901234567890123456789
	 *    0         1         2         3         4         5         6         7
	 */

	/* Output consumption parameters */
	{ "output.current",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	49,	53,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.realpower",		0,	NULL,	"WA\r",	"",	80,	'(',	"",	37,	41,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },
	{ "ups.power",			0,	NULL,	"WA\r",	"",	80,	'(',	"",	43,	47,	"%.1f",	0,	NULL,	NULL,	qx_multiply_x1000 },

	/*
	 * > [BPS\r]
	 * < [(225.4 ---.- ---.- 49.9\r]
	 *    012345678901234567890123
	 *    0         1         2
	 */

	/* Bypass parameters */
	{ "input.bypass.voltage",	0,	NULL,	"BPS\r",	"",	24,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.bypass.frequency",	0,	NULL,	"BPS\r",	"",	24,	'(',	"",	19,	22,	"%.1f",	0,	NULL,	NULL,	NULL },

	/*
	 * > [WH\r]
	 * < [(00 10.35 00.00 C1K(G7)                    1/1 6 220 50.00 012 02.00 004.00 011.30 006 00 50 187 264 45.00 55.00\r]
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901
	 *    0         1         2         3         4         5         6         7         8         9         10        11
	 *
	 * Ratings/identification frame. This is what makes the device usable despite
	 * 'F' and 'I' both being NAKed: model, nominal output and the transfer window
	 * all come from here instead.
	 *
	 * Field at 43-45 is the topology, "1/1" on this unit (input phases / output
	 * phases). It is not mapped - initinfo() states the phases directly - but it
	 * is the device's own declaration and a sibling subdriver for another
	 * topology could key on it.
	 *
	 * Offsets 1-2, 4-8, 10-14, 47, 59-61, 63-67, 69-74, 76-81, 83-85, 87-88 and
	 * 90-91 are of unknown meaning and deliberately left unmapped.
	 */

	{ "ups.model",			0,	NULL,	"WH\r",	"",	113,	'(',	"",	16,	42,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "output.voltage.nominal",	0,	NULL,	"WH\r",	"",	113,	'(',	"",	49,	51,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "output.frequency.nominal",	0,	NULL,	"WH\r",	"",	113,	'(',	"",	53,	57,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.transfer.low",		0,	NULL,	"WH\r",	"",	113,	'(',	"",	93,	95,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.transfer.high",	0,	NULL,	"WH\r",	"",	113,	'(',	"",	97,	99,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.frequency.low",	0,	NULL,	"WH\r",	"",	113,	'(',	"",	101,	105,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.frequency.high",	0,	NULL,	"WH\r",	"",	113,	'(',	"",	107,	111,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/*
	 * > [FW?\r]
	 * < [08477-0307\r]
	 *    01234567890
	 *    0         1
	 */

	/* Firmware version */
	{ "ups.firmware",		0,	NULL,	"FW?\r",	"",	0,	0,	"",	0,	0,	"%s",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/*
	 * > [SASV07?\r]
	 * < [GASV07DBFPB-A9492270135    \r]
	 *    012345678901234567890123456
	 *    0         1         2
	 *
	 * The leading "GASV07D" is a constant echo of the query and is skipped.
	 * The reply is space-padded to 27 bytes + CR, same as Innova units, hence
	 * answer_len 28.
	 */

	/* UPS serial number */
	{ "ups.serial",			0,	NULL,	"SASV07?\r",	"",	28,	0,	"",	7,	0,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },

	/*
	 * > [Q1\r]
	 * < [(225.8 225.8 226.3 014 49.9 2.25 26.5 00000001\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 *
	 * Only the status bits are taken from Q1: its battery.voltage field is
	 * reported per cell (2.25 V) whereas Q6 gives the pack voltage directly.
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
	/* NB: these follow the common blazer/Q* set. They were NOT exercised against
	 * the Castle C1K used for development, which was powering live equipment. */
	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
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
static testing_t	santak_testing[] = {
	{ "Q6\r",	"(225.4 ---.- ---.- 49.9 226.3 ---.- ---.- 49.9 013 --- --- 027.1 ---.- 26.5 02293 100 82 00000000 00000000 11\r",	-1 },
	{ "Q1\r",	"(225.8 225.8 226.3 014 49.9 2.25 26.5 00000001\r",	-1 },
	{ "WA\r",	"(000.1 ---.- ---.- 000.1 ---.- ---.- 000.1 000.1 000.6 ---.- ---.- 014 00000001\r",	-1 },
	{ "BPS\r",	"(225.4 ---.- ---.- 49.9\r",	-1 },
	{ "WH\r",	"(00 10.35 00.00 C1K(G7)                    1/1 6 220 50.00 012 02.00 004.00 011.30 006 00 50 187 264 45.00 55.00\r",	-1 },
	{ "FW?\r",	"08477-0307\r",	-1 },
	{ "SASV07?\r",	"GASV07DBFPB-A9492270135    \r",	-1 },
	{ "F\r",	"NAK\r",	-1 },
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

/* This function allows the subdriver to "claim" a device: return 1 if the
 * device is supported by this subdriver, else 0.
 *
 * This subdriver is listed ahead of the generic 'q2'/'q6' probes, so the claim
 * has to be strict enough not to steal a three-phase Innova. Two independent
 * gates do that: the mandatory command set (which 'q6' devices need not answer
 * in full), and the single-phase placeholder signature in the Q6 reply.
 */
static int	santak_claim(void)
{
	/* We need Q6, Q1, WA, BPS, WH, FW? and SASV07?. Deliberately NOT F: this
	 * device NAKs it, which is what 'innovart31' trips over. */
	struct {
		char	*var;
		char	*cmd;
	} mandatory[] = {
		{ "input.voltage", "Q6" },
		{ "ups.type", "Q1" },
		{ "output.current", "WA" },
		{ "input.bypass.voltage", "BPS" },
		{ "ups.model", "WH" },
		{ "ups.firmware", "FW?" },
		{ "ups.serial", "SASV07?" },
		{ NULL, NULL }
	};
	int	vari;
	char	*sp;
	item_t	*item;
	item_t	*q6item = NULL;

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

		/* Keep the Q6-sourced item: its answer buffer holds the whole
		 * Q6 reply, which the phase check below inspects. */
		if (!strcasecmp(mandatory[vari].cmd, "Q6"))
			q6item = item;
	}

	if (!q6item)
		return 0;

	/* Single-phase signature: both input L2 and L3 must read as the "---.-"
	 * placeholder. A three-phase device reports real voltages there, so this
	 * is what keeps Innova RT 3/1 and RT 3/3 units on their own subdrivers. */
	if (strlen(q6item->answer) < SANTAK_Q6_L3_OFFSET + strlen(SANTAK_Q6_PLACEHOLDER)) {
		upsdebugx(2, "%s: Q6 answer too short for a phase check", __func__);
		return 0;
	}

	if (strncmp(q6item->answer + SANTAK_Q6_L2_OFFSET, SANTAK_Q6_PLACEHOLDER, strlen(SANTAK_Q6_PLACEHOLDER)) ||
	    strncmp(q6item->answer + SANTAK_Q6_L3_OFFSET, SANTAK_Q6_PLACEHOLDER, strlen(SANTAK_Q6_PLACEHOLDER))) {
		upsdebugx(2,
			"%s: device reports real L2/L3 input voltages, "
			"so it is not single-phase; leaving it to another subdriver",
			__func__);
		return 0;
	}

	return 1;
}

/* Subdriver-specific initups */
static void	santak_initups(void)
{
	blazer_initups_light(santak_qx2nut);
}

/* Subdriver-specific initinfo */
static void	santak_initinfo(void)
{
	dstate_setinfo("input.phases", "%d", 1);
	dstate_setinfo("input.bypass.phases", "%d", 1);
	dstate_setinfo("output.phases", "%d", 1);
}

/* Subdriver interface */
subdriver_t	santak_subdriver = {
	SANTAK_VERSION,
	santak_claim,
	santak_qx2nut,
	santak_initups,
	santak_initinfo,
	blazer_makevartable_light,
	"ACK",
	"NAK\r",
#ifdef TESTING
	santak_testing,
#endif	/* TESTING */
};
