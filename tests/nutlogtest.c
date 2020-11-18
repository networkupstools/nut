/* nutlogtest - some trivial usage for upslog*() and upsdebug*() related
 * routines to sanity-check their code (compiler does not warn, test runs
 * do not crash).
 */
#include "common.h"

int main(void) {
    const char *s1 = "!NULL";
    const char *s2 = NULL;
    upsdebugx(0, "D: '%s' vs '%s'", s1, s2);
    return 0;
}
