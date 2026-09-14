/* unsetenv.c Jim Klimov <jimklimov+nut@gmail.com> */
#include "config.h" /* must be first */

#ifndef HAVE_UNSETENV
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "proto.h"

int nut_unsetenv(const char *name)
{
	return setenv(name, "", 1);
}
#endif	/* !HAVE_UNSETENV */
