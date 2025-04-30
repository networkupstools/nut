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

	/* test case #1 (from scratch) */
	/* we test for duplicate status tokens (no alarms) */
	/* expectation: no duplicates among status tokens */
	status_init();
	status_set(" OL ");
	status_set("OL BOOST");
	status_set("OB ");
	status_set(" BOOST");
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "OL BOOST OB"));
	printf(" test for ups.status: '%s'; got no duplicates?\n", NUT_STRARG(valueStr));

	/* test case #2 (built on top of #1) */
	/* we raise an alarm using the modern alarm functions */
	/* expectation: alarm status token is present */
	/* note: normally we would re-init and re-set the values */
	alarm_init();
	alarm_set("Test alarm 1");
	alarm_set("[Test alarm 2]");
	alarm_set("Test alarm 1");
	alarm_commit();
	status_commit(); /* to register ALARM status */

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status: '%s'; got alarm, no duplicates?\n", NUT_STRARG(valueStr));

	/* test case #3 (built on top of #2) */
	/* we raise an alarm using the modern alarm functions */
	/* expectation: three alarm messages stored in ups.alarm */
	/* note: normally we would re-init and re-set the values */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "Test alarm 1 [Test alarm 2] Test alarm 1"));
	printf(" test for ups.alarm: '%s'; got 3 alarms?\n", NUT_STRARG(valueStr));

	/* test case #4 (built on top of #3) */
	/* reset the status, see if the raised modern alarm persists */
	/* expectation: alarm remains, no duplicate tokens reported */
	/* note: normally we would re-init and re-set the values */
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

	/* test case #5 (built on top of #4) */
	/* reset the status, see if the raised modern alarm persists */
	/* expectation: three alarm messages still stored in ups.alarm */
	/* note: normally we would re-init and re-set the values */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "Test alarm 1 [Test alarm 2] Test alarm 1"));
	printf(" test for ups.alarm: '%s'; got 3 alarms?\n", NUT_STRARG(valueStr));

	/* clear testing state for next from-scratch test */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* test case #6 (from scratch) */
	/* mix legacy alarm status and modern alarm functions */
	/* expectation: alarm status token is present, no duplicates */
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

	/* test case #7 (built on top of #6) */
	/* mix legacy alarm status and modern alarm functions */
	/* expectation: 1 alarm message is recorded in ups.alarm */
	/* note: normally we would re-init and re-set the values */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm 2]"));
	printf(" test for ups.alarm with explicit ALARM set via status_set() and alarm_set() was not used first: '%s'; got 1 alarm?\n", NUT_STRARG(valueStr));

	/* clear testing state for next from-scratch test */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* test case #8 (from scratch) */
	/* mix legacy alarm status and modern alarm functions (reverse) */
	/* expectation: alarm status token is present, no duplicates */
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

	/* test case #9 (built on top of #8) */
	/* mix legacy alarm status and modern alarm functions (reverse) */
	/* expectation: 1 alarm message is recorded in ups.alarm */
	/* note: normally we would re-init and re-set the values */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm 2]"));
	printf(" test for ups.alarm with explicit ALARM set via status_set() and alarm_set() was used first: '%s'; got 1 alarm?\n", NUT_STRARG(valueStr));

	/* clear testing state for next from-scratch test */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* test case #10 (from scratch) */
	/* legacy alarm status and no modern alarm functions */
	/* expectation: alarm status token is present, no duplicates */
	status_init();
	status_set("OL BOOST");
	status_set("ALARM");
	status_set("OB");
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM OL BOOST OB"));
	printf(" test for ups.status with explicit ALARM set via status_set() and no extra alarm_ funcs: '%s'; got alarm, no duplicates?\n", NUT_STRARG(valueStr));

	/* test case #11 (built on top of #10) */
	/* legacy alarm status and no modern alarm functions */
	/* expectation: no alarm message is recorded in ups.alarm */
	/* note: normally we would re-init and re-set the values */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(valueStr != NULL); /* should be NULL */
	printf(" test for ups.alarm with explicit ALARM set via status_set() and no extra alarm_ funcs: '%s'; got NULL?\n", NUT_STRARG(valueStr));

	/* clear testing state for next from-scratch test */
	/* alarm_ was not used, should be cleared from before */
	status_init();
	status_commit();

	/* test case #12 (from scratch) */
	/* ignored LB */
	/* expectation: previous legacy alarm is cleared, LB ignored */
	dstate_setinfo("driver.flag.ignorelb", "enabled");

	status_init();
	status_set("OL BOOST");
	status_set("OB");
	status_set("LB");	/* Should be honoured */
	status_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "OL BOOST OB"));
	printf(" test for ups.status with ignored LB: '%s'; got no alarm, ignored LB, no duplicates?\n", NUT_STRARG(valueStr));

	/* clear testing state for next from-scratch test */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* test case #13 (from scratch) */
	/* alarm raised with modern alarm functions, no status, no status commit */
	/* expectation: empty status, no whitespace */
	alarm_init();
	alarm_set("[Test alarm]");
	alarm_commit();

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "\0"));
	printf(" test for ups.status with alarm_ set and no status_commit(): '%s'; got empty, no whitespace?\n", NUT_STRARG(valueStr));

	/* test case #14 (built on top of #13) */
	/* alarm raised with modern alarm functions, no status, no status commit */
	/* expectation: 1 alarm message is recorded in ups.alarm */
	/* note: normally we would re-init and re-set the values */
	valueStr = dstate_getinfo("ups.alarm");
	report_0_means_pass(strcmp(valueStr, "[Test alarm]"));
	printf(" test for ups.status with alarm_ set and no status_commit(): '%s'; got 1 alarm?\n", NUT_STRARG(valueStr));

	/* clear testing state for next from-scratch test */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* test case #15 (from scratch) */
	/* alarm raised with modern alarm functions, no status, but status commit */
	/* expectation: alarm status token is present, no leading/trailing spaces */
	alarm_init();
	alarm_set("alarm");
	alarm_commit();
	status_commit(); /* to register ALARM status */

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "ALARM"));
	printf(" test for ups.status with alarm_ set, no status and status_commit(): '%s'; got alarm, no whitespace?\n", NUT_STRARG(valueStr));

	/* clear testing state for next from-scratch test */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

	/* test case #16 (from scratch) */
	/* alarm raised with modern alarm functions, status commit before alarm commit */
	/* expectation: empty status, no whitespace */
	alarm_init();
	alarm_set("alarm");
	status_commit();
	alarm_commit(); /* too late? */

	valueStr = dstate_getinfo("ups.status");
	report_0_means_pass(strcmp(valueStr, "\0"));
	printf(" test for ups.status with alarm_ set, status_commit() before alarm_commit(): '%s'; got empty, no whitespace?\n", NUT_STRARG(valueStr));

	/* clear testing state before finish */
	alarm_init();
	alarm_commit();
	status_init();
	status_commit();

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
