/* cppunit-warnings-end.h - restore diagnostics after CppUnit tests

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

#ifndef NUT_CPPUNIT_WARNINGS_END_H_SEEN
#define NUT_CPPUNIT_WARNINGS_END_H_SEEN 1

#ifdef NUT_CPPUNIT_WARNINGS_CLANG_PUSHED
# pragma clang diagnostic pop
# undef NUT_CPPUNIT_WARNINGS_CLANG_PUSHED
#endif

#ifdef NUT_CPPUNIT_WARNINGS_GCC_PUSHED
# pragma GCC diagnostic pop
# undef NUT_CPPUNIT_WARNINGS_GCC_PUSHED
#endif

#undef NUT_CPPUNIT_WARNINGS_ZERO_AS_NULL_POINTER_CONSTANT

#endif /* NUT_CPPUNIT_WARNINGS_END_H_SEEN */
