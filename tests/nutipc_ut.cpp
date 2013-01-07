/*
NUT IPC unit test

Copyright (C) 2012

\author Vaclav Krpec <VaclavKrpec@Eaton.com>

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


#include "nutipc.hpp"

#include <cppunit/extensions/HelperMacros.h>


/**
 *  \brief  NUT IPC module unit test
 */
class NutIPCUnitTest: public CppUnit::TestFixture {
	private:

	CPPUNIT_TEST_SUITE(NutIPCUnitTest);
		CPPUNIT_TEST(test);
	CPPUNIT_TEST_SUITE_END();

	/** External command execution test */
	void testExec();

	public:

	inline void setUp() {}
	inline void tearDown() {}

	inline void test() {
		testExec();
	}

};  // end of class NutIPCUnitTest


// Register the test suite
CPPUNIT_TEST_SUITE_REGISTRATION(NutIPCUnitTest);


void NutIPCUnitTest::testExec() {
	static const std::string bin = "/bin/sh";

	static nut::Process::Executor::Arguments bin_args;

	bin_args.push_back("-c");
	bin_args.push_back("exit 123");

	nut::Process::Execution child(bin, bin_args);

	CPPUNIT_ASSERT(123 == child.wait());

	CPPUNIT_ASSERT(0 == nut::Process::execute("test 'Hello world' == 'Hello world'"));
}
