/*  driver_methods_utest.c - NUT driver code test tool
 *
 *  Copyright (C)
 *	2025		Jim Klimov <jimklimov+nut@gmail.com>
 *	2025		desertwitch <dezertwitsh@gmail.com>
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
#define DRIVER_NAME	"Mock driver for unit tests"
#define DRIVER_VERSION	"0.02"

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
void upsdrv_initups(void) {}
void upsdrv_initinfo(void) {}
void upsdrv_makevartable(void) {}
void upsdrv_tweak_prognames(void) {}
void upsdrv_updateinfo(void) {}
void upsdrv_help(void) {}

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

/* #if !(defined ENABLE_SHARED_PRIVATE_LIBS) || !ENABLE_SHARED_PRIVATE_LIBS */
	default_register_upsdrv_callbacks();
/* #endif */

	cases_passed = 0;
	cases_failed = 0;

	/* Test case #1 (from scratch)
	 * We test for duplicate status tokens (no alarms).
	 * Expectation: no duplicates among status tokens.
	 */
	status_init();
	status_set(" OL ");
	status_set("OL BOOST");
	status_set("OB ");
	status_set(" BOOST");
	status_set(" LB");	/* Initial ignorelb==0 so this should get set */
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "OL BOOST OB LB"));
	printf(" test for ups.status: '%s'; got no duplicates?\n", NUT_STRARG(valueStr));

	/* Test case #2 (built on top of #1)
	 * We raise an alarm using the modern alarm functions.
	 * Expectation: ALARM status token is present.
	 * Note: normally we would re-init and re-set the values.
	 */
	alarm_init();
	alarm_set("Test alarm 1");
	alarm_set("[Test alarm 2]");
	alarm_set("Test alarm 1");
	alarm_commit();
	status_commit(); /* to register ALARM status */

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB LB"));
	printf(" test for ups.status: '%s'; got alarm, no duplicates?\n", NUT_STRARG(valueStr));

	/* Test case #3 (built on top of #2)
	 * We raise an alarm using the modern alarm functions.
	 * Expectation: three alarm messages stored in ups.alarm.
	 * Note: normally we would re-init and re-set the values.
	 */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "Test alarm 1 [Test alarm 2] Test alarm 1"));
	printf(" test for ups.alarm: '%s'; got 3 alarms?\n", NUT_STRARG(valueStr));

	/* Test case #4 (built on top of #3)
	 * Reset the status, see if the raised modern alarm persists.
	 * Expectation: ALARM remains, no duplicate tokens reported.
	 * Note: normally we would re-init and re-set the values.
	 */
	status_init();
	status_set(" OL ");
	status_set("OL BOOST");
	status_set("OB ");
	status_set(" BOOST");
	status_set("BOO");
	status_set("BOO");
	status_set("OST");
	status_set("OST");
	status_set("OOS");
	status_set("OOS");
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB BOO OST OOS"));
	printf(" test for ups.status: '%s'; got alarm, no duplicates?\n", NUT_STRARG(valueStr));

	/* Test case #5 (built on top of #4)
	 * Reset the status, see if the raised modern alarm persists.
	 * Expectation: three alarm messages still stored in ups.alarm.
	 * Note: normally we would re-init and re-set the values.
	 */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "Test alarm 1 [Test alarm 2] Test alarm 1"));
	printf(" test for ups.alarm: '%s'; got 3 alarms?\n", NUT_STRARG(valueStr));

	/* Clear the testing stage for the next from-scratch test. */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* Test case #6 (from scratch)
	 * Mix legacy alarm status and modern alarm functions.
	 * Expectation: ALARM status token is present, no duplicates.
	 */
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
	printf(" test for ups.status with explicit ALARM set via status_set() and alarm_set() was not used first: '%s'; got alarm, no duplicates?\n", NUT_STRARG(valueStr));

	/* Test case #7 (built on top of #6)
	 * Mix legacy alarm status and modern alarm functions.
	 * Expectation: 1 alarm message is recorded in ups.alarm.
	 * Note: normally we would re-init and re-set the values.
	 */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm 2]"));
	printf(" test for ups.alarm with explicit ALARM set via status_set() and alarm_set() was not used first: '%s'; got 1 alarm?\n", NUT_STRARG(valueStr));

	/* Clear the testing stage for the next from-scratch test. */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* Test case #8 (from scratch)
	 * Mix legacy alarm status and modern alarm functions (reverse).
	 * Expectation: ALARM status token is present, no duplicates.
	 */
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
	printf(" test for ups.status with explicit ALARM set via status_set() and alarm_set() was used first: '%s'; got alarm, no duplicates?\n", NUT_STRARG(valueStr));

	/* Test case #9 (built on top of #8)
	 * Mix legacy alarm status and modern alarm functions (reverse).
	 * Expectation: 1 alarm message is recorded in ups.alarm.
	 * Note: normally we would re-init and re-set the values.
	 */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm 2]"));
	printf(" test for ups.alarm with explicit ALARM set via status_set() and alarm_set() was used first: '%s'; got 1 alarm?\n", NUT_STRARG(valueStr));

	/* Clear the testing stage for the next from-scratch test. */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* Test case #10 (from scratch)
	 * Legacy alarm status and no modern alarm functions.
	 * Expectation: ALARM status token is present, no duplicates.
	 */
	status_init();
	status_set("OL BOOST");
	status_set("ALARM");
	status_set("OB");
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status with explicit ALARM set via status_set() and no extra alarm_ funcs: '%s'; got alarm, no duplicates?\n", NUT_STRARG(valueStr));

	/* Test case #11 (built on top of #10)
	 * Legacy alarm status and no modern alarm functions.
	 * Expectation: no alarm message is recorded in ups.alarm.
	 * Note: normally we would re-init and re-set the values.
	 */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(valueStr != NULL); /* should be NULL */
	printf(" test for ups.alarm with explicit ALARM set via status_set() and no extra alarm_ funcs: '%s'; got NULL?\n", NUT_STRARG(valueStr));

	/* Clear testing state for next from-scratch test.
	 * alarm_ was not used, so it should be cleared from before.
	 */
	status_init();
	status_commit();

	/* Test case #12 (from scratch)
	 * Ignored LB.
	 * Expectation: previous ALARM is cleared, LB is ignored.
	 */
	dstate_setinfo("driver.flag.ignorelb", "enabled");

	status_init();
	status_set("OL BOOST");
	status_set("OB");
	status_set("LB");	/* ignorelb should be honoured */
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "OL BOOST OB"));
	printf(" test for ups.status with ignored LB: '%s'; got no alarm, no LB, no duplicates?\n", NUT_STRARG(valueStr));

	/* Clear the testing stage for the next from-scratch test. */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* Test case #13 (from scratch)
	 * Alarm raised with modern alarm functions, no status, no status commit.
	 * Expectation: empty status, no whitespace.
	 */
	alarm_init();
	alarm_set("[Test alarm]");
	alarm_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "\0"));
	printf(" test for ups.status with alarm_ set and no status_commit(): '%s'; got empty, no whitespace?\n", NUT_STRARG(valueStr));

	/* Test case #14 (built on top of #13)
	 * Alarm raised with modern alarm functions, no status, no status commit.
	 * Expectation: 1 alarm message is recorded in ups.alarm.
	 * Note: normally we would re-init and re-set the values.
	 */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm]"));
	printf(" test for ups.status with alarm_ set and no status_commit(): '%s'; got 1 alarm?\n", NUT_STRARG(valueStr));

	/* Clear the testing stage for the next from-scratch test. */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* Test case #15 (from scratch)
	 * Alarm raised with modern alarm functions, no status, but status commit.
	 * Expectation: ALARM status token is present, no leading/trailing spaces.
	 */
	alarm_init();
	alarm_set("alarm");
	alarm_commit();
	status_commit(); /* to register ALARM status */

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM"));
	printf(" test for ups.status with alarm_ set, no status and status_commit(): '%s'; got alarm, no whitespace?\n", NUT_STRARG(valueStr));

	/* Clear the testing stage for the next from-scratch test. */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* Test case #16 (from scratch)
	 * Alarm raised with modern alarm functions, status commit before alarm commit.
	 * Expectation: empty status, no whitespace.
	 */
	alarm_init();
	alarm_set("alarm");
	status_commit();
	alarm_commit(); /* too late? */

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "\0"));
	printf(" test for ups.status with alarm_ set, status_commit() before alarm_commit(): '%s'; got empty, no whitespace?\n", NUT_STRARG(valueStr));

	/* Test cases #17+#18+#19+#20 (from scratch, checking data step by step)
	 * Set and commit a status, then add and commit some more (without
	 * a re-init). The resulting ups.status should contain all set tokens.
	 */
	/* NOTE: This is a flag, either present with any value (true) or absent */
	dstate_delinfo("driver.flag.ignorelb");

	alarm_init();
	alarm_commit();
	status_init();
	status_set("OB");
	status_set("LB");	/* LB should be honoured when we do not ignorelb */

	/* #17 */
	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "\0"));
	printf(" test for ups.status after status_set() and before status_commit(): '%s'; got empty, no whitespace?\n", NUT_STRARG(valueStr));

	status_commit();

	/* #18 */
	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "OB LB"));
	printf(" test for ups.status with just OB and LB set via status_set() and committed: '%s'; got OB LB?\n", NUT_STRARG(valueStr));

	status_set("FSD");

	/* #19 */
	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "OB LB"));
	printf(" test for ups.status with next token set via status_set() but not yet committed: '%s'; got OB LB, no FSD?\n", NUT_STRARG(valueStr));

	status_commit();

	/* #20 */
	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "OB LB FSD"));
	printf(" test for ups.status with FSD token set and now committed: '%s'; got OB LB FSD?\n", NUT_STRARG(valueStr));

	/* Clear testing state before finishing. */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* Finish */
	printf("test_rules completed. Total cases %d, passed %d, failed %d\n",
		cases_passed+cases_failed, cases_passed, cases_failed);

	dstate_free();
	upsdrv_cleanup();

	/* Return 0 (exit-code OK, boolean false) if no tests failed and some ran */
	if ( (cases_failed == 0) && (cases_passed > 0) )
		return 0;

	return 1;
}
