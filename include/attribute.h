/* attribute.h - portability hacks for __attribute__ usage in other header files

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

#ifndef NUT_ATTRIBUTE_H_SEEN
#define NUT_ATTRIBUTE_H_SEEN 1

/* To complicate matters, compilers with native support
 * for the keyword may expose or not expose it as a macro...
 * but for those that perform such courtesy, or are known
 * supporters, we can put up the flag. For others, someone
 * with those compilers should check and file PRs to NUT.
 */
#if (!defined HAVE___ATTRIBUTE__) || (HAVE___ATTRIBUTE__ == 0)
# if ( defined(__GNUC__) && ( __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8) ) ) || ( defined(__STRICT_ANSI__) && __STRICT_ANSI__ )
#  ifndef __attribute__
#   define __attribute__(x)
#  endif
#  ifndef HAVE___ATTRIBUTE__
#   define HAVE___ATTRIBUTE__ 0
#  endif
# else
#  if defined(__clang__) || defined(__GNUC__) || defined(__SUNPRO_C)
#   ifndef HAVE___ATTRIBUTE__
#    define HAVE___ATTRIBUTE__ 1
#   endif
#  else
#   ifndef HAVE___ATTRIBUTE__
#    define HAVE___ATTRIBUTE__ 0
#   endif
#  endif
# endif
#endif

#if (!defined HAVE___ATTRIBUTE__) || (HAVE___ATTRIBUTE__ == 0)
# ifdef HAVE___ATTRIBUTE__UNUSED_ARG
#  undef HAVE___ATTRIBUTE__UNUSED_ARG
# endif
# ifdef HAVE___ATTRIBUTE__UNUSED_FUNC
#  undef HAVE___ATTRIBUTE__UNUSED_FUNC
# endif
# ifdef HAVE___ATTRIBUTE__NORETURN
#  undef HAVE___ATTRIBUTE__NORETURN
# endif
# ifdef HAVE___ATTRIBUTE__
#  undef HAVE___ATTRIBUTE__
# endif
#endif

/* Other source files now can simply check for `ifdef HAVE___ATTRIBUTE__*`
 * as usual, and not bother about 0/1 values of the macro as well.
 */

#endif /* NUT_ATTRIBUTE_H_SEEN */
