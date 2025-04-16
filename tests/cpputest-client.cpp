/* cpputest-client - CppUnit libnutclient active test

   Module for NUT `cpputest` runner, to check client-server interactions
   as part of NIT (NUT Integration Testing) suite and similar scenarios.
   This is an "active" NUT client talking to an `upsd` on $NUT_PORT,
   as opposed to nutclienttest.cpp which unit-tests the class API etc.
   in isolated-binary fashion.

   Copyright (C)
	2022-2025	Jim Klimov <jimklimov+nut@gmail.com>

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

#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#include <stdlib.h>

/* Current CPPUnit offends the honor of C++98 and maybe later versions */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC)
#pragma GCC diagnostic push
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
#endif
#if (defined __clang__) && (defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS)
# ifdef HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push "-Wdeprecated-declarations"
# endif
#endif

#include <cppunit/extensions/HelperMacros.h>

namespace nut {

class NutActiveClientTest : public CppUnit::TestFixture
{
	/* Note: is "friend" of nut::TcpClient class
	 * to test a few of its protected methods */

	CPPUNIT_TEST_SUITE( NutActiveClientTest );
		CPPUNIT_TEST( test_query_ver );
		CPPUNIT_TEST( test_list_ups );
		CPPUNIT_TEST( test_list_ups_clients );
		CPPUNIT_TEST( test_auth_user );
		CPPUNIT_TEST( test_auth_primary );
	CPPUNIT_TEST_SUITE_END();

private:
	/* Fed by caller via envvars: */
	uint16_t NUT_PORT = 0;
	std::string NUT_USER = "";
	std::string NUT_PASS = "";
	std::string NUT_PRIMARY_DEVICE = "";
	std::string NUT_SETVAR_DEVICE = "";

public:
	void setUp() override;
	void tearDown() override;

	void test_query_ver();
	void test_list_ups();
	void test_list_ups_clients();
	void test_auth_user();
	void test_auth_primary();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( NutActiveClientTest );

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

void NutActiveClientTest::setUp()
{
	/* NUT_PORT etc. are provided by external test suite driver */
	char * s;

	s = std::getenv("NUT_PORT");
	if (s) {
		long l = atol(s);
		if (l < 1 || l > 65535) {
			throw std::runtime_error("NUT_PORT specified by caller is out of range");
		}
		NUT_PORT = static_cast<uint16_t>(l);
	} else {
		throw std::runtime_error("NUT_PORT not specified by caller, NIT should call this test");
	}

	s = std::getenv("NUT_USER");
	if (s) {
		NUT_USER = s;
	} // else stays empty

	s = std::getenv("NUT_PASS");
	if (s) {
		NUT_PASS = s;
	} // else stays empty

	s = std::getenv("NUT_PRIMARY_DEVICE");
	if (s) {
		NUT_PRIMARY_DEVICE = s;
	} // else stays empty

	s = std::getenv("NUT_SETVAR_DEVICE");
	if (s) {
		NUT_SETVAR_DEVICE = s;
	} // else stays empty
}

void NutActiveClientTest::tearDown()
{
}

void NutActiveClientTest::test_query_ver() {
	nut::TcpClient c("localhost", NUT_PORT);
	std::string s;

	std::cerr << "[D] C++ NUT Client lib test running against Data Server at: "
		<< c.getHost() << ':' << c.getPort() << std::endl;

	CPPUNIT_ASSERT_MESSAGE(
		"TcpClient is not connected after constructor",
		c.isConnected());

	/* Note: generic client code can not use protected methods
	 * like low-level sendQuery(), list(), get() and some more,
	 * but this NutActiveClientTest is a friend of TcpClient:
	 */
	s = c.sendQuery("VER");
	std::cerr << "[D] Got Data Server VER: " << s << std::endl;

	try {
		s = c.sendQuery("PROTVER");
		std::cerr << "[D] Got PROTVER: " << s << std::endl;
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Did not get PROTVER: " << ex.what() << std::endl;
	}

	try {
		s = c.sendQuery("NETVER");
		std::cerr << "[D] Got NETVER (obsolete): " << s << std::endl;
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Did not get NETVER (obsolete): " << ex.what() << std::endl;
	}

	try {
		c.logout();
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Could not get LOGOUT: " << ex.what() << std::endl;
	}

	try {
		c.disconnect();
	}
	catch(nut::NutException& ex)
	{
		/* NUT_UNUSED_VARIABLE(ex); */
		std::cerr << "[D] Could not get disconnect(): " << ex.what() << std::endl;
	}
}

void NutActiveClientTest::test_list_ups() {
	nut::TcpClient c("localhost", NUT_PORT);
	std::set<std::string> devs;
	bool noException = true;

	try {
		devs = c.getDeviceNames();
		std::cerr << "[D] Got device list (" << devs.size() << "): [";
		for (std::set<std::string>::iterator it = devs.begin();
			it != devs.end(); it++
		) {
			if (it != devs.begin()) {
				std::cerr << ", ";
			}
			std::cerr << '"' << *it << '"';
		}
		std::cerr << "]" << std::endl;
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Could not device list: " << ex.what() << std::endl;
		noException = false;
	}

	c.logout();
	c.disconnect();

	CPPUNIT_ASSERT_MESSAGE(
		"Failed to list UPS with TcpClient: threw NutException",
		noException);
}

void NutActiveClientTest::test_list_ups_clients() {
	nut::TcpClient c("localhost", NUT_PORT);
	std::map<std::string, std::set<std::string>> deviceClients;
	bool noException = true;

	try {
		c.authenticate(NUT_USER, NUT_PASS);
		std::cerr << "[D] Authenticated without exceptions" << std::endl;
		/* Note: no high hopes here, credentials are checked by server
		 * when running critical commands, not at auth request itself */
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Could not authenticate as a simple user: " << ex.what() << std::endl;
		/* no failure here */
	}

	try {
		c.deviceLogin(NUT_PRIMARY_DEVICE);
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Could not log into primary device (envvars not set?) so test below should return empty client lists: " << ex.what() << std::endl;
		/* no failure here */
	}

	try {
		deviceClients = c.listDeviceClients();
		std::cerr << "[D] Got device client list (" << deviceClients.size() << "): [";
		for (std::map<std::string, std::set<std::string>>::iterator itM = deviceClients.begin();
			itM != deviceClients.end(); itM++
		) {
			if (itM != deviceClients.begin()) {
				std::cerr << "," << std::endl;
			}
			std::cerr << "{ \"" << itM->first << "\" : ";
			for (std::set<std::string>::iterator it = itM->second.begin();
				it != itM->second.end(); it++
			) {
				if (it != itM->second.begin()) {
					std::cerr << ", ";
				}
				std::cerr << '"' << *it << '"';
			}
			std::cerr << " }";
		}
		std::cerr << " ]" << std::endl;
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Could not device client list: " << ex.what() << std::endl;
		noException = false;
	}

	c.logout();
	c.disconnect();

	CPPUNIT_ASSERT_MESSAGE(
		"Failed to list UPS with TcpClient: threw NutException",
		noException);
}

void NutActiveClientTest::test_auth_user() {
	if (NUT_USER.empty()) {
		std::cerr << "[D] SKIPPING test_auth_user()" << std::endl;
		return;
	}

	nut::TcpClient c("localhost", NUT_PORT);
	bool noException = true;
	try {
		c.authenticate(NUT_USER, NUT_PASS);
		std::cerr << "[D] Authenticated without exceptions" << std::endl;
		/* Note: no high hopes here, credentials are checked by server
		 * when running critical commands, not at auth request itself */
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Could not authenticate as a simple user: " << ex.what() << std::endl;
		noException = false;
	}

	if (!NUT_SETVAR_DEVICE.empty()) {
		try {
			TrackingResult tres;
			TrackingID tid;
			int i;
			std::string nutVar = "ups.status"; /* Has a risk of flip-flop with NIT dummy setup */
			std::string s1 = c.getDeviceVariableValue(NUT_SETVAR_DEVICE, nutVar)[0];
			std::string sTest = s1 + "-test";

			std::cerr << "[D] Got initial device '" << NUT_SETVAR_DEVICE
				<< "' variable '" << nutVar << "' value: " << s1 << std::endl;
			CPPUNIT_ASSERT_MESSAGE(
				"Did not expect empty value here",
				!s1.empty());

			tid = c.setDeviceVariable(NUT_SETVAR_DEVICE, nutVar, sTest);
			while ( (tres = c.getTrackingResult(tid)) == PENDING) {
				usleep(100);
			}
			if (tres != SUCCESS) {
				std::cerr << "[D] Failed to set device variable: "
					<< "tracking result is " << tres << std::endl;
				noException = false;
			}
			/* Check what we got after set */
			/* Note that above we told the server to tell the driver
			 * to set a dstate entry; below we ask the server to ask
			 * the driver and relay the answer to us. The dummy-ups
			 * driver may also be in a sleeping state between cycles.
			 * Data propagation may be not instantaneous, so we loop
			 * for a while to see the expected value (or give up).
			 */
			std::string s2;
			for (i = 0; i < 100 ; i++) {
				s2 = c.getDeviceVariableValue(NUT_SETVAR_DEVICE, nutVar)[0];
				if (s2 == sTest)
					break;
				usleep(100000);
			}
			std::cerr << "[D] Read back: " << s2
				<< " after " << (100 * i) << "msec"
				<< std::endl;

			/* Fix it back */
			tid = c.setDeviceVariable(NUT_SETVAR_DEVICE, nutVar, s1);
			while ( (tres = c.getTrackingResult(tid)) == PENDING) {
				usleep(100);
			}
			if (tres != SUCCESS) {
				std::cerr << "[D] Failed to set device variable: "
					<< "tracking result is " << tres << std::endl;
				noException = false;
			}
			std::string s3;
			for (i = 0; i < 100 ; i++) {
				s3 = c.getDeviceVariableValue(NUT_SETVAR_DEVICE, nutVar)[0];
				if (s3 == s1)
					break;
				usleep(100000);
			}
			std::cerr << "[D] Read back: " << s3
				<< " after " << (100 * i) << "msec"
				<< std::endl;

			if (s3 != s1) {
				std::cerr << "[D] Final device variable value '" << s3
					<< "' differs from original '" << s1
					<< "'" << std::endl;
				noException = false;
			}

			if (s2 == s1) {
				std::cerr << "[D] Tweaked device variable value '" << s2
					<< "' does not differ from original '" << s1
					<< "'" << std::endl;
				noException = false;
			}

			if (noException) {
				std::cerr << "[D] Tweaked device variable value OK" << std::endl;
			}
		}
		catch(nut::NutException& ex)
		{
			std::cerr << "[D] Failed to set device variable: "
				<< ex.what() << std::endl;
			noException = false;
		}
	} else {
		std::cerr << "[D] SKIPPING test_auth_user() active test "
			<< "(got no NUT_SETVAR_DEVICE to poke)" << std::endl;
	}

	c.logout();
	c.disconnect();

	CPPUNIT_ASSERT_MESSAGE(
		"Failed to auth as user with TcpClient or tweak device variable",
		noException);
}

void NutActiveClientTest::test_auth_primary() {
	if (NUT_USER.empty() || NUT_PRIMARY_DEVICE.empty()) {
		std::cerr << "[D] SKIPPING test_auth_primary()" << std::endl;
		return;
	}

	nut::TcpClient c("localhost", NUT_PORT);
	bool noException = true;
	try {
		c.authenticate(NUT_USER, NUT_PASS);
		std::cerr << "[D] Authenticated without exceptions" << std::endl;
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] Could not authenticate as an upsmon primary user: "
			<< ex.what() << std::endl;
		noException = false;
	}

	try {
		Device d = c.getDevice(NUT_PRIMARY_DEVICE);
		bool gotPrimary = false;
		bool gotMaster = false;

		try {
			c.deviceMaster(NUT_PRIMARY_DEVICE);
			gotMaster = true;
			std::cerr << "[D] Elevated as MASTER without exceptions" << std::endl;
		}
		catch(nut::NutException& ex)
		{
			std::cerr << "[D] Could not elevate as MASTER for "
				<< "NUT_PRIMARY_DEVICE " << NUT_PRIMARY_DEVICE << ": "
				<< ex.what() << std::endl;
		}

		try {
			c.devicePrimary(NUT_PRIMARY_DEVICE);
			gotPrimary = true;
			std::cerr << "[D] Elevated as PRIMARY without exceptions" << std::endl;
		}
		catch(nut::NutException& ex)
		{
			std::cerr << "[D] Could not elevate as PRIMARY for "
				<< "NUT_PRIMARY_DEVICE " << NUT_PRIMARY_DEVICE << ": "
				<< ex.what() << std::endl;
		}

		if (!gotMaster && !gotPrimary)
			noException = false;
	}
	catch(nut::NutException& ex)
	{
		std::cerr << "[D] NUT_PRIMARY_DEVICE " << NUT_PRIMARY_DEVICE
			<< " not found on Data Server: "
			<< ex.what() << std::endl;
		noException = false;
	}

	c.logout();
	c.disconnect();

	CPPUNIT_ASSERT_MESSAGE(
		"Failed to auth as user with TcpClient: threw NutException",
		noException);
}

} // namespace nut {}

#if (defined __clang__) && (defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS)
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC)
# pragma GCC diagnostic pop
#endif
