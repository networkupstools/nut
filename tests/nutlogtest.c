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
    upsdebugx(0, "D: '%s' vs '%s'", s1, s2);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_OVERFLOW)
#  pragma GCC diagnostic pop
#endif

    return 0;
}
