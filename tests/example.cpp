/* example - CppUnit unit test example

   Copyright (C)
	2012	Emilien Kia <emilienkia-guest@alioth.debian.org>
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

#include "common.h"

/* Current CPPUnit offends the honor of C++98 */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic push
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS
#  pragma GCC diagnostic ignored "-Wglobal-constructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS
#  pragma GCC diagnostic ignored "-Wexit-time-destructors"
# endif
#endif

#include <cppunit/extensions/HelperMacros.h>

class ExampleTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE( ExampleTest );
    CPPUNIT_TEST( testOne );
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void testOne();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( ExampleTest );


void ExampleTest::setUp()
{
}


void ExampleTest::tearDown()
{
}


void ExampleTest::testOne()
{
  // Set up
  int i = 1;
  float f = 1.0;

  // Process
  int cast = static_cast<int>(f);

  // Check
  CPPUNIT_ASSERT_EQUAL( i, cast );
}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic pop
#endif
