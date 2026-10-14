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
	static	int	ups_section_ignored = 0;

	struct section_node {
		char	*name;
		struct section_node	*next;
	};

	static	struct section_node	*seen_sections = NULL;

	void (*callback_upsconf_args)(char *upsname, char *var, char *val) = NULL;

static void free_seen_sections(void)
{
	struct section_node	*curr = seen_sections;

	while (curr) {
		struct section_node	*next = curr->next;
		free(curr->name);
		free(curr);
		curr = next;
	}
	seen_sections = NULL;
}

static int is_seen_section(const char *name)
{
	struct section_node	*curr;

	for (curr = seen_sections; curr != NULL; curr = curr->next) {
		if (!strcmp(curr->name, name))
			return 1;
	}
	return 0;
}

static void add_seen_section(const char *name)
{
	struct section_node	*node = (struct section_node *)xmalloc(sizeof(*node));
	node->name = xstrdup(name);
	node->next = seen_sections;
	seen_sections = node;
}

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
		ups_section = NULL;

		arg[0][strlen(arg[0])-1] = '\0';

		if (is_seen_section(&arg[0][1])) {
			upslogx(LOG_WARNING, "Warning: ignoring duplicate UPS [%s]", &arg[0][1]);
			ups_section_ignored = 1;
			return;
		}

		add_seen_section(&arg[0][1]);
		ups_section_ignored = 0;
		ups_section = xstrdup(&arg[0][1]);
		return;
	}

	if (ups_section_ignored)
		return;

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
	char	**files;
	size_t	i, count = 0;

	snprintf(fn, sizeof(fn), "%s/ups.conf", confpath());

	files = pconf_get_files(fn, &count);
	if (!files || count == 0) {
		if (fatal_errors) {
			fatalx(EXIT_FAILURE, "Can't open %s (and %s.d/*.conf)", fn, fn);
		} else {
			upslogx(LOG_WARNING, "Can't open %s (and %s.d/*.conf)", fn, fn);
			return -1;
		}
	}

	free_seen_sections();
	ups_section = NULL;
	ups_section_ignored = 0;

	for (i = 0; i < count; i++) {
		ups_section = NULL;
		ups_section_ignored = 0;
		pconf_init(&ctx, upsconf_err);

		if (!pconf_file_begin(&ctx, files[i])) {
			if (fatal_errors) {
				fatalx(EXIT_FAILURE, "Can't open %s: %s", files[i], ctx.errmsg);
			} else {
				upslogx(LOG_WARNING, "Can't open %s: %s", files[i], ctx.errmsg);
				pconf_finish(&ctx);
				pconf_free_files(files);
				free_seen_sections();
				return -1;
			}
		}

		while (pconf_file_next(&ctx)) {
			if (pconf_parse_error(&ctx)) {
				upslogx(LOG_ERR, "Parse error: %s:%d: %s",
					files[i], ctx.linenum, ctx.errmsg);
				continue;
			}

			conf_args(ctx.numargs, ctx.arglist);
		}

		pconf_finish(&ctx);
		free(ups_section);
		ups_section = NULL;
		ups_section_ignored = 0;
	}

	pconf_free_files(files);
	free_seen_sections();
	return 1; /* Handled OK */
}
