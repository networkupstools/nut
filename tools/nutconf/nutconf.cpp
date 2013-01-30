#include "config.h"
#include "nutconf.h"
#include "nutstream.hpp"

extern "C" {
#include "nut-scan.h"
#include "nutscan-init.h"
#include "nutscan-device.h"
}

#include <iostream>
#include <list>
#include <vector>
#include <map>
#include <stdexcept>
#include <cassert>


class Usage {
	private:

	/** Usage text */
	static const char * s_text[];

	/** Private constructor (no instances) */
	Usage() {}

	public:

	/** Print usage */
	static void print(const std::string & bin);

};  // end of class usage


const char * Usage::s_text[] = {
"    --help                              Display this help and exit",
"    --autoconfigure                     Perform autoconfiguration",
"    --is-configured                     Checks whether NUT is configured",
"    --local <directory>                 Sets configuration directory",
"    --system                            Sets configuration directory to " CONFPATH " (default)",
"    --mode <NUT mode>                   Sets NUT mode (see below)",
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
"    --set-shutdowncmd <command>         Configures shutdown command",
"    --set-user <spec>                   Configures one user (see below)",
"                                        All existing users are removed; however, it may be",
"                                        specified multiple times to set multiple users",
"    --add-user <spec>                   Same as --set-user, but keeps existing users",
"                                        The two options are mutually exclusive",
"    -v",
"    --verbose                           Increase verbosity of output one level",
"                                        May be specified multiple times",
"    --scan-usb                          Scan USB devices",
"    --scan-xml-http [<timeout>]         Scan XML/HTTP devices (optional timeout in us)",
"    --scan-nut <spec>                   Scan NUT devices (see below for the specs)",
"                                        May be specified multiple times",
"    --scan-avahi [<timeout>]            Scan Avahi devices (optional timeout in us)",
"    --scan-ipmi                         Scan IPMI devices",
"",
"NUT modes: standalone, netserver, netclient, controlled, manual, none",
"Monitor is specified by the following sequence:",
"    <ups_ID> <host>[:<port>] <power_value> <user> <passwd> (\"master\"|\"slave\")",
"UPS device is specified by the following sequence:",
"    <ups_ID> <driver> <port> [<description>]",
"Notification types:",
"    ONLINE, ONBATT, LOWBATT, FSD, COMMOK, COMMBAD, SHUTDOWN, REPLBATT, NOCOMM, NOPARENT",
"Notification flags:",
"    SYSLOG, WALL, EXEC, IGNORE",
"User specification:",
"The very 1st argument is the username (but see the upsmon exception, below).",
"Next arguments use scheme of key/value pairs in form <key>=<value>",
"Known keys are:",
"    password, actions (from {SET,FSD}), instcmds (accepted multiple times)",
"Specially, for the upsmon user, the 1st argument takes form of",
"    upsmon={master|slave}",
"NUT device scan specification:",
"    <start IP> <stop IP> <port> [<us_timeout>]",
"",
};


void Usage::print(const std::string & bin) {
	std::cerr
		<< "Usage: " << bin << " [OPTIONS]" << std::endl
		<< std::endl
		<< "OPTIONS:" << std::endl;

	for (size_t i = 0; i < sizeof(s_text) / sizeof(char *); ++i) {
		std::cerr << s_text[i] << std::endl;
	}
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

	/** Arguments of the last option processed (\c NULL means bin. args) */
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
		Arguments * args = NULL != m_last ? m_last : &m_args;

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
	 *  \retval true  iff the option was specified on the command line
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
	 *  \retval true  iff the option was specified on the command line
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
	 *  \retval true  iff the option was specified on the command line
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
	 *  \retval true  iff the option was specified on the command line
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
	 *  \retval true  iff the option was specified on the command line
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
	 *  \retval true  iff the option was specified on the command line
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


Options::Options(char * const argv[], int argc): m_last(NULL) {
	for (int i = 1; i < argc; ++i) {
		const std::string arg(argv[i]);

		// Empty string is the current option argument, too
		// '-' alone is also an option argument // (like stdout placeholder etc)
		if (arg.empty() || '-' != arg[0] || 1 == arg.size())
			addArg(arg);

		// Single-dashed option
		else if ('-' != arg[1])
			add(singleDash, arg.substr(1));

		// "--" alone is valid as it means that what follows
		// belongs to the binary ("empty" option arguments)
		else if (2 == arg.size())
			m_last = NULL;

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

		static options_t createOptions(nutscan_device_t * dev);

		/** Constructor */
		Device(nutscan_device_t * dev);

	};  // end of class Device

	/** Device list */
	typedef std::list<Device> devices_t;

	private:

	/** NUT scanner initialisation/finalisation */
	struct InitFinal {
		/** Initialisation */
		InitFinal() { nutscan_init(); }

		/** Finalisation */
		~InitFinal() { nutscan_free(); }

	};  // end of struct InitFinal

	/** Initialiser / finaliser */
	static InitFinal s_init_final;

	/**
	 *  \brief  Transform nut-scan provided devices into list of device info
	 *
	 *  The nut-scan provided device list is destroyed.
	 *
	 *  \param  dev_list  nut-scan provided device list
	 *
	 *  \return Dvice info list
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
	 *  TODO
	 */
#if (0)
	devices_t devicesSNMP(
		const std::string & start_ip,
		const std::string & stop_ip,
		long                us_timeout,
		SNMPAttributes &    attrs);
#endif

	/**
	 *  \brief  Scan for USB devices
	 *
	 *  \return Device list
	 */
	inline static devices_t devicesUSB() {
		nutscan_device_t * dev = nutscan_scan_usb();

		return dev2list(dev);
	}

	/**
	 *  \brief  Scan for XML/HTTP devices
	 *
	 *  \param  us_timeout  Scan timeout
	 *
	 *  \return Device list
	 */
	inline static devices_t devicesXMLHTTP(long us_timeout) {
		nutscan_device_t * dev = nutscan_scan_xml_http(us_timeout);

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
		long                us_timeout)
	{
		nutscan_device_t * dev = nutscan_scan_nut(
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
	inline static devices_t devicesAvahi(long us_timeout) {
		nutscan_device_t * dev = nutscan_scan_avahi(us_timeout);

		return dev2list(dev);
	}

	/**
	 *  \brief  Scan for IPMI devices
	 *
	 *  \return Device list
	 */
	inline static devices_t devicesIPMI() {
		nutscan_device_t * dev = nutscan_scan_ipmi();

		return dev2list(dev);
	}

};  // end of class NutScanner


NutScanner::InitFinal NutScanner::s_init_final;


NutScanner::Device::options_t NutScanner::Device::createOptions(nutscan_device_t * dev) {
	assert(NULL != dev);

	options_t options;

	// Create options
	nutscan_options_t * opt = &dev->opt;

	for (; NULL != opt; opt = opt->next)
		options.insert(options_t::value_type(opt->option, opt->value));

	return options;
}


NutScanner::Device::Device(nutscan_device_t * dev):
	type(nutscan_device_type_string(dev->type)),
	driver(dev->driver),
	port(dev->port),
	options(createOptions(dev))
{}


NutScanner::devices_t NutScanner::dev2list(nutscan_device_t * dev_list) {
	devices_t list;

	nutscan_device_t * dev = dev_list;

	for (; dev != NULL; dev = dev->next)
		list.push_back(Device(dev));

	nutscan_free_device(dev);

	return list;
}


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
		std::string id;      /**< Device ID          */
		std::string driver;  /**< Device driver      */
		std::string port;    /**< Device port        */
		std::string desc;    /**< Device description */
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

	/** -- local argument */
	std::string local;

	/** --system */
	bool system;

	/** --mode argument */
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

	/** Shutdown command */
	std::string shutdown_cmd;

	/** Users specifications */
	UserSpecs users;

	/** Set user options count */
	size_t set_user_cnt;

	/** Add user options count */
	size_t add_user_cnt;

	/** Verbosity level */
	unsigned int verbose;

	/** Scan NUT devices */
	size_t scan_nut_cnt;

	/** Scan USB devices */
	bool scan_usb;

	/** Scan xml_http_devices */
	bool scan_xml_http;

	/** Scan Avahi devices */
	bool scan_avahi;

	/** Scan IPMI devices */
	bool scan_ipmi;

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
	void reportInvalid() const throw(std::logic_error);

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
		size_t        which = 0) const throw(std::range_error);

	private:

	/**
	 *  \brief  Check --mode argument validity
	 *
	 *  \param  mode  Mode argument
	 *
	 *  \retval  true  iff the mode is set correctly
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
	scan_ipmi(false)
{
	static const std::string sDash("-");
	static const std::string dDash("--");

	// Specificate single-dashed options
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

	// Specificate double-dashed options
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
		else if ("mode" == *opt) {
			Arguments args;

			if (!mode.empty())
				m_errors.push_back("--mode option specified more than once");

			else if (NutConfOptions::SETTER != optMode("mode", args))
				m_errors.push_back("--mode option requires an argument");

			else if (args.size() > 1)
				m_errors.push_back("Only one argument allowed for the --mode option");

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

			else if (args.size() > 4) {
				m_errors.push_back("--" + *opt + " option takes at most 4 arguments");
				m_errors.push_back("    (perhaps you need to quote description?)");
			}

			else {
				DeviceSpec dev;

				Arguments::const_iterator arg = args.begin();

				assert(args.size() >= 3);

				dev.id     = *arg++;
				dev.driver = *arg++;
				dev.port   = *arg++;

				if (arg != args.end())
					dev.desc = *arg;

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
		else if ("scan-usb" == *opt) {
			if (scan_usb)
				m_errors.push_back("--scan-usb option specified more than once");
			else
				scan_usb = true;
		}
		else if ("scan-nut" == *opt) {
			Arguments args;

			getDouble(*opt, args, scan_nut_cnt);

			if (args.size() < 3)
				m_errors.push_back("--scan-nut option requires at least 3 arguments");

			else if (args.size() > 4)
				m_errors.push_back("--scan-nut option requires at most 4 arguments");

			++scan_nut_cnt;
		}
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
		else if ("scan-ipmi" == *opt) {
			if (scan_ipmi)
				m_errors.push_back("--scan-ipmi option specified more than once");
			else
				scan_ipmi = true;
		}

		// Unknown option
		else {
			m_unknown.push_back(dDash + *opt);
		}
	}

	// Options are valid iff we know all of them
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
		*user = NULL;
	}
}


void NutConfOptions::reportInvalid() const throw(std::logic_error) {
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
 *  (i.e. not having to warry about calling \c delete by hand).
 */
template <typename T>
class autodelete_ptr {
	private:

	T * m_impl;

	/** Cleanup */
	inline void cleanup() {
		if (NULL != m_impl)
			delete m_impl;

		m_impl = NULL;
	}

	public:

	/** Constructor (unset) */
	autodelete_ptr(): m_impl(NULL) {}

	/** Constructor */
	autodelete_ptr(T * ptr): m_impl(ptr) {}

	/** Setter */
	inline autodelete_ptr & operator = (T * ptr) {
		cleanup();

		m_impl = ptr;
	}

	/** Pointer accessor */
	inline operator T * () const { return m_impl; }

	/** Pointer accessor */
	inline T * operator -> () const { return m_impl; }

	/** Pointer overtake (invalidates the object) */
	inline T * give() {
		T * ptr = m_impl;

		m_impl = NULL;

		return ptr;
	}

	/** Destructor */
	~autodelete_ptr() {
		cleanup();
	}

	private:

	/** Copying is forbidden */
	autodelete_ptr(const autodelete_ptr & orig) {}

	/** Assignment is forbidden */
	autodelete_ptr & operator = (const autodelete_ptr & orig) {}

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
	if (NULL == user) {
		user = new UserSpec;

		user->name = name;
	}

	assert(NULL != user);

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
	std::string & mode,
	size_t        which) const throw(std::range_error)
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
	mode      = monitors[base_idx + 5];
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
bool source(nut::Serialisable * config, const std::string & file_name) {
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
void store(nut::Serialisable * config, const std::string & file_name) {
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
 *  \retval true  iff nut.conf exists and MODE != none
 *  \retval false otherwise
 */
bool isConfigured(const std::string & etc) {
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
nut::UpsmonConfiguration::Monitor monitor(
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
	monitor.isMaster   = "master" == mode;

	return monitor;
}


/**
 *  \brief  Set monitors in upsmon.conf
 *
 *  \param  monitors  Monitor list
 *  \param  etc       Configuration directory
 *  \param  keep_ex   Keep existing entries (discard by default)
 */
void setMonitors(
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
nut::UpsdConfiguration::Listen listenAddr(
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
void setListenAddrs(
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
void setDevices(
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

		ups_conf.setDriver(id, (*dev).driver);
		ups_conf.setPort(id, (*dev).port);

		if (!(*dev).desc.empty())
			ups_conf.setDescription(id, (*dev).desc);
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
void setNotifyFlags(
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

		nut::Settable<unsigned short> & sum =
			upsmon_conf.notifyFlags[type];

		// Clear current flags (unless we want to keep them)
		if (specs->second.first || !sum.set())
			sum = nut::UpsmonConfiguration::NOTIFY_IGNORE;

		// Assemble flags
		std::list<std::string>::const_iterator spec = specs->second.second.begin();

		for (; spec != specs->second.second.end(); ++spec) {
			nut::UpsmonConfiguration::NotifyFlag flag =
				nut::UpsmonConfiguration::NotifyFlagFromString(*spec);

			sum |= (unsigned short)flag;
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
void setNotifyMsgs(
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
 *  \brief  Set shutdown command in upsmon.conf
 *
 *  \param  cmd  Shutdown command
 *  \param  etc  Configuration directory
 */
void setShutdownCmd(const std::string & cmd, const std::string & etc)
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
 *  \brief  Set users in upsd.users
 *
 *  \param  users    User list
 *  \param  etc      Configuration directory
 *  \param  keep_ex  Keep existing entries (discard by default)
 */
void setUsers(
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

			if ("master" == upsmon_user->mode)
				mode = nut::UpsdUsersConfiguration::UPSMON_MASTER;

			else if ("slave" == upsmon_user->mode)
				mode = nut::UpsdUsersConfiguration::UPSMON_SLAVE;

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


/**
 *  \brief  Print devices info
 *
 *  \param  devices  Device list
 *  \param  verbose  Verbosity level
 */
void printDevicesInfo(const NutScanner::devices_t & devices, unsigned int verbose = 0) {
	NutScanner::devices_t::const_iterator dev_iter = devices.begin();

	unsigned int dev_no = 1;

	for (; dev_iter != devices.end(); ++dev_iter, ++dev_no) {
		const NutScanner::Device & dev = *dev_iter;

		// Print just plain list
		if (verbose == 0)
			std::cout
			<< dev.type << ' '
			<< dev.driver << ' '
			<< dev.port << std::endl;

		// Print full info
		else {
			std::cout
			<< "[device_type_" << dev.type
			<< "_no_" << dev_no << ']' << std::endl
			<< "\tdriver = " << dev.driver << std::endl
			<< "\tport = " << dev.port   << std::endl;

			NutScanner::Device::options_t::const_iterator
				opt = dev.options.begin();

			for (; opt != dev.options.end(); ++opt)
				std::cout
				<< '\t'  << opt->first
				<< " = " << opt->second
				<< std::endl;

			std::cout << std::endl;
		}
	}
}


/**
 *  \brief  Scan for USB devices
 *
 *  \param  options  Options
 */
void scanUSBdevices(const NutConfOptions & options) {
	NutScanner::devices_t devices = NutScanner::devicesUSB();

	printDevicesInfo(devices, options.verbose);
}


/**
 *  \brief  Scan for NUT devices
 *
 *  \param  options  Options
 */
void scanNUTdevices(const NutConfOptions & options) {
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
		long us_timeout = 1000000;

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
void scanXMLHTTPdevices(const NutConfOptions & options) {
	NutConfOptions::Arguments args;

	bool ok = options.getDouble("scan-xml-http", args);

	// Sanity checks
	assert(ok);

	// TBD: where should we get the default?
	long us_timeout = 1000000;

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
void scanAvahiDevices(const NutConfOptions & options) {
	NutConfOptions::Arguments args;

	bool ok = options.getDouble("scan-avahi", args);

	// Sanity checks
	assert(ok);

	// TBD: where should we get the default?
	long us_timeout = 1000000;

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
void scanIPMIdevices(const NutConfOptions & options) {
	NutScanner::devices_t devices = NutScanner::devicesIPMI();

	printDevicesInfo(devices, options.verbose);
}


/**
 *  \brief  Main routine (exceptions unsafe)
 *
 *  \param  argc  Argument count
 *  \param  argv  Arguments
 *
 *  \return 0 always (exits on error)
 */
int mainx(int argc, char * const argv[]) {
	// Get options
	NutConfOptions options(argv, argc);

	// Usage
	if (options.exists("help")) {
		Usage::print(argv[0]);

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

	// Shutdown command was set
	if (!options.shutdown_cmd.empty()) {
		setShutdownCmd(options.shutdown_cmd, etc);
	}

	// Users were set
	if (!options.users.empty()) {
		setUsers(options.users, etc, options.add_user_cnt > 0);
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
	if (options.scan_ipmi) {
		scanIPMIdevices(options);
	}

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
			<< "Please issue a bugreport to nut-upsdev@lists.alioth.debian.org"
			<< std::endl;
	}

	::exit(128);
}
