/* nutconf.cpp - configuration API

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

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include <sstream>
#include <iostream>

#include "nutconf.h"


namespace nut {

//
// Tool functions
//

/**
 * Parse a specified type from a string and set it as Settable if success.
 */
template <typename T>
Settable<T> StringToSettableNumber(const std::string & src)
{
	std::stringstream ss(src);
	T result;
	if(ss >> result)
	{
		return Settable<T>(result);
	}
	else
	{
		return Settable<T>();
	}
}




//
// NutParser
//

NutParser::NutParser(const char* buffer) :
_buffer(buffer),
_pos(0) {
}

NutParser::NutParser(const std::string& buffer) :
_buffer(buffer),
_pos(0) {
}

char NutParser::get() {
    if (_pos >= _buffer.size())
        return 0;
    else
        return _buffer[_pos++];
}

char NutParser::peek() {
    return _buffer[_pos];
}

size_t NutParser::getPos()const {
    return _pos;
}

void NutParser::setPos(size_t pos) {
    _pos = pos;
}

char NutParser::charAt(size_t pos)const {
    return _buffer[pos];
}

void NutParser::pushPos() {
    _stack.push_back(_pos);
}

size_t NutParser::popPos() {
    size_t pos = _stack.back();
    _stack.pop_back();
    return pos;
}

void NutParser::rewind() {
    _pos = popPos();
}

void NutParser::back() {
    if (_pos > 0)
        --_pos;
}

/* Parse a string source for a CHARS and return its size if found or 0, if not.
 * CHARS     ::= CHAR+
 * CHAR      ::= __ASCIICHAR__ - ( __SPACES__ | '\\' | '\"' | '#' )
 *             | '\\' ( __SPACES__ | '\\' | '\"' | '#' )
 * TODO: accept "\t", "\s", "\r", "\n" ??
 */
std::string NutParser::parseCHARS() {
    bool escaped = false; // Is char escaped ?
    std::string res; // Stored string

    pushPos();

    for (char c = get(); c != 0 /*EOF*/; c = get()) {
        if (escaped) {
            if (isspace(c) || c == '\\' || c == '"' || c == '#') {
                res += c;
            } else {
                /* WTF ??? */
            }
            escaped = false;
        } else {
            if (c == '\\') {
                escaped = true;
            } else if (isgraph(c) /*&& c != '\\'*/ && c != '"' && c != '#') {
                res += c;
            } else {
                back();
                break;
            }
        }
    }

    popPos();
    return res;
}

/* Parse a string source for a STRCHARS and return its size if found or 0, if not.
 * STRCHARS  ::= STRCHAR+
 * STRCHAR   ::= __ASCIICHAR__ - ( '\\' | '\"')
 *             | '\\' ( '\\' | '\"' )
 * TODO: accept "\t", "\s", "\r", "\n" ??
 */
std::string NutParser::parseSTRCHARS() {
    bool escaped = false; // Is char escaped ?
    std::string str; // Stored string

    pushPos();

    for (char c = get(); c != 0 /*EOF*/; c = get()) {
        if (escaped) {
            if (isspace(c) || c == '\\' || c == '"') {
                str += c;
            } else {
                /* WTF ??? */
            }
            escaped = false;
        } else {
            if (c == '\\') {
                escaped = true;
            } else if (isprint(c) && c != '\\' && c != '"') {
                str += c;
            } else {
                back();
                break;
            }
        }
    }

    popPos();
    return str;
}

/** Parse a string source for getting the next token, ignoring spaces.
 * \return Token type.
 */
NutParser::Token NutParser::parseToken() {

    /** Lexical parsing machine state enumeration.*/
    typedef enum {
        LEXPARSING_STATE_DEFAULT,
        LEXPARSING_STATE_QUOTED_STRING,
        LEXPARSING_STATE_STRING,
        LEXPARSING_STATE_COMMENT
    } LEXPARSING_STATE_e;
    LEXPARSING_STATE_e state = LEXPARSING_STATE_DEFAULT;

    Token token;
    bool escaped = false;

    pushPos();

    for (char c = get(); c != 0 /*EOF*/; c = get()) {
        switch (state) {
            case LEXPARSING_STATE_DEFAULT: /* Wait for a non-space char */
            {
                if (c == ' ' || c == '\t') {
                    /* Space : do nothing */
                } else if (c == '[') {
                    token = Token(Token::TOKEN_BRACKET_OPEN, c);
                    popPos();
                    return token;
                } else if (c == ']') {
                    token = Token(Token::TOKEN_BRACKET_CLOSE, c);
                    popPos();
                    return token;
                } else if (c == ':') {
                    token = Token(Token::TOKEN_COLON, c);
                    popPos();
                    return token;
                } else if (c == '=') {
                    token = Token(Token::TOKEN_EQUAL, c);
                    popPos();
                    return token;
                } else if (c == '\r' || c == '\n') {
                    token = Token(Token::TOKEN_EOL, c);
                    popPos();
                    return token;
                } else if (c == '#') {
                    token.type = Token::TOKEN_COMMENT;
                    state = LEXPARSING_STATE_COMMENT;
                } else if (c == '"') {
                    /* Begin of QUOTED STRING */
                    token.type = Token::TOKEN_QUOTED_STRING;
                    state = LEXPARSING_STATE_QUOTED_STRING;
                } else if (c == '\\') {
                    /* Begin of STRING with escape */
                    token.type = Token::TOKEN_STRING;
                    state = LEXPARSING_STATE_STRING;
                    escaped = true;
                } else if (isgraph(c)) {
                    /* Begin of STRING */
                    token.type = Token::TOKEN_STRING;
                    state = LEXPARSING_STATE_STRING;
                    token.str += c;
                } else {
                    rewind();
                    return Token(Token::TOKEN_UNKNOWN);
                }
                break;
            }
            case LEXPARSING_STATE_QUOTED_STRING:
            {
                if (c == '"') {
                    if (escaped) {
                        escaped = false;
                        token.str += '"';
                    } else {
                        popPos();
                        return token;
                    }
                } else if (c == '\\') {
                    if (escaped) {
                        escaped = false;
                        token.str += '\\';
                    } else {
                        escaped = true;
                    }
                } else if (c == ' ' || c == '\t' || isgraph(c)) {
                    token.str += c;
                } else if (c == '\r' || c == '\n') /* EOL */{
					/* WTF ? consider it as correct ? */
					back();
                    popPos();
                    return token;					
                } else if (c == 0) /* EOF */ {
                    popPos();
                    return token;
                } else /* Bad character ?? */ {
                    /* WTF ? Keep, Ignore ? */
                }
                /* TODO What about other escaped character ? */
                break;
            }
            case LEXPARSING_STATE_STRING:
            {
                if (c == ' ' || c == '"' || c == '#' || c == '[' || c == ']' || c == ':' || c == '=') {
                    if (escaped) {
                        escaped = false;
                        token.str += c;
                    } else {
                        back();
                        popPos();
                        return token;
                    }
                } else if (c == '\\') {
                    if (escaped) {
                        escaped = false;
                        token.str += c;
                    } else {
                        escaped = true;
                    }
                } else if (c == '\r' || c == '\n') /* EOL */{
					back();
                    popPos();
                    return token;					
                } else if (c == 0) /* EOF */ {
                    popPos();
                    return token;
                }else if (isgraph(c)) {
                    token.str += c;
                } else /* Bad character ?? */ {
                    /* WTF ? Keep, Ignore ? */
                }
                /* TODO What about escaped character ? */
                break;
            }
            case LEXPARSING_STATE_COMMENT:
            {
                if (c == '\r' || c == '\n') {
                    return token;
                } else {
                    token.str += c;
                }
                break;
            }
            default:
                /* Must not occur. */
                break;
        }
    }
    popPos();
    return token;
}

std::list<NutParser::Token> NutParser::parseLine() {
    std::list<NutParser::Token> res;

    while (true) {
        NutParser::Token token = parseToken();

        switch (token.type) {
            case Token::TOKEN_STRING:
            case Token::TOKEN_QUOTED_STRING:
            case Token::TOKEN_BRACKET_OPEN:
            case Token::TOKEN_BRACKET_CLOSE:
            case Token::TOKEN_EQUAL:
            case Token::TOKEN_COLON:
                res.push_back(token);
                break;
            case Token::TOKEN_COMMENT:
                res.push_back(token);
                // Do not break, should return (EOL)Token::TOKEN_COMMENT:
            case Token::TOKEN_UNKNOWN:
            case Token::TOKEN_NONE:
            case Token::TOKEN_EOL:
                return res;
        }
    }
}

//
// NutConfigParser
//

NutConfigParser::NutConfigParser(const char* buffer) :
NutParser(buffer) {
}

NutConfigParser::NutConfigParser(const std::string& buffer) :
NutParser(buffer) {
}

void NutConfigParser::parseConfig() {
    onParseBegin();

    enum ConfigParserState {
        CPS_DEFAULT,
        CPS_SECTION_OPENED,
        CPS_SECTION_HAVE_NAME,
        CPS_SECTION_CLOSED,
        CPS_DIRECTIVE_HAVE_NAME,
        CPS_DIRECTIVE_VALUES
    } state = CPS_DEFAULT;

    Token tok;
    std::string name;
    std::list<std::string> values;
    char sep;
    while (tok = parseToken()) {
        switch (state) {
            case CPS_DEFAULT:
                switch (tok.type) {
                    case Token::TOKEN_COMMENT:
                        onParseComment(tok.str);
                        /* Clean and return to default */
                        break;
                    case Token::TOKEN_BRACKET_OPEN:
                        state = CPS_SECTION_OPENED;
                        break;
                    case Token::TOKEN_STRING:
                    case Token::TOKEN_QUOTED_STRING:
                        name = tok.str;
                        state = CPS_DIRECTIVE_HAVE_NAME;
                        break;
                    default:
                        /* WTF ? */
                        break;
                }
                break;
            case CPS_SECTION_OPENED:
                switch (tok.type) {
                    case Token::TOKEN_STRING:
                    case Token::TOKEN_QUOTED_STRING:
                        /* Should occur ! */
                        name = tok.str;
                        state = CPS_SECTION_HAVE_NAME;
                        break;
                    case Token::TOKEN_BRACKET_CLOSE:
                        /* Empty section name */
                        state = CPS_SECTION_CLOSED;
                        break;
                    case Token::TOKEN_COMMENT:
                        /* Lack of closing bracket !!! */
                        onParseSectionName(name, tok.str);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    case Token::TOKEN_EOL:
                        /* Lack of closing bracket !!! */
                        onParseSectionName(name);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    default:
                        /* WTF ? */
                        break;
                }
                break;
            case CPS_SECTION_HAVE_NAME:
                switch (tok.type) {
                    case Token::TOKEN_BRACKET_CLOSE:
                        /* Must occur ! */
                        state = CPS_SECTION_CLOSED;
                        break;
                    case Token::TOKEN_COMMENT:
                        /* Lack of closing bracket !!! */
                        onParseSectionName(name, tok.str);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    case Token::TOKEN_EOL:
                        /* Lack of closing bracket !!! */
                        onParseSectionName(name);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    default:
                        /* WTF ? */
                        break;
                }
                break;
            case CPS_SECTION_CLOSED:
                switch (tok.type) {
                    case Token::TOKEN_COMMENT:
                        /* Could occur ! */
                        onParseSectionName(name, tok.str);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    case Token::TOKEN_EOL:
                        /* Could occur ! */
                        onParseSectionName(name);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    default:
                        /* WTF ? */
                        break;
                }
                break;
            case CPS_DIRECTIVE_HAVE_NAME:
                switch (tok.type) {
                    case Token::TOKEN_COMMENT:
                        /* Could occur ! */
                        onParseDirective(name, 0, std::list<std::string > (), tok.str);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    case Token::TOKEN_EOL:
                        /* Could occur ! */
                        onParseDirective(name);
                        /* Clean and return to default */
                        name.clear();
                        state = CPS_DEFAULT;
                        break;
                    case Token::TOKEN_COLON:
                    case Token::TOKEN_EQUAL:
                        /* Could occur ! */
                        sep = tok.str[0];
                        state = CPS_DIRECTIVE_VALUES;
                        break;
                    case Token::TOKEN_STRING:
                    case Token::TOKEN_QUOTED_STRING:
                        /* Could occur ! */
                        values.push_back(tok.str);
                        state = CPS_DIRECTIVE_VALUES;
                        break;
                    default:
                        /* WTF ? */
                        break;
                }
                break;
            case CPS_DIRECTIVE_VALUES:
                switch (tok.type) {
                    case Token::TOKEN_COMMENT:
                        /* Could occur ! */
                        onParseDirective(name, sep, values, tok.str);
                        /* Clean and return to default */
                        name.clear();
                        values.clear();
                        sep = 0;
                        state = CPS_DEFAULT;
                        break;
                    case Token::TOKEN_EOL:
                        /* Could occur ! */
                        onParseDirective(name, sep, values);
                        /* Clean and return to default */
                        name.clear();
                        values.clear();
                        sep = 0;
                        state = CPS_DEFAULT;
                        break;
                    case Token::TOKEN_STRING:
                    case Token::TOKEN_QUOTED_STRING:
                        /* Could occur ! */
                        values.push_back(tok.str);
                        state = CPS_DIRECTIVE_VALUES;
                        break;
                    default:
                        /* WTF ? */
                        break;
                }
                break;
        }
    }

	switch(state)
	{
	case CPS_SECTION_OPENED:
    case CPS_SECTION_HAVE_NAME:
        /* Lack of closing bracket !!! */
        onParseSectionName(name);
		break;
    case CPS_SECTION_CLOSED:
        /* Could occur ! */
        onParseSectionName(name);
		break;
    case CPS_DIRECTIVE_HAVE_NAME:
        /* Could occur ! */
        onParseDirective(name);
		break;
    case CPS_DIRECTIVE_VALUES:
        /* Could occur ! */
        onParseDirective(name, sep, values);
		break;
	}

    onParseEnd();
}



//
// DefaultConfigParser
//

DefaultConfigParser::DefaultConfigParser(const char* buffer) :
NutConfigParser(buffer) {
}

DefaultConfigParser::DefaultConfigParser(const std::string& buffer) :
NutConfigParser(buffer) {
}

void DefaultConfigParser::onParseBegin() {
    // Start with empty section (ie global one)
    _section.clear();
}

void DefaultConfigParser::onParseComment(const std::string& /*comment*/) {
    // Comment are ignored for now
}

void DefaultConfigParser::onParseSectionName(const std::string& sectionName, const std::string& /*comment*/) {
    // Comment are ignored for now

    // Process current section.
    if (!_section.empty()) {
        onParseSection(_section);
        _section.clear();
    }

    // Start a new section
    _section.name = sectionName;
}

void DefaultConfigParser::onParseDirective(const std::string& directiveName, char /*sep*/, const ConfigParamList& values, const std::string& /*comment*/) {
    // Comment are ignored for now
    // Separator has no specific semantic in this context

    // Save values
    _section.entries[directiveName].name = directiveName;
    _section.entries[directiveName].values = values;

	// TODO Can probably be optimized.
}

void DefaultConfigParser::onParseEnd() {
    // Process current (last) section
    if (!_section.empty()) {
        onParseSection(_section);
        _section.clear();
    }
}


//
// GenericConfigSection
//

bool GenericConfigSection::empty()const {
    return name.empty() && entries.empty();
}

void GenericConfigSection::clear() {
    name.clear();
    entries.clear();
}

//
// GenericConfigParser
//

GenericConfigParser::GenericConfigParser(const char* buffer):
DefaultConfigParser(buffer),
_config(NULL)
{
}

GenericConfigParser::GenericConfigParser(const std::string& buffer):
DefaultConfigParser(buffer),
_config(NULL)
{
}

void GenericConfigParser::parseConfig(BaseConfiguration* config)
{
	if(config!=NULL)
	{
		_config = config;
		NutConfigParser::parseConfig();
		_config = NULL;
	}
}


void GenericConfigParser::onParseSection(const GenericConfigSection& section)
{
	if(_config!=NULL)
	{
		_config->setGenericConfigSection(section);
	}
}

//
// GenericConfiguration
//

void GenericConfiguration::setGenericConfigSection(const GenericConfigSection& section)
{
	sections[section.name] = section;
}

void GenericConfiguration::parseFromString(const std::string& str)
{
	GenericConfigParser parser(str);
	parser.parseConfig(this);
}

//
// UpsmonConfiguration
//

UpsmonConfiguration::UpsmonConfiguration()
{
}

void UpsmonConfiguration::parseFromString(const std::string& str)
{
    UpsmonConfigParser parser(str);
    parser.parseUpsmonConfig(this);
}

UpsmonConfiguration::NotifyFlag UpsmonConfiguration::NotifyFlagFromString(const std::string& str)
{
	if(str=="SYSLOG")
		return NOTIFY_SYSLOG;
	else if(str=="WALL")
		return NOTIFY_WALL;
	else if(str=="EXEC")
		return NOTIFY_EXEC;
	else if(str=="IGNORE")
		return NOTIFY_IGNORE;
	else
		return NOTIFY_IGNORE;
}

UpsmonConfiguration::NotifyType UpsmonConfiguration::NotifyTypeFromString(const std::string& str)
{
	if(str=="ONLINE")
		return NOTIFY_ONLINE;
	else if(str=="ONBATT")
		return NOTIFY_ONBATT;
	else if(str=="LOWBATT")
		return NOTIFY_LOWBATT;
	else if(str=="FSD")
		return NOTIFY_FSD;
	else if(str=="COMMOK")
		return NOTIFY_COMMOK;
	else if(str=="COMMBAD")
		return NOTIFY_COMMBAD;
	else if(str=="SHUTDOWN")
		return NOTIFY_SHUTDOWN;
	else if(str=="REPLBATT")
		return NOTIFY_REPLBATT;
	else if(str=="NOCOMM")
		return NOTIFY_NOCOMM;
	else if(str=="NOPARENT")
		return NOTIFY_NOPARENT;
	else
		return NOTIFY_TYPE_MAX;
}

//
// UpsmonConfigParser
//

UpsmonConfigParser::UpsmonConfigParser(const char* buffer):
NutConfigParser(buffer)
{
}

UpsmonConfigParser::UpsmonConfigParser(const std::string& buffer):
NutConfigParser(buffer)
{
}

void UpsmonConfigParser::parseUpsmonConfig(UpsmonConfiguration* config)
{
	if(config!=NULL)
	{
		_config = config;
		NutConfigParser::parseConfig();
		_config = NULL;
	}
}

void UpsmonConfigParser::onParseBegin()
{
    // Do nothing
}

void UpsmonConfigParser::onParseComment(const std::string& /*comment*/)
{
    // Comment are ignored for now
}

void UpsmonConfigParser::onParseSectionName(const std::string& /*sectionName*/, const std::string& /*comment*/)
{
    // There must not have sections in upsm.conf.
    // Ignore it
    // TODO Add error reporting ?
}


void UpsmonConfigParser::onParseDirective(const std::string& directiveName, char /*sep*/, const ConfigParamList& values, const std::string& /*comment*/)
{
	// NOTE: separators are always ignored

	if(_config)
		{
		if(directiveName == "RUN_AS_USER")
		{
			if(values.size()>0)
			{
				_config->runAsUser = values.front();
			}
		}
		else if(directiveName == "MONITOR")
		{
			if(values.size()==5)
			{
				UpsmonConfiguration::Monitor monitor;
				ConfigParamList::const_iterator it = values.begin();
				std::stringstream system(*it++);
				std::string word;
				monitor.upsname = (getline(system, word, '@'), word);
				monitor.hostname = (getline(system, word, ':'), word);
				monitor.port = (getline(system, word) ? *StringToSettableNumber<unsigned int>(word) : 0u);
				monitor.powerValue = StringToSettableNumber<unsigned int>(*it++);
				monitor.username = *it++;
				monitor.password = *it++;
				monitor.isMaster = (*it) == "master";
				_config->monitors.push_back(monitor);
			}
		}
		else if(directiveName == "MINSUPPLIES")
		{
			if(values.size()>0)
			{
				_config->minSupplies = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "SHUTDOWNCMD")
		{
			if(values.size()>0)
			{
				_config->shutdownCmd = values.front();
			}
		}
		else if(directiveName == "NOTIFYCMD")
		{
			if(values.size()>0)
			{
				_config->notifyCmd = values.front();
			}
		}
		else if(directiveName == "POLLFREQ")
		{
			if(values.size()>0)
			{
				_config->poolFreq = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "POLLFREQALERT")
		{
			if(values.size()>0)
			{
				_config->poolFreqAlert = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "HOSTSYNC")
		{
			if(values.size()>0)
			{
				_config->hotSync = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "DEADTIME")
		{
			if(values.size()>0)
			{
				_config->deadTime = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "POWERDOWNFLAG")
		{
			if(values.size()>0)
			{
				_config->powerDownFlag = values.front();
			}
		}
		else if(directiveName == "NOTIFYMSG")
		{
			if(values.size()==2)
			{
				UpsmonConfiguration::NotifyType type = UpsmonConfiguration::NotifyTypeFromString(values.front());
				if(type!=UpsmonConfiguration::NOTIFY_TYPE_MAX)
				{
					_config->notifyMessages[(unsigned int)type] = *(++values.begin());
				}
			}
		}
		else if(directiveName == "NOTIFYFLAG")
		{
			if(values.size()==2)
			{
				UpsmonConfiguration::NotifyType type = UpsmonConfiguration::NotifyTypeFromString(values.front());
				if(type!=UpsmonConfiguration::NOTIFY_TYPE_MAX)
				{
					unsigned int flags = 0;
					std::string word;
					std::stringstream stream(*(++values.begin()));
					while( getline(stream, word, '+') )
					{
						flags |= UpsmonConfiguration::NotifyFlagFromString(word);
					}
					_config->notifyFlags[(unsigned int)type] = flags;
				}
			}
		}
		else if(directiveName == "RBWARNTIME")
		{
			if(values.size()>0)
			{
				_config->rbWarnTime = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "NOCOMMWARNTIME")
		{
			if(values.size()>0)
			{
				_config->noCommWarnTime = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "FINALDELAY")
		{
			if(values.size()>0)
			{
				_config->finalDelay = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else
		{
			// TODO WTF with unknown commands ?
		}
	}
}

void UpsmonConfigParser::onParseEnd()
{
    // Do nothing
}


} /* namespace nut */
