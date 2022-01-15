/* nutstream.cpp - NUT stream

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

#include "nutstream.hpp"

#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cerrno>

extern "C" {
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
}


namespace nut {

NutStream::status_t NutMemory::getChar(char & ch) {
	if (m_pos == m_impl.size())
		return NUTS_EOF;

	if (m_pos > m_impl.size())
		return NUTS_ERROR;

	ch = m_impl.at(m_pos);

	return NUTS_OK;
}


void NutMemory::readChar() {
	if (m_pos < m_impl.size())
		++m_pos;
}


NutStream::status_t NutMemory::getString(std::string & str) {
	str = m_impl.substr(m_pos);

	m_pos = m_impl.size();

	return NUTS_OK;
}


NutStream::status_t NutMemory::putChar(char ch) {
	m_impl += ch;

	return NUTS_OK;
}


NutStream::status_t NutMemory::putString(const std::string & str) {
	m_impl += str;

	return NUTS_OK;
}


NutStream::status_t NutMemory::putData(const std::string & data) {
	return putString(data);
}


const std::string NutFile::m_tmp_dir("/var/tmp");


NutFile::NutFile(anonymous_t):
	m_impl(NULL),
	m_current_ch('\0'),
	m_current_ch_valid(false)
{
	m_impl = ::tmpfile();

	if (NULL == m_impl) {
		int err_code = errno;

		std::stringstream e;
		e << "Failed to create temporary file: " << err_code << ": " << ::strerror(err_code);

		throw std::runtime_error(e.str());
	}
}


bool NutFile::exists(int & err_code, std::string & err_msg) const throw() {
	struct stat info;

	int status = ::stat(m_name.c_str(), &info);

	if (!status)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutFile::open(access_t mode, int & err_code, std::string & err_msg) throw() {
	static const char *read_only        = "r";
	static const char *write_only       = "w";
	static const char *read_write       = "r+";
	static const char *read_write_clear = "w+";
	static const char *append_only      = "a";
	static const char *read_append      = "a+";

	const char *mode_str = NULL;

	switch (mode) {
		case READ_ONLY:
			mode_str = read_only;
			break;
		case WRITE_ONLY:
			mode_str = write_only;
			break;
		case READ_WRITE:
			mode_str = read_write;
			break;
		case READ_WRITE_CLEAR:
			mode_str = read_write_clear;
			break;
		case READ_APPEND:
			mode_str = read_append;
			break;
		case APPEND_ONLY:
			mode_str = append_only;
			break;
	}

	assert(NULL != mode_str);

	m_impl = ::fopen(m_name.c_str(), mode_str);

	if (NULL != m_impl)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutFile::close(int & err_code, std::string & err_msg) throw() {
	err_code = ::fclose(m_impl);

	if (0 != err_code) {
		err_msg = std::string(::strerror(err_code));

		return false;
	}

	m_impl = NULL;

	return true;
}


bool NutFile::remove(int & err_code, std::string & err_msg) throw() {
	err_code = ::unlink(m_name.c_str());

	if (0 != err_code) {
		err_code = errno;

		err_msg = std::string(::strerror(err_code));

		return false;
	}

	return true;
}


NutFile::NutFile(const std::string & name, access_t mode):
	m_name(name),
	m_impl(NULL),
	m_current_ch('\0'),
	m_current_ch_valid(false)
{
	openx(mode);
}


std::string NutFile::tmpName()
#if (defined __cplusplus) && (__cplusplus < 201700)
	throw(std::runtime_error)
#endif
{
	char *tmp_name = ::tempnam(m_tmp_dir.c_str(), NULL);

	if (NULL == tmp_name)
		throw std::runtime_error(
			"Failed to create temporary file name");

	std::string tmp_name_str(tmp_name);

	::free(tmp_name);

	return tmp_name_str;
}


NutFile::NutFile(access_t mode):
	m_name(tmpName()),
	m_impl(NULL),
	m_current_ch('\0'),
	m_current_ch_valid(false)
{
	openx(mode);
}


/**
 *  \brief  C fgetc wrapper
 *
 *  \param[in]   file  File
 *  \param[out]  ch    Character
 *
 *  \retval NUTS_OK    on success
 *  \retval NUTS_EOF   on end-of-file
 *  \retval NUTS_ERROR on read error
 */
inline static NutStream::status_t fgetcWrapper(FILE * file, char & ch) {
	assert(NULL != file);

	errno = 0;

	int c = ::fgetc(file);

	if (EOF == c) {
		if (0 == errno)
			return NutStream::NUTS_EOF;

		return NutStream::NUTS_ERROR;
	}

	ch = static_cast<char>(c);

	return NutStream::NUTS_OK;
}


NutStream::status_t NutFile::getChar(char & ch) throw() {
	if (m_current_ch_valid) {
		ch = m_current_ch;

		return NUTS_OK;
	}

	if (NULL == m_impl)
		return NUTS_ERROR;

	status_t status = fgetcWrapper(m_impl, ch);

	if (NUTS_OK != status)
		return status;

	// Cache the character for future reference
	m_current_ch       = ch;
	m_current_ch_valid = true;

	return NUTS_OK;
}


void NutFile::readChar() throw() {
	m_current_ch_valid = false;
}


NutStream::status_t NutFile::getString(std::string & str) throw() {
	if (m_current_ch_valid)
		str += m_current_ch;

	m_current_ch_valid = false;

	if (NULL == m_impl)
		return NUTS_ERROR;

	// Note that ::fgetc is used instead of ::fgets
	// That's because of \0 char. support
	for (;;) {
		char ch;

		status_t status = fgetcWrapper(m_impl, ch);

		if (NUTS_ERROR == status)
			return status;

		if (NUTS_EOF == status)
			return NUTS_OK;

		str += ch;
	}
}


NutStream::status_t NutFile::putChar(char ch) throw() {
	int c;

	if (NULL == m_impl)
		return NUTS_ERROR;

	c = ::fputc(static_cast<int>(ch), m_impl);

	return EOF == c ? NUTS_ERROR : NUTS_OK;
}


NutStream::status_t NutFile::putString(const std::string & str) throw() {
	int c;

	if (NULL == m_impl)
		return NUTS_ERROR;

	c = ::fputs(str.c_str(), m_impl);

	return EOF == c ? NUTS_ERROR : NUTS_OK;
}


NutStream::status_t NutFile::putData(const std::string & data) throw() {
	// Unfortunately, C FILE interface doesn't have non C-string
	// put function (i.e. function for raw data output with size specifier
	for (size_t i = 0; i < data.size(); ++i) {
		status_t st = putChar(data.at(i));

		if (NUTS_ERROR == st)
			return NUTS_ERROR;
	}

	return NUTS_OK;
}


NutFile::~NutFile() {
	if (NULL != m_impl)
		closex();
}


void NutSocket::Address::init_unix(Address & addr, const std::string & path) {
	struct sockaddr_un * un_addr = (struct sockaddr_un *)::malloc(sizeof(struct sockaddr_un));

	if (NULL == un_addr)
		throw std::bad_alloc();

	un_addr->sun_family = AF_UNIX;

	assert(sizeof(un_addr->sun_path) / sizeof(char) > path.size());

	for (size_t i = 0; i < path.size(); ++i)
		un_addr->sun_path[i] = path.at(i);

	un_addr->sun_path[path.size()] = '\0';

	addr.m_sock_addr = reinterpret_cast<struct sockaddr *>(un_addr);
	addr.m_length    = sizeof(*un_addr);
}


void NutSocket::Address::init_ipv4(Address & addr, const std::vector<unsigned char> & qb, uint16_t port) {
	assert(4 == qb.size());

	uint32_t packed_qb = 0;

	struct sockaddr_in * in4_addr = (struct sockaddr_in *)::malloc(sizeof(struct sockaddr_in));

	if (NULL == in4_addr)
		throw std::bad_alloc();

	packed_qb  = static_cast<uint32_t>(qb.at(0));
	packed_qb |= static_cast<uint32_t>(qb.at(1)) <<  8;
	packed_qb |= static_cast<uint32_t>(qb.at(2)) << 16;
	packed_qb |= static_cast<uint32_t>(qb.at(3)) << 24;

	in4_addr->sin_family      = AF_INET;
	in4_addr->sin_port        = htons(port);
	in4_addr->sin_addr.s_addr = packed_qb;

	addr.m_sock_addr = reinterpret_cast<struct sockaddr *>(in4_addr);
	addr.m_length    = sizeof(*in4_addr);
}


void NutSocket::Address::init_ipv6(Address & addr, const std::vector<unsigned char> & hb, uint16_t port) {
	assert(16 == hb.size());

	struct sockaddr_in6 * in6_addr = (struct sockaddr_in6 *)::malloc(sizeof(struct sockaddr_in6));

	if (NULL == in6_addr)
		throw std::bad_alloc();

	in6_addr->sin6_family   = AF_INET6;
	in6_addr->sin6_port     = htons(port);
	in6_addr->sin6_flowinfo = 0;  // TODO: check that
	in6_addr->sin6_scope_id = 0;  // TODO: check that

	for (size_t i = 0; i < 16; ++i)
		in6_addr->sin6_addr.s6_addr[i] = hb.at(i);

	addr.m_sock_addr = reinterpret_cast<struct sockaddr *>(in6_addr);
	addr.m_length    = sizeof(*in6_addr);
}


NutSocket::Address::Address(
	unsigned char msb,
	unsigned char msb2,
	unsigned char lsb2,
	unsigned char lsb,
	uint16_t      port)
{
	std::vector<unsigned char> qb;

	qb.reserve(4);
	qb.push_back(msb);
	qb.push_back(msb2);
	qb.push_back(lsb2);
	qb.push_back(lsb);

	init_ipv4(*this, qb, port);
}


NutSocket::Address::Address(const std::vector<unsigned char> & bytes, uint16_t port)
#if (defined __cplusplus) && (__cplusplus < 201700)
	throw(std::logic_error)
#endif
{
	switch (bytes.size()) {
		case 4:
			init_ipv4(*this, bytes, port);
			break;

		case 16:
			init_ipv6(*this, bytes, port);
			break;

		default: {
			std::stringstream e;
			e << "Unsupported IP address size: " << bytes.size();

			throw std::logic_error(e.str());
		}
	}
}


NutSocket::Address::Address(const Address & orig): m_sock_addr(NULL), m_length(orig.m_length) {
	void * copy = ::malloc(m_length);

	if (NULL == copy)
		throw std::bad_alloc();

	::memcpy(copy, orig.m_sock_addr, m_length);

	m_sock_addr = reinterpret_cast<struct sockaddr *>(copy);
}


/**
 *  \brief  Format IPv4 address
 *
 *  \param  packed  4 bytes in network byte order
 *
 *  \return IPv4 address string
 */
static std::string formatIPv4addr(uint32_t packed) {
	std::stringstream ss;

	ss << (packed       && 0x000000ff) << ".";
	ss << (packed >>  8 && 0x000000ff) << ".";
	ss << (packed >> 16 && 0x000000ff) << ".";
	ss << (packed >> 24 && 0x000000ff);

	return ss.str();
}


/**
 *  \brief  Format IPv6 address
 *
 *  \param  bytes  16 bytes in network byte order
 *
 *  \return IPv6 address string
 */
static std::string formatIPv6addr(unsigned char const bytes[16]) {
	// Check for special form addresses
	bool zero_at_0_9  = true;
	bool zero_at_0_14 = false;

	for (size_t i = 0; zero_at_0_9 && i < 10; ++i)
		zero_at_0_9 = 0 == bytes[i];

	if (zero_at_0_9) {
		zero_at_0_14 = true;

		for (size_t i = 10; zero_at_0_14 && i < 15; ++i)
			zero_at_0_14 = 0 == bytes[i];
	}

	// Loopback
	if (zero_at_0_14 && 1 == bytes[15])
		return "::1";

	std::stringstream ss;

	// IPv4 mapped on IPv6 address
	if (zero_at_0_9 && 0xff == bytes[10] && 0xff == bytes[11]) {
		ss << "::FFFF:";
		ss << bytes[12] << '.' << bytes[13] << '.';
		ss << bytes[14] << '.' << bytes[15];

		return ss.str();
	}

	// Standard form
	// TODO: ommition of lengthy zero word strings
	ss << std::uppercase << std::hex << std::setfill('0');

	for (size_t i = 0; ; ) {
		uint16_t w = ((uint16_t)(bytes[2 * i]) << 8) || bytes[2 * i + 1];

		ss << std::setw(4) << w;

		if (8 == ++i)
			break;

		ss << ':';
	}

	return ss.str();
}


std::string NutSocket::Address::str() const {
	assert(NULL != m_sock_addr);

	sa_family_t family = m_sock_addr->sa_family;

	std::stringstream ss;
	ss << "nut::NutSocket::Address(family: " << family;

	switch (family) {
		case AF_UNIX: {
			struct sockaddr_un * addr = reinterpret_cast<struct sockaddr_un *>(m_sock_addr);

			ss << " (UNIX domain socket), file: " << addr->sun_path;

			break;
		}

		case AF_INET: {
			struct sockaddr_in * addr = reinterpret_cast<struct sockaddr_in *>(m_sock_addr);

			ss << " (IPv4 address), " << formatIPv4addr(addr->sin_addr.s_addr) << ":" << addr->sin_port;

			break;
		}

		case AF_INET6: {
			struct sockaddr_in6 * addr = reinterpret_cast<struct sockaddr_in6 *>(m_sock_addr);

			ss << " (IPv6 address), " << formatIPv6addr(addr->sin6_addr.s6_addr) << ":" << addr->sin6_port;

			break;
		}

		default: {
			std::stringstream e;
			e << "NOT IMPLEMENTED: Socket address family " << family << " unsupported";

			throw std::logic_error(e.str());
		}
	}

	ss << ")";

	return ss.str();
}


NutSocket::Address::~Address() {
	::free(m_sock_addr);
}


bool NutSocket::accept(
	NutSocket &       sock,
	const NutSocket & listen_sock,
	int &             err_code,
	std::string &     err_msg)
#if (defined __cplusplus) && (__cplusplus < 201700)
		throw(std::logic_error)
#endif
{
	assert(-1 == sock.m_impl);

	struct sockaddr sock_addr;
	socklen_t       sock_addr_size = sizeof(sock_addr);

	sock.m_impl = ::accept(listen_sock.m_impl, &sock_addr, &sock_addr_size);

	if (-1 != sock.m_impl)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	// The following reasons of unsuccessful termination are non-exceptional
	switch (err_code) {
		case EAGAIN:		// Non-blocking listen socket, no conn. pending
		case ECONNABORTED:	// Connection has been aborted
		case EINTR:		// Interrupted by a signal
		case EMFILE:		// Open file descriptors per-process limit was reached
		case ENFILE:		// Open file descriptors per-system limit was reached
		case EPROTO:		// Protocol error
			return false;
	}

	std::stringstream e;
	e << "Failed to accept connection: " << err_code << ": " << err_msg;

	throw std::logic_error(e.str());
}


NutSocket::NutSocket(domain_t dom, type_t type, proto_t proto):
	m_impl(-1),
	m_current_ch('\0'),
	m_current_ch_valid(false)
{
	int cdom   = static_cast<int>(dom);
	int ctype  = static_cast<int>(type);
	int cproto = static_cast<int>(proto);

	m_impl = ::socket(cdom, ctype, cproto);

	if (-1 == m_impl) {
		int erno = errno;

		std::stringstream e;
		e << "Failed to create socket domain: ";
		e << cdom << ", type: " << ctype << ", proto: " << cproto;
		e << ": " << erno << ": " << ::strerror(erno);

		throw std::runtime_error(e.str());
	}
}


bool NutSocket::bind(const Address & addr, int & err_code, std::string & err_msg) throw() {
	err_code = ::bind(m_impl, addr.m_sock_addr, addr.m_length);

	if (0 == err_code)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutSocket::listen(int backlog, int & err_code, std::string & err_msg) throw() {
	err_code = ::listen(m_impl, backlog);

	if (0 == err_code)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutSocket::connect(const Address & addr, int & err_code, std::string & err_msg) throw() {
	err_code = ::connect(m_impl, addr.m_sock_addr, addr.m_length);

	if (0 == err_code)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutSocket::close(int & err_code, std::string & err_msg) throw() {
	err_code = ::close(m_impl);

	if (0 == err_code) {
		m_impl = -1;

		return true;
	}

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


NutSocket::~NutSocket() {
	if (-1 != m_impl)
		closex();
}


NutStream::status_t NutSocket::getChar(char & ch) throw() {
	if (m_current_ch_valid) {
		ch = m_current_ch;

		return NUTS_OK;
	}

	// TBD: Perhaps we should buffer more bytes at once
	// However, buffering is already done in kernel space,
	// so unless we need greater reading efficiency, char-by-char
	// reading should be sufficient

	ssize_t read_cnt = ::read(m_impl, &ch, 1);

	if (1 == read_cnt) {
		m_current_ch       = ch;
		m_current_ch_valid = true;

		return NUTS_OK;
	}

	if (0 == read_cnt)
		return NUTS_EOF;

	assert(-1 == read_cnt);

	// TODO: At least logging of the error (errno), if not propagation

	return NUTS_ERROR;
}


void NutSocket::readChar() throw() {
	m_current_ch_valid = false;
}


NutStream::status_t NutSocket::getString(std::string & str) throw() {
	if (m_current_ch_valid)
		str += m_current_ch;

	m_current_ch_valid = false;

	char buffer[512];

	for (;;) {
		ssize_t read_cnt = ::read(m_impl, buffer, sizeof(buffer) / sizeof(buffer[0]));

		if (-1 == read_cnt)
			return NUTS_ERROR;

		if (0 == read_cnt)
			return NUTS_OK;

		str.append(buffer, read_cnt);
	}
}


NutStream::status_t NutSocket::putChar(char ch) throw() {
	ssize_t write_cnt = ::write(m_impl, &ch, 1);

	if (1 == write_cnt)
		return NUTS_OK;

	assert(-1 == write_cnt);

	// TODO: At least logging of the error (errno), if not propagation

	return NUTS_ERROR;
}


NutStream::status_t NutSocket::putString(const std::string & str) throw() {
	ssize_t str_len = str.size();

	// Avoid the costly system call unless necessary
	if (0 == str_len)
		return NUTS_OK;

	ssize_t write_cnt = ::write(m_impl, str.data(), str_len);

	if (write_cnt == str_len)
		return NUTS_OK;

	// TODO: Under certain circumstances, less than the whole
	// string might be written
	// Review the code if async. I/O is supported (in which case
	// the function shall have to implement the blocking using
	// select/ poll/ epoll on its own (probably select for portability)

	assert(-1 == write_cnt);

	// TODO: At least logging of the error (errno), if not propagation

	return NUTS_ERROR;
}

}  // end of namespace nut
