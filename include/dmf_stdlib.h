/* dmf_stdlib.h - Shared library of "standard" data-conversion helpers
 * usable from three call sites: statically compiled mapping tables
 * (SNMP MIB files and similar), DMF XML definitions (via a named
 * lookup once wired into the parser), and DMF dynamic-language glue
 * (e.g. LUA) where enabled.
 *
 * The intent is to stop each driver/MIB file from privately
 * re-implementing the same handful of numeric/date/phase-naming
 * conversions (see e.g. legrand-hid.c, mge-hid.c, belkin-hid.c,
 * snmp-ups-helpers.c before this file existed).
 *
 * Copyright (C) 2026 Jim Klimov <jimklimov+nut@gmail.com>
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

#ifndef NUT_DMF_STDLIB_H
#define NUT_DMF_STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Temperature unit values; numerically compatible with the historical
 * TEMPERATURE_UNKNOWN/CELSIUS/KELVIN/FAHRENHEIT macros in snmp-ups.h
 * (0/1/2/3) so callers may pass either interchangeably. Kept as a
 * separate definition here to avoid a common/ -> drivers/ header
 * dependency. */
#define NUT_STDLIB_TEMP_UNKNOWN    0
#define NUT_STDLIB_TEMP_CELSIUS    1
#define NUT_STDLIB_TEMP_KELVIN     2
#define NUT_STDLIB_TEMP_FAHRENHEIT 3

/* Runtime policy for handling DMF mapping entries that reference a
 * dynamic-language function (e.g. LUA) which this build cannot execute
 * (interpreter not compiled in), or any other "unsupported function"
 * situation flagged by a DMF consumer.
 *   STRICT (default): treat as a hard error for the affected mapping/file.
 *   DROP:             skip just the affected mapping entry, keep going.
 * Either way, the event is logged loudly (LOG_ERR/LOG_WARNING), not just
 * at a buried debug verbosity, so operators are not left guessing about
 * silently missing data points. */
typedef enum {
	DMF_FUNCTION_POLICY_STRICT = 0,
	DMF_FUNCTION_POLICY_DROP = 1
} dmf_function_policy_t;

/* Reads NUT_DMF_FUNCTION_POLICY=strict|drop from the environment
 * (case-insensitive); defaults to DMF_FUNCTION_POLICY_STRICT if unset
 * or unrecognized. */
dmf_function_policy_t dmf_stdlib_get_function_policy(void);

/* Log (at LOG_ERR for strict policy, LOG_WARNING for drop policy) that
 * a DMF mapping entry could not be honored in "context" (e.g. a mapping
 * name or file name) because it requires "capability" (a dynamic-language
 * name such as "lua", or an unrecognized 'conversion=' method name) that
 * this build does not support/recognize. Returns the policy that was in
 * effect at the time of the call, so the caller can decide whether to
 * abort processing of the current file/mapping or just skip the one
 * entry. */
dmf_function_policy_t dmf_stdlib_log_unsupported_function(
	const char *context, const char *capability);

/* Reset the sticky "an unsupported function was encountered" flag; call
 * this once at the start of parsing a DMF source (file/string/dir entry).
 * Used together with dmf_stdlib_had_unsupported_function() to implement
 * "reject the whole file" behavior for the strict policy. */
void dmf_stdlib_reset_unsupported_function_flag(void);

/* Returns non-zero if dmf_stdlib_log_unsupported_function() was called at
 * least once since the last dmf_stdlib_reset_unsupported_function_flag(). */
int dmf_stdlib_had_unsupported_function(void);

/* --- Generic numeric scale/format helper ---
 * Multiply "value" by "factor" and format the result with the given
 * printf-style "fmt" (e.g. "%0.1f", "%.2f"). Returns a pointer to an
 * internal static buffer (like the rest of this codebase's similar
 * helpers) - NOT thread-safe, do not hold the pointer across another
 * call from the same thread. Covers the very common "timesN"/"/N"
 * driver-local helpers (legrand_times10, mge_powerfactor_conversion,
 * mge_battery_capacity_fun, etc). */
const char *nut_scale_format_static(double value, double factor, const char *fmt);

/* --- Temperature conversion ---
 * Convert a raw "deci-value" reading (i.e. already-scaled by 10, as
 * commonly seen straight off SNMP/HID) expressed in the given unit
 * (NUT_STDLIB_TEMP_*) into a Celsius value formatted as "%.1f" text.
 * Returns a pointer to an internal static buffer; NULL if unit is
 * unknown/unrecognized. */
const char *nut_temperature_deci_to_celsius_static(long value_deci, int unit);

/* --- Date conversion ---
 * Convert a US-formatted date string "mm/dd/yyyy" into an ISO-8601
 * calendar date "yyyy-mm-dd". Returns a pointer to an internal static
 * buffer, or NULL if the input could not be parsed. */
const char *nut_usdate_to_isodate_static(const char *usdate);

/* --- Multi-phase naming helpers ---
 * Compute the "L<N>" phase label for a 1-based outlet/group index,
 * given the total number of phases the device reports (1 or 3).
 * Returns a pointer to an internal static buffer, or NULL on
 * out-of-range input. */
const char *nut_phase_name_static(int index, int total_phases);

/* Compute the "L<N1>-L<N2>" (or "L<N1>-N" when n2==0) phase-pair label
 * for two 1-based phase numbers (e.g. 1,2 -> "L1-L2"; 1,0 -> "L1-N").
 * Returns a pointer to an internal static buffer, or NULL on
 * out-of-range input. */
const char *nut_phase_pair_name_static(int n1, int n2);

/* --- DMF XML "conversion" attribute dispatcher ---
 * Apply the named stdlib conversion (one of the DMF_STDLIB_METHODS()
 * "dmf_name" entries below, e.g. "scale_format") to a raw value coming
 * from a DMF mapping entry, given its (optional) comma-separated
 * "args" as found in the DMF XML "conversion_args" attribute:
 *   scale_format:                  args = "factor[,fmt]"
 *   temperature_deci_to_celsius:   args = "unit" (celsius|kelvin|fahrenheit)
 *   usdate_to_isodate:             args ignored, uses raw_string
 *   phase_name:                    args = "total_phases"
 *   phase_pair_name:               args = "n2" (raw_number is n1)
 * "context" is used only for logging (e.g. the mapping's info_type).
 * Returns a pointer to an internal static buffer, or NULL if dmf_name
 * is not a recognized method (in which case this also logs via
 * dmf_stdlib_log_unsupported_function() and applies the current
 * dmf_function_policy_t, same as an unsupported LUA language would). */
const char *nut_dmf_apply_conversion(const char *dmf_name, const char *args,
	double raw_number, const char *raw_string, const char *context);

/* --- Introspection / self-documentation ---
 * A read-only description of each of the methods above, primarily so
 * that:
 *  - a generator script (see scripts/DMF/gen-dmf-stdlib-methods.py)
 *    can emit an XML listing of method names/args/return types for
 *    schema validation or documentation purposes;
 *  - LUA glue code (where enabled) can enumerate what is available
 *    without hardcoding a duplicate list;
 *  - developers can grep this header, which is also authoritative.
 * NOTE: keep the DMF_STDLIB_METHODS(X) list below in sync with the
 * public functions declared above - the generator and the runtime
 * registry both derive from it.
 */
typedef struct {
	const char *c_name;      /* C function name, e.g. "nut_scale_format_static" */
	const char *dmf_name;    /* short name for DMF XML / LUA reference */
	const char *description;
	const char *args;        /* human-readable arg list, e.g. "double value, double factor, string fmt" */
	const char *retval;      /* human-readable return type, e.g. "string" */
} dmf_stdlib_method_t;

/* X-Macro listing of registered methods: X(c_name, dmf_name, description, args, retval) */
#define DMF_STDLIB_METHODS(X) \
	X(nut_scale_format_static, "scale_format", \
		"Multiply value by factor and format with a printf-style spec", \
		"double value, double factor, string fmt", "string") \
	X(nut_temperature_deci_to_celsius_static, "temperature_deci_to_celsius", \
		"Convert a raw deci-value temperature reading in a given unit to a Celsius string", \
		"double value_deci, int unit", "string") \
	X(nut_usdate_to_isodate_static, "usdate_to_isodate", \
		"Convert a US date mm/dd/yyyy to ISO 8601 yyyy-mm-dd", \
		"string usdate", "string") \
	X(nut_phase_name_static, "phase_name", \
		"Compute the phase label (L1/L2/L3) for a 1-based index given the total phase count", \
		"int index, int total_phases", "string") \
	X(nut_phase_pair_name_static, "phase_pair_name", \
		"Compute the phase-pair label (e.g. L1-L2, or L1-N) for two 1-based phase numbers", \
		"int n1, int n2", "string")

/* Returns a pointer to a static array of method descriptors and sets
 * *count to its length. The array and its contents are owned by this
 * library and must not be freed or modified by the caller. */
const dmf_stdlib_method_t *dmf_stdlib_list_methods(size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* NUT_DMF_STDLIB_H */
