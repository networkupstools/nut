/* setenv.c Ben Collver <collver@softhome.net> */
#ifndef HAVE_SETENV
#include <stdlib.h>
#include <string.h>
#include "common.h"

int nut_setenv(const char *name, const char *value, int overwrite)
{
	char	*val;
	char	*buffer;
	int	rv;

	if (overwrite == 0) {
		val = getenv(name);
		if (val != NULL) {
			return 0;
		}
	}

	buffer = xmalloc(strlen(value) + strlen(name) + 2);
	strcpy(buffer, name);
	strcat(buffer, "=");
	strcat(buffer, value);
	rv = putenv(buffer); /* man putenv, do not free(buffer) */
	return (rv);
}
#endif
