/* timehead.h - from the autoconf docs: sanely include the right time headers everywhere

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
	2005	Arnaud Quette <arnaud.quette@free.fr>
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

#ifndef NUT_TIMEHEAD_H_SEEN
#define NUT_TIMEHEAD_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifndef HAVE_STRPTIME
/* Use fallback implementation provided in e.g. libcommon(client).la: */
char * strptime(const char *buf, const char *fmt, struct tm *tm);
#endif

#ifndef HAVE_LOCALTIME_R
# ifdef HAVE_LOCALTIME_S
/* A bit of a silly trick, but should help on MSYS2 builds it seems */
#  define localtime_r(timer, buf) localtime_s(timer, buf)
# else
#  include <string.h> /* memcpy */
static inline struct tm *localtime_r( const time_t *timer, struct tm *buf ) {
	/* Note: not thread-safe per se! */
	struct tm *tmp = localtime (timer);
	memcpy(buf, tmp, sizeof(struct tm));
	return buf;
}
# endif
#endif

#ifndef HAVE_GMTIME_R
# ifdef HAVE_GMTIME_S
#  define gmtime_r(timer, buf) gmtime_s(timer, buf)
# else
#  include <string.h> /* memcpy */
static inline struct tm *gmtime_r( const time_t *timer, struct tm *buf ) {
        /* Note: not thread-safe per se! */
        struct tm *tmp = gmtime (timer);
        memcpy(buf, tmp, sizeof(struct tm));
        return buf;
}
# endif
#endif

#ifndef HAVE_TIMEGM
# ifdef HAVE__MKGMTIME
#  define timegm(tm) _mkgmtime(tm)
# else
# error "No fallback implementation for timegm"
# endif
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_TIMEHEAD_H_SEEN */
