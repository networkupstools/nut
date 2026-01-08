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

#include "common.h"

#include <ctype.h>
#include <stdio.h>

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
			long l;
			if (i + 2 > buflen)
				fatalx(EXIT_FAILURE, "string too short for escaped char");
			hex[0] = buf[++i];
			hex[1] = buf[++i];
			hex[2] = '\0';
			if (!isxdigit((unsigned char) hex[0])
				|| !isxdigit((unsigned char) hex[1]))
				fatalx(EXIT_FAILURE, "bad escape char");
			l = strtol(hex, NULL, 16);
			assert(l>=0);
			assert(l<=255);
			ch = (char)l;	/* FIXME: Loophole about non-ASCII symbols in top 128 values, or negatives for signed char... */

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
	char	buf[SMALLBUF], *ptr, *cleanval, *server_software = NULL;
	int	ch, content_length = -1, bytes_seen = 0;
	size_t	buflen;

	/* First, see if there's anything waiting...
	 * the server may not close STDIN properly
	 * or somehow delay opening/populating it. */
#ifndef WIN32
	int	selret;
	fd_set	fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 250000; /* wait for up to 250ms for a POST query to come */

	selret = select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
	if (selret <= 0) {
#else
	HANDLE	hSTDIN = GetStdHandle(STD_INPUT_HANDLE);
	DWORD	selret = WaitForSingleObject(hSTDIN, 250);
	if (selret != WAIT_OBJECT_0) { /* or == WAIT_TIMEOUT ? */
#endif	/* WIN32 */
		upsdebug_with_errno(1, "%s: no stdin is waiting (%" PRIiMAX ")<br/>", __func__, (intmax_t)selret);
		return;
	}

	buf[0] = '\0';

	/* Does the web server tell us how much it sent
	 * (and might keep the channel open... indefinitely)? */
	ptr = getenv("CONTENT_LENGTH");
	if (ptr) {
		content_length = atoi(ptr);
	}

	ptr = getenv("SERVER_SOFTWARE");
	if (ptr) {
		server_software = ptr;
	} else {
		server_software = "";
	}

	if (content_length > 0 && strstr(server_software, "IIS")) {
		/* Our POSTs end with a newline, and that one never arrives
		 * (reads hang), possibly buffered output from IIS?
		 * Our own setmode() in e.g. upsset.c does not help.
		 * So upsset.c ends each FORM with do_hidden_sentinel()
		 * to sacrifice a few bytes we would not use.
		 */
		upsdebugx(3, "%s: truncating expected content length on IIS<br/>", __func__);
		content_length--;
	}
	upsdebugx(3, "%s: starting to read %d POSTed bytes on server '%s'<br/>", __func__, content_length, server_software);

	ch = fgetc(stdin);
	upsdebugx(6, "%s: got char: '%c' (%d, 0x%02X)<br/>", __func__, ch, ch, (unsigned int)ch);

	if (ch == EOF) {
		bytes_seen++;
		upsdebugx(3, "%s: got immediate EOF in stdin<br/>", __func__);
	} else while(1) {
		bytes_seen++;
		if (ch == '&' || ch == EOF || (content_length >= 0 && bytes_seen >= content_length)) {
			buflen = strlen(buf);
			upsdebugx(1, "%s: collected a chunk of %" PRIuSIZE " bytes on stdin: %s<br/>",
				__func__, buflen, buf);
			ptr = strchr(buf, '=');
			if (!ptr) {
				upsdebugx(3, "%s: parsearg('%s', '')<br/>", __func__, buf);
				parsearg(buf, "");
			} else {
				*ptr++ = '\0';
				cleanval = unescape(ptr);
				upsdebugx(3, "%s: parsearg('%s', '%s')<br/>", __func__, buf, cleanval);
				parsearg(buf, cleanval);
				free(cleanval);
			}
			buf[0] = '\0';

			if (ch == EOF || (content_length >= 0 && bytes_seen >= content_length))
				break;	/* end the loop */
		}
		else
			snprintfcat(buf, sizeof(buf), "%c", ch);

#ifndef WIN32
		/* Must re-init every time when looping (array is changed by select method) */
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 250000; /* wait for up to 250ms  for a POST response */

		selret = select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
		if (selret <= 0) {
#else
		selret = WaitForSingleObject(hSTDIN, 250);
		if (selret != WAIT_OBJECT_0) { /* or == WAIT_TIMEOUT ? */
#endif
			/* We do not always get EOF, so assume the input stream stopped */
			upsdebug_with_errno(1, "%s: timed out waiting for an stdin byte (%" PRIiMAX ")<br/>", __func__, (intmax_t)selret);
			break;
		}

		fflush(stderr);
		ch = fgetc(stdin);
		upsdebugx(6, "%s: got char: '%c' (%d, 0x%02X)<br/>", __func__, ch, ch, (unsigned int)ch);
		if (ch == EOF)
			upsdebugx(3, "%s: got proper stdin EOF<br/>", __func__);
		upsdebugx(6, "%s: processed %d bytes with %d expected incoming content length on server '%s'<br/>", __func__, bytes_seen, content_length, server_software);
	}	/* end of infinite loop */

	upsdebugx(3, "%s: processed %d bytes with %d incoming content length<br/>", __func__, bytes_seen, content_length);
}

/* called for fatal errors in parseconf like malloc failures */
static void cgilib_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(ups.conf): %s", errmsg);
}

int checkhost(const char *host, char **desc)
{
	char	fn[NUT_PATH_MAX + 1];
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
