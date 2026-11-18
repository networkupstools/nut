/*
 * powerp-txt.c - Model specific routines for CyberPower text
 *                protocol UPSes
 *
 * Copyright (C)
 *	2007        Doug Reynolds <mav@wastegate.net>
 *	2007-2008   Arjen de Korte <adkorte-guest@alioth.debian.org>
 *	2012-2016   Timothy Pearson <kb9vqf@pearsoncomputing.net>
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
 */

#include "common.h"
#include "powerp-txt-parser.h"
#include <ctype.h>
#include <float.h>
#include <limits.h>

static int parse_current(const char *text, float *out)
{
	char *end;
	double value;
	errno = 0;
	value = strtod(text, &end);
	if (end == text || *end || errno == ERANGE || !(value >= 0 && value <= FLT_MAX)) {
		return -1;
	}
	*out = (float)value;
	return 0;
}

static int parse_power(const char *text, int *out)
{
	char *end;
	long value;
	errno = 0;
	value = strtol(text, &end, 10);
	if (end == text || *end || errno == ERANGE || value < 0 || value > INT_MAX) {
		return -1;
	}
	*out = (int)value;
	return 0;
}

	/*
	 * Status format from the D query (wire examples):
	 * READ #I119.0O119.0L000B100T027F060.0S..\r
	 *      #I118.0O118.0L029B100F060.0R0218S..\r
	 *      #I118.1O118.1L13B100V27.5F60.0HF60.0R65Q1.4S\x80\x84\xc0\x88\x80W\r		(CST135XLU)
	 *      01234567890123456789012345678901234567890123
	 *      0         1         2         3         4
	 */

int powpan_parse_status(const char *answer, size_t length, powpan_status_t *status)
{
	size_t i, ofs = 0;
	size_t valid = 0;
	int code = -1;
	char value[32];
	char seen_i_freq = 0;
	int seen_flags = 0;
	if (!answer || !status || length < 2) {
		return -1;
	}
	memset(status, 0, sizeof(*status));
	if (answer[0] != '#') {
		return -1;
	}

	for (i = 1; i <= length; i++) {
		if (i == length || isalpha((unsigned char)(answer[i]))) {
			if (i < length && (code == 'Q' || code == 'J' || code == 'E')
			&& islower((unsigned char)answer[i])) {
				return -1;
			}
			value[ofs] = '\0';
			valid++;

			switch (code) {
				case 'I':
					status->i_volt = strtof(value, NULL);
					break;
				case 'O':
					status->o_volt = strtof(value, NULL);
					break;
				case 'L':
					status->o_load = strtol(value, NULL, 10);
					break;
				case 'B':
					status->b_chrg = strtol(value, NULL, 10);
					break;
				case 'V':
					status->b_volt = strtof(value, NULL);
					status->has_b_volt = 1;
					break;
				case 'T':
					status->u_temp = strtol(value, NULL, 10);
					status->has_u_temp = 1;
					break;
				case 'F':
					status->i_freq = strtof(value, NULL);
					seen_i_freq = 1;
					break;
				case 'H':
					status->o_freq = strtof(value, NULL);
					status->has_o_freq = 1;
					break;
				case 'R':
					status->runtime = strtol(value, NULL, 10);
					status->has_runtime = 1;
					break;
				case 'Q':
					if (status->has_o_current || parse_current(value, &status->o_current) < 0) {
						return -1;
					}
					status->has_o_current = 1;
					break;
				case 'J':
					if (status->has_realpower || parse_power(value, &status->realpower) < 0) {
						return -1;
					}
					status->has_realpower = 1;
					break;
				case 'E':
					if (status->has_power || parse_power(value, &status->power) < 0) {
						return -1;
					}
					status->has_power = 1;
					break;
				case 'S':
					if (ofs < 2) {
						return -1;
					}
					memcpy(&status->flags, value, 2);
					seen_flags = 1;
					break;
				default:
					/* We didn't really find valid data */
					valid--;
					break;
			}

			if (i == length) {
				break;
			}
			code = answer[i];
			ofs = 0;

			/*
				* Depending on device/firmware, the output frequency is coded as
				* either an H, HF, or a second F (seen once transfered to battery)
				*/
			if (seen_i_freq && code == 'F')
				code = 'H';

			continue;
		}

		if (ofs >= sizeof(value) - 1) {
			return -1;
		}
		value[ofs++] = answer[i];
	}

	/* if we didn't get at least 3 values consider it a failure */
	if (valid < 3 || !seen_flags) {
		return -1;
	}

	return 0;
}
