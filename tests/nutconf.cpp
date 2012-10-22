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

// Define to desactivate protection of parsing tool members:
#define UNITEST_MODE 1

#include "nutconf.h"
using namespace nut;

#include <string>
using namespace std;

class NutConfTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE( NutConfTest );
    CPPUNIT_TEST( testParseCHARS );
    CPPUNIT_TEST( testParseSTRCHARS );
    CPPUNIT_TEST( testPasreToken );
	CPPUNIT_TEST( testGenericConfigParser );
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void testParseCHARS();
  void testParseSTRCHARS();
  void testPasreToken();

  void testGenericConfigParser();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( NutConfTest );


void NutConfTest::setUp()
{
}


void NutConfTest::tearDown()
{
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
        "titi = \"tata toto\"";
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


