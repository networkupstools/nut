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

#include "nutconf.h"


namespace nut
{

//
// NutConfigParser
//

NutConfigParser::NutConfigParser(const char* buffer):
_buffer(buffer),
_pos(0)
{
}

NutConfigParser::NutConfigParser(const std::string& buffer):
_buffer(buffer),
_pos(0)
{
}

char NutConfigParser::get()
{
    if(_pos>=_buffer.size())
        return 0;
    else
        return _buffer[_pos++];
}

char NutConfigParser::peek()
{
    return _buffer[_pos];
}

size_t NutConfigParser::getPos()const
{
    return _pos;
}

void NutConfigParser::setPos(size_t pos)
{
    _pos = pos;
}

char NutConfigParser::charAt(size_t pos)const
{
    return _buffer[pos];
}

void NutConfigParser::pushPos()
{
    _stack.push_back(_pos);
}

size_t NutConfigParser::popPos()
{
    size_t pos = _stack.back();
    _stack.pop_back();
    return pos;
}

void NutConfigParser::rewind()
{
    _pos = popPos();
}

void NutConfigParser::back()
{
    if(_pos>0)
        --_pos;
}


/* Parse a string source for a CHARS and return its size if found or 0, if not.
 * CHARS     ::= CHAR+
 * CHAR      ::= __ASCIICHAR__ - ( __SPACES__ | '\\' | '\"' | '#' )
 *             | '\\' ( __SPACES__ | '\\' | '\"' | '#' )
 * TODO: accept "\t", "\s", "\r", "\n" ??
 */
std::string NutConfigParser::parseCHARS()
{
	bool escaped = false; // Is char escaped ?
    std::string res;      // Stored string

    pushPos();

	for (char c = get(); c != 0 /*EOF*/; c = get())
	{
		if (escaped)
		{
			if (isspace(c) || c == '\\' || c == '"' || c == '#')
			{
                res += c;
			}
			else
			{
				/* WTF ??? */
			}
			escaped = false;
		}
		else
		{
			if (c == '\\')
			{
				escaped = true;
			}
			else if (isgraph(c) /*&& c != '\\'*/ && c != '"' && c != '#')
			{
                res += c;
			}
			else
			{
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
std::string NutConfigParser::parseSTRCHARS()
{
	bool escaped = false; // Is char escaped ?
    std::string str;      // Stored string

    pushPos();

	for (char c = get(); c != 0 /*EOF*/; c = get())
	{
		if (escaped)
		{
			if (isspace(c) || c == '\\' || c == '"')
			{
                str += c;
			}
			else
			{
				/* WTF ??? */
			}
			escaped = false;
		}
		else
		{
			if (c == '\\')
			{
				escaped = true;
			}
			else if (isprint(c) && c != '\\' && c != '"')
			{
                str += c;
			}
			else
			{
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
NutConfigParser::Token NutConfigParser::parseToken()
{
    /** Lexical parsing machine state enumeration.*/
    typedef enum {
        LEXPARSING_STATE_DEFAULT,
        LEXPARSING_STATE_QUOTED_STRING,
        LEXPARSING_STATE_STRING,
        LEXPARSING_STATE_COMMENT
    }LEXPARSING_STATE_e;
	LEXPARSING_STATE_e state = LEXPARSING_STATE_DEFAULT;

    Token token;
	bool escaped = false;

    pushPos();

    for (char c = get(); c != 0 /*EOF*/; c = get())
	{
		switch (state)
		{
			case LEXPARSING_STATE_DEFAULT: /* Wait for a non-space char */
			{
				if (c == ' ' || c == '\t')
				{
					/* Space : do nothing */
				}
				else if (c == '[')
				{
                    token = Token(Token::TOKEN_BRACKET_OPEN, c);
                    popPos();
                    return token;
				}
				else if (c == ']')
				{
                    token = Token(Token::TOKEN_BRACKET_CLOSE, c);
                    popPos();
                    return token;
				}
				else if (c == ':')
				{
                    token = Token(Token::TOKEN_COLON, c);
                    popPos();
                    return token;
				}
				else if (c == '=')
				{
                    token = Token(Token::TOKEN_EQUAL, c);
                    popPos();
                    return token;
				}
				else if (c == '\r' || c == '\n')
				{
                    token = Token(Token::TOKEN_EOL, c);
                    popPos();
                    return token;
				}
				else if (c == '#')
				{
                    token.type = Token::TOKEN_COMMENT;
					state = LEXPARSING_STATE_COMMENT;
				}
				else if (c == '"')
				{
					/* Begin of QUOTED STRING */
                    token.type = Token::TOKEN_QUOTED_STRING;
					state = LEXPARSING_STATE_QUOTED_STRING;
				}
				else if (c == '\\')
				{
					/* Begin of STRING with escape */
                    token.type = Token::TOKEN_STRING;
					state = LEXPARSING_STATE_STRING;
					escaped = true;
				}
				else if (isgraph(c))
				{
					/* Begin of STRING */
                    token.type = Token::TOKEN_STRING;
					state = LEXPARSING_STATE_STRING;
                    token.str += c;
				}
				else
				{
                    rewind();
                    return Token(Token::TOKEN_UNKNOWN);
				}
				break;
			}
			case LEXPARSING_STATE_QUOTED_STRING:
			{
				if (c == '"')
				{
					if(escaped)
					{
						escaped = false;
                        token.str += '"';
					}
					else
					{
                        popPos();
                        return token;
					}
				}
				else if (c == '\\')
				{
					if (escaped)
					{
						escaped = false;
                        token.str += '\\';
					}
					else
					{
						escaped = true;
					}
				}
				else if (c == ' ' || c == '\t' || isgraph(c))
				{
                    token.str += c;
				}
                else if(c == 0) /* EOL */
                {
                    popPos();
                    return token;
                }
                else /* Bad character ?? */
                {
                    /* WTF ? Keep, Ignore ? */
                }
				/* TODO What about other escaped character ? */
				break;
			}
			case LEXPARSING_STATE_STRING:
			{
				if (c == ' ' || c == '"' || c == '#' || c == '[' || c == ']' || c == ':' || c == '=')
				{
					if (escaped)
					{
						escaped = false;
                        token.str += c;
					}
					else
					{
                        back();
                        popPos();
                        return token;
					}
				}
				else if (c == '\\')
				{
					if (escaped)
					{
						escaped = false;
                        token.str += c;
					}
					else
					{
						escaped = true;
					}
				}
				/* else if (c == '\r' || c == '\n') */
                else if (isgraph(c))
                {
                    token.str += c;
                }
				else  /* Bad character ?? */
				{
                    /* WTF ? Keep, Ignore ? */
				}
				/* TODO What about escaped character ? */
				break;
			}
			case LEXPARSING_STATE_COMMENT:
			{
				if (c == '\r' || c == '\n')
				{
                    return token;
				}
                else
                {
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


std::list<NutConfigParser::Token> NutConfigParser::parseLine()
{
    std::list<NutConfigParser::Token> res;

    while(true)
    {
        NutConfigParser::Token token = parseToken();
        
        switch(token.type)
        {
            
        }
    }

    return res;
}

} /* namespace nut */







#if 0

/** Parse a string source for a line.
 * \param src Begin of text source to parse.
 * \param len Size of text source to parse in byte.
 * \param[out] rend Pointer where store end of line (address of byte following
 * the last character of the line, so the size of line is end-src).
 * \param[out] parsed_line Parsed line result.
 * TODO Handle end-of-file
 */
void nutconf_parse_line(const char* src, unsigned int len,
	const char** rend, SYNLINE_t* parsed_line)
{
	PARSING_TOKEN_e    directive_separator = TOKEN_NONE;
    SYNPARSING_STATE_e state = SYNPARSING_STATE_INIT;
    SYNPARSING_LINETYPE_e line_type = SYNPARSING_LINETYPE_UNKNWON;

	LEXTOKEN_t current;

/* Little macros (state-machine sub-functions) to reuse in this function: */

/* Add current token to arg list.*/
#define nutconf_parse_line__PUSH_ARG() \
{\
    if (parsed_line->arg_count < parsed_line->nb_args-1)\
    {\
        LEXTOKEN_copy(&parsed_line->args[parsed_line->arg_count], &current);\
        parsed_line->arg_count++;\
    }\
    /* TODO Add args overflow handling. */\
}
/* Set comment and end state machine: */
#define nutconf_parse_line__SET_COMMENT_AND_END_STM() \
{\
    LEXTOKEN_copy(&parsed_line->comment, &current);\
    state = SYNPARSING_STATE_FINISHED;\
}

	if (parsed_line == NULL)
	{
		return; /* TODO add error code. */
	}
	/* Init returned values */
	parsed_line->line_type = SYNPARSING_LINETYPE_UNKNWON;
	parsed_line->directive_separator = TOKEN_NONE;
	parsed_line->arg_count = 0;
	LEXTOKEN_set(&parsed_line->comment, NULL, NULL, TOKEN_NONE);

	/* Lets parse */
    while (TRUE)
    {
        nutconf_parse_token(src, len, &current);
        switch (state)
        {
        case SYNPARSING_STATE_INIT:
            switch (current.type)
            {
            case TOKEN_COMMENT: /* Line with only a comment. */
                line_type = SYNPARSING_LINETYPE_COMMENT;
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_BRACKET_OPEN: /* Begin of a section. */
                state = SYNPARSING_STATE_SECTION_BEGIN;
                break;
            case TOKEN_STRING:  /* Begin of a directive line. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                state = SYNPARSING_STATE_DIRECTIVE_BEGIN;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;
            }
            break;
        case SYNPARSING_STATE_SECTION_BEGIN:
            switch (current.type)
            {
            case TOKEN_BRACKET_CLOSE: /* Empty section. */
                state = SYNPARSING_STATE_SECTION_END;
                break;
            case TOKEN_STRING:  /* Section name. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                state = SYNPARSING_STATE_SECTION_NAME;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_SECTION_NAME:
            switch (current.type)
            {
            case TOKEN_BRACKET_CLOSE: /* End of named section. */
                state = SYNPARSING_STATE_SECTION_END;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_SECTION_END:
            switch (current.type)
            {
            case TOKEN_COMMENT:
                line_type = SYNPARSING_LINETYPE_SECTION;
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_EOL:
                line_type = SYNPARSING_LINETYPE_SECTION;
                state = SYNPARSING_STATE_FINISHED;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_DIRECTIVE_BEGIN:
            switch (current.type)
            {
            case TOKEN_COLON:   /* Directive with ':'.*/
            case TOKEN_EQUAL:   /* Directive with '='.*/
                directive_separator = current.type;
                state = SYNPARSING_STATE_DIRECTIVE_ARGUMENT;
                break;
            case TOKEN_STRING:  /* Directive direct argument, no separator. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                state = SYNPARSING_STATE_DIRECTIVE_ARGUMENT;
                break;
            case TOKEN_COMMENT:
                line_type = SYNPARSING_LINETYPE_DIRECTIVE_NOSEP;
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_EOL:
                line_type = SYNPARSING_LINETYPE_DIRECTIVE_NOSEP;
                state = SYNPARSING_STATE_FINISHED;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_DIRECTIVE_ARGUMENT:
            switch (current.type)
            {
            case TOKEN_STRING:  /* Directive argument. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                /* Keep here, in SYNPARSING_STATE_DIRECTIVE_ARGUMENT state.*/
                break;
            case TOKEN_COMMENT:
                /* TODO signal directive with comment */
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_EOL:
                line_type = SYNPARSING_LINETYPE_DIRECTIVE_NOSEP;
                state = SYNPARSING_STATE_FINISHED;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;
            }
            break;
        default:
            /* Must not occur. */
            /* TODO WTF ? .*/
            break;
        }

		src = *rend = current.end; 
        if (state == SYNPARSING_STATE_FINISHED)
            break; /* Go out infinite while loop. */
    }

    if (line_type == SYNPARSING_LINETYPE_DIRECTIVE_NOSEP)
    {
        if (directive_separator == TOKEN_COLON)
        {
            line_type = SYNPARSING_LINETYPE_DIRECTIVE_COLON;
        }
        else if (directive_separator == TOKEN_EQUAL)
        {
            line_type = SYNPARSING_LINETYPE_DIRECTIVE_EQUAL;
        }
    }

    /* End of process : save data for returning */
	parsed_line->line_type = line_type;
	parsed_line->directive_separator = directive_separator;

#undef nutconf_parse_line__PUSH_ARG
#undef nutconf_parse_line__SET_COMMENT_AND_END_STM
}


/* Parse a string source, memory mapping of a conf file.
 * End the parsing at the end of file (ie null-char, specified size
 * or error).
 */
void nutconf_parse_memory(const char* src, int len,
	nutconf_parse_line_callback cb, void* user_data)
{
	const char* rend = src;
	SYNLINE_t    parsed_line;
	LEXTOKEN_t   tokens[16];

	parsed_line.args = tokens;
	parsed_line.nb_args = 16;

	while (len > 0)
	{
		nutconf_parse_line(src, len, &rend, &parsed_line);

		cb(&parsed_line, user_data);

		len -= rend - src;
		src = rend;
	}
}




typedef struct {
	NUTCONF_t* conf;
	NUTCONF_SECTION_t* current_section;
	NUTCONF_ARG_t* current_arg;
}NUTCONF_CONF_PARSE_t;

static void nutconf_conf_parse_callback(SYNLINE_t* line, void* user_data)
{
	NUTCONF_CONF_PARSE_t* parse = (NUTCONF_CONF_PARSE_t*)user_data;
	int num;

	/* Verify parameters */
	if (parse==NULL)
	{
		return;
	}

	/* Parsing state treatment */
	switch (line->line_type)
	{
	case SYNPARSING_LINETYPE_SECTION:
		if (parse->current_section == NULL)
		{
			/* No current section - begin of the parsing.*/
			/* Use conf as section .*/
			parse->current_section = parse->conf;
		}
		else
		{
			/* Already have a section, add new one to chain. */
			parse->current_section->next = malloc(sizeof(NUTCONF_SECTION_t));
			parse->current_section = parse->current_section->next;
			memset(parse->current_section, 0, sizeof(NUTCONF_SECTION_t));
			parse->current_arg = NULL;
		}
		/* Set the section name. */
		if (line->arg_count > 0)
		{
			parse->current_section->name = LEXTOKEN_chralloc(&line->args[0]);
		}
		break;
	case SYNPARSING_LINETYPE_DIRECTIVE_COLON:
	case SYNPARSING_LINETYPE_DIRECTIVE_EQUAL:
	case SYNPARSING_LINETYPE_DIRECTIVE_NOSEP:
		if (line->arg_count < 1)
		{
			/* No directive if no argument. */
			break;
		}

		if (parse->current_section == NULL)
		{
			/* No current section - begin of the parsing.*/
			/* Use conf as section .*/
			parse->current_section = parse->conf;
		}

		/* Add a new argument. */
		if (parse->current_arg != NULL)
		{
			parse->current_arg->next = malloc(sizeof(NUTCONF_ARG_t));
			parse->current_arg = parse->current_arg->next;
		}
		else
		{
			parse->current_arg = malloc(sizeof(NUTCONF_ARG_t));
		}
		memset(parse->current_arg, 0, sizeof(NUTCONF_ARG_t));

		/* Set directive name. */
		parse->current_arg->name = LEXTOKEN_chralloc(&line->args[0]);
		
		/* Set directive type. */
		switch(line->line_type)
		{
		case SYNPARSING_LINETYPE_DIRECTIVE_COLON:
			parse->current_arg->type = NUTCONF_ARG_COLON;
			break;
		case SYNPARSING_LINETYPE_DIRECTIVE_EQUAL:
			parse->current_arg->type = NUTCONF_ARG_EQUAL;
			break;
		default:
			parse->current_arg->type = NUTCONF_ARG_NONE;
			break;
		}

		/* TODO Add directive values.*/
		for(num=1; num<line->arg_count; num++)
		{
			/* ... */
		}

		break;
	default:
		/* Do nothing (unknown or comment. */
		break;
	}
}

NUTCONF_t* nutconf_conf_parse(const char* src, int len)
{
	NUTCONF_CONF_PARSE_t parse;
	
	/* Validate parameters */
	if (src==NULL || len <=0)
	{
		return NULL;
	}

	/* Initialize working structures */
	memset(&parse.conf, 0, sizeof(NUTCONF_CONF_PARSE_t));
	parse.conf = malloc(sizeof(NUTCONF_t));
	memset(parse.conf, 0, sizeof(NUTCONF_t));
	
	/* Do the parsing. */
	nutconf_parse_memory(src, len, nutconf_conf_parse_callback, &parse);

	/* TODO Test for successfull parsing. */
	return parse.conf;
}


#endif /* 0 */