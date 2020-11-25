/* nutlogtest - some trivial usage for upslog*() and upsdebug*() related
 * routines to sanity-check their code (compiler does not warn, test runs
 * do not crash).
 */
#include "common.h"

int main(void) {
    const char *s1 = "!NULL";
    const char *s2 = NULL;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_OVERFLOW)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-overflow"
#endif
    upsdebugx(0, "D: checking with libc handling of NULL: '%s' vs '%s'", s1, s2);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_OVERFLOW)
#  pragma GCC diagnostic pop
#endif

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

    upsdebugx(0, "D: checking that macro wrap trick works: '%s' vs '%s'", NUT_STRARG(s2), s2);

    return 0;
}
