/* upsconf_maxpath_utest.c - reject oversized overrides before filesystem I/O.
 * Uses the actual platform-specific NUT_PATH_MAX without exceeding execve's
 * environment limit. Runs independently of USB, discovery and Python.
 * Copyright 2026 NUT contributors. GPL version 2 or later.
 */
#include "config.h"
#include "common.h"
#include "upsconf.h"

static int parsed;

void do_upsconf_args(char *name, char *var, char *value)
{
	NUT_UNUSED_VARIABLE(name);
	NUT_UNUSED_VARIABLE(var);
	NUT_UNUSED_VARIABLE(value);
	parsed++;
}

int main(void)
{
	char *path = (char *)xmalloc(NUT_PATH_MAX + 2);
	int result;
	memset(path, 'x', NUT_PATH_MAX + 1);
	path[NUT_PATH_MAX + 1] = '\0';
	if (setenv("NUT_UPSCONF", path, 1)) {
		fatal_with_errno(EXIT_FAILURE, "setenv");
	}
	result = read_upsconf(0);
	free(path);
	return result == -1 && !parsed ? EXIT_SUCCESS : EXIT_FAILURE;
}
