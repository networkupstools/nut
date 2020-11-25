/* nutclient.h - definitions for nutclient C/C++ library

   Copyright (C) 2012  Emilien Kia <emilien.kia@gmail.com>

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

#ifndef NUTCLIENT_HPP_SEEN
#define NUTCLIENT_HPP_SEEN

/* Begin of C++ nutclient library declaration */
#ifdef __cplusplus

#include <string>
#include <vector>
#include <map>
#include <set>
#include <exception>

namespace nut
{

namespace internal
{
class Socket;
} /* namespace internal */


class Client;
class TcpClient;
class Device;
class Variable;
class Command;

/**
 * Basic nut exception.
 */
class NutException : public std::exception
{
public:
	NutException(const std::string& msg):_msg(msg){}
	virtual ~NutException() {}
	virtual const char * what() const noexcept {return this->_msg.c_str();}
	virtual std::string str() const noexcept {return this->_msg;}
private:
	std::string _msg;
};

/**
 * System error.
 */
class SystemException : public NutException
{
public:
	SystemException();
	virtual ~SystemException() {}
private:
	static std::string err();
};


/**
 * IO oriented nut exception.
 */
class IOException : public NutException
{
public:
	IOException(const std::string& msg):NutException(msg){}
	virtual ~IOException() {}
};

/**
 * IO oriented nut exception specialized for unknown host
 */
class UnknownHostException : public IOException
{
public:
	UnknownHostException():IOException("Unknown host"){}
	virtual ~UnknownHostException() {}
};

/**
 * IO oriented nut exception when client is not connected
 */
class NotConnectedException : public IOException
{
public:
	NotConnectedException():IOException("Not connected"){}
	virtual ~NotConnectedException() {}
};

/**
 * IO oriented nut exception when there is no response.
 */
class TimeoutException : public IOException
{
public:
	TimeoutException():IOException("Timeout"){}
	virtual ~TimeoutException() {}
};

/**
 * Cookie given when performing async action, used to redeem result at a later date.
 */
typedef std::string TrackingID;

/**
 * Result of an async action.
 */
typedef enum
{
	UNKNOWN,
	PENDING,
	SUCCESS,
	INVALID_ARGUMENT,
	FAILURE,
} TrackingResult;

typedef std::string Feature;

/**
 * A nut client is the starting point to dialog to NUTD.
 * It can connect to an NUTD then retrieve its device list.
 * Use a specific client class to connect to a NUTD.
 */
class Client
{
	friend class Device;
	friend class Variable;
	friend class Command;
public:
	virtual ~Client();

	/**
	 * Intend to authenticate to a NUTD server.
	 * Set the username and password associated to the connection.
	 * \param user User name.
	 * \param passwd Password.
	 * \todo Is his method is global to all connection protocol or is it specific to TCP ?
	 * \note Actually, authentication fails only if already set, not if bad values are sent.
	 */
	virtual void authenticate(const std::string& user, const std::string& passwd) = 0;

	/**
	 * Disconnect from the NUTD server.
	 * \todo Is his method is global to all connection protocol or is it specific to TCP ?
	 */
	virtual void logout() = 0;

	/**
	 * Device manipulations.
	 * \see nut::Device
	 * \{
	 */
	/**
	 * Retrieve a device from its name.
	 * If the device does not exist, a bad (not ok) device is returned.
	 * \param name Name of the device.
	 * \return The device.
	 */
	virtual Device getDevice(const std::string& name);
	/**
	 * Retrieve the list of all devices supported by UPSD server.
	 * \return The set of supported devices.
	 */
	virtual std::set<Device> getDevices();
	/**
	 * Test if a device is supported by the NUTD server.
	 * \param dev Device name.
	 * \return true if supported, false otherwise.
	 */
	virtual bool hasDevice(const std::string& dev);
	/**
	 * Retrieve names of devices supported by NUTD server.
	 * \return The set of names of supported devices.
	 */
	virtual std::set<std::string> getDeviceNames() = 0;
	/**
	 * Retrieve the description of a device.
	 * \param name Device name.
	 * \return Device description.
	 */
	virtual std::string getDeviceDescription(const std::string& name) = 0;
	/** \} */

	/**
	 * Variable manipulations.
	 * \see nut::Variable
	 * \{
	 */
	/**
	 * Retrieve names of all variables supported by a device.
	 * \param dev Device name
	 * \return Variable names
	 */
	virtual std::set<std::string> getDeviceVariableNames(const std::string& dev) = 0;
	/**
	 * Retrieve names of read/write variables supported by a device.
	 * \param dev Device name
	 * \return RW variable names
	 */
	virtual std::set<std::string> getDeviceRWVariableNames(const std::string& dev) = 0;
	/**
	 * Test if a variable is supported by a device.
	 * \param dev Device name
	 * \param name Variable name
	 * \return true if the variable is supported.
	 */
	virtual bool hasDeviceVariable(const std::string& dev, const std::string& name);
	/**
	 * Retrieve the description of a variable.
	 * \param dev Device name
	 * \param name Variable name
	 * \return Variable description if provided.
	 */
	virtual std::string getDeviceVariableDescription(const std::string& dev, const std::string& name) = 0;
	/**
	 * Retrieve values of a variable.
	 * \param dev Device name
	 * \param name Variable name
	 * \return Variable values (usually one) if available.
	 */
	virtual std::vector<std::string> getDeviceVariableValue(const std::string& dev, const std::string& name) = 0;
	/**
	 * Retrieve values of all variables of a device.
	 * \param dev Device name
	 * \return Variable values indexed by variable names.
	 */
	virtual std::map<std::string,std::vector<std::string> > getDeviceVariableValues(const std::string& dev);
	/**
	 * Retrieve values of all variables of a set of devices.
	 * \param devs Device names
	 * \return Variable values indexed by variable names, indexed by device names.
	 */
	virtual std::map<std::string,std::map<std::string,std::vector<std::string> > > getDevicesVariableValues(const std::set<std::string>& devs);
	/**
	 * Intend to set the value of a variable.
	 * \param dev Device name
	 * \param name Variable name
	 * \param value Variable value
	 */
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value) = 0;
	/**
	 * Intend to set the value of a variable.
	 * \param dev Device name
	 * \param name Variable name
	 * \param values Vector of variable values
	 */
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::vector<std::string>& values) = 0;
	/** \} */

	/**
	 * Instant command manipulations.
	 * \see nut::Command
	 * \{
	 */
	/**
	 * Retrieve names of all commands supported by a device.
	 * \param dev Device name
	 * \return Command names
	 */
	virtual std::set<std::string> getDeviceCommandNames(const std::string& dev) = 0;
	/**
	 * Test if a command is supported by a device.
	 * \param dev Device name
	 * \param name Command name
	 * \return true if the command is supported.
	 */
	virtual bool hasDeviceCommand(const std::string& dev, const std::string& name);
	/**
	 * Retrieve the description of a command.
	 * \param dev Device name
	 * \param name Command name
	 * \return Command description if provided.
	 */
	virtual std::string getDeviceCommandDescription(const std::string& dev, const std::string& name) = 0;
	/**
	 * Intend to execute a command.
	 * \param dev Device name
	 * \param name Command name
	 * \param param Additional command parameter
	 */
	virtual TrackingID executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param="") = 0;
	/** \} */

	/**
	 * Device specific commands.
	 * \{
	 */
	/**
	 * Log the current user (if authenticated) for a device.
	 * \param dev Device name.
	 */
	virtual void deviceLogin(const std::string& dev) = 0;
	/**
	 * Retrieve the number of user longged in the specified device.
	 * \param dev Device name.
	 * \return Number of logged-in users.
	 */
	virtual int deviceGetNumLogins(const std::string& dev) = 0;
	virtual void deviceMaster(const std::string& dev) = 0;
	virtual void deviceForcedShutdown(const std::string& dev) = 0;

	/**
	 * Retrieve the result of a tracking ID.
	 * \param id Tracking ID.
	 */
	virtual TrackingResult getTrackingResult(const TrackingID& id) = 0;

	virtual bool hasFeature(const Feature& feature);
	virtual bool isFeatureEnabled(const Feature& feature) = 0;
	virtual void setFeature(const Feature& feature, bool status) = 0;

	static const Feature TRACKING;

protected:
	Client();
};

/**
 * TCP NUTD client.
 * It connect to NUTD with a TCP socket.
 */
class TcpClient : public Client
{
public:
	/**
	 * Construct a nut TcpClient object.
	 * You must call one of TcpClient::connect() after.
	 */
	TcpClient();

	/**
	 * Construct a nut TcpClient object then connect it to the specified server.
	 * \param host Server host name.
	 * \param port Server port.
	 */
	TcpClient(const std::string& host, int port = 3493);
	~TcpClient();

	/**
	 * Connect it to the specified server.
	 * \param host Server host name.
	 * \param port Server port.
	 */
	void connect(const std::string& host, int port = 3493);

	/**
	 * Connect to the server.
	 * Host name and ports must have already set (usefull for reconnection).
	 */
	void connect();

	/**
	 * Test if the connection is active.
	 * \return tru if the connection is active.
	 */
	bool isConnected()const;
	/**
	 * Force the deconnection.
	 */
	void disconnect();

	/**
	 * Set the timeout in seconds.
	 * \param timeout Timeout n seconds, negative to block operations.
	 */
	void setTimeout(long timeout);

	/**
	 * Retrieve the timeout.
	 * \returns Current timeout in seconds.
	 */
	long getTimeout()const;

	/**
	 * Retriueve the host name of the server the client is connected to.
	 * \return Server host name
	 */
	std::string getHost()const;
	/**
	 * Retriueve the port of host of the server the client is connected to.
	 * \return Server port
	 */
	int getPort()const;

	virtual void authenticate(const std::string& user, const std::string& passwd);
	virtual void logout();

	virtual Device getDevice(const std::string& name);
	virtual std::set<std::string> getDeviceNames();
	virtual std::string getDeviceDescription(const std::string& name);

	virtual std::set<std::string> getDeviceVariableNames(const std::string& dev);
	virtual std::set<std::string> getDeviceRWVariableNames(const std::string& dev);
	virtual std::string getDeviceVariableDescription(const std::string& dev, const std::string& name);
	virtual std::vector<std::string> getDeviceVariableValue(const std::string& dev, const std::string& name);
	virtual std::map<std::string,std::vector<std::string> > getDeviceVariableValues(const std::string& dev);
	virtual std::map<std::string,std::map<std::string,std::vector<std::string> > > getDevicesVariableValues(const std::set<std::string>& devs);
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value);
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::vector<std::string>& values);

	virtual std::set<std::string> getDeviceCommandNames(const std::string& dev);
	virtual std::string getDeviceCommandDescription(const std::string& dev, const std::string& name);
	virtual TrackingID executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param="");

 	virtual void deviceLogin(const std::string& dev);
	virtual void deviceMaster(const std::string& dev);
	virtual void deviceForcedShutdown(const std::string& dev);
	virtual int deviceGetNumLogins(const std::string& dev);

	virtual TrackingResult getTrackingResult(const TrackingID& id);

	virtual bool isFeatureEnabled(const Feature& feature);
	virtual void setFeature(const Feature& feature, bool status);

protected:
	std::string sendQuery(const std::string& req);
	void sendAsyncQueries(const std::vector<std::string>& req);
	static void detectError(const std::string& req);
	TrackingID sendTrackingQuery(const std::string& req);

	std::vector<std::string> get(const std::string& subcmd, const std::string& params = "");

	std::vector<std::vector<std::string> > list(const std::string& subcmd, const std::string& params = "");

	std::vector<std::vector<std::string> > parseList(const std::string& req);

	static std::vector<std::string> explode(const std::string& str, size_t begin=0);
	static std::string escape(const std::string& str);

private:
	std::string _host;
	int _port;
	long _timeout;
	internal::Socket* _socket;
};


/**
 * Device attached to a client.
 * Device is a lightweight class which can be copied easily.
 */
class Device
{
	friend class Client;
	friend class TcpClient;
#ifdef _NUTCLIENTTEST_BUILD
	friend class NutClientTest;
#endif
public:
	~Device();
	Device(const Device& dev);
	Device& operator=(const Device& dev);

	/**
	 * Retrieve the name of the device.
	 * The name is the unique id under which NUTD known the device.
	 */
	std::string getName()const;
	/**
	 * Retrieve the client to which the device is attached.
	 */
	const Client* getClient()const;
	/**
	 * Retrieve the client to which the device is attached.
	 */
	Client* getClient();

	/**
	 * Test if the device is valid (has a name and is attached to a client).
	 */
	bool isOk()const;
	/**
	 * Test if the device is valid (has a name and is attached to a client).
	 * @see Device::isOk()
	 */
	operator bool()const;
	/**
	 * Test if the device is not valid (has no name or is not attached to any client).
	 * @see Device::isOk()
	 */
	bool operator!()const;
	/**
	 * Test if the two devices are sames (same name ad same client attached to).
	 */
	bool operator==(const Device& dev)const;
	/**
	 * Comparison operator.
	 */
	bool operator<(const Device& dev)const;

	/**
	 * Retrieve the description of the devce if specified.
	 */
	std::string getDescription();

	/**
	 * Intend to retrieve the value of a variable of the device.
	 * \param name Name of the variable to get.
	 * \return Value of the variable, if available.
	 */
	std::vector<std::string> getVariableValue(const std::string& name);
	/**
	 * Intend to retrieve values of all variables of the devices.
	 * \return Map of all variables values indexed by their names.
	 */
	std::map<std::string,std::vector<std::string> > getVariableValues();
	/**
	 * Retrieve all variables names supported by the device.
	 * \return Set of available variable names.
	 */
	std::set<std::string> getVariableNames();
	/**
	 * Retrieve all Read/Write variables names supported by the device.
	 * \return Set of available Read/Write variable names.
	 */
	std::set<std::string> getRWVariableNames();
	/**
	 * Intend to set the value of a variable of the device.
	 * \param name Variable name.
	 * \param value New variable value.
	 */
	void setVariable(const std::string& name, const std::string& value);
	/**
	 * Intend to set values of a variable of the device.
	 * \param name Variable name.
	 * \param values Vector of new variable values.
	 */
	void setVariable(const std::string& name, const std::vector<std::string>& values);

	/**
	 * Retrieve a Variable object representing the specified variable.
	 * \param name Variable name.
	 * \return Variable object.
	 */
	Variable getVariable(const std::string& name);
	/**
	 * Retrieve Variable objects representing all variables available for the device.
	 * \return Set of Variable objects.
	 */
	std::set<Variable> getVariables();
	/**
	 * Retrieve Variable objects representing all Read/Write variables available for the device.
	 * \return Set of Variable objects.
	 */
	std::set<Variable> getRWVariables();

	/**
	 * Retrieve names of all commands supported by the device.
	 * \return Set of available command names.
	 */
	std::set<std::string> getCommandNames();
	/**
	 * Retrieve objects for all commands supported by the device.
	 * \return Set of available Command objects.
	 */
	std::set<Command> getCommands();
	/**
	 * Retrieve an object representing a command of the device.
	 * \param name Command name.
	 * \return Command object.
	 */
	Command getCommand(const std::string& name);
	/**
	 * Intend to execute a command on the device.
	 * \param name Command name.
	 * \param param Additional command parameter
	 */
	TrackingID executeCommand(const std::string& name, const std::string& param="");

	/**
	 * Login current client's user for the device.
	 */
	void login();
	void master();
	void forcedShutdown();
	/**
	 * Retrieve the number of logged user for the device.
	 * \return Number of users.
	 */
	int getNumLogins();

protected:
	Device(Client* client, const std::string& name);

private:
	Client* _client;
	std::string _name;
};

/**
 * Variable attached to a device.
 * Variable is a lightweight class which can be copied easily.
 */
class Variable
{
	friend class Device;
	friend class TcpClient;
#ifdef _NUTCLIENTTEST_BUILD
	friend class NutClientTest;
#endif
public:
	~Variable();

	Variable(const Variable& var);
	Variable& operator=(const Variable& var);

	/**
	 * Retrieve variable name.
	 */
	std::string getName()const;
	/**
	 * Retrieve the device to which the variable is attached to.
	 */
	const Device* getDevice()const;
	/**
	 * Retrieve the device to which the variable is attached to.
	 */
	Device* getDevice();

	/**
	 * Test if the variable is valid (has a name and is attached to a device).
	 */
	bool isOk()const;
	/**
	 * Test if the variable is valid (has a name and is attached to a device).
	 * @see Variable::isOk()
	 */
	operator bool()const;
	/**
	 * Test if the variable is not valid (has no name or is not attached to any device).
	 * @see Variable::isOk()
	 */
	bool operator!()const;
	/**
	 * Test if the two variables are sames (same name ad same device attached to).
	 */
	bool operator==(const Variable& var)const;
	/**
	 * Less-than operator (based on variable name) to allow variable sorting.
	 */
	bool operator<(const Variable& var)const;

	/**
	 * Intend to retrieve variable value.
	 * \return Value of the variable.
	 */
	std::vector<std::string> getValue();
	/**
	 * Intend to retireve variable description.
	 * \return Variable description if provided.
	 */
	std::string getDescription();

	/**
	 * Intend to set a value to the variable.
	 * \param value New variable value.
	 */
	void setValue(const std::string& value);
	/**
	 * Intend to set (multiple) values to the variable.
	 * \param values Vector of new variable values.
	 */
	void setValues(const std::vector<std::string>& values);

protected:
	Variable(Device* dev, const std::string& name);

private:
	Device* _device;
	std::string _name;
};

/**
 * Command attached to a device.
 * Command is a lightweight class which can be copied easily.
 */
class Command
{
	friend class Device;
	friend class TcpClient;
#ifdef _NUTCLIENTTEST_BUILD
	friend class NutClientTest;
#endif
public:
	~Command();

	Command(const Command& cmd);
	Command& operator=(const Command& cmd);

	/**
	 * Retrieve command name.
	 */
	std::string getName()const;
	/**
	 * Retrieve the device to which the command is attached to.
	 */
	const Device* getDevice()const;
	/**
	 * Retrieve the device to which the command is attached to.
	 */
	Device* getDevice();

	/**
	 * Test if the command is valid (has a name and is attached to a device).
	 */
	bool isOk()const;
	/**
	 * Test if the command is valid (has a name and is attached to a device).
	 * @see Command::isOk()
	 */
	operator bool()const;
	/**
	 * Test if the command is not valid (has no name or is not attached to any device).
	 * @see Command::isOk()
	 */
	bool operator!()const;
	/**
	 * Test if the two commands are sames (same name ad same device attached to).
	 */
	bool operator==(const Command& var)const;

	/**
	 * Less-than operator (based on command name) to allow comand sorting.
	 */
	bool operator<(const Command& var)const;

	/**
	 * Intend to retireve command description.
	 * \return Command description if provided.
	 */
	std::string getDescription();

	/**
	 * Intend to execute the instant command on device.
	 * \param param Optional additional command parameter
	 */
	void execute(const std::string& param="");

protected:
	Command(Device* dev, const std::string& name);

private:
	Device* _device;
	std::string _name;
};

} /* namespace nut */

#endif /* __cplusplus */
/* End of C++ nutclient library declaration */




/* Begin of C nutclient library declaration */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * Array of string manipulation functions.
 * \{
 */
/** Array of string.*/
typedef char** strarr;
/**
 * Alloc an array of string.
 */
strarr strarr_alloc(size_t count);

/**
 * Free an array of string.
 */
void strarr_free(strarr arr);

/**
 * Convert C++ types into an array of string.
 */
strarr stringvector_to_strarr(const std::vector<std::string>& strset);
strarr stringset_to_strarr(const std::set<std::string>& strset);


/**
 * Nut general client types and functions.
 * \{
 */
/** Hidden structure representing a connection to NUTD. */
typedef void* NUTCLIENT_t;

/**
 * Destroy a client.
 * \param client Nut client handle.
 */
void nutclient_destroy(NUTCLIENT_t client);

/**
 * Authenticate into the server.
 * \param client Nut client handle.
 * \param login User name.
 * \param passwd Password.
 */
void nutclient_authenticate(NUTCLIENT_t client, const char* login, const char* passwd);

/**
 * Log out from server.
 * \param client Nut client handle.
 */
void nutclient_logout(NUTCLIENT_t client);

/**
 * Register current user on the device.
 * \param client Nut client handle.
 * \param dev Device name to test.
 */
void nutclient_device_login(NUTCLIENT_t client, const char* dev);

/**
 * Retrieve the number of users registered on a device.
 * \param client Nut client handle.
 * \param dev Device name to test.
 */
int nutclient_get_device_num_logins(NUTCLIENT_t client, const char* dev);

/**
 * Set current user as master user of the device.
 * \param client Nut client handle.
 * \param dev Device name to test.
 */
void nutclient_device_master(NUTCLIENT_t client, const char* dev);

/**
 * Set the FSD flag for the device.
 * \param client Nut client handle.
 * \param dev Device name to test.
 */
void nutclient_device_forced_shutdown(NUTCLIENT_t client, const char* dev);

/**
 * Retrieve the list of devices of a client.
 * \param client Nut client handle.
 * \return Array of string containing device names. Must be freed with strarr_free(strarr).
 */
strarr nutclient_get_devices(NUTCLIENT_t client);

/**
 * Test if a device is supported by the client.
 * \param client Nut client handle.
 * \param dev Device name to test.
 * \return 1 if supported, 0 otherwise.
 */
int nutclient_has_device(NUTCLIENT_t client, const char* dev);

/**
 * Intend to retrieve device description.
 * \param client Nut client handle.
 * \param dev Device name to test.
 * \return Description of device. Must be freed after use.
 */
char* nutclient_get_device_description(NUTCLIENT_t client, const char* dev);

/**
 * Intend to retrieve device variable names.
 * \param client Nut client handle.
 * \param dev Device name.
 * \return Array of string containing variable names. Must be freed with strarr_free(strarr).
 */
strarr nutclient_get_device_variables(NUTCLIENT_t client, const char* dev);

/**
 * Intend to retrieve device read/write variable names.
 * \param client Nut client handle.
 * \param dev Device name.
 * \return Array of string containing read/write variable names. Must be freed with strarr_free(strarr).
 */
strarr nutclient_get_device_rw_variables(NUTCLIENT_t client, const char* dev);

/**
 * Test if a variable is supported by the device and the client.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param var Variable name.
 * \return 1 if supported, 0 otherwise.
 */
int nutclient_has_device_variable(NUTCLIENT_t client, const char* dev, const char* var);

/**
 * Intend to retrieve device variable description.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param var Variable name.
 * \return Description of device variable. Must be freed after use.
 */
char* nutclient_get_device_variable_description(NUTCLIENT_t client, const char* dev, const char* var);

/**
 * Intend to retrieve device variable values.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param var Variable name.
 * \return Array of string containing variable values. Must be freed with strarr_free(strarr).
 */
strarr nutclient_get_device_variable_values(NUTCLIENT_t client, const char* dev, const char* var);

/**
 * Intend to set device variable value.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param var Variable name.
 * \param value Value to set.
 */
void nutclient_set_device_variable_value(NUTCLIENT_t client, const char* dev, const char* var, const char* value);

/**
 * Intend to set device variable  multiple values.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param var Variable name.
 * \param values Values to set. The cller is responsible to free it after call.
 */
void nutclient_set_device_variable_values(NUTCLIENT_t client, const char* dev, const char* var, const strarr values);

/**
 * Intend to retrieve device command names.
 * \param client Nut client handle.
 * \param dev Device name.
 * \return Array of string containing command names. Must be freed with strarr_free(strarr).
 */
strarr nutclient_get_device_commands(NUTCLIENT_t client, const char* dev);

/**
 * Test if a command is supported by the device and the client.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param cmd Command name.
 * \return 1 if supported, 0 otherwise.
 */
int nutclient_has_device_command(NUTCLIENT_t client, const char* dev, const char* cmd);

/**
 * Intend to retrieve device command description.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param cmd Command name.
 * \return Description of device command. Must be freed after use.
 */
char* nutclient_get_device_command_description(NUTCLIENT_t client, const char* dev, const char* cmd);

/**
 * Intend to execute device command.
 * \param client Nut client handle.
 * \param dev Device name.
 * \param cmd Command name.
 */
void nutclient_execute_device_command(NUTCLIENT_t client, const char* dev, const char* cmd, const char* param="");

/** \} */


/**
 * Nut TCP client dedicated types and functions
 * \{
 */
/**
 * Hidden structure representing a TCP connection to NUTD.
 * NUTCLIENT_TCP_t is back compatible to NUTCLIENT_t.
 */
typedef NUTCLIENT_t NUTCLIENT_TCP_t;

/**
 * Create a client to NUTD using a TCP connection.
 * \param host Host name to connect to.
 * \param port Host port.
 * \return New client or nullptr if failed.
 */
NUTCLIENT_TCP_t nutclient_tcp_create_client(const char* host, unsigned short port);
/**
 * Test if a nut TCP client is connected.
 * \param client Nut TCP client handle.
 * \return 1 if connected, 0 otherwise.
 */
int nutclient_tcp_is_connected(NUTCLIENT_TCP_t client);
/**
 * Disconnect a nut TCP client.
 * \param client Nut TCP client handle.
 */
void nutclient_tcp_disconnect(NUTCLIENT_TCP_t client);
/**
 * Intend to reconnect a nut TCP client.
 * \param client Nut TCP client handle.
 * \return 0 if correctly connected.
 * \todo Implement different error codes.
 */
int nutclient_tcp_reconnect(NUTCLIENT_TCP_t client);
/**
 * Set the timeout value for the TCP connection.
 * \param timeout Timeout in seconds, negative for blocking.
 */
void nutclient_tcp_set_timeout(NUTCLIENT_TCP_t client, long timeout);
/**
 * Retrieve the timeout value for the TCP connection.
 * \return Timeout value in seconds.
 */
long nutclient_tcp_get_timeout(NUTCLIENT_TCP_t client);

/** \} */

#ifdef __cplusplus
}
#endif /* __cplusplus */
/* End of C nutclient library declaration */


#endif	/* NUTCLIENT_HPP_SEEN */
