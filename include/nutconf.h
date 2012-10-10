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

#ifdef __cplusplus

namespace nut
{

/**
 * NUT config parser.
 */
class NutConfigParser
{
public:
  NutConfigParser(const char* buffer = NULL);
  NutConfigParser(const std::string& buffer);

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

      bool operator==(const Token& tok)const{return tok.type==type && tok.str==str;}
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


} /* namespace nut */
#endif /* __cplusplus */


#if 0

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif


/** Token type enumeration.*/
typedef enum{
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
}PARSING_TOKEN_e;



/** Lexical token pointer.
 * It is a fragment of a c-string represented by its first character (begin) and
 * the byte after its last character.
 * Empty token is represented by end==begin.
 * Null token is represented by begin==end==NULL.
 */
typedef struct {
    const char* begin;
    const char* end;
    PARSING_TOKEN_e type;
}LEXTOKEN_t;

/** Helper to copy a token to another.*/
inline void LEXTOKEN_copy(LEXTOKEN_t* tgt, LEXTOKEN_t* src)
{
    tgt->begin = src->begin;
    tgt->end   = src->end;
    tgt->type  = src->type;
}

/** Helper to set token values.*/
inline void LEXTOKEN_set(LEXTOKEN_t* tok, const char* begin, const char* end,
    PARSING_TOKEN_e type)
{
    tok->begin = begin;
    tok->end   = end;
    tok->type  = type;
}

/** Helper to copy a string in a dedicated allocated memory.*/
inline char* LEXTOKEN_chralloc(LEXTOKEN_t* tok)
{
	if(tok->begin && tok->begin<tok->end)
	{
		size_t len = tok->end-tok->begin;
		char* mem = (char*)malloc(len+1);
		memcpy(mem, tok->begin, len);
		mem[len] = 0;
		return mem;
	}
	else
	{
		return 0;
	}
}


/** Syntaxical parsing machine state enumeration.*/
typedef enum {
	SYNPARSING_STATE_INIT,
	SYNPARSING_STATE_FINISHED,
	SYNPARSING_STATE_SECTION_BEGIN,
	SYNPARSING_STATE_SECTION_NAME,
	SYNPARSING_STATE_SECTION_END,
	SYNPARSING_STATE_DIRECTIVE_BEGIN,
	SYNPARSING_STATE_DIRECTIVE_ARGUMENT
}SYNPARSING_STATE_e;

/** Line type.*/
typedef enum {
    SYNPARSING_LINETYPE_UNKNWON = -1,
    SYNPARSING_LINETYPE_COMMENT,
    SYNPARSING_LINETYPE_SECTION,
    SYNPARSING_LINETYPE_DIRECTIVE_COLON,
    SYNPARSING_LINETYPE_DIRECTIVE_EQUAL,
    SYNPARSING_LINETYPE_DIRECTIVE_NOSEP
}SYNPARSING_LINETYPE_e;


/** Syntaxic line parsing result structure. */
typedef struct {
	SYNPARSING_LINETYPE_e line_type;
	PARSING_TOKEN_e       directive_separator;
	LEXTOKEN_t*           args; /* Array of LEXTOKEN_t. */
	unsigned int        nb_args; /* Size of array args. */
	unsigned int        arg_count; /* Number of arg in array args. */
	LEXTOKEN_t            comment;
}SYNLINE_t;



int nutconf_parse_rule_CHARS(const char* src, unsigned int len);

int nutconf_parse_rule_STRCHARS(const char* src, unsigned int len);


PARSING_TOKEN_e nutconf_parse_token(const char* src, unsigned int len,
	LEXTOKEN_t* token);

void nutconf_parse_line(const char* src, unsigned int len,
	const char** rend, SYNLINE_t* parsed_line);


typedef void (*nutconf_parse_line_callback)(SYNLINE_t* line, void* user_data);

void nutconf_parse_memory(const char* src, int len,
	nutconf_parse_line_callback cb, void* user_data);





/** Generic chaining node for values. */
typedef struct NUTCONF_ARGVALUE_s{
	char* value;
	struct NUTCONF_ARGVALUE_s* next;
}NUTCONF_ARGVALUE_t;

/** Type of argument.
 * TODO Change it to a more semantic naming scheme
 */
typedef enum {
	NUTCONF_ARG_NONE,
	NUTCONF_ARG_COLON,
	NUTCONF_ARG_EQUAL
}NUTCONF_ARG_TYPE_e;

/** Generic chaining node for names. */
typedef struct NUTCONF_ARG_s{
	char* name;
	NUTCONF_ARG_TYPE_e type;
	NUTCONF_ARGVALUE_t* value;
	struct NUTCONF_ARG_s* next;
}NUTCONF_ARG_t;


/** Generic nutconf storage structure.
 * Bloc of values for one section of data.
 * Have a pointer to an eventually following bloc.
 */
typedef struct NUTCONF_SECTION_s{
	char* name;
	NUTCONF_ARG_t* args;
	struct NUTCONF_SECTION_s* next;
}NUTCONF_SECTION_t, NUTCONF_t;


/** Generic configuration parsing. */
NUTCONF_t* nutconf_conf_parse(const char* src, int len);


#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* 0 */

#endif	/* NUTCONF_H_SEEN */
