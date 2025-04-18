/*
    nutwriter.cpp - NUT writer

    Copyright (C) 2012 Eaton

        Author: Vaclav Krpec  <VaclavKrpec@Eaton.com>

    Copyright (C) 2024-2025 NUT Community

        Author: Jim Klimov  <jimklimov+nut@gmail.com>

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

#include "nutwriter.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include <cassert>
#include <typeinfo>

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


/**
 *  \brief  Shell (envvar) configuration directive generator
 *
 *  The macro is used to simplify generation of
 *  nut.conf file directives.
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
// NOTE: Due to this being a macro applied to any argument type,
// implementation for e.g. bool handling jumps through hoops like
// stringification and back. FIXME: working optimization welcome.
#define SHELL_CONFIG_DIRECTIVEX(name, arg_t, arg, quote_arg) \
	do { \
		if ((arg).set()) { \
			const arg_t & arg_val = (arg); \
			std::stringstream ss; \
			ss << name << '='; \
			if (quote_arg) \
				ss << '\''; \
			if (typeid(arg_val) == typeid(bool&)) { \
				std::stringstream ssb; \
				ssb << arg_val; \
				std::string sb = ssb.str(); \
				if ("1" == sb) { ss << "true"; } \
				else if ("0" == sb) { ss << "false"; } \
				else { ss << arg_val; } \
			} else \
				ss << arg_val; \
			if (quote_arg) \
				ss << '\''; \
			status_t status = writeDirective(ss.str()); \
			if (NUTW_OK != status) \
				return status; \
		} \
	} while (0)


namespace nut {


/* Trivial implementations out of class declaration to avoid
 * error: 'ClassName' has no out-of-line virtual method definitions; its vtable
 *   will be emitted in every translation unit [-Werror,-Wweak-vtables]
 */
NutConfigWriter::~NutConfigWriter() {}	// generic interface/base class
// Flat-config classes:
NutConfConfigWriter::~NutConfConfigWriter() {}	// nut.conf, shell format
UpsmonConfigWriter::~UpsmonConfigWriter() {}	// upsmon.conf
UpsdConfigWriter::~UpsdConfigWriter() {}		// upsd.conf
// Structured-config classes R/W is handled via GenericConfiguration:
//	UpsConfiguration:		ups.conf
//	UpsdUsersConfiguration:	upsd.users
// Not handled currently:
//	xxx:	upssched.conf
//	xxx:	upsset.conf
//	xxx:	hosts.conf

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


/**
 *  \brief  NSS certificate identity serializer
 *
 *  \param  ident  Certificate identity object
 *
 *  \return Serialized certificate identity
 */
static std::string serializeCertIdent(const nut::CertIdent & ident) {
	std::stringstream directive;
	const std::string & val1 = (ident.certName), val2 = (ident.certDbPass);

	directive << "CERTIDENT \"" << val1 << "\" \"" << val2 << "\"";

	return directive.str();
}


/**
 *  \brief  NSS certificate-protected host info serializer
 *
 *  \param  certHost  Certificate-protected host info object
 *
 *  \return Serialized certificate-protected host info
 */
static std::string serializeCertHost(const nut::CertHost & certHost) {
	std::stringstream directive;
	const std::string & val1 = (certHost.host), val2 = (certHost.certName);

	directive << "CERTHOST \"" << val1 << "\" \"" << val2 << "\"";

	// Spec says to write these as 0/1 integers
	nut::BoolInt bi;
	int i;
	// NOTE: After copy-assignments below (which inherit original strictness),
	// need to add relaxed mode for 0/1 as false/true handling:
	//bi.bool01 = true;

	// Avoid static analysis concerns that the internal _value
	// "may be used uninitialized in this function" (ETOOSMART):
	bi = false;

	// Assumed to be set() - exception otherwise
	bi = certHost.certVerify;
	bi.bool01 = true;
	i = bi;
	directive << " " << i;

	bi = certHost.forceSsl;
	bi.bool01 = true;
	i = bi;
	directive << " " << i;

	return directive.str();
}


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
	// Mode
	// TBD: How should I serialize an unknown mode?
	if (config.mode.set()) {
		status_t status;
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

		status = writeDirective("MODE=" + mode_str);

		if (NUTW_OK != status)
			return status;
	}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	SHELL_CONFIG_DIRECTIVEX("ALLOW_NO_DEVICE",		bool,			config.allowNoDevice,	false);
	SHELL_CONFIG_DIRECTIVEX("ALLOW_NOT_ALL_LISTENERS",	bool,		config.allowNotAllListeners,	false);
	SHELL_CONFIG_DIRECTIVEX("UPSD_OPTIONS",			std::string,	config.upsdOptions,		true);
	SHELL_CONFIG_DIRECTIVEX("UPSMON_OPTIONS",		std::string,	config.upsmonOptions,	true);
	SHELL_CONFIG_DIRECTIVEX("POWEROFF_WAIT",		unsigned int,	config.poweroffWait,	false);
	SHELL_CONFIG_DIRECTIVEX("POWEROFF_QUIET",		bool,			config.poweroffQuiet,	false);
	SHELL_CONFIG_DIRECTIVEX("NUT_DEBUG_LEVEL",		int,			config.debugLevel,		false);
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic pop
#endif

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
	 *  \brief  Initialize notify flag strings
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
	"CAL\t",	// NOTIFY_CAL (including padding)
	"NOTCAL",	// NOTIFY_NOTCAL
	"OFF\t",	// NOTIFY_OFF (including padding)
	"NOTOFF",	// NOTIFY_NOTOFF
	"BYPASS",	// NOTIFY_BYPASS
	"NOTBYPASS",	// NOTIFY_NOTBYPASS
	"ECO\t",	// NOTIFY_ECO (including padding); NOTE: inverter mode, not ups state, for notifications
	"NOTECO",	// NOTIFY_NOTECO
	"ALARM",	// NOTIFY_ALARM
	"NOTALARM",	// NOTIFY_NOTALARM
	"OVER",		// NOTIFY_OVER
	"NOTOVER",	// NOTIFY_NOTOVER
	"TRIM",		// NOTIFY_TRIM
	"NOTTRIM",	// NOTIFY_NOTTRIM
	"BOOST",	// NOTIFY_BOOST
	"NOTBOOST",	// NOTIFY_NOTBOOST
	"OTHER",	// NOTIFY_OTHER
	"NOTOTHER",	// NOTIFY_NOTOTHER
	"SUSPEND_STARTING",	// NOTIFY_SUSPEND_STARTING
	"SUSPEND_FINISHED",	// NOTIFY_SUSPEND_FINISHED
};


const NotifyFlagsStrings::FlagStrings NotifyFlagsStrings::flag_str =
	NotifyFlagsStrings::initFlagStrings();


/**
 *  \brief  upsmon notify flags serializer
 *
 *  \param  type   Notification type
 *  \param  flags  Notification flags
 *
 *  \return NOTIFYFLAG directive string
 */
static std::string serializeNotifyFlags(UpsmonConfiguration::NotifyType type, unsigned int flags) {
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
 *  \brief  upsmon notify messages serializer
 *
 *  \param  type   Notification type
 *  \param  msg    Notification message
 *
 *  \return NOTIFYMSG directive string
 */
static std::string serializeNotifyMessage(UpsmonConfiguration::NotifyType type, const std::string & msg) {
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
 *  TBD: Should be in nutconf.hpp
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
 *  \brief  Notify type pre-increment
 *
 *  TBD: Should be in nutconf.hpp
 *
 *  \param[in,out]  type  Notify type
 *
 *  \return \c type successor
 */
inline static UpsmonConfiguration::NotifyType operator ++(UpsmonConfiguration::NotifyType & type) {
	return type = nextNotifyType(type);
}


/**
 *  \brief  Notify type post-increment
 *
 *  TBD: Should be in nutconf.hpp
 *
 *  \param[in,out]  type  Notify type
 *
 *  \return \c type
 */
/* // CURRENTLY UNUSED
inline static UpsmonConfiguration::NotifyType operator ++(UpsmonConfiguration::NotifyType & type, int) {
	UpsmonConfiguration::NotifyType type_copy = type;

	type = nextNotifyType(type);

	return type_copy;
}
*/

/**
 *  \brief  UPS monitor definition serializer
 *
 *  \param  monitor  Monitor
 *
 *  \return Monitor config. directive
 */
static std::string serializeMonitor(const UpsmonConfiguration::Monitor & monitor) {
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

	// Primary/secondary (legacy master/slave)
	directive << (monitor.isPrimary ? "primary" : "secondary");
	/* NUT v2.7.4 and older: directive << (monitor.isPrimary ? "master" : "slave");*/

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
#	define UPSMON_DIRECTIVEX(name, arg_t, arg, quote_arg) \
		CONFIG_DIRECTIVEX(name, arg_t, arg, quote_arg)

/* The "false" arg in macro below evaluates to `if (false) ...` after
 * pre-processing, and causes warnings about unreachable code */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	UPSMON_DIRECTIVEX("DEBUG_MIN",      int,          config.debugMin,       false);
	UPSMON_DIRECTIVEX("RUN_AS_USER",    std::string,  config.runAsUser,      true);
	UPSMON_DIRECTIVEX("SHUTDOWNCMD",    std::string,  config.shutdownCmd,    true);
	UPSMON_DIRECTIVEX("NOTIFYCMD",      std::string,  config.notifyCmd,      true);
	UPSMON_DIRECTIVEX("POWERDOWNFLAG",  std::string,  config.powerDownFlag,  true);
	UPSMON_DIRECTIVEX("MINSUPPLIES",    unsigned int, config.minSupplies,    false);
	UPSMON_DIRECTIVEX("POLLFREQ",       unsigned int, config.pollFreq,       false);
	UPSMON_DIRECTIVEX("POLLFREQALERT",  unsigned int, config.pollFreqAlert,  false);
	UPSMON_DIRECTIVEX("POLLFAIL_LOG_THROTTLE_MAX", int, config.pollFailLogThrottleMax,  false);
	UPSMON_DIRECTIVEX("OFFDURATION",    int,          config.offDuration,    false);
	UPSMON_DIRECTIVEX("OBLBDURATION",   int,          config.oblbDuration,   false);
	UPSMON_DIRECTIVEX("OVERDURATION",   int,          config.overDuration,   false);
	UPSMON_DIRECTIVEX("SHUTDOWNEXIT",   nut::BoolInt, config.shutdownExit,   false);

	UPSMON_DIRECTIVEX("CERTPATH",       std::string,  config.certPath,       true);

	// Spec says to write these as 0/1 integers
	// and the macro requires Settable<>
	// Mumbo-jumbo below for guaranteed casting to int
	nut::BoolInt bi, bi2;
	Settable<nut::BoolInt> bis;
	int i;
	// NOTE: After copy-assignments below (which inherit original strictness),
	// need to add relaxed mode for 0/1 as false/true handling:
	// bi.bool01 = true;
	bi2.bool01 = false;	// strict mode for 0/1 as int handling
	// Avoid static analysis concerns that the internal _value
	// "may be used uninitialized in this function" (ETOOSMART):
	bi = false;
	bi2 = false;

	if (config.certVerify.set()) {
		bi = config.certVerify;
		bi.bool01 = true;
		i = bi;
		bi2 = i;
		bis = bi2;
		UPSMON_DIRECTIVEX("CERTVERIFY",     nut::BoolInt, bis,                   false);
	}

	if (config.forceSsl.set()) {
		bi = config.forceSsl;
		bi.bool01 = true;
		i = bi;
		bi2 = i;
		bis = bi2;
		UPSMON_DIRECTIVEX("FORCESSL",       nut::BoolInt, bis,                   false);
	}

	UPSMON_DIRECTIVEX("HOSTSYNC",       unsigned int, config.hostSync,       false);
	UPSMON_DIRECTIVEX("DEADTIME",       unsigned int, config.deadTime,       false);
	UPSMON_DIRECTIVEX("RBWARNTIME",     unsigned int, config.rbWarnTime,     false);
	UPSMON_DIRECTIVEX("NOCOMMWARNTIME", unsigned int, config.noCommWarnTime, false);
	UPSMON_DIRECTIVEX("FINALDELAY",     unsigned int, config.finalDelay,     false);
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic pop
#endif

#	undef UPSMON_DIRECTIVEX

	// Certificate identity
	if (config.certIdent.set()) {
		std::string directive = serializeCertIdent(config.certIdent);

		status_t status = writeDirective(directive);

		if (NUTW_OK != status)
			return status;
	}

	// Remote host(s) protected by specific certificates on their listeners
	std::list<nut::CertHost>::const_iterator la_iter = config.certHosts.begin();

	for (; la_iter != config.certHosts.end(); ++la_iter) {
		std::string directive = serializeCertHost(*la_iter);

		status_t status = writeDirective(directive);

		if (NUTW_OK != status)
			return status;
	}

	UpsmonConfiguration::NotifyType type;

	// Notify flags
	type = UpsmonConfiguration::NOTIFY_ONLINE;

	for (; type < UpsmonConfiguration::NOTIFY_TYPE_MAX; ++type) {
		if (config.notifyFlags[type].set()) {
			std::string directive = serializeNotifyFlags(type, config.notifyFlags[type]);

			status_t status = writeDirective(directive);

			if (NUTW_OK != status)
				return status;
		}
	}

	// Notify messages
	type = UpsmonConfiguration::NOTIFY_ONLINE;

	for (; type < UpsmonConfiguration::NOTIFY_TYPE_MAX; ++type) {
		if (config.notifyMessages[type].set()) {
			std::string directive = serializeNotifyMessage(type, config.notifyMessages[type]);

			status_t status = writeDirective(directive);

			if (NUTW_OK != status)
				return status;
		}
	}

	// Monitors
	std::list<UpsmonConfiguration::Monitor>::const_iterator mon_iter = config.monitors.begin();

	for (; mon_iter != config.monitors.end(); ++mon_iter) {
		std::string directive = serializeMonitor(*mon_iter);

		status_t status = writeDirective(directive);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


/**
 *  \brief  upsd listen address serializer
 *
 *  \param  address  Listen address
 *
 *  \return Serialized listen address
 */
static std::string serializeUpsdListenAddress(const UpsdConfiguration::Listen & address) {
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
#	define UPSD_DIRECTIVEX(name, arg_t, arg) \
		CONFIG_DIRECTIVEX(name, arg_t, arg, false)

/* The "false" arg in macro below evaluates to `if (false) ...` after
 * pre-processing, and causes warnings about unreachable code */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	UPSD_DIRECTIVEX("DEBUG_MIN",                int,          config.debugMin);
	UPSD_DIRECTIVEX("MAXAGE",                   unsigned int, config.maxAge);
	UPSD_DIRECTIVEX("MAXCONN",                  unsigned int, config.maxConn);
	UPSD_DIRECTIVEX("TRACKINGDELAY",            unsigned int, config.trackingDelay);
	UPSD_DIRECTIVEX("ALLOW_NO_DEVICE",          bool,         config.allowNoDevice);
	UPSD_DIRECTIVEX("ALLOW_NOT_ALL_LISTENERS",  bool,         config.allowNotAllListeners);
	UPSD_DIRECTIVEX("DISABLE_WEAK_SSL",         bool,         config.disableWeakSsl);
	CONFIG_DIRECTIVEX("STATEPATH",              std::string,  config.statePath, true);
	CONFIG_DIRECTIVEX("CERTFILE",               std::string,  config.certFile, true);
	CONFIG_DIRECTIVEX("CERTPATH",               std::string,  config.certPath, true);
	UPSD_DIRECTIVEX("CERTREQUEST",              unsigned int, config.certRequestLevel);
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
# pragma GCC diagnostic pop
#endif

#	undef UPSD_DIRECTIVEX

	// Certificate identity
	if (config.certIdent.set()) {
		std::string directive = serializeCertIdent(config.certIdent);

		status_t status = writeDirective(directive);

		if (NUTW_OK != status)
			return status;
	}

	// Listen addresses
	std::list<UpsdConfiguration::Listen>::const_iterator la_iter = config.listens.begin();

	for (; la_iter != config.listens.end(); ++la_iter) {
		std::string directive = serializeUpsdListenAddress(*la_iter);

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
 *  \return Value string ready for serialization
 */
static std::string encodeValue(const std::string & val) {
	// Check the string for spaces and '='
	bool quote = false;

	for (size_t i = 0; i < val.size() && !quote; ++i) {
		char ch = val[i];

		quote = ' ' == ch || '=' == ch || ':' == ch;
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
	// (i.e. empty-name) section as the first one
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

		// Standard entry serialization
		else {
			status = writeSectionEntry(entry_iter->second);
		}

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}

}  // end of namespace nut
