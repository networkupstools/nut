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
			SU_INFOSIZE, "not-an-oid.%i", NULL, SU_OUTLET | SU_FLAG_OK, NULL),
		snmp_info_default("outlet.unavailable", ST_FLAG_RW | ST_FLAG_STRING,
			20, NULL, "Unavailable", SU_FLAG_ABSENT, NULL),
		snmp_info_default("outlet.load.off", 0, 1,
			"not-an-oid", "0", SU_TYPE_CMD | SU_FLAG_OK, NULL),
		snmp_info_sentinel
	};
	const char *value;
	int failed = 0;

	snmp_info = test_snmp_info;
	dstate_setinfo("outlet.count", "%d", 1);
	dstate_setinfo("outlet.desc", "%s", "All outlets");

	failed += expect_result("server-side outlet.desc",
		su_setvar("outlet.desc", "All rack outlets"), STAT_SET_HANDLED);
	value = dstate_getinfo("outlet.desc");
	if (!value || strcmp(value, "All rack outlets")) {
		fprintf(stderr, "server-side outlet.desc: got '%s'\n", NUT_STRARG(value));
		failed++;
	}

	failed += expect_result("numbered outlet template",
		su_setvar("outlet.1.desc", "Rack outlet 1"), STAT_SET_FAILED);
	failed += expect_result("unknown numbered outlet template",
		su_setvar("outlet.1.missing", "unused"), STAT_SET_UNKNOWN);
	failed += expect_result("unavailable exact outlet mapping",
		su_setvar("outlet.unavailable", "unused"), STAT_SET_UNKNOWN);
	failed += expect_result("exact outlet command",
		su_instcmd("outlet.load.off", NULL), STAT_INSTCMD_FAILED);

	dstate_free();
	snmp_info = NULL;

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
