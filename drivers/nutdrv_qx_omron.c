/* nutdrv_qx_omron.c - Subdriver for OMRON UPSes speaking the Q1 protocol
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>	(nutdrv_qx_q1.c, on which this is based)
 *   2026 Jun Kurihara <junkurihara@users.noreply.github.com>
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
 * This subdriver is the 'q1' subdriver plus the OMRON-specific corrections
 * listed below. It has been tested on a BN150T (USB 0590:00b7) only; other
 * OMRON models are untested. What was exercised on that unit is the reading
 * side: Q1 polling and the capability queries. No instant command has ever
 * been sent to it, so every command string below rests on the vendor source
 * alone.
 *
 * Differences from 'q1':
 *
 * - The third status bit of the Q1 reply ("Bypass/Boost or Buck Active",
 *   index 2 of the status field) is not published. On a BN150T running on
 *   mains, idle and with nothing connected to its output, that bit reads 1 on
 *   every poll, and 'q1' maps it unconditionally to the BYPASS status - which
 *   in NUT means that protection has been bypassed, i.e. an abnormal state.
 *   OMRON's own driver deliberately does not evaluate this bit either: the
 *   block that would decide between TRIM/BYPASS/BOOST is commented out in its
 *   entirety, identically in the 1.00 and 1.02 releases of the vendor source,
 *   while every other status bit is handled. What the bit actually reports on
 *   this hardware is unknown - resolving it needs an observed on-battery
 *   event - so nothing is published for it.
 *
 * - The instant commands that OMRON's own driver does not implement are not
 *   registered: test.battery.start, test.battery.start.deep,
 *   test.battery.start.quick and test.battery.stop (the vendor source has no
 *   T or TL command at all), load.on, load.off, and beeper.toggle (no Q
 *   command; the vendor uses Bn/Bf to enable/disable the beeper instead).
 *   Registering an instant command publishes it to NUT clients as something
 *   the device can perform, so listing commands that may not exist is a defect
 *   in its own right.
 *
 * - The shutdown commands use OMRON's own byte strings rather than the Megatec
 *   ones, and shutdown.return / shutdown.stayoff are two transactions, not one:
 *   the auto-restart flag is set first and its failure aborts the command. See
 *   omron_start_auto() and omron_process_command() below.
 *
 * - A command accepted by the UPS is acknowledged with "OK", not with the "ACK"
 *   that the Megatec-derived subdrivers expect.
 *
 * - The claim function additionally requires OMRON's USB vendor ID, see
 *   omron_claim() below.
 *
 */

#include "main.h"
#include "nutdrv_qx.h"
#include "nutdrv_qx_blazer-common.h"

#include "nutdrv_qx_omron.h"

#define OMRON_VERSION "Omron 0.01"

/* OMRON's USB vendor ID, as published in 'ups.vendorid' by nutdrv_qx.
 * Keep in sync with OMRON_VENDORID in nutdrv_qx.c */
#define OMRON_USB_VENDORID	"0590"

/* What the UPS answers to a command it accepted. The Megatec-derived
 * subdrivers expect "ACK"; OMRON acknowledges with "OK" and its own driver
 * tests it with strncmp(buf, "OK", 2). */
#define OMRON_ACCEPTED		"OK"

/* Set the UPS's auto-restart flag ahead of a shutdown command
 *
 * "An" enables restarting when mains returns, "Af" disables it. The vendor
 * driver sends the matching one *before* the shutdown command itself, and
 * abandons the instant command if the UPS answers it with anything other than
 * "OK"; identical code in both releases. Without it, shutdown.return can leave
 * a unit whose auto-restart flag happens to be off dead after mains returns.
 *
 * This is also NUT's documented convention, not just OMRON's: docs/nut-names.txt
 * states that shutdown commands set ups.start.auto to the matching value first.
 * nutdrv_qx implements that in the instcmd() fallback path, but the fallback
 * only runs for commands *absent* from the subdriver table, so a subdriver that
 * registers shutdown.return itself has to do this for itself.
 *
 * Divergence from the vendor, deliberate. The vendor has three outcomes: it
 * skips the prologue entirely when its CF capability bitmap says the command
 * is unsupported; it abandons the shutdown on an explicit non-"OK" answer; and
 * it proceeds to shut down anyway when the transfer itself fails or nothing
 * comes back, because the guard around the reply test has no else branch.
 * This driver has no CF gating, and it abandons the shutdown in all three of
 * transfer failure, no answer, and non-"OK" answer. A shutdown that did not
 * happen is visible in the logs and reversible; a shutdown whose auto-restart
 * state is unknown may need someone at the front panel. */
static int	omron_start_auto(const int on)
{
	/* A throwaway item, deliberately not a row of omron_qx2nut[]: this
	 * transaction happens inside the preprocessing of another item, and
	 * qx_process() stores the reply in whichever item it is handed - it
	 * must not be the one instcmd() is working on. answer_len 0 and
	 * leading 0 accept any reply; the OMRON_ACCEPTED test below is the one
	 * that decides. */
	item_t	prologue = {
		on ? "ups.start.auto (An)" : "ups.start.auto (Af)",
		0,	NULL,
		on ? "An\r" : "Af\r",
		"",	0,	0,
		"",	0,	0,
		"%s",	QX_FLAG_NONUT,
		NULL,	NULL,	NULL
	};

	/* A rejected query is also a -1 here, not an answer to compare against:
	 * qx_process_answer() matches subdriver->rejected before anything else */
	if (qx_process(&prologue, NULL)) {
		upslogx(LOG_ERR, "%s: [%.*s] failed or was rejected", __func__,
			(int)strcspn(prologue.command, "\r"), prologue.command);
		return -1;
	}

	/* Covers an empty answer too: answer_len is 0, so a zero-length reply
	 * reaches this point rather than being caught as a short one */
	if (strcasecmp(prologue.value, OMRON_ACCEPTED)) {
		upslogx(LOG_ERR, "%s: [%.*s] not acknowledged, answer was [%s]",
			__func__,
			(int)strcspn(prologue.command, "\r"), prologue.command,
			prologue.value);
		return -1;
	}

	dstate_setinfo("ups.start.auto", "%s", on ? "yes" : "no");

	return 0;
}

/* Build the OMRON form of a shutdown command
 *
 * OMRON expresses the delay either in tenths of a minute (".n", i.e. 6-second
 * steps) below one minute, or in whole minutes ("nn") from one minute up, and
 * never appends the Megatec "R<mmmm>" return-delay field. It also spells
 * "shut down and stay off" as a distinct "Sf<n>" command, where the Megatec
 * form is "S<n>R0000". blazer_process_command() produces the Megatec form for
 * both, and appends the R field as soon as ups.delay.start is non-zero, which
 * it is by default - hence this replacement rather than a reuse of it.
 *
 * Mirrors on_shutdown_return() and on_shutdown_stayoff() of the vendor driver,
 * which are identical in the 1.00 and 1.02 releases. ups.delay.start has no
 * effect on these commands; it stays in the table because upsdrv_shutdown()
 * looks it up before dispatching.
 *
 * The two-command shutdown sequence lives here as well: omron_start_auto()
 * runs first, exactly as the vendor orders it, and its failure aborts the
 * command before anything is sent to power the unit down.
 *
 * NOTE: no shutdown command has ever been sent to this hardware, and the
 * byte strings below have not been confirmed by observation - only by reading
 * the vendor source. Treat a first real shutdown as an experiment.
 */
static int	omron_process_command(item_t *item, char *value, const size_t valuelen)
{
	const char	*offdelay_str = dstate_getinfo("ups.delay.shutdown");
	long		offdelay;
	char		buf[SMALLBUF] = "";

	if (!offdelay_str) {
		upslogx(LOG_ERR, "%s: ups.delay.shutdown is not set",
			item->info_type);
		return -1;
	}

	offdelay = strtol(offdelay_str, NULL, 10);

	if (offdelay < 0) {
		upslogx(LOG_ERR, "%s: offdelay '%ld' should not be negative",
			item->info_type, offdelay);
		return -1;
	}

	if (offdelay < 60) {
		snprintf(buf, sizeof(buf), ".%ld", offdelay / 6);
	} else {
		snprintf(buf, sizeof(buf), "%02ld", offdelay / 60);
	}

	/* Auto-restart flag first, and only then the power-down command */
	if (!strcasecmp(item->info_type, "shutdown.return")) {

		if (omron_start_auto(1)) {
			/* Not a value-conversion failure: keep instcmd() from
			 * reporting one because qx_process() left EINVAL behind */
			errno = 0;
			return -1;
		}

	} else if (!strcasecmp(item->info_type, "shutdown.stayoff")) {

		if (omron_start_auto(0)) {
			errno = 0;
			return -1;
		}

	}

	snprintf_dynamic(value, valuelen, item->command, "%s", buf);

	return 0;
}

/* qx2nut lookup table */
static item_t	omron_qx2nut[] = {

	/*
	 * > [Q1\r]
	 * < [(102.5 000.0 102.4 000 49.9 54.5 22.7 00101000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */

	{ "input.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.voltage.fault",	0,	NULL,	"Q1\r",	"",	47,	'(',	"",	7,	11,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	13,	17,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	19,	21,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	23,	26,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	28,	31,	"%.2f",	0,	NULL,	NULL,	qx_multiply_battvolt },
	{ "ups.temperature",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	33,	36,	"%.1f",	0,	NULL,	NULL,	NULL },
	/* Status bits.
	 * Index 2 of the status field ("Bypass/Boost or Buck Active") is
	 * intentionally absent, see the note at the top of this file. */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			NULL,	NULL,	blazer_process_status_bits },	/* Beeper status */

	/* Instant commands */
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	omron_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"Sf%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	omron_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};

/* Testing table
 *
 * The Q1 answer is a reply captured from a BN150T, not a hand-written one.
 * The command answers are "OK\r" because that is what OMRON acknowledges with;
 * an empty answer here would pass regardless of the 'accepted' string, since
 * nutdrv_qx's instcmd() treats "no reply" as success.
 *
 * Both delay forms of each shutdown command are listed: ".n" for an
 * ups.delay.shutdown below 60 seconds (the default, 30, gives ".5") and "nn"
 * for one minute or more. A command absent from this table is answered with
 * the 'rejected' string by the testing implementation, which is how the NAK
 * path gets exercised. */
#ifdef TESTING
static testing_t	omron_testing[] = {
	{ "Q1\r",	"(102.5 000.0 102.4 000 49.9 54.5 22.7 00101000\r",	-1 },
	{ "An\r",	"OK\r",	-1 },
	{ "Af\r",	"OK\r",	-1 },
	{ "S.5\r",	"OK\r",	-1 },
	{ "S02\r",	"OK\r",	-1 },
	{ "Sf.5\r",	"OK\r",	-1 },
	{ "Sf02\r",	"OK\r",	-1 },
	{ "C\r",	"OK\r",	-1 },
	{ NULL }
};
#endif	/* TESTING */

/* Subdriver-specific claim
 *
 * This subdriver differs from 'q1' only in OMRON-specific details, so its
 * protocol check is exactly as permissive as 'q1''s and would otherwise claim
 * every device that the generic 'q1' fallback is meant to serve. Restrict
 * auto-detection to OMRON's USB vendor ID, which nutdrv_qx's upsdrv_initups()
 * publishes as 'ups.vendorid' before subdriver_matcher() runs. A user who
 * names this protocol explicitly gets it either way: subdriver_matcher() skips
 * every other subdriver before calling its claim, so reaching this function
 * with 'protocol' set means it was set to ours. */
static int	omron_claim(void)
{
	if (!getval("protocol")) {

		const char	*vendorid = dstate_getinfo("ups.vendorid");

		if (!vendorid || strcasecmp(vendorid, OMRON_USB_VENDORID)) {
			upsdebugx(2, "%s: not an OMRON device (ups.vendorid: %s)",
				__func__, vendorid ? vendorid : "unset");
			return 0;
		}

	}

	return blazer_claim_light();
}

/* Subdriver-specific initups */
static void	omron_initups(void)
{
	blazer_initups_light(omron_qx2nut);
}

/* Subdriver interface */
subdriver_t	omron_subdriver = {
	OMRON_VERSION,
	omron_claim,
	omron_qx2nut,
	omron_initups,
	NULL,
	blazer_makevartable_light,
	OMRON_ACCEPTED,
	/* Rejected: the vendor tests the first three characters only, so what
	 * follows "NAK" on this hardware is unverified - no NAK has ever been
	 * observed here, and nutdrv_qx compares the whole answer. A wrong guess
	 * costs nothing: the answer then fails the ordinary length and format
	 * checks instead, with the same result and a less specific log line. */
	"NAK\r",
#ifdef TESTING
	omron_testing,
#endif	/* TESTING */
};
