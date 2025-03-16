/*
    nutconf.cpp - configuration API

    Copyright (C)
        2012	Emilien Kia <emilien.kia@gmail.com>
        2024	Jim Klimov <jimklimov+nut@gmail.com>

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

#include "nutconf.hpp"
#include "nutwriter.hpp"

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include <sstream>
#include <iostream>
#include <cassert>


namespace nut {

/* Trivial implementations out of class declaration to avoid
 * error: 'ClassName' has no out-of-line virtual method definitions; its vtable
 *   will be emitted in every translation unit [-Werror,-Wweak-vtables]
 */
Serialisable::~Serialisable() {}
BaseConfiguration::~BaseConfiguration() {}
GenericConfiguration::~GenericConfiguration() {}
UpsConfiguration::~UpsConfiguration() {}
NutParser::~NutParser() {}

//
// Tool functions
//

/**
 * Parse a specified type from a string and set it as Settable if success.
 */
template <typename T>
Settable<T> StringToSettableNumber(const std::string & src)
{
	if (typeid(T) == typeid(bool)) {
		static const Settable<T> b0(false);
		static const Settable<T> b1(true);

		// See also: GenericConfiguration::str2bool()
		// FIXME: Can these two methods be merged?
		if ("true" == src) return b1;
		if ("on"   == src) return b1;
		if ("1"    == src) return b1;
		if ("yes"  == src) return b1;
		if ("ok"   == src) return b1;

		return b0;
	} else {
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
}


//
// NutParser
//

NutParser::NutParser(const char* buffer, unsigned int options) :
_options(options),
_buffer(buffer),
_pos(0) {
}

NutParser::NutParser(const std::string& buffer, unsigned int options) :
_options(options),
_buffer(buffer),
_pos(0) {
}

void NutParser::setOptions(unsigned int options, bool set)
{
	if(set)
	{
		_options |= options;
	}
	else
	{
		_options &= ~options;
	}
}

char NutParser::get()
{
	if (_pos >= _buffer.size())
		return 0;
	else
		return _buffer[_pos++];
}

char NutParser::peek()
{
	return _buffer[_pos];
}

size_t NutParser::getPos()const
{
	return _pos;
}

void NutParser::setPos(size_t pos)
{
	_pos = pos;
}

char NutParser::charAt(size_t pos)const
{
	return _buffer[pos];
}

void NutParser::pushPos()
{
	_stack.push_back(_pos);
}

size_t NutParser::popPos()
{
	size_t pos = _stack.back();
	_stack.pop_back();
	return pos;
}

void NutParser::rewind()
{
	_pos = popPos();
}

void NutParser::back()
{
	if (_pos > 0)
		--_pos;
}

/* Parse a string source for a CHARS and return its size if found or 0, if not.
 * CHARS     ::= CHAR+
 * CHAR      ::= __ASCIICHAR__ - ( __SPACES__ | '\\' | '\"' | '#' )
 *             | '\\' ( __SPACES__ | '\\' | '\"' | '#' )
 * TODO: accept "\t", "\s", "\r", "\n" ??
 */
std::string NutParser::parseCHARS()
{
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
std::string NutParser::parseSTRCHARS()
{
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
NutParser::Token NutParser::parseToken()
{

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
				} else if (c == ':' && !hasOptions(OPTION_IGNORE_COLON)) {
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
				if (c == ' ' || c == '\t' || c == '"' || c == '#' || c == '[' || c == ']'
				||  (c == ':' && !hasOptions(OPTION_IGNORE_COLON))
				||  c == '='
				) {
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

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
			default:
				/* Must not occur. */
				break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}
	}
	popPos();
	return token;
}

std::list<NutParser::Token> NutParser::parseLine()
{
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
				// Should return (EOL)Token::TOKEN_COMMENT:
				return res;
			case Token::TOKEN_UNKNOWN:
			case Token::TOKEN_NONE:
			case Token::TOKEN_EOL:
				return res;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
			default:
				/* Must not occur. */
				break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}
	}
}

//
// NutConfigParser
//

NutConfigParser::NutConfigParser(const char* buffer, unsigned int options) :
NutParser(buffer, options)
{
}

NutConfigParser::NutConfigParser(const std::string& buffer, unsigned int options) :
NutParser(buffer, options)
{
}

void NutConfigParser::parseConfig(BaseConfiguration* config)
{
	NUT_UNUSED_VARIABLE(config);
	parseConfig();
}

void NutConfigParser::parseConfig()
{
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
	char sep = 0;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
	while (1) {
		tok = parseToken();
		if (!tok)
			break;
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

					case Token::TOKEN_UNKNOWN:
					case Token::TOKEN_NONE:
					case Token::TOKEN_BRACKET_CLOSE:
					case Token::TOKEN_EQUAL:
					case Token::TOKEN_COLON:
					case Token::TOKEN_EOL:
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

					case Token::TOKEN_UNKNOWN:
					case Token::TOKEN_NONE:
					case Token::TOKEN_BRACKET_OPEN:
					case Token::TOKEN_EQUAL:
					case Token::TOKEN_COLON:
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

					case Token::TOKEN_QUOTED_STRING:
					case Token::TOKEN_BRACKET_OPEN:
					case Token::TOKEN_COLON:
					case Token::TOKEN_EQUAL:
					case Token::TOKEN_UNKNOWN:
					case Token::TOKEN_NONE:
					case Token::TOKEN_STRING:
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

					case Token::TOKEN_QUOTED_STRING:
					case Token::TOKEN_BRACKET_OPEN:
					case Token::TOKEN_BRACKET_CLOSE:
					case Token::TOKEN_UNKNOWN:
					case Token::TOKEN_NONE:
					case Token::TOKEN_STRING:
					case Token::TOKEN_COLON:
					case Token::TOKEN_EQUAL:
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

					case Token::TOKEN_UNKNOWN:
					case Token::TOKEN_NONE:
					case Token::TOKEN_BRACKET_OPEN:
					case Token::TOKEN_BRACKET_CLOSE:
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

					case Token::TOKEN_UNKNOWN:
					case Token::TOKEN_NONE:
					case Token::TOKEN_BRACKET_OPEN:
					case Token::TOKEN_BRACKET_CLOSE:
					case Token::TOKEN_EQUAL:
					case Token::TOKEN_COLON:
					default:
						/* WTF ? */
						break;
				}
				break;

			default:
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
		case CPS_DEFAULT:
			/* TOTHINK: no-op? */
			break;
		default:
			break;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif

	onParseEnd();
}



//
// DefaultConfigParser
//

DefaultConfigParser::DefaultConfigParser(const char* buffer) :
NutConfigParser(buffer)
{
}

DefaultConfigParser::DefaultConfigParser(const std::string& buffer) :
NutConfigParser(buffer)
{
}

void DefaultConfigParser::onParseBegin()
{
	// Start with empty section (i.e. global one)
	_section.clear();
}

void DefaultConfigParser::onParseComment(
		const std::string& /*comment*/)
{
	// Comments are ignored for now
}

void DefaultConfigParser::onParseSectionName(
		const std::string& sectionName,
		const std::string& /*comment*/)
{
	// Comments are ignored for now

	// Process current section.
	if (!_section.empty()) {
		onParseSection(_section);
		_section.clear();
	}

	// Start a new section
	_section.name = sectionName;
}

void DefaultConfigParser::onParseDirective(
		const std::string& directiveName,
		char /*sep*/,
		const ConfigParamList& values,
		const std::string& /*comment*/)
{
	// Comments are ignored for now
	// Separator has no specific semantic in this context

	// Save values
	_section.entries[directiveName].name = directiveName;
	_section.entries[directiveName].values = values;

	// TODO Can probably be optimized.
}

void DefaultConfigParser::onParseEnd()
{
	// Process current (last) section
	if (!_section.empty()) {
		onParseSection(_section);
		_section.clear();
	}
}


//
// GenericConfigSection
//

bool GenericConfigSection::empty()const
{
	return name.empty() && entries.empty();
}

void GenericConfigSection::clear()
{
	name.clear();
	entries.clear();
}

//
// GenericConfigParser
//

GenericConfigParser::GenericConfigParser(const char* buffer):
DefaultConfigParser(buffer),
_config(nullptr)
{
}

GenericConfigParser::GenericConfigParser(const std::string& buffer):
DefaultConfigParser(buffer),
_config(nullptr)
{
}

void GenericConfigParser::parseConfig(BaseConfiguration* config)
{
	if(config!=nullptr)
	{
		_config = config;
		NutConfigParser::parseConfig();
		_config = nullptr;
	}
}


void GenericConfigParser::onParseSection(const GenericConfigSection& section)
{
	if(_config!=nullptr)
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


bool GenericConfiguration::parseFrom(NutStream & istream)
{
	// TODO: The parser is highly inefficient, it should use NutStream, directly
	std::string str;

	if (NutStream::NUTS_OK != istream.getString(str))
		return false;

	parseFromString(str);

	return true;
}


bool GenericConfiguration::writeTo(NutStream & ostream) const
{
	GenericConfigWriter writer(ostream);

	return NutWriter::NUTW_OK == writer.writeConfig(*this);
}


bool GenericConfiguration::get(const std::string & section, const std::string & entry, ConfigParamList & params, bool caseSensitive) const
{
	// Get section
	SectionMap::const_iterator section_iter = sections.find(section);
	if (section_iter == sections.end())
		return false;

	// Get entry
	const GenericConfigSection::EntryMap & entries = section_iter->second.entries;

	GenericConfigSection::EntryMap::const_iterator entry_iter = entries.find(entry);
	if (entry_iter == entries.end()) {
		if (caseSensitive)
			return false;

		// Another pass, maybe slower and inefficient, for case-insensitive matching
		// We are already at one end of the entries, so scroll back to beginning
		GenericConfigSection::EntryMap::const_iterator entry_begin = entries.begin();
		for (; entry_iter != entry_begin; entry_iter--) {
			if (!(::strcasecmp(entry_iter->first.c_str(), entry.c_str())))
				goto found;
		}

		return false;
	}

found:
	// Provide parameters values
	params = entry_iter->second.values;

	return true;
}


void GenericConfiguration::set(const std::string & section, const std::string & entry, const ConfigParamList & params)
{
	// Get section
	SectionMap::iterator section_iter = sections.lower_bound(section);
	if (sections.end() == section_iter || section_iter->first != section) {
		section_iter = sections.insert(section_iter,
				std::pair<const std::string, GenericConfigSection>(section, GenericConfigSection()));

		section_iter->second.name = section;
	}

	// Get entry
	GenericConfigSection::EntryMap & entries = section_iter->second.entries;

	GenericConfigSection::EntryMap::iterator entry_iter = entries.lower_bound(entry);
	if (entries.end() == entry_iter || entry_iter->first != entry) {
		entry_iter = entries.insert(entry_iter,
				std::pair<const std::string, GenericConfigSectionEntry>(entry, GenericConfigSectionEntry()));

		entry_iter->second.name = entry;
	}

	// Set parameters values
	entry_iter->second.values = params;
}


void GenericConfiguration::add(const std::string & section, const std::string & entry, const ConfigParamList & params)
{
	// No job is another job well done
	if (params.empty())
		return;

	// Note that the implementation is quite naive and inefficient.
	// However, efficiency isn't our aim at the moment.

	// Get current parameters
	ConfigParamList current_params;

	get(section, entry, current_params);

	// Add the provided parameters
	current_params.insert(current_params.end(), params.begin(), params.end());

	set(section, entry, current_params);
}


void GenericConfiguration::remove(const std::string & section, const std::string & entry)
{
	// Get section
	SectionMap::iterator section_iter = sections.find(section);
	if (sections.end() == section_iter)
		return;

	// Get entry
	GenericConfigSection::EntryMap & entries = section_iter->second.entries;

	GenericConfigSection::EntryMap::iterator entry_iter = entries.find(entry);
	if (entries.end() == entry_iter)
		return;

	entries.erase(entry_iter);
}


void GenericConfiguration::removeSection(const std::string & section)
{
	// Get section
	SectionMap::iterator section_iter = sections.find(section);
	if (sections.end() == section_iter)
		return;

	sections.erase(section_iter);
}


std::string GenericConfiguration::getStr(const std::string & section, const std::string & entry, bool caseSensitive) const
{
	std::string str;

	ConfigParamList params;

	if (!get(section, entry, params, caseSensitive))
		return str;

	if (params.empty())
		return str;

	return params.front();
}


void GenericConfiguration::setStr(
		const std::string & section,
		const std::string & entry,
		const std::string & value)
{
	ConfigParamList param;

	param.push_back(value);

	set(section, entry, param);
}


bool GenericConfiguration::getBool(
		const std::string & section,
		const std::string & entry,
		bool                val) const
{
		std::string s = getStr(section, entry);
		if (s.empty())
			return val;
		return str2bool(s);
}


bool GenericConfiguration::getFlag(
		const std::string & section,
		const std::string & entry) const
{
	ConfigParamList params;

	if (!get(section, entry, params))
		return false;

	// Flag - if exists then "true"
	return true;
}


void GenericConfiguration::setFlag(
		const std::string & section,
		const std::string & entry,
		bool                val)
{
	ConfigParamList param;

	if (val) {
		param.push_back("true");
		set(section, entry, param);
	} else {
		remove(section, entry);
	}
}


long long int GenericConfiguration::getInt(
		const std::string & section,
		const std::string & entry,
		long long int val) const
{
	ConfigParamList params;

	if (!get(section, entry, params))
		return val;

	if (params.empty())
		return val;

	// TBD: What if there are multiple values?
	std::stringstream val_str(params.front());

	val_str >> val;

	return val;
}


void GenericConfiguration::setInt(
		const std::string & section,
		const std::string & entry,
		long long int val)
{
	std::stringstream val_str;
	val_str << val;

	set(section, entry, ConfigParamList(1, val_str.str()));
}


long long int GenericConfiguration::getIntHex(
		const std::string & section,
		const std::string & entry,
		long long int val) const
{
	ConfigParamList params;

	if (!get(section, entry, params))
		return val;

	if (params.empty())
		return val;

	// TBD: What if there are multiple values?
	std::string s = params.front();
	size_t foundPos = s.rfind("0x", 0);
	if (foundPos == std::string::npos || foundPos != 0) {
		// Add the prefix for hex conversion
		s = "0x" + s;
	}
	std::stringstream val_str;
	val_str << std::hex << s;

	// Output into int type
	val_str >> std::hex >> val;

	return val;
}


void GenericConfiguration::setIntHex(
		const std::string & section,
		const std::string & entry,
		long long int val)
{
	std::stringstream val_str;

	// Note NUT v2.8.1 introduced these as "hexnum" values,
	// but the strtoul() underneath knows to strip "0x" for
	// base16 conversions - so can we write them either way:

	// https://stackoverflow.com/a/61060765/4715872
	// << std::showbase for "0x" in saved printouts
	val_str << std::nouppercase << std::hex << val;

	set(section, entry, ConfigParamList(1, val_str.str()));
}


double GenericConfiguration::getDouble(
		const std::string & section,
		const std::string & entry,
		double val) const
{
	ConfigParamList params;

	if (!get(section, entry, params))
		return val;

	if (params.empty())
		return val;

	// TBD: What if there are multiple values?
	std::stringstream val_str(params.front());

	val_str >> val;

	return val;
}


void GenericConfiguration::setDouble(
		const std::string & section,
		const std::string & entry,
		double val)
{
	std::stringstream val_str;
	val_str << val;

	set(section, entry, ConfigParamList(1, val_str.str()));
}


nut::BoolInt GenericConfiguration::getBoolInt(
		const std::string & section,
		const std::string & entry,
		nut::BoolInt val) const
{
	ConfigParamList params;

	if (!get(section, entry, params))
		return val;

	if (params.empty())
		return val;

	// TBD: What if there are multiple values?
	nut::BoolInt bi(params.front());

	return bi;
}


bool GenericConfiguration::str2bool(const std::string & str)
{
	if ("true" == str) return true;
	if ("on"   == str) return true;
	if ("1"    == str) return true;
	if ("yes"  == str) return true;
	if ("ok"   == str) return true;

	return false;
}


const std::string & GenericConfiguration::bool2str(bool val)
{
	static const std::string b0("off");
	static const std::string b1("on");

	return val ? b1 : b0;
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
	else if(str=="CAL")
		return NOTIFY_CAL;
	else if(str=="NOTCAL")
		return NOTIFY_NOTCAL;
	else if(str=="OFF")
		return NOTIFY_OFF;
	else if(str=="NOTOFF")
		return NOTIFY_NOTOFF;
	else if(str=="BYPASS")
		return NOTIFY_BYPASS;
	else if(str=="NOTBYPASS")
		return NOTIFY_NOTBYPASS;
	else if(str=="ECO")	/* inverter mode, not ups state, for notifications */
		return NOTIFY_ECO;
	else if(str=="NOTECO")	/* inverter mode, not ups state, for notifications */
		return NOTIFY_NOTECO;
	else if(str=="ALARM")
		return NOTIFY_ALARM;
	else if(str=="NOTALARM")
		return NOTIFY_NOTALARM;
	else if(str=="OTHER")
		return NOTIFY_OTHER;
	else if(str=="NOTOTHER")
		return NOTIFY_NOTOTHER;
	else if(str=="SUSPEND_STARTING")
		return NOTIFY_SUSPEND_STARTING;
	else if(str=="SUSPEND_FINISHED")
		return NOTIFY_SUSPEND_FINISHED;
	else
		return NOTIFY_TYPE_MAX;
}


bool UpsmonConfiguration::parseFrom(NutStream & istream)
{
	// TODO: The parser is highly inefficient, it should use NutStream, directly
	std::string str;

	if (NutStream::NUTS_OK != istream.getString(str))
		return false;

	parseFromString(str);

	return true;
}


bool UpsmonConfiguration::writeTo(NutStream & ostream) const
{
	UpsmonConfigWriter writer(ostream);

	return NutWriter::NUTW_OK == writer.writeConfig(*this);
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
	if(config!=nullptr)
	{
		_config = config;
		NutConfigParser::parseConfig();
		_config = nullptr;
	}
}

void UpsmonConfigParser::onParseBegin()
{
	// Do nothing
}

void UpsmonConfigParser::onParseComment(const std::string& /*comment*/)
{
	// Comments are ignored for now
}

void UpsmonConfigParser::onParseSectionName(const std::string& /*sectionName*/, const std::string& /*comment*/)
{
	// There must not be sections in upsmon.conf.
	// Ignore it
	// TODO Add error reporting ?
}


void UpsmonConfigParser::onParseDirective(const std::string& directiveName, char /*sep*/, const ConfigParamList& values, const std::string& /*comment*/)
{
	// NOTE: separators are always ignored

	if(_config)
	{
		if(!(::strcasecmp(directiveName.c_str(), "DEBUG_MIN")))
		{
			// NOTE: We allow DEBUG_MIN in any casing as it can be copy-pasted
			// between different configs and they use different historic casing
			if(values.size()>0)
			{
				_config->debugMin = StringToSettableNumber<int>(values.front());
			}
		}
		else if(directiveName == "RUN_AS_USER")
		{
			if(values.size()>0)
			{
				_config->runAsUser = values.front();
			}
		}
		else if(directiveName == "MONITOR")
		{
			if (values.size() == 5 || values.size() == 6)
			{
				UpsmonConfiguration::Monitor monitor;
				ConfigParamList::const_iterator it = values.begin();
				std::stringstream upsAtHost(*it++);
				std::string wordToken;
				/*
				 * Why didn't the original author just receive the words
				 * into their target strings?.. e.g.:
				 *     std::getline(upsAtHost, monitor.upsname, '@');
				 *     std::getline(upsAtHost, monitor.hostname);
				 */
				monitor.upsname = (static_cast<void>(std::getline(upsAtHost, wordToken, '@')), wordToken);
				monitor.hostname = (static_cast<void>(std::getline(upsAtHost, wordToken)), wordToken);
				monitor.port = (values.size() == 6 ? *StringToSettableNumber<uint16_t>(*it++) : 0u);
				monitor.powerValue = StringToSettableNumber<unsigned int>(*it++);
				monitor.username = *it++;
				monitor.password = *it++;
				monitor.isPrimary = (*it) == "primary";	// master for NUT v2.7.4 and older
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
				_config->pollFreq = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "POLLFREQALERT")
		{
			if(values.size()>0)
			{
				_config->pollFreqAlert = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "POLLFAIL_LOG_THROTTLE_MAX")
		{
			if(values.size()>0)
			{
				_config->pollFailLogThrottleMax = StringToSettableNumber<int>(values.front());
			}
		}
		else if(directiveName == "OFFDURATION")
		{
			if(values.size()>0)
			{
				_config->offDuration = StringToSettableNumber<int>(values.front());
			}
		}
		else if(directiveName == "OBLBDURATION")
		{
			if(values.size()>0)
			{
				_config->oblbDuration = StringToSettableNumber<int>(values.front());
			}
		}
		else if(directiveName == "SHUTDOWNEXIT")
		{
			if(values.size()>0)
			{
				nut::BoolInt bi;
				bi << values.front();
				_config->shutdownExit = bi;
			}
		}
		else if(directiveName == "CERTPATH")
		{
			if(values.size()>0)
			{
				_config->certPath = values.front();
			}
		}
		else if(directiveName == "CERTIDENT")
		{
			if(values.size()==2)
			{
				_config->certIdent.certName = values.front();
				_config->certIdent.certDbPass = (*(++values.begin()));
			}
		}
		else if(directiveName == "CERTHOST")
		{
			if(values.size()==4)
			{
				nut::CertHost certHost;
				ConfigParamList::const_iterator it = values.begin();
				certHost.host       = *it++;
				certHost.certName   = *it++;
				certHost.certVerify = *it++;
				certHost.forceSsl   = *it++;

				_config->certHosts.push_back(certHost);
			}
		}
		else if(directiveName == "CERTVERIFY")
		{
			if(values.size()>0)
			{
				nut::BoolInt bi;
				bi << values.front();
				_config->certVerify = bi;
			}
		}
		else if(directiveName == "FORCESSL")
		{
			if(values.size()>0)
			{
				nut::BoolInt bi;
				bi << values.front();
				_config->forceSsl = bi;
			}
		}
		else if(directiveName == "HOSTSYNC")
		{
			if(values.size()>0)
			{
				_config->hostSync = StringToSettableNumber<unsigned int>(values.front());
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
					_config->notifyMessages[static_cast<unsigned int>(type)] = *(++values.begin());
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
					while( std::getline(stream, word, '+') )
					{
						flags |= UpsmonConfiguration::NotifyFlagFromString(word);
					}
					_config->notifyFlags[static_cast<unsigned int>(type)] = flags;
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

//
// NutConfiguration
//

NutConfiguration::NutConfiguration():
	mode(MODE_UNKNOWN)
{
}

void NutConfiguration::parseFromString(const std::string& str)
{
	NutConfConfigParser parser(str);
	parser.parseNutConfConfig(this);
}

NutConfiguration::NutMode NutConfiguration::NutModeFromString(const std::string& str)
{
	if(str == "none")
		return MODE_NONE;
	else if(str == "standalone")
		return MODE_STANDALONE;
	else if(str == "netserver")
		return MODE_NETSERVER;
	else if(str == "netclient")
		return MODE_NETCLIENT;
	else if(str == "controlled")
		return MODE_CONTROLLED;
	else if(str == "manual")
		return MODE_MANUAL;
	else
		return MODE_UNKNOWN;
}


bool NutConfiguration::parseFrom(NutStream & istream)
{
	// TODO: The parser is highly inefficient, it should use NutStream, directly
	std::string str;

	if (NutStream::NUTS_OK != istream.getString(str))
		return false;

	parseFromString(str);

	return true;
}


bool NutConfiguration::writeTo(NutStream & ostream) const
{
	NutConfConfigWriter writer(ostream);

	return NutWriter::NUTW_OK == writer.writeConfig(*this);
}


//
// NutConfConfigParser
//

NutConfConfigParser::NutConfConfigParser(const char* buffer):
NutConfigParser(buffer)
{
}

NutConfConfigParser::NutConfConfigParser(const std::string& buffer):
NutConfigParser(buffer)
{
}

void NutConfConfigParser::parseNutConfConfig(NutConfiguration* config)
{
	if(config!=nullptr)
	{
		_config = config;
		NutConfigParser::parseConfig();
		_config = nullptr;
	}
}

void NutConfConfigParser::onParseBegin()
{
	// Do nothing
}

void NutConfConfigParser::onParseComment(const std::string& /*comment*/)
{
	// Comments are ignored for now
}

void NutConfConfigParser::onParseSectionName(const std::string& /*sectionName*/, const std::string& /*comment*/)
{
	// There must not be sections in nutconf.conf.
	// Ignore it
	// TODO Add error reporting ?
}

void NutConfConfigParser::onParseDirective(const std::string& directiveName, char /*sep*/, const ConfigParamList& values, const std::string& /*comment*/)
{
	// Comments are ignored for now
	// NOTE: although sep must be '=', sep is not verified.
	if(_config)
	{
		if(directiveName == "MODE")
		{
			if (values.size()==1) {
				std::string val = values.front();
				NutConfiguration::NutMode mode = NutConfiguration::NutModeFromString(val);
				if(mode != NutConfiguration::MODE_UNKNOWN)
					_config->mode = mode;
			}
		}
		else if(directiveName == "ALLOW_NO_DEVICE")
		{
			if(values.size()>0)
			{
				_config->allowNoDevice = StringToSettableNumber<bool>(values.front());
			}
		}
		else if(directiveName == "ALLOW_NOT_ALL_LISTENERS")
		{
			if(values.size()>0)
			{
				_config->allowNotAllListeners = StringToSettableNumber<bool>(values.front());
			}
		}
		else if(directiveName == "UPSD_OPTIONS")
		{
			if(values.size()>0)
			{
				_config->upsdOptions = values.front();
			}
		}
		else if(directiveName == "UPSMON_OPTIONS")
		{
			if(values.size()>0)
			{
				_config->upsmonOptions = values.front();
			}
		}
		else if(directiveName == "POWEROFF_WAIT")
		{
			if(values.size()>0)
			{
				_config->poweroffWait = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "POWEROFF_QUIET")
		{
			if(values.size()>0)
			{
				_config->poweroffQuiet = StringToSettableNumber<bool>(values.front());
			}
		}
		else if(directiveName == "NUT_DEBUG_LEVEL")
		{
			if(values.size()>0)
			{
				_config->debugLevel = StringToSettableNumber<int>(values.front());
			}
		}
		else
		{
			// TODO WTF with errors ?
		}
	}
}

void NutConfConfigParser::onParseEnd()
{
	// Do nothing
}


//
// UpsdConfiguration
//

UpsdConfiguration::UpsdConfiguration()
{
}

void UpsdConfiguration::parseFromString(const std::string& str)
{
	UpsdConfigParser parser(str);
	parser.parseUpsdConfig(this);
}


bool UpsdConfiguration::parseFrom(NutStream & istream)
{
	// TODO: The parser is highly inefficient, it should use NutStream, directly
	std::string str;

	if (NutStream::NUTS_OK != istream.getString(str))
		return false;

	parseFromString(str);

	return true;
}


bool UpsdConfiguration::writeTo(NutStream & ostream) const
{
	UpsdConfigWriter writer(ostream);

	return NutWriter::NUTW_OK == writer.writeConfig(*this);
}


//
// UpsdConfigParser
//

UpsdConfigParser::UpsdConfigParser(const char* buffer):
NutConfigParser(buffer, NutParser::OPTION_IGNORE_COLON)
{
}

UpsdConfigParser::UpsdConfigParser(const std::string& buffer):
NutConfigParser(buffer, NutParser::OPTION_IGNORE_COLON)
{
}

void UpsdConfigParser::parseUpsdConfig(UpsdConfiguration* config)
{
	if(config!=nullptr)
	{
		_config = config;
		NutConfigParser::parseConfig();
		_config = nullptr;
	}
}

void UpsdConfigParser::onParseBegin()
{
	// Do nothing
}

void UpsdConfigParser::onParseComment(const std::string& comment)
{
	// Comments are ignored for now
	NUT_UNUSED_VARIABLE(comment);
}

void UpsdConfigParser::onParseSectionName(const std::string& sectionName, const std::string& comment)
{
	// There must not be sections in upsd.conf.
	// Ignore it
	// TODO Add error reporting ?
	NUT_UNUSED_VARIABLE(sectionName);
	NUT_UNUSED_VARIABLE(comment);
}

void UpsdConfigParser::onParseDirective(const std::string& directiveName, char sep, const ConfigParamList& values, const std::string& comment)
{
	// NOTE: separators are always ignored
	NUT_UNUSED_VARIABLE(sep);
	NUT_UNUSED_VARIABLE(comment);

	if(_config)
	{
		if(!(::strcasecmp(directiveName.c_str(), "DEBUG_MIN")))
		{
			// NOTE: We allow DEBUG_MIN in any casing as it can be copy-pasted
			// between different configs and they use different historic casing
			if(values.size()>0)
			{
				_config->debugMin = StringToSettableNumber<int>(values.front());
			}
		}
		else if(directiveName == "MAXAGE")
		{
			if(values.size()>0)
			{
				_config->maxAge = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "STATEPATH")
		{
			if(values.size()>0)
			{
				_config->statePath = values.front();
			}
		}
		else if(directiveName == "MAXCONN")
		{
			if(values.size()>0)
			{
				_config->maxConn = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "TRACKINGDELAY")
		{
			if(values.size()>0)
			{
				_config->trackingDelay = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "ALLOW_NO_DEVICE")
		{
			if(values.size()>0)
			{
				_config->allowNoDevice = StringToSettableNumber<bool>(values.front());
			}
		}
		else if(directiveName == "ALLOW_NOT_ALL_LISTENERS")
		{
			if(values.size()>0)
			{
				_config->allowNotAllListeners = StringToSettableNumber<bool>(values.front());
			}
		}
		else if(directiveName == "DISABLE_WEAK_SSL")
		{
			if(values.size()>0)
			{
				_config->allowNotAllListeners = StringToSettableNumber<bool>(values.front());
			}
		}
		else if(directiveName == "CERTFILE")
		{
			if(values.size()>0)
			{
				_config->certFile = values.front();
			}
		}
		else if(directiveName == "CERTPATH")
		{
			if(values.size()>0)
			{
				_config->certPath = values.front();
			}
		}
		else if(directiveName == "CERTIDENT")
		{
			if(values.size()==2)
			{
				_config->certIdent.certName = values.front();
				_config->certIdent.certDbPass = (*(++values.begin()));
			}
		}
		else if(directiveName == "CERTREQUEST")
		{
			if(values.size()>0)
			{
				_config->certRequestLevel = StringToSettableNumber<unsigned int>(values.front());
			}
		}
		else if(directiveName == "LISTEN")
		{
			if(values.size()==1 || values.size()==2)
			{
				UpsdConfiguration::Listen listen;
				listen.address = values.front();
				if(values.size()==2)
				{
					listen.port = StringToSettableNumber<uint16_t>(*(++values.begin()));
				}
				_config->listens.push_back(listen);
			}
		}
		else
		{
			// TODO WTF with unknown commands ?
		}
	}
}

void UpsdConfigParser::onParseEnd()
{
	// Do nothing
}


//
// UpsdUsersConfiguration
//

UpsdUsersConfiguration::upsmon_mode_t UpsdUsersConfiguration::getUpsmonMode() const
{
	std::string mode_str = getStr("upsmon", "upsmon");

	if ("primary" == mode_str || "master" == mode_str)
		return UPSMON_PRIMARY;

	if ("secondary" == mode_str || "slave" == mode_str)
		return UPSMON_SECONDARY;

	return UPSMON_UNDEF;
}


void UpsdUsersConfiguration::setUpsmonMode(upsmon_mode_t mode)
{
	assert(UPSMON_UNDEF != mode);

	setStr("upsmon", "upsmon", (UPSMON_PRIMARY == mode ? "primary" : "secondary"));
	/* NUT v2.7.4 and older: setStr("upsmon", "upsmon", (UPSMON_PRIMARY == mode ? "master" : "slave")); */
}


bool UpsdUsersConfiguration::parseFrom(NutStream & istream)
{
	// TODO: The parser is highly inefficient, it should use NutStream, directly
	std::string str;

	if (NutStream::NUTS_OK != istream.getString(str))
		return false;

	parseFromString(str);

	return true;
}


bool UpsdUsersConfiguration::writeTo(NutStream & ostream) const
{
	UpsdUsersConfigWriter writer(ostream);

	return NutWriter::NUTW_OK == writer.writeConfig(*this);
}

} /* namespace nut */
