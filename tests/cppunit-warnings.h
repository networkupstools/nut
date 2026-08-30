/* cppunit-warnings.h - quiesce warnings caused by CppUnit tests

   Copyright (C) 2020-2026 Jim Klimov <jimklimov+nut@gmail.com>

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

#ifndef NUT_CPPUNIT_WARNINGS_H_SEEN
#define NUT_CPPUNIT_WARNINGS_H_SEEN 1

/* Current CppUnit headers and macros raise warnings in supported C++ modes. */
#if defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP \
 && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC \
  || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC \
  || (defined NUT_CPPUNIT_WARNINGS_ZERO_AS_NULL_POINTER_CONSTANT \
   && defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ZERO_AS_NULL_POINTER_CONSTANT_BESIDEFUNC))
# define NUT_CPPUNIT_WARNINGS_GCC_PUSHED 1
# pragma GCC diagnostic push
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS
#  pragma GCC diagnostic ignored "-Wglobal-constructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS
#  pragma GCC diagnostic ignored "-Wexit-time-destructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wsuggest-override"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wsuggest-destructor-override"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wweak-vtables"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wextra-semi"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wold-style-cast"
# endif
# if defined NUT_CPPUNIT_WARNINGS_ZERO_AS_NULL_POINTER_CONSTANT \
 && defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ZERO_AS_NULL_POINTER_CONSTANT_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
# endif
#endif /* GCC diagnostic guards */

#if defined __clang__ \
 && defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_PUSH_POP \
 && defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
# define NUT_CPPUNIT_WARNINGS_CLANG_PUSHED 1
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#endif /* NUT_CPPUNIT_WARNINGS_H_SEEN */
