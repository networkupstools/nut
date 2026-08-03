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
 * This subdriver extends 'q1' with the OMRON-specific corrections
 * listed below. It has been tested on a BN150T (USB 0590:00b7) only; other
 * OMRON models are untested. Q1 polling, the capability and extension queries,
 * and shutdown.stop/return/stayoff were exercised on that unit with its output
 * unloaded. The observed command sequences and acknowledgements matched the
 * OMRON 1.00 and 1.02 driver sources from which they were derived. Version
 * 1.00 was distributed in the Synology DSM 7.3-86009 GPL sources, and version
 * 1.02 in the QNAP QTS 5.2.3 GPL sources. Only the direct instant commands
 * were exercised; the shared shutdown.default and driver.killpower commands
 * and the driver's -k path were not.
 *
 * Differences from 'q1':
 *
 * - The third status bit of the Q1 reply ("Bypass/Boost or Buck Active",
 *   index 2 of the status field) is not published. During stable operation on
 *   mains in both normal and ECO modes, that bit was 1; it was 0 while on
 *   battery and also varied during startup/calibration. 'q1' maps it
 *   unconditionally to the BYPASS status. In NUT that means protection has
 *   been bypassed, i.e. an abnormal state.
 *   Neither OMRON driver version evaluates this bit: the block that would
 *   decide between TRIM/BYPASS/BOOST is commented out in its entirety,
 *   identically in the 1.00 and 1.02 sources.
 *   What the bit actually reports on this hardware remains unknown, so nothing
 *   is published for it.
 *
 * - The instant commands that neither OMRON driver version implements are not
 *   registered: test.battery.start, test.battery.start.deep,
 *   test.battery.start.quick and test.battery.stop (neither source has a T or
 *   TL command), load.on, load.off, and beeper.toggle (no Q command; both
 *   sources use Bn/Bf to enable/disable the beeper instead).
 *   Registering an instant command publishes it to NUT clients as something
 *   the device can perform, so listing commands that may not exist is a defect
 *   in its own right.
 *
 * - The shutdown commands use the byte strings implemented by the OMRON 1.00
 *   and 1.02 drivers rather than the Megatec ones. shutdown.return and
 *   shutdown.stayoff are two transactions, not one: the auto-restart flag is
 *   set first and its failure aborts the command. See omron_start_auto() and
 *   omron_process_command() below.
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

/* Accepted-command acknowledgement. The Megatec-derived
 * subdrivers expect "ACK"; OMRON acknowledges with "OK", and driver versions
 * 1.00 and 1.02 both test it with strncmp(buf, "OK", 2). */
#define OMRON_ACCEPTED		"OK"

/* Answer preprocess for the instant commands and the An/Af prologue
 *
 * Both OMRON sources accept replies whose first two bytes are "OK", for An/Af
 * and the shutdown commands alike. nutdrv_qx's instcmd() instead treats an
 * empty reply as success and otherwise compares the complete extracted reply
 * with "accepted". The former would report an unacknowledged shutdown as
 * accepted, while the latter would reject a reply accepted by both OMRON
 * sources, such as "OKn". Enforce the OMRON test here and normalise the answer
 * so that downstream exact comparisons see the expected acknowledgement. */
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

/* Set the UPS auto-restart flag ahead of a shutdown command
 *
 * "An" enables restarting when mains returns, "Af" disables it. Both OMRON
 * driver versions send the matching one *before* the shutdown command itself,
 * and abandon the instant command if the UPS answers with anything other than
 * "OK". Without it, shutdown.return may leave the unit powered off after mains
 * returns if auto-restart is disabled.
 *
 * This is also the documented NUT convention: docs/nut-names.txt states that
 * shutdown commands set ups.start.auto to the matching value first. nutdrv_qx
 * implements that in the instcmd() fallback path, but the fallback only runs
 * for commands *absent* from the subdriver table, so a subdriver that registers
 * shutdown.return must set it explicitly.
 *
 * This implementation deliberately diverges from both OMRON sources. Their
 * shared implementation has three outcomes: it skips the prologue when its CF
 * capability bitmap says the command is unsupported; it abandons the shutdown
 * on an explicit non-"OK" answer; and it proceeds to shut down when the
 * transfer itself fails or nothing comes back, because the guard around the
 * reply test has no else branch. This driver has no CF gating and abandons the
 * shutdown in all three cases. A shutdown that did not happen is visible in
 * the logs and reversible; a shutdown whose auto-restart state is unknown may
 * need someone at the front panel. */
static int	omron_start_auto(const int on)
{
	/* A temporary item, deliberately not a row of omron_qx2nut[]: this
	 * transaction happens while another item is being preprocessed.
	 * qx_process() stores the reply in the item passed to it, so using the
	 * item handled by instcmd() would overwrite that item. Setting answer_len
	 * and leading to 0 accepts any reply; omron_command_answer() then enforces
	 * the same acknowledgement test used by the shutdown command rows. */
	item_t	prologue = {
		on ? "ups.start.auto (An)" : "ups.start.auto (Af)",
		0,	NULL,
		on ? "An\r" : "Af\r",
		"",	0,	0,
		"",	0,	0,
		"%s",	QX_FLAG_NONUT,
		NULL,	omron_command_answer,	NULL
	};

	/* All failure modes return -1: qx_process() handles transfer failures,
	 * while omron_command_answer() rejects an empty answer, a NAK or any reply
	 * other than the "OK" acknowledgement. */
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
 * OMRON encodes delays below one minute in tenths of a minute (".n", six-second
 * steps) and delays of one minute or more in whole minutes ("nn"). It never
 * appends the Megatec "R<mmmm>" return-delay field. It also spells "shut down
 * and stay off" as a distinct "Sf<n>" command, where the Megatec form is
 * "S<n>R0000". blazer_process_command() produces the Megatec form for both and
 * appends the R field whenever ups.delay.start is non-zero, which it is by
 * default. This driver therefore cannot reuse that function.
 *
 * This mirrors on_shutdown_return() and on_shutdown_stayoff() in the OMRON
 * 1.00 and 1.02 sources; their implementations are identical.
 * ups.delay.start has no effect on these commands; it stays in the table because
 * upsdrv_shutdown() looks it up before dispatching.
 *
 * The two-command shutdown sequence is also implemented here: omron_start_auto()
 * runs first, exactly as both OMRON sources order it, and its failure aborts
 * the command before anything is sent to power the unit down.
 *
 * The two delay encodings and all three direct shutdown operations have been
 * confirmed on an unloaded BN150T: An/S.2 and Af/Sf.2 performed 12-second
 * shutdowns, An/S02 and Af/Sf02 were scheduled and cancelled with C, and every
 * transaction was acknowledged with "OK". The byte strings were originally
 * derived from the OMRON 1.00 and 1.02 sources.
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
 * The serial-number reply is the only one whose characters are unconstrained,
 * so it is also the one place a rejected query could be published as data. Q1
 * rows require a leading '(', and the parsed extension rows exclude the letters
 * of "NAK" by character set or template, but a sufficiently long NAK reply with
 * padding would pass for a serial number. The core's 'rejected' test cannot
 * catch that because it compares the whole answer with one guessed form. */
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
 * The core guard is skipped for any item with a preprocess callback, so a
 * reply of acceptable length that is otherwise corrupt would reach dstate as a
 * plausible value. Every OMRON extension below that publishes a parsed reading
 * therefore validates its whole reply before converting it. This function uses
 * the character set defined for the matching command in the OMRON sources;
 * the UN rows instead match a positional template. ups.serial is the
 * exception: both OMRON sources accept any character in that reply, so its only
 * check here is omron_reject_nak(). */
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
 * The character set from the OMRON sources also keeps strtod() away from "nan",
 * "inf" and exponent notation. The conversion must additionally consume the
 * entire value and stay in range. */
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
 * TPb returns an explicit sign, e.g. "+22.5". It is validated as a signed
 * decimal and republished at one decimal place, which also removes the leading
 * "+" that no other reading in this table carries. Both OMRON sources publish
 * the reply as it arrives. */
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
 * The OMRON sources permit only digits for this command, with no sign or
 * decimal point. A value outside 0..100 is corrupt, not a reading. */
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
 * The OMRON sources permit digits and ".", with no sign. */
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
 * The value passed here has already been sliced to the item's from/to window,
 * so it does not show whether the rest of the reply was valid. The whole answer
 * is matched against a positional template first - exact field widths, every
 * separator and unit in place, and no trailing data - which is what makes the
 * fixed offsets safe to use. The nominal battery voltage is published
 * without the leading zero the OMRON sources print. */
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
 * The reply is eight digits, YYYYMMDD. It is checked for exactly that format
 * and for a valid calendar date, then published as ISO rather than the
 * MM/DD/YY form used by the OMRON sources, which drops the century. The variable
 * is QX_FLAG_STATIC, so a wrong value published once is never refreshed. */
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

/* Parse ups.firmware and ups.firmware.aux from the FWV reply
 *
 * The reply contains two delimited versions, "M:0.17(S:2.00)". The auxiliary
 * version is published from here rather than from a row of its own because the
 * fields are delimited rather than at fixed offsets. Neither is published
 * unless both parse successfully. The main row is QX_FLAG_STATIC, so a wrong
 * value published once would never be refreshed. */
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
	 * cannot be published in reverse order. Only spaces are tolerated after
	 * the closing bracket. */
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

	/* Both versions start after an "M:" or "S:" tag and must end exactly at
	 * their respective delimiters. */
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
	 * Command strings come from the OMRON sources. The specified reply lengths
	 * are minimums, not exact hardware lengths.
	 *
	 * All eight read commands, the shapes of their replies, and the values this
	 * table publishes from them have been exercised on a BN150T. Because the
	 * device returned no malformed replies, only the validation success paths
	 * were exercised.
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
 * The query answers are replies captured during BN150T hardware runs. Plain
 * "OK\r" replies were also observed for An, Af, C, S02, Sf02, S.2 and Sf.2.
 * The table uses S.5/Sf.5 for the default 30-second delay; those exact command
 * strings were not sent to hardware, but the same short-delay encoding was
 * observed with .2. The OMRON 1.00 and 1.02 drivers accept a reply whose first
 * two bytes are "OK". omron_command_answer() applies that test to the
 * shutdown rows and the An/Af prologue item. "OKn\r" remains a synthetic
 * prefix-acceptance case, not an observed reply.
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
 * protocol check is just as permissive as the 'q1' check and could otherwise
 * claim every device intended for the generic 'q1' fallback. Restrict
 * autodetection to OMRON's USB vendor ID, which nutdrv_qx's upsdrv_initups()
 * publishes as 'ups.vendorid' before subdriver_matcher() runs. The OMRON entry
 * precedes command-based protocol probes: non-OMRON devices are rejected here
 * without a transfer, while a known OMRON device goes directly to the Q1
 * check. Explicit protocol selection bypasses the vendor-ID gate:
 * subdriver_matcher() skips every other subdriver before calling this claim,
 * so reaching this function with 'protocol' set means that 'omron' was
 * requested. */
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
	/* Rejected: no NAK response has been observed from this hardware.
	 * Both OMRON sources test only the first three characters. The configured
	 * form is unverified, while nutdrv_qx compares the whole answer. The code
	 * paths where a mismatch could matter perform their own prefix checks in
	 * omron_reject_nak() and omron_command_answer(). */
	"NAK\r",
#ifdef TESTING
	omron_testing,
#endif	/* TESTING */
};
