/* cgilib - common routines for CGI programs

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

#include <ctype.h>

#include "common.h"
#include "cgilib.h"
#include "parseconf.h"

static char *unescape(char *buf)
{
	size_t	i, buflen;
	char	ch, *newbuf, hex[8];

	buflen = strlen(buf) + 2;
	newbuf = xmalloc(buflen);
	*newbuf = '\0';

	fflush(stdout);
	for (i = 0; i < buflen - 1; i++) {
		ch = buf[i];

		if (ch == '+')
			ch = ' ';

		if (ch == '%') {
			if (i + 2 > buflen)
				fatalx(EXIT_FAILURE, "string too short for escaped char");
			hex[0] = buf[++i];
			hex[1] = buf[++i];
			hex[2] = '\0';
			if (!isxdigit((unsigned char) hex[0])
				|| !isxdigit((unsigned char) hex[1]))
				fatalx(EXIT_FAILURE, "bad escape char");
			ch = strtol(hex, NULL, 16);

			if ((ch == 10) || (ch == 13))
				ch = ' ';
		}

		snprintfcat(newbuf, buflen, "%c", ch);
	}

	return newbuf;
}

void extractcgiargs(void)
{
	char	*query, *ptr, *eq, *varname, *value, *amp;
	char	*cleanval, *cleanvar;

	query = getenv("QUERY_STRING");
	if (query == NULL)
		return;		/* not run as a cgi script! */
	if (strlen(query) == 0)
		return;		/* no query string to parse! */

	/* varname=value&varname=value&varname=value ... */

	ptr = query;

	while (ptr) {
		varname = ptr;
		eq = strchr(varname, '=');
		if (!eq) {
			ptr = strchr(varname, '&');
			if (ptr)
				*ptr++ = '\0';

			cleanvar = unescape(varname);
			parsearg(cleanvar, "");
			free(cleanvar);

			continue;
		}

		*eq = '\0';
		value = eq + 1;
		amp = strchr(value, '&');
		if (amp) {
			ptr = amp + 1;
			*amp = '\0';
		}
		else
			ptr = NULL;

		cleanvar = unescape(varname);
		cleanval = unescape(value);
		parsearg(cleanvar, cleanval);
		free(cleanvar);
		free(cleanval);
	}
}

void extractpostargs(void)
{
	char	buf[SMALLBUF], *ptr, *cleanval;
	int	ch;

	ch = fgetc(stdin);
	buf[0] = '\0';

	while (ch != EOF) {
		if (ch == '&') {
			ptr = strchr(buf, '=');
			if (!ptr)
				parsearg(buf, "");
			else {
				*ptr++ = '\0';
				cleanval = unescape(ptr);
				parsearg(buf, cleanval);
				free(cleanval);
			}
			buf[0] = '\0';
		}
		else
			snprintfcat(buf, sizeof(buf), "%c", ch);

		ch = fgetc(stdin);
	}

	if (strlen(buf) != 0) {
		ptr = strchr(buf, '=');
		if (!ptr)
			parsearg(buf, "");
		else {
			*ptr++ = '\0';
			cleanval = unescape(ptr);
			parsearg(buf, cleanval);
			free(cleanval);
		}
	}
}

/* called for fatal errors in parseconf like malloc failures */
static void cgilib_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(ups.conf): %s", errmsg);
}

int checkhost(const char *host, char **desc)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	if (!host)
		return 0;		/* deny null hostnames */

	snprintf(fn, sizeof(fn), "%s/hosts.conf", confpath());

	pconf_init(&ctx, cgilib_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);
		fprintf(stderr, "%s\n", ctx.errmsg);

		return 0;	/* failed: deny access */
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			fprintf(stderr, "Error: %s:%d: %s\n",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		/* MONITOR <host> <description> */
		if (ctx.numargs < 3)
			continue;

		if (strcmp(ctx.arglist[0], "MONITOR") != 0)
			continue;

		if (!strcmp(ctx.arglist[1], host)) {
			if (desc)
				*desc = xstrdup(ctx.arglist[2]);

			pconf_finish(&ctx);
			return 1;	/* found: allow access */
		}
	}

	pconf_finish(&ctx);

	return 0;	/* not found: access denied */
}
