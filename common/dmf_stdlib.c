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

/* Sticky flag for "reject whole file" support under the strict policy;
 * intentionally process-global (not thread-local) like several other
 * bits of DMF parser state (e.g. 'functions_aux', 'temperature_unit') -
 * DMF parsing is not currently re-entrant/threaded. */
static int dmf_stdlib_unsupported_function_seen = 0;

void
dmf_stdlib_reset_unsupported_function_flag(void)
{
	dmf_stdlib_unsupported_function_seen = 0;
}

int
dmf_stdlib_had_unsupported_function(void)
{
	return dmf_stdlib_unsupported_function_seen;
}

dmf_function_policy_t
dmf_stdlib_get_function_policy(void)
{
	const char	*s = getenv("NUT_DMF_FUNCTION_POLICY");

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
dmf_stdlib_log_unsupported_function(const char *context, const char *capability)
{
	dmf_function_policy_t	policy = dmf_stdlib_get_function_policy();

	dmf_stdlib_unsupported_function_seen = 1;

	switch (policy) {
	case DMF_FUNCTION_POLICY_DROP:
		upslogx(LOG_WARNING,
			"DMF: dropping mapping entry for '%s' - requested capability '%s' "
			"is not supported/recognized by this build "
			"(NUT_DMF_FUNCTION_POLICY=drop)",
			context ? context : "<unknown>",
			capability ? capability : "<unknown>");
		break;
	case DMF_FUNCTION_POLICY_STRICT:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
	default:	/* Must not occur. */
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		upslogx(LOG_ERR,
			"DMF: rejecting mapping table - entry '%s' requires capability '%s' "
			"which is not supported/recognized by this build "
			"(set NUT_DMF_FUNCTION_POLICY=drop to only skip this entry)",
			context ? context : "<unknown>",
			capability ? capability : "<unknown>");
		break;
	}

	return policy;
}

const char *
nut_scale_format_static(double value, double factor, const char *fmt)
{
	static char	buf[32];

	if (!fmt || !*fmt)
		fmt = "%0.1f";

	/* NOTE: caller-provided fmt is expected to be a single numeric
	 * conversion (as used throughout the codebase for this pattern);
	 * we do not validate it further here. */
	snprintf_dynamic(buf, sizeof(buf), fmt, "%f", value * factor);
	return buf;
}

const char *
nut_temperature_deci_to_celsius_static(long value_deci, int unit)
{
	static char	buf[32];
	long	celsius_value = value_deci;

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
	static char	buf[32];
	struct tm	tm;

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
	static char	buf[8];

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
	static char	buf[16];

	if (n1 < 1 || n1 > 3 || n2 < 0 || n2 > 3)
		return NULL;

	if (n2 == 0) {
		snprintf(buf, sizeof(buf), "L%d-N", n1);
	} else {
		snprintf(buf, sizeof(buf), "L%d-L%d", n1, n2);
	}

	return buf;
}

const char *
nut_dmf_apply_conversion(const char *dmf_name, const char *args,
	double raw_number, const char *raw_string, const char *context)
{
	char	argbuf[128];
	char	*save = NULL;
	char	*tok1 = NULL, *tok2 = NULL;

	if (!dmf_name)
		return NULL;

	if (args) {
		snprintf(argbuf, sizeof(argbuf), "%s", args);
		tok1 = strtok_r(argbuf, ",", &save);
		if (tok1)
			tok2 = strtok_r(NULL, ",", &save);
	}

	if (!strcmp(dmf_name, "scale_format")) {
		double	factor = tok1 ? atof(tok1) : 1.0;
		const char	*fmt = tok2 ? tok2 : "%0.1f";
		return nut_scale_format_static(raw_number, factor, fmt);
	}

	if (!strcmp(dmf_name, "temperature_deci_to_celsius")) {
		int	unit = NUT_STDLIB_TEMP_CELSIUS;
		if (tok1) {
			if (!strcasecmp(tok1, "kelvin"))
				unit = NUT_STDLIB_TEMP_KELVIN;
			else if (!strcasecmp(tok1, "fahrenheit"))
				unit = NUT_STDLIB_TEMP_FAHRENHEIT;
			else if (!strcasecmp(tok1, "celsius"))
				unit = NUT_STDLIB_TEMP_CELSIUS;
			else
				unit = NUT_STDLIB_TEMP_UNKNOWN;
		}
		return nut_temperature_deci_to_celsius_static((long) raw_number, unit);
	}

	if (!strcmp(dmf_name, "usdate_to_isodate")) {
		return nut_usdate_to_isodate_static(raw_string);
	}

	if (!strcmp(dmf_name, "phase_name")) {
		int	total_phases = tok1 ? atoi(tok1) : 1;
		return nut_phase_name_static((int) raw_number, total_phases);
	}

	if (!strcmp(dmf_name, "phase_pair_name")) {
		int	n2 = tok1 ? atoi(tok1) : 0;
		return nut_phase_pair_name_static((int) raw_number, n2);
	}

	/* Unknown conversion name: log via the same policy machinery used
	 * for an unsupported dynamic-language function - this also sets the
	 * sticky flag so the strict policy can reject the whole DMF source,
	 * catching typos in 'conversion="..."' the same way. */
	dmf_stdlib_log_unsupported_function(context, dmf_name);
	return NULL;
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
