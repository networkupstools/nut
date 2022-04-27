/* cgilib.h - headers for cgilib.c

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

#ifndef NUT_CGILIB_H_SEEN
#define NUT_CGILIB_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* other programs that link to this should provide parsearg() ... */
void parsearg(char *var, char *value);

/* actually extract the values from QUERY_STRING */
void extractcgiargs(void);

/* like extractcgiargs, but this one is for POSTed values */
void extractpostargs(void);

/* see if a host is allowed per the hosts.conf */
int checkhost(const char *host, char **desc);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_CGILIB_H_SEEN */
