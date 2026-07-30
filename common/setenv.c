/* fallback setenv.c Ben Collver <collver@softhome.net>
 * tightened by Jim Klimov <jimklimov+nut@gmail.com>
 */
#include "config.h" /* must be first */

#ifndef HAVE_SETENV
#include <stdlib.h>
#include <string.h>
#include "common.h"

int nut_setenv(const char *name, const char *value, int overwrite)
{
	char	*val;
	char	*buffer;
	size_t	buflen = 0;
	int	rv;

	if (overwrite == 0) {
		val = getenv(name);
		if (val != NULL) {
			return 0;
		}
	}

	buflen = strlen(value) + strlen(name) + 2;
	buffer = xmalloc(buflen);
	/* TOTHINK: is this stack more portable than one command?
	 *   snprintf(buffer, buflen, "%s=%s", name, value);
	 * (also can easily check that we got (buflen-1) as return value)
	 */
	strncpy(buffer, name, buflen);
	strncat(buffer, "=", buflen);
	strncat(buffer, value, buflen);
	rv = putenv(buffer); /* man putenv, do not free(buffer) */
	return (rv);
}
#endif	/* !HAVE_SETENV */
