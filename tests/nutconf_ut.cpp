/*
NUT configuration unit test

Copyright (C)
2012	Vaclav Krpec <VaclavKrpec@Eaton.com>

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


#include "nutstream.hpp"
#include "nutconf.h"
#include "nutwriter.hpp"

#include <cppunit/extensions/HelperMacros.h>


/**
 *  \brief  NUT configuration unit test
 */
class NutConfigUnitTest: public CppUnit::TestFixture {
	private:

	CPPUNIT_TEST_SUITE(NutConfigUnitTest);
		CPPUNIT_TEST(test);

		CPPUNIT_TEST(testGenericConfiguration);
	CPPUNIT_TEST_SUITE_END();

	/**
	 *  \brief  Load configuration from file
	 *
	 *  \param  config     Configuration object
	 *  \param  file_name  Configuration file name
	 */
	void load(nut::Serialisable * config, const std::string & file_name);

	/**
	 *  \brief  Check configuration serialisation contents
	 *
	 *  \param  config   Configuration object
	 *  \param  content  Expected serialisation
	 */
	void check(const nut::Serialisable * config, const std::string & content);

	/** nut.conf test */
	void testNutConfiguration();

	/** upsmon.conf test */
	void testUpsmonConfiguration();

	/** upsd.conf test */
	void testUpsdConfiguration();

	/** ups.conf test */
	void testUpsConfiguration();

	/** upsd.users test */
	void testUpsdUsersConfiguration();

	public:

	inline void setUp() {}
	inline void tearDown() {}

	inline void test() {
		testNutConfiguration();
		testUpsmonConfiguration();
		testUpsdConfiguration();
		testUpsConfiguration();
		testUpsdUsersConfiguration();
	}

	/** Test generic configuration serialization. */
	void testGenericConfiguration();
	void compareGenericConfiguration(const nut::GenericConfiguration& expected, const nut::GenericConfiguration& tested);

};  // end of class NutConfigUnitTest


// Register the test suite
CPPUNIT_TEST_SUITE_REGISTRATION(NutConfigUnitTest);


void NutConfigUnitTest::load(nut::Serialisable * config, const std::string & file_name) {
	nut::NutFile file(file_name, nut::NutFile::READ_ONLY);

	CPPUNIT_ASSERT(config->parseFrom(file));
}


void NutConfigUnitTest::check(const nut::Serialisable * config, const std::string & content) {
	nut::NutMemory mem;

	CPPUNIT_ASSERT(config->writeTo(mem));

	std::string str;

	nut::NutStream::status_t status = mem.getString(str);

	CPPUNIT_ASSERT(nut::NutStream::NUTS_OK == status);

	if (content != str) {
		std::cerr << "--- expected ---" << std::endl << content << "--- end ---" << std::endl;
		std::cerr << "--- serialised ---" << std::endl << str << "--- end ---" << std::endl;

		CPPUNIT_ASSERT_MESSAGE("Configuration serialisation check failed", 0);
	}
}


void NutConfigUnitTest::testNutConfiguration() {
	nut::NutConfiguration config;

	load(static_cast<nut::Serialisable *>(&config), TOP_SRCDIR "/conf/nut.conf.sample");

	config.mode = nut::NutConfiguration::MODE_STANDALONE;

	check(static_cast<nut::Serialisable *>(&config),
		"MODE=standalone\n"
	);
}


void NutConfigUnitTest::testUpsmonConfiguration() {
	nut::UpsmonConfiguration config;

	load(static_cast<nut::Serialisable *>(&config), TOP_SRCDIR "/conf/upsmon.conf.sample");

	config.shutdownCmd   = "/sbin/shutdown -h +2 'System shutdown in 2 minutes!'";
	config.powerDownFlag = "/run/nut/killpower";
	config.poolFreqAlert = 10;
	config.deadTime      = 30;

	check(static_cast<nut::Serialisable *>(&config),
		"SHUTDOWNCMD \"/sbin/shutdown -h +2 'System shutdown in 2 minutes!'\"\n"
		"POWERDOWNFLAG /run/nut/killpower\n"
		"MINSUPPLIES 1\n"
		"POLLFREQ 5\n"
		"POLLFREQALERT 10\n"
		"HOSTSYNC 15\n"
		"DEADTIME 30\n"
		"RBWARNTIME 43200\n"
		"NOCOMMWARNTIME 300\n"
		"FINALDELAY 5\n"
	);
}


void NutConfigUnitTest::testUpsdConfiguration() {
	nut::UpsdConfiguration config;

	load(static_cast<nut::Serialisable *>(&config), TOP_SRCDIR "/conf/upsd.conf.sample");

	config.maxAge    = 15;
	config.statePath = "/var/run/nut";
	config.maxConn   = 1024;
	config.certFile  = "/usr/share/ssl-cert/ssleay.cnf";

	nut::UpsdConfiguration::Listen listen;

	listen.address = "127.0.0.1";
	listen.port    = 3493;

	config.listens.push_back(listen);

	listen.address = "::1";

	config.listens.push_back(listen);

	check(static_cast<nut::Serialisable *>(&config),
		"MAXAGE 15\n"
		"MAXCONN 1024\n"
		"STATEPATH /var/run/nut\n"
		"CERTFILE /usr/share/ssl-cert/ssleay.cnf\n"
		"LISTEN 127.0.0.1 3493\n"
		"LISTEN ::1 3493\n"
	);
}


void NutConfigUnitTest::testUpsConfiguration() {
	nut::UpsConfiguration config;

	load(static_cast<nut::Serialisable *>(&config), TOP_SRCDIR "/conf/ups.conf.sample");

	static const std::string my_ups("powerpal");

	config.setDriver(my_ups, "blazer_ser");
	config.setPort(my_ups, "/dev/ttyS0");
	config.setDescription(my_ups, "Web server");

	check(static_cast<nut::Serialisable *>(&config),
		"[powerpal]\n"
		"\tdesc = \"Web server\"\n"
		"\tdriver = blazer_ser\n"
		"\tport = /dev/ttyS0\n"
		"\n"
	);
}


void NutConfigUnitTest::testUpsdUsersConfiguration() {
	nut::UpsdUsersConfiguration config;

	load(static_cast<nut::Serialisable *>(&config), TOP_SRCDIR "/conf/upsd.users.sample");

	config.setPassword("upsmon", "ytrewq");
	config.setUpsmonMode(nut::UpsdUsersConfiguration::UPSMON_MASTER);

	config.setPassword("admin", "qwerty=ui");
	config.setActions("admin", nut::ConfigParamList(1, "SET"));
	config.setInstantCommands("admin", nut::ConfigParamList(1, "ALL"));

	check(static_cast<nut::Serialisable *>(&config),
		"[admin]\n"
		"\tactions = SET\n"
		"\tinstcmds = ALL\n"
		"\tpassword = \"qwerty=ui\"\n"
		"\n"
		"[upsmon]\n"
		"\tpassword = ytrewq\n"
		"\tupsmon master\n"
		"\n"
	);
}

void NutConfigUnitTest::testGenericConfiguration()
{
	nut::GenericConfiguration config;
	config["section01"]["value01"] = 1;
	config["section01"]["value02"] = "2";
	config["section02"]["val"] = 42;
	config["section02"]["mul"] << "01" << "02" << 3.4;
	config[""]["global"] = "The \"Global\" value.";

	nut::NutMemory memory;
	config.writeTo(memory);

	nut::GenericConfiguration test;
	test.parseFrom(memory);

	compareGenericConfiguration(config, test);
}

void NutConfigUnitTest::compareGenericConfiguration(const nut::GenericConfiguration& expected, const nut::GenericConfiguration& tested)
{
	// Assert they have the same section count.
	CPPUNIT_ASSERT_EQUAL_MESSAGE("GenericConfiguration must have same section count",
		expected.sections.size(), tested.sections.size());

	// Test each section.
	for(nut::GenericConfiguration::SectionMap::const_iterator it = expected.sections.begin(); it != expected.sections.end(); ++it)
	{
		// Section must be found in the other.
		CPPUNIT_ASSERT_MESSAGE("GenericConfiguration must have same sections",
			tested.hasSection(it->first) );

		const nut::GenericConfigSection& expectedSection = it->second;
		const nut::GenericConfigSection& testedSection = tested[it->first];

		// Section name must be sames.
		CPPUNIT_ASSERT_EQUAL_MESSAGE("GenericConfiguration section must have same name",
			expectedSection.name, testedSection.name);

		// Sections must have same entry count.
		CPPUNIT_ASSERT_EQUAL_MESSAGE("GenericConfiguration section must have same entry count",
			expectedSection.entries.size(), testedSection.entries.size());

		// Test each entry.
		for(nut::GenericConfigSection::EntryMap::const_iterator iter = expectedSection.entries.begin(); iter != expectedSection.entries.end(); ++iter)
		{
			// Entry must be found in the other.
			CPPUNIT_ASSERT_MESSAGE("GenericConfigSection must have same entries",
				testedSection.hasEntry(iter->first) );

			const nut::GenericConfigSectionEntry& expectedEntry = iter->second;
			const nut::GenericConfigSectionEntry& testedEntry = testedSection[iter->first];

			// Entry name must be sames.
			CPPUNIT_ASSERT_EQUAL_MESSAGE("GenericConfigSectionEntry must have same name",
				expectedEntry.name, testedEntry.name);

			// Entries must have same value count.
			CPPUNIT_ASSERT_EQUAL_MESSAGE("GenericConfigSectionEntry must have same value count",
				expectedEntry.values.size(), testedEntry.values.size());

			// Entries must have same values.
			for(size_t n=0; n<expectedEntry.values.size(); ++n)
			{
				CPPUNIT_ASSERT_EQUAL_MESSAGE("GenericConfigSectionEntry must have same values",
					expectedEntry.values[n], testedEntry.values[n]);
			}
		}
	}
}