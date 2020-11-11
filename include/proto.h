/* proto.h - fill in the gaps about prototypes and definitions for portability

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
	2005	Arnaud Quette <arnaud.quette@free.fr>
	2006	Peter Selinger <selinger@users.sourceforge.net>
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

#ifndef NUT_PROTO_H_SEEN
#define NUT_PROTO_H_SEEN 1

#include "attribute.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)

/* Define this as a fall through, HAVE_STDARG_H is probably already set */

#ifndef HAVE_VARARGS_H
#define HAVE_VARARGS_H
#endif

/* varargs declarations: */

#if defined(HAVE_STDARG_H)
# include <stdarg.h>
# define HAVE_STDARGS    /* let's hope that works everywhere (mj) */
# define VA_LOCAL_DECL   va_list ap
# define VA_START(f)     va_start(ap, f)
# define VA_SHIFT(v,t)  ;   /* no-op for ANSI */
# define VA_END          va_end(ap)
#else
# if defined(HAVE_VARARGS_H)
#  include <varargs.h>
#  undef HAVE_STDARGS
#  define VA_LOCAL_DECL   va_list ap
#  define VA_START(f)     va_start(ap)      /* f is ignored! */
#  define VA_SHIFT(v,t) v = va_arg(ap,t)
#  define VA_END        va_end(ap)
# else
/*XX ** NO VARARGS ** XX*/
# endif
#endif

#if !defined (HAVE_SNPRINTF) || defined (__Lynx__)
int snprintf (char *str, size_t count, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 3, 4)));
#endif

#if !defined (HAVE_VSNPRINTF)
int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif

#endif

#ifndef HAVE_SETENV
int nut_setenv(const char *name, const char *value, int overwrite);
static inline int setenv(const char *name, const char *value, int overwrite) {
	return nut_setenv(name, value, overwrite);
}
#endif

#ifdef __hpux
#ifdef HAVE_SYS_MODEM_H
#include <sys/modem.h>
#endif
/* See sys/termio.h and sys/modem.h
   The following serial bits are not defined by HPUX.
   The numbers are octal like I found in BSD.
   TIOCM_ST is used in genericups.[ch] for the Powerware 3115.
   These defines make it compile, but I have no idea if it works.
 */
#define         TIOCM_LE        0001            /* line enable */
#define         TIOCM_ST        0010            /* secondary transmit */
#define         TIOCM_SR        0020            /* secondary receive */
#endif

#ifdef HAVE_GETPASSPHRASE
#define GETPASS getpassphrase
#else
#define GETPASS getpass
#endif

#ifdef __Lynx__
/* Missing prototypes on LynxOS */
int seteuid(uid_t);
int vprintf(const char *, va_list);
int putenv(char *);
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_PROTO_H_SEEN */
