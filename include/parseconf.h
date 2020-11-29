/* parseconf.h - state machine-driven dynamic configuration file parser

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

#ifndef PARSECONF_H_SEEN
#define PARSECONF_H_SEEN 1

#include <stdio.h>

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define PCONF_CTX_t_MAGIC 0x00726630
#define PCONF_ERR_LEN 256

/* conservative defaults: use at most 16 KB for parsing any given line */
#define PCONF_DEFAULT_ARG_LIMIT 32
#define PCONF_DEFAULT_WORDLEN_LIMIT 512

typedef struct {
	FILE	*f;			/* stream to current file	*/
	int	state;			/* current parser state		*/
	int	ch;			/* last character read		*/

	char	**arglist;		/* array of pointers to words	*/
	size_t	*argsize;		/* list of sizes for realloc	*/
	size_t	numargs;		/* max usable in arglist	*/
	size_t	maxargs;		/* for reallocing arglist	*/

	char	*wordbuf;		/* accumulator for current word	*/
	char	*wordptr;		/* where next char goes in word	*/
	size_t	wordbufsize;		/* for reallocing wordbuf	*/

	int	linenum;		/* for good error reporting	*/
	int	error;			/* set when an error occurred	*/
	char	errmsg[PCONF_ERR_LEN];	/* local buffer for errors 	*/

	void	(*errhandler)(const char *);	/* user's error handler */

	int	magic;			/* buffer validity check	*/

	/* these may be redefined by the caller to customize memory use	*/
	size_t	arg_limit;		/* halts growth of arglist	*/
	size_t	wordlen_limit;		/* halts growth of any wordbuf	*/

}	PCONF_CTX_t;

int pconf_init(PCONF_CTX_t *ctx, void errhandler(const char *));
int pconf_file_begin(PCONF_CTX_t *ctx, const char *fn);
int pconf_file_next(PCONF_CTX_t *ctx);
int pconf_parse_error(PCONF_CTX_t *ctx);
int pconf_line(PCONF_CTX_t *ctx, const char *line);
void pconf_finish(PCONF_CTX_t *ctx);
char *pconf_encode(const char *src, char *dest, size_t destsize);
int pconf_char(PCONF_CTX_t *ctx, char ch);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* PARSECONF_H_SEEN */
