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
#include <stdint.h>

#include <string>
#include <vector>
#include <list>
#include <map>
#include <stdexcept>

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

	/**
	 *  \brief  Configuration parameters getter
	 *
	 *  \param[in]   section  Section name
	 *  \param[in]   entry    Entry name
	 *  \param[out]  params   Configuration parameters
	 *
	 *  \retval true  if the entry was found
	 *  \retval false otherwise
	 */
	bool get(const std::string & section, const std::string & entry, ConfigParamList & params) const;

	/**
	 *  \brief  Global scope configuration parameters getter
	 *
	 *  \param[in]   entry    Entry name
	 *  \param[out]  params   Configuration parameters
	 *
	 *  \retval true  if the entry was found
	 *  \retval false otherwise
	 */
	inline bool get(const std::string & entry, ConfigParamList & params) const
	{
		return get("", entry, params);
	}

	/**
	 *  \brief  Configuration parameters setter
	 *
	 *  The section and entry are created unless they already exist.
	 *
	 *  \param[in]  section  Section name
	 *  \param[in]  entry    Entry name
	 *  \param[in]  params   Configuration parameters
	 */
	void set(const std::string & section, const std::string & entry, const ConfigParamList & params);

	/**
	 *  \brief  Global scope configuration parameters setter
	 *
	 *  The entry is created unless it already exists.
	 *
	 *  \param[in]  entry    Entry name
	 *  \param[in]  params   Configuration parameters
	 */
	inline void set(const std::string & entry, const ConfigParamList & params)
	{
		set("", entry, params);
	}

	/**
	 *  \brief  Add configuration parameters
	 *
	 *  The section and entry are created unless they already exist.
	 *  Current parameters are kept, the provided are added to the list end.
	 *
	 *  \param[in]  section  Section name
	 *  \param[in]  entry    Entry name
	 *  \param[in]  params   Configuration parameters
	 */
	void add(const std::string & section, const std::string & entry, const ConfigParamList & params);

	/**
	 *  \brief  Add global scope configuration parameters
	 *
	 *  The entry is created unless they already exists.
	 *  Current parameters are kept, the provided are added to the list end.
	 *
	 *  \param[in]  entry    Entry name
	 *  \param[in]  params   Configuration parameters
	 */
	inline void add(const std::string & entry, const ConfigParamList & params)
	{
		add("", entry, params);
	}

	/**
	 *  \brief  Configuration parameters removal
	 *
	 *  Removes the entry, only.
	 *  Does nothing if the section or the entry don't exist.
	 *
	 *  \param  section  Section name
	 *  \param  entry    Entry name
	 */
	void remove(const std::string & section, const std::string & entry);

	/**
	 *  \brief  Global scope configuration parameters removal
	 *
	 *  Removes the entry, only.
	 *  Does nothing if the entry don't exist.
	 *
	 *  \param  entry    Entry name
	 */
	inline void remove(const std::string & entry)
	{
		remove("", entry);
	}

	/**
	 *  \brief  Configuration section removal
	 *
	 *  Removes entire section (if exists).
	 *
	 *  \param  section  Section name
	 */
	void removeSection(const std::string & section);

	/** Global scope configuration removal */
	inline void removeGlobal()
	{
		removeSection("");
	}

	/**
	 *  \brief  Configuration string getter
	 *
	 *  Empty string is returned if the section or entry doesn't exist.
	 *
	 *  \param  section  Section name
	 *  \param  entry    Entry name
	 *  \param  quoted   \c true iff the value is quoted
	 *
	 *  \return Configuration parameter as string
	 */
	std::string getStr(
		const std::string & section,
		const std::string & entry,
		bool                quoted = false) const;

	/**
	 *  \brief  Global scope configuration string getter
	 *
	 *  Empty string is returned if the entry doesn't exist.
	 *
	 *  \param  entry    Entry name
	 *  \param  quoted   \c true iff the value is quoted
	 *
	 *  \return Configuration parameter as string
	 */
	inline std::string getStr(const std::string & entry, bool quoted = false) const
	{
		return getStr("", entry, quoted);
	}

	/**
	 *  \brief  Configuration string setter
	 *
	 *  \param  section  Section name
	 *  \param  entry    Entry name
	 *  \param  value    Parameter value
	 *  \param  quoted   \c true iff the value is quoted
	 */
	void setStr(
		const std::string & section,
		const std::string & entry,
		const std::string & value,
		bool                quoted = false);

	/**
	 *  \brief  Global scope configuration string setter
	 *
	 *  \param  entry    Entry name
	 *  \param  value    Parameter value
	 *  \param  quoted   \c true iff the value is quoted
	 */
	inline void setStr(
		const std::string & entry,
		const std::string & value,
		bool                quoted = false)
	{
		setStr("", entry, value, quoted);
	}

	/**
	 *  \brief  Configuration number getter
	 *
	 *  \param  section  Section name
	 *  \param  entry    Entry name
	 *  \param  val      Default value
	 *
	 *  \return Configuration parameter as number (or the default if not defined)
	 */
	long long int getInt(
		const std::string & section,
		const std::string & entry,
		long long int       val = 0) const;

	/**
	 *  \brief  Global scope configuration number getter
	 *
	 *  \param  entry    Entry name
	 *  \param  val      Default value
	 *
	 *  \return Configuration parameter as number (or the default if not defined)
	 */
	inline long long int getInt(const std::string & entry, long long int val = 0) const
	{
		return getInt("", entry, val);
	}

	/**
	 *  \brief  Configuration number setter
	 *
	 *  \param  section  Section name
	 *  \param  entry    Entry name
	 *  \param  val      Default value
	 */
	void setInt(
		const std::string & section,
		const std::string & entry,
		long long int       val);

	/**
	 *  \brief  Global scope configuration number setter
	 *
	 *  \param  entry    Entry name
	 *  \param  val      Default value
	 */
	inline void setInt(
		const std::string & entry,
		long long int       val)
	{
		setInt("", entry, val);
	}

	/**
	 *  \brief  Cast numeric type with range check
	 *
	 *  Throws an exception on cast error.
	 *
	 *  \param  number  Number
	 *  \param  min     Minimum
	 *  \param  max     Maximum
	 *
	 *  \return \c number casted to target type
	 */
	template <typename T>
	static T range_cast(long long int number, long long int min, long long int max) throw(std::range_error)
	{
		if (number < min)
		{
			std::stringstream e;
			e << "Failed to range-cast " << number << " (underflows " << min << ')';

			throw std::range_error(e.str());
		}

		if (number > max)
		{
			std::stringstream e;
			e << "Failed to range-cast " << number << " (overflows " << max << ')';

			throw std::range_error(e.str());
		}

		return static_cast<T>(number);
	}

	/**
	 *  \brief  Resolve string as Boolean value
	 *
	 *  \param  str  String
	 *
	 *  \retval true  iff the string expresses a known true value
	 *  \retval false otherwise
	 */
	static bool str2bool(const std::string & str);

	/**
	 *  \brief  Convert boolean value to string
	 *
	 *  \param  val  Boolean value
	 *
	 *  \return \c vla as string
	 */
	static const std::string & bool2str(bool val);

};  // end of class GenericConfiguration



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


/** UPS configuration */
class UpsConfiguration : public GenericConfiguration
{
public:
	/** Global configuration attributes getters and setters \{ */

	inline std::string getChroot()     const { return getStr("chroot",     true); }
	inline std::string getDriverPath() const { return getStr("driverpath", true); }
	inline std::string getUser()       const { return getStr("user",       false); }

	inline long long int getMaxStartDelay() const { return getInt("maxstartdelay"); }
	inline long long int getPollInterval() const  { return getInt("pollinterval", 5); }  // TODO: check the default

	inline void setChroot(const std::string & path)     { setStr("chroot",     path, true); }
	inline void setDriverPath(const std::string & path) { setStr("driverpath", path, true); }
	inline void setUser(const std::string & user)       { setStr("user",       user, false); }

	inline void setMaxStartDelay(long long int delay)   { setInt("maxstartdelay", delay); }
	inline void setPollInterval(long long int interval) { setInt("pollinterval",  interval); }

	/** \} */

	/** UPS-specific configuration attributes getters and setters \{ */
	inline std::string getDriver(const std::string & ups)              const { return getStr(ups, "driver",              false); }
	inline std::string getDescription(const std::string & ups)         const { return getStr(ups, "desc",                true); }
	inline std::string getCP(const std::string & ups)                  const { return getStr(ups, "CP",                  false); }
	inline std::string getCS(const std::string & ups)                  const { return getStr(ups, "CS",                  false); }
	inline std::string getID(const std::string & ups)                  const { return getStr(ups, "ID",                  false); }
	inline std::string getLB(const std::string & ups)                  const { return getStr(ups, "LB",                  false); }
	inline std::string getLowBatt(const std::string & ups)             const { return getStr(ups, "LowBatt",             false); }  // CHECKME
	inline std::string getOL(const std::string & ups)                  const { return getStr(ups, "OL",                  false); }  // CHECKME
	inline std::string getSD(const std::string & ups)                  const { return getStr(ups, "SD",                  false); }  // CHECKME
	inline std::string getAuthPassword(const std::string & ups)        const { return getStr(ups, "authPassword",        false); }  // CHECKME
	inline std::string getAuthProtocol(const std::string & ups)        const { return getStr(ups, "authProtocol",        false); }  // CHECKME
	inline std::string getAuthType(const std::string & ups)            const { return getStr(ups, "authtype",            false); }  // CHECKME
	inline std::string getAWD(const std::string & ups)                 const { return getStr(ups, "awd",                 false); }  // CHECKME
	inline std::string getBatText(const std::string & ups)             const { return getStr(ups, "battext",             true); }   // CHECKME
	inline std::string getBus(const std::string & ups)                 const { return getStr(ups, "bus",                 false); }  // CHECKME
	inline std::string getCommunity(const std::string & ups)           const { return getStr(ups, "community",           false); }  // CHECKME
	inline std::string getFRUID(const std::string & ups)               const { return getStr(ups, "fruid",               false); }  // CHECKME
	inline std::string getLoadStatus(const std::string & ups)          const { return getStr(ups, "load.status",         false); }  // CHECKME
	inline std::string getLogin(const std::string & ups)               const { return getStr(ups, "login",               false); }  // CHECKME
	inline std::string getLowbatt(const std::string & ups)             const { return getStr(ups, "lowbatt",             false); }  // CHECKME
	inline std::string getManufacturer(const std::string & ups)        const { return getStr(ups, "manufacturer",        true); }   // CHECKME
	inline std::string getMethodOfFlowControl(const std::string & ups) const { return getStr(ups, "methodOfFlowControl", false); }  // CHECKME
	inline std::string getMIBs(const std::string & ups)                const { return getStr(ups, "mibs",                false); }  // CHECKME
	inline std::string getModel(const std::string & ups)               const { return getStr(ups, "model",               true); }   // CHECKME
	inline std::string getModelName(const std::string & ups)           const { return getStr(ups, "modelname",           true); }   // CHECKME
	inline std::string getNotification(const std::string & ups)        const { return getStr(ups, "notification",        false); }  // CHECKME
	inline std::string getOldMAC(const std::string & ups)              const { return getStr(ups, "oldmac",              false); }  // CHECKME
	inline std::string getPassword(const std::string & ups)            const { return getStr(ups, "password",            false); }  // CHECKME
	inline std::string getPrefix(const std::string & ups)              const { return getStr(ups, "prefix",              true); }   // CHECKME
	inline std::string getPrivPassword(const std::string & ups)        const { return getStr(ups, "privPassword",        false); }  // CHECKME
	inline std::string getPrivProtocol(const std::string & ups)        const { return getStr(ups, "privProtocol",        false); }  // CHECKME
	inline std::string getProduct(const std::string & ups)             const { return getStr(ups, "product",             true); }   // CHECKME
	inline std::string getProductID(const std::string & ups)           const { return getStr(ups, "productid",           false); }  // CHECKME
	inline std::string getProtocol(const std::string & ups)            const { return getStr(ups, "protocol",            false); }  // CHECKME
	inline std::string getRuntimeCal(const std::string & ups)          const { return getStr(ups, "runtimecal",          false); }  // CHECKME
	inline std::string getSDType(const std::string & ups)              const { return getStr(ups, "sdtype",              false); }  // CHECKME
	inline std::string getSecLevel(const std::string & ups)            const { return getStr(ups, "secLevel",            false); }  // CHECKME
	inline std::string getSecName(const std::string & ups)             const { return getStr(ups, "secName",             true); }   // CHECKME
	inline std::string getSensorID(const std::string & ups)            const { return getStr(ups, "sensorid",            false); }  // CHECKME
	inline std::string getSerial(const std::string & ups)              const { return getStr(ups, "serial",              false); }  // CHECKME
	inline std::string getSerialNumber(const std::string & ups)        const { return getStr(ups, "serialnumber",        false); }  // CHECKME
	inline std::string getShutdownArguments(const std::string & ups)   const { return getStr(ups, "shutdownArguments",   false); }  // CHECKME
	inline std::string getSNMPversion(const std::string & ups)         const { return getStr(ups, "snmp_version",        false); }  // CHECKME
	inline std::string getSubdriver(const std::string & ups)           const { return getStr(ups, "subdriver",           false); }  // CHECKME
	inline std::string getType(const std::string & ups)                const { return getStr(ups, "type",                false); }  // CHECKME
	inline std::string getUPStype(const std::string & ups)             const { return getStr(ups, "upstype",             false); }  // CHECKME
	inline std::string getUSD(const std::string & ups)                 const { return getStr(ups, "usd",                 false); }  // CHECKME
	inline std::string getUsername(const std::string & ups)            const { return getStr(ups, "username",            false); }  // CHECKME
	inline std::string getValidationSequence(const std::string & ups)  const { return getStr(ups, "validationSequence",  false); }  // CHECKME
	inline std::string getVendor(const std::string & ups)              const { return getStr(ups, "vendor",              true); }   // CHECKME
	inline std::string getVendorID(const std::string & ups)            const { return getStr(ups, "vendorid",            false); }  // CHECKME
	inline std::string getWUGrace(const std::string & ups)             const { return getStr(ups, "wugrace",             false); }  // CHECKME


	inline uint16_t      getPort(const std::string & ups) const { return range_cast<uint16_t>(getInt(ups, "port"), 0, 65535); }  // TBD:  Any default?
	inline long long int getSDOrder(const std::string & ups)           const { return getInt(ups, "sdorder"); }             // TODO: Is that a number?
	inline long long int getMaxStartDelay(const std::string & ups)     const { return getInt(ups, "maxstartdelay"); }
	inline long long int getAdvOrder(const std::string & ups)          const { return getInt(ups, "advorder"); }            // CHECKME
	inline long long int getBatteryPercentage(const std::string & ups) const { return getInt(ups, "batteryPercentage"); }   // CHECKME
	inline long long int getOffDelay(const std::string & ups)          const { return getInt(ups, "OffDelay"); }            // CHECKME
	inline long long int getOnDelay(const std::string & ups)           const { return getInt(ups, "OnDelay"); }             // CHECKME
	inline long long int getBattVoltMult(const std::string & ups)      const { return getInt(ups, "battvoltmult"); }        // CHECKME
	inline long long int getBaudRate(const std::string & ups)          const { return getInt(ups, "baud_rate"); }           // CHECKME
	inline long long int getBaudrate(const std::string & ups)          const { return getInt(ups, "baudrate"); }            // CHECKME
	inline long long int getCablePower(const std::string & ups)        const { return getInt(ups, "cablepower"); }          // CHECKME
	inline long long int getChargeTime(const std::string & ups)        const { return getInt(ups, "chargetime"); }          // CHECKME
	inline long long int getDaysOff(const std::string & ups)           const { return getInt(ups, "daysoff"); }             // CHECKME
	inline long long int getDaySweek(const std::string & ups)          const { return getInt(ups, "daysweek"); }            // CHECKME
	inline long long int getFrequency(const std::string & ups)         const { return getInt(ups, "frequency"); }           // CHECKME
	inline long long int getHourOff(const std::string & ups)           const { return getInt(ups, "houroff"); }             // CHECKME
	inline long long int getHourOn(const std::string & ups)            const { return getInt(ups, "houron"); }              // CHECKME
	inline long long int getIdleLoad(const std::string & ups)          const { return getInt(ups, "idleload"); }            // CHECKME
	inline long long int getInputTimeout(const std::string & ups)      const { return getInt(ups, "input_timeout"); }       // CHECKME
	inline long long int getLineVoltage(const std::string & ups)       const { return getInt(ups, "linevoltage"); }         // CHECKME
	inline long long int getLoadpercentage(const std::string & ups)    const { return getInt(ups, "loadPercentage"); }      // CHECKME
	inline long long int getMaxLoad(const std::string & ups)           const { return getInt(ups, "max_load"); }            // CHECKME
	inline long long int getMFR(const std::string & ups)               const { return getInt(ups, "mfr"); }                 // CHECKME
	inline long long int getMinCharge(const std::string & ups)         const { return getInt(ups, "mincharge"); }           // CHECKME
	inline long long int getMinRuntime(const std::string & ups)        const { return getInt(ups, "minruntime"); }          // CHECKME
	inline long long int getNomBattVolt(const std::string & ups)       const { return getInt(ups, "nombattvolt"); }         // CHECKME
	inline long long int getNumOfBytesFromUPS(const std::string & ups) const { return getInt(ups, "numOfBytesFromUPS"); }   // CHECKME
	inline long long int getOffdelay(const std::string & ups)          const { return getInt(ups, "offdelay"); }            // CHECKME
	inline long long int getOndelay(const std::string & ups)           const { return getInt(ups, "ondelay"); }             // CHECKME
	inline long long int getOutputPace(const std::string & ups)        const { return getInt(ups, "output_pace"); }         // CHECKME
	inline long long int getPollFreq(const std::string & ups)          const { return getInt(ups, "pollfreq"); }            // CHECKME
	inline long long int getPowerUp(const std::string & ups)           const { return getInt(ups, "powerup"); }             // CHECKME
	inline long long int getPrgShut(const std::string & ups)           const { return getInt(ups, "prgshut"); }             // CHECKME
	inline long long int getRebootDelay(const std::string & ups)       const { return getInt(ups, "rebootdelay"); }         // CHECKME
	inline long long int getSDtime(const std::string & ups)            const { return getInt(ups, "sdtime"); }              // CHECKME
	inline long long int getShutdownDelay(const std::string & ups)     const { return getInt(ups, "shutdown_delay"); }      // CHECKME
	inline long long int getStartDelay(const std::string & ups)        const { return getInt(ups, "startdelay"); }          // CHECKME
	inline long long int getTestTime(const std::string & ups)          const { return getInt(ups, "testtime"); }            // CHECKME
	inline long long int getTimeout(const std::string & ups)           const { return getInt(ups, "timeout"); }             // CHECKME
	inline long long int getUPSdelayShutdown(const std::string & ups)  const { return getInt(ups, "ups.delay.shutdown"); }  // CHECKME
	inline long long int getUPSdelayStart(const std::string & ups)     const { return getInt(ups, "ups.delay.start"); }     // CHECKME
	inline long long int getVoltage(const std::string & ups)           const { return getInt(ups, "voltage"); }             // CHECKME
	inline long long int getWait(const std::string & ups)              const { return getInt(ups, "wait"); }                // CHECKME

	inline bool getNolock(const std::string & ups)         const { return str2bool(getStr(ups, "nolock",         false)); }  // TODO: check whether it's indeed boolean
	inline bool getCable(const std::string & ups)          const { return str2bool(getStr(ups, "cable",          false)); }  // CHECKME
	inline bool getDumbTerm(const std::string & ups)       const { return str2bool(getStr(ups, "dumbterm",       false)); }  // CHECKME
	inline bool getExplore(const std::string & ups)        const { return str2bool(getStr(ups, "explore",        false)); }  // CHECKME
	inline bool getFakeLowBatt(const std::string & ups)    const { return str2bool(getStr(ups, "fake_lowbatt",   false)); }  // CHECKME
	inline bool getFlash(const std::string & ups)          const { return str2bool(getStr(ups, "flash",          false)); }  // CHECKME
	inline bool getFullUpdate(const std::string & ups)     const { return str2bool(getStr(ups, "full_update",    false)); }  // CHECKME
	inline bool getLangIDfix(const std::string & ups)      const { return str2bool(getStr(ups, "langid_fix",     false)); }  // CHECKME
	inline bool getLoadOff(const std::string & ups)        const { return str2bool(getStr(ups, "load.off",       false)); }  // CHECKME
	inline bool getLoadOn(const std::string & ups)         const { return str2bool(getStr(ups, "load.on",        false)); }  // CHECKME
	inline bool getNoHang(const std::string & ups)         const { return str2bool(getStr(ups, "nohang",         false)); }  // CHECKME
	inline bool getNoRating(const std::string & ups)       const { return str2bool(getStr(ups, "norating",       false)); }  // CHECKME
	inline bool getNoTransferOIDs(const std::string & ups) const { return str2bool(getStr(ups, "notransferoids", false)); }  // CHECKME
	inline bool getNoVendor(const std::string & ups)       const { return str2bool(getStr(ups, "novendor",       false)); }  // CHECKME
	inline bool getNoWarnNoImp(const std::string & ups)    const { return str2bool(getStr(ups, "nowarn_noimp",   false)); }  // CHECKME
	inline bool getPollOnly(const std::string & ups)       const { return str2bool(getStr(ups, "pollonly",       false)); }  // CHECKME
	inline bool getSilent(const std::string & ups)         const { return str2bool(getStr(ups, "silent",         false)); }  // CHECKME
	inline bool getStatusOnly(const std::string & ups)     const { return str2bool(getStr(ups, "status_only",    false)); }  // CHECKME
	inline bool getSubscribe(const std::string & ups)      const { return str2bool(getStr(ups, "subscribe",      false)); }  // CHECKME
	inline bool getUseCRLF(const std::string & ups)        const { return str2bool(getStr(ups, "use_crlf",       false)); }  // CHECKME
	inline bool getUsePreLF(const std::string & ups)       const { return str2bool(getStr(ups, "use_pre_lf",     false)); }  // CHECKME


	inline void setDriver(const std::string & ups, const std::string & driver)                { setStr(ups, "driver",              driver,       false); }
	inline void setDescription(const std::string & ups, const std::string & desc)             { setStr(ups, "desc",                desc,         true); }
	inline void setLowBatt(const std::string & ups, const std::string & lowbatt)              { setStr(ups, "LowBatt",             lowbatt,      false); }  // CHECKME
	inline void setOL(const std::string & ups, const std::string & ol)                        { setStr(ups, "OL",                  ol,           false); }  // CHECKME
	inline void setSD(const std::string & ups, const std::string & sd)                        { setStr(ups, "SD",                  sd,           false); }  // CHECKME
	inline void setAuthPassword(const std::string & ups, const std::string & auth_passwd)     { setStr(ups, "authPassword",        auth_passwd,  false); }  // CHECKME
	inline void setAuthProtocol(const std::string & ups, const std::string & auth_proto)      { setStr(ups, "authProtocol",        auth_proto,   false); }  // CHECKME
	inline void setAuthType(const std::string & ups, const std::string & authtype)            { setStr(ups, "authtype",            authtype,     false); }  // CHECKME
	inline void setAWD(const std::string & ups, const std::string & awd)                      { setStr(ups, "awd",                 awd,          false); }  // CHECKME
	inline void setBatText(const std::string & ups, const std::string & battext)              { setStr(ups, "battext",             battext,      false); }  // CHECKME
	inline void setBus(const std::string & ups, const std::string & bus)                      { setStr(ups, "bus",                 bus,          false); }  // CHECKME
	inline void setCommunity(const std::string & ups, const std::string & community)          { setStr(ups, "community",           community,    false); }  // CHECKME
	inline void setFRUID(const std::string & ups, const std::string & fruid)                  { setStr(ups, "fruid",               fruid,        false); }  // CHECKME
	inline void setLoadStatus(const std::string & ups, const std::string & load_status)       { setStr(ups, "load.status",         load_status,  false); }  // CHECKME
	inline void setLogin(const std::string & ups, const std::string & login)                  { setStr(ups, "login",               login,        false); }  // CHECKME
	inline void setLowbatt(const std::string & ups, const std::string & lowbatt)              { setStr(ups, "lowbatt",             lowbatt,      false); }  // CHECKME
	inline void setManufacturer(const std::string & ups, const std::string & manufacturer)    { setStr(ups, "manufacturer",        manufacturer, false); }  // CHECKME
	inline void setMethodOfFlowControl(const std::string & ups, const std::string & method)   { setStr(ups, "methodOfFlowControl", method,       false); }  // CHECKME
	inline void setMIBs(const std::string & ups, const std::string & mibs)                    { setStr(ups, "mibs",                mibs,         false); }  // CHECKME
	inline void setModel(const std::string & ups, const std::string & model)                  { setStr(ups, "model",               model,        false); }  // CHECKME
	inline void setModelName(const std::string & ups, const std::string & modelname)          { setStr(ups, "modelname",           modelname,    false); }  // CHECKME
	inline void setNotification(const std::string & ups, const std::string & notification)    { setStr(ups, "notification",        notification, false); }  // CHECKME
	inline void setOldMAC(const std::string & ups, const std::string & oldmac)                { setStr(ups, "oldmac",              oldmac,       false); }  // CHECKME
	inline void setPassword(const std::string & ups, const std::string & password)            { setStr(ups, "password",            password,     false); }  // CHECKME
	inline void setPrefix(const std::string & ups, const std::string & prefix)                { setStr(ups, "prefix",              prefix,       false); }  // CHECKME
	inline void setPrivPassword(const std::string & ups, const std::string & priv_passwd)     { setStr(ups, "privPassword",        priv_passwd,  false); }  // CHECKME
	inline void setPrivProtocol(const std::string & ups, const std::string & priv_proto)      { setStr(ups, "privProtocol",        priv_proto,   false); }  // CHECKME
	inline void setProduct(const std::string & ups, const std::string & product)              { setStr(ups, "product",             product,      false); }  // CHECKME
	inline void setProductID(const std::string & ups, const std::string & productid)          { setStr(ups, "productid",           productid,    false); }  // CHECKME
	inline void setProtocol(const std::string & ups, const std::string & protocol)            { setStr(ups, "protocol",            protocol,     false); }  // CHECKME
	inline void setRuntimeCal(const std::string & ups, const std::string & runtimecal)        { setStr(ups, "runtimecal",          runtimecal,   false); }  // CHECKME
	inline void setSDtype(const std::string & ups, const std::string & sdtype)                { setStr(ups, "sdtype",              sdtype,       false); }  // CHECKME
	inline void setSecLevel(const std::string & ups, const std::string & sec_level)           { setStr(ups, "secLevel",            sec_level,    false); }  // CHECKME
	inline void setSecName(const std::string & ups, const std::string & sec_name)             { setStr(ups, "secName",             sec_name,     false); }  // CHECKME
	inline void setSensorID(const std::string & ups, const std::string & sensorid)            { setStr(ups, "sensorid",            sensorid,     false); }  // CHECKME
	inline void setSerial(const std::string & ups, const std::string & serial)                { setStr(ups, "serial",              serial,       false); }  // CHECKME
	inline void setSerialNumber(const std::string & ups, const std::string & serialnumber)    { setStr(ups, "serialnumber",        serialnumber, false); }  // CHECKME
	inline void setShutdownArguments(const std::string & ups, const std::string & sd_args)    { setStr(ups, "shutdownArguments",   sd_args,      false); }  // CHECKME
	inline void setSNMPversion(const std::string & ups, const std::string & snmp_version)     { setStr(ups, "snmp_version",        snmp_version, false); }  // CHECKME
	inline void setSubdriver(const std::string & ups, const std::string & subdriver)          { setStr(ups, "subdriver",           subdriver,    false); }  // CHECKME
	inline void setType(const std::string & ups, const std::string & type)                    { setStr(ups, "type",                type,         false); }  // CHECKME
	inline void setUPStype(const std::string & ups, const std::string & upstype)              { setStr(ups, "upstype",             upstype,      false); }  // CHECKME
	inline void setUSD(const std::string & ups, const std::string & usd)                      { setStr(ups, "usd",                 usd,          false); }  // CHECKME
	inline void setUsername(const std::string & ups, const std::string & username)            { setStr(ups, "username",            username,     false); }  // CHECKME
	inline void setValidationSequence(const std::string & ups, const std::string & valid_seq) { setStr(ups, "validationSequence",  valid_seq,    false); }  // CHECKME
	inline void setVendor(const std::string & ups, const std::string & vendor)                { setStr(ups, "vendor",              vendor,       false); }  // CHECKME
	inline void setVendorID(const std::string & ups, const std::string & vendorid)            { setStr(ups, "vendorid",            vendorid,     false); }  // CHECKME
	inline void setWUGrace(const std::string & ups, const std::string & wugrace)              { setStr(ups, "wugrace",             wugrace,      false); }  // CHECKME

	inline void setPort(const std::string & ups, uint16_t port)                    { setInt(ups, "port",               port); }
	inline void setSDOrder(const std::string & ups, long long int ord)             { setInt(ups, "sdorder",            ord); }
	inline void setMaxStartDelay(const std::string & ups, long long int delay)     { setInt(ups, "maxstartdelay",      delay); }
	inline void setADVorder(const std::string & ups, long long int advorder)       { setInt(ups, "advorder",           advorder); }     // CHECKME
	inline void setBatteryPercentage(const std::string & ups, long long int batt)  { setInt(ups, "batteryPercentage",  batt); }         // CHECKME
	inline void setOffDelay(const std::string & ups, long long int offdelay)       { setInt(ups, "OffDelay",           offdelay); }     // CHECKME
	inline void setOnDelay(const std::string & ups, long long int ondelay)         { setInt(ups, "OnDelay",            ondelay); }      // CHECKME
	inline void setBattVoltMult(const std::string & ups, long long int mult)       { setInt(ups, "battvoltmult",       mult); }         // CHECKME
	inline void setBaudRate(const std::string & ups, long long int baud_rate)      { setInt(ups, "baud_rate",          baud_rate); }    // CHECKME
	inline void setBaudrate(const std::string & ups, long long int baudrate)       { setInt(ups, "baudrate",           baudrate); }     // CHECKME
	inline void setCablePower(const std::string & ups, long long int cablepower)   { setInt(ups, "cablepower",         cablepower); }   // CHECKME
	inline void setChargeTime(const std::string & ups, long long int chargetime)   { setInt(ups, "chargetime",         chargetime); }   // CHECKME
	inline void setDaysOff(const std::string & ups, long long int daysoff)         { setInt(ups, "daysoff",            daysoff); }      // CHECKME
	inline void setDaysWeek(const std::string & ups, long long int daysweek)       { setInt(ups, "daysweek",           daysweek); }     // CHECKME
	inline void setFrequency(const std::string & ups, long long int frequency)     { setInt(ups, "frequency",          frequency); }    // CHECKME
	inline void setHourOff(const std::string & ups, long long int houroff)         { setInt(ups, "houroff",            houroff); }      // CHECKME
	inline void setHourOn(const std::string & ups, long long int houron)           { setInt(ups, "houron",             houron); }       // CHECKME
	inline void setIdleLoad(const std::string & ups, long long int idleload)       { setInt(ups, "idleload",           idleload); }     // CHECKME
	inline void setInputTimeout(const std::string & ups, long long int timeout)    { setInt(ups, "input_timeout",      timeout); }      // CHECKME
	inline void setLineVoltage(const std::string & ups, long long int linevoltage) { setInt(ups, "linevoltage",        linevoltage); }  // CHECKME
	inline void setLoadpercentage(const std::string & ups, long long int load)     { setInt(ups, "loadPercentage",     load); }         // CHECKME
	inline void setMaxLoad(const std::string & ups, long long int max_load)        { setInt(ups, "max_load",           max_load); }     // CHECKME
	inline void setMFR(const std::string & ups, long long int mfr)                 { setInt(ups, "mfr",                mfr); }          // CHECKME
	inline void setMinCharge(const std::string & ups, long long int mincharge)     { setInt(ups, "mincharge",          mincharge); }    // CHECKME
	inline void setMinRuntime(const std::string & ups, long long int minruntime)   { setInt(ups, "minruntime",         minruntime); }   // CHECKME
	inline void setNomBattVolt(const std::string & ups, long long int nombattvolt) { setInt(ups, "nombattvolt",        nombattvolt); }  // CHECKME
	inline void setNumOfBytesFromUPS(const std::string & ups, long long int bytes) { setInt(ups, "numOfBytesFromUPS",  bytes); }        // CHECKME
	inline void setOffdelay(const std::string & ups, long long int offdelay)       { setInt(ups, "offdelay",           offdelay); }     // CHECKME
	inline void setOndelay(const std::string & ups, long long int ondelay)         { setInt(ups, "ondelay",            ondelay); }      // CHECKME
	inline void setOutputPace(const std::string & ups, long long int output_pace)  { setInt(ups, "output_pace",        output_pace); }  // CHECKME
	inline void setPollFreq(const std::string & ups, long long int pollfreq)       { setInt(ups, "pollfreq",           pollfreq); }     // CHECKME
	inline void setPowerUp(const std::string & ups, long long int powerup)         { setInt(ups, "powerup",            powerup); }      // CHECKME
	inline void setPrgShut(const std::string & ups, long long int prgshut)         { setInt(ups, "prgshut",            prgshut); }      // CHECKME
	inline void setRebootDelay(const std::string & ups, long long int delay)       { setInt(ups, "rebootdelay",        delay); }        // CHECKME
	inline void setSDtime(const std::string & ups, long long int sdtime)           { setInt(ups, "sdtime",             sdtime); }       // CHECKME
	inline void setShutdownDelay(const std::string & ups, long long int delay)     { setInt(ups, "shutdown_delay",     delay); }        // CHECKME
	inline void setStartDelay(const std::string & ups, long long int delay)        { setInt(ups, "startdelay",         delay); }        // CHECKME
	inline void setTestTime(const std::string & ups, long long int testtime)       { setInt(ups, "testtime",           testtime); }     // CHECKME
	inline void setTimeout(const std::string & ups, long long int timeout)         { setInt(ups, "timeout",            timeout); }      // CHECKME
	inline void setUPSdelayShutdown(const std::string & ups, long long int delay)  { setInt(ups, "ups.delay.shutdown", delay); }        // CHECKME
	inline void setUPSdelayStart(const std::string & ups, long long int delay)     { setInt(ups, "ups.delay.start",    delay); }        // CHECKME
	inline void setVoltage(const std::string & ups, long long int voltage)         { setInt(ups, "voltage",            voltage); }      // CHECKME
	inline void setWait(const std::string & ups, long long int wait)               { setInt(ups, "wait",               wait); }         // CHECKME

	inline void setNolock(const std::string & ups, bool set = true)         { setStr(ups, "nolock",         bool2str(set), false); }
	inline void setCable(const std::string & ups, bool set = true)          { setStr(ups, "cable",          bool2str(set), false); }  // CHECKME
	inline void setDumbTerm(const std::string & ups, bool set = true)       { setStr(ups, "dumbterm",       bool2str(set), false); }  // CHECKME
	inline void setExplore(const std::string & ups, bool set = true)        { setStr(ups, "explore",        bool2str(set), false); }  // CHECKME
	inline void setFakeLowBatt(const std::string & ups, bool set = true)    { setStr(ups, "fake_lowbatt",   bool2str(set), false); }  // CHECKME
	inline void setFlash(const std::string & ups, bool set = true)          { setStr(ups, "flash",          bool2str(set), false); }  // CHECKME
	inline void setFullUpdate(const std::string & ups, bool set = true)     { setStr(ups, "full_update",    bool2str(set), false); }  // CHECKME
	inline void setLangIDfix(const std::string & ups, bool set = true)      { setStr(ups, "langid_fix",     bool2str(set), false); }  // CHECKME
	inline void setLoadOff(const std::string & ups, bool set = true)        { setStr(ups, "load.off",       bool2str(set), false); }  // CHECKME
	inline void setLoadOn(const std::string & ups, bool set = true)         { setStr(ups, "load.on",        bool2str(set), false); }  // CHECKME
	inline void setNoHang(const std::string & ups, bool set = true)         { setStr(ups, "nohang",         bool2str(set), false); }  // CHECKME
	inline void setNoRating(const std::string & ups, bool set = true)       { setStr(ups, "norating",       bool2str(set), false); }  // CHECKME
	inline void setNoTransferOIDs(const std::string & ups, bool set = true) { setStr(ups, "notransferoids", bool2str(set), false); }  // CHECKME
	inline void setNoVendor(const std::string & ups, bool set = true)       { setStr(ups, "novendor",       bool2str(set), false); }  // CHECKME
	inline void setNoWarnNoImp(const std::string & ups, bool set = true)    { setStr(ups, "nowarn_noimp",   bool2str(set), false); }  // CHECKME
	inline void setPollOnly(const std::string & ups, bool set = true)       { setStr(ups, "pollonly",       bool2str(set), false); }  // CHECKME
	inline void setSilent(const std::string & ups, bool set = true)         { setStr(ups, "silent",         bool2str(set), false); }  // CHECKME
	inline void setStatusOnly(const std::string & ups, bool set = true)     { setStr(ups, "status_only",    bool2str(set), false); }  // CHECKME
	inline void setSubscribe(const std::string & ups, bool set = true)      { setStr(ups, "subscribe",      bool2str(set), false); }  // CHECKME
	inline void setUseCRLF(const std::string & ups, bool set = true)        { setStr(ups, "use_crlf",       bool2str(set), false); }  // CHECKME
	inline void setUsePreLF(const std::string & ups, bool set = true)       { setStr(ups, "use_pre_lf",     bool2str(set), false); }  // CHECKME

	/** \} */

};  // end of class UpsConfiguration


/** upsd users configuration */
class UpsdUsersConfiguration : public GenericConfiguration
{
public:
	/** User-specific configuration attributes getters and setters \{ */

	inline std::string getPassword(const std::string & user) const { return getStr(user, "password", false); }

	inline ConfigParamList getActions(const std::string & user) const
	{
		ConfigParamList actions;
		get(user, "actions", actions);
		return actions;
	}

	inline ConfigParamList getInstantCommands(const std::string & user) const
	{
		ConfigParamList cmds;
		get(user, "instcmds", cmds);
		return cmds;
	}

	inline void setPassword(const std::string & user, const std::string & passwd) { setStr(user, "password", passwd, false); }

	inline void setActions(const std::string & user, const ConfigParamList & actions)      { set(user, "actions",  actions); }
	inline void setInstantCommands(const std::string & user, const ConfigParamList & cmds) { set(user, "instcmds", cmds); }

	inline void addActions(const std::string & user, const ConfigParamList & actions)      { add(user, "actions",  actions); }
	inline void addInstantCommands(const std::string & user, const ConfigParamList & cmds) { add(user, "instcmds", cmds); }

	/** \} */

};  // end of class UpsdUsersConfiguration

} /* namespace nut */
#endif /* __cplusplus */
#endif	/* NUTCONF_H_SEEN */
