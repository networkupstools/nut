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
 * side: Q1 polling, the capability queries and the extension queries. No
 * instant command has ever been sent to it, so every instant-command byte
 * string below rests on the vendor source alone.
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
 *   entirety, identically in the 1.00 and 1.02 releases of the vendor source.
 *   What the bit actually reports on this hardware is unknown - resolving it
 *   needs an observed on-battery event - so nothing is published for it.
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
 * - Autodetection is restricted to OMRON's USB vendor ID and this protocol is
 *   tried before command-based probes. An explicit 'protocol = omron' skips
 *   the vendor-ID check. See omron_claim() below.
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

/* Answer preprocess for the instant commands and the An/Af prologue
 *
 * The vendor tests exactly the first two bytes of the reply against "OK", for
 * An/Af and for the shutdown commands alike. nutdrv_qx's instcmd() differs on
 * both sides of that test: it treats an empty reply as success, which would
 * report a shutdown as accepted when nothing acknowledged it, and it then
 * compares the whole extracted reply against 'accepted', which would fail an
 * acknowledgement the vendor accepts, such as "OKn". Enforce the vendor's
 * test here, and normalise the answer to the acknowledgement alone so that
 * the exact compares downstream see the reply they expect. */
static int	omron_command_answer(item_t *item, const int len)
{
	if (len < 2 || strncmp(item->answer, OMRON_ACCEPTED, 2)) {
		upsdebugx(2, "%s: %s: not acknowledged, answer was [%.*s]",
			__func__, item->info_type,
			(int)strcspn(item->answer, "\r"), item->answer);
		/* A rejection, not a value-conversion failure. Best effort:
		 * logging between here and instcmd()'s errno test may leave
		 * another value behind. */
		errno = 0;
		return -1;
	}

	snprintf(item->answer, sizeof(item->answer), "%s\r", OMRON_ACCEPTED);

	return (int)strlen(item->answer);
}

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
	 * leading 0 accept any reply; the acknowledgement is enforced by
	 * omron_command_answer() on this item, the same test the shutdown
	 * command rows carry. */
	item_t	prologue = {
		on ? "ups.start.auto (An)" : "ups.start.auto (Af)",
		0,	NULL,
		on ? "An\r" : "Af\r",
		"",	0,	0,
		"",	0,	0,
		"%s",	QX_FLAG_NONUT,
		NULL,	omron_command_answer,	NULL
	};

	/* Everything lands here as a -1: a transfer failure fails qx_process()
	 * itself, and an empty answer, a NAK and any reply that is not the "OK"
	 * acknowledgement fail omron_command_answer() */
	if (qx_process(&prologue, NULL)) {
		upslogx(LOG_ERR, "%s: [%.*s] failed or was not acknowledged",
			__func__,
			(int)strcspn(prologue.command, "\r"), prologue.command);
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

/* Answer preprocess for ups.serial
 *
 * The serial number is the one reply whose characters are unconstrained, so it
 * is also the one place a rejected query could be published as data: the Q1
 * rows require a leading '(', the parsed extension rows exclude the letters
 * of "NAK" by character set or template, but a NAK-plus-padding reply of
 * sufficient length would pass for a serial number. The core's 'rejected'
 * test cannot catch that, being a whole-answer compare against one guessed
 * form. */
static int	omron_reject_nak(item_t *item, const int len)
{
	if (len >= 3 && !strncmp(item->answer, "NAK", 3)) {
		upsdebugx(2, "%s: %s: query rejected by the UPS",
			__func__, item->info_type);
		return -1;
	}

	return len;
}

/* Check a whole reply against the characters it may contain
 *
 * The core's own guard is skipped for any item with a preprocess, so a reply
 * of acceptable length that is otherwise corrupt would reach dstate as a
 * plausible value. Every OMRON extension below that publishes a parsed reading
 * therefore validates its whole reply before converting any of it: through
 * this function, against the character set the vendor uses for that command,
 * except for the UN rows, which match a positional template instead.
 * ups.serial is the exception: the vendor accepts any character in that reply,
 * and the only check made here is omron_reject_nak(). */
static int	omron_check_charset(item_t *item, const char *accept)
{
	if (item->value[0] == '\0'
	||  strspn(item->value, accept) != strlen(item->value)
	) {
		upsdebugx(2, "%s: %s: unexpected characters in [%s]",
			__func__, item->info_type, item->value);
		return -1;
	}

	return 0;
}

/* Parse a whole reply as one decimal number
 *
 * The character set is the vendor's for that command, which also keeps
 * strtod() away from "nan", "inf" and exponent notation; on top of it the
 * conversion has to consume the entire value and stay in range. */
static int	omron_strict_decimal(item_t *item, const char *accept, double *reading)
{
	char	*end = NULL;

	if (omron_check_charset(item, accept))
		return -1;

	errno = 0;
	*reading = strtod(item->value, &end);

	if (end == item->value || *end != '\0' || errno == ERANGE) {
		upsdebugx(2, "%s: %s: unparsable reading [%s]",
			__func__, item->info_type, item->value);
		return -1;
	}

	return 0;
}

/* Normalise a signed decimal reading
 *
 * TPb answers with an explicit sign, e.g. "+22.5". It is validated as a signed
 * decimal and republished at one decimal place, which also removes the leading
 * '+' that no other reading in this table carries. The vendor publishes the
 * reply as it arrives. */
static int	omron_process_signed(item_t *item, char *value, const size_t valuelen)
{
	double	reading;

	if (omron_strict_decimal(item, "0123456789.+-", &reading))
		return -1;

	snprintf(value, valuelen, "%.1f", reading);

	return 0;
}

/* battery.charge out of the Bl ? reply
 *
 * Vendor character set for this command: digits only, no sign and no decimal
 * point. A percentage outside 0..100 is a corrupt reply, not a reading. */
static int	omron_process_charge(item_t *item, char *value, const size_t valuelen)
{
	double	reading;

	if (omron_strict_decimal(item, "0123456789", &reading))
		return -1;

	if (reading < 0 || reading > 100) {
		upsdebugx(2, "%s: %s: charge out of range [%s]",
			__func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, "%.1f", reading);

	return 0;
}

/* output.frequency out of the Lf ? reply
 *
 * Vendor character set for this command: digits and '.', so no sign either. */
static int	omron_process_frequency(item_t *item, char *value, const size_t valuelen)
{
	double	reading;

	if (omron_strict_decimal(item, "0123456789.", &reading))
		return -1;

	snprintf(value, valuelen, "%.1f", reading);

	return 0;
}

/* The three nominal ratings out of the UN reply
 *
 * The value this is handed has already been cut to a from/to window, so it
 * says nothing about whether the rest of the reply survived. The whole answer
 * is matched against a positional template first - exact field widths, every
 * separator and unit in place, nothing after the last one - which is what makes
 * the fixed offsets safe to use. The nominal battery voltage is published
 * without the leading zero the vendor prints. */
static int	omron_process_un(item_t *item, char *value, const size_t valuelen)
{
	/* 'd' stands for a digit, everything else is itself */
	static const char	shape[] = "ddddW/ddddVA/dddV/dddW/dddW";
	const char		*answer = item->answer;
	size_t			i;

	for (i = 0; i < sizeof(shape) - 1; i++) {

		if (shape[i] == 'd') {
			if (answer[i] < '0' || answer[i] > '9')
				break;
			continue;
		}

		if (answer[i] != shape[i])
			break;
	}

	if (i < sizeof(shape) - 1
	||  (answer[i] != '\0' && answer[i] != '\r')
	) {
		upsdebugx(2, "%s: %s: reply does not match [%s]: [%s]",
			__func__, item->info_type, shape, answer);
		return -1;
	}

	/* The window is all digits by the check above */
	snprintf(value, valuelen, "%ld", strtol(item->value, NULL, 10));

	return 0;
}

/* battery.runtime out of the RTS reply
 *
 * RTS answers in whole minutes and NUT wants seconds, so the reply is parsed
 * strictly and converted here rather than through qx_multiply_m2s(), which
 * accepts any numeric prefix. */
static int	omron_process_runtime(item_t *item, char *value, const size_t valuelen)
{
	char	*end = NULL;
	long	minutes;

	if (omron_check_charset(item, "0123456789"))
		return -1;

	errno = 0;
	minutes = strtol(item->value, &end, 10);

	if (end == item->value || *end != '\0' || errno == ERANGE
	||  minutes < 0 || minutes > LONG_MAX / 60
	) {
		upsdebugx(2, "%s: %s: unusable runtime [%s]",
			__func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, "%ld", minutes * 60);

	return 0;
}

/* battery.date out of the BRR reply
 *
 * The reply is eight digits, YYYYMMDD. It is checked for being exactly that
 * and for forming a date that exists, then published as ISO rather than in the
 * vendor's MM/DD/YY, which drops the century. The variable is QX_FLAG_STATIC,
 * so a wrong value published once is never refreshed. */
static int	omron_process_batt_date(item_t *item, char *value, const size_t valuelen)
{
	static const int	monthlen[12] = {
		31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
	const char		*answer = item->value;
	int			year, month, day, len;

	if (omron_check_charset(item, "0123456789"))
		return -1;

	if (strlen(answer) != 8) {
		upsdebugx(2, "%s: %s: expected eight digits, got [%s]",
			__func__, item->info_type, answer);
		return -1;
	}

	year  = (answer[0] - '0') * 1000 + (answer[1] - '0') * 100
	      + (answer[2] - '0') * 10 + (answer[3] - '0');
	month = (answer[4] - '0') * 10 + (answer[5] - '0');
	day   = (answer[6] - '0') * 10 + (answer[7] - '0');

	if (month < 1 || month > 12) {
		upsdebugx(2, "%s: %s: month out of range in [%s]",
			__func__, item->info_type, answer);
		return -1;
	}

	len = monthlen[month - 1];
	if (month == 2 && !(year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
		len = 28;

	if (day < 1 || day > len) {
		upsdebugx(2, "%s: %s: day out of range in [%s]",
			__func__, item->info_type, answer);
		return -1;
	}

	snprintf(value, valuelen, "%04d-%02d-%02d", year, month, day);

	return 0;
}

/* ups.firmware, and ups.firmware.aux alongside it, out of the FWV reply
 *
 * The reply is two delimited versions, "M:0.17(S:2.00)". The auxiliary one is
 * published from here rather than from a row of its own because the fields are
 * delimited rather than at fixed offsets. Neither is published until both
 * parse: the variable is QX_FLAG_STATIC, so a wrong value published once would
 * never be refreshed. */
static int	omron_process_firmware(item_t *item, char *value, const size_t valuelen)
{
	const char	*answer = item->value;
	const char	*open, *close;
	char		*end = NULL;
	double		main_version, aux_version;

	if (omron_check_charset(item, "0123456789MS.:() "))
		return -1;

	open = strchr(answer, '(');
	close = strchr(answer, ')');

	/* Both tags are checked, not just the shape, so that the two versions
	 * cannot be published the wrong way round. Only spaces are tolerated
	 * after the closing bracket. */
	if (open == NULL || close == NULL || close < open + 4 || open < answer + 3
	||  strncmp(answer, "M:", 2) != 0
	||  open[1] != 'S' || open[2] != ':'
	||  strchr(open + 1, '(') != NULL
	||  strchr(close + 1, ')') != NULL
	||  strspn(close + 1, " ") != strlen(close + 1)
	) {
		upsdebugx(2, "%s: %s: unexpected shape [%s]",
			__func__, item->info_type, answer);
		return -1;
	}

	/* Both versions start two characters into their half, past a "M:"/"S:"
	 * style tag, and have to run up to their delimiter with nothing left */
	errno = 0;
	main_version = strtod(answer + 2, &end);

	if (end != open || errno == ERANGE) {
		upsdebugx(2, "%s: %s: unparsable main version in [%s]",
			__func__, item->info_type, answer);
		return -1;
	}

	errno = 0;
	aux_version = strtod(open + 3, &end);

	if (end != close || errno == ERANGE) {
		upsdebugx(2, "%s: %s: unparsable auxiliary version in [%s]",
			__func__, item->info_type, answer);
		return -1;
	}

	snprintf(value, valuelen, "%.2f", main_version);
	dstate_setinfo("ups.firmware.aux", "%.2f", aux_version);

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
	 * intentionally absent, see the note at the top of this file. So is
	 * index 7, which 'q1' publishes as ups.beeper.status: OMRON's own
	 * parser never reads that character, and takes the beeper state from
	 * a different query, in which '0' means enabled - while
	 * blazer_process_status_bits() maps an index-7 '0' to disabled. */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Shutdown Active */

	/* OMRON extensions
	 *
	 * Commands and reply lengths are the vendor's own. Its lengths are
	 * minimums, not the exact lengths this hardware returns.
	 *
	 * All eight read commands, the shape of their replies and the values this
	 * table publishes from them have been exercised on a BN150T. The reply
	 * validation is the exception: this hardware has never returned a
	 * malformed reply, so only the accepting side of it has ever run.
	 *
	 * None carries QX_FLAG_QUICK_POLL: a QUICK_POLL item that fails aborts
	 * qx_ups_walk() before QX_FLAG_SKIP can be set, which during
	 * initialisation stops the driver from starting at all. */
	{ "battery.charge",		0,	NULL,	"Bl ?\r",	"",	4,	0,	"",	0,	0,	"%.1f",	0,			NULL,	NULL,	omron_process_charge },
	{ "battery.runtime",		0,	NULL,	"RTS\r",	"",	4,	0,	"",	0,	0,	"%.0f",	0,			NULL,	NULL,	omron_process_runtime },
	{ "battery.temperature",	0,	NULL,	"TPb\r",	"",	5,	0,	"",	0,	0,	"%s",	0,			NULL,	NULL,	omron_process_signed },
	{ "output.frequency",		0,	NULL,	"Lf ?\r",	"",	4,	0,	"",	0,	0,	"%.1f",	0,			NULL,	NULL,	omron_process_frequency },
	{ "ups.serial",			0,	NULL,	"PSNR\r",	"",	17,	0,	"",	0,	0,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	omron_reject_nak,	NULL },	/* Trim '#' and space padding */
	{ "ups.firmware",		0,	NULL,	"FWV\r",	"",	15,	0,	"",	0,	0,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	omron_process_firmware },
	{ "battery.date",		0,	NULL,	"BRR\r",	"",	9,	0,	"",	0,	0,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	omron_process_batt_date },
	/*
	 * > [UN\r]
	 * < [1125W/1125VA/048V/160W/020W\r]
	 *    0123456789012345678901234567
	 *    0         1         2
	 * Keep these three adjacent: qx_ups_walk() reuses the previous item's
	 * answer when the command matches, so one transfer serves all three.
	 * OMRON 1.00 and 1.02 publish the first two fields as current power;
	 * use nominal NUT variables here because UN reports the UPS ratings.
	 */
	{ "ups.realpower.nominal",	0,	NULL,	"UN\r",		"",	27,	0,	"",	0,	3,	"%.0f",	QX_FLAG_STATIC,		NULL,	NULL,	omron_process_un },
	{ "ups.power.nominal",		0,	NULL,	"UN\r",		"",	27,	0,	"",	6,	9,	"%.0f",	QX_FLAG_STATIC,		NULL,	NULL,	omron_process_un },
	{ "battery.voltage.nominal",	0,	NULL,	"UN\r",		"",	27,	0,	"",	13,	15,	"%.0f",	QX_FLAG_STATIC,		NULL,	NULL,	omron_process_un },

	/* Instant commands */
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	omron_command_answer,	omron_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"Sf%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	omron_command_answer,	omron_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	omron_command_answer,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};

/* Testing table
 *
 * The query answers are replies captured from BN150T hardware runs. The
 * instant-command answers are synthetic fixtures: no instant command has ever
 * been sent to real hardware, so no instant-command reply has ever been
 * observed. The vendor drivers accept a reply whose first two bytes are "OK",
 * which is the test omron_command_answer() applies for the shutdown command
 * rows and the An/Af prologue item alike; "OK\r" is the plain form of that,
 * and the "OKn\r" entry is a synthetic prefix-acceptance case, not an observed
 * reply.
 *
 * Both delay forms of each shutdown command are listed: ".n" for an
 * ups.delay.shutdown below 60 seconds (the default, 30, gives ".5") and "nn"
 * for one minute or more. A command absent from this table is answered with
 * the 'rejected' string by the testing implementation, which is how the NAK
 * path gets exercised. */
#ifdef TESTING
static testing_t	omron_testing[] = {
	{ "Q1\r",	"(102.5 000.0 102.4 000 49.9 54.5 22.7 00101000\r",	-1 },
	{ "Bl ?\r",	"100\r",	-1 },
	{ "RTS\r",	"0699\r",	-1 },
	{ "TPb\r",	"+22.5\r",	-1 },
	{ "Lf ?\r",	"49.9\r",	-1 },
	{ "PSNR\r",	"A0Z25110005109G \r",	-1 },
	{ "FWV\r",	"M:0.17(S:2.00)\r",	-1 },
	{ "BRR\r",	"20260730\r",	-1 },
	{ "UN\r",	"1125W/1125VA/048V/160W/020W\r",	-1 },
	{ "An\r",	"OK\r",	-1 },
	{ "Af\r",	"OK\r",	-1 },
	{ "S.5\r",	"OK\r",	-1 },
	{ "S02\r",	"OKn\r",	-1 },
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
 * publishes as 'ups.vendorid' before subdriver_matcher() runs. The OMRON entry
 * precedes command-based protocol probes: non-OMRON devices are rejected here
 * without a transfer, while a known OMRON device goes directly to the Q1
 * check. A user who names this protocol explicitly gets it either way:
 * subdriver_matcher() skips every other subdriver before calling its claim, so
 * reaching this function with 'protocol' set means it was set to ours. */
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
	/* Rejected: no NAK has ever been observed from this hardware, and the
	 * vendor tests the first three characters only, so the exact form
	 * guessed here is unverified while nutdrv_qx compares the whole answer.
	 * The places where a wrong guess could matter carry a prefix test of
	 * their own: omron_reject_nak() and omron_command_answer(). */
	"NAK\r",
#ifdef TESTING
	omron_testing,
#endif	/* TESTING */
};
