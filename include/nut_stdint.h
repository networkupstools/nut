/*
 * nut_stdint.h - Network UPS Tools sets of integer types having specified widths 
 * 
 * Copyright (C) 2011	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef NUT_STDINT_H_SEEN
#define NUT_STDINT_H_SEEN 1

/* "config.h" is generated by autotools and lacks a header guard, so
 * we use an unambiguously named macro we know we must have, as one.
 * It must be the first header: be sure to know all about system config.
 */
#ifndef NUT_NETVERSION
# include "config.h"
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif

#if defined HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

#if defined HAVE_STDINT_H
#  include <stdint.h>
#endif

#if defined HAVE_LIMITS_H
#  include <limits.h>
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)(-1LL))
#endif

#ifndef SSIZE_MAX
#define SSIZE_MAX ((ssize_t)(-1LL))
#endif

/* Printing format for size_t and ssize_t */
#ifndef PRIuSIZE
# ifdef PRIsize
#  define PRIuSIZE PRIsize
# else
#  if defined(__MINGW32__) || defined (WIN32)
#   define PRIuSIZE "llu"
#  else
#   define PRIuSIZE "zu"
#  endif
# endif
#endif

#ifndef PRIxSIZE
#  if defined(__MINGW32__) || defined (WIN32)
#   define PRIxSIZE "llx"
#  else
#   define PRIxSIZE "zx"
#  endif
#endif

/* Note: Windows headers are known to define at least "d" values,
 * so macros below revolve around that and not "i" directly */
#ifndef PRIiSIZE
# ifdef PRIssize
#  define PRIiSIZE PRIssize
# else
#  ifdef PRIdSIZE
#   define PRIiSIZE PRIdSIZE
#  else
#   if defined(__MINGW32__) || defined (WIN32)
#    define PRIiSIZE "lld"
#   else
#    define PRIiSIZE "zd"
#   endif
#   define PRIdSIZE PRIiSIZE
#  endif
# endif
#else
# ifndef PRIdSIZE
#  define PRIdSIZE PRIiSIZE
# endif
#endif /* format for size_t and ssize_t */

/* Printing format for uintmax_t and intmax_t */
#ifndef PRIuMAX
# if defined(__MINGW32__) || defined (WIN32)
#  if (SIZEOF_VOID_P == 8)
#   ifdef PRIu64
#    define PRIuMAX PRIu64
#   else
/* assume new enough compiler and standard, and no Windows %I64 etc... check "%ll" support via configure? */
#    define PRIuMAX "llu"
#   endif
#  endif
#  if (SIZEOF_VOID_P == 4)
#   ifdef PRIu32
#    define PRIuMAX PRIu32
#   else
/* assume new enough compiler and standard, and no Windows %I64 etc... check "%ll" support via configure? */
#    define PRIuMAX "lu"
#   endif
#  endif
# else
/* assume new enough compiler and standard... check "%j" support via configure? */
#  define PRIuMAX "ju"
# endif
#endif /* format for uintmax_t and intmax_t */

#ifndef PRIdMAX
# ifdef PRIiMAX
#  define PRIdMAX PRIiMAX
# else
#  if defined(__MINGW32__) || defined (WIN32)
#   if (SIZEOF_VOID_P == 8)
#    ifdef PRId64
#     define PRIdMAX PRId64
#    else
/* assume new enough compiler and standard, and no Windows %I64 etc... check "%ll" support via configure? */
#     define PRIdMAX "lld"
#    endif
#   endif
#   if (SIZEOF_VOID_P == 4)
#    ifdef PRId32
#     define PRIdMAX PRId32
#    else
/* assume new enough compiler and standard, and no Windows %I64 etc... check "%ll" support via configure? */
#     define PRIdMAX "ld"
#    endif
#   endif
#  else
/* assume new enough compiler and standard... check "%j" support via configure? */
#   define PRIdMAX "jd"
#  endif
#  define PRIiMAX PRIdMAX
# endif
#else
# ifndef PRIiMAX
#  define PRIiMAX PRIdMAX
# endif
#endif /* format for uintmax_t and intmax_t */

#ifndef PRIxMAX
# if defined(__MINGW32__) || defined (WIN32)
#  if (SIZEOF_VOID_P == 8)
#   ifdef PRIx64
#    define PRIxMAX PRIx64
#   else
/* assume new enough compiler and standard, and no Windows %I64 etc... check "%ll" support via configure? */
#    define PRIxMAX "llx"
#   endif
#  endif
#  if (SIZEOF_VOID_P == 4)
#   ifdef PRIx32
#    define PRIxMAX PRIx32
#   else
/* assume new enough compiler and standard, and no Windows %I64 etc... check "%ll" support via configure? */
#    define PRIxMAX "lx"
#   endif
#  endif
# else
/* assume new enough compiler and standard... check "%j" support via configure? */
#  define PRIxMAX "jx"
# endif
#endif /* format for uintmax_t and intmax_t */

#endif	/* NUT_STDINT_H_SEEN */
