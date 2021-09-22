/* nutclienttest - CppUnit nutclient unit test

   Copyright (C) 2016  Emilien Kia <emilien.kia@gmail.com>
   Copyright (C) 2020  Jim Klimov <jimklimov@gmail.com>

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

namespace nut {

class NutClientTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE( NutClientTest );
		CPPUNIT_TEST( test_stringset_to_strarr );
		CPPUNIT_TEST( test_stringvector_to_strarr );

		CPPUNIT_TEST( test_copy_constructor_dev );
		CPPUNIT_TEST( test_copy_assignment_dev );

		CPPUNIT_TEST( test_copy_constructor_cmd );
		CPPUNIT_TEST( test_copy_assignment_cmd );

		CPPUNIT_TEST( test_copy_constructor_var );
		CPPUNIT_TEST( test_copy_assignment_var );

		CPPUNIT_TEST( test_nutclientstub_dev );
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp();
	void tearDown();

	void test_stringset_to_strarr();
	void test_stringvector_to_strarr();

	void test_copy_constructor_dev();
	void test_copy_assignment_dev();

	void test_copy_constructor_cmd();
	void test_copy_assignment_cmd();

	void test_copy_constructor_var();
	void test_copy_assignment_var();

	void test_nutclientstub_dev();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( NutClientTest );

} // namespace nut {}

#ifndef _NUTCLIENTTEST_BUILD
# define _NUTCLIENTTEST_BUILD 1
#endif

#include "../clients/nutclient.h"
#include "../clients/nutclientmem.h"

namespace nut {

extern "C" {
strarr stringset_to_strarr(const std::set<std::string>& strset);
strarr stringvector_to_strarr(const std::vector<std::string>& strset);
} // extern "C"

void NutClientTest::setUp()
{
}

void NutClientTest::tearDown()
{
}

void NutClientTest::test_stringset_to_strarr()
{
	std::set<std::string> strset;
	strset.insert("test");
	strset.insert("hello");
	strset.insert("world");

	strarr arr = stringset_to_strarr(strset);
	CPPUNIT_ASSERT_MESSAGE("stringset_to_strarr(...) result is null", arr != nullptr);

	std::set<std::string> res;

	char** ptr = arr;
	while(*ptr != nullptr)
	{
		res.insert(std::string(*ptr));
		ptr++;
	}

	CPPUNIT_ASSERT_EQUAL_MESSAGE("stringset_to_strarr(...) result has not 3 items", static_cast<size_t>(3), res.size());
	CPPUNIT_ASSERT_MESSAGE("stringset_to_strarr(...) result has not item \"test\"", res.find("test")!=res.end());
	CPPUNIT_ASSERT_MESSAGE("stringset_to_strarr(...) result has not item \"hello\"", res.find("hello")!=res.end());
	CPPUNIT_ASSERT_MESSAGE("stringset_to_strarr(...) result has not item \"world\"", res.find("world")!=res.end());

	strarr_free(arr);
}

void NutClientTest::test_stringvector_to_strarr()
{
	std::vector<std::string> strset;
	strset.push_back("test");
	strset.push_back("hello");
	strset.push_back("world");

	strarr arr = stringvector_to_strarr(strset);
	CPPUNIT_ASSERT_MESSAGE("stringvector_to_strarr(...) result is null", arr != nullptr);

	char** ptr = arr;
	CPPUNIT_ASSERT_EQUAL_MESSAGE("stringvector_to_strarr(...) result has not item 0==\"test\"", std::string("test"), std::string(*ptr));
	++ptr;
	CPPUNIT_ASSERT_EQUAL_MESSAGE("stringvector_to_strarr(...) result has not item 1==\"hello\"", std::string("hello"), std::string(*ptr));
	++ptr;
	CPPUNIT_ASSERT_EQUAL_MESSAGE("stringvector_to_strarr(...) result has not item 2==\"world\"", std::string("world"), std::string(*ptr));
	++ptr;

	/* https://stackoverflow.com/a/12565009/4715872
	 * Can not compare nullptr_t and another data type (char*)
	 * with CPPUNIT template assertEquals()
	 */
	CPPUNIT_ASSERT_MESSAGE("stringvector_to_strarr(...) result has not only 3 items", nullptr == *ptr);

	strarr_free(arr);
}

void NutClientTest::test_copy_constructor_dev() {
	nut::TcpClient c;
	nut::Device i(&c, "ups1");
	nut::Device j(i);

	CPPUNIT_ASSERT_EQUAL_MESSAGE("Failed to assign value of Device variable j by initializing from i", i, j);
}

void NutClientTest::test_copy_assignment_dev() {
	nut::TcpClient c;
	nut::Device i(&c, "ups1");
	nut::Device j(nullptr, "ups2");

	CPPUNIT_ASSERT_ASSERTION_FAIL_MESSAGE("Device variables i and j were initialized differently but claim to be equal",
		CPPUNIT_ASSERT_EQUAL(i, j) );

	j = i;
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Failed to assign value of Device Command j by equating to i", i, j);
}

void NutClientTest::test_copy_constructor_cmd() {
	nut::TcpClient c;
	nut::Device d(nullptr, "ups1");

	nut::Command i(&d, "cmd1");
	nut::Command j(i);

	CPPUNIT_ASSERT_EQUAL_MESSAGE("Failed to assign value of Command variable j by initializing from i", i, j);
}

void NutClientTest::test_copy_assignment_cmd() {
	nut::TcpClient c;
	nut::Device d(nullptr, "ups1");

	nut::Command i(&d, "var1");
	nut::Command j(nullptr, "var2");

	CPPUNIT_ASSERT_ASSERTION_FAIL_MESSAGE("Command variables i and j were initialized differently but claim to be equal",
		CPPUNIT_ASSERT_EQUAL(i, j) );

	j = i;
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Failed to assign value of Command variable j by equating to i", i, j);
}

void NutClientTest::test_copy_constructor_var() {
	nut::TcpClient c;
	nut::Device d(nullptr, "ups1");

	nut::Variable i(&d, "var1");
	nut::Variable j(i);

	CPPUNIT_ASSERT_EQUAL_MESSAGE("Failed to assign value of Variable variable j by initializing from i", i, j);
}

void NutClientTest::test_copy_assignment_var() {
	nut::TcpClient c;
	nut::Device d(nullptr, "ups1");

	nut::Variable i(&d, "var1");
	nut::Variable j(nullptr, "var2");

	CPPUNIT_ASSERT_ASSERTION_FAIL_MESSAGE("Variable variables i and j were initialized differently but claim to be equal",
		CPPUNIT_ASSERT_EQUAL(i, j) );

	j = i;
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Failed to assign value of Variable variable j by equating to i", i, j);
}

void NutClientTest::test_nutclientstub_dev() {
	bool noException = true;

	nut::MemClientStub c;
	nut::Device d(nullptr, "ups_1");
	try
	{
		// set mono value
		c.setDeviceVariable("ups_1", "name_1", "value_1");
		// get mono value
		ListValue values = c.getDeviceVariableValue("ups_1", "name_1");
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: mono wrong values number", values.size() == 1);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: mono bad value", values[0] == std::string("value_1"));
		// set multi value
		ListValue values_multi = { "multi_1", "multi_2" };
		c.setDeviceVariable("ups_1", "name_multi_1", values_multi);
		// get multi value
		values = c.getDeviceVariableValue("ups_1", "name_multi_1");
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: multi wrong values number", values.size() == 2);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: multi first bad value", values[0] == std::string("multi_1"));
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: multi second bad value", values[1] == std::string("multi_2"));
		// get object values
		ListObject objects = c.getDeviceVariableValues("ups_1");
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: objects wrong values number", objects.size() == 2);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: objects mono wrong values number", objects["name_1"].size() == 1);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: objects mono bad value", objects["name_1"][0] == std::string("value_1"));
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: objects multi wrong values number", objects["name_multi_1"].size() == 2);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: objects mono bad value", objects["name_multi_1"][0] == std::string("multi_1"));
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: objects mono bad value", objects["name_multi_1"][1] == std::string("multi_2"));
		// get device values
		std::set<std::string> devices_name = { "ups_1" };
		ListDevice devices = c.getDevicesVariableValues(devices_name);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: devices wrong values number", devices.size() == 1);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: devices mono wrong values number", devices["ups_1"]["name_1"].size() == 1);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: devices mono bad value", devices["ups_1"]["name_1"][0] == std::string("value_1"));
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: devices multi wrong values number", devices["ups_1"]["name_multi_1"].size() == 2);
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: devices mono bad value", devices["ups_1"]["name_multi_1"][0] == std::string("multi_1"));
		CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: devices mono bad value", devices["ups_1"]["name_multi_1"][1] == std::string("multi_2"));
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw exception", noException);

	// List of functions not implemented (should return exception)
	noException = true;
	try {
		std::set<std::string> cmd = c.getDeviceCommandNames("ups-1");
		CPPUNIT_ASSERT_MESSAGE("Variable not use", cmd.size() == 0);
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		std::string desc = c.getDeviceCommandDescription("ups-1", "cmd-1");
		CPPUNIT_ASSERT_MESSAGE("Variable not use", desc.empty());
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		TrackingID id = c.executeDeviceCommand("ups-1", "cmd-1", "param-1");
		CPPUNIT_ASSERT_MESSAGE("Variable not use", id.empty());
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		c.deviceLogin("ups-1");
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		c.deviceMaster("ups-1");
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		c.deviceForcedShutdown("ups-1");
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		c.deviceGetNumLogins("ups-1");
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		TrackingResult result = c.getTrackingResult("track-1");
		CPPUNIT_ASSERT_MESSAGE("Variable not use", result == TrackingResult::SUCCESS);
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		bool status = c.isFeatureEnabled(Feature("feature-1"));
		CPPUNIT_ASSERT_MESSAGE("Variable not use", !status);
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);

	noException = true;
	try {
		c.setFeature(Feature("feature-1"), true);
	}
	catch(nut::NutException& ex)
	{
		NUT_UNUSED_VARIABLE(ex);
		noException = false;
	}
	CPPUNIT_ASSERT_MESSAGE("Failed stub tcp client: throw no exception", !noException);
}

} // namespace nut {}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic pop
#endif
