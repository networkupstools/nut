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
 * Helper to specify if a configuration variable is set or not.
 * In addition of its value.
 */
template<typename Type>
class Settable
{
protected:
	Type _value;
	bool _set;
public:
	Settable():_set(false){}
	Settable(const Settable<Type>& val):_value(val._value), _set(val._set){}
	Settable(const Type& val):_value(val), _set(true){}

	bool set()const{return _set;}
	void clear(){_set = false;}

	operator const Type&()const{return _value;}
	operator Type&(){return _value;}

	const Type& operator *()const{return _value;}
	Type& operator *(){return _value;}

	Settable<Type>& operator=(const Type& val){_value = val; _set = true;}

	bool operator==(const Settable<Type>& val)const
	{
		if(!set() && !val.set())
			return false;
		else 
			return (set() && val.set() && _value==val._value);
	}

	bool operator==(const Type& val)const
	{
		if(!set())
			return false;
		else
			return _value == val;
	}

};


/**
 * NUT config parser.
 */
class NutParser
{
public:
	enum ParsingOption
	{
		OPTION_DEFAULT = 0,
		/** Colon character is considered as string character and not as specific token.
			Usefull for IPv6 addresses */
		OPTION_IGNORE_COLON = 1
	};

	NutParser(const char* buffer = NULL, unsigned int options = OPTION_DEFAULT);
	NutParser(const std::string& buffer, unsigned int options = OPTION_DEFAULT);

	/** Parsing configuration functions
	 * \{ */
	void setOptions(unsigned int options){_options = options;}
	unsigned int getOptions()const{return _options;}
	void setOptions(unsigned int options, bool set = true);
	void unsetOptions(unsigned int options){setOptions(options, false);}
	bool hasOptions(unsigned int options)const{return (_options&options) == options;}
	/** \} */

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
	unsigned int _options;

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
    NutConfigParser(const char* buffer = NULL, unsigned int options = OPTION_DEFAULT);
    NutConfigParser(const std::string& buffer, unsigned int options = OPTION_DEFAULT);

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
	/** Section entries map */
	typedef std::map<std::string, GenericConfigSectionEntry> EntryMap;

	std::string name;
	// std::string comment;
	EntryMap entries;

	const GenericConfigSectionEntry& operator [] (const std::string& varname)const{return entries.find(varname)->second;}
	GenericConfigSectionEntry& operator [] (const std::string& varname){return entries[varname];}

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
	/** Sections map */
	typedef std::map<std::string, GenericConfigSection> SectionMap;

	GenericConfiguration(){}

	void parseFromString(const std::string& str);

	// TODO Add functions to write to string or files (Vasek ?)


	// FIXME Let me public or set it as protected with public accessors ?
	SectionMap sections;

	const GenericConfigSection& operator[](const std::string& secname)const{return sections.find(secname)->second;}
	GenericConfigSection& operator[](const std::string& secname){return sections[secname];}


protected:
	virtual void setGenericConfigSection(const GenericConfigSection& section);
};



class UpsmonConfiguration
{
public:
    UpsmonConfiguration();
    void parseFromString(const std::string& str);

    Settable<std::string>  runAsUser, shutdownCmd, notifyCmd, powerDownFlag;
    Settable<unsigned int> minSupplies, poolFreq, poolFreqAlert, hotSync;
    Settable<unsigned int> deadTime, rbWarnTime, noCommWarnTime, finalDelay;

    enum NotifyFlag {
        NOTIFY_IGNORE = 0,
        NOTIFY_SYSLOG = 1,
        NOTIFY_WALL = 1 << 1,
        NOTIFY_EXEC = 1 << 2
    };

    enum NotifyType {
        NOTIFY_ONLINE,
        NOTIFY_ONBATT,
        NOTIFY_LOWBATT,
        NOTIFY_FSD,
        NOTIFY_COMMOK,
        NOTIFY_COMMBAD,
        NOTIFY_SHUTDOWN,
        NOTIFY_REPLBATT,
        NOTIFY_NOCOMM,
        NOTIFY_NOPARENT,
        NOTIFY_TYPE_MAX
    };

	static NotifyFlag NotifyFlagFromString(const std::string& str);
	static NotifyType NotifyTypeFromString(const std::string& str);

    Settable<unsigned short> notifyFlags[NOTIFY_TYPE_MAX];
	Settable<std::string>    notifyMessages[NOTIFY_TYPE_MAX];

    struct Monitor {
        std::string upsname, hostname;
        unsigned short port;
        unsigned int powerValue;
        std::string username, password;
        bool isMaster;
    };

    std::list<Monitor> monitors;

};



class UpsmonConfigParser : public NutConfigParser
{
public:
    UpsmonConfigParser(const char* buffer = NULL);
    UpsmonConfigParser(const std::string& buffer);

    void parseUpsmonConfig(UpsmonConfiguration* config);
protected:
    virtual void onParseBegin();
    virtual void onParseComment(const std::string& comment);
    virtual void onParseSectionName(const std::string& sectionName, const std::string& comment = "");
    virtual void onParseDirective(const std::string& directiveName, char sep = 0, const ConfigParamList& values = ConfigParamList(), const std::string& comment = "");
    virtual void onParseEnd();

    UpsmonConfiguration* _config;
};


class NutConfiguration
{
public:
    NutConfiguration();
    void parseFromString(const std::string& str);

    enum NutMode {
		MODE_UNKNOWN = -1,
		MODE_NONE = 0,
		MODE_STANDALONE,
		MODE_NETSERVER,
		MODE_NETCLIENT
    };

	Settable<NutMode> mode;

	static NutMode NutModeFromString(const std::string& str);
};


class NutConfConfigParser : public NutConfigParser
{
public:
    NutConfConfigParser(const char* buffer = NULL);
    NutConfConfigParser(const std::string& buffer);

    void parseNutConfConfig(NutConfiguration* config);
protected:
    virtual void onParseBegin();
    virtual void onParseComment(const std::string& comment);
    virtual void onParseSectionName(const std::string& sectionName, const std::string& comment = "");
    virtual void onParseDirective(const std::string& directiveName, char sep = 0, const ConfigParamList& values = ConfigParamList(), const std::string& comment = "");
    virtual void onParseEnd();

    NutConfiguration* _config;
};


class UpsdConfiguration
{
public:
	UpsdConfiguration();
    void parseFromString(const std::string& str);

    Settable<unsigned int> maxAge, maxConn;
    Settable<std::string>  statePath, certFile;

	struct Listen
	{
		std::string address;
		Settable<unsigned short> port;

		inline bool operator==(const Listen& listen)const
		{
			return address == listen.address && port == listen.port;
		}
	};
	std::list<Listen> listens;
};




class UpsdConfigParser : public NutConfigParser
{
public:
    UpsdConfigParser(const char* buffer = NULL);
    UpsdConfigParser(const std::string& buffer);

    void parseUpsdConfig(UpsdConfiguration* config);
protected:
    virtual void onParseBegin();
    virtual void onParseComment(const std::string& comment);
    virtual void onParseSectionName(const std::string& sectionName, const std::string& comment = "");
    virtual void onParseDirective(const std::string& directiveName, char sep = 0, const ConfigParamList& values = ConfigParamList(), const std::string& comment = "");
    virtual void onParseEnd();

    UpsdConfiguration* _config;
};

} /* namespace nut */
#endif /* __cplusplus */
#endif	/* NUTCONF_H_SEEN */
