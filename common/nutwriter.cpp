/* nutwriter.cpp - NUT writer

   Copyright (C)
        2012	Vaclav Krpec  <VaclavKrpec@Eaton.com>

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

#include "nutwriter.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include <cassert>


/**
 *  \brief  NUT configuration directive generator
 *
 *  The macro is used to simplify generation of
 *  NUT config. directives.
 *
 *  IMPORTANT NOTE:
 *  In case of writing error, the macro causes immediate
 *  return from the calling function (propagating the writing status).
 *
 *  \param  name       Directive name
 *  \param  arg_t      Directive argument implementation type
 *  \param  arg        Directive argument
 *  \param  quote_arg  Boolean flag; check to quote the argument
 */
#define CONFIG_DIRECTIVEX(name, arg_t, arg, quote_arg) \
	do { \
		if ((arg).set()) { \
			const arg_t & arg_val = (arg); \
			std::stringstream ss; \
			ss << name << ' '; \
			if (quote_arg) \
				ss << '"'; \
			ss << arg_val; \
			if (quote_arg) \
				ss << '"'; \
			status_t status = writeDirective(ss.str()); \
			if (NUTW_OK != status) \
				return status; \
		} \
	} while (0)


namespace nut {

// End-of-Line separators (arch. dependent)

/** UNIX style EoL */
static const std::string LF("\n");

// TODO: Make a compile-time selection
#if (0)
// M$ Windows EoL
static const std::string CRLF("\r\n");

// Apple MAC EoL
static const std::string CR("\r");
#endif  // end of #if (0)


const std::string & NutWriter::eol(LF);

const std::string GenericConfigWriter::s_default_section_entry_indent("\t");
const std::string GenericConfigWriter::s_default_section_entry_separator(" = ");


NutWriter::status_t NutWriter::writeEachLine(const std::string & str, const std::string & pref) {
	for (size_t pos = 0; pos < str.size(); ) {
		// Prefix every line
		status_t status = write(pref);

		if (NUTW_OK != status)
			return status;

		// Write up to the next EoL (or till the end)
		size_t eol_pos = str.find(eol, pos);

		if (str.npos == eol_pos)
			return write(str.substr(pos) + eol);

		eol_pos += eol.size();

		status = write(str.substr(pos, eol_pos));

		if (NUTW_OK != status)
			return status;

		// Update position
		pos = eol_pos;
	}

	return NUTW_OK;
}


NutWriter::status_t SectionlessConfigWriter::writeDirective(const std::string & str) {
	return write(str + eol);
}


NutWriter::status_t SectionlessConfigWriter::writeComment(const std::string & str) {
	return writeEachLine(str, "# ");
}


NutWriter::status_t SectionlessConfigWriter::writeSectionName(const std::string & name) {
	std::string e("INTERNAL ERROR: Attempt to write section name ");
	e += name + " to a section-less configuration file";

	throw std::logic_error(e);
}


NutWriter::status_t NutConfConfigWriter::writeConfig(const NutConfiguration & config) {
	status_t status;

	// Mode
	// TBD: How should I serialise an unknown mode?
	if (config.mode.set()) {
		std::string mode_str;

		NutConfiguration::NutMode mode = config.mode;

		switch (mode) {
			case NutConfiguration::MODE_UNKNOWN:
				// BEWARE!  Intentional fall-through to MODE_NONE branch

			case NutConfiguration::MODE_NONE:
				mode_str = "none";
				break;

			case NutConfiguration::MODE_STANDALONE:
				mode_str = "standalone";
				break;

			case NutConfiguration::MODE_NETSERVER:
				mode_str = "netserver";
				break;

			case NutConfiguration::MODE_NETCLIENT:
				mode_str = "netclient";
				break;

			case NutConfiguration::MODE_CONTROLLED:
				mode_str = "controlled";
				break;

			case NutConfiguration::MODE_MANUAL:
				mode_str = "manual";
				break;
		}

		status = writeDirective("MODE=" + mode_str);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


/** Notify types & flags strings */
struct NotifyFlagsStrings {
	// TBD: Shouldn't this mapping be shared with the parser?
	// This is an obvious redundancy...

	/** Notify type strings list */
	typedef const char * TypeStrings[UpsmonConfiguration::NOTIFY_TYPE_MAX];

	/** Notify flag strings map */
	typedef std::map<UpsmonConfiguration::NotifyFlag, const char *> FlagStrings;

	/** Notify type strings */
	static const TypeStrings type_str;

	/** Notify flag strings */
	static const FlagStrings flag_str;

	/**
	 *  \brief  Initialise notify flag strings
	 *
	 *  \return Notify flag strings map
	 */
	static FlagStrings initFlagStrings() {
		FlagStrings str;

		str[UpsmonConfiguration::NOTIFY_IGNORE] = "IGNORE";
		str[UpsmonConfiguration::NOTIFY_SYSLOG] = "SYSLOG";
		str[UpsmonConfiguration::NOTIFY_WALL]   = "WALL";
		str[UpsmonConfiguration::NOTIFY_EXEC]   = "EXEC";

		return str;
	}

};  // end of struct NotifyFlagsStrings


const NotifyFlagsStrings::TypeStrings NotifyFlagsStrings::type_str = {
	"ONLINE",	// NOTIFY_ONLINE
	"ONBATT",	// NOTIFY_ONBATT
	"LOWBATT",	// NOTIFY_LOWBATT
	"FSD\t",	// NOTIFY_FSD (including padding)
	"COMMOK",	// NOTIFY_COMMOK
	"COMMBAD",	// NOTIFY_COMMBAD
	"SHUTDOWN",	// NOTIFY_SHUTDOWN
	"REPLBATT",	// NOTIFY_REPLBATT
	"NOCOMM",	// NOTIFY_NOCOMM
	"NOPARENT",	// NOTIFY_NOPARENT
};


const NotifyFlagsStrings::FlagStrings NotifyFlagsStrings::flag_str =
	NotifyFlagsStrings::initFlagStrings();


/**
 *  \brief  upsmon notify flags serialiser
 *
 *  \param  type   Notification type
 *  \param  flags  Notification flags
 *
 *  \return NOTIFYFLAG directive string
 */
static std::string serialiseNotifyFlags(UpsmonConfiguration::NotifyType type, unsigned short flags) {
	static const NotifyFlagsStrings::FlagStrings::const_iterator ignore_str_iter =
		NotifyFlagsStrings::flag_str.find(UpsmonConfiguration::NOTIFY_IGNORE);

	static const std::string ignore_str(ignore_str_iter->second);

	assert(type < UpsmonConfiguration::NOTIFY_TYPE_MAX);

	std::string directive("NOTIFYFLAG ");

	directive += NotifyFlagsStrings::type_str[type];

	char separator = '\t';

	// The IGNORE flag is actually no-flag case
	if (UpsmonConfiguration::NOTIFY_IGNORE == flags) {
		directive += separator;
		directive += ignore_str;

		return directive;
	}

	NotifyFlagsStrings::FlagStrings::const_iterator fl_iter =
		NotifyFlagsStrings::flag_str.begin();

	for (; fl_iter != NotifyFlagsStrings::flag_str.end(); ++fl_iter) {
		if (fl_iter->first & flags) {
			directive += separator;
			directive += fl_iter->second;

			separator = '+';
		}
	}

	return directive;
}


/**
 *  \brief  upsmon notify messages serialiser
 *
 *  \param  type   Notification type
 *  \param  msg    Notification message
 *
 *  \return NOTIFYMSG directive string
 */
static std::string serialiseNotifyMessage(UpsmonConfiguration::NotifyType type, const std::string & msg) {
	assert(type < UpsmonConfiguration::NOTIFY_TYPE_MAX);

	std::string directive("NOTIFYMSG ");

	directive += NotifyFlagsStrings::type_str[type];
	directive += '\t';
	directive += '"' + msg + '"';

	return directive;
}


/**
 *  \brief  Get notify type successor
 *
 *  TBD: Should be in nutconf.h
 *
 *  \param  type  Notify type
 *
 *  \return Notify type successor
 */
inline static UpsmonConfiguration::NotifyType nextNotifyType(UpsmonConfiguration::NotifyType type) {
	assert(type < UpsmonConfiguration::NOTIFY_TYPE_MAX);

	int type_ord = static_cast<int>(type);

	return static_cast<UpsmonConfiguration::NotifyType>(type_ord + 1);
}


/**
 *  \brief  Notify type pre-incrementation
 *
 *  TBD: Should be in nutconf.h
 *
 *  \param[in,out]  type  Notify type
 *
 *  \return \c type successor
 */
inline static UpsmonConfiguration::NotifyType operator ++(UpsmonConfiguration::NotifyType & type) {
	return type = nextNotifyType(type);
}


/**
 *  \brief  Notify type post-incrementation
 *
 *  TBD: Should be in nutconf.h
 *
 *  \param[in,out]  type  Notify type
 *
 *  \return \c type
 */
inline static UpsmonConfiguration::NotifyType operator ++(UpsmonConfiguration::NotifyType & type, int) {
	UpsmonConfiguration::NotifyType type_copy = type;

	type = nextNotifyType(type);

	return type_copy;
}


/**
 *  \brief  UPS monitor definition serialiser
 *
 *  \param  monitor  Monitor
 *
 *  \return Monitor config. directive
 */
static std::string serialiseMonitor(const UpsmonConfiguration::Monitor & monitor) {
	std::stringstream directive;

	directive << "MONITOR ";

	// System
	directive << monitor.upsname << '@' << monitor.hostname;

	if (monitor.port)
		directive << ':' << monitor.port;

	directive << ' ';

	// Power value
	directive << monitor.powerValue << ' ';

	// Username & password
	directive << monitor.username << ' ' << monitor.password << ' ';

	// Master/slave
	directive << (monitor.isMaster ? "master" : "slave");

	return directive.str();
}


NutWriter::status_t UpsmonConfigWriter::writeConfig(const UpsmonConfiguration & config) {
	/**
	 *  \brief  upsmon directive generator
	 *
	 *  The macro is locally used to simplify generation of
	 *  upsmon config. directives (except those with enumerated
	 *  arguments).
	 *
	 *  NOTE that the macro may cause return from the function
	 *  (upon writing error).
	 *  See \ref CONFIG_DIRECTIVEX for more information.
	 *
	 *  \param  name       Directive name
	 *  \param  arg_t      Directive argument implementation type
	 *  \param  arg        Directive argument
	 *  \param  quote_arg  Boolean flag; check to quote the argument
	 */
	#define UPSMON_DIRECTIVEX(name, arg_t, arg, quote_arg) \
		CONFIG_DIRECTIVEX(name, arg_t, arg, quote_arg)

	UPSMON_DIRECTIVEX("RUN_AS_USER",    std::string,  config.runAsUser,      false);
	UPSMON_DIRECTIVEX("SHUTDOWNCMD",    std::string,  config.shutdownCmd,    true);
	UPSMON_DIRECTIVEX("NOTIFYCMD",      std::string,  config.notifyCmd,      true);
	UPSMON_DIRECTIVEX("POWERDOWNFLAG",  std::string,  config.powerDownFlag,  false);
	UPSMON_DIRECTIVEX("MINSUPPLIES",    unsigned int, config.minSupplies,    false);
	UPSMON_DIRECTIVEX("POLLFREQ",       unsigned int, config.poolFreq,       false);
	UPSMON_DIRECTIVEX("POLLFREQALERT",  unsigned int, config.poolFreqAlert,  false);
	UPSMON_DIRECTIVEX("HOSTSYNC",       unsigned int, config.hotSync,        false);
	UPSMON_DIRECTIVEX("DEADTIME",       unsigned int, config.deadTime,       false);
	UPSMON_DIRECTIVEX("RBWARNTIME",     unsigned int, config.rbWarnTime,     false);
	UPSMON_DIRECTIVEX("NOCOMMWARNTIME", unsigned int, config.noCommWarnTime, false);
	UPSMON_DIRECTIVEX("FINALDELAY",     unsigned int, config.finalDelay,     false);

	#undef UPSMON_DIRECTIVEX

	UpsmonConfiguration::NotifyType type;

	// Notify flags
	type = UpsmonConfiguration::NOTIFY_ONLINE;

	for (; type < UpsmonConfiguration::NOTIFY_TYPE_MAX; ++type) {
		if (config.notifyFlags[type].set()) {
			std::string directive = serialiseNotifyFlags(type, config.notifyFlags[type]);

			status_t status = writeDirective(directive);

			if (NUTW_OK != status)
				return status;
		}
	}

	// Notify messages
	type = UpsmonConfiguration::NOTIFY_ONLINE;

	for (; type < UpsmonConfiguration::NOTIFY_TYPE_MAX; ++type) {
		if (config.notifyMessages[type].set()) {
			std::string directive = serialiseNotifyMessage(type, config.notifyMessages[type]);

			status_t status = writeDirective(directive);

			if (NUTW_OK != status)
				return status;
		}
	}

	// Monitors
	std::list<UpsmonConfiguration::Monitor>::const_iterator mon_iter = config.monitors.begin();

	for (; mon_iter != config.monitors.end(); ++mon_iter) {
		std::string directive = serialiseMonitor(*mon_iter);

		status_t status = writeDirective(directive);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


/**
 *  \brief  upsd listen address serialiser
 *
 *  \param  address  Listen address
 *
 *  \return Serialised listen address
 */
static std::string serialiseUpsdListenAddress(const UpsdConfiguration::Listen & address) {
	std::stringstream directive;

	directive << "LISTEN " << address.address;

	if (address.port.set())
		directive << ' ' << static_cast<unsigned short>(address.port);

	return directive.str();
}


NutWriter::status_t UpsdConfigWriter::writeConfig(const UpsdConfiguration & config) {
	/**
	 *  \brief  upsd directive generator
	 *
	 *  The macro is locally used to simplify generation of
	 *  upsd config. directives (except the listen addresses).
	 *
	 *  NOTE that the macro may cause return from the function
	 *  (upon writing error).
	 *  See \ref CONFIG_DIRECTIVEX for more information.
	 *
	 *  \param  name       Directive name
	 *  \param  arg_t      Directive argument implementation type
	 *  \param  arg        Directive argument
	 */
	#define UPSD_DIRECTIVEX(name, arg_t, arg) \
		CONFIG_DIRECTIVEX(name, arg_t, arg, false)

	UPSD_DIRECTIVEX("MAXAGE",    unsigned int, config.maxAge);
	UPSD_DIRECTIVEX("MAXCONN",   unsigned int, config.maxConn);
	UPSD_DIRECTIVEX("STATEPATH", std::string,  config.statePath);
	UPSD_DIRECTIVEX("CERTFILE",  std::string,  config.certFile);

	#undef UPSD_DIRECTIVEX

	// Listen addresses
	std::list<UpsdConfiguration::Listen>::const_iterator la_iter = config.listens.begin();

	for (; la_iter != config.listens.end(); ++la_iter) {
		std::string directive = serialiseUpsdListenAddress(*la_iter);

		status_t status = writeDirective(directive);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


NutWriter::status_t DefaultConfigWriter::writeComment(const std::string & str) {
	return writeEachLine(str, "# ");
}


NutWriter::status_t DefaultConfigWriter::writeSectionName(const std::string & name) {
	std::string section_line("[");
	section_line += name + "]" + eol;

	return write(section_line);
}


NutWriter::status_t DefaultConfigWriter::writeDirective(const std::string & str) {
	return write(str + eol);
}


/**
 *  \brief  Value quoting and escaping
 *
 *  The function checks whether the value string contains
 *  any spaces and/or '=' characters.
 *  If so, the result is double-quoted and all inner double
 *  quotes shall be escaped using backslash.
 *
 *  \param  val  Value string
 *
 *  \return Value string ready for serialisation
 */
static std::string encodeValue(const std::string & val) {
	// Check the string for spaces and '='
	bool quote = false;

	for (size_t i = 0; i < val.size() && !quote; ++i) {
		char ch = val[i];

		quote = ' ' == ch || '=' == ch;
	}

	if (!quote)
		return val;

	// Quote value and escape inner quotes
	std::string qval;

	qval += '"';

	for (size_t i = 0; i < val.size(); ++i) {
		char ch = val[i];

		if ('"' == ch)
			qval += '\\';

		qval += ch;
	}

	qval += '"';

	return qval;
}


NutWriter::status_t GenericConfigWriter::writeSectionEntry(
	const GenericConfigSectionEntry & entry,
	const std::string & indent,
	const std::string & kv_sep)
{
	ConfigParamList::const_iterator value_iter = entry.values.begin();

	for (; value_iter != entry.values.end(); ++value_iter) {
		std::string value = encodeValue(*value_iter);

		status_t status = writeDirective(indent + entry.name + kv_sep + value);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


NutWriter::status_t GenericConfigWriter::writeSection(const GenericConfigSection & section) {
	status_t status;

	// Note that global scope definitions are in section
	// with an empty name
	// The section name won't be written and the assignments
	// won't be indented
	std::string indent;

	if (!section.name.empty()) {
		status = writeSectionName(section.name);

		if (NUTW_OK != status)
			return status;

		indent += "\t";
	}

	// Write section name/value pairs
	GenericConfigSection::EntryMap::const_iterator entry_iter = section.entries.begin();

	for (; entry_iter != section.entries.end(); ++entry_iter) {
		status = writeSectionEntry(entry_iter->second, indent);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


NutWriter::status_t GenericConfigWriter::writeConfig(const GenericConfiguration & config) {
	// Write sections
	// Note that lexicographic ordering places the global
	// (i.e. empty-name) section as the 1st one
	GenericConfiguration::SectionMap::const_iterator section_iter = config.sections.begin();

	for (; section_iter != config.sections.end(); ++section_iter) {
		status_t status = writeSection(section_iter->second);

		if (NUTW_OK != status)
			return status;

		// TBD: Write one empty line as section separator
		status = write(eol);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


NutWriter::status_t UpsdUsersConfigWriter::writeSection(const GenericConfigSection & section) {
	static const std::string upsmon_entry_separator(" ");

	status_t status;

	// upsmon section requires special handling because of the upsmon (master|slave) directive
	if ("upsmon" != section.name)
		return GenericConfigWriter::writeSection(section);

	status = writeSectionName(section.name);

	if (NUTW_OK != status)
		return status;

	// Write section name/value pairs
	GenericConfigSection::EntryMap::const_iterator entry_iter = section.entries.begin();

	for (; entry_iter != section.entries.end(); ++entry_iter) {
		// Special case of upsmon parameter
		if ("upsmon" == entry_iter->second.name) {
			status = writeSectionEntry(entry_iter->second,
					s_default_section_entry_indent,
					upsmon_entry_separator);
		}

		// Standard entry serialisation
		else {
			status = writeSectionEntry(entry_iter->second);
		}

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}

}  // end of namespace nut
