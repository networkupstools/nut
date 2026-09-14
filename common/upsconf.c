/* upsconf.c - code for handling ups.conf ini-style parsing

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
       2026            Jim Klimov <jimklimov+nut@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"  /* must be the first header */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "upsconf.h"
#include "common.h"
#include "parseconf.h"

	static	char	*ups_section;

	void (*callback_upsconf_args)(char *upsname, char *var, char *val) = NULL;

/* handle arguments separated by parseconf */
static void conf_args(size_t numargs, char **arg)
{
	if (numargs < 1)
		return;

	if (callback_upsconf_args == NULL) {
#if (defined ENABLE_SHARED_PRIVATE_LIBS) && ENABLE_SHARED_PRIVATE_LIBS
		upsdebugx(1, "%s: coding error: when building NUT with ENABLE_SHARED_PRIVATE_LIBS mode, 'callback_upsconf_args' must be initialized early in ultimate program code", __func__);
		fatalx(EXIT_FAILURE, "FATAL: Dynamic consumer of a NUT private library was not initialized correctly");
#else
		/* We should see the original method in the binary in statically linked scope */
		callback_upsconf_args = do_upsconf_args;
#endif
	}

	/* look for section headers - [upsname] */
	if ((arg[0][0] == '[') && (arg[0][strlen(arg[0])-1] == ']')) {

		free(ups_section);

		arg[0][strlen(arg[0])-1] = '\0';
		ups_section = xstrdup(&arg[0][1]);
		return;
	}

	/* handle 'foo' (flag) */
	if (numargs == 1) {
		callback_upsconf_args(ups_section, arg[0], NULL);
		return;
	}

	if (numargs < 3)
		return;

	/* handle 'foo = bar', 'foo=bar', 'foo =bar' or 'foo= bar' forms */
	if (!strcmp(arg[1], "=")) {
		callback_upsconf_args(ups_section, arg[0], arg[2]);
		return;
	}
}

/* called for fatal errors in parseconf like malloc failures */
static void upsconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(ups.conf): %s", errmsg);
}

/* open the ups.conf, parse it, and call back callback_upsconf_args()
 * returns -1 (or aborts the program) in case of errors;
 * returns 1 if processing finished successfully
 * See also reload_flag support in main.c for live-reload feature
 */
int read_upsconf(int fatal_errors)
{
	char	fn[NUT_PATH_MAX + 1];
	PCONF_CTX_t	ctx;

	ups_section = NULL;
	snprintf(fn, sizeof(fn), "%s/ups.conf", confpath());

	pconf_init(&ctx, upsconf_err);

	if (!pconf_file_begin(&ctx, fn)) {
		if (fatal_errors) {
			fatalx(EXIT_FAILURE, "Can't open %s: %s", fn, ctx.errmsg);
		} else {
			upslogx(LOG_WARNING, "Can't open %s: %s", fn, ctx.errmsg);
			return -1;
		}
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		conf_args(ctx.numargs, ctx.arglist);
	}

	pconf_finish(&ctx);

	free(ups_section);

	return 1; /* Handled OK */
}
