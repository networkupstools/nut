/*  driver_methods_utest.c - NUT driver code test tool
 *
 *  Copyright (C)
 *	2025       	Jim Klimov <jimklimov+nut@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "config.h"
#include "main.h"
#include "dstate.h"
#include "attribute.h"
#include "nut_stdint.h"

/* driver version */
#define DRIVER_NAME     "Mock driver for unit tests"
#define DRIVER_VERSION  "0.01"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Jim Klimov <jimklimov+nut@gmail.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static int cases_passed = 0;
static int cases_failed = 0;

static char * pass_fail[2] = {"pass", "fail"};

void upsdrv_cleanup(void) {}
void upsdrv_shutdown(void) {}

static void report_pass(void) {
	printf("%s", pass_fail[0]);
	cases_passed++;
}

static void report_fail(void) {
	printf("%s", pass_fail[1]);
	cases_failed++;
}

static int report_0_means_pass(int i) {
	if (i == 0) {
		report_pass();
	} else {
		report_fail();
	}
	return i;
}

int main(int argc, char **argv) {
	const char	*valueStr = NULL;

	NUT_UNUSED_VARIABLE(argc);
	NUT_UNUSED_VARIABLE(argv);

	cases_passed = 0;
	cases_failed = 0;

	/* test case #1 */
	status_init();
	nut_debug_level = 6;
	status_set(" OL ");
	status_set("OL BOOST");
	status_set("OB ");
	status_set(" BOOST");
	status_commit();
	valueStr = dstate_getinfo("ups.status");
	nut_debug_level = 0;
	report_0_means_pass(strcmp(valueStr, "OL BOOST OB"));
	printf(" test for ups.status: '%s'; any duplicates?\n", NUT_STRARG(valueStr));

	/* test case #2, build on top of #1 */
	alarm_init();
	alarm_set("Test alarm 1");
	alarm_set("[Test alarm 2]");
	alarm_set("Test alarm 1");
	alarm_commit();
	/* Note: normally we re-init and re-set the values */
	status_commit();
	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status: '%s'; got alarm?\n", NUT_STRARG(valueStr));

	/* test case #3, build on top of #2 */
	valueStr = dstate_getinfo("ups.alarm");
	/* NOTE: no dedup here! */
	report_0_means_pass(strcmp(valueStr, "Test alarm 1 [Test alarm 2] Test alarm 1"));
	printf(" test for ups.alarm: '%s'; got 3 alarms?\n", NUT_STRARG(valueStr));

	/* test case #4, build on top of #1 and #2 */
	/* Note: normally we re-init and re-set the values */
	status_set("BOO");
	status_set("BOO");
	status_set("OST");
	status_set("OST");
	status_set("OOS");
	status_set("OOS");
	status_commit();
	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB BOO OST OOS"));
	printf(" test for ups.status: '%s'; any duplicates?\n", NUT_STRARG(valueStr));

	/* test case #5+#6 from scratch */
	status_init();
	alarm_init();
	status_set("OL BOOST");
	status_set("ALARM");
	status_set("OB");
	alarm_set("[Test alarm 2]");
	alarm_commit();
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status with explicit ALARM set via status_set() and alarm_set() was not used first: '%s'; any duplicate ALARM?\n", NUT_STRARG(valueStr));

	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm 2]"));
	printf(" test for ups.alarm  with explicit ALARM set via status_set() and alarm_set() was not used first: '%s'; got 1 alarm (injected N/A replaced by later explicit text)\n", NUT_STRARG(valueStr));

	/* test case #7+#8 from scratch */
	status_init();
	alarm_init();
	status_set("OL BOOST");
	alarm_set("[Test alarm 2]");
	status_set("ALARM");
	status_set("OB");
	alarm_commit();
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status with explicit ALARM set via status_set() and alarm_set() was used first: '%s'; any duplicate ALARM?\n", NUT_STRARG(valueStr));

	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm 2]"));
	printf(" test for ups.alarm  with explicit ALARM set via status_set() and alarm_set() was used first: '%s'; got 1 alarm (none injected)\n", NUT_STRARG(valueStr));

	/* test case #9+#10 from scratch */
	status_init();
	alarm_init();
	status_set("OL BOOST");
	status_set("ALARM");
	status_set("OB");
	alarm_commit();
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status with explicit ALARM set via status_set() and no extra alarm_set(): '%s'; any duplicate ALARM?\n", NUT_STRARG(valueStr));

	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(NUT_STRARG(valueStr), "[N/A]"));
	printf(" test for ups.alarm  with explicit ALARM set via status_set() and no extra alarm_set(): '%s'; got 1 (injected) alarm\n", NUT_STRARG(valueStr));

	/* test case #11+#12 from scratch */
	/* flush ups.alarm report */
	alarm_init();
	alarm_commit();

	status_init();
	alarm_init();
	status_set("OL BOOST");
	status_set("ALARM");
	status_set("OB");
	status_set("LB");	/* Should be honoured */
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB LB"));
	printf(" test for ups.status with explicit ALARM set via status_set() and no extra alarm_set() nor alarm_commit(): '%s'; is ALARM reported?\n", NUT_STRARG(valueStr));

	valueStr = dstate_getinfo("ups.alarm");
/*
	report_0_means_pass(valueStr != NULL);	// pass if valueStr is NULL
	printf(" test for ups.alarm  with explicit ALARM set via status_set() and no extra alarm_set() nor alarm_commit(): '%s'; got no alarms spelled out\n", NUT_STRARG(valueStr));
*/
	report_0_means_pass(strcmp(NUT_STRARG(valueStr), "[N/A]"));
	printf(" test for ups.alarm  with explicit ALARM set via status_set() and no extra alarm_set() nor alarm_commit(): '%s'; got 1 (injected) alarm\n", NUT_STRARG(valueStr));

	/* test case #13+#14 from scratch */
	/* flush ups.alarm report */
	alarm_init();
	alarm_commit();

	dstate_setinfo("driver.flag.ignorelb", "enabled");

	status_init();
	alarm_init();
	status_set("OL BOOST");
	alarm_set("[N/A]");
	alarm_set("[Test alarm 2]");
	status_set("OB");
	status_set("LB");	/* Should be ignored */
	alarm_commit();
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status with explicit alarm_set(N/A) and another alarm_set(): '%s'; is ALARM reported?\n", NUT_STRARG(valueStr));

	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[N/A] [Test alarm 2]"));
	printf(" test for ups.alarm  with explicit alarm_set(N/A) and another alarm_set(): '%s'; got both alarms, namesake of not-injected is not overwritten\n", NUT_STRARG(valueStr));

	/* finish */
	printf("test_rules completed. Total cases %d, passed %d, failed %d\n",
		cases_passed+cases_failed, cases_passed, cases_failed);

	dstate_free();
	upsdrv_cleanup();

	/* Return 0 (exit-code OK, boolean false) if no tests failed and some ran */
	if ( (cases_failed == 0) && (cases_passed > 0) )
		return 0;

	return 1;
}
