/* nutstream.h - NUT stream

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

#ifndef nut_nutstream_h
#define nut_nutstream_h

#ifdef __cplusplus

#include <stdexcept>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdio>

extern "C" {
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
}


namespace nut
{

/**
 *  \brief  Data stream interface
 *
 *  The intarface provides character-based streamed I/O.
 */
class NutStream {
	public:

	/** Data store status */
	typedef enum {
		NUTS_OK = 0,	/** Operation successful  */
		NUTS_EOF,	/** End of stream reached */
		NUTS_ERROR,	/** Error ocurred         */
	} status_t;

	protected:

	/** Formal constructor */
	NutStream() {}

	public:

	/**
	 *  \brief  Get one character from current position
	 *
	 *  The method provides character from current position
	 *  in the data store.
	 *  The position is not shifted (see \ref readChar for
	 *  current character consumption).
	 *
	 *  The operation is synchronous (the call blocks if necessary).
	 *
	 *  \param[out]  ch  Character
	 *
	 *  \retval NUTS_OK    on success,
	 *  \retval NUTS_EOF   on end of stream,
	 *  \retval NUTS_ERROR on read error
	 */
	virtual status_t getChar(char & ch) = 0;

	/**
	 *  \brief  Consume current character
	 *
	 *  The method shifts position in the stream.
	 *  It shall never block (if the position gets
	 *  past-the-end of currently available data,
	 *  subsequent call to \ref getChar will block).
	 */
	virtual void readChar() = 0;

	/**
	 *  \brief  Read characters from the stream till EoF
	 *
	 *  The method may be used to synchronously read
	 *  whole (rest of) stream.
	 *  Note that implementation may block flow.
	 *
	 *  \retval NUTS_OK    on success,
	 *  \retval NUTS_ERROR on read error
	 */
	virtual status_t getString(std::string & str) = 0;

	/**
	 *  \brief  Put one character to the stream end
	 *
	 *  \param[in]  ch  Character
	 *
	 *  \retval NUTS_OK    on success,
	 *  \retval NUTS_ERROR on write error
	 */
	virtual status_t putChar(char ch) = 0;

	/**
	 *  \brief  Put string to the stream end
	 *
	 *  \param[in]  str  String
	 *
	 *  \retval NUTS_OK    on success,
	 *  \retval NUTS_ERROR on write error
	 */
	virtual status_t putString(const std::string & str) = 0;

	/**
	 *  \brief  Put data to the stream end
	 *
	 *  The difference between \ref putString and this method
	 *  is that it is able to serialise also data containing
	 *  null characters.
	 *
	 *  \param[in]  data  Data
	 *
	 *  \retval NUTS_OK    on success,
	 *  \retval NUTS_ERROR on write error
	 */
	virtual status_t putData(const std::string & data) = 0;

	/** Formal destructor */
	virtual ~NutStream() {}

};  // end of class NutStream


/** Memory stream */
class NutMemory: public NutStream {
	private:

	/** Implementation */
	std::string m_impl;

	/** Position in implementation */
	size_t m_pos;

	public:

	/** Constructor */
	NutMemory(): m_pos(0) {}

	/**
	 *  \brief  Constructor
	 *
	 *  \param  str  Init value
	 */
	NutMemory(const std::string & str): m_impl(str), m_pos(0) {}

	// NutStream interface implementation
	status_t getChar(char & ch);
	void     readChar();
	status_t getString(std::string & str);
	status_t putChar(char ch);
	status_t putString(const std::string & str);
	status_t putData(const std::string & data);

};  // end of class NutMemory


/** File stream */
class NutFile: public NutStream {
	public:

	/** Access mode */
	typedef enum {
		/** Read-only, initial pos. is at the beginning */
		READ_ONLY = 0,

		/** Write-only, with creation, clears the file if exists */
		WRITE_ONLY,

		/** Read-write, initial pos. is at the beginning */
		READ_WRITE,

		/** As previous, but with creation, clears the file if exists */
		READ_WRITE_CLEAR,

		/** Read & write to end, with creation, init. pos: beginning/end for r/w */
		READ_APPEND,

		/** Write only, with creation, initial position is at the end */
		APPEND_ONLY,
	} access_t;

	/** Unnamed temp. file constructor flag */
	typedef enum {
		ANONYMOUS,  /** Anonymous temp. file flag */
	} anonymous_t;

	private:

	/** Temporary files directory */
	static const std::string m_tmp_dir;

	/** File name */
	const std::string m_name;

	/** Implementation */
	FILE *m_impl;

	/** Current character cache */
	char m_current_ch;

	/** Current character cache status */
	bool m_current_ch_valid;

	/**
	 *  \brief  Generate temporary file name
	 *
	 *  Throws an exception on file name generation error.
	 *
	 *  \return Temporary file name
	 */
	std::string tmpName() throw(std::runtime_error);

	public:

	/** Constructor
	 *
	 *  \param[in]  name  File name
	 */
	NutFile(const std::string & name):
		m_name(name),
		m_impl(NULL),
		m_current_ch('\0'),
		m_current_ch_valid(false) {}

	/**
	 *  \brief  Temporary file constructor (with open)
	 *
	 *  Anonymous temp. file (with automatic destruction) will be created.
	 */
	NutFile(anonymous_t);

	/**
	 *  \brief  File name getter
	 *
	 *  \return File name
	 */
	inline const std::string & name() const {
		return m_name;
	}

	/**
	 *  \brief  Open file
	 *
	 *  \param[in]   mode      File open mode
	 *  \param[out]  err_code  Error code
	 *  \param[out]  err-msg   Error message
	 *
	 *  \retval true  if open succeeded
	 *  \retval false if open failed
	 */
	bool open(access_t mode, int & err_code, std::string & err_msg) throw();

	/**
	 *  \brief  Open file
	 *
	 *  \param[in]  mode  File open mode (read-only by default)
	 *
	 *  \retval true  if open succeeded
	 *  \retval false if open failed
	 */
	inline bool open(access_t mode = READ_ONLY) {
		int ec;
		std::string em;

		return open(mode, ec, em);
	}

	/**
	 *  \brief  Open file (or throw exception)
	 *
	 *  \param[in]  mode  File open mode (read-only by default)
	 *
	 *  \retval true  if open succeeded
	 *  \retval false if open failed
	 */
	inline void openx(access_t mode = READ_ONLY) throw(std::runtime_error) {
		int ec;
		std::string em;

		if (open(mode, ec, em))
			return;

		std::stringstream e;
		e << "Failed to open file " << m_name << ": " << ec << ": " << em;

		throw std::runtime_error(e.str());
	}

	/**
	 *  \brief  Close file
	 *
	 *  \param[out]  err_code  Error code
	 *  \param[out]  err-msg   Error message
	 *
	 *  \retval true  if close succeeded
	 *  \retval false if close failed
	 */
	bool close(int & err_code, std::string & err_msg) throw();

	/**
	 *  \brief  Close file
	 *
	 *  \retval true  if close succeeded
	 *  \retval false if close failed
	 */
	inline bool close() throw() {
		int ec;
		std::string em;

		return close(ec, em);
	}

	/** Close file (or throw exception) */
	inline void closex() throw(std::runtime_error) {
		int ec;
		std::string em;

		if (close(ec, em))
			return;

		std::stringstream e;
		e << "Failed to close file " << m_name << ": " << ec << ": " << em;

		throw std::runtime_error(e.str());
	}

	/**
	 *  \brief  Constructor (with open)
	 *
	 *  Opens the file, throws exception on open error.
	 *
	 *  \param[in]  name  File name
	 *  \param[in]  mode  File open mode
	 */
	NutFile(const std::string & name, access_t mode);

	/**
	 *  \brief  Temporary file constructor (with open)
	 *
	 *  Opens file in \ref m_tmp_dir.
	 *  Throws exception on open error.
	 *  Note that for temporary files, non-creation modes
	 *  don't make sense (and will result in throwing an exception).
	 *
	 *  \param[in]  name  File name
	 *  \param[in]  mode  File open mode (r/w by default)
	 */
	NutFile(access_t mode = READ_WRITE_CLEAR);

	// NutStream interface implementation
	status_t getChar(char & ch)			throw();
	void     readChar()				throw();
	status_t getString(std::string & str)		throw();
	status_t putChar(char ch)			throw();
	status_t putString(const std::string & str)	throw();
	status_t putData(const std::string & data)	throw();

	/** Destructor (closes the file) */
	~NutFile();

	private:

	/**
	 *  \brief  Copy constructor
	 *
	 *  TODO: Copying is forbidden (for now).
	 *  If required, it may be enabled, using fdup.
	 *
	 *  \param  orig  Original file
	 */
	NutFile(const NutFile & orig) {
		throw std::logic_error("NOT IMPLEMENTED");
	}

	/**
	 *  \brief  Assignment
	 *
	 *  TODO: Assignment is forbidden (for now).
	 *  See copy constructor for implementation notes
	 *  (and don't forget to destroy left value, properly).
	 *
	 *  \param  rval  Right value
	 */
	NutFile & operator = (const NutFile & rval) {
		throw std::logic_error("NOT IMPLEMENTED");
	}

};  // end of class NutFile


/** Socket stream */
class NutSocket: public NutStream {
	public:

	/** Socket domain */
	typedef enum {
		NUTSOCKD_UNIX   = AF_UNIX,	/** Unix */
		NUTSOCKD_INETv4 = AF_INET,	/** IPv4 */
		NUTSOCKD_INETv6 = AF_INET6,	/** IPv6 */
	} domain_t;

	/** Socket type */
	typedef enum {
		NUTSOCKT_STREAM = SOCK_STREAM,	/** Stream   */
		NUTSOCKT_DGRAM  = SOCK_DGRAM,	/** Datagram */
	} type_t;

	/** Socket protocol */
	typedef enum {
		NUTSOCKP_IMPLICIT = 0,	/** Implicit protocol for chosen type */
	} proto_t;

	/** Socket address */
	class Address {
		friend class NutSocket;

		private:

		/** Implementation */
		struct sockaddr *m_sock_addr;

		/** Length */
		socklen_t m_length;

		/**
		 *  \brief  Invalid address constructor
		 *
		 *  Invalid address may be produced e.g. by failed DNS resolving.
		 */
		Address(): m_sock_addr(NULL), m_length(0) {}

		/**
		 *  \brief  Initialise UNIX socket address
		 *
		 *  \param  addr  UNIX socket address
		 *  \param  path  Pathname
		 */
		static void init_unix(Address & addr, const std::string & path);

		/**
		 *  \brief  Initialise IPv4 address
		 *
		 *  \param  addr  IPv4 address
		 *  \param  qb    Byte quadruplet (MSB is at index 0)
		 *  \param  port  Port number
		 */
		static void init_ipv4(Address & addr, const std::vector<unsigned char> & qb, uint16_t port);

		/**
		 *  \brief  Initialise IPv6 address
		 *
		 *  \param  addr  IPv6 address
		 *  \param  hb    16 bytes of the address (MSB is at index 0)
		 *  \param  port  Port number
		 */
		static void init_ipv6(Address & addr, const std::vector<unsigned char> & hb, uint16_t port);

		public:

		/**
		 *  \brief  Check address validity
		 *
		 *  \retval true  if the address is valid
		 *  \retval false otherwise
		 */
		inline bool valid() throw() {
			return NULL != m_sock_addr;
		}

		/**
		 *  \brief  UNIX socket address constructor
		 *
		 *  \param  path  Pathname
		 */
		Address(const std::string & path) {
			init_unix(*this, path);
		}

		/**
		 *  \brief  IPv4 address constructor
		 *
		 *  \param  msb   Most significant byte
		 *  \param  msb2  2nd most significant byte
		 *  \param  lsb2  2nd least significant byte
		 *  \param  lsb   Least significant byte
		 *  \param  port  Port number
		 */
		Address(unsigned char msb,
			unsigned char msb2,
			unsigned char lsb2,
			unsigned char lsb,
			uint16_t      port);

		/**
		 *  \brief  IP address constructor
		 *
		 *  Creates either IPv4 or IPv6 address (depending on
		 *  how many bytes are provided bia the \c bytes argument).
		 *  Throws an exception if the bytecount is invalid.
		 *
		 *  \param  bytes 4 or 16 address bytes (MSB is at index 0)
		 *  \param  port  Port number
		 */
		Address(const std::vector<unsigned char> & bytes, uint16_t port) throw(std::logic_error);

		/**
		 *  \brief  Copy constructor
		 *
		 *  \param  orig  Original address
		 */
		Address(const Address & orig);

		/**
		 *  \brief  Stringifisation
		 *
		 *  \return String representation of the address
		 */
		std::string str() const;

		/** Stringifisation */
		inline operator std::string() const {
			return str();
		}

		/** Destructor */
		~Address();

	};  // end of class Address

	/** Flag for socket constructor of accepted connection */
	typedef enum {
		ACCEPT  /** Accept flag */
	} accept_flag_t;

	private:

	/** Socket implementation */
	int m_impl;

	/** Current character cache */
	char m_current_ch;

	/** Current character cache status */
	bool m_current_ch_valid;

	/**
	 *  \brief  Accept client connection on a listen socket
	 *
	 *  \param[out]  sock         Socket
	 *  \param[in]   listen_sock  Listen socket
	 *  \param[out]  err_code     Error code
	 *  \param[out]  err_msg      Error message
	 *
	 *  \retval true  on success
	 *  \retval false otherwise
	 */
	static bool accept(
		NutSocket &       sock,
		const NutSocket & listen_sock,
		int &             err_code,
		std::string &     err_msg) throw(std::logic_error);

	/**
	 *  \brief  Accept client connection on a listen socket
	 *
	 *  \param[out]  sock         Socket
	 *  \param[in]   listen_sock  Listen socket
	 *
	 *  \retval true  on success
	 *  \retval false otherwise
	 */
	inline static bool accept(
		NutSocket &       sock,
		const NutSocket & listen_sock) throw(std::logic_error)
	{
		int ec;
		std::string em;

		return accept(sock, listen_sock, ec, em);
	}

	/**
	 *  \brief  Accept client connection (or throw exception)
	 *
	 *  \param[out]  sock         Socket
	 *  \param[in]   listen_sock  Listen socket
	 */
	inline static void acceptx(
		NutSocket &       sock,
		const NutSocket & listen_sock) throw(std::logic_error, std::runtime_error)
	{
		int ec;
		std::string em;

		if (accept(sock, listen_sock, ec, em))
			return;

		std::stringstream e;
		e << "Failed to accept connection: " << ec << ": " << em;

		throw std::runtime_error(e.str());
	}

	public:

	/**
	 *  \brief  Socket valid check
	 *
	 *  \retval true  if the socket is initialised
	 *  \retval false otherwise
	 */
	inline bool valid() throw() {
		return -1 != m_impl;
	}

	/**
	 *  \brief  Constructor
	 *
	 *  \param  dom    Socket domain
	 *  \param  type   Socket type
	 *  \param  proto  Socket protocol
	 */
	NutSocket(
		domain_t dom   = NUTSOCKD_INETv4,
		type_t   type  = NUTSOCKT_STREAM,
		proto_t  proto = NUTSOCKP_IMPLICIT);

	/**
	 *  \brief  Accepted client socket constructor
	 *
	 *  Accepts connection on a listen socket.
	 *  If the argument isn't a listen socket, exception is thrown.
	 *  The call will block until either there's an incoming connection
	 *  or the listening socket is closed or there's an error.
	 *
	 *  \param  listen_sock  Listening socket
	 */
	NutSocket(accept_flag_t, const NutSocket & listen_sock, int & err_code, std::string & err_msg):
		m_impl(-1),
		m_current_ch('\0'),
		m_current_ch_valid(false)
	{
		accept(*this, listen_sock, err_code, err_msg);
	}

	/**
	 *  \brief  Accepted client socket constructor
	 *
	 *  Accepts connection on a listen socket.
	 *
	 *  \param  listen_sock  Listening socket
	 */
	NutSocket(accept_flag_t, const NutSocket & listen_sock):
		m_impl(-1),
		m_current_ch('\0'),
		m_current_ch_valid(false)
	{
		accept(*this, listen_sock);
	}

	/**
	 *  \brief  Bind socket to an address
	 *
	 *  \param[in]   addr      Socket address
	 *  \param[out]  err_code  Error code
	 *  \param[out]  err_msg   Error message
	 *
	 *  \retval true  on success
	 *  \retval false on error
	 */
	bool bind(const Address & addr, int & err_code, std::string & err_msg) throw();

	/**
	 *  \brief  Bind socket to an address
	 *
	 *  \param  addr  Socket address
	 *
	 *  \retval true  on success
	 *  \retval false on error
	 */
	inline bool bind(const Address & addr) throw() {
		int ec;
		std::string em;

		return bind(addr, ec, em);
	}

	/**
	 *  \brief  Bind socket to an address (or throw exception)
	 *
	 *  \param  addr  Socket address
	 */
	inline void bindx(const Address & addr) throw(std::runtime_error) {
		int ec;
		std::string em;

		if (bind(addr, ec, em))
			return;

		std::stringstream e;
		e << "Failed to bind socket to address " << addr.str() << ": " << ec << ": " << em;

		throw std::runtime_error(e.str());
	}

	/**
	 *  \brief  Listen on a socket
	 *
	 *  The function sets TCP listen socket.
	 *
	 *  \param[in]   backlog   Limit of pending connections
	 *  \param[out]  err_code  Error code
	 *  \param[out]  err_msg   Error message
	 *
	 *  \retval true  on success
	 *  \retval false on error
	 */
	bool listen(int backlog, int & err_code, std::string & err_msg) throw();

	/**
	 *  \brief  Listen on socket
	 *
	 *  \param[in]   backlog   Limit of pending connections
	 *
	 *  \retval true  on success
	 *  \retval false on error
	 */
	inline bool listen(int backlog) throw() {
		int ec;
		std::string em;

		return listen(backlog, ec, em);
	}

	/**
	 *  \brief  Listen on socket (or throw an exception)
	 *
	 *  \param[in]   backlog   Limit of pending connections
	 */
	inline void listenx(int backlog) throw(std::runtime_error) {
		int ec;
		std::string em;

		if (listen(backlog, ec, em))
			return;

		std::stringstream e;
		e << "Failed to listen on socket: " << ec << ": " << em;

		throw std::runtime_error(e.str());
	}

	/**
	 *  \brief  Connect to a listen socket
	 *
	 *  \param[in]   addr      Remote address
	 *  \param[out]  err_code  Error code
	 *  \param[out]  err_msg   Error message
	 *
	 *  \retval true  on success
	 *  \retval false on error
	 */
	bool connect(const Address & addr, int & err_code, std::string & err_msg) throw();

	/**
	 *  \brief  Connect to a listen socket
	 *
	 *  \param[in]  addr  Remote address
	 *
	 *  \retval true  on success
	 *  \retval false on error
	 */
	inline bool connect(const Address & addr) throw() {
		int ec;
		std::string em;

		return connect(addr, ec, em);
	}

	/**
	 *  \brief  Connect to a listen socket (or throw an exception)
	 *
	 *  \param[in]  addr  Remote address
	 */
	inline void connectx(const Address & addr) throw(std::runtime_error) {
		int ec;
		std::string em;

		if (connect(addr, ec, em))
			return;

		std::stringstream e;
		e << "Failed to connect socket: " << ec << ": " << em;

		throw std::runtime_error(e.str());
	}

	/**
	 *  \brief  Close socket
	 *
	 *  \param[out]  err_code  Error code
	 *  \param[out]  err-msg   Error message
	 *
	 *  \retval true  if close succeeded
	 *  \retval false if close failed
	 */
	bool close(int & err_code, std::string & err_msg) throw();

	/**
	 *  \brief  Close socket
	 *
	 *  \retval true  if close succeeded
	 *  \retval false if close failed
	 */
	inline bool close() throw() {
		int ec;
		std::string em;

		return close(ec, em);
	}

	/** Close socket (or throw exception) */
	inline void closex() throw(std::runtime_error) {
		int ec;
		std::string em;

		if (close(ec, em))
			return;

		std::stringstream e;
		e << "Failed to close socket " << m_impl << ": " << ec << ": " << em;

		throw std::runtime_error(e.str());
	}

	// NutStream interface implementation
	status_t getChar(char & ch)			throw();
	void     readChar()				throw();
	status_t getString(std::string & str)		throw();
	status_t putChar(char ch)			throw();
	status_t putString(const std::string & str)	throw();
	inline status_t putData(const std::string & data) {
		return putString(data);  // no difference on sockets
	}

	/** Destructor (closes socket if necessary) */
	~NutSocket();

	private:

	/**
	 *  \brief  Copy constructor
	 *
	 *  TODO: Copying is forbidden (for now).
	 *  If required, it may be enabled, using dup.
	 *
	 *  \param  orig  Original file
	 */
	NutSocket(const NutSocket & orig) {
		throw std::logic_error("NOT IMPLEMENTED");
	}

	/**
	 *  \brief  Assignment
	 *
	 *  TODO: Assignment is forbidden (for now).
	 *  See copy constructor for implementation notes
	 *  (and don't forget to destroy left value, properly).
	 *
	 *  \param  rval  Right value
	 */
	NutSocket & operator = (const NutSocket & rval) {
		throw std::logic_error("NOT IMPLEMENTED");
	}

};  // end of class NutSocket

}  // end of namespace nut

#endif /* __cplusplus */

#endif /* end of #ifndef nut_nutstream_h */
