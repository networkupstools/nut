/*
    tests/nutconf.cpp - based on CppUnit unit test example

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

#include "config.h"

#include <cppunit/extensions/HelperMacros.h>

// Define to de-activate protection of parsing tool members:
#define UNITEST_MODE 1

#include "nutconf.hpp"
using namespace nut;

#include <string>
#include <algorithm>
using namespace std;

class NutConfTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE( NutConfTest );
    CPPUNIT_TEST( testOptions );
    CPPUNIT_TEST( testParseCHARS );
    CPPUNIT_TEST( testParseSTRCHARS );
    CPPUNIT_TEST( testPasreToken );
    CPPUNIT_TEST( testPasreTokenWithoutColon );
	CPPUNIT_TEST( testGenericConfigParser );
	CPPUNIT_TEST( testUpsmonConfigParser );
	CPPUNIT_TEST( testNutConfConfigParser );
	CPPUNIT_TEST( testUpsdConfigParser );
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;
  void tearDown() override;

  void testOptions();
  void testParseCHARS();
  void testParseSTRCHARS();
  void testPasreToken();
  void testPasreTokenWithoutColon();

  void testGenericConfigParser();
  void testUpsmonConfigParser();
  void testNutConfConfigParser();
  void testUpsdConfigParser();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( NutConfTest );


void NutConfTest::setUp()
{
}


void NutConfTest::tearDown()
{
}

void NutConfTest::testOptions()
{
    {
        NutParser parse("Bonjour monde!", NutParser::OPTION_DEFAULT);
		CPPUNIT_ASSERT_EQUAL_MESSAGE("Has parsing options", 0u, parse.getOptions());
		CPPUNIT_ASSERT_MESSAGE("Has OPTION_IGNORE_COLON parsing option", !parse.hasOptions(NutParser::OPTION_IGNORE_COLON));
    }

    {
        NutParser parse("Bonjour monde!", NutParser::OPTION_IGNORE_COLON);
		CPPUNIT_ASSERT_EQUAL_MESSAGE("Has bad parsing options", static_cast<unsigned int>(NutParser::OPTION_IGNORE_COLON), parse.getOptions());
		CPPUNIT_ASSERT_MESSAGE("Has not OPTION_IGNORE_COLON parsing option", parse.hasOptions(NutParser::OPTION_IGNORE_COLON));
    }

}

void NutConfTest::testParseCHARS()
{
    {
        NutParser parse("Bonjour monde!");
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find first string 'Bonjour'", string("Bonjour"), parse.parseCHARS());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot get a character ''", ' ', parse.get());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find second string 'monde!'", string("monde!"), parse.parseCHARS());
    }

    {
        NutParser parse("To\\ to");
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find escaped string 'To to'", string("To to"), parse.parseCHARS());
    }

    {
        NutParser parse("To\"to");
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find escaped string 'To'", string("To"), parse.parseCHARS());
    }

    {
        NutParser parse("To\\\"to");
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find escaped string 'To\"to'", string("To\"to"), parse.parseCHARS());
    }

}


void NutConfTest::testParseSTRCHARS()
{
    {
        NutParser parse("Bonjour\"monde!\"");
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find first string 'Bonjour'", string("Bonjour"), parse.parseSTRCHARS());
        parse.get();
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find second string 'monde!'", string("monde!"), parse.parseSTRCHARS());
    }

    {
        NutParser parse("To to");
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find spaced string 'To tue de l’appareil qui se serait malencontreuo'", string("To to"), parse.parseSTRCHARS());
    }

    {
        NutParser parse("To\\\"to");
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find quoted-escaped string 'To\"to'", string("To\"to"), parse.parseSTRCHARS());
    }
}

void NutConfTest::testPasreToken()
{
    static const char* src =
        "Bonjour monde\n"
        "[ceci]# Plouf\n"
        "\n"
        "titi = \"tata toto\"\n"
		"NOTIFYFLAG LOWBATT SYSLOG+WALL\n"
		"::1"
		;
    NutParser parse(src);

//    NutConfigParser::Token tok = parse.parseToken();
//    std::cout << "token = " << tok.type << " - " << tok.str << std::endl;

    CPPUNIT_ASSERT_MESSAGE("Cannot find 1st token 'Bonjour'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "Bonjour"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 2nd token 'monde'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "monde"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 3th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 4rd token '['", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_BRACKET_OPEN, "["));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 5th token 'ceci'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "ceci"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 6th token ']'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_BRACKET_CLOSE, "]"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 7th token ' Plouf'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_COMMENT, " Plouf"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 8th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 9th token 'titi'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "titi"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 10th token '='", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EQUAL, "="));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 11th token 'tata toto'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_QUOTED_STRING, "tata toto"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 12th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 13th token 'NOTIFYFLAG'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "NOTIFYFLAG"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 14th token 'LOWBATT'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "LOWBATT"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 15th token 'SYSLOG+WALL'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "SYSLOG+WALL"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 16th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 17th token ':'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_COLON, ":"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 18th token ':'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_COLON, ":"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 19th token '1'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "1"));

}

void NutConfTest::testPasreTokenWithoutColon()
{
    static const char* src =
        "Bonjour monde\n"
        "[ceci]# Plouf\n"
        "\n"
        "titi = \"tata toto\"\n"
		"NOTIFYFLAG LOWBATT SYSLOG+WALL\n"
		"::1"
		;
    NutParser parse(src, NutParser::OPTION_IGNORE_COLON);

//    NutConfigParser::Token tok = parse.parseToken();
//    std::cout << "token = " << tok.type << " - " << tok.str << std::endl;

    CPPUNIT_ASSERT_MESSAGE("Cannot find 1st token 'Bonjour'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "Bonjour"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 2nd token 'monde'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "monde"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 3th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 4rd token '['", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_BRACKET_OPEN, "["));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 5th token 'ceci'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "ceci"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 6th token ']'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_BRACKET_CLOSE, "]"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 7th token ' Plouf'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_COMMENT, " Plouf"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 8th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 9th token 'titi'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "titi"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 10th token '='", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EQUAL, "="));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 11th token 'tata toto'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_QUOTED_STRING, "tata toto"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 12th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 13th token 'NOTIFYFLAG'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "NOTIFYFLAG"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 14th token 'LOWBATT'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "LOWBATT"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 15th token 'SYSLOG+WALL'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "SYSLOG+WALL"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 16th token '\n'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_EOL, "\n"));
    CPPUNIT_ASSERT_MESSAGE("Cannot find 17th token '::1'", parse.parseToken() == NutParser::Token(NutParser::Token::TOKEN_STRING, "::1"));

}

void NutConfTest::testGenericConfigParser()
{
	static const char* src =
		"glovar1 = toto\n"
		"glovar2 = \"truc bidule\"\n"
		"\n"
		"[section1] # One section\n"
		"var1 = \"one value\"\n"
		" \n"
		"var2\n"
		"\n"
		"[section2]\n"
		"var1 = other value\n"
		"var toto";

	GenericConfiguration conf;
	conf.parseFromString(src);

	CPPUNIT_ASSERT_MESSAGE("Cannot find a global section", conf.sections.find("") != conf.sections.end() );
	CPPUNIT_ASSERT_MESSAGE("Cannot find global section's glovar1 variable", conf.sections[""]["glovar1"].values.front() == "toto" );
	CPPUNIT_ASSERT_MESSAGE("Cannot find global section's glovar2 variable", conf.sections[""]["glovar2"].values.front() == "truc bidule" );

	CPPUNIT_ASSERT_MESSAGE("Cannot find section1", conf.sections.find("section1") != conf.sections.end() );
	CPPUNIT_ASSERT_MESSAGE("Cannot find section1's var1 variable", conf.sections["section1"]["var1"].values.front() == "one value" );
	CPPUNIT_ASSERT_MESSAGE("Cannot find section1's var2 variable", conf.sections["section1"]["var2"].values.size() == 0 );

	CPPUNIT_ASSERT_MESSAGE("Cannot find section2", conf.sections.find("section2") != conf.sections.end() );
	CPPUNIT_ASSERT_MESSAGE("Cannot find section2's var1 variable", conf.sections["section2"]["var1"].values.front() == "other" );
	CPPUNIT_ASSERT_MESSAGE("Cannot find section2's var1 variable", *(++(conf.sections["section2"]["var1"].values.begin())) == "value" );
	CPPUNIT_ASSERT_MESSAGE("Cannot find section2's var variable", conf.sections["section2"]["var"].values.front() == "toto" );

}

void NutConfTest::testUpsmonConfigParser()
{
	static const char* src =
		"RUN_AS_USER nutmon\n"
		"MONITOR myups@bigserver 1 monmaster blah master\n"
		"MONITOR su700@server.example.com 1 upsmon secretpass slave\n"
		"MONITOR myups@localhost 1 upsmon pass master\n"
		"MINSUPPLIES 1\n"
		"\n"
		"# MINSUPPLIES 25\n"
		"SHUTDOWNCMD \"/sbin/shutdown -h +0\"\n"
		"NOTIFYCMD /usr/local/ups/bin/notifyme\n"
		"POLLFREQ 30\n"
		"POLLFREQALERT 5\n"
		"HOSTSYNC 15\n"
		"DEADTIME 15\n"
		"POWERDOWNFLAG /etc/killpower\n"
		"NOTIFYMSG ONLINE \"UPS %s on line power\"\n"
		"NOTIFYFLAG LOWBATT SYSLOG+WALL\n"
		"RBWARNTIME 43200\n"
		"NOCOMMWARNTIME 300\n"
		"FINALDELAY 5"
		;

	UpsmonConfiguration conf;
	conf.parseFromString(src);

	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find RUN_AS_USER 'nutmon'", string("nutmon"), *conf.runAsUser);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find MINSUPPLIES 1", 1u, *conf.minSupplies);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find SHUTDOWNCMD '/sbin/shutdown -h +0'", string("/sbin/shutdown -h +0"), *conf.shutdownCmd);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find NOTIFYCMD '/usr/local/ups/bin/notifyme'", string("/usr/local/ups/bin/notifyme"), *conf.notifyCmd);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find POWERDOWNFLAG '/etc/killpower'", string("/etc/killpower"), *conf.powerDownFlag);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find POLLFREQ 30", 30u, *conf.poolFreq);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find POLLFREQALERT 5", 5u, *conf.poolFreqAlert);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find HOSTSYNC 15", 15u, *conf.hostSync);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find DEADTIME 15", 15u, *conf.deadTime);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find RBWARNTIME 43200", 43200u, *conf.rbWarnTime);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find NOCOMMWARNTIME 300", 300u, *conf.noCommWarnTime);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find FINALDELAY 5", 5u, *conf.finalDelay);

	CPPUNIT_ASSERT_MESSAGE("Find a NOTIFYFLAG ONLINE", !conf.notifyFlags[nut::UpsmonConfiguration::NOTIFY_ONLINE].set());
	CPPUNIT_ASSERT_MESSAGE("Cannot find a NOTIFYFLAG LOWBATT", conf.notifyFlags[nut::UpsmonConfiguration::NOTIFY_LOWBATT].set());
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find a NOTIFYFLAG LOWBATT SYSLOG+WALL", 3u, static_cast<unsigned int>(conf.notifyFlags[nut::UpsmonConfiguration::NOTIFY_LOWBATT]));


	CPPUNIT_ASSERT_MESSAGE("Find a NOTIFYMSG LOWBATT", !conf.notifyMessages[nut::UpsmonConfiguration::NOTIFY_LOWBATT].set());
	CPPUNIT_ASSERT_MESSAGE("Cannot find a NOTIFYMSG ONLINE", conf.notifyMessages[nut::UpsmonConfiguration::NOTIFY_ONLINE].set());
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find a NOTIFYMSG ONLINE \"UPS %s on line power\"", string("UPS %s on line power"), *conf.notifyMessages[nut::UpsmonConfiguration::NOTIFY_ONLINE]);
}


void NutConfTest::testNutConfConfigParser()
{
	static const char* src =
		"\n\nMODE=standalone\n";

	NutConfiguration conf;
	conf.parseFromString(src);

	CPPUNIT_ASSERT_MESSAGE("Cannot find a MODE", conf.mode.set());
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find a MODE=standalone", nut::NutConfiguration::MODE_STANDALONE, *conf.mode);
}

void NutConfTest::testUpsdConfigParser()
{
	static const char* src =
		"MAXAGE 15\n"
		"STATEPATH /var/run/nut\n"
		"LISTEN 127.0.0.1 3493\n"
		"LISTEN ::1 3493\n"
		"MAXCONN 1024\n"
		"CERTFILE /home/toto/cert.file"
		;

	UpsdConfiguration conf;
	conf.parseFromString(src);

	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find MAXAGE 15", 15u, *conf.maxAge);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find MAXCONN 1024", 1024u, *conf.maxConn);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find STATEPATH /var/run/nut", string("/var/run/nut"), *conf.statePath);
	CPPUNIT_ASSERT_EQUAL_MESSAGE("Cannot find CERTFILE /home/toto/cert.file", string("/home/toto/cert.file"), *conf.certFile);

	// Find Listen 127.0.0.1 3493
	{
		typedef std::list<UpsdConfiguration::Listen> ListenList;
		UpsdConfiguration::Listen listen = {"127.0.0.1", 3493};
		ListenList::const_iterator it = find(conf.listens.begin(), conf.listens.end(), listen);
		CPPUNIT_ASSERT_MESSAGE("LISTEN 127.0.0.1 3493", it != conf.listens.end());
	}

	// Find Listen ::1 3493
	{
		typedef std::list<UpsdConfiguration::Listen> ListenList;
		UpsdConfiguration::Listen listen = {"::1", 3493};
		ListenList::const_iterator it = find(conf.listens.begin(), conf.listens.end(), listen);
		CPPUNIT_ASSERT_MESSAGE("LISTEN ::1 3493", it != conf.listens.end());
	}

}
