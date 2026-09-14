/* snmp-ups-setvar-test.c - focused tests for SNMP outlet settings
 *
 * Copyright (C) 2026 Network UPS Tools contributors
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

#include "config.h"
#include "main.h"
#include "dstate.h"
#include "snmp-ups.h"

static const char *expected_set_oid;
static const char *expected_set_value;
static char expected_set_type;
static bool_t expected_set_result;
static int set_call_count;
static int set_failures;

bool_t nut_snmp_set(const char *OID, char type, const char *value)
{
	set_call_count++;
	if (strcmp(OID, expected_set_oid)) {
		fprintf(stderr, "setter OID: got '%s', expected '%s'\n",
			OID, expected_set_oid);
		set_failures++;
	}
	if (type != expected_set_type) {
		fprintf(stderr, "setter type: got '%c', expected '%c'\n",
			type, expected_set_type);
		set_failures++;
	}
	if (strcmp(value, expected_set_value)) {
		fprintf(stderr, "setter value: got '%s', expected '%s'\n",
			value, expected_set_value);
		set_failures++;
	}

	return expected_set_result;
}

static void expect_set(const char *OID, char type, const char *value, bool_t result)
{
	expected_set_oid = OID;
	expected_set_type = type;
	expected_set_value = value;
	expected_set_result = result;
	set_call_count = 0;
	set_failures = 0;
}

static int check_set_calls(const char *name, int expected)
{
	if (set_call_count != expected) {
		fprintf(stderr, "%s: setter called %d times, expected %d\n",
			name, set_call_count, expected);
		return set_failures + 1;
	}

	return set_failures;
}

static int expect_result(const char *name, int actual, int expected)
{
	if (actual == expected)
		return 0;

	fprintf(stderr, "%s: got %d, expected %d\n", name, actual, expected);
	return 1;
}

int main(void)
{
	static snmp_info_t test_snmp_info[] = {
		snmp_info_default("outlet.desc", ST_FLAG_RW | ST_FLAG_STRING,
			20, NULL, "All outlets", SU_FLAG_ABSENT | SU_FLAG_OK, NULL),
		snmp_info_default("outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING,
			SU_INFOSIZE, ".1.3.6.1.4.1.9999.%i", NULL, SU_OUTLET | SU_FLAG_OK, NULL),
		snmp_info_default("outlet.unavailable", ST_FLAG_RW | ST_FLAG_STRING,
			20, NULL, "Unavailable", SU_FLAG_ABSENT, NULL),
		snmp_info_default("outlet.load.off", 0, 1,
			".1.3.6.1.4.1.9999.2", "0", SU_TYPE_CMD | SU_FLAG_OK, NULL),
		snmp_info_sentinel
	};
	const char *value;
	int failed = 0;

	snmp_info = test_snmp_info;
	dstate_setinfo("outlet.count", "%d", 1);
	dstate_setinfo("outlet.desc", "%s", "All outlets");

	expect_set("", 0, "", TRUE);
	failed += expect_result("server-side outlet.desc",
		su_setvar("outlet.desc", "All rack outlets"), STAT_SET_HANDLED);
	failed += check_set_calls("server-side outlet.desc", 0);
	value = dstate_getinfo("outlet.desc");
	if (!value || strcmp(value, "All rack outlets")) {
		fprintf(stderr, "server-side outlet.desc: got '%s'\n", NUT_STRARG(value));
		failed++;
	}

	expect_set(".1.3.6.1.4.1.9999.1", 's', "Rack outlet 1", TRUE);
	failed += expect_result("numbered outlet template",
		su_setvar("outlet.1.desc", "Rack outlet 1"), STAT_SET_HANDLED);
	failed += check_set_calls("numbered outlet template", 1);
	value = dstate_getinfo("outlet.1.desc");
	if (!value || strcmp(value, "Rack outlet 1")) {
		fprintf(stderr, "numbered outlet template: got '%s'\n", NUT_STRARG(value));
		failed++;
	}

	expect_set("", 0, "", TRUE);
	failed += expect_result("unknown numbered outlet template",
		su_setvar("outlet.1.missing", "unused"), STAT_SET_UNKNOWN);
	failed += check_set_calls("unknown numbered outlet template", 0);

	expect_set("", 0, "", TRUE);
	failed += expect_result("unavailable exact outlet mapping",
		su_setvar("outlet.unavailable", "unused"), STAT_SET_UNKNOWN);
	failed += check_set_calls("unavailable exact outlet mapping", 0);

	expect_set(".1.3.6.1.4.1.9999.2", 'i', "0", TRUE);
	failed += expect_result("exact outlet command",
		su_instcmd("outlet.load.off", NULL), STAT_INSTCMD_HANDLED);
	failed += check_set_calls("exact outlet command", 1);

	expect_set(".1.3.6.1.4.1.9999.2", 'i', "0", FALSE);
	failed += expect_result("failed exact outlet command",
		su_instcmd("outlet.load.off", NULL), STAT_INSTCMD_FAILED);
	failed += check_set_calls("failed exact outlet command", 1);

	dstate_free();
	snmp_info = NULL;

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
