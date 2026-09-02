/* dmf_stdlib.c - Shared library of "standard" data-conversion helpers,
 * reusable from statically compiled mapping tables, DMF XML mappings,
 * and DMF dynamic-language glue (LUA) alike. See dmf_stdlib.h for the
 * public API and rationale.
 *
 * Copyright (C) 2016 - 2017 Jim Klimov <EvgenyKlimov@eaton.com>
 * Copyright (C) 2024-2026 Jim Klimov <jimklimov+nut@gmail.com>
 * Copyright (C) by NUT Community
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

#include "common.h"	/* includes "config.h" which must be the first header */
#include "timehead.h"	/* time.h => strptime()/strftime() */
#include "dmf_stdlib.h"

#include <string.h>
#include <stdlib.h>

dmf_function_policy_t
dmf_stdlib_get_function_policy(void)
{
	const char *s = getenv("NUT_DMF_FUNCTION_POLICY");

	if (s) {
		if (!strcasecmp(s, "drop"))
			return DMF_FUNCTION_POLICY_DROP;
		if (!strcasecmp(s, "strict"))
			return DMF_FUNCTION_POLICY_STRICT;
		upslogx(LOG_WARNING,
			"dmf_stdlib_get_function_policy(): unrecognized "
			"NUT_DMF_FUNCTION_POLICY='%s', defaulting to 'strict'", s);
	}

	return DMF_FUNCTION_POLICY_STRICT;
}

dmf_function_policy_t
dmf_stdlib_log_unsupported_function(const char *context, const char *language)
{
	dmf_function_policy_t policy = dmf_stdlib_get_function_policy();

	switch (policy) {
	case DMF_FUNCTION_POLICY_DROP:
		upslogx(LOG_WARNING,
			"DMF: dropping mapping entry for '%s' - dynamic-language "
			"function in '%s' is not supported by this build "
			"(NUT_DMF_FUNCTION_POLICY=drop)",
			context ? context : "<unknown>",
			language ? language : "<unknown>");
		break;
	case DMF_FUNCTION_POLICY_STRICT:
	default:
		upslogx(LOG_ERR,
			"DMF: rejecting mapping table - entry '%s' uses dynamic-language "
			"function in '%s' which is not supported by this build "
			"(set NUT_DMF_FUNCTION_POLICY=drop to only skip this entry)",
			context ? context : "<unknown>",
			language ? language : "<unknown>");
		break;
	}

	return policy;
}

const char *
nut_scale_format_static(double value, double factor, const char *fmt)
{
	static char buf[32];

	if (!fmt || !*fmt)
		fmt = "%0.1f";

	/* NOTE: caller-provided fmt is expected to be a single numeric
	 * conversion (as used throughout the codebase for this pattern);
	 * we do not validate it further here. */
	snprintf(buf, sizeof(buf), fmt, value * factor);
	return buf;
}

const char *
nut_temperature_deci_to_celsius_static(long value_deci, int unit)
{
	static char buf[32];
	long celsius_value = value_deci;

	memset(buf, 0, sizeof(buf));

	switch (unit) {
		case NUT_STDLIB_TEMP_KELVIN:
			celsius_value = (value_deci / 10) - 273.15;
			snprintf(buf, sizeof(buf), "%.1ld", celsius_value);
			break;
		case NUT_STDLIB_TEMP_CELSIUS:
			snprintf(buf, sizeof(buf), "%.1ld", (value_deci / 10));
			break;
		case NUT_STDLIB_TEMP_FAHRENHEIT:
			celsius_value = (((value_deci / 10) - 32) * 5) / 9;
			snprintf(buf, sizeof(buf), "%.1ld", celsius_value);
			break;
		case NUT_STDLIB_TEMP_UNKNOWN:
		default:
			upsdebugx(1, "%s: not a known temperature unit for conversion!", __func__);
			return NULL;
	}

	return buf;
}

const char *
nut_usdate_to_isodate_static(const char *usdate)
{
	static char buf[32];
	struct tm tm;

	if (!usdate)
		return NULL;

	memset(&tm, 0, sizeof(tm));
	memset(buf, 0, sizeof(buf));

	upsdebugx(3, "%s: US date = %s", __func__, usdate);

	/* Note strptime() returns NULL upon failure, and a ptr to the last
	 * NUL char of the string upon success. Just try blindly the
	 * conversion, same as the legacy snmp-ups-helpers.c code did. */
	strptime(usdate, "%m/%d/%Y", &tm);
	if (strftime(buf, sizeof(buf) - 1, "%F", &tm) != 0) {
		upsdebugx(3, "%s: successfully reformatted: %s", __func__, buf);
		return buf;
	}

	return NULL;
}

const char *
nut_phase_name_static(int index, int total_phases)
{
	static char buf[8];

	if (index < 1)
		return NULL;

	if (total_phases <= 1) {
		/* Single-phase devices: everything maps to L1 */
		snprintf(buf, sizeof(buf), "L1");
		return buf;
	}

	/* 3ph assumed (2ph PDUs do not exist in practice); wrap index into
	 * the [1..3] range the same way the historical per-driver helpers
	 * did (see e.g. the commented-out marlin_outlet_group_phase_fun()
	 * in eaton-pdu-marlin-mib.c). */
	if (index > 3) {
		index = ((index - 1) % 3) + 1;
	}

	if (index < 1 || index > 3)
		return NULL;

	snprintf(buf, sizeof(buf), "L%d", index);
	return buf;
}

const char *
nut_phase_pair_name_static(int n1, int n2)
{
	static char buf[16];

	if (n1 < 1 || n1 > 3 || n2 < 0 || n2 > 3)
		return NULL;

	if (n2 == 0) {
		snprintf(buf, sizeof(buf), "L%d-N", n1);
	} else {
		snprintf(buf, sizeof(buf), "L%d-L%d", n1, n2);
	}

	return buf;
}

#define DMF_STDLIB_METHOD_ENTRY(cname, dname, desc, args, ret) \
	{ #cname, dname, desc, args, ret },

static const dmf_stdlib_method_t dmf_stdlib_methods[] = {
	DMF_STDLIB_METHODS(DMF_STDLIB_METHOD_ENTRY)
};

const dmf_stdlib_method_t *
dmf_stdlib_list_methods(size_t *count)
{
	if (count)
		*count = sizeof(dmf_stdlib_methods) / sizeof(dmf_stdlib_methods[0]);
	return dmf_stdlib_methods;
}
