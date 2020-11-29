# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_c___attribute__.html
#
#    Downloaded into the Network UPS Tools (NUT) codebase from
#    http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_c___attribute__.m4
#    as of 2020-11-20 and adapted for attribute supports we needed
# ===========================================================================
#
# SYNOPSIS
#
#   AX_C___ATTRIBUTE__
#
# DESCRIPTION
#
#   Provides a test for the compiler support of __attribute__ extensions.
#   Defines HAVE___ATTRIBUTE__ if it is found.
#   Also in particular defines
#       HAVE___ATTRIBUTE__UNUSED_ARG
#       HAVE___ATTRIBUTE__UNUSED_FUNC
#       HAVE___ATTRIBUTE__NORETURN
#   if support for respective values and use-cases of interest for NUT
#   codebase is found.
#
# LICENSE
#
#   Copyright (c) 2008 Stepan Kasal <skasal@redhat.com>
#   Copyright (c) 2008 Christian Haggstrom
#   Copyright (c) 2008 Ryan McCabe <ryan@numb.org>
#   Copyright (c) 2020 Jim Klimov <jimklimov+nut@gmail.com>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 2 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <https://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 9

AC_DEFUN([AX_C___ATTRIBUTE__], [
  AC_CACHE_CHECK([for __attribute__((unused)) for function arguments], [ax_cv___attribute__unused_arg],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
	[[#include <stdlib.h>
	  static void foo( int);
	  static void
	  foo(__attribute__ ((unused)) int i) {
	      return;
	  }
        ]], [func(1);])],
      [ax_cv___attribute__unused_arg=yes],
      [ax_cv___attribute__unused_arg=no]
    )
  ])

  AC_CACHE_CHECK([for __attribute__((unused)) for functions], [ax_cv___attribute__unused_func],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
	[[#include <stdlib.h>
	  static void foo(void) __attribute__ ((unused));
	  static void
	  foo(void) {
	      return;
	  }
        ]], [])],
      [ax_cv___attribute__unused_func=yes],
      [ax_cv___attribute__unused_func=no]
    )
  ])

  AC_CACHE_CHECK([for __attribute__((noreturn))], [ax_cv___attribute__noreturn],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
	[[#include <stdlib.h>
	  static void foo(void) __attribute__ ((noreturn));
	  static void
	  foo(void) {
	      exit(1);
	  }
        ]], [foo();])],
      [ax_cv___attribute__noreturn=yes],
      [ax_cv___attribute__noreturn=no]
    )
  ])

  AC_CACHE_CHECK([for at least some __attribute__ support], [ax_cv___attribute__],
   [if test "$ax_cv___attribute__unused_arg" = "yes" \
    || test "$ax_cv___attribute__unused_func" = "yes" \
    || test "$ax_cv___attribute__noreturn" = "yes" \
    ; then
      dnl # Some values did not error, support for keyword itself exists
      ax_cv___attribute__=yes
    else
      dnl # At least none of the options we are interested in work...
      ax_cv___attribute__=no
    fi
   ])

  if test "$ax_cv___attribute__unused_arg" = "yes"; then
    AC_DEFINE([HAVE___ATTRIBUTE__UNUSED_ARG], 1, [define if your compiler has __attribute__((unused)) for function arguments])
  fi

  if test "$ax_cv___attribute__unused_func" = "yes"; then
    AC_DEFINE([HAVE___ATTRIBUTE__UNUSED_FUNC], 1, [define if your compiler has __attribute__((unused)) for functions])
  fi
  if test "$ax_cv___attribute__noreturn" = "yes"; then
    AC_DEFINE([HAVE___ATTRIBUTE__NORETURN], 1, [define if your compiler has __attribute__((noreturn))])
  fi

  if test "$ax_cv___attribute__" = "yes"; then
    AC_DEFINE([HAVE___ATTRIBUTE__], 1, [define if your compiler has __attribute__])
  fi
])
