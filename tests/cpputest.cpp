/* cpputest - basic runner for unit tests

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

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include "common.h"

int main(int argc, char* argv[])
{
  NUT_UNUSED_VARIABLE(argc);
  NUT_UNUSED_VARIABLE(argv);

  /* Get the top level suite from the registry */
  std::cerr << "D: Getting test suite..." << std::endl;
  CppUnit::Test *suite = CppUnit::TestFactoryRegistry::getRegistry().makeTest();

  /* Adds the test to the list of test to run */
  std::cerr << "D: Preparing test runner..." << std::endl;
  CppUnit::TextUi::TestRunner runner;
  runner.addTest( suite );

  /* Change the default outputter to a compiler error format outputter */
  std::cerr << "D: Setting test runner outputter..." << std::endl;
  runner.setOutputter( new CppUnit::CompilerOutputter( &runner.result(),
                                                       std::cerr ) );

  /* Run the tests. */
  bool wasSucessful = false;
  try {
    std::cerr << "D: Launching the test run..." << std::endl;
    wasSucessful = runner.run();
  }
  catch ( std::invalid_argument &e )  // Test path not resolved
  {
    std::cerr  << std::endl
               << "ERROR: " <<  e.what()
               << std::endl;
    wasSucessful = false;
  }

  /* Return error code 1 if the one of test failed. */
  std::cerr << "D: Got to the end of test suite with code " <<
    "'" << ( wasSucessful ? "true" : "false" ) << "'" << std::endl;
  return wasSucessful ? 0 : 1;
}
