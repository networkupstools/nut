/* parseconf.c - state machine-driven dynamic configuration file parser

   Copyright (C) 2002  Russell Kroll <rkroll@exploits.org>

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

/* parseconf, version 4.
 *
 * This one abandons the "callback" system introduced last time.  It
 * didn't turn out as well as I had hoped - you got stuck "behind"
 * parseconf too often.
 *
 * There is now a context buffer, and you call pconf_init to set it up.
 * All subsequent calls must have it as the first argument.  There are
 * two entry points for parsing lines.  You can have it read a file
 * (pconf_file_begin and pconf_file_next), take lines directly from
 * the caller (pconf_line), or go along a character at a time (pconf_char).
 * The parsing is identical no matter how you feed it.
 *
 * Since there are no more callbacks, you take the successful return
 * from the function and access ctx->arglist and ctx->numargs yourself.
 * You must check for errors with pconf_parse_error before using them,
 * since it might not be complete.  This lets the caller handle all
 * error reporting that's nonfatal.
 *
 * Fatal errors are those that involve memory allocation.  If the user
 * defines an error handler when calling pconf_init, that function will
 * be called with the error message before parseconf exits.  By default
 * it will just write the message to stderr before exiting.
 *
 * Input vs. Output:
 *
 * What it reads		--> What ends up in each argument
 *
 * this is a line 		--> "this" "is" "a" "line"
 * this "is also" a line	--> "this" "is also" "a" "line"
 * embedded\ space		--> "embedded space"
 * embedded\\backslash		--> "embedded\backslash"
 *
 * Arguments are split by whitespace (isspace()) unless that whitespace
 * occurs inside a "quoted pair like this".
 *
 * You can also escape the double quote (") character.  The backslash
 * also allows you to join lines, allowing you to have logical lines
 * that span physical lines, just like you can do in some shells.
 *
 * Lines normally end with a newline, but reaching EOF will also force
 * parsing on what's been scanned so far.
 *
 * Design:
 *
 * Characters are read one at a time to drive the state machine.
 * As words are completed (by hitting whitespace or ending a "" item),
 * they are committed to the next buffer in the arglist.  realloc is
 * used, so the buffer can grow to handle bigger words.
 *
 * The arglist also grows as necessary with a similar approach.  As a
 * result, you can parse extremely long words and lines with an insane
 * number of elements.
 *
 * Finally, there is argsize, which remembers how long each of the
 * arglist elements are.  This is how we know when to expand them.
 *
 */

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "parseconf.h"
#include "attribute.h"

/* possible states */

#define STATE_FINDWORDSTART	1
#define STATE_FINDEOL		2
#define STATE_QUOTECOLLECT	3
#define STATE_QC_LITERAL	4
#define STATE_COLLECT		5
#define STATE_COLLECTLITERAL	6
#define STATE_ENDOFLINE		7
#define STATE_PARSEERR		8

static void pconf_fatal(PCONF_CTX_t *ctx, const char *errtxt)
	__attribute__((noreturn));

static void pconf_fatal(PCONF_CTX_t *ctx, const char *errtxt)
{
	if (ctx->errhandler)
		ctx->errhandler(errtxt);
	else
		fprintf(stderr, "parseconf: fatal error: %s\n", errtxt);

	exit(EXIT_FAILURE);
}

static void add_arg_word(PCONF_CTX_t *ctx)
{
	size_t	argpos;
	size_t	wbuflen;

	/* this is where the new value goes */
	argpos = ctx->numargs;

	ctx->numargs++;

	/* when facing more args than ever before, expand the list */
	if (ctx->numargs > ctx->maxargs) {
		ctx->maxargs = ctx->numargs;

		/* resize the lists */
		ctx->arglist = realloc(ctx->arglist,
			sizeof(char *) * ctx->numargs);

		if (!ctx->arglist)
			pconf_fatal(ctx, "realloc arglist failed");

		ctx->argsize = realloc(ctx->argsize,
			sizeof(size_t) * ctx->numargs);

		if (!ctx->argsize)
			pconf_fatal(ctx, "realloc argsize failed");

		/* ensure sane starting values */
		ctx->arglist[argpos] = NULL;
		ctx->argsize[argpos] = 0;
	}

	wbuflen = strlen(ctx->wordbuf);

	/* now see if the string itself grew compared to last time */
	if (wbuflen >= ctx->argsize[argpos]) {
		size_t	newlen;

		/* allow for the trailing NULL */
		newlen = wbuflen + 1;

		/* expand the string storage */
		ctx->arglist[argpos] = realloc(ctx->arglist[argpos], newlen);

		if (!ctx->arglist[argpos])
			pconf_fatal(ctx, "realloc arglist member failed");

		/* remember the new size */
		ctx->argsize[argpos] = newlen;
	}

	/* strncpy doesn't give us a trailing NULL, so prep the space */
	memset(ctx->arglist[argpos], '\0', ctx->argsize[argpos]);

	/* finally copy the new value into the provided space */
	strncpy(ctx->arglist[argpos], ctx->wordbuf, wbuflen);
}

static void addchar(PCONF_CTX_t *ctx)
{
	size_t	wbuflen;

	wbuflen = strlen(ctx->wordbuf);

	/* CVE-2012-2944: only allow the subset of ASCII charset from Space to ~ */
	if ((ctx->ch < 0x20) || (ctx->ch > 0x7f)) {
		fprintf(stderr, "addchar: discarding invalid character (0x%02x)!\n",
				ctx->ch);
		return;
	}

	if (ctx->wordlen_limit != 0) {
		if (wbuflen >= ctx->wordlen_limit) {

			/* limit reached: don't append any more */
			return;
		}
	}

	/* allow for the null */
	if (wbuflen >= (ctx->wordbufsize - 1)) {
		ctx->wordbufsize += 8;

		ctx->wordbuf = realloc(ctx->wordbuf, ctx->wordbufsize);

		if (!ctx->wordbuf)
			pconf_fatal(ctx, "realloc wordbuf failed");

		/* repoint as wordbuf may have moved */
		ctx->wordptr = &ctx->wordbuf[wbuflen];
	}

	*ctx->wordptr++ = (char)ctx->ch;
	*ctx->wordptr = '\0';
}

static void endofword(PCONF_CTX_t *ctx)
{
	if (ctx->arg_limit != 0) {
		if (ctx->numargs >= ctx->arg_limit) {

			/* don't accept this word - just drop it */
			ctx->wordptr = ctx->wordbuf;
			*ctx->wordptr = '\0';

			return;
		}
	}

	add_arg_word(ctx);

	ctx->wordptr = ctx->wordbuf;
	*ctx->wordptr = '\0';
}

/* look for the beginning of a word */
static int findwordstart(PCONF_CTX_t *ctx)
{
	/* newline = the physical line is over, so the logical one is too */
	if (ctx->ch == 10)
		return STATE_ENDOFLINE;

	/* the rest of the line is a comment */
	if (ctx->ch == '#')
		return STATE_FINDEOL;

	/* space = not in a word yet, so loop back */
	if (isspace(ctx->ch))
		return STATE_FINDWORDSTART;

	/* \ = literal = accept the next char blindly */
	if (ctx->ch == '\\')
		return STATE_COLLECTLITERAL;

	/* " = begin word bounded by quotes */
	if (ctx->ch == '"')
		return STATE_QUOTECOLLECT;

	/* at this point the word just started */
	addchar(ctx);

	/* if the first character is a '=' this is considered a whole word */
	if (ctx->ch == '=') {
		endofword(ctx);
		return STATE_FINDWORDSTART;
	}

	return STATE_COLLECT;
}

/* eat characters until the end of the line is found */
static int findeol(PCONF_CTX_t *ctx)
{
	/* newline = found it, so start a new line */
	if (ctx->ch == 10)
		return STATE_ENDOFLINE;

	/* come back here */
	return STATE_FINDEOL;
}

/* set up the error reporting details */
static void pconf_seterr(PCONF_CTX_t *ctx, const char *errmsg)
{
	snprintf(ctx->errmsg, PCONF_ERR_LEN, "%s", errmsg);

	ctx->error = 1;
}

/* quote characters inside a word bounded by "quotes" */
static int quotecollect(PCONF_CTX_t *ctx)
{
	/* user is trying to break us */
	if (ctx->ch == '#') {
		pconf_seterr(ctx, "Unbalanced word due to unescaped # in quotes");
		endofword(ctx);

		/* this makes us drop all the way out of the caller */
		return STATE_PARSEERR;
	}

	/* another " means we're done with this word */
	if (ctx->ch == '"') {
		endofword(ctx);

		return STATE_FINDWORDSTART;
	}

	/* literal - special case since it needs to return here */
	if (ctx->ch == '\\')
		return STATE_QC_LITERAL;

	/* otherwise save it and loop back */
	addchar(ctx);

	return STATE_QUOTECOLLECT;
}

/* take almost anything literally, but return to quotecollect */
static int qc_literal(PCONF_CTX_t *ctx)
{
	/* continue onto the next line of the file */
	if (ctx->ch == 10)
		return STATE_QUOTECOLLECT;

	addchar(ctx);
	return STATE_QUOTECOLLECT;
}

/* collect characters inside a word */
static int collect(PCONF_CTX_t *ctx)
{
	/* comment means the word is done, and skip to the end of the line */
	if (ctx->ch == '#') {
		endofword(ctx);

		return STATE_FINDEOL;
	}

	/* newline means the word is done, and the line is done */
	if (ctx->ch == 10) {
		endofword(ctx);

		return STATE_ENDOFLINE;
	}

	/* space means the word is done */
	if (isspace(ctx->ch)) {
		endofword(ctx);

		return STATE_FINDWORDSTART;
	}

	/* '=' means the word is done and the = is a single char word*/
	if (ctx->ch == '=') {
		endofword(ctx);
		findwordstart(ctx);

		return STATE_FINDWORDSTART;
	}

	/* \ = literal = accept the next char blindly */
	if (ctx->ch == '\\')
		return STATE_COLLECTLITERAL;

	/* otherwise store it and come back for more */
	addchar(ctx);
	return STATE_COLLECT;
}

/* take almost anything literally */
static int collectliteral(PCONF_CTX_t *ctx)
{
	/* continue to the next line */
	if (ctx->ch == 10)
		return STATE_COLLECT;

	addchar(ctx);
	return STATE_COLLECT;
}

/* clean up memory before going back to the user */
static void free_storage(PCONF_CTX_t *ctx)
{
	unsigned int	i;

	free(ctx->wordbuf);

	/* clear out the individual words first */
	for (i = 0; i < ctx->maxargs; i++)
		free(ctx->arglist[i]);

	free(ctx->arglist);
	free(ctx->argsize);

	/* put things back to the initial state */
	ctx->arglist = NULL;
	ctx->argsize = NULL;
	ctx->numargs = 0;
	ctx->maxargs = 0;
}

int pconf_init(PCONF_CTX_t *ctx, void errhandler(const char *))
{
	/* set up the ctx elements */

	ctx->f = NULL;
	ctx->state = STATE_FINDWORDSTART;
	ctx->numargs = 0;
	ctx->maxargs = 0;
	ctx->arg_limit = PCONF_DEFAULT_ARG_LIMIT;
	ctx->wordlen_limit = PCONF_DEFAULT_WORDLEN_LIMIT;
	ctx->linenum = 0;
	ctx->error = 0;
	ctx->arglist = NULL;
	ctx->argsize = NULL;

	ctx->wordbufsize = 16;
	ctx->wordbuf = calloc(1, ctx->wordbufsize);

	if (!ctx->wordbuf)
		pconf_fatal(ctx, "malloc wordbuf failed");
	ctx->wordptr = ctx->wordbuf;

	ctx->errhandler = errhandler;
	ctx->magic = PCONF_CTX_t_MAGIC;

	return 1;
}

static int check_magic(PCONF_CTX_t *ctx)
{
	if (!ctx)
		return 0;

	if (ctx->magic != PCONF_CTX_t_MAGIC) {
		snprintf(ctx->errmsg, PCONF_ERR_LEN, "Invalid ctx buffer");
		return 0;
	}

	return 1;
}

int pconf_file_begin(PCONF_CTX_t *ctx, const char *fn)
{
	if (!check_magic(ctx))
		return 0;

	ctx->f = fopen(fn, "r");

	if (!ctx->f) {
		snprintf(ctx->errmsg, PCONF_ERR_LEN, "Can't open %s: %s",
			fn, strerror(errno));
		return 0;
	}

	/* prevent fd leaking to child processes */
	fcntl(fileno(ctx->f), F_SETFD, FD_CLOEXEC);

	return 1;	/* OK */
}

static void parse_char(PCONF_CTX_t *ctx)
{
	switch(ctx->state) {
		case STATE_FINDWORDSTART:
			ctx->state = findwordstart(ctx);
			break;

		case STATE_FINDEOL:
			ctx->state = findeol(ctx);
			break;

		case STATE_QUOTECOLLECT:
			ctx->state = quotecollect(ctx);
			break;

		case STATE_QC_LITERAL:
			ctx->state = qc_literal(ctx);
			break;

		case STATE_COLLECT:
			ctx->state = collect(ctx);
			break;

		case STATE_COLLECTLITERAL:
			ctx->state = collectliteral(ctx);
			break;
	}	/* switch */
}

/* return 1 if an error occurred, but only do it once */
int pconf_parse_error(PCONF_CTX_t *ctx)
{
	if (!check_magic(ctx))
		return 0;

	if (ctx->error == 1) {
		ctx->error = 0;
		return 1;
	}

	return 0;
}

/* clean up the ctx space */
void pconf_finish(PCONF_CTX_t *ctx)
{
	if (!check_magic(ctx))
		return;

	if (ctx->f)
		fclose(ctx->f);

	free_storage(ctx);

	ctx->magic = 0;
}

/* read from a file until a whole line is ready for use */
int pconf_file_next(PCONF_CTX_t *ctx)
{
	if (!check_magic(ctx))
		return 0;

	ctx->linenum++;

	/* start over for the new line */
	ctx->numargs = 0;
	ctx->state = STATE_FINDWORDSTART;

	while ((ctx->ch = fgetc(ctx->f)) != EOF) {
		parse_char(ctx);

		if (ctx->state == STATE_PARSEERR)
			return 1;

		if (ctx->state == STATE_ENDOFLINE)
			return 1;
	}

	/* deal with files that don't end in a newline */

	if (ctx->numargs != 0) {

		/* still building a word? */
		if (ctx->wordptr != ctx->wordbuf)
			endofword(ctx);

		return 1;
	}

	/* finished with nothing left over */
	return 0;
}

/* parse a provided line */
int pconf_line(PCONF_CTX_t *ctx, const char *line)
{
	size_t	i, linelen;

	if (!check_magic(ctx))
		return 0;

	ctx->linenum++;

	/* start over for the new line */
	ctx->numargs = 0;
	ctx->state = STATE_FINDWORDSTART;

	linelen = strlen(line);

	for (i = 0; i < linelen; i++) {
		ctx->ch = line[i];

		parse_char(ctx);

		if (ctx->state == STATE_PARSEERR)
			return 1;

		if (ctx->state == STATE_ENDOFLINE)
			return 1;
	}

	/* deal with any lingering characters */

	/* still building a word? */
	if (ctx->wordptr != ctx->wordbuf)
		endofword(ctx);		/* tie it off */

	return 1;
}

#define PCONF_ESCAPE "#\\\""

char *pconf_encode(const char *src, char *dest, size_t destsize)
{
	size_t	i, srclen, destlen, maxlen;

	if (destsize < 1)
		return dest;

	memset(dest, '\0', destsize);

	/* always leave room for a final NULL */
	maxlen = destsize - 1;
	srclen = strlen(src);
	destlen = 0;

	for (i = 0; i < srclen; i++) {
		if (strchr(PCONF_ESCAPE, src[i])) {

			/* if they both won't fit, we're done */
			if (destlen >= maxlen - 1)
				return dest;

			dest[destlen++] = '\\';
		}

		/* bail out when dest is full */
		if (destlen >= maxlen)
			return dest;

		dest[destlen++] = src[i];
	}

	return dest;
}

/* parse input a character at a time */
int pconf_char(PCONF_CTX_t *ctx, char ch)
{
	if (!check_magic(ctx))
		return -1;

	/* if the last call finished a line, clean stuff up for another */
	if ((ctx->state == STATE_ENDOFLINE) || (ctx->state == STATE_PARSEERR)) {
		ctx->numargs = 0;
		ctx->state = STATE_FINDWORDSTART;
	}

	ctx->ch = ch;
	parse_char(ctx);

	if (ctx->state == STATE_ENDOFLINE)
		return 1;

	if (ctx->state == STATE_PARSEERR)
		return -1;

	return 0;
}
