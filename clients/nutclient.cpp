/* nutclient.cpp - nutclient C++ library implementation

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

#include "nutclient.h"

#include <sstream>

#include <errno.h>
#include <string.h>
#include <stdio.h>

/* Windows/Linux Socket compatibility layer: */
/* Thanks to Benjamin Roux (http://broux.developpez.com/articles/c/sockets/) */
#ifdef WIN32
#  include <winsock2.h>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h> /* close */
#  include <netdb.h> /* gethostbyname */
#  include <fcntl.h>
#  define INVALID_SOCKET -1
#  define SOCKET_ERROR -1
#  define closesocket(s) close(s)
   typedef int SOCKET;
   typedef struct sockaddr_in SOCKADDR_IN;
   typedef struct sockaddr SOCKADDR;
   typedef struct in_addr IN_ADDR;
#endif /* WIN32 */
/* End of Windows/Linux Socket compatibility layer: */


/* Include nut common utility functions or define simple ones if not */
#ifdef HAVE_NUTCOMMON
#include "common.h"
#else /* HAVE_NUTCOMMON */
#include <stdlib.h>
#include <string.h>
static inline void *xmalloc(size_t size){return malloc(size);}
static inline void *xcalloc(size_t number, size_t size){return calloc(number, size);}
static inline void *xrealloc(void *ptr, size_t size){return realloc(ptr, size);}
static inline char *xstrdup(const char *string){return strdup(string);}
#endif /* HAVE_NUTCOMMON */

/* To stay in line with modern C++, we use nullptr (not numeric NULL
 * or shim __null on some systems) which was defined after C++98.
 * The NUT C++ interface is intended for C++11 and newer, so we
 * quiesce these warnigns if possible.
 * An idea might be to detect if we do build with old C++ standard versions
 * and define a nullptr like https://stackoverflow.com/a/44517878/4715872
 * but again - currently we do not intend to support that officially.
 */
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT_PEDANTIC
#pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT
#pragma GCC diagnostic ignored "-Wc++98-compat"
#endif

namespace nut
{

SystemException::SystemException():
NutException(err())
{
}

std::string SystemException::err()
{
	if(errno==0)
		return "Undefined system error";
	else
	{
		std::stringstream str;
		str << "System error " << errno << ": " << strerror(errno);
		return str.str();
	}
}


namespace internal
{

/**
 * Internal socket wrapper.
 * Provides only client socket functions.
 *
 * Implemented as separate internal class to easily hide plateform specificities.
 */
class Socket
{
public:
	Socket();
	~Socket();

	void connect(const std::string& host, int port);
	void disconnect();
	bool isConnected()const;

	void setTimeout(long timeout);
	bool hasTimeout()const{return _tv.tv_sec>=0;}

	size_t read(void* buf, size_t sz);
	size_t write(const void* buf, size_t sz);

	std::string read();
	void write(const std::string& str);


private:
	SOCKET _sock;
	struct timeval	_tv;
	std::string _buffer; /* Received buffer, string because data should be text only. */
};

Socket::Socket():
_sock(INVALID_SOCKET),
_tv()
{
	_tv.tv_sec = -1;
	_tv.tv_usec = 0;
}

Socket::~Socket()
{
	disconnect();
}

void Socket::setTimeout(long timeout)
{
	_tv.tv_sec = timeout;
}

void Socket::connect(const std::string& host, int port)
{
	int	sock_fd;
	struct addrinfo	hints, *res, *ai;
	char			sport[NI_MAXSERV];
	int			v;
	fd_set 			wfds;
	int			error;
	socklen_t		error_size;
	long			fd_flags;

	_sock = -1;

	if (host.empty()) {
		throw nut::UnknownHostException();
	}

	snprintf(sport, sizeof(sport), "%hu", static_cast<unsigned short int>(port));

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	while ((v = getaddrinfo(host.c_str(), sport, &hints, &res)) != 0) {
		switch (v)
		{
		case EAI_AGAIN:
			continue;
		case EAI_NONAME:
			throw nut::UnknownHostException();
		case EAI_SYSTEM:
			throw nut::SystemException();
		case EAI_MEMORY:
			throw nut::NutException("Out of memory");
		default:
			throw nut::NutException("Unknown error");
		}
	}

	for (ai = res; ai != nullptr; ai = ai->ai_next) {

		sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (sock_fd < 0) {
			switch (errno)
			{
			case EAFNOSUPPORT:
			case EINVAL:
				break;
			default:
				throw nut::SystemException();
			}
			continue;
		}

		/* non blocking connect */
		if(hasTimeout()) {
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags |= O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
		}

		while ((v = ::connect(sock_fd, ai->ai_addr, ai->ai_addrlen)) < 0) {
			if(errno == EINPROGRESS) {
				FD_ZERO(&wfds);
				FD_SET(sock_fd, &wfds);
				select(sock_fd+1, nullptr, &wfds, nullptr, hasTimeout() ? &_tv : nullptr);
				if (FD_ISSET(sock_fd, &wfds)) {
					error_size = sizeof(error);
					getsockopt(sock_fd, SOL_SOCKET, SO_ERROR,
							&error, &error_size);
					if( error == 0) {
						/* connect successful */
						v = 0;
						break;
					}
					errno = error;
				}
				else {
					/* Timeout */
					v = -1;
					break;
				}
			}

			switch (errno)
			{
			case EAFNOSUPPORT:
				break;
			case EINTR:
			case EAGAIN:
				continue;
			default:
//				ups->upserror = UPSCLI_ERR_CONNFAILURE;
//				ups->syserrno = errno;
				break;
			}
			break;
		}

		if (v < 0) {
			close(sock_fd);
			continue;
		}

		/* switch back to blocking operation */
		if(hasTimeout()) {
			fd_flags = fcntl(sock_fd, F_GETFL);
			fd_flags &= ~O_NONBLOCK;
			fcntl(sock_fd, F_SETFL, fd_flags);
		}

		_sock = sock_fd;
//		ups->upserror = 0;
//		ups->syserrno = 0;
		break;
	}

	freeaddrinfo(res);

	if (_sock < 0) {
		throw nut::IOException("Cannot connect to host");
	}


#ifdef OLD
	struct hostent *hostinfo = nullptr;
	SOCKADDR_IN sin = { 0 };
	hostinfo = ::gethostbyname(host.c_str());
	if(hostinfo == nullptr) /* Host doesnt exist */
	{
		throw nut::UnknownHostException();
	}

	// Create socket
	_sock = ::socket(PF_INET, SOCK_STREAM, 0);
	if(_sock == INVALID_SOCKET)
	{
		throw nut::IOException("Cannot create socket");
	}

	// Connect
	sin.sin_addr = *(IN_ADDR *) hostinfo->h_addr;
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	if(::connect(_sock,(SOCKADDR *) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		_sock = INVALID_SOCKET;
		throw nut::IOException("Cannot connect to host");
	}
#endif // OLD
}

void Socket::disconnect()
{
	if(_sock != INVALID_SOCKET)
	{
		::closesocket(_sock);
		_sock = INVALID_SOCKET;
	}
	_buffer.clear();
}

bool Socket::isConnected()const
{
	return _sock!=INVALID_SOCKET;
}

size_t Socket::read(void* buf, size_t sz)
{
	if(!isConnected())
	{
		throw nut::NotConnectedException();
	}

	if(_tv.tv_sec>=0)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(_sock, &fds);
		int ret = select(_sock+1, &fds, nullptr, nullptr, &_tv);
		if (ret < 1) {
			throw nut::TimeoutException();
		}
	}

	ssize_t res = ::read(_sock, buf, sz);
	if(res==-1)
	{
		disconnect();
		throw nut::IOException("Error while reading on socket");
	}
	return static_cast<size_t>(res);
}

size_t Socket::write(const void* buf, size_t sz)
{
	if(!isConnected())
	{
		throw nut::NotConnectedException();
	}

	if(_tv.tv_sec>=0)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(_sock, &fds);
		int ret = select(_sock+1, nullptr, &fds, nullptr, &_tv);
		if (ret < 1) {
			throw nut::TimeoutException();
		}
	}

	ssize_t res = ::write(_sock, buf, sz);
	if(res==-1)
	{
		disconnect();
		throw nut::IOException("Error while writing on socket");
	}
	return static_cast<size_t>(res);
}

std::string Socket::read()
{
	std::string res;
	char buff[256];

	while(true)
	{
		// Look at already read data in _buffer
		if(!_buffer.empty())
		{
			size_t idx = _buffer.find('\n');
			if(idx!=std::string::npos)
			{
				res += _buffer.substr(0, idx);
				_buffer.erase(0, idx+1);
				return res;
			}
			res += _buffer;
		}

		// Read new buffer
		size_t sz = read(&buff, 256);
		if(sz==0)
		{
			disconnect();
			throw nut::IOException("Server closed connection unexpectedly");
		}
		_buffer.assign(buff, sz);
	}
}

void Socket::write(const std::string& str)
{
//	write(str.c_str(), str.size());
//	write("\n", 1);
	std::string buff = str + "\n";
	write(buff.c_str(), buff.size());
}

}/* namespace internal */


/*
 *
 * Client implementation
 *
 */

/* Pedantic builds complain about the static variable below...
 * It is assumed safe to ignore since it is a std::string with
 * no complex teardown at program exit.
 */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic push
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS
#  pragma GCC diagnostic ignored "-Wglobal-constructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS
#  pragma GCC diagnostic ignored "-Wexit-time-destructors"
# endif
#endif
const Feature Client::TRACKING = "TRACKING";
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic pop
#endif

Client::Client()
{
}

Client::~Client()
{
}

bool Client::hasDevice(const std::string& dev)
{
	std::set<std::string> devs = getDeviceNames();
	return devs.find(dev) != devs.end();
}

Device Client::getDevice(const std::string& name)
{
	if(hasDevice(name))
		return Device(this, name);
	else
		return Device(nullptr, "");
}

std::set<Device> Client::getDevices()
{
	std::set<Device> res;

	std::set<std::string> devs = getDeviceNames();
	for(std::set<std::string>::iterator it=devs.begin(); it!=devs.end(); ++it)
	{
		res.insert(Device(this, *it));
	}

	return res;
}

bool Client::hasDeviceVariable(const std::string& dev, const std::string& name)
{
	std::set<std::string> names = getDeviceVariableNames(dev);
	return names.find(name) != names.end();
}

std::map<std::string,std::vector<std::string> > Client::getDeviceVariableValues(const std::string& dev)
{
	std::map<std::string,std::vector<std::string> > res;

	std::set<std::string> names = getDeviceVariableNames(dev);
	for(std::set<std::string>::iterator it=names.begin(); it!=names.end(); ++it)
	{
		const std::string& name = *it;
		res[name] = getDeviceVariableValue(dev, name);
	}

	return res;
}

std::map<std::string,std::map<std::string,std::vector<std::string> > > Client::getDevicesVariableValues(const std::set<std::string>& devs)
{
	std::map<std::string,std::map<std::string,std::vector<std::string> > > res;

	for(std::set<std::string>::const_iterator it=devs.cbegin(); it!=devs.cend(); ++it)
	{
		res[*it] = getDeviceVariableValues(*it);
	}

	return res;
}

bool Client::hasDeviceCommand(const std::string& dev, const std::string& name)
{
	std::set<std::string> names = getDeviceCommandNames(dev);
	return names.find(name) != names.end();
}

bool Client::hasFeature(const Feature& feature)
{
	try
	{
		// If feature is known, querying it won't throw an exception.
		isFeatureEnabled(feature);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

/*
 *
 * TCP Client implementation
 *
 */

TcpClient::TcpClient():
Client(),
_host("localhost"),
_port(3493),
_timeout(0),
_socket(new internal::Socket)
{
	// Do not connect now
}

TcpClient::TcpClient(const std::string& host, int port):
Client(),
_timeout(0),
_socket(new internal::Socket)
{
	connect(host, port);
}

TcpClient::~TcpClient()
{
	delete _socket;
}

void TcpClient::connect(const std::string& host, int port)
{
	_host = host;
	_port = port;
	connect();
}

void TcpClient::connect()
{
	_socket->connect(_host, _port);
}

std::string TcpClient::getHost()const
{
	return _host;
}

int TcpClient::getPort()const
{
	return _port;
}

bool TcpClient::isConnected()const
{
	return _socket->isConnected();
}

void TcpClient::disconnect()
{
	_socket->disconnect();
}

void TcpClient::setTimeout(long timeout)
{
	_timeout = timeout;
}

long TcpClient::getTimeout()const
{
	return _timeout;
}

void TcpClient::authenticate(const std::string& user, const std::string& passwd)
{
	detectError(sendQuery("USERNAME " + user));
	detectError(sendQuery("PASSWORD " + passwd));
}

void TcpClient::logout()
{
	detectError(sendQuery("LOGOUT"));
	_socket->disconnect();
}

Device TcpClient::getDevice(const std::string& name)
{
	try
	{
		get("UPSDESC", name);
	}
	catch(NutException& ex)
	{
		if(ex.str()=="UNKNOWN-UPS")
			return Device(nullptr, "");
		else
			throw;
	}
	return Device(this, name);
}

std::set<std::string> TcpClient::getDeviceNames()
{
	std::set<std::string> res;

	std::vector<std::vector<std::string> > devs = list("UPS");
	for(std::vector<std::vector<std::string> >::iterator it=devs.begin();
		it!=devs.end(); ++it)
	{
		std::string id = (*it)[0];
		if(!id.empty())
			res.insert(id);
	}

	return res;
}

std::string TcpClient::getDeviceDescription(const std::string& name)
{
	return get("UPSDESC", name)[0];
}

std::set<std::string> TcpClient::getDeviceVariableNames(const std::string& dev)
{
	std::set<std::string> set;

	std::vector<std::vector<std::string> > res = list("VAR", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(res[n][0]);
	}

	return set;
}

std::set<std::string> TcpClient::getDeviceRWVariableNames(const std::string& dev)
{
	std::set<std::string> set;

	std::vector<std::vector<std::string> > res = list("RW", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(res[n][0]);
	}

	return set;
}

std::string TcpClient::getDeviceVariableDescription(const std::string& dev, const std::string& name)
{
	return get("DESC", dev + " " + name)[0];
}

std::vector<std::string> TcpClient::getDeviceVariableValue(const std::string& dev, const std::string& name)
{
	return get("VAR", dev + " " + name);
}

std::map<std::string,std::vector<std::string> > TcpClient::getDeviceVariableValues(const std::string& dev)
{

	std::map<std::string,std::vector<std::string> >  map;

	std::vector<std::vector<std::string> > res = list("VAR", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		std::vector<std::string>& vals = res[n];
		std::string var = vals[0];
		vals.erase(vals.begin());
		map[var] = vals;
	}

	return map;
}

std::map<std::string,std::map<std::string,std::vector<std::string> > > TcpClient::getDevicesVariableValues(const std::set<std::string>& devs)
{
	std::map<std::string,std::map<std::string,std::vector<std::string> > > map;

	if (devs.empty())
	{
		// This request might come from processing the empty valid
		// response of an upsd server which was allowed to start
		// with no device sections in its ups.conf
		return map;
	}

	std::vector<std::string> queries;
	for (std::set<std::string>::const_iterator it=devs.cbegin(); it!=devs.cend(); ++it)
	{
		queries.push_back("LIST VAR " + *it);
	}
	sendAsyncQueries(queries);

	for (std::set<std::string>::const_iterator it=devs.cbegin(); it!=devs.cend(); ++it)
	{
		try
		{
			std::map<std::string,std::vector<std::string> > map2;
			std::vector<std::vector<std::string> > res = parseList("VAR " + *it);
			for (std::vector<std::vector<std::string> >::iterator it2=res.begin(); it2!=res.end(); ++it2)
			{
				std::vector<std::string>& vals = *it2;
				std::string var = vals[0];
				vals.erase(vals.begin()),
				map2[var] = vals;
			}
			map[*it] = map2;
		}
		catch (NutException&)
		{
			// We sent a bunch of queries, we need to process them all to clear up the backlog.
		}
	}

	if (map.empty())
	{
		// We may fail on some devices, but not on ALL devices.
		throw NutException("Invalid device");
	}

	return map;
}

TrackingID TcpClient::setDeviceVariable(const std::string& dev, const std::string& name, const std::string& value)
{
	std::string query = "SET VAR " + dev + " " + name + " " + escape(value);
	return sendTrackingQuery(query);
}

TrackingID TcpClient::setDeviceVariable(const std::string& dev, const std::string& name, const std::vector<std::string>& values)
{
	std::string query = "SET VAR " + dev + " " + name;
	for(size_t n=0; n<values.size(); ++n)
	{
		query += " " + escape(values[n]);
	}
	return sendTrackingQuery(query);
}

std::set<std::string> TcpClient::getDeviceCommandNames(const std::string& dev)
{
	std::set<std::string> cmds;

	std::vector<std::vector<std::string> > res = list("CMD", dev);
	for(size_t n=0; n<res.size(); ++n)
	{
		cmds.insert(res[n][0]);
	}

	return cmds;
}

std::string TcpClient::getDeviceCommandDescription(const std::string& dev, const std::string& name)
{
	return get("CMDDESC", dev + " " + name)[0];
}

TrackingID TcpClient::executeDeviceCommand(const std::string& dev, const std::string& name, const std::string& param)
{
	return sendTrackingQuery("INSTCMD " + dev + " " + name + " " + param);
}

void TcpClient::deviceLogin(const std::string& dev)
{
	detectError(sendQuery("LOGIN " + dev));
}

void TcpClient::deviceMaster(const std::string& dev)
{
	detectError(sendQuery("MASTER " + dev));
}

void TcpClient::deviceForcedShutdown(const std::string& dev)
{
	detectError(sendQuery("FSD " + dev));
}

int TcpClient::deviceGetNumLogins(const std::string& dev)
{
	std::string num = get("NUMLOGINS", dev)[0];
	return atoi(num.c_str());
}

TrackingResult TcpClient::getTrackingResult(const TrackingID& id)
{
	if (id.empty())
	{
		return TrackingResult::SUCCESS;
	}

	std::string result = sendQuery("GET TRACKING " + id);

	if (result == "PENDING")
	{
		return TrackingResult::PENDING;
	}
	else if (result == "SUCCESS")
	{
		return TrackingResult::SUCCESS;
	}
	else if (result == "ERR UNKNOWN")
	{
		return TrackingResult::UNKNOWN;
	}
	else if (result == "ERR INVALID-ARGUMENT")
	{
		return TrackingResult::INVALID_ARGUMENT;
	}
	else
	{
		return TrackingResult::FAILURE;
	}
}

bool TcpClient::isFeatureEnabled(const Feature& feature)
{
	std::string result = sendQuery("GET " + feature);
	detectError(result);

	if (result == "ON")
	{
		return true;
	}
	else if (result == "OFF")
	{
		return false;
	}
	else
	{
		throw NutException("Unknown feature result " + result);
	}
}
void TcpClient::setFeature(const Feature& feature, bool status)
{
	std::string result = sendQuery("SET " + feature + " " + (status ? "ON" : "OFF"));
	detectError(result);
}

std::vector<std::string> TcpClient::get
	(const std::string& subcmd, const std::string& params)
{
	std::string req = subcmd;
	if(!params.empty())
	{
		req += " " + params;
	}
	std::string res = sendQuery("GET " + req);
	detectError(res);
	if(res.substr(0, req.size()) != req)
	{
		throw NutException("Invalid response");
	}

	return explode(res, req.size());
}

std::vector<std::vector<std::string> > TcpClient::list
	(const std::string& subcmd, const std::string& params)
{
	std::string req = subcmd;
	if(!params.empty())
	{
		req += " " + params;
	}
	std::vector<std::string> query;
	query.push_back("LIST " + req);
	sendAsyncQueries(query);
	return parseList(req);
}

std::vector<std::vector<std::string> > TcpClient::parseList
	(const std::string& req)
{
	std::string res = _socket->read();
	detectError(res);
	if(res != ("BEGIN LIST " + req))
	{
		throw NutException("Invalid response");
	}

	std::vector<std::vector<std::string> > arr;
	while(true)
	{
		res = _socket->read();
		detectError(res);
		if(res == ("END LIST " + req))
		{
			return arr;
		}
		if(res.substr(0, req.size()) == req)
		{
			arr.push_back(explode(res, req.size()));
		}
		else
		{
			throw NutException("Invalid response");
		}
	}
}

std::string TcpClient::sendQuery(const std::string& req)
{
	_socket->write(req);
	return _socket->read();
}

void TcpClient::sendAsyncQueries(const std::vector<std::string>& req)
{
	for (std::vector<std::string>::const_iterator it = req.cbegin(); it != req.cend(); ++it)
	{
		_socket->write(*it);
	}
}

void TcpClient::detectError(const std::string& req)
{
	if(req.substr(0,3)=="ERR")
	{
		throw NutException(req.substr(4));
	}
}

std::vector<std::string> TcpClient::explode(const std::string& str, size_t begin)
{
	std::vector<std::string> res;
	std::string temp;

	enum STATE {
		INIT,
		SIMPLE_STRING,
		QUOTED_STRING,
		SIMPLE_ESCAPE,
		QUOTED_ESCAPE
	} state = INIT;

	for(size_t idx=begin; idx<str.size(); ++idx)
	{
		char c = str[idx];
		switch(state)
		{
		case INIT:
			if(c==' ' /* || c=='\t' */)
			{ /* Do nothing */ }
			else if(c=='"')
			{
				state = QUOTED_STRING;
			}
			else if(c=='\\')
			{
				state = SIMPLE_ESCAPE;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
				state = SIMPLE_STRING;
			}
			break;
		case SIMPLE_STRING:
			if(c==' ' /* || c=='\t' */)
			{
				/* if(!temp.empty()) : Must not occur */
					res.push_back(temp);
				temp.clear();
				state = INIT;
			}
			else if(c=='\\')
			{
				state = SIMPLE_ESCAPE;
			}
			else if(c=='"')
			{
				/* if(!temp.empty()) : Must not occur */
					res.push_back(temp);
				temp.clear();
				state = QUOTED_STRING;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
			}
			break;
		case QUOTED_STRING:
			if(c=='\\')
			{
				state = QUOTED_ESCAPE;
			}
			else if(c=='"')
			{
				res.push_back(temp);
				temp.clear();
				state = INIT;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
			}
			break;
		case SIMPLE_ESCAPE:
			if(c=='\\' || c=='"' || c==' ' /* || c=='\t'*/)
			{
				temp += c;
			}
			else
			{
				temp += '\\' + c; // Really do this ?
			}
			state = SIMPLE_STRING;
			break;
		case QUOTED_ESCAPE:
			if(c=='\\' || c=='"')
			{
				temp += c;
			}
			else
			{
				temp += '\\' + c; // Really do this ?
			}
			state = QUOTED_STRING;
			break;
		}
	}

	if(!temp.empty())
	{
		res.push_back(temp);
	}

	return res;
}

std::string TcpClient::escape(const std::string& str)
{
	std::string res = "\"";

	for(size_t n=0; n<str.size(); n++)
	{
		char c = str[n];
		if(c=='"')
			res += "\\\"";
		else if(c=='\\')
			res += "\\\\";
		else
			res += c;
	}

	res += '"';
	return res;
}

TrackingID TcpClient::sendTrackingQuery(const std::string& req)
{
	std::string reply = sendQuery(req);
	detectError(reply);
	std::vector<std::string> res = explode(reply);

	if (res.size() == 1 && res[0] == "OK")
	{
		return TrackingID("");
	}
	else if (res.size() == 3 && res[0] == "OK" && res[1] == "TRACKING")
	{
		return TrackingID(res[2]);
	}
	else
	{
		throw NutException("Unknown query result");
	}
}

/*
 *
 * Device implementation
 *
 */

Device::Device(Client* client, const std::string& name):
_client(client),
_name(name)
{
}

Device::Device(const Device& dev):
_client(dev._client),
_name(dev._name)
{
}

Device& Device::operator=(const Device& dev)
{
	// Self assignment?
	if (this==&dev)
		return *this;

	_client = dev._client;
	_name = dev._name;
	return *this;
}

Device::~Device()
{
}

std::string Device::getName()const
{
	return _name;
}

const Client* Device::getClient()const
{
	return _client;
}

Client* Device::getClient()
{
	return _client;
}

bool Device::isOk()const
{
	return _client!=nullptr && !_name.empty();
}

Device::operator bool()const
{
	return isOk();
}

bool Device::operator!()const
{
	return !isOk();
}

bool Device::operator==(const Device& dev)const
{
	return dev._client==_client && dev._name==_name;
}

bool Device::operator<(const Device& dev)const
{
	return getName()<dev.getName();
}

std::string Device::getDescription()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceDescription(getName());
}

std::vector<std::string> Device::getVariableValue(const std::string& name)
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceVariableValue(getName(), name);
}

std::map<std::string,std::vector<std::string> > Device::getVariableValues()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceVariableValues(getName());
}

std::set<std::string> Device::getVariableNames()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceVariableNames(getName());
}

std::set<std::string> Device::getRWVariableNames()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceRWVariableNames(getName());
}

void Device::setVariable(const std::string& name, const std::string& value)
{
	if (!isOk()) throw NutException("Invalid device");
	getClient()->setDeviceVariable(getName(), name, value);
}

void Device::setVariable(const std::string& name, const std::vector<std::string>& values)
{
	if (!isOk()) throw NutException("Invalid device");
	getClient()->setDeviceVariable(getName(), name, values);
}



Variable Device::getVariable(const std::string& name)
{
	if (!isOk()) throw NutException("Invalid device");
	if(getClient()->hasDeviceVariable(getName(), name))
		return Variable(this, name);
	else
		return Variable(nullptr, "");
}

std::set<Variable> Device::getVariables()
{
	std::set<Variable> set;
	if (!isOk()) throw NutException("Invalid device");

	std::set<std::string> names = getClient()->getDeviceVariableNames(getName());
	for(std::set<std::string>::iterator it=names.begin(); it!=names.end(); ++it)
	{
		set.insert(Variable(this, *it));
	}

	return set;
}

std::set<Variable> Device::getRWVariables()
{
	std::set<Variable> set;
	if (!isOk()) throw NutException("Invalid device");

	std::set<std::string> names = getClient()->getDeviceRWVariableNames(getName());
	for(std::set<std::string>::iterator it=names.begin(); it!=names.end(); ++it)
	{
		set.insert(Variable(this, *it));
	}

	return set;
}

std::set<std::string> Device::getCommandNames()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->getDeviceCommandNames(getName());
}

std::set<Command> Device::getCommands()
{
	std::set<Command> cmds;

	std::set<std::string> res = getCommandNames();
	for(std::set<std::string>::iterator it=res.begin(); it!=res.end(); ++it)
	{
		cmds.insert(Command(this, *it));
	}

	return cmds;
}

Command Device::getCommand(const std::string& name)
{
	if (!isOk()) throw NutException("Invalid device");
	if(getClient()->hasDeviceCommand(getName(), name))
		return Command(this, name);
	else
		return Command(nullptr, "");
}

TrackingID Device::executeCommand(const std::string& name, const std::string& param)
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->executeDeviceCommand(getName(), name, param);
}

void Device::login()
{
	if (!isOk()) throw NutException("Invalid device");
	getClient()->deviceLogin(getName());
}

void Device::master()
{
	if (!isOk()) throw NutException("Invalid device");
	getClient()->deviceMaster(getName());
}

void Device::forcedShutdown()
{
}

int Device::getNumLogins()
{
	if (!isOk()) throw NutException("Invalid device");
	return getClient()->deviceGetNumLogins(getName());
}

/*
 *
 * Variable implementation
 *
 */

Variable::Variable(Device* dev, const std::string& name):
_device(dev),
_name(name)
{
}

Variable::Variable(const Variable& var):
_device(var._device),
_name(var._name)
{
}

Variable& Variable::operator=(const Variable& var)
{
	// Self assignment?
	if (this==&var)
		return *this;

	_device = var._device;
	_name = var._name;
	return *this;
}

Variable::~Variable()
{
}

std::string Variable::getName()const
{
	return _name;
}

const Device* Variable::getDevice()const
{
	return _device;
}

Device* Variable::getDevice()
{
	return _device;
}

bool Variable::isOk()const
{
	return _device!=nullptr && !_name.empty();

}

Variable::operator bool()const
{
	return isOk();
}

bool Variable::operator!()const
{
	return !isOk();
}

bool Variable::operator==(const Variable& var)const
{
	return var._device==_device && var._name==_name;
}

bool Variable::operator<(const Variable& var)const
{
	return getName()<var.getName();
}

std::vector<std::string> Variable::getValue()
{
	return getDevice()->getClient()->getDeviceVariableValue(getDevice()->getName(), getName());
}

std::string Variable::getDescription()
{
	return getDevice()->getClient()->getDeviceVariableDescription(getDevice()->getName(), getName());
}

void Variable::setValue(const std::string& value)
{
	getDevice()->setVariable(getName(), value);
}

void Variable::setValues(const std::vector<std::string>& values)
{
	getDevice()->setVariable(getName(), values);
}


/*
 *
 * Command implementation
 *
 */

Command::Command(Device* dev, const std::string& name):
_device(dev),
_name(name)
{
}

Command::Command(const Command& cmd):
_device(cmd._device),
_name(cmd._name)
{
}

Command& Command::operator=(const Command& cmd)
{
	// Self assignment?
	if (this==&cmd)
		return *this;

	_device = cmd._device;
	_name = cmd._name;
	return *this;
}

Command::~Command()
{
}

std::string Command::getName()const
{
	return _name;
}

const Device* Command::getDevice()const
{
	return _device;
}

Device* Command::getDevice()
{
	return _device;
}

bool Command::isOk()const
{
	return _device!=nullptr && !_name.empty();
}

Command::operator bool()const
{
	return isOk();
}

bool Command::operator!()const
{
	return !isOk();
}

bool Command::operator==(const Command& cmd)const
{
	return cmd._device==_device && cmd._name==_name;
}

bool Command::operator<(const Command& cmd)const
{
	return getName()<cmd.getName();
}

std::string Command::getDescription()
{
	return getDevice()->getClient()->getDeviceCommandDescription(getDevice()->getName(), getName());
}

void Command::execute(const std::string& param)
{
	getDevice()->executeCommand(getName(), param);
}

} /* namespace nut */


/**
 * C nutclient API.
 */
extern "C" {


strarr strarr_alloc(size_t count)
{
	strarr arr = static_cast<strarr>(xcalloc(count+1, sizeof(char*)));
	arr[count] = nullptr;
	return arr;
}

void strarr_free(strarr arr)
{
	char** pstr = arr;
	while(*pstr!=nullptr)
	{
		free(*pstr);
		++pstr;
	}
	free(arr);
}

strarr stringset_to_strarr(const std::set<std::string>& strset)
{
	strarr arr = strarr_alloc(strset.size());
	strarr pstr = arr;
	for(std::set<std::string>::const_iterator it=strset.begin(); it!=strset.end(); ++it)
	{
		*pstr = xstrdup(it->c_str());
		pstr++;
	}
	return arr;
}

strarr stringvector_to_strarr(const std::vector<std::string>& strset)
{
	strarr arr = strarr_alloc(strset.size());
	strarr pstr = arr;
	for(std::vector<std::string>::const_iterator it=strset.begin(); it!=strset.end(); ++it)
	{
		*pstr = xstrdup(it->c_str());
		pstr++;
	}
	return arr;
}

NUTCLIENT_TCP_t nutclient_tcp_create_client(const char* host, unsigned short port)
{
	nut::TcpClient* client = new nut::TcpClient;
	try
	{
		client->connect(host, port);
		return static_cast<NUTCLIENT_TCP_t>(client);
	}
	catch(nut::NutException& ex)
	{
		// TODO really catch it
		delete client;
		return nullptr;
	}

}

void nutclient_destroy(NUTCLIENT_t client)
{
	if(client)
	{
		delete static_cast<nut::Client*>(client);
	}
}

int nutclient_tcp_is_connected(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->isConnected() ? 1 : 0;
		}
	}
	return 0;
}

void nutclient_tcp_disconnect(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->disconnect();
		}
	}
}

int nutclient_tcp_reconnect(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			try
			{
				cl->connect();
				return 0;
			}
			catch(...){}
		}
	}
	return -1;
}

void nutclient_tcp_set_timeout(NUTCLIENT_TCP_t client, long timeout)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			cl->setTimeout(timeout);
		}
	}
}

long nutclient_tcp_get_timeout(NUTCLIENT_TCP_t client)
{
	if(client)
	{
		nut::TcpClient* cl = dynamic_cast<nut::TcpClient*>(static_cast<nut::Client*>(client));
		if(cl)
		{
			return cl->getTimeout();
		}
	}
	return -1;
}

void nutclient_authenticate(NUTCLIENT_t client, const char* login, const char* passwd)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->authenticate(login, passwd);
			}
			catch(...){}
		}
	}
}

void nutclient_logout(NUTCLIENT_t client)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->logout();
			}
			catch(...){}
		}
	}
}

void nutclient_device_login(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->deviceLogin(dev);
			}
			catch(...){}
		}
	}
}

int nutclient_get_device_num_logins(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->deviceGetNumLogins(dev);
			}
			catch(...){}
		}
	}
	return -1;
}

void nutclient_device_master(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->deviceMaster(dev);
			}
			catch(...){}
		}
	}
}

void nutclient_device_forced_shutdown(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->deviceForcedShutdown(dev);
			}
			catch(...){}
		}
	}
}

strarr nutclient_get_devices(NUTCLIENT_t client)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceNames());
			}
			catch(...){}
		}
	}
	return nullptr;
}

int nutclient_has_device(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->hasDevice(dev)?1:0;
			}
			catch(...){}
		}
	}
	return 0;
}

char* nutclient_get_device_description(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return xstrdup(cl->getDeviceDescription(dev).c_str());
			}
			catch(...){}
		}
	}
	return nullptr;
}

strarr nutclient_get_device_variables(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceVariableNames(dev));
			}
			catch(...){}
		}
	}
	return nullptr;
}

strarr nutclient_get_device_rw_variables(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceRWVariableNames(dev));
			}
			catch(...){}
		}
	}
	return nullptr;
}

int nutclient_has_device_variable(NUTCLIENT_t client, const char* dev, const char* var)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->hasDeviceVariable(dev, var)?1:0;
			}
			catch(...){}
		}
	}
	return 0;
}

char* nutclient_get_device_variable_description(NUTCLIENT_t client, const char* dev, const char* var)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return xstrdup(cl->getDeviceVariableDescription(dev, var).c_str());
			}
			catch(...){}
		}
	}
	return nullptr;
}

strarr nutclient_get_device_variable_values(NUTCLIENT_t client, const char* dev, const char* var)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringvector_to_strarr(cl->getDeviceVariableValue(dev, var));
			}
			catch(...){}
		}
	}
	return nullptr;
}

void nutclient_set_device_variable_value(NUTCLIENT_t client, const char* dev, const char* var, const char* value)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->setDeviceVariable(dev, var, value);
			}
			catch(...){}
		}
	}
}

void nutclient_set_device_variable_values(NUTCLIENT_t client, const char* dev, const char* var, const strarr values)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				std::vector<std::string> vals;
				strarr pstr = static_cast<strarr>(values);
				while(*pstr)
				{
					vals.push_back(std::string(*pstr));
					++pstr;
				}

				cl->setDeviceVariable(dev, var, vals);
			}
			catch(...){}
		}
	}
}

strarr nutclient_get_device_commands(NUTCLIENT_t client, const char* dev)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return stringset_to_strarr(cl->getDeviceCommandNames(dev));
			}
			catch(...){}
		}
	}
	return nullptr;
}

int nutclient_has_device_command(NUTCLIENT_t client, const char* dev, const char* cmd)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return cl->hasDeviceCommand(dev, cmd)?1:0;
			}
			catch(...){}
		}
	}
	return 0;
}

char* nutclient_get_device_command_description(NUTCLIENT_t client, const char* dev, const char* cmd)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				return xstrdup(cl->getDeviceCommandDescription(dev, cmd).c_str());
			}
			catch(...){}
		}
	}
	return nullptr;
}

void nutclient_execute_device_command(NUTCLIENT_t client, const char* dev, const char* cmd, const char* param)
{
	if(client)
	{
		nut::Client* cl = static_cast<nut::Client*>(client);
		if(cl)
		{
			try
			{
				cl->executeDeviceCommand(dev, cmd, param);
			}
			catch(...){}
		}
	}
}

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_CXX98_COMPAT
#pragma GCC diagnostic pop
#endif

} /* extern "C" */
