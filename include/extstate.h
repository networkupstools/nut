/* extstate.h - external state structures used by things like upsd

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
	2005	Arnaud Quette <arnaud.quette@free.fr>
	2007	Peter Selinger <selinger@users.sourceforge.net>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2013	Emilien Kia <kiae.dev@gmail.com>
	2020	Jim Klimov <jimklimov@gmail.com>

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

#ifndef NUT_EXTSTATE_H_SEEN
#define NUT_EXTSTATE_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* this could be made dynamic if someone really needs more than this... */
#define ST_MAX_VALUE_LEN 256

/* state tree flags */
#define ST_FLAG_NONE      0x0000
#define ST_FLAG_RW        0x0001
#define ST_FLAG_STRING    0x0002 /* not STRING implies NUMBER */
#define ST_FLAG_NUMBER    0x0004
#define ST_FLAG_IMMUTABLE 0x0008

/* list of possible ENUM values */
typedef struct enum_s {
	char	*val;
	struct enum_s	*next;
} enum_t;

/* RANGE boundaries */
typedef struct range_s {
	int min;
	int max;
	struct range_s	*next;
} range_t;

/* list of instant commands */
typedef struct cmdlist_s {
	char	*name;
	struct cmdlist_s	*next;
} cmdlist_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_EXTSTATE_H_SEEN */
