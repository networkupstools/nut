/*
 *  Copyright (C)
 *      2013 - EATON
 *      2024-2025 - Jim Klimov <jimklimov+nut@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*! \file nutconf.cpp
    \brief NUT configuration tool
    \author Frederic Bohe <fredericbohe@eaton.com>
    \author Vaclav Krpec <vaclavkrpec@eaton.com>
*/

#include "config.h"
#include "nutconf.hpp"
#include "nutstream.hpp"

extern "C" {
/* FIXME? Is it counter-intentional to use our C common library
 * for C++ code (here for common printing of version banner)? */
#include "common.h"
#if (defined WITH_NUTSCANNER)
/* NOTE: `libnutscan` itself gets linked in at run-time, no ltdl optionality */
# include "nut-scan.h"
# include "nutscan-init.h"
# include "nutscan-device.h"
#endif	/* WITH_NUTSCANNER */
}

#include <iostream>
#include <list>
#include <vector>
#include <map>
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <cstdlib>


class Usage {
	private:

	/** Usage text */
	static const char * s_text[];

	/** Private constructor (no instances) */
	Usage() {}

	public:

	/** Print version and usage to stderr */
	static void print(const std::string & bin);

	/** Print version info to stdout */
	static void printVersion(const std::string & bin);

};  // end of class usage


const char * Usage::s_text[] = {
	"    -h  -help",
	"    --help                              Display this help and exit",
	"    -V",
	"    --version                           Display tool version on stdout and exit",
	"    --autoconfigure                     Perform automatic configuration",
	"    --is-configured                     Checks whether NUT is configured",
	"    --local <directory>                 Sets configuration directory",
	"    --system                            Sets configuration directory to " CONFPATH " (default)",
	"                                        NOTE: If NUT_CONFPATH envvar is set,",
	"                                        it is the default unless overridden by",
	"                                        --system (lower prio) or --local DIR",
	"    --get-mode                          Gets NUT mode (see below)",
	"    --set-mode <NUT mode>               Sets NUT mode (see below)",
	"    --set-monitor <spec>                Configures one monitor (see below)",
	"                                        All existing entries are removed; however, it may be",
	"                                        specified multiple times to set multiple entries",
	"    --add-monitor <spec>                Same as --set-monitor, but keeps existing entries",
	"                                        The two options are mutually exclusive",
	"    --set-listen <addr> [<port>]        Configures one listen address for the NUT daemon",
	"                                        All existing entries are removed; however, it may be",
	"                                        specified multiple times to set multiple entries",
	"    --add-listen <addr> [<port>]        Same as --set-listen, but keeps existing entries",
	"                                        The two options are mutually exclusive",
	"    --set-device <spec>                 Configures one UPS device (see below)",
	"                                        All existing devices are removed; however, it may be",
	"                                        specified multiple times to set multiple devices",
	"    --add-device <spec>                 Same as --set-device, but keeps existing devices",
	"                                        The two options are mutually exclusive",
	"    --set-notifyflags <type> <flag>+    Configures notify flags for notification type",
	"                                        See below for the types and supported flags",
	"                                        Existing flags are replaced",
	"    --add-notifyflags <type> <flag>+    Same as --set-notifyflags, but keeps existing flags",
	"    --set-notifymsg <type> <message>    Configures notification message for the type",
	"    --set-notifycmd <command>           Configures notification command",
	"    --set-shutdowncmd <command>         Configures shutdown command",
	"    --set-minsupplies <supp_no>         Configures minimum of required power supplies",
	"    --set-powerdownflag <file>          Configures powerdown flag file",
	"    --set-user <spec>                   Configures one user (see below)",
	"                                        All existing users are removed; however, it may be",
	"                                        specified multiple times to set multiple users",
	"    --add-user <spec>                   Same as --set-user, but keeps existing users",
	"                                        The two options are mutually exclusive",
	/* FIXME: Alias as "-D"? Is this the same as nut_debug_level
	 * NOTE: upsdebugx() not used here directly (yet?), though we
	 * could setenv() the envvar for libnutscan perhaps? */
	"    -v",
	"    --verbose                           Increase verbosity of output one level",
	"                                        May be specified multiple times",
#if (defined WITH_NUTSCANNER)
#if (defined WITH_SNMP)
	"    --scan-snmp <spec>                  Scan SNMP devices (see below)",
	"                                        May be specified multiple times",
#endif  // defined WITH_SNMP
#if (defined WITH_USB)
	"    --scan-usb                          Scan USB devices",
#endif  // defined WITH_USB
#if (defined WITH_NEON)
	"    --scan-xml-http [<timeout>]         Scan XML/HTTP devices (optional timeout in us)",
#endif  // defined WITH_NEON
	"    --scan-nut <spec>                   Scan NUT devices (see below for the specs)",
	"                                        May be specified multiple times",
#if (defined WITH_AVAHI)
	"    --scan-avahi [<timeout>]            Scan Avahi devices (optional timeout in us)",
#endif  // defined WITH_AVAHI
#if (defined WITH_IPMI)
	"    --scan-ipmi <spec>                  Scan IPMI devices (see below)",
	"                                        May be specified multiple times",
#endif  // defined WITH_IPMI
	"    --scan-serial <port>*               Scan for serial devices on specified ports",
#endif  // defined WITH_NUTSCANNER
	"",
	"NUT modes: standalone, netserver, netclient, controlled, manual, none",
	"Monitor is specified by the following sequence:",
	"    <ups_ID> <host>[:<port>] <power_value> <user> <passwd> (\"primary\"|\"secondary\")",
	"UPS device is specified by the following sequence:",
	"    <ups_ID> <driver> <port> [<key>=<value>]*",
	"Notification types:",
	"    ONLINE, ONBATT, LOWBATT, FSD, COMMOK, COMMBAD, SHUTDOWN, REPLBATT, NOCOMM, NOPARENT,",
	"    CAL, NOTCAL, OFF, NOTOFF, BYPASS, NOTBYPASS, ECO, NOTECO, ALARM, NOTALARM,",
	"    OVER, NOTOVER, TRIM, NOTTRIM, BOOST, NOTBOOST,",
	"    OTHER, NOTOTHER, SUSPEND_STARTING, SUSPEND_FINISHED",
	"Notification flags:",
	"    SYSLOG, WALL, EXEC, IGNORE",
	"User specification:",
	"The very 1st argument is the username (but see the upsmon exception, below).",
	"Next arguments use scheme of key/value pairs in form <key>=<value>",
	"Known keys are:",
	"    password, actions (from {SET,FSD}), instcmds (accepted multiple times)",
	"Specially, for the upsmon user, the 1st argument takes form of",
	"    upsmon={master|slave}",
#if (defined WITH_NUTSCANNER)
#if (defined WITH_SNMP)
	"SNMP device scan specification:",
	"    <start IP> <stop IP> [<attr>=<val>]*",
	"Known attributes are:",
	"    timeout (in us), community, sec-level, sec-name, auth-password, priv-password,",
	"    auth-protocol, priv-protocol, peer-name",
#endif  // defined WITH_SNMP
	"NUT device scan specification:",
	"    <start IP> <stop IP> <port> [<us_timeout>]",
#if (defined WITH_IPMI)
	"IMPI device scan specification:",
	"    <start IP> <stop IP> [<attr>=<val>]*",
	"Known attributes are:",
	"    username, password, auth-type, cipher-suite-id, K-g-BMC-key, priv-level,",
	"    workaround-flags, version",
#endif  // defined WITH_IPMI
#endif  // defined WITH_NUTSCANNER
	"",
};

/**
 * Print version info to stdout (like other NUT tools)
 */
void Usage::printVersion(const std::string & bin) {
	std::cout
		<< "Network UPS Tools " << bin
		<< " " << describe_NUT_VERSION_once() << std::endl;
}

/**
 * Print help text (including version info) to stderr
 */
void Usage::print(const std::string & bin) {
	std::cerr
		<< "Network UPS Tools " << bin
		<< " " << describe_NUT_VERSION_once() << std::endl
		<< std::endl
		<< "Usage: " << bin << " [OPTIONS]" << std::endl
		<< std::endl
		<< "OPTIONS:" << std::endl;

	for (size_t i = 0; i < sizeof(s_text) / sizeof(char *); ++i) {
		std::cerr << s_text[i] << std::endl;
	}

	std::cerr
		/* << std::endl // last line of s_text is blank */
		<< suggest_doc_links(bin.c_str(), nullptr);
		/* Method output brings its own endl */
}


/** Command line options */
class Options {
	public:

	/** Options list */
	typedef std::list<std::string> List;

	/** Option arguments list */
	typedef std::list<std::string> Arguments;

	protected:

	/** Options map */
	typedef std::multimap<std::string, Arguments> Map;

	private:

	/** Option type */
	typedef enum {
		singleDash,  /**< Single-dash prefixed option */
		doubleDash,  /**< Double-dash prefixed option */
	} type_t;

	/** Arguments of the last option processed (\c nullptr means bin. args) */
	Arguments * m_last;

	/** Binary arguments */
	Arguments m_args;

	/** Single-dashed options */
	Map m_single;

	/** Double-dashed options */
	Map m_double;

	/**
	 *  \brief  Add option
	 *
	 *  \param  type  Option type
	 *  \param  opt   Option
	 */
	void add(type_t type, const std::string & opt);

	/**
	 *  \brief  Add argument to the last option
	 *
	 *  \param  arg  Argument
	 */
	inline void addArg(const std::string & arg) {
		Arguments * args = nullptr != m_last ? m_last : &m_args;

		args->push_back(arg);
	}

	/**
	 *  \brief  Count options
	 *
	 *  \param  map  Option map
	 *  \param  opt  Option
	 *
	 *  \return Options count
	 */
	size_t count(const Map & map, const std::string & opt) const;

	/**
	 *  \brief  Get option arguments
	 *
	 *  \param[in]   map    Option map
	 *  \param[in]   opt    Option
	 *  \param[out]  args   Option arguments
	 *  \param[in]   order  Option order (1st by default)
	 *
	 *  \retval true  IFF the option was specified on the command line
	 *  \retval false otherwise
	 */
	bool get(const Map & map, const std::string & opt, Arguments & args, size_t order = 0) const;

	/**
	 *  \brief  Get options list
	 *
	 *  \param[in]   map   Option map
	 *  \param[out]  list  Option list
	 *
	 *  \return List of options
	 */
	void strings(const Map & map, List & list) const;

	/**
	 *  \brief  Dump options (for debugging reasons)
	 *
	 *  \param  stream  Output stream
	 */
	void dump(std::ostream & stream) const;

	public:

	/**
	 *  \brief  Constructor (from \c main routine arguments)
	 *
	 *  \param  argv  Argument list
	 *  \param  argc  Argument count
	 */
	Options(char * const argv[], int argc);

	/**
	 *  \brief  Count single-dashed options
	 *
	 *  \param  opt  Option
	 *
	 *  \return Options count
	 */
	inline size_t countSingle(const std::string & opt) const {
		return count(m_single, opt);
	}

	/**
	 *  \brief  Count double-dashed options
	 *
	 *  \param  opt  Option
	 *
	 *  \return Options count
	 */
	inline size_t countDouble(const std::string & opt) const {
		return count(m_double, opt);
	}

	/**
	 *  \brief  Count options (single or double dashed)
	 *
	 *  \param  opt  Option
	 *
	 *  \return Options count
	 */
	inline bool count(const std::string & opt) const {
		return countSingle(opt) + countDouble(opt);
	}

	/**
	 *  \brief  Check single-dashed option existence
	 *
	 *  \param  opt  Option
	 *
	 *  \retval true  IFF the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool existsSingle(const std::string & opt) const {
		return countSingle(opt) > 0;
	}

	/**
	 *  \brief  Check double-dashed option existence
	 *
	 *  \param  opt  Option
	 *
	 *  \retval true  IFF the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool existsDouble(const std::string & opt) const {
		return countDouble(opt) > 0;
	}

	/**
	 *  \brief  Check option existence (single or double dashed)
	 *
	 *  \param  opt  Option
	 *
	 *  \retval true  IFF the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool exists(const std::string & opt) const {
		return existsSingle(opt) || existsDouble(opt);
	}

	/**
	 *  \brief  Get single-dashed option arguments
	 *
	 *  \param[in]   opt    Option
	 *  \param[out]  args   Option arguments
	 *  \param[in]   order  Option order (1st by default)
	 *
	 *  \retval true  IFF the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool getSingle(const std::string & opt, Arguments & args, size_t order = 0) const {
		return get(m_single, opt, args, order);
	}

	/**
	 *  \brief  Get double-dashed option arguments
	 *
	 *  \param[in]   opt   Option
	 *  \param[out]  args  Option arguments
	 *  \param[in]   order  Option order (1st by default)
	 *
	 *  \retval true  IFF the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool getDouble(const std::string & opt, Arguments & args, size_t order = 0) const {
		return get(m_double, opt, args, order);
	}

	/**
	 *  \brief  Get binary arguments
	 *
	 *  \return Arguments of the binary itself
	 */
	inline const Arguments & get() const { return m_args; }

	/**
	 *  \brief  Get single-dashed options list
	 *
	 *  \return List of single-dashed options
	 */
	inline List stringsSingle() const {
		List list;

		strings(m_single, list);

		return list;
	}

	/**
	 *  \brief  Get double-dashed options list
	 *
	 *  \return List of double-dashed options
	 */
	inline List stringsDouble() const {
		List list;

		strings(m_double, list);

		return list;
	}

	/**
	 *  \brief  Get all options list
	 *
	 *  \return List of single or double-dashed options
	 */
	inline List strings() const;

};  // end of class Options


void Options::add(Options::type_t type, const std::string & opt) {
	Map * map;

	switch (type) {
		case singleDash:
			map = &m_single;

			break;

		case doubleDash:
			map = &m_double;

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
				/* Must not occur thanks to enum.
				 * But otherwise we can see
				 *   error: 'map' may be used uninitialized
				 * from some overly zealous compilers.
				 */
				if (1) { // scoping
					std::stringstream e;

					e << "Options::add() got unsupported enum value";

					throw std::logic_error(e.str());
				}
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}

	Map::iterator entry = map->insert(Map::value_type(opt, Arguments()));

	m_last = &entry->second;
}


size_t Options::count(const Options::Map & map, const std::string & opt) const {
	size_t cnt = 0;

	Map::const_iterator entry = map.find(opt);

	for (; entry != map.end() && entry->first == opt; ++entry)
		++cnt;

	return cnt;
}


bool Options::get(const Options::Map & map, const std::string & opt, Arguments & args, size_t order) const {
	Map::const_iterator entry = map.find(opt);

	if (map.end() == entry)
		return false;

	for (; order; --order) {
		Map::const_iterator next = entry;

		++next;

		if (map.end() == next || next->first != opt)
			return false;

		entry = next;
	}

	args = entry->second;

	return true;
}


void Options::strings(const Map & map, List & list) const {
	for (Map::const_iterator opt = map.begin(); opt != map.end(); ++opt)
		list.push_back(opt->first);
}


void Options::dump(std::ostream & stream) const {
	stream << "----- Options dump begin -----" << std::endl;

	Map::const_iterator opt;

	Arguments::const_iterator arg;

	for (opt = m_single.begin(); opt != m_single.end(); ++opt) {
		stream << '-' << opt->first << ' ';

		for (arg = opt->second.begin(); arg != opt->second.end(); ++arg)
			stream << *arg << ' ';

		stream << std::endl;
	}

	for (opt = m_double.begin(); opt != m_double.end(); ++opt) {
		stream << "--" << opt->first << ' ';

		for (arg = opt->second.begin(); arg != opt->second.end(); ++arg)
			stream << *arg << ' ';

		stream << std::endl;
	}

	stream << "-- ";

	for (arg = m_args.begin(); arg != m_args.end(); ++arg)
		stream << *arg;

	stream << std::endl << "----- Options dump end -----" << std::endl;
}


Options::Options(char * const argv[], int argc): m_last(nullptr) {
	int i;
	for (i = 1; i < argc; ++i) {
		const std::string arg(argv[i]);

		// Empty string is the current option argument, too
		// '-' alone is also an option argument
		// (like stdout placeholder etc)
		if (arg.empty() || '-' != arg[0] || 1 == arg.size())
			addArg(arg);

		// Single-dashed option
		else if ('-' != arg[1])
			add(singleDash, arg.substr(1));

		// "--" alone is valid as it means that what follows
		// belongs to the binary ("empty" option arguments)
		else if (2 == arg.size())
			m_last = nullptr;

		// Double-dashed option
		else if ('-' != arg[2])
			add(doubleDash, arg.substr(2));

		// "---" prefix means an option argument
		else
			addArg(arg);
	}

	// Options debugging
	//dump(std::cerr);
}


Options::List Options::strings() const {
	List list = stringsSingle();

	strings(m_double, list);

	return list;
}


#if (defined WITH_NUTSCANNER)

/** NUT scanner wrapper */
class NutScanner {
	public:

	/** Device info */
	class Device {
		friend class NutScanner;

		public:

		/** Device type */
		typedef nutscan_device_type_t type_t;

		/** Device options */
		typedef std::map<std::string, const std::string> options_t;

		public:

		const std::string type;     /**< Type    */
		const std::string driver;   /**< Driver  */
		const std::string port;     /**< Port    */
		const options_t   options;  /**< Options */

		private:

		/**
		 *  \brief  Create options
		 *
		 *  \param  opt  libnutscan device options
		 *
		 *  \return Device option list
		 */
		static options_t createOptions(nutscan_options_t * opt);

		/** Constructor */
		Device(nutscan_device_t * dev);

	};  // end of class Device

	/** Device list */
	typedef std::list<Device> devices_t;

	/** SNMP attributes */
	struct SNMPAttributes {
		std::string community;    /**< Community               */
		std::string sec_level;    /**< Sec. level              */
		std::string sec_name;     /**< Sec. name               */
		std::string auth_passwd;  /**< Authentication password */
		std::string priv_passwd;  /**< Priv. password          */
		std::string auth_proto;   /**< Authentication protocol */
		std::string priv_proto;   /**< Priv. protocol          */
		std::string peer_name;    /**< Peer name               */
	};  // end of class SNMPAttributes

	/** IMPI attributes */
	struct IPMIAttributes {
		std::string username;         /**< Username              */
		std::string passwd;           /**< Password              */
		int         auth_type;        /**< Authentication type   */
		int         cipher_suite_id;  /**< Cipher suite ID       */
		std::string K_g_BMC_key;      /**< Optional 2nd key      */
		int         priv_level;       /**< Priviledge level      */
		unsigned    wa_flags;         /**< Workaround flags      */
		int         version;          /**< IPMI protocol version */

		/** Constructor */
		IPMIAttributes():
			auth_type(IPMI_AUTHENTICATION_TYPE_MD5),
			cipher_suite_id(3),
			priv_level(IPMI_PRIVILEGE_LEVEL_ADMIN),
			wa_flags(0),
			version(IPMI_1_5)
		{}

	};  // end of struct IMPIAttributes

	private:

	/** NUT scanner initialization/finalization */
	struct InitFinal {
		/** Initialization */
		InitFinal() { nutscan_init(); }

		/** Finalization */
		~InitFinal() { nutscan_free(); }

	};  // end of struct InitFinal

	/** Initializer / finalizer */
	static InitFinal s_init_final;

	/**
	 *  \brief  Transform nut-scan provided devices into list of device info
	 *
	 *  The nut-scan provided device list is destroyed.
	 *
	 *  \param  dev_list  nut-scan provided device list
	 *
	 *  \return Device info list
	 */
	static devices_t dev2list(nutscan_device_t * dev_list);

	/** Instantiation forbidden */
	NutScanner() {}

	public:

	/**
	 *  \brief  Scan for SNMP devices
	 *
	 *  \param  start_ip    Address range left border
	 *  \param  stop_ip     Address range right border
	 *  \param  us_timeout  Device scan timeout
	 *  \param  attrs       SNMP attributes
	 *
	 *  \return Device list
	 */
	static devices_t devicesSNMP(
		const std::string &    start_ip,
		const std::string &    stop_ip,
		useconds_t             us_timeout,
		const SNMPAttributes & attrs);

	/**
	 *  \brief  Scan for USB devices
	 *
	 *  \return Device list
	 */
	inline static devices_t devicesUSB() {
		// FIXME: Since NUT v2.8.2 nutscan_scan_usb accepts
		// a `nutscan_usb_t * scanopts` to tweak what values
		// it reports -- make use of it in this class.
		// A nullptr value causes safe defaults to be used,
		// as decided by the library.
		nutscan_device_t * dev = ::nutscan_scan_usb(nullptr);

		return dev2list(dev);
	}

	/**
	 *  \brief  Scan for XML/HTTP devices (broadcast)
	 *
	 *  \param  us_timeout  Scan timeout
	 *
	 *  \return Device list
	 */
	inline static devices_t devicesXMLHTTP(useconds_t us_timeout) {
		nutscan_xml_t xml_sec;
		nutscan_device_t * dev;

		::memset(&xml_sec, 0, sizeof(xml_sec));
		/* Set the default values for XML HTTP (run_xml()) */
		xml_sec.port_http = 80;
		xml_sec.port_udp = 4679;
		xml_sec.usec_timeout = us_timeout;
		xml_sec.peername = nullptr;
		dev = ::nutscan_scan_xml_http_range(nullptr, nullptr, us_timeout, &xml_sec);

		return dev2list(dev);
	}

	/**
	 *  \brief  Scan for NUT (pseudo-)devices
	 *
	 *  \param  start_ip    Address range left border
	 *  \param  stop_ip     Address range right border
	 *  \param  port        Port
	 *  \param  us_timeout  Device scan timeout
	 *
	 *  \return Device list
	 */
	inline static devices_t devicesNUT(
		const std::string & start_ip,
		const std::string & stop_ip,
		const std::string & port,
		useconds_t          us_timeout)
	{
		nutscan_device_t * dev = ::nutscan_scan_nut(
			start_ip.c_str(), stop_ip.c_str(), port.c_str(), us_timeout);

		return dev2list(dev);
	}

	/**
	 *  \brief  Scan for Avahi devices
	 *
	 *  \param  us_timeout  Scan timeout
	 *
	 *  \return Device list
	 */
	inline static devices_t devicesAvahi(useconds_t us_timeout) {
		nutscan_device_t * dev = ::nutscan_scan_avahi(us_timeout);

		return dev2list(dev);
	}

	/**
	 *  \brief  Scan for IPMI devices
	 *
	 *  \return Device list
	 */
	static devices_t devicesIPMI(
		const std::string &    start_ip,
		const std::string &    stop_ip,
		const IPMIAttributes & attrs);

	/**
	 *  \brief  Scan for Eaton serial devices
	 *
	 *  \param  ports  List of serial ports
	 *
	 *  \return Device list
	 */
	static devices_t devicesEatonSerial(const std::list<std::string> & ports);

};  // end of class NutScanner


NutScanner::InitFinal NutScanner::s_init_final;


NutScanner::Device::options_t NutScanner::Device::createOptions(nutscan_options_t * opt) {
	options_t options;

	// Create options
	for (; nullptr != opt; opt = opt->next) {
		assert(nullptr != opt->option);

		options.insert(
			options_t::value_type(opt->option,
				nullptr != opt->value ? opt->value : ""));
	}

	return options;
}


NutScanner::Device::Device(nutscan_device_t * dev):
	type(nutscan_device_type_string(dev->type)),
	driver(nullptr != dev->driver ? dev->driver : ""),
	port(nullptr != dev->port ? dev->port : ""),
	options(createOptions(dev->opt))
{}


NutScanner::devices_t NutScanner::dev2list(nutscan_device_t * dev_list) {
	devices_t list;

	nutscan_device_t * dev = dev_list;

	for (; dev != nullptr; dev = dev->next) {
		// Skip devices of type NONE
		// TBD: This happens with the serial scan on an invalid device
		// Should be fixed in libnutscan I think
		if (TYPE_NONE == dev->type)
			continue;

		list.push_back(Device(dev));
	}

	::nutscan_free_device(dev_list);

	return list;
}


NutScanner::devices_t NutScanner::devicesSNMP(
	const std::string &    start_ip,
	const std::string &    stop_ip,
	useconds_t             us_timeout,
	const SNMPAttributes & attrs)
{
	nutscan_snmp_t snmp_attrs;

	::memset(&snmp_attrs, 0, sizeof(snmp_attrs));

	// TBD: const casting is necessery
	// Shouldn't the nutscan_snmp_t items be constant?

	if (!attrs.community.empty())
		snmp_attrs.community = const_cast<char *>(attrs.community.c_str());

	if (!attrs.sec_level.empty())
		snmp_attrs.secLevel = const_cast<char *>(attrs.sec_level.c_str());

	if (!attrs.sec_name.empty())
		snmp_attrs.secName = const_cast<char *>(attrs.sec_name.c_str());

	if (!attrs.auth_passwd.empty())
		snmp_attrs.authPassword = const_cast<char *>(attrs.auth_passwd.c_str());

	if (!attrs.priv_passwd.empty())
		snmp_attrs.privPassword = const_cast<char *>(attrs.priv_passwd.c_str());

	if (!attrs.auth_proto.empty())
		snmp_attrs.authProtocol = const_cast<char *>(attrs.auth_proto.c_str());

	if (!attrs.priv_proto.empty())
		snmp_attrs.privProtocol = const_cast<char *>(attrs.priv_proto.c_str());

	if (!attrs.peer_name.empty())
		snmp_attrs.peername = const_cast<char *>(attrs.peer_name.c_str());

	nutscan_device_t * dev = ::nutscan_scan_snmp(
		start_ip.c_str(), stop_ip.c_str(), us_timeout, &snmp_attrs);

	return dev2list(dev);
}


NutScanner::devices_t NutScanner::devicesIPMI(
	const std::string &    start_ip,
	const std::string &    stop_ip,
	const IPMIAttributes & attrs)
{
	nutscan_ipmi_t ipmi_attrs;

	::memset(&ipmi_attrs, 0, sizeof(ipmi_attrs));

	// TBD: const casting is necessary
	// Shouldn't the nutscan_ipmi_t C-string items be constant?

	if (!attrs.username.empty())
		ipmi_attrs.username = const_cast<char *>(attrs.username.c_str());

	if (!attrs.passwd.empty())
		ipmi_attrs.password = const_cast<char *>(attrs.passwd.c_str());

	ipmi_attrs.authentication_type = attrs.auth_type;
	ipmi_attrs.cipher_suite_id     = attrs.cipher_suite_id;

	if (!attrs.K_g_BMC_key.empty())
		ipmi_attrs.K_g_BMC_key = const_cast<char *>(attrs.K_g_BMC_key.c_str());

	ipmi_attrs.privilege_level  = attrs.priv_level;
	ipmi_attrs.workaround_flags = attrs.wa_flags;
	ipmi_attrs.ipmi_version     = attrs.version;

	nutscan_device_t * dev = ::nutscan_scan_ipmi(
		start_ip.c_str(), stop_ip.c_str(), &ipmi_attrs);

	return dev2list(dev);
}


NutScanner::devices_t NutScanner::devicesEatonSerial(const std::list<std::string> & ports) {
	std::string port_list;

	std::list<std::string>::const_iterator port = ports.begin();

	while (port != ports.end()) {
		port_list += *port;

		++port;

		if (port == ports.end())
			break;

		port_list += ' ';
	}

	nutscan_device_t * dev = ::nutscan_scan_eaton_serial(port_list.c_str());

	return dev2list(dev);
}

#endif // defined WITH_NUTSCANNER


/** nutconf tool specific options */
class NutConfOptions: public Options {
	public:

	/**
	 *  \brief  Option mode (getter/setter)
	 *
	 *  The mode is typically used for options that act as a value (info)
	 *  getter when specified without arguments while if given arguments,
	 *  it sets the value (info).
	 */
	typedef enum {
		NOT_SPECIFIED,  /**< Option not specified on command line */
		GETTER,         /**< Option is a getter                   */
		SETTER,         /**< Option is a setter                   */
	} mode_t;

	/** Listen address specification */
	typedef std::pair<std::string, std::string> ListenAddrSpec;

	/** Device specification */
	struct DeviceSpec {
		/** Device settings map */
		typedef std::map<std::string, std::string> Map;

		std::string id;        /**< Device identifier */
		Map         settings;  /**< Device settings   */
	};  // end of struct DeviceSpec

	/** Notify flags specification */
	typedef std::pair<bool, std::list<std::string> > NotifyFlagsSpec;

	/** Notify flags specifications */
	typedef std::map<std::string, NotifyFlagsSpec> NotifyFlagsSpecs;

	/** Notify messages specifications */
	typedef std::map<std::string, std::string> NotifyMsgSpecs;

	/** User specification */
	struct UserSpec {
		std::string            name;      /**< Username         */
		std::string            passwd;    /**< Password         */
		std::list<std::string> actions;   /**< Actions          */
		std::list<std::string> instcmds;  /**< Instant commands */
	};  // end of struct UserSpec

	/** upsmon user specification */
	struct UpsmonUserSpec: public UserSpec {
		std::string mode;  /**< upsmon mode (master/slave) */
	};  // end of struct UpsmonUserSpec

	/** User specification list */
	typedef std::list<UserSpec *> UserSpecs;

	private:

	/** Unknown options */
	List m_unknown;

	/** Option specification errors */
	std::list<std::string> m_errors;

	/**
	 *  \brief  Option mode getter (including arguments for setters)
	 *
	 *  \param[in]   opt    Option
	 *  \param[out]  args   Option arguments
	 *  \param[in]   order  Option order (1st by default)
	 *
	 *  \retval NOT_SPECIFIED if the option was not specified on the command line
	 *  \retval GETTER        if the option has no arguments
	 *  \retval SETTER        otherwise (option specified with arguments)
	 */
	mode_t optMode(const std::string & opt, Arguments & args, size_t order = 0) const;


	/**
	 *  \brief  Option mode getter
	 *
	 *  \param  opt    Option
	 *  \param  order  Option order (1st by default)
	 *
	 *  \retval NOT_SPECIFIED if the option was not specified on the command line
	 *  \retval GETTER        if the option has no arguments
	 *  \retval SETTER        otherwise (option specified with arguments)
	 */
	mode_t optMode(const std::string & opt, size_t order = 0) const {
		Arguments args;

		return optMode(opt, args, order);
	}

	public:

	/** Options are valid */
	bool valid;

	/** --autoconfigure */
	bool autoconfigure;

	/** --is-configured */
	bool is_configured;

	/** --local argument */
	std::string local;

	/** --system */
	bool system;

	/** --get-mode */
	bool get_mode;

	/** --set-mode argument */
	std::string mode;

	/** --{add|set}-monitor arguments (all the monitors) */
	std::vector<std::string> monitors;

	/** Set monitor options count */
	size_t set_monitor_cnt;

	/** Add monitor options count */
	size_t add_monitor_cnt;

	/** --{add|set}-listen arguments (all the addresses) */
	std::vector<ListenAddrSpec> listen_addrs;

	/** Set listen address options count */
	size_t set_listen_cnt;

	/** Add listen address options count */
	size_t add_listen_cnt;

	/** Device specifications */
	std::vector<DeviceSpec> devices;

	/** Set devices options count */
	size_t set_device_cnt;

	/** Add devices options count */
	size_t add_device_cnt;

	/** Notify flags specifications */
	NotifyFlagsSpecs notify_flags;

	/** Set notify flags options count */
	size_t set_notify_flags_cnt;

	/** Add notify flags options count */
	size_t add_notify_flags_cnt;

	/** Notify messages specifications */
	NotifyMsgSpecs notify_msgs;

	/** Set notify message options count */
	size_t set_notify_msg_cnt;

	/** Notify command */
	std::string notify_cmd;

	/** Shutdown command */
	std::string shutdown_cmd;

	/** Min. supplies */
	std::string min_supplies;

	/** Powerdown flag */
	std::string powerdown_flag;

	/** Users specifications */
	UserSpecs users;

	/** Set user options count */
	size_t set_user_cnt;

	/** Add user options count */
	size_t add_user_cnt;

	/** Verbosity level */
	unsigned int verbose;

	/** Scan NUT devices options count */
	size_t scan_nut_cnt;

	/** Scan USB devices */
	bool scan_usb;

	/** Scan xml_http_devices */
	bool scan_xml_http;

	/** Scan Avahi devices */
	bool scan_avahi;

	/** Scan IPMI devices */
	size_t scan_ipmi_cnt;

	/** Scan SNMP devices options count */
	size_t scan_snmp_cnt;

	/** Scan Eaton serial devices */
	bool scan_serial;

	/** Constructor */
	NutConfOptions(char * const argv[], int argc);

	/** Destructor */
	~NutConfOptions();

	/**
	 *  \brief  Report invalid options to STDERR
	 *
	 *  BEWARE: throws an exception if options are valid.
	 *  Check that using the \ref valid flag.
	 */
	void reportInvalid() const
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw(std::logic_error)
#endif
		;

	/**
	 *  \brief  Get NUT mode
	 *
	 *  \return NUT mode (if --mode option was specified with an argument)
	 */
	inline std::string getMode() const {
		Arguments args;

		if (SETTER != optMode("mode", args))
			return "";

		assert(!args.empty());

		return args.front();
	}

	/**
	 *  \brief  Get monitor definition
	 *
	 *  \param[out]  ups        UPS name
	 *  \param[out]  host_port  Host (possibly including port specification)
	 *  \param[out]  pwr_val    Power value
	 *  \param[out]  user       Username
	 *  \param[out]  passwd     User password
	 *  \param[out]  mode       Monitor mode
	 *  \param[in]   which      Monitor order (1st by default)
	 */
	void getMonitor(
		std::string & ups,
		std::string & host_port,
		std::string & pwr_val,
		std::string & user,
		std::string & passwd,
		std::string & mode,
		size_t        which = 0) const
#if (defined __cplusplus) && (__cplusplus < 201100)
			throw(std::range_error)
#endif
		;

	private:

	/**
	 *  \brief  Check --mode argument validity
	 *
	 *  \param  mode  Mode argument
	 *
	 *  \retval  true  IFF the mode is set correctly
	 *  \retval  false otherwise
	 */
	static bool checkMode(const std::string & mode);

	/**
	 *  \brief  Add another user specification
	 *
	 *  The method is responsible for parsing --{set|add}-user option arguments
	 *  and storing the user specification in \ref users list.
	 *
	 *  \param  args  User specification (raw)
	 */
	void addUser(const Arguments & args);

};  // end of class NutConfOptions


NutConfOptions::mode_t NutConfOptions::optMode(const std::string & opt, Arguments & args, size_t order) const {
	if (!getDouble(opt, args, order))
		return NOT_SPECIFIED;

	if (args.empty())
		return GETTER;

	return SETTER;
}


NutConfOptions::NutConfOptions(char * const argv[], int argc):
	Options(argv, argc),
	valid(true),
	autoconfigure(false),
	is_configured(false),
	system(false),
	get_mode(false),
	set_monitor_cnt(0),
	add_monitor_cnt(0),
	set_listen_cnt(0),
	add_listen_cnt(0),
	set_device_cnt(0),
	add_device_cnt(0),
	set_notify_flags_cnt(0),
	add_notify_flags_cnt(0),
	set_notify_msg_cnt(0),
	set_user_cnt(0),
	add_user_cnt(0),
	verbose(0),
	scan_nut_cnt(0),
	scan_usb(false),
	scan_xml_http(false),
	scan_avahi(false),
	scan_ipmi_cnt(0),
	scan_snmp_cnt(0),
	scan_serial(false)
{
	static const std::string sDash("-");
	static const std::string dDash("--");

	// Specify single-dashed options
	List list = stringsSingle();

	for (List::const_iterator opt = list.begin(); opt != list.end(); ++opt) {
		// Known options
		if ("v" == *opt) {
			++verbose;
		}

		// Unknown option
		else {
			m_unknown.push_back(sDash + *opt);
		}
	}

	// Specify double-dashed options
	list = stringsDouble();

	for (List::const_iterator opt = list.begin(); opt != list.end(); ++opt) {
		// Known options
		if ("autoconfigure" == *opt) {
			if (autoconfigure)
				m_errors.push_back("--autoconfigure option specified more than once");
			else
				autoconfigure = true;
		}
		else if ("is-configured" == *opt) {
			if (is_configured)
				m_errors.push_back("--is-configured option specified more than once");
			else
				is_configured = true;
		}
		else if ("local" == *opt) {
			Arguments args;

			if (!local.empty())
				m_errors.push_back("--local option specified more than once");

			else if (NutConfOptions::SETTER != optMode("local", args))
				m_errors.push_back("--local option requires an argument");

			else if (args.size() > 1)
				m_errors.push_back("Only one directory may be specified with the --local option");

			else
				local = args.front();
		}
		else if ("system" == *opt) {
			if (system)
				m_errors.push_back("--system option specified more than once");
			else
				system = true;
		}
		else if ("get-mode" == *opt) {
			if (get_mode)
				m_errors.push_back("--get-mode option specified more than once");
			else
				get_mode = true;
		}
		else if ("set-mode" == *opt) {
			Arguments args;

			if (!mode.empty())
				m_errors.push_back("--set-mode option specified more than once");

			else if (NutConfOptions::SETTER != optMode(*opt, args))
				m_errors.push_back("--set-mode option requires an argument");

			else if (args.size() > 1)
				m_errors.push_back("Only one argument allowed for the --set-mode option");

			else if (args.size() == 1 && !checkMode(args.front()))
				m_errors.push_back("Unknown NUT mode: \"" + args.front() + "\"");

			else
				mode = args.front();
		}
		else if ("set-monitor" == *opt || "add-monitor" == *opt) {
			size_t * cnt = ('s' == (*opt)[0] ? &set_monitor_cnt : &add_monitor_cnt);

			Arguments args;

			if (NutConfOptions::SETTER != optMode(*opt, args, *cnt))
				m_errors.push_back("--" + *opt + " option requires arguments");

			else if (args.size() != 6)
				m_errors.push_back("--" + *opt + " option requires exactly 6 arguments");

			else
				for (Arguments::const_iterator arg = args.begin(); arg != args.end(); ++arg)
					monitors.push_back(*arg);

			++*cnt;
		}
		else if ("set-listen" == *opt || "add-listen" == *opt) {
			size_t * cnt = ('s' == (*opt)[0] ? &set_listen_cnt : &add_listen_cnt);

			Arguments args;

			if (NutConfOptions::SETTER != optMode(*opt, args, *cnt))
				m_errors.push_back("--" + *opt + " option requires arguments");

			else if (args.size() < 1 || args.size() > 2)
				m_errors.push_back("--" + *opt + " option requires 1 or 2 arguments");

			else {
				ListenAddrSpec addr_port(args.front(), args.size() > 1 ? args.back() : "");

				listen_addrs.push_back(addr_port);
			}

			++*cnt;
		}
		else if ("set-device" == *opt || "add-device" == *opt) {
			size_t * cnt = ('s' == (*opt)[0] ? &set_device_cnt : &add_device_cnt);

			Arguments args;

			if (NutConfOptions::SETTER != optMode(*opt, args, *cnt))
				m_errors.push_back("--" + *opt + " option requires arguments");

			else if (args.size() < 3)
				m_errors.push_back("--" + *opt + " option requires at least 3 arguments");

			else {
				DeviceSpec dev;

				Arguments::const_iterator arg = args.begin();

				assert(args.size() >= 3);

				dev.id = *arg++;

				dev.settings["driver"] = *arg++;
				dev.settings["port"]   = *arg++;

				for (; arg != args.end(); ++arg) {
					size_t eq_pos = (*arg).find('=');

					if (std::string::npos == eq_pos)
						m_errors.push_back("--" + *opt +
							" option extra argument '" +
							*arg + "' is illegal");

					else
						dev.settings[(*arg).substr(0, eq_pos)] =
							(*arg).substr(eq_pos + 1);
				}

				devices.push_back(dev);
			}

			++*cnt;
		}
		else if ("set-notifyflags" == *opt || "add-notifyflags" == *opt) {
			bool     set = 's' == (*opt)[0];
			size_t * cnt = (set ? &set_notify_flags_cnt : &add_notify_flags_cnt);

			Arguments args;

			if (NutConfOptions::SETTER != optMode(*opt, args, *cnt))
				m_errors.push_back("--" + *opt + " option requires arguments");

			else if (args.size() < 2)
				m_errors.push_back("--" + *opt + " option requires at least 2 arguments");

			else {
				Arguments::const_iterator arg = args.begin();

				const std::string & type = *arg++;

				NotifyFlagsSpecs::iterator flags = notify_flags.lower_bound(type);

				if (flags != notify_flags.end() && flags->first == type)
					m_errors.push_back(
						"--" + *opt + " option: conflicting specifications " +
						"for notification type " + type);

				else {
					flags = notify_flags.insert(flags,
							NotifyFlagsSpecs::value_type(
								type,
								NotifyFlagsSpec(set, std::list<std::string>())));

					for (; arg != args.end(); ++arg)
						flags->second.second.push_back(*arg);
				}
			}

			++*cnt;
		}
		else if ("set-notifymsg" == *opt) {
			Arguments args;

			if (NutConfOptions::SETTER != optMode(*opt, args, set_notify_msg_cnt))
				m_errors.push_back("--" + *opt + " option requires arguments");

			else if (args.size() != 2) {
				m_errors.push_back("--" + *opt + " option requires 2 arguments");
				m_errors.push_back("    (perhaps you need to quote the message?)");
			}

			else {
				notify_msgs[args.front()] = args.back();
			}

			++set_notify_msg_cnt;
		}
		else if ("set-notifycmd" == *opt) {
			Arguments args;

			if (!notify_cmd.empty())
				m_errors.push_back("--set-notifycmd option specified more than once");

			else if (NutConfOptions::SETTER != optMode("set-notifycmd", args))
				m_errors.push_back("--set-notifycmd option requires an argument");

			else if (args.size() > 1) {
				m_errors.push_back("Too many arguments for the --set-notifycmd option");
				m_errors.push_back("    (perhaps you need to quote the command?)");
			}

			else
				notify_cmd = args.front();
		}
		else if ("set-shutdowncmd" == *opt) {
			Arguments args;

			if (!shutdown_cmd.empty())
				m_errors.push_back("--set-shutdowncmd option specified more than once");

			else if (NutConfOptions::SETTER != optMode("set-shutdowncmd", args))
				m_errors.push_back("--set-shutdowncmd option requires an argument");

			else if (args.size() > 1) {
				m_errors.push_back("Too many arguments for the --set-shutdowncmd option");
				m_errors.push_back("    (perhaps you need to quote the command?)");
			}

			else
				shutdown_cmd = args.front();
		}
		else if ("set-minsupplies" == *opt) {
			Arguments args;

			if (!min_supplies.empty())
				m_errors.push_back("--set-minsupplies option specified more than once");

			else if (NutConfOptions::SETTER != optMode("set-minsupplies", args))
				m_errors.push_back("--set-minsupplies option requires an argument");

			else if (args.size() > 1) {
				m_errors.push_back("Too many arguments for the --set-minsupplies option");
			}

			else
				min_supplies = args.front();
		}
		else if ("set-powerdownflag" == *opt) {
			Arguments args;

			if (!powerdown_flag.empty())
				m_errors.push_back("--set-powerdownflag option specified more than once");

			else if (NutConfOptions::SETTER != optMode("set-powerdownflag", args))
				m_errors.push_back("--set-powerdownflag option requires an argument");

			else if (args.size() > 1) {
				m_errors.push_back("Too many arguments for the --set-powerdownflag option");
			}

			else
				powerdown_flag = args.front();
		}
		else if ("set-user" == *opt || "add-user" == *opt) {
			size_t * cnt = ('s' == (*opt)[0] ? &set_user_cnt : &add_user_cnt);

			Arguments args;

			if (NutConfOptions::SETTER != optMode(*opt, args, *cnt))
				m_errors.push_back("--" + *opt + " option requires arguments");

			else
				addUser(args);

			++*cnt;
		}
		else if ("verbose" == *opt) {
			++verbose;
		}

#if (defined WITH_NUTSCANNER)

#if (defined WITH_USB)
		else if ("scan-usb" == *opt) {
			if (scan_usb)
				m_errors.push_back("--scan-usb option specified more than once");
			else
				scan_usb = true;
		}
#endif  // defined WITH_USB
		else if ("scan-nut" == *opt) {
			Arguments args;

			getDouble(*opt, args, scan_nut_cnt);

			if (args.size() < 3)
				m_errors.push_back("--scan-nut option requires at least 3 arguments");

			else if (args.size() > 4)
				m_errors.push_back("--scan-nut option requires at most 4 arguments");

			++scan_nut_cnt;
		}
#if (defined WITH_NEON)
		else if ("scan-xml-http" == *opt) {
			if (scan_xml_http)
				m_errors.push_back("--scan-xml-http option specified more than once");
			else {
				Arguments args;

				getDouble(*opt, args);

				if (args.size() > 1)
					m_errors.push_back("--scan-xml-http option accepts only one argument");

				scan_xml_http = true;
			}
		}
#endif  // defined WITH_NEON
#if (defined WITH_AVAHI)
		else if ("scan-avahi" == *opt) {
			if (scan_avahi)
				m_errors.push_back("--scan-avahi option specified more than once");
			else {
				Arguments args;

				getDouble(*opt, args);

				if (args.size() > 1)
					m_errors.push_back("--scan-avahi option accepts only one argument");

				scan_avahi = true;
			}
		}
#endif  // defined WITH_AVAHI
#if (defined WITH_IPMI)
		else if ("scan-ipmi" == *opt) {
			Arguments args;

			getDouble(*opt, args, scan_ipmi_cnt);

			if (args.size() < 2)
				m_errors.push_back("--scan-ipmi option requires at least 2 arguments");

			++scan_ipmi_cnt;
		}
#endif  // defined WITH_IPMI
#if (defined WITH_SNMP)
		else if ("scan-snmp" == *opt) {
			Arguments args;

			getDouble(*opt, args, scan_snmp_cnt);

			if (args.size() < 2)
				m_errors.push_back("--scan-snmp option requires at least 2 arguments");

			++scan_snmp_cnt;
		}
#endif  // defined WITH_SNMP
		else if ("scan-serial" == *opt) {
			if (scan_serial)
				m_errors.push_back("--scan-serial option specified more than once");
			else
				scan_serial = true;
		}

#endif  // defined WITH_NUTSCANNER)

		// Unknown option
		else {
			m_unknown.push_back(dDash + *opt);
		}
	}

	// Options are valid IFF we know all of them
	// and there are no direct binary arguments
	valid = m_unknown.empty() && m_errors.empty() && get().empty();

	// --set-monitor and --add-monitor are mutually exclusive
	if (set_monitor_cnt > 0 && add_monitor_cnt > 0) {
		m_errors.push_back("--set-monitor and --add-monitor options can't both be specified");

		valid = false;
	}

	// --set-listen and --add-listen are mutually exclusive
	if (set_listen_cnt > 0 && add_listen_cnt > 0) {
		m_errors.push_back("--set-listen and --add-listen options can't both be specified");

		valid = false;
	}

	// --set-device and --add-device are mutually exclusive
	if (set_device_cnt > 0 && add_device_cnt > 0) {
		m_errors.push_back("--set-device and --add-device options can't both be specified");

		valid = false;
	}

	// --set-user and --add-user are mutually exclusive
	if (set_user_cnt > 0 && add_user_cnt > 0) {
		m_errors.push_back("--set-user and --add-user options can't both be specified");

		valid = false;
	}
}


NutConfOptions::~NutConfOptions() {
	UserSpecs::iterator user = users.begin();

	for (; user != users.end(); ++user) {
		delete *user;
		*user = nullptr;
	}
}


void NutConfOptions::reportInvalid() const
#if (defined __cplusplus) && (__cplusplus < 201100)
	throw(std::logic_error)
#endif
{
	if (valid)
		throw std::logic_error("No invalid options to report");

	List::const_iterator unknown_opt = m_unknown.begin();

	for (; unknown_opt != m_unknown.end(); ++unknown_opt) {
		std::cerr << "Unknown option: " << *unknown_opt << std::endl;
	}

	std::list<std::string>::const_iterator error = m_errors.begin();

	for (; error != m_errors.end(); ++error) {
		std::cerr << "Option error: " << *error << std::endl;
	}

	// No direct arguments expected
	const Arguments & args = get();

	Arguments::const_iterator arg = args.begin();

	for (; arg != args.end(); ++arg) {
		std::cerr << "Unexpected argument: " << *arg << std::endl;
	}
}


bool NutConfOptions::checkMode(const std::string & mode) {
	if ("standalone" == mode) return true;
	if ("netserver"  == mode) return true;
	if ("netclient"  == mode) return true;
	if ("controlled" == mode) return true;
	if ("manual"     == mode) return true;
	if ("none"       == mode) return true;

	return false;
}


/**
 *  \brief  Automatically destroyed dynamic object pointer
 *
 *  The template class is useful in situation where you need to
 *  create a dynamic object, process its attributes and automatically
 *  destroy it in case of error simply by leaving the scope
 *  (i.e. not having to worry about calling \c delete by hand).
 */
template <typename T>
class autodelete_ptr {
	private:

	T * m_impl;

	/** Cleanup */
	inline void cleanup() {
		if (nullptr != m_impl)
			delete m_impl;

		m_impl = nullptr;
	}

	public:

	/** Constructor (unset) */
	autodelete_ptr(): m_impl(nullptr) {}

	/** Constructor */
	autodelete_ptr(T * ptr): m_impl(ptr) {}

	/** Setter */
	inline autodelete_ptr & operator = (T * ptr) {
		cleanup();

		m_impl = ptr;

		return *this;
	}

	/** Pointer accessor */
	inline operator T * () const { return m_impl; }

	/** Pointer accessor */
	inline T * operator -> () const { return m_impl; }

	/** Pointer overtake (invalidates the object) */
	inline T * give() {
		T * ptr = m_impl;

		m_impl = nullptr;

		return ptr;
	}

	/** Destructor */
	~autodelete_ptr() {
		cleanup();
	}

	private:

	/** Copying is forbidden */
	autodelete_ptr(const autodelete_ptr & orig) {
		NUT_UNUSED_VARIABLE(orig);
	}

	/** Assignment is forbidden */
	autodelete_ptr & operator = (const autodelete_ptr & orig) {
		NUT_UNUSED_VARIABLE(orig);
	}

};  // end of template class autodelete_ptr


void NutConfOptions::addUser(const Options::Arguments & args) {
	assert(args.size() > 0);

	Arguments::const_iterator arg_iter = args.begin();

	// Create new user specification
	autodelete_ptr<UserSpec> user;

	const std::string & name = *arg_iter;

	// upsmon user (apparently)
	// Note that we use pragmatic do ... while (0) loop to enable break
	do if (name.size() >= 6 && name.substr(0, 6) == "upsmon") {
		if (6 == name.size()) {
			m_errors.push_back("upsmon user specification requires monitor mode");

			return;
		}

		// Fall-through to ordinary user specification
		if ('=' != name[6])
			break;

		UpsmonUserSpec * upsmon_user = new UpsmonUserSpec;

		upsmon_user->name = "upsmon";
		upsmon_user->mode = name.substr(7);

		user = upsmon_user;

	} while (0);  // end of pragmatic do ... while (0) loop

	// Ordinary user
	if (nullptr == user) {
		user = new UserSpec;

		user->name = name;
	}

	assert(nullptr != user);

	// Set user attributes
	bool errors = false;

	for (++arg_iter; arg_iter != args.end(); ++arg_iter) {
		const std::string & arg = *arg_iter;

		size_t eq_pos = arg.find('=');

		if (std::string::npos == eq_pos) {
			m_errors.push_back("Illegal user attribute specification: \"" + arg + '"');

			errors = true;

			continue;
		}

		const std::string attr = arg.substr(0, eq_pos);
		const std::string val  = arg.substr(eq_pos + 1);

		if ("password" == attr)
			user->passwd = val;

		else if ("actions" == attr)
			user->actions.push_back(val);

		else if ("instcmds" == attr)
			user->instcmds.push_back(val);

		else {
			m_errors.push_back("Unknown user attribute: \"" + attr + '"');

			errors = true;
		}
	}

	if (errors)
		return;

	// Store user specification
	users.push_back(user.give());
}


void NutConfOptions::getMonitor(
	std::string & ups,
	std::string & host_port,
	std::string & pwr_val,
	std::string & user,
	std::string & passwd,
	std::string & mode_arg,
	size_t        which) const
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw(std::range_error)
#endif
{
	if (which >= monitors.size() / 6)
		throw std::range_error("INTERNAL ERROR: monitors index overflow");

	size_t base_idx = 6 * which;

	assert(monitors.size() >= base_idx + 6);

	ups       = monitors[base_idx];
	host_port = monitors[base_idx + 1];
	pwr_val   = monitors[base_idx + 2];
	user      = monitors[base_idx + 3];
	passwd    = monitors[base_idx + 4];
	mode_arg  = monitors[base_idx + 5];
}


/**
 *  \brief  Sources configuration object from file (if exists)
 *
 *  If the file doesn't exist, the conf. object is unchanged
 *  (and the result is indicated by the return value).
 *  If the file exists, but can't be parsed, an error is reported
 *  and the execution is terminated.
 *
 *  \param  config     Configuration object
 *  \param  file_name  File name
 *
 *  \retval true  if the configuration file was sourced
 *  \retval false if the file doesn't exist
 */
static bool source(nut::Serialisable * config, const std::string & file_name) {
	nut::NutFile file(file_name);

	if (!file.exists())
		return false;

	file.openx();

	bool parsed_ok = config->parseFrom(file);

	file.closex();

	if (parsed_ok)
		return true;

	std::cerr << "Error: Failed to parse " << file_name << std::endl;

	::exit(1);
}


/**
 *  \brief  Store configuration object to file
 *
 *  If the file exists, it's rewritten.
 *
 *  \param  config     Configuration object
 *  \param  file_name  File name
 */
static void store(nut::Serialisable * config, const std::string & file_name) {
	nut::NutFile file(file_name, nut::NutFile::WRITE_ONLY);

	bool written_ok = config->writeTo(file);

	file.closex();

	if (written_ok)
		return;

	std::cerr << "Error: Failed to write " << file_name << std::endl;

	::exit(1);
}


/**
 *  \brief  Check whether NUT was configured
 *
 *  \param  etc  Configuration directory
 *
 *  \retval true  IFF nut.conf exists and MODE != none
 *  \retval false otherwise
 */
static bool isConfigured(const std::string & etc) {
	nut::NutFile nut_conf_file(etc + "/nut.conf");

	if (!nut_conf_file.exists())
		return false;

	nut_conf_file.openx();

	nut::NutConfiguration nut_conf;

	nut_conf.parseFrom(nut_conf_file);

	return
		nut::NutConfiguration::MODE_UNKNOWN != nut_conf.mode &&
		nut::NutConfiguration::MODE_NONE    != nut_conf.mode;
}


/**
 *  \brief  Transform monitor specification
 *
 *  Transform monitor specification from cmd. line to monitor configuration.
 *
 *  \param  i        Monitor index
 *  \param  options  nutconf options
 *
 *  \return Monitor configuration
 */
static nut::UpsmonConfiguration::Monitor monitor(
	size_t                 i,
	const NutConfOptions & options)
{
	nut::UpsmonConfiguration::Monitor monitor;

	std::string host_port, pwr_val, mode;

	options.getMonitor(
		monitor.upsname,  host_port,        pwr_val,
		monitor.username, monitor.password, mode,
		i);

	// Parse host[:port]
	unsigned short port = 0;

	size_t colon_idx = host_port.rfind(':');

	if (std::string::npos != colon_idx) {
		std::stringstream ss(host_port.substr(colon_idx + 1));

		if ((ss >> port).fail()) {
			std::cerr
				<< "Error: failed to parse host specification \""
				<< host_port << '"' << std::endl;

			::exit(1);
		}
	}

	// Parse power value
	unsigned int power_value;

	std::stringstream ss(pwr_val);

	if ((ss >> power_value).fail()) {
		std::cerr
			<< "Error: failed to parse power value \""
			<< pwr_val << '"' << std::endl;

		::exit(1);
	}

	monitor.hostname   = host_port.substr(0, colon_idx);
	monitor.port       = port;
	monitor.powerValue = power_value;
	monitor.isPrimary  = ("primary" == mode || "master" == mode);

	return monitor;
}


/**
 *  \brief  NUT mode getter
 *
 *  \param  etc   Configuration directory
 *
 *  \return NUT mode (as string)
 */
static std::string getMode(const std::string & etc) {
	std::string nut_conf_file(etc + "/nut.conf");
	std::stringstream e;
	nut::NutConfiguration nut_conf;

	// Source previous configuration
	source(&nut_conf, nut_conf_file);

	nut::NutConfiguration::NutMode mode = nut_conf.mode;

	switch (mode) {
		case nut::NutConfiguration::MODE_UNKNOWN:    return "unknown";
		case nut::NutConfiguration::MODE_NONE:       return "none";
		case nut::NutConfiguration::MODE_STANDALONE: return "standalone";
		case nut::NutConfiguration::MODE_NETSERVER:  return "netserver";
		case nut::NutConfiguration::MODE_NETCLIENT:  return "netclient";
		case nut::NutConfiguration::MODE_CONTROLLED: return "controlled";
		case nut::NutConfiguration::MODE_MANUAL:     return "manual";

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
			break;
	}

	e << "INTERNAL ERROR: Unknown NUT mode: " << mode;
	throw std::logic_error(e.str());
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
}


/**
 *  \brief  NUT mode setter
 *
 *  \param  mode  Mode
 *  \param  etc   Configuration directory
 */
static void setMode(const std::string & mode, const std::string & etc) {
	std::string nut_conf_file(etc + "/nut.conf");

	nut::NutConfiguration nut_conf;

	// Source previous configuration (if any)
	source(&nut_conf, nut_conf_file);

	// Set mode
	nut_conf.mode = nut::NutConfiguration::NutModeFromString(mode);

	// Store configuration
	store(&nut_conf, nut_conf_file);
}


/**
 *  \brief  Set monitors in upsmon.conf
 *
 *  \param  monitors  Monitor list
 *  \param  etc       Configuration directory
 *  \param  keep_ex   Keep existing entries (discard by default)
 */
static void setMonitors(
	const std::list<nut::UpsmonConfiguration::Monitor> & monitors,
	const std::string & etc, bool keep_ex = false)
{
	std::string upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	source(&upsmon_conf, upsmon_conf_file);

	// Remove existing monitors (unless we want to keep them)
	if (!keep_ex)
		upsmon_conf.monitors.clear();

	// Add monitors to the current ones (if any)
	std::list<nut::UpsmonConfiguration::Monitor>::const_iterator
		monitor = monitors.begin();

	for (; monitor != monitors.end(); ++monitor)
		upsmon_conf.monitors.push_back(*monitor);

	// Store configuration
	store(&upsmon_conf, upsmon_conf_file);
}


/**
 *  \brief  Transform listen address specification
 *
 *  Transform listen address specification from cmd. line to listen address configuration.
 *
 *  \param  i        Listen address index
 *  \param  options  nutconf options
 *
 *  \return Listen address configuration
 */
static nut::UpsdConfiguration::Listen listenAddr(
	size_t                 i,
	const NutConfOptions & options)
{
	nut::UpsdConfiguration::Listen listen_addr;

	const NutConfOptions::ListenAddrSpec & addr_spec = options.listen_addrs[i];

	listen_addr.address = addr_spec.first;

	// Parse port
	if (!addr_spec.second.empty()) {
		unsigned short port = 0;

		std::stringstream ss(addr_spec.second);

		if ((ss >> port).fail()) {
			std::cerr
				<< "Error: failed to parse port specification \""
				<< addr_spec.second << '"' << std::endl;

			::exit(1);
		}

		listen_addr.port = port;
	}

	return listen_addr;
}


/**
 *  \brief  Set listen addresses in upsd.conf
 *
 *  \param  listen_addrs  Address list
 *  \param  etc           Configuration directory
 *  \param  keep_ex       Keep existing entries (discard by default)
 */
static void setListenAddrs(
	const std::list<nut::UpsdConfiguration::Listen> & listen_addrs,
	const std::string & etc, bool keep_ex = false)
{
	std::string upsd_conf_file(etc + "/upsd.conf");

	nut::UpsdConfiguration upsd_conf;

	// Source previous configuration (if any)
	source(&upsd_conf, upsd_conf_file);

	// Remove existing listen addresses (unless we want to keep them)
	if (!keep_ex)
		upsd_conf.listens.clear();

	// Add listen addresses to the current ones (if any)
	std::list<nut::UpsdConfiguration::Listen>::const_iterator
		listen = listen_addrs.begin();

	for (; listen != listen_addrs.end(); ++listen)
		upsd_conf.listens.push_back(*listen);

	// Store configuration
	store(&upsd_conf, upsd_conf_file);
}


/**
 *  \brief  Set devices in ups.conf
 *
 *  \param  devices  Device list
 *  \param  etc      Configuration directory
 *  \param  keep_ex  Keep existing entries (discard by default)
 */
static void setDevices(
	const std::vector<NutConfOptions::DeviceSpec> & devices,
	const std::string & etc, bool keep_ex = false)
{
	std::string ups_conf_file(etc + "/ups.conf");

	nut::UpsConfiguration ups_conf;

	// Source previous configuration (if any)
	source(&ups_conf, ups_conf_file);

	// Remove existing devices (unless we want to keep them)
	if (!keep_ex) {
		nut::UpsConfiguration::SectionMap::iterator
			ups = ups_conf.sections.begin();

		for (; ups != ups_conf.sections.end(); ++ups) {
			// Keep global section
			if (ups->first.empty())
				continue;

			ups_conf.sections.erase(ups);
		}
	}

	// Add devices to the current ones (if any)
	std::vector<NutConfOptions::DeviceSpec>::const_iterator
		dev = devices.begin();

	for (; dev != devices.end(); ++dev) {
		const std::string & id = (*dev).id;

		NutConfOptions::DeviceSpec::Map::const_iterator
			setting = (*dev).settings.begin();

		for (; setting != (*dev).settings.end(); ++setting)
			ups_conf.setKey(id, setting->first, setting->second);
	}

	// Store configuration
	store(&ups_conf, ups_conf_file);
}


/**
 *  \brief  Set notify flags in upsmon.conf
 *
 *  \param  flags  Notify flags specifications
 *  \param  etc    Configuration directory
 */
static void setNotifyFlags(
	const NutConfOptions::NotifyFlagsSpecs & flags,
	const std::string & etc)
{
	std::string upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	source(&upsmon_conf, upsmon_conf_file);

	NutConfOptions::NotifyFlagsSpecs::const_iterator specs = flags.begin();

	for (; specs != flags.end(); ++specs) {
		// Resolve notification type
		nut::UpsmonConfiguration::NotifyType type =
			nut::UpsmonConfiguration::NotifyTypeFromString(specs->first);

		if (nut::UpsmonConfiguration::NOTIFY_TYPE_MAX == type) {
			std::cerr
				<< "Error: failed to parse notification type specification \""
				<< specs->first << '"' << std::endl;

			::exit(1);
		}

		nut::Settable<unsigned int> & sum =
			upsmon_conf.notifyFlags[type];

		// Clear current flags (unless we want to keep them)
		if (specs->second.first || !sum.set())
			sum = nut::UpsmonConfiguration::NOTIFY_IGNORE;

		// Assemble flags
		std::list<std::string>::const_iterator spec = specs->second.second.begin();

		for (; spec != specs->second.second.end(); ++spec) {
			nut::UpsmonConfiguration::NotifyFlag flag =
				nut::UpsmonConfiguration::NotifyFlagFromString(*spec);

			sum |= static_cast<unsigned int>(flag);
		}
	}

	// Store configuration
	store(&upsmon_conf, upsmon_conf_file);
}


/**
 *  \brief  Set notify messages in upsmon.conf
 *
 *  \param  msgs  Notify messages specifications
 *  \param  etc   Configuration directory
 */
static void setNotifyMsgs(
	const NutConfOptions::NotifyMsgSpecs & msgs,
	const std::string & etc)
{
	std::string upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	source(&upsmon_conf, upsmon_conf_file);

	NutConfOptions::NotifyMsgSpecs::const_iterator spec = msgs.begin();

	for (; spec != msgs.end(); ++spec) {
		// Resolve notification type
		nut::UpsmonConfiguration::NotifyType type =
			nut::UpsmonConfiguration::NotifyTypeFromString(spec->first);

		if (nut::UpsmonConfiguration::NOTIFY_TYPE_MAX == type) {
			std::cerr
				<< "Error: failed to parse notification type specification \""
				<< spec->first << '"' << std::endl;

			::exit(1);
		}

		// Set message
		upsmon_conf.notifyMessages[type] = spec->second;
	}

	// Store configuration
	store(&upsmon_conf, upsmon_conf_file);
}


/**
 *  \brief  Set notify command in upsmon.conf
 *
 *  \param  cmd  Notify command
 *  \param  etc  Configuration directory
 */
static void setNotifyCmd(const std::string & cmd, const std::string & etc)
{
	std::string upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	source(&upsmon_conf, upsmon_conf_file);

	upsmon_conf.notifyCmd = cmd;

	// Store configuration
	store(&upsmon_conf, upsmon_conf_file);
}


/**
 *  \brief  Set shutdown command in upsmon.conf
 *
 *  \param  cmd  Shutdown command
 *  \param  etc  Configuration directory
 */
static void setShutdownCmd(const std::string & cmd, const std::string & etc)
{
	std::string upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	source(&upsmon_conf, upsmon_conf_file);

	upsmon_conf.shutdownCmd = cmd;

	// Store configuration
	store(&upsmon_conf, upsmon_conf_file);
}


/**
 *  \brief  Set minimum of power supplies in upsmon.conf
 *
 *  \param  min_supplies  Minimum of power supplies
 *  \param  etc           Configuration directory
 */
static void setMinSupplies(const std::string & min_supplies, const std::string & etc) {
	std::string upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	source(&upsmon_conf, upsmon_conf_file);

	unsigned int min;

	std::stringstream ss(min_supplies);

	if ((ss >> min).fail()) {
		std::cerr
		<< "Error: invalid min. power supplies specification: \""
		<< min_supplies << '"' << std::endl;

		::exit(1);
	}

	upsmon_conf.minSupplies = min;

	// Store configuration
	store(&upsmon_conf, upsmon_conf_file);
}


/**
 *  \brief  Set powerdown flag file in upsmon.conf
 *
 *  \param  powerdown_flag  Powerdown flag file
 *  \param  etc             Configuration directory
 */
static void setPowerdownFlag(const std::string & powerdown_flag, const std::string & etc) {
	std::string upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	source(&upsmon_conf, upsmon_conf_file);

	upsmon_conf.powerDownFlag = powerdown_flag;

	// Store configuration
	store(&upsmon_conf, upsmon_conf_file);
}


/**
 *  \brief  Set users in upsd.users
 *
 *  \param  users    User list
 *  \param  etc      Configuration directory
 *  \param  keep_ex  Keep existing entries (discard by default)
 */
static void setUsers(
	const NutConfOptions::UserSpecs & users,
	const std::string & etc, bool keep_ex = false)
{
	std::string upsd_users_file(etc + "/upsd.users");

	nut::UpsdUsersConfiguration upsd_users;

	// Source previous configuration (if any)
	source(&upsd_users, upsd_users_file);

	// Remove existing users (unless we want to keep them)
	if (!keep_ex) {
		nut::UpsdUsersConfiguration::SectionMap::iterator
			user = upsd_users.sections.begin();

		for (; user != upsd_users.sections.end(); ++user) {
			// Keep global section
			if (user->first.empty())
				continue;

			upsd_users.sections.erase(user);
		}
	}

	// Set/add users and/or their attributes
	NutConfOptions::UserSpecs::const_iterator
		user_iter = users.begin();

	for (; user_iter != users.end(); ++user_iter) {
		const NutConfOptions::UserSpec * user = *user_iter;

		const std::string & username = user->name;

		// Set password
		if (!user->passwd.empty())
			upsd_users.setPassword(username, user->passwd);

		// Set actions
		std::list<std::string>::const_iterator action = user->actions.begin();

		for (; action != user->actions.end(); ++action)
			upsd_users.addActions(username, nut::ConfigParamList(1, *action));

		// Set instant commands
		std::list<std::string>::const_iterator cmd = user->instcmds.begin();

		for (; cmd != user->instcmds.end(); ++cmd)
			upsd_users.addInstantCommands(username, nut::ConfigParamList(1, *cmd));

		// upsmon user-specific settings
		if ("upsmon" == username) {
			const NutConfOptions::UpsmonUserSpec * upsmon_user =
				static_cast<const NutConfOptions::UpsmonUserSpec *>(user);

			// Set upsmon mode
			nut::UpsdUsersConfiguration::upsmon_mode_t mode =
				nut::UpsdUsersConfiguration::UPSMON_UNDEF;

			if ("primary" == upsmon_user->mode || "master" == upsmon_user->mode)
				mode = nut::UpsdUsersConfiguration::UPSMON_PRIMARY;

			else if ("secondary" == upsmon_user->mode || "slave" == upsmon_user->mode)
				mode = nut::UpsdUsersConfiguration::UPSMON_SECONDARY;

			else {
				std::cerr
					<< "Error: Invalid upsmon mode specification: \""
					<< upsmon_user->mode << '"' << std::endl;

				::exit(1);
			}

			upsd_users.setUpsmonMode(mode);
		}
	}

	// Store configuration
	store(&upsd_users, upsd_users_file);
}


#if (defined WITH_NUTSCANNER)

/**
 *  \brief  Print devices info
 *
 *  \param  devices  Device list
 *  \param  verbose  Verbosity level
 */
static void printDevicesInfo(const NutScanner::devices_t & devices, unsigned int verbose = 0) {
	NutScanner::devices_t::const_iterator dev_iter = devices.begin();

	nut::GenericConfiguration devices_conf;

	unsigned int dev_no = 1;

	for (; dev_iter != devices.end(); ++dev_iter, ++dev_no) {
		const NutScanner::Device & dev = *dev_iter;

		// Print just plain list
		if (verbose == 0)
			std::cout
			<< dev.type << ' '
			<< dev.driver << ' '
			<< dev.port << std::endl;

		// Assemble full info
		else {
			std::stringstream name;

			name << "device_type_" << dev.type << "_no_";

			name.width(3);
			name.fill('0');

			name << dev_no;

			nut::GenericConfigSection & device_conf = devices_conf[name.str()];

			device_conf.name = name.str();

			// Set driver
			nut::GenericConfigSectionEntry & driver = device_conf["driver"];

			driver.name = "driver";
			driver.values.push_back(dev.driver);

			// Set port
			nut::GenericConfigSectionEntry & port = device_conf["port"];

			port.name = "port";
			port.values.push_back(dev.port);

			// Set options
			NutScanner::Device::options_t::const_iterator
				opt = dev.options.begin();

			for (; opt != dev.options.end(); ++opt) {
				nut::GenericConfigSectionEntry & option = device_conf[opt->first];

				option.name = opt->first;
				option.values.push_back(opt->second);
			}
		}
	}

	// Print full info
	if (0 != verbose) {
		nut::NutMemory info;

		devices_conf.writeTo(info);

		std::string info_str;

		assert(nut::NutStream::NUTS_OK == info.getString(info_str));

		std::cout << info_str;
	}
}


/**
 *  \brief  Scan for SNMP devices
 *
 *  \param  options  Options
 */
static void scanSNMPdevices(const NutConfOptions & options) {
	for (size_t i = 0; ; ++i) {
		NutConfOptions::Arguments args;

		bool ok = options.getDouble("scan-snmp", args, i);

		if (!ok) break;

		// Sanity checks
		assert(args.size() >= 2);

		NutConfOptions::Arguments::const_iterator arg = args.begin();

		const std::string & start_ip = *arg++;
		const std::string & stop_ip  = *arg++;

		// TBD: where should we get the default?
		useconds_t us_timeout = 1000000;

		NutScanner::SNMPAttributes attrs;

		// Parse <attr>=<val> pairs
		bool errors = false;

		for (; arg != args.end(); ++arg) {
			size_t eq_pos = (*arg).find('=');

			if (std::string::npos == eq_pos) {
				std::cerr
				<< "Error: Invalid SNMP attribute specification: \""
				<< *arg << '"' << std::endl;

				errors = true;

				continue;
			}

			std::string attr = (*arg).substr(0, eq_pos);
			std::string val  = (*arg).substr(eq_pos + 1);

			if ("timeout" == attr) {
				std::stringstream ss(val);

				if ((ss >> us_timeout).fail()) {
					std::cerr
					<< "Error: Invalid SNMP timeout specification: \""
					<< val << '"' << std::endl;

					errors = true;

					continue;
				}
			}
			else if ("community" == attr) {
				attrs.community = val;
			}
			else if ("sec-level" == attr) {
				attrs.sec_level = val;
			}
			else if ("sec-name" == attr) {
				attrs.sec_name = val;
			}
			else if ("auth-password" == attr) {
				attrs.auth_passwd = val;
			}
			else if ("priv-password" == attr) {
				attrs.priv_passwd = val;
			}
			else if ("auth-protocol" == attr) {
				attrs.auth_proto = val;
			}
			else if ("priv-protocol" == attr) {
				attrs.priv_proto = val;
			}
			else if ("peer-name" == attr) {
				attrs.peer_name = val;
			}

			else {
				std::cerr
				<< "Error: Unknown SNMP attribute: \""
				<< attr << '"' << std::endl;

				errors = true;
			}
		}

		if (errors) continue;

		NutScanner::devices_t devices = NutScanner::devicesSNMP(
			start_ip, stop_ip, us_timeout, attrs);

		printDevicesInfo(devices, options.verbose);
	}
}


/**
 *  \brief  Scan for USB devices
 *
 *  \param  options  Options
 */
static void scanUSBdevices(const NutConfOptions & options) {
	NutScanner::devices_t devices = NutScanner::devicesUSB();

	printDevicesInfo(devices, options.verbose);
}


/**
 *  \brief  Scan for NUT devices
 *
 *  \param  options  Options
 */
static void scanNUTdevices(const NutConfOptions & options) {
	for (size_t i = 0; ; ++i) {
		NutConfOptions::Arguments args;

		bool ok = options.getDouble("scan-nut", args, i);

		if (!ok) break;

		// Sanity checks
		assert(args.size() >= 3);

		NutConfOptions::Arguments::const_iterator arg = args.begin();

		const std::string & start_ip = *arg++;
		const std::string & stop_ip  = *arg++;
		const std::string & port     = *arg++;

		// TBD: where should we get the default?
		useconds_t us_timeout = 1000000;

		if (arg != args.end()) {
			std::stringstream ss(*arg);

			ss >> us_timeout;
		}

		NutScanner::devices_t devices = NutScanner::devicesNUT(
			start_ip, stop_ip, port, us_timeout);

		printDevicesInfo(devices, options.verbose);
	}
}


/**
 *  \brief  Scan for XML/HTTP devices
 *
 *  \param  options  Options
 */
static void scanXMLHTTPdevices(const NutConfOptions & options) {
	NutConfOptions::Arguments args;

	bool ok = options.getDouble("scan-xml-http", args);

	// Sanity checks
	assert(ok);

	// TBD: where should we get the default?
	useconds_t us_timeout = 1000000;

	if (!args.empty()) {
		std::stringstream ss(args.front());

		ss >> us_timeout;
	}

	NutScanner::devices_t devices = NutScanner::devicesXMLHTTP(us_timeout);

	printDevicesInfo(devices, options.verbose);
}


/**
 *  \brief  Scan for Avahi devices
 *
 *  \param  options  Options
 */
static void scanAvahiDevices(const NutConfOptions & options) {
	NutConfOptions::Arguments args;

	bool ok = options.getDouble("scan-avahi", args);

	// Sanity checks
	assert(ok);

	// TBD: where should we get the default?
	useconds_t us_timeout = 1000000;

	if (!args.empty()) {
		std::stringstream ss(args.front());

		ss >> us_timeout;
	}

	NutScanner::devices_t devices = NutScanner::devicesAvahi(us_timeout);

	printDevicesInfo(devices, options.verbose);
}


/**
 *  \brief  Scan for IPMI devices
 *
 *  \param  options  Options
 */
static void scanIPMIdevices(const NutConfOptions & options) {
	for (size_t i = 0; ; ++i) {
		NutConfOptions::Arguments args;

		bool ok = options.getDouble("scan-ipmi", args, i);

		if (!ok) break;

		// Sanity checks
		assert(args.size() >= 2);

		NutConfOptions::Arguments::const_iterator arg = args.begin();

		const std::string & start_ip = *arg++;
		const std::string & stop_ip  = *arg++;

		NutScanner::IPMIAttributes attrs;

		// Parse <attr>=<val> pairs
		bool errors = false;

		for (; arg != args.end(); ++arg) {
			size_t eq_pos = (*arg).find('=');

			if (std::string::npos == eq_pos) {
				std::cerr
				<< "Error: Invalid IPMI attribute specification: \""
				<< *arg << '"' << std::endl;

				errors = true;

				continue;
			}

			std::string attr = (*arg).substr(0, eq_pos);
			std::string val  = (*arg).substr(eq_pos + 1);

			if ("username" == attr) {
				attrs.username = val;
			}
			else if ("password" == attr) {
				attrs.passwd = val;
			}
			else if ("auth-type" == attr) {
				if ("none" == val)
					attrs.auth_type = IPMI_AUTHENTICATION_TYPE_NONE;
				else if ("MD2" == val)
					attrs.auth_type = IPMI_AUTHENTICATION_TYPE_MD2;
				else if ("MD5" == val)
					attrs.auth_type = IPMI_AUTHENTICATION_TYPE_MD5;
				else if ("plain-password" == val)
					attrs.auth_type = IPMI_AUTHENTICATION_TYPE_STRAIGHT_PASSWORD_KEY;
				else if ("OEM" == val)
					attrs.auth_type = IPMI_AUTHENTICATION_TYPE_OEM_PROP;
				else if ("RMCPplus" == val)
					attrs.auth_type = IPMI_AUTHENTICATION_TYPE_RMCPPLUS;
				else {
					std::cerr
					<< "Error: Invalid IPMI auth. type: \""
					<< val << '"' << std::endl;

					errors = true;

					continue;
				}
			}
			else if ("cipher-suite-id" == attr) {
				std::stringstream ss(val);

				if ((ss >> attrs.cipher_suite_id).fail()) {
					std::cerr
					<< "Error: Invalid IPMI cipher suite ID: \""
					<< val << '"' << std::endl;

					errors = true;

					continue;
				}
			}
			else if ("K-g-BMC-key" == attr) {
				attrs.K_g_BMC_key = val;
			}
			else if ("priv-level" == attr) {
				std::stringstream ss(val);

				if ((ss >> attrs.priv_level).fail()) {
					std::cerr
					<< "Error: Invalid IPMI priv. level: \""
					<< val << '"' << std::endl;

					errors = true;

					continue;
				}
			}
			else if ("workaround-flags" == attr) {
				std::stringstream ss(val);

				if ((ss >> attrs.wa_flags).fail()) {
					std::cerr
					<< "Error: Invalid IPMI workaround flags: \""
					<< val << '"' << std::endl;

					errors = true;

					continue;
				}
			}

			else if ("version" == attr) {
				if ("1.5" == val)
					attrs.version = IPMI_1_5;
				else if ("2.0" == val)
					attrs.version = IPMI_2_0;
				else {
					std::cerr
					<< "Error: Unsupported IPMI version "
					<< val << std::endl;

					errors = true;

					continue;
				}
			}

			else {
				std::cerr
				<< "Error: Unknown IPMI attribute: \""
				<< attr << '"' << std::endl;

				errors = true;
			}
		}

		if (errors) continue;

		NutScanner::devices_t devices = NutScanner::devicesIPMI(
			start_ip, stop_ip, attrs);

		printDevicesInfo(devices, options.verbose);
	}
}


/**
 *  \brief  Scan for serial devices devices
 *
 *  \param  options  Options
 */
static void scanSerialDevices(const NutConfOptions & options) {
	NutConfOptions::Arguments args;

	bool ok = options.getDouble("scan-serial", args);

	// Sanity checks
	assert(ok);

	NutScanner::devices_t devices = NutScanner::devicesEatonSerial(args);

	printDevicesInfo(devices, options.verbose);
}

#endif  // defined WITH_NUTSCANNER


/**
 *  \brief  Main routine (exceptions unsafe)
 *
 *  \param  argc  Argument count
 *  \param  argv  Arguments
 *
 *  \return 0 always (exits on error)
 */
static int mainx(int argc, char * const argv[]) {
	const char	*prog = xbasename(argv[0]);
	char	*s = nullptr;

	// Get options
	NutConfOptions options(argv, argc);

	// Usage
	if (options.exists("help") || options.existsSingle("h")) {
		Usage::print(prog);

		::exit(0);
	}

	// Usage
	if (options.exists("version") || options.existsSingle("V")) {
		Usage::printVersion(prog);

		::exit(0);
	}

	// Check that command-line options validity
	if (!options.valid) {
		options.reportInvalid();

		Usage::print(argv[0]);

		::exit(1);
	}

	// Set configuration directory
	std::string etc(CONFPATH);

	s = ::getenv("NUT_CONFPATH");
	if (s != nullptr && !options.system)
		etc = s;

	if (!options.local.empty()) {
		etc = options.local;
	}

	// Check configuration directory availability
	nut::NutFile etc_dir(etc);

	if (!etc_dir.exists()) {
		std::cerr << "Error: Configuration directory " << etc << " isn't available" << std::endl;

		::exit(1);
	}

	// --is-configured query
	if (options.is_configured) {
		bool is_configured = isConfigured(etc);

		std::cout << (is_configured ? "true" : "false") << std::endl;

		::exit(is_configured ? 0 : 1);
	}

	// --get-mode
	if (options.get_mode) {
		std::cout << getMode(etc) << std::endl;
	}

	// --set-mode
	if (!options.mode.empty()) {
		setMode(options.mode, etc);
	}

	// Monitors were set
	if (!options.monitors.empty()) {
		std::list<nut::UpsmonConfiguration::Monitor> monitors;

		for (size_t n = options.monitors.size() / 6, i = 0; i < n; ++i) {
			monitors.push_back(monitor(i, options));
		}

		setMonitors(monitors, etc, options.add_monitor_cnt > 0);
	}

	// Listen addresses were set
	if (!options.listen_addrs.empty()) {
		std::list<nut::UpsdConfiguration::Listen> listen_addrs;

		for (size_t i = 0; i < options.listen_addrs.size(); ++i) {
			listen_addrs.push_back(listenAddr(i, options));
		}

		setListenAddrs(listen_addrs, etc, options.add_listen_cnt > 0);
	}

	// Devices were set
	if (!options.devices.empty()) {
		setDevices(options.devices, etc, options.add_device_cnt > 0);
	}

	// Notify flags were set
	if (!options.notify_flags.empty()) {
		setNotifyFlags(options.notify_flags, etc);
	}

	// Notify messages were set
	if (!options.notify_msgs.empty()) {
		setNotifyMsgs(options.notify_msgs, etc);
	}

	// Notify command was set
	if (!options.notify_cmd.empty()) {
		setNotifyCmd(options.notify_cmd, etc);
	}

	// Shutdown command was set
	if (!options.shutdown_cmd.empty()) {
		setShutdownCmd(options.shutdown_cmd, etc);
	}

	// Min. of power supplies was set
	if (!options.min_supplies.empty()) {
		setMinSupplies(options.min_supplies, etc);
	}

	// Powerdown flag file was set
	if (!options.powerdown_flag.empty()) {
		setPowerdownFlag(options.powerdown_flag, etc);
	}

	// Users were set
	if (!options.users.empty()) {
		setUsers(options.users, etc, options.add_user_cnt > 0);
	}

#if (defined WITH_NUTSCANNER)

	// SNMP devices scan
	if (options.scan_snmp_cnt) {
		scanSNMPdevices(options);
	}

	// USB devices scan
	if (options.scan_usb) {
		scanUSBdevices(options);
	}

	// NUT devices scan
	if (options.scan_nut_cnt) {
		scanNUTdevices(options);
	}

	// XML/HTTP devices scan
	if (options.scan_xml_http) {
		scanXMLHTTPdevices(options);
	}

	// Avahi devices scan
	if (options.scan_avahi) {
		scanAvahiDevices(options);
	}

	// IPMI devices scan
	if (options.scan_ipmi_cnt) {
		scanIPMIdevices(options);
	}

	// Serial devices scan
	if (options.scan_serial) {
		scanSerialDevices(options);
	}

#endif  // defined WITH_NUTSCANNER

	return 0;
}


/**
 *  \brief  Main routine exception-safe wrapper
 *
 *  Exceptions should never leak...
 *
 *  \param  argc  Argument count
 *  \param  argv  Arguments
 */
int main(int argc, char * const argv[]) {
	try {
		return mainx(argc, argv);
	}
	catch (const std::exception & e) {
		std::cerr
			<< "Error: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr
			<< "INTERNAL ERROR: exception of unknown origin caught" << std::endl
			<< "Please issue a bug report to nut-upsdev@lists.alioth.debian.org"
			<< std::endl;
	}

	::exit(128);
}
