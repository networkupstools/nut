/* nutconf.h - Nut configuration file manipulation API

   Copyright (C)
	2012	Emilien Kia <emilien.kia@gmail.com>

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

#ifndef NUTCONF_H_SEEN
#define NUTCONF_H_SEEN 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <list>
#include <map>

#ifdef __cplusplus

namespace nut
{

class NutParser;
class NutConfigParser;
class DefaultConfigParser;
class GenericConfigParser;



/**
 * NUT config parser.
 */
class NutParser
{
public:
  NutParser(const char* buffer = NULL);
  NutParser(const std::string& buffer);

  struct Token
  {
      enum TokenType {
	TOKEN_UNKNOWN = -1,
	TOKEN_NONE    = 0,
	TOKEN_STRING  = 1,
	TOKEN_QUOTED_STRING,
	TOKEN_COMMENT,
	TOKEN_BRACKET_OPEN,
	TOKEN_BRACKET_CLOSE,
	TOKEN_EQUAL,
	TOKEN_COLON,
	TOKEN_EOL
      }type;
      std::string str;

      Token():type(TOKEN_NONE),str(){}
      Token(TokenType type, const std::string& str=""):type(type),str(str){}
      Token(TokenType type, char c):type(type),str(1, c){}
      Token(const Token& tok):type(tok.type),str(tok.str){}

      bool is(TokenType type)const{return this->type==type;}

      bool operator==(const Token& tok)const{return tok.type==type && tok.str==str;}

      operator bool()const{return type!=TOKEN_UNKNOWN && type!=TOKEN_NONE;}
  };

  /** Parsing functions
   * \{ */
  std::string parseCHARS();
  std::string parseSTRCHARS();
  Token parseToken();
  std::list<Token> parseLine();
  /** \} */

#ifndef UNITEST_MODE
protected:
#endif /* UNITEST_MODE */
    size_t getPos()const;
    void setPos(size_t pos);
    char charAt(size_t pos)const;

    void pushPos();
    size_t popPos();
    void rewind();

    void back();

    char get();
    char peek();

private:
    std::string _buffer;
    size_t _pos;
    std::vector<size_t> _stack;
};


typedef std::list<std::string> ConfigParamList;

class NutConfigParser : public NutParser
{
public:
    virtual void parseConfig();

protected:
    NutConfigParser(const char* buffer = NULL);
    NutConfigParser(const std::string& buffer);

    virtual void onParseBegin()=0;
    virtual void onParseComment(const std::string& comment)=0;
    virtual void onParseSectionName(const std::string& sectionName, const std::string& comment = "")=0;
    virtual void onParseDirective(const std::string& directiveName, char sep = 0, const ConfigParamList& values = ConfigParamList(), const std::string& comment = "")=0;
    virtual void onParseEnd()=0;
};

struct GenericConfigSectionEntry
{
	std::string     name;
	ConfigParamList values;
	// std::string  comment;
};

struct GenericConfigSection
{
	std::string name;
	// std::string comment;
	std::map<std::string, GenericConfigSectionEntry> entries;

	bool empty()const;
	void clear();
};

class DefaultConfigParser : public NutConfigParser
{
public:
    DefaultConfigParser(const char* buffer = NULL);
    DefaultConfigParser(const std::string& buffer);

protected:
	virtual void onParseSection(const GenericConfigSection& section)=0;

    virtual void onParseBegin();
    virtual void onParseComment(const std::string& comment);
    virtual void onParseSectionName(const std::string& sectionName, const std::string& comment = "");
    virtual void onParseDirective(const std::string& directiveName, char sep = 0, const ConfigParamList& values = ConfigParamList(), const std::string& comment = "");
    virtual void onParseEnd();

	GenericConfigSection _section; ///> Currently parsed section
};


class BaseConfiguration
{
	friend class GenericConfigParser;
protected:
	virtual void setGenericConfigSection(const GenericConfigSection& section) = 0;
};

class GenericConfigParser : public DefaultConfigParser
{
public:
    GenericConfigParser(const char* buffer = NULL);
    GenericConfigParser(const std::string& buffer);

	virtual void parseConfig(BaseConfiguration* config);

protected:
	virtual void onParseSection(const GenericConfigSection& section);

	BaseConfiguration* _config;
};


class GenericConfiguration : public BaseConfiguration
{
public:
	GenericConfiguration(){}

	// FIXME Let me public or set it as protected with public accessors ?
	std::map<std::string, GenericConfigSection> sections;

protected:
	virtual void setGenericConfigSection(const GenericConfigSection& section);
};





} /* namespace nut */
#endif /* __cplusplus */
#endif	/* NUTCONF_H_SEEN */
