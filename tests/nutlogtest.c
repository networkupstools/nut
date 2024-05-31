/* nutlogtest - some trivial usage for upslog*() and upsdebug*() related
 * routines to sanity-check their code (compiler does not warn, test runs
 * do not crash).
 *
 * Copyright (C)
 *	2020-2023	Jim Klimov <jimklimov@gmail.com>
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
#include "common.h"

int main(void) {
	const char *s1 = "!NULL";
	const char *s2 = NULL;
	int ret = 0;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_OVERFLOW)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-overflow"
#endif

	upsdebugx(0, "D: checking with libc handling of NULL (can segfault for some libc implementations):");
	upsdebugx(0, "D:   '%s' vs '%s'", s1, s2);

/* This explicitly does not work with -Wformat, due to verbatim NULL without a var:
 * nutlogtest.c:20:5: error: reading through null pointer (argument 4) [-Werror=format=]
 * and also due to (void*) vs (char*) in naive case:
 *   upsdebugx(0, "D: '%s' vs '%s'", NUT_STRARG(NULL), NULL);
 * but with casting the explicit NULL remains:
 *   upsdebugx(0, "D: '%s' vs '%s'", NUT_STRARG((char *)NULL), (char *)NULL);
 */

	upsdebugx(0, "D: checking with NUT_STRARG macro: '%s' vs '%s'", NUT_STRARG(s2), s2);

#ifdef NUT_STRARG
#undef NUT_STRARG
#endif

#define NUT_STRARG(x) (x?x:"<N/A>")

/* This explicitly does not work with -Wformat, due to a NULL in the '%s'
 * format string expansion (e.g. due to NUT PR #675 conversion to macros):
 *   ../include/common.h:155:41: warning: '%s' directive argument is null [-Wformat-overflow=]
 *   <...snip...>
 *   nutlogtest.c:45:63: note: format string is defined here
 *      45 |     upsdebugx(0, "D: checking with NUT_STRARG macro: '%s' vs '%s'", NUT_STRARG(s2), s2);
 *         |                                                               ^~
 */
	upsdebugx(0, "D: checking that macro wrap trick works: '%s' vs '%s'", NUT_STRARG(s2), s2);

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_OVERFLOW)
#  pragma GCC diagnostic pop
#endif

	if (1) {	/* scoping */
		char *dynfmt = "Test '%s'", *p;
		char buf[LARGEBUF];

		nut_debug_level = 1;
		if (snprintf_dynamic(buf, sizeof(buf), dynfmt, "%s", "Single string via dynamic format") > 0) {
			upsdebugx(0, "D: >>> %s", buf);
			if (!strcmp(buf, "Test 'Single string via dynamic format'")) {
				upsdebugx(0, "D: snprintf_dynamic() prepared a dynamically formatted string with expected content");
			} else {
				upsdebugx(0, "E: snprintf_dynamic() failed to prepare a dynamically formatted string: got unexpected content");
				ret++;
			}
		} else {
			upsdebugx(0, "E: snprintf_dynamic() failed to prepare a dynamically formatted string: returned empty or error");
			ret++;
		}

		if (snprintf_dynamic(buf, sizeof(buf), dynfmt, "%s%d", "Single string via dynamic format", 1) < 0) {
			upsdebugx(0, "D: snprintf_dynamic() correctly reports mis-matched formats");
		} else {
			upsdebugx(0, "E: snprintf_dynamic() wrongly reports well-matched formats");
			ret++;
		}

		/* Note extra non-type chars in "expected" format are stripped */
		p = mkstr_dynamic(dynfmt, " %.4s %%", "Single string inlined by mkstr_dynamic()");
		upsdebugx(0, ">>> %s", NUT_STRARG(p));
		if (!p || *p == '\0' || strcmp(p, "Test 'Single string inlined by mkstr_dynamic()'")) {
			upsdebugx(0, "E: mkstr_dynamic() failed to prepare a dynamically formatted string: got unexpected content");
			ret++;
		} else {
			upsdebugx(0, "D: mkstr_dynamic() prepared a dynamically formatted string with expected content");
		}
	}

	if (1) {	/* scoping */
		char	**p,
			*fmtFloat[] = { "%f", " %A", " %0.1E%% ", NULL },
			*fmtNotFloat[] = { "%f%", "%m", "$f", NULL };

		for (p = &(fmtFloat[0]); *p; p++) {
			if (validate_formatting_string(*p, "Voltage: %G is not %%d") < 0) {
				upsdebugx(0, "E: validate_formatting_string() expecting %%f equivalent failed for: '%s'", *p);
				ret++;
			} else {
				upsdebugx(0, "D: validate_formatting_string() expecting %%f equivalent passed for: '%s'", *p);
			}
		}

		for (p = &(fmtNotFloat[0]); *p; p++) {
			if (validate_formatting_string("%f", *p) < 0) {
				upsdebugx(0, "D: validate_formatting_string() expecting %%f failed (as it should have) for: '%s'", *p);
			} else {
				upsdebugx(0, "E: validate_formatting_string() expecting %%f passed (but should not have) for: '%s'", *p);
				ret++;
			}
		}
	}

	return ret;
}
