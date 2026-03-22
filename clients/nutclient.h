/* nutclient.h - definitions for nutclient C/C++ library

    Copyright (C) 2012 Eaton

        Author: Emilien Kia <emilien.kia@gmail.com>

    Copyright (C) 2024-2026 NUT Community

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

#ifndef NUTCLIENT_HPP_SEEN
#define NUTCLIENT_HPP_SEEN 1

/* Begin of C++ nutclient library declaration */
#ifdef __cplusplus

#ifdef WITH_SSL_CXX
# ifdef WITH_OPENSSL
#	include <openssl/err.h>
#	include <openssl/ssl.h>
# elif defined(WITH_NSS) /* not WITH_OPENSSL */
#	include <nss.h>
#	include <ssl.h>
# endif  /* WITH_OPENSSL | WITH_NSS */
/*
// This should not be needed if macros in code are all in the right places:
#else
# ifdef WITH_OPENSSL
#  undefine WITH_OPENSSL
# endif
# ifdef WITH_NSS
#  undefine WITH_NSS
# endif
*/
#endif	/* WITH_SSL_CXX */

#include <string>
#include <vector>
#include <map>
#include <set>
#include <exception>
#include <cstdint>
#include <ctime>

/* See include/common.h for details behind this */
#ifndef NUT_UNUSED_VARIABLE
# define NUT_UNUSED_VARIABLE(x) (void)(x)
#endif

/* Should be defined via autoconf in include/config.h -
 * if that is included earlier by the program code.
 * If not - got a fallback here:
 */
#ifndef NUT_PORT
# define NUT_PORT 3493
#endif

#define UPSCLI_SSL_CAPS_NONE	0	/* No ability to use SSL */
#define UPSCLI_SSL_CAPS_OPENSSL	1	/* Can use OpenSSL-specific setup */
#define UPSCLI_SSL_CAPS_NSS	2	/* Can use Mozilla NSS-specific setup */


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
 * Base class of SSL configuration for NUT connections.
 */
class SSLConfig
{
public:
	SSLConfig(bool force_ssl = false, int certverify = -1)
		: _force_ssl(force_ssl), _certverify(certverify) {}
	virtual ~SSLConfig();

	bool getForceSsl() const { return _force_ssl; }
	int getCertVerify() const { return _certverify; }

	virtual void apply(TcpClient& client) const;

protected:
	bool _force_ssl;
	int _certverify;
};

/**
 * SSL configuration with added options specific for OpenSSL.
 */
class SSLConfig_OpenSSL : public SSLConfig
{
public:
	SSLConfig_OpenSSL(bool force_ssl = false, int certverify = -1,
		const std::string& ca_path = "", const std::string& ca_file = "",
		const std::string& cert_file = "", const std::string& key_file = "",
		const std::string& key_pass = "")
		: SSLConfig(force_ssl, certverify), _ca_path(ca_path), _ca_file(ca_file),
		  _cert_file(cert_file), _key_file(key_file), _key_pass(key_pass) {}

	SSLConfig_OpenSSL(bool force_ssl, int certverify,
		const char *ca_path, const char *ca_file,
		const char *cert_file, const char *key_file,
		const char *key_pass)
		: SSLConfig(force_ssl, certverify), _ca_path(ca_path), _ca_file(ca_file),
		  _cert_file(cert_file), _key_file(key_file), _key_pass(key_pass) {}

	const std::string& getCAPath() const { return _ca_path; }
	const std::string& getCAFile() const { return _ca_file; }
	const std::string& getCertFile() const { return _cert_file; }
	const std::string& getKeyFile() const { return _key_file; }
	const std::string& getKeyPass() const { return _key_pass; }

	virtual void apply(TcpClient& client) const override;

private:
	std::string _ca_path;
	std::string _ca_file;
	std::string _cert_file;
	std::string _key_file;
	std::string _key_pass;
};

/**
 * SSL configuration with added options specific for Mozilla NSS.
 */
class SSLConfig_NSS : public SSLConfig
{
public:
	SSLConfig_NSS(bool force_ssl = false, int certverify = -1,
		const std::string& certstore_path = "", const std::string& certstore_pass = "",
		const std::string& certstore_prefix = "", const std::string& certhost_name = "",
		const std::string& certident_name = "")
		: SSLConfig(force_ssl, certverify), _certstore_path(certstore_path),
		  _certstore_pass(certstore_pass), _certstore_prefix(certstore_prefix),
		  _certhost_name(certhost_name), _certident_name(certident_name) {}

	SSLConfig_NSS(bool force_ssl, int certverify,
		const char *certstore_path, const char *certstore_pass,
		const char *certstore_prefix, const char *certhost_name,
		const char *certident_name)
		: SSLConfig(force_ssl, certverify), _certstore_path(certstore_path),
		  _certstore_pass(certstore_pass), _certstore_prefix(certstore_prefix),
		  _certhost_name(certhost_name), _certident_name(certident_name) {}

	const std::string& getCertStorePath() const { return _certstore_path; }
	const std::string& getCertStorePass() const { return _certstore_pass; }
	const std::string& getCertStorePrefix() const { return _certstore_prefix; }
	const std::string& getCertHostName() const { return _certhost_name; }
	const std::string& getCertIdentName() const { return _certident_name; }

	virtual void apply(TcpClient& client) const override;

private:
	std::string _certstore_path;
	std::string _certstore_pass;
	std::string _certstore_prefix;
	std::string _certhost_name;
	std::string _certident_name;
};

/**
 * Basic nut exception.
 */
class NutException : public std::exception
{
public:
	NutException(const std::string& msg):_msg(msg){}
	NutException(const NutException&) = default;
	NutException& operator=(NutException& rhs) = default;
	virtual ~NutException() noexcept override;
	virtual const char * what() const noexcept override {return this->_msg.c_str();}
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
	SystemException(const SystemException&) = default;
	SystemException& operator=(SystemException& rhs) = default;
	virtual ~SystemException() noexcept override;
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
	IOException(const IOException&) = default;
	IOException& operator=(IOException& rhs) = default;
	virtual ~IOException() noexcept override;
};

/**
 * IO oriented nut exceptions specialized for SSL secured channel setup problems.
 */
class SSLException : public IOException
{
public:
	SSLException(const std::string& msg):IOException(msg){}
	SSLException():IOException("SSL failure"){}
	SSLException(const SSLException&) = default;
	SSLException& operator=(SSLException& rhs) = default;
	virtual ~SSLException() noexcept override;
};

class SSLException_OpenSSL : public SSLException
{
public:
	SSLException_OpenSSL(const std::string& msg):SSLException(std::string("OpenSSL: ") + msg){}
	SSLException_OpenSSL():SSLException("OpenSSL: failure"){}
	SSLException_OpenSSL(const SSLException_OpenSSL&) = default;
	SSLException_OpenSSL& operator=(SSLException_OpenSSL& rhs) = default;
	virtual ~SSLException_OpenSSL() noexcept override;
};

class SSLException_NSS : public SSLException
{
public:
	SSLException_NSS(const std::string& msg):SSLException(std::string("NSS: ") + msg){}
	SSLException_NSS():SSLException("NSS: failure"){}
	SSLException_NSS(const SSLException_NSS&) = default;
	SSLException_NSS& operator=(SSLException_NSS& rhs) = default;
	virtual ~SSLException_NSS() noexcept override;
};

/**
 * IO oriented nut exception specialized for unknown host
 */
class UnknownHostException : public IOException
{
public:
	UnknownHostException():IOException("Unknown host"){}
	UnknownHostException(const UnknownHostException&) = default;
	UnknownHostException& operator=(UnknownHostException& rhs) = default;
	virtual ~UnknownHostException() noexcept override;
};

/**
 * IO oriented nut exception when client is not connected
 */
class NotConnectedException : public IOException
{
public:
	NotConnectedException():IOException("Not connected"){}
	NotConnectedException(const NotConnectedException&) = default;
	NotConnectedException& operator=(NotConnectedException& rhs) = default;
	virtual ~NotConnectedException() noexcept override;
};

/**
 * IO oriented nut exception when there is no response.
 */
class TimeoutException : public IOException
{
public:
	TimeoutException():IOException("Timeout"){}
	TimeoutException(const TimeoutException&) = default;
	TimeoutException& operator=(TimeoutException& rhs) = default;
	virtual ~TimeoutException() noexcept override;
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
	 * Retrieve the number of user logged-in for the specified device.
	 * \param dev Device name.
	 * \return Number of logged-in users.
	 */
	virtual int deviceGetNumLogins(const std::string& dev) = 0;
	/**
	 * Who did a deviceLogin() to this dev?
	 * \param dev Device name.
	 * \return List of clients e.g. {'127.0.0.1', 'admin-workstation.local.domain'}
	 */
	virtual std::set<std::string> deviceGetClients(const std::string& dev) = 0;
	/* NOTE: "master" is deprecated since NUT v2.8.0 in favor of "primary".
	 * For the sake of old/new server/client interoperability,
	 * practical implementations should try to use one and fall
	 * back to the other, and only fail if both return "ERR".
	 */
	virtual void deviceMaster(const std::string& dev) = 0;
	virtual void devicePrimary(const std::string& dev) = 0;
	virtual void deviceForcedShutdown(const std::string& dev) = 0;

	/**
	 * Lists all clients of all devices (which have at least one client)
	 * \return Map with device names vs. list of their connected clients
	 */
	virtual std::map<std::string, std::set<std::string>> listDeviceClients(void) = 0;

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
 * It connects to NUTD with a TCP socket.
 */
class TcpClient : public Client
{
	/* We have a number of direct-call methods we do not expose
	 * generally, but still want covered with integration tests
	 */
	friend class NutActiveClientTest;
	/* The SSL options are stamped via apply() methods */
	friend class SSLConfig;
	friend class SSLConfig_OpenSSL;
	friend class SSLConfig_NSS;

public:
	/**
	 * Construct a NUT TcpClient object.
	 * You must call one of TcpClient::connect() after.
	 */
	TcpClient();

	/**
	 * Construct a NUT TcpClient object, then connect it to the
	 * specified server right away (without any SSL options).
	 * \param host Server host name.
	 * \param port Server port.
	 */
	TcpClient(const std::string& host, uint16_t port = NUT_PORT);

	/**
	 * Construct a NUT TcpClient object with SSL options,
	 * then connect it to the specified server right away.
	 * \param host Server host name.
	 * \param port Server port.
	 * \param config SSL configuration (typically a derived
	 *               class for OpenSSL or NSS).
	 */
	TcpClient(const std::string& host, uint16_t port, const SSLConfig& config);

	~TcpClient() override;

	/**
	 * Set SSL configuration.
	 * \param config SSL configuration (typically a derived
	 *               class for OpenSSL or NSS).
	 */
	void setSSLConfig(const SSLConfig& config);

	/**
	 * Connect it to the specified server.
	 * \param host Server host name.
	 * \param port Server port.
	 */
	void connect(const std::string& host, uint16_t port = NUT_PORT);

	/**
	 * Connect it to the specified server with explicit toggle
	 * to allow (or not) use of SSL/TLS.
	 * \param host Server host name.
	 * \param port Server port.
	 * \param try_ssl Use SSL/TLS for the connection (may be
	                  overridden by force_ssl if set to true earlier).
	 */
	void connect(const std::string& host, uint16_t port, bool try_ssl);

	/**
	 * Connect to the server.
	 * Host name and ports must have already set (useful for reconnection).
	 */
	void connect();

	/**
	 * Enable or disable std::cerr tracing of internal Socket operations
	 * during connect() processing. Primarily for developer troubleshooting.
	 */
	void setDebugConnect(bool d);

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
	void setTimeout(time_t timeout);

	/**
	 * Retrieve the timeout.
	 * \returns Current timeout in seconds.
	 */
	time_t getTimeout()const;

	/**
	 * Retriueve the host name of the server the client is connected to.
	 * \return Server host name
	 */
	std::string getHost()const;
	/**
	 * Retriueve the port of host of the server the client is connected to.
	 * \return Server port
	 */
	uint16_t getPort()const;

	virtual void authenticate(const std::string& user, const std::string& passwd) override;
	virtual void logout() override;

	virtual Device getDevice(const std::string& name) override;
	virtual std::set<std::string> getDeviceNames() override;
	virtual std::string getDeviceDescription(const std::string& name) override;

	virtual std::set<std::string> getDeviceVariableNames(const std::string& dev) override;
	virtual std::set<std::string> getDeviceRWVariableNames(const std::string& dev) override;
	virtual std::string getDeviceVariableDescription(const std::string& dev, const std::string& name) override;
	virtual std::vector<std::string> getDeviceVariableValue(const std::string& dev, const std::string& name) override;
	virtual std::map<std::string,std::vector<std::string> > getDeviceVariableValues(const std::string& dev) override;
	virtual std::map<std::string,std::map<std::string,std::vector<std::string> > > getDevicesVariableValues(const std::set<std::string>& devs) override;
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value) override;
	virtual TrackingID setDeviceVariable(const std::string& dev, const std::string& name, const std::vector<std::string>& values) override;

	virtual std::set<std::string> getDeviceCommandNames(const std::string& dev) override;
	virtual std::string getDeviceCommandDescription(const std::string& dev, const std::string& name) override;
	virtual TrackingID executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param="") override;

	virtual void deviceLogin(const std::string& dev) override;
	/* FIXME: Protocol update needed to handle master/primary alias
	 * and probably an API bump also, to rename/alias the routine.
	 */
	virtual void deviceMaster(const std::string& dev) override;
	virtual void devicePrimary(const std::string& dev) override;
	virtual void deviceForcedShutdown(const std::string& dev) override;
	virtual int deviceGetNumLogins(const std::string& dev) override;
	virtual std::set<std::string> deviceGetClients(const std::string& dev) override;

	virtual std::map<std::string, std::set<std::string>> listDeviceClients(void) override;

	virtual TrackingResult getTrackingResult(const TrackingID& id) override;

	/**
	 * Return a bitmask of SSL capabilities supported by this build of
	 * libnutclient, see UPSCLI_SSL_CAPS_NONE, UPSCLI_SSL_CAPS_OPENSSL,
	 * UPSCLI_SSL_CAPS_NSS. */
	static int getSslCaps();

	virtual bool isSSL() const;

	virtual bool getSslTry() const;
	virtual void setSslTry(bool try_ssl);

	virtual bool getSslForce() const;
	virtual void setSslForce(bool force_ssl);

	virtual int getSslCertVerify() const;
	virtual void setSslCertVerify(int certverify);

	virtual const std::string& getSslCAPath() const;
	virtual void setSslCAPath(const char* ca_path);
	virtual void setSslCAPath(const std::string& ca_path);

	virtual const std::string& getSslCAFile() const;
	virtual void setSslCAFile(const char* ca_file);
	virtual void setSslCAFile(const std::string& ca_file);

	virtual const std::string& getSslCertFile() const;
	virtual void setSslCertFile(const char* cert_file);
	virtual void setSslCertFile(const std::string& cert_file);

	virtual const std::string& getSslKeyFile() const;
	virtual void setSslKeyFile(const char* key_file);
	virtual void setSslKeyFile(const std::string& key_file);

	virtual const std::string& getSslKeyPass() const;
	virtual void setSslKeyPass(const char* key_pass);
	virtual void setSslKeyPass(const std::string& key_pass);

	virtual const std::string& getSslCertstorePath() const;
	virtual void setSslCertstorePath(const char* certstore_path);
	virtual void setSslCertstorePath(const std::string& certstore_path);

	virtual const std::string& getSslCertstorePrefix() const;
	virtual void setSslCertstorePrefix(const char* certstore_prefix);
	virtual void setSslCertstorePrefix(const std::string& certstore_prefix);

	virtual const std::string& getSslCertIdentName() const;
	virtual void setSslCertIdentName(const char* certident_name);
	virtual void setSslCertIdentName(const std::string& certident_name);

	virtual const std::string& getSslCertHostName() const;
	virtual void setSslCertHostName(const char* certhost_name);
	virtual void setSslCertHostName(const std::string& certhost_name);

	virtual bool isFeatureEnabled(const Feature& feature) override;
	virtual void setFeature(const Feature& feature, bool status) override;

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

	/**
	 * Set SSL configuration for OpenSSL
	 * (with C-style string arguments for SSL-related file paths).
	 * Primarily exposed for C API bridges
	 * \param force_ssl Whether to require SSL connection.
	 * \param certverify Whether to verify the server certificate.
	 * \param ca_path Path to a directory with CA certificates (PEM format for OpenSSL).
	 * \param ca_file Path to a CA certificate file (PEM format for OpenSSL).
	 * \param cert_file Path to a client certificate file (PEM format for OpenSSL) or nickname (NSS).
	 * \param key_file Path to a client private key file (PEM format for OpenSSL).
	 * \param key_pass Optional passphrase to decrypt the private key.
	 */
	void setSSLConfig_OpenSSL(bool force_ssl, int certverify, const char *ca_path, const char *ca_file, const char *cert_file, const char *key_file, const char *key_pass);

	/**
	 * Set SSL configuration for OpenSSL.
	 * \param force_ssl Whether to require SSL connection.
	 * \param certverify Whether to verify the server certificate.
	 * \param ca_path Path to a directory with CA certificates (PEM format for OpenSSL).
	 * \param ca_file Path to a CA certificate file (PEM format for OpenSSL).
	 * \param cert_file Path to a client certificate file (PEM format for OpenSSL) or nickname (NSS).
	 * \param key_file Path to a client private key file (PEM format for OpenSSL).
	 * \param key_pass Optional passphrase to decrypt the private key.
	 */
	void setSSLConfig_OpenSSL(bool force_ssl, int certverify, const std::string& ca_path, const std::string& ca_file, const std::string& cert_file, const std::string& key_file, const std::string& key_pass);

	/**
	 * Set SSL configuration for Mozilla NSS
	 * (with C-style string arguments for SSL-related file paths).
	 * \param force_ssl Whether to require SSL connection.
	 * \param certverify Whether to verify the server certificate.
	 * \param certstore_path Path to a directory with CA, server and client certificates and private keys (3-file NSS database).
	 * \param certstore_pass Password to open the (private) key store of the database (NSS database).
	 * \param certstore_prefix Many NSS databases can be co-located in same directory, with prefixed file names.
	 * \param certhost_name Remote host name to match in the certificate (NSS database).
	 * \param certident_name Client nickname to match in the certificate (NSS database).
	 */
	void setSSLConfig_NSS(bool force_ssl, int certverify, const char *certstore_path, const char *certstore_pass, const char *certstore_prefix, const char *certhost_name, const char *certident_name);

	/**
	 * Set SSL configuration for Mozilla NSS.
	 * \param force_ssl Whether to require SSL connection.
	 * \param certverify Whether to verify the server certificate.
	 * \param certstore_path Path to a directory with CA, server and client certificates and private keys (3-file NSS database).
	 * \param certstore_pass Password to open the (private) key store of the database (NSS database).
	 * \param certstore_prefix Many NSS databases can be co-located in same directory, with prefixed file names.
	 * \param certhost_name Remote host name to match in the certificate (NSS database).
	 * \param certident_name Client nickname to match in the certificate (NSS database).
	 */
	void setSSLConfig_NSS(bool force_ssl, int certverify, const std::string& certstore_path, const std::string& certstore_pass, const std::string& certstore_prefix, const std::string& certhost_name, const std::string& certident_name);

private:
	std::string _host;
	uint16_t _port;
	/* SSL shared */
	bool _try_ssl;
	bool _force_ssl;
	int _certverify;
	/* OpenSSL specific */
	std::string _ca_path;
	std::string _ca_file;
	std::string _cert_file;
	std::string _key_file;
	/* SSL shared */
	std::string _key_pass;	/* aka certstore_pass for NSS */
	/* NSS specific */
	std::string _certstore_path;
	std::string _certstore_prefix;
	std::string _certident_name;
	std::string _certhost_name;
	/* general info */
	time_t _timeout;
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
	friend class TcpClientMock;
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
	/**
	 * Who did a login() to this dev?
	 */
	std::set<std::string> getClients();
	/* FIXME: Protocol update needed to handle master/primary alias
	 * and probably an API bump also, to rename/alias the routine.
	 */
	void master();
	void primary();
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
	friend class TcpClientMock;
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
	friend class TcpClientMock;
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
/* FIXME: Protocol update needed to handle master/primary alias
 * and probably an API bump also, to rename/alias the routine.
 */
void nutclient_device_master(NUTCLIENT_t client, const char* dev);
void nutclient_device_primary(NUTCLIENT_t client, const char* dev);

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
 * \param try_ssl Try to use SSL/TLS for the connection.
 * \param force_ssl Fail if SSL/TLS is not available or handshake fails.
 * \param certverify Whether to verify the server certificate.
 * \return New client or nullptr if failed.
 */
NUTCLIENT_TCP_t nutclient_tcp_create_client(const char* host, uint16_t port);
int nutclient_tcp_get_ssl_caps(void);

NUTCLIENT_TCP_t nutclient_tcp_create_client_ssl_OpenSSL(
	const char* host, uint16_t port, int try_ssl,
	int force_ssl, int certverify,
	const char *ca_path, const char *ca_file,
	const char *cert_file, const char *key_file, const char *key_pass);
void nutclient_tcp_set_ssl_config_OpenSSL(NUTCLIENT_TCP_t client,
	int force_ssl, int certverify,
	const char *ca_path, const char *ca_file,
	const char *cert_file, const char *key_file, const char *key_pass);

NUTCLIENT_TCP_t nutclient_tcp_create_client_ssl_NSS(
	const char* host, uint16_t port, int try_ssl,
	int force_ssl, int certverify,
	const char *certstore_path, const char *certstore_pass,
	const char *certstore_prefix,
	const char *certhost_name,
	const char *certident_name);
void nutclient_tcp_set_ssl_config_NSS(NUTCLIENT_TCP_t client,
	int force_ssl, int certverify,
	const char *certstore_path, const char *certstore_pass,
	const char *certstore_prefix,
	const char *certhost_name,
	const char *certident_name);

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
int nutclient_tcp_is_ssl(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_try(NUTCLIENT_TCP_t client, int try_ssl);
int nutclient_tcp_get_ssl_try(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_force(NUTCLIENT_TCP_t client, int force_ssl);
int nutclient_tcp_get_ssl_force(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_certverify(NUTCLIENT_TCP_t client, int certverify);
int nutclient_tcp_get_ssl_certverify(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_capath(NUTCLIENT_TCP_t client, const char* ca_path);
const char* nutclient_tcp_get_ssl_capath(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_cafile(NUTCLIENT_TCP_t client, const char* ca_file);
const char* nutclient_tcp_get_ssl_cafile(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_certfile(NUTCLIENT_TCP_t client, const char* cert_file);
const char* nutclient_tcp_get_ssl_certfile(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_keyfile(NUTCLIENT_TCP_t client, const char* key_file);
const char* nutclient_tcp_get_ssl_keyfile(NUTCLIENT_TCP_t client);

/* Also used for NSS certstore pass */
void nutclient_tcp_set_ssl_keypass(NUTCLIENT_TCP_t client, const char* key_pass);
const char* nutclient_tcp_get_ssl_keypass(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_certstore_path(NUTCLIENT_TCP_t client, const char* certstore_path);
const char* nutclient_tcp_get_ssl_certstore_path(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_certstore_prefix(NUTCLIENT_TCP_t client, const char* certstore_prefix);
const char* nutclient_tcp_get_ssl_certstore_prefix(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_certident_name(NUTCLIENT_TCP_t client, const char* certident_name);
const char* nutclient_tcp_get_ssl_certident_name(NUTCLIENT_TCP_t client);

void nutclient_tcp_set_ssl_certhost_name(NUTCLIENT_TCP_t client, const char* certhost_name);
const char* nutclient_tcp_get_ssl_certhost_name(NUTCLIENT_TCP_t client);

/**
 * Set the timeout value for the TCP connection.
 * \param timeout Timeout in seconds, negative for blocking.
 */
void nutclient_tcp_set_timeout(NUTCLIENT_TCP_t client, time_t timeout);
/**
 * Retrieve the timeout value for the TCP connection.
 * \return Timeout value in seconds.
 */
time_t nutclient_tcp_get_timeout(NUTCLIENT_TCP_t client);

/** \} */

#ifdef __cplusplus
}
#endif /* __cplusplus */
/* End of C nutclient library declaration */


#endif	/* NUTCLIENT_HPP_SEEN */
