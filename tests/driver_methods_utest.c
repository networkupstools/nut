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
	if (!strcmp(valueStr, "OL BOOST OB")) {
		printf("%s", pass_fail[0]);
		cases_passed++;
	} else {
		printf("%s", pass_fail[1]);
		cases_failed++;
	}
	printf(" test for ups.status: '%s'; any duplicates?\n", NUT_STRARG(valueStr));

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
