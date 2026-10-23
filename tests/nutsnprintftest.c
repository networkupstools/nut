/*  nutsnprintftest.c - check that snprintf() and vsnprintf() as seen by NUT
 *  code, and NUT helpers built on them, have C99 semantics for truncated
 *  output: the result is always NUL-terminated, and the return value is
 *  the would-be output length.
 *
 *  NUT code relies on that everywhere; e.g. Microsoft _snprintf() differs
 *  (returns -1 and does not terminate the buffer when output is truncated)
 *  so it must not be used instead, as wincompat.h historically did.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "config.h"
#include "common.h"
#ifdef WIN32
# include "wincompat.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define SOURCE_STRING	"0123456789"
#define BUFFER_SIZE	8

/* Fill the test buffer (larger than BUFFER_SIZE, so that a missing NUL
 * is detectable but harmless) with a known non-NUL pattern */
static void prepare(char *buf, size_t bufsize)
{
	memset(buf, 'X', bufsize);
	buf[bufsize - 1] = '\0';
}

static int check_result(const char *what, int ret, const char *buf)
{
	int	failed = 0;

	if (ret != (int)strlen(SOURCE_STRING)) {
		printf("FAIL: %s returned %d, expected %d (would-be length)\n",
			what, ret, (int)strlen(SOURCE_STRING));
		failed++;
	}

	if (buf[BUFFER_SIZE - 1] != '\0') {
		printf("FAIL: %s did not NUL-terminate the truncated output\n",
			what);
		failed++;
	} else if (strcmp(buf, "0123456") != 0) {
		printf("FAIL: %s produced '%s', expected '0123456'\n",
			what, buf);
		failed++;
	}

	if (!failed)
		printf("PASS: %s truncates with C99 semantics\n", what);

	return failed;
}

static int call_vsnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list	ap;
	int	ret;

	va_start(ap, fmt);
#include "nut-pragmas-format-nonliteral.h"
	/* Note: this code intentionally uses a caller-provided format string */
	ret = vsnprintf(buf, size, fmt, ap);
#include "nut-pragmas-format-nonliteral-end.h"
	va_end(ap);

	return ret;
}

static int call_vsnprintf_dynamic(char *buf, size_t size,
	const char *fmt_dynamic, const char *fmt_reference, ...)
{
	va_list	ap;
	int	ret;

	va_start(ap, fmt_reference);
	ret = vsnprintf_dynamic(buf, size, fmt_dynamic, fmt_reference, ap);
	va_end(ap);

	return ret;
}

int main(void)
{
	char	buf[BUFFER_SIZE + 8];
	int	ret, failed = 0;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
	/* The C library functions, as mapped (or not) by NUT headers */
	prepare(buf, sizeof(buf));
	ret = snprintf(buf, BUFFER_SIZE, "%s", SOURCE_STRING);
	failed += check_result("snprintf()", ret, buf);

	prepare(buf, sizeof(buf));
	ret = call_vsnprintf(buf, BUFFER_SIZE, "%s", SOURCE_STRING);
	failed += check_result("vsnprintf()", ret, buf);

	/* NUT helpers in libcommon which pass through to vsnprintf() */
	prepare(buf, sizeof(buf));
	ret = snprintf_dynamic(buf, BUFFER_SIZE, "%s", "%s", SOURCE_STRING);
	failed += check_result("snprintf_dynamic()", ret, buf);

	prepare(buf, sizeof(buf));
	ret = call_vsnprintf_dynamic(buf, BUFFER_SIZE, "%s", "%s", SOURCE_STRING);
	failed += check_result("vsnprintf_dynamic()", ret, buf);

	/* snprintfcat() appends to existing content and returns the would-be
	 * total length; it must keep the result terminated within the buffer */
	prepare(buf, sizeof(buf));
	buf[0] = 'A';
	buf[1] = 'B';
	buf[2] = '\0';
	ret = snprintfcat(buf, BUFFER_SIZE, "%s", SOURCE_STRING);
	if (ret != 2 + (int)strlen(SOURCE_STRING)) {
		printf("FAIL: snprintfcat() returned %d, expected %d (would-be length)\n",
			ret, 2 + (int)strlen(SOURCE_STRING));
		failed++;
	} else if (memchr(buf, '\0', BUFFER_SIZE) == NULL
	        || strncmp(buf, "AB0", 3) != 0
	) {
		printf("FAIL: snprintfcat() produced unterminated or wrong output\n");
		failed++;
	} else {
		printf("PASS: snprintfcat() truncates with C99-like semantics\n");
	}

	/* Control: output which fits must be unaffected */
	prepare(buf, sizeof(buf));
	ret = snprintf(buf, BUFFER_SIZE, "%s", "0123");
	if (ret != 4 || strcmp(buf, "0123") != 0) {
		printf("FAIL: snprintf() of fitting output returned %d '%s'\n",
			ret, buf);
		failed++;
	} else {
		printf("PASS: snprintf() of fitting output\n");
	}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic pop
#endif

	return (failed ? 1 : 0);
}
