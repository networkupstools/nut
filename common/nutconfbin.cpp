#include "config.h"
#include "nutconf.h"
#include "nutstream.hpp"

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
"    --help                    Display this help and exit",
"    --autoconfigure           Perform autoconfiguration",
"    --is-configured           Checks whether NUT is configured",
"    --local <directory>       Sets configuration directory",
"    --system                  Sets configuration directory to " CONFPATH,
"    --mode <NUT mode>         Sets NUT mode (see below)",
"    --set-monitor <spec>      Configures one monitor (see below)",
"                              May be used multiple times",
"",
"NUT modes: standalone, netserver, netclient, controlled, manual, none",
"Monitor is specified by the following sequence:",
"    <ups_ID> <host>[:<port>] <power_value> <user> <passwd> (\"master\"|\"slave\")",
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

	private:

	/** Option type */
	typedef enum {
		singleDash,  /**< Single-dash prefixed option */
		doubleDash,  /**< Double-dash prefixed option */
	} type_t;

	protected:

	/** Option arguments list */
	typedef std::list<std::string> Arguments;

	/** Options map */
	typedef std::multimap<std::string, Arguments> Map;

	private:

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
}


Options::List Options::strings() const {
	List list = stringsSingle();

	strings(m_double, list);

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

	/** --set-monitor arguments (all the monitors) */
	std::vector<std::string> monitors;

	/** Monitors count */
	size_t monitor_cnt;

	/** Constructor */
	NutConfOptions(char * const argv[], int argc);

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
	monitor_cnt(0)
{
	static const std::string sDash("-");
	static const std::string dDash("--");

	// No single-dashed options used
	List list = stringsSingle();

	for (List::const_iterator opt = list.begin(); opt != list.end(); ++opt) {
		m_unknown.push_back(sDash + *opt);
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
		else if ("set-monitor" == *opt) {
			Arguments args;

			if (NutConfOptions::SETTER != optMode("set-monitor", args, monitor_cnt))
				m_errors.push_back("--set-monitor option requires arguments");

			else if (args.size() != 6)
				m_errors.push_back("--set-monitor option requires exactly 6 arguments");

			else
				for (Arguments::const_iterator arg = args.begin(); arg != args.end(); ++arg)
					monitors.push_back(*arg);

			++monitor_cnt;
		}

		// Unknown option
		else {
			m_unknown.push_back(dDash + *opt);
		}
	}

	// Options are valid iff we know all of them
	// and there are no direct binary arguments
	valid = m_unknown.empty() && m_errors.empty() && get().empty();
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


void NutConfOptions::getMonitor(
	std::string & ups,
	std::string & host_port,
	std::string & pwr_val,
	std::string & user,
	std::string & passwd,
	std::string & mode,
	size_t        which) const throw(std::range_error)
{
	if (which >= monitor_cnt)
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

	return nut::NutConfiguration::MODE_NONE != nut_conf.mode;
}


/**
 *  \brief  Transform monitor specification from cmd. line to monitor configuration
 *
 *  \param  i        Monitor index
 *  \param  options  nutconf options
 *
 *  \return Monitor configuration
 */
nut::UpsmonConfiguration::Monitor getMonitor(
	size_t                   i,
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
 */
void setMonitors(
	const std::list<nut::UpsmonConfiguration::Monitor> & monitors,
	const std::string & etc)
{
	nut::NutFile upsmon_conf_file(etc + "/upsmon.conf");

	nut::UpsmonConfiguration upsmon_conf;

	// Source previous configuration (if any)
	if (upsmon_conf_file.exists()) {
		upsmon_conf_file.openx();

		bool parsed_ok = upsmon_conf.parseFrom(upsmon_conf_file);

		upsmon_conf_file.closex();

		if (!parsed_ok) {
			std::cerr
				<< "Error: Failed to parse existing "
				<< upsmon_conf_file.name() << std::endl;

			::exit(1);
		}
	}

	// Add monitors to the current ones (if any)
	std::list<nut::UpsmonConfiguration::Monitor>::const_iterator
		monitor = monitors.begin();

	for (; monitor != monitors.end(); ++monitor)
		upsmon_conf.monitors.push_back(*monitor);

	// Store configuration
	upsmon_conf_file.openx(nut::NutFile::WRITE_ONLY);

	bool written_ok = upsmon_conf.writeTo(upsmon_conf_file);

	upsmon_conf_file.closex();

	if (!written_ok) {
		std::cerr
			<< "Error: Failed to write "
			<< upsmon_conf_file.name() << std::endl;

		::exit(1);
	}
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
	std::string etc;

	if (options.system) {
		etc = CONFPATH;
	}
	else if (options.local.empty()) {
		std::cerr << "Error: Configuration directory wasn't specified" << std::endl;

		Usage::print(argv[0]);

		::exit(1);
	}
	else {
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

	// Monitors were set by --set-monitor
	if (options.monitor_cnt) {
		std::list<nut::UpsmonConfiguration::Monitor> monitors;

		for (size_t i = 0; i < options.monitor_cnt; ++i) {
			monitors.push_back(getMonitor(i, options));
		}

		setMonitors(monitors, etc);
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
		std::cerr << "Error: " << e.what() << std::endl;
	}

	::exit(128);
}
