/* example - CppUnit unit test example

   Copyright (C)
	2012	Emilien Kia <emilienkia-guest@alioth.debian.org>

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


