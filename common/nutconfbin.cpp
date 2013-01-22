#include "nutconf.h"
#include "nutstream.hpp"

#include <iostream>
#include <list>
#include <map>


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
};


void Usage::print(const std::string & bin) {
	std::cerr
		<< "Usage: " << bin << " [OPTIONS]" << std::endl
		<< std::endl
		<< "OPTIONS:" << std::endl;

	for (size_t i = 0; i < sizeof(s_text) / sizeof(char *); ++i) {
		std::cerr << s_text[i] << std::endl;
	}

	std::cerr << std::endl;
}


/** Command line options */
class Options {
	public:

	/** Options list */
	typedef std::list<std::string> List;

	protected:

	/** Option type */
	typedef enum {
		binaryArgument,  /**< Argument of the binary itself */
		singleDash,      /**< Single-dash prefixed option   */
		doubleDash,      /**< Double-dash prefixed option   */
	} type_t;

	/** Option arguments list */
	typedef std::list<std::string> Arguments;

	/** Options map */
	typedef std::map<std::string, Arguments> Map;

	private:

	/** Binary arguments */
	Arguments m_args;

	/** Single-dashed options */
	Map m_single;

	/** Double-dashed options */
	Map m_double;

	/**
	 *  \brief  Add option
	 *
	 *  If the option already exists, the arguments are added
	 *  to the existing (if any).
	 *
	 *  \param  type  Option type
	 *  \param  opt   Option (ignored if type is \c binaryArgument)
	 *  \param  args  Option arguments
	 */
	void add(type_t type, const std::string & opt, const Arguments & args);

	/**
	 *  \brief  Add option without arguments
	 *
	 *  \param  type  Option type
	 *  \param  opt   Option
	 */
	inline void add(type_t type, const std::string & opt) {
		add(type, opt, Arguments());
	}

	/**
	 *  \brief  Add option argument
	 *
	 *  \param  type  Option type
	 *  \param  opt   Option
	 *  \param  arg   Argument
	 */
	inline void addArg(type_t type, const std::string & opt, const std::string & arg) {
		add(type, opt, Arguments(1, arg));
	}

	/**
	 *  \brief  Check option existence
	 *
	 *  \param  map  Option map
	 *  \param  opt  Option
	 *
	 *  \retval true  iff the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool exists(const Map & map, const std::string & opt) const {
		return map.end() != map.find(opt);
	}

	/**
	 *  \brief  Get option arguments
	 *
	 *  \param[in]   map   Option map
	 *  \param[in]   opt   Option
	 *  \param[out]  args  Option arguments
	 *
	 *  \retval true  iff the option was specified on the command line
	 *  \retval false otherwise
	 */
	bool get(const Map & map, const std::string & opt, Arguments & args) const;

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
	 *  \brief  Check single-dashed option existence
	 *
	 *  \param  opt  Option
	 *
	 *  \retval true  iff the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool existsSingle(const std::string & opt) const {
		return exists(m_single, opt);
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
		return exists(m_double, opt);
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
	 *  \param[in]   opt   Option
	 *  \param[out]  args  Option arguments
	 *
	 *  \retval true  iff the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool getSingle(const std::string & opt, Arguments & args) const {
		return get(m_single, opt, args);
	}

	/**
	 *  \brief  Get double-dashed option arguments
	 *
	 *  \param[in]   opt   Option
	 *  \param[out]  args  Option arguments
	 *
	 *  \retval true  iff the option was specified on the command line
	 *  \retval false otherwise
	 */
	inline bool getDouble(const std::string & opt, Arguments & args) const {
		return get(m_single, opt, args);
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


void Options::add(Options::type_t type, const std::string & opt, const Arguments & args) {
	Arguments * arguments = &m_args;

	switch (type) {
		case binaryArgument:

			break;;

		case singleDash:
			arguments = &m_single[opt];

			break;

		case doubleDash:
			arguments = &m_double[opt];

			break;
	}

	Arguments::const_iterator arg = args.begin();

	for (; arg != args.end(); ++arg) {
		arguments->push_back(*arg);
	}
}


bool Options::get(const Options::Map & map, const std::string & opt, Arguments & args) const {
	Map::const_iterator entry = map.find(opt);

	if (map.end() == entry)
		return false;

	args = entry->second;

	return true;
}


void Options::strings(const Map & map, List & list) const {
	for (Map::const_iterator opt = map.begin(); opt != map.end(); ++opt)
		list.push_back(opt->first);
}


Options::Options(char * const argv[], int argc) {
	type_t      current_type = binaryArgument;
	std::string current_opt;

	for (int i = 1; i < argc; ++i) {
		const std::string arg(argv[i]);

		// Empty string is the current option argument, too
		// '-' alone is also an option argument // (like stdout placeholder etc)
		if (arg.empty() || '-' != arg[0] || 1 == arg.size()) {
			addArg(current_type, current_opt, arg);

			continue;
		}

		// Single-dashed option
		if ('-' != arg[1]) {
			current_type = singleDash;
			current_opt  = arg.substr(1);

			add(current_type, current_opt);

			continue;
		}

		// "--" alone is valid as it means that what follows
		// belongs to the binary ("empty" option arguments)
		if (2 == arg.size()) {
			current_type = binaryArgument;

			continue;
		}

		// Double-dashed option
		if ('-' != arg[2]) {
			current_type = doubleDash;
			current_opt  = arg.substr(2);

			add(current_type, current_opt);

			continue;
		}

		// "---" prefix means an option argument
		addArg(current_type, current_opt, arg);
	}
}


Options::List Options::strings() const {
	List list = stringsSingle();

	strings(m_double, list);

	return list;
}


/** nutconf tool specific options */
class NutConfOptions: public Options {
	private:

	/** Unknown options */
	List m_unknown;

	public:

	/** --autoconfigure */
	bool autoconfigure;

	NutConfOptions(char * const argv[], int argc);

	inline bool valid() const {
		// We don't accept any direct arguments
		if (!get().empty())
			return false;

		return m_unknown.empty();
	}

	void reportInvalid() const;

};  // end of class NutConfOptions


NutConfOptions::NutConfOptions(char * const argv[], int argc):
	Options(argv, argc),
	autoconfigure(false)
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
			autoconfigure = true;
		}

		// TODO

		// Unknown option
		else {
			m_unknown.push_back(dDash + *opt);
		}
	}
}


void NutConfOptions::reportInvalid() const {
	List::const_iterator unknown_opt = m_unknown.begin();

	for (; unknown_opt != m_unknown.end(); ++unknown_opt) {
		std::cerr << "Unknown option: " << *unknown_opt << std::endl;
	}

	// No direct arguments expected
	const Arguments & args = get();

	Arguments::const_iterator arg = args.begin();

	for (; arg != args.end(); ++arg) {
		std::cerr << "Unexpected argument: " << *arg << std::endl;
	}
}


int main(int argc, char * const argv[]) {
	// Get options
	NutConfOptions options(argv, argc);

	// Usage
	if (options.exists("help")) {
		Usage::print(argv[0]);

		::exit(0);
	}

	// Check that command-line options validity
	if (!options.valid()) {
		options.reportInvalid();

		Usage::print(argv[0]);

		::exit(1);
	}

	return 0;
}
