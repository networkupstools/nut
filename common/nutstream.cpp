/*
    nutstream.cpp - NUT stream

    Copyright (C)
        2012	Vaclav Krpec  <VaclavKrpec@Eaton.com>
        2024	Jim Klimov <jimklimov+nut@gmail.com>

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

#include "nutstream.hpp"

#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cerrno>

extern "C" {

/* For C++ code below, we do not actually use the fallback time methods
 * (on mingw mostly), but in C++ context they happen to conflict with
 * time.h or ctime headers, while native-C does not. Just disable the
 * fallback localtime_r(), gmtime_r() etc. if/when NUT timehead.h gets
 * included by the header chain from common.h:
 */
#ifndef HAVE_GMTIME_R
# define HAVE_GMTIME_R 111
#endif
#ifndef HAVE_LOCALTIME_R
# define HAVE_LOCALTIME_R 111
#endif
#include "common.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Windows/Linux Socket compatibility layer, lifted from nutclient.cpp
 * (note we do not use wincompat.h here as it slightly conflicts, with
 * extern vs. static etc.) */
/* Thanks to Benjamin Roux (http://broux.developpez.com/articles/c/sockets/) */
#ifdef WIN32
#  define SOCK_OPT_CAST(x) reinterpret_cast<const char*>(x)

/* equivalent of W32_NETWORK_CALL_OVERRIDE
 * invoked by wincompat.h in upsclient.c:
 */
static inline int sktconnect(int fh, struct sockaddr * name, int len)
{
	int ret = connect(fh,name,len);
	errno = WSAGetLastError();
	return ret;
}
static inline int sktread(int fh, void *buf, int size)
{
	int ret = recv(fh,(char*)buf,size,0);
	errno = WSAGetLastError();
	return ret;
}
static inline int sktwrite(int fh, const void*buf, int size)
{
	int ret = send(fh,(char*)buf,size,0);
	errno = WSAGetLastError();
	return ret;
}
static inline int sktclose(int fh)
{
	int ret = closesocket((SOCKET)fh);
	errno = WSAGetLastError();
	return ret;
}

#else /* not WIN32 */

#  include <sys/un.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <netdb.h> /* gethostbyname */
#  include <fcntl.h>
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET -1
#  endif
#  ifndef SOCKET_ERROR
#    define SOCKET_ERROR -1
#  endif
#  ifndef closesocket
#    define closesocket(s) close(s)
#  endif
#  define SOCK_OPT_CAST(x) reinterpret_cast<const char*>(x)
   typedef int SOCKET;
   typedef struct sockaddr_in SOCKADDR_IN;
   typedef struct sockaddr SOCKADDR;
   typedef struct in_addr IN_ADDR;

#  define sktconnect(h,n,l)	::connect(h,n,l)
#  define sktread(h,b,s) 	::read(h,b,s)
#  define sktwrite(h,b,s) 	::write(h,b,s)
#  define sktclose(h)		::close(h)

/* WA for Solaris/i386 bug: non-blocking connect sets errno to ENOENT */
#  if (defined NUT_PLATFORM_SOLARIS)
#    define SOLARIS_i386_NBCONNECT_ENOENT(status) ( (!strcmp("i386", CPU_TYPE)) ? (ENOENT == (status)) : 0 )
#  else
#    define SOLARIS_i386_NBCONNECT_ENOENT(status) (0)
#  endif  /* end of Solaris/i386 WA for non-blocking connect */

/* WA for AIX bug: non-blocking connect sets errno to 0 */
#  if (defined NUT_PLATFORM_AIX)
#    define AIX_NBCONNECT_0(status) (0 == (status))
#  else
#    define AIX_NBCONNECT_0(status) (0)
#  endif  /* end of AIX WA for non-blocking connect */

#endif /* WIN32 */
/* End of Windows/Linux Socket compatibility layer */
}


namespace nut {

/* Trivial implementation out of class declaration to avoid
 * error: 'ClassName' has no out-of-line virtual method definitions; its vtable
 *   will be emitted in every translation unit [-Werror,-Wweak-vtables]
 */
NutStream::~NutStream() {}

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


/* Here we align with OS envvars like TMPDIR or TEMPDIR,
 * consider portability to Windows, or use of tmpfs like
 * /dev/shm or (/var)/run on some platforms - e.g. NUT
 * STATEPATH, (ALT)PIDPATH or similar locations desired
 * by packager who knows their system. */

static bool checkExistsWritableDir(const char *s) {
	DIR	*pd;
#ifdef DEBUG
	std::cerr << "checkExistsWritableDir(" << (s ? s : "<null>") << "): ";
#endif
	if (!s || *s == '\0') {
#ifdef DEBUG
		std::cerr << "null or empty string" << std::endl;
#endif
		return false;
	}

	if (!(pd = opendir(s))) {
#ifdef DEBUG
		std::cerr << "not a dir" << std::endl;
#endif
		return false;
	}
	closedir(pd);

	/* POSIX: If the requested access is permitted, access() succeeds
	 * and shall return 0; otherwise, -1 shall be returned and errno
	 * shall be set to indicate the error. */
	if (access(s, X_OK)) {
#ifdef DEBUG
		std::cerr << "not traversable" << std::endl;
#endif
		return false;
	}

	if (access(s, W_OK)) {
#ifdef DEBUG
		std::cerr << "not writeable" << std::endl;
#endif
		return false;
	}

#ifdef DEBUG
	std::cerr << "is ok" << std::endl;
#endif
	return true;
}

static const char* getTmpDirPath() {
	const char *s;

#ifdef WIN32
	/* Suggestions from https://sourceforge.net/p/mingw/bugs/666/ */
	static char pathbuf[NUT_PATH_MAX + 1];
	int i;
#endif

	if (checkExistsWritableDir(s = ::altpidpath()))
		return s;

	/* NOTE: For C++17 or newer we might also call
	 * https://en.cppreference.com/w/cpp/filesystem/temp_directory_path
	 */

#ifdef WIN32
	i = GetTempPathA(sizeof(pathbuf), pathbuf);
	if ((i > 0) && (i < NUT_PATH_MAX) && checkExistsWritableDir(pathbuf))
		return (const char *)pathbuf;
#endif

	if (checkExistsWritableDir(s = ::getenv("TMPDIR")))
		return s;
	if (checkExistsWritableDir(s = ::getenv("TEMPDIR")))
		return s;
	if (checkExistsWritableDir(s = ::getenv("TEMP")))
		return s;
	if (checkExistsWritableDir(s = ::getenv("TMP")))
		return s;

	/* Some OS-dependent locations */
#ifdef WIN32
	if (checkExistsWritableDir(s = "C:\\Temp"))
		return s;
	if (checkExistsWritableDir(s = "C:\\Windows\\Temp"))
		return s;
	if (checkExistsWritableDir(s = "/c/Temp"))
		return s;
	if (checkExistsWritableDir(s = "/c/Windows/Temp"))
		return s;
#else
	if (checkExistsWritableDir(s = "/dev/shm"))
		return s;
	if (checkExistsWritableDir(s = "/run"))
		return s;
	if (checkExistsWritableDir(s = "/var/run"))
		return s;
#endif

	/* May be applicable to WIN32 depending on emulation environment/mapping */
	if (checkExistsWritableDir(s = "/tmp"))
		return s;
	if (checkExistsWritableDir(s = "/var/tmp"))
		return s;

	/* Maybe ok, for tests at least: a current working directory */
	if (checkExistsWritableDir(s = "."))
		return s;

	return "/tmp";
}

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
const std::string NutFile::m_tmp_dir(getTmpDirPath());
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS)
#pragma GCC diagnostic pop
#endif

NutFile::NutFile(anonymous_t):
	m_name(""),
	m_impl(nullptr),
	m_current_ch('\0'),
	m_current_ch_valid(false)
{
#ifdef WIN32
	/* Suggestions from https://sourceforge.net/p/mingw/bugs/666/ because
	 * msvcrt tmpfile() uses C: root dir and lacks permissions to actually
	 * use it, and mingw tends to call that OS method so far */
	char filename[NUT_PATH_MAX + 1];
	memset(filename, 0, sizeof(filename));

	GetTempFileNameA(m_tmp_dir.c_str(), "nuttemp", 0, filename);
	/* if (verbose) std::cerr << "TMP FILE: " << filename << std::endl; */
	std::string mode_str = std::string(strAccessMode(READ_WRITE_CLEAR));
	/* ...Still, we ask to auto-delete where supported: */
	mode_str += std::string("D");
	/* Per https://en.cppreference.com/w/cpp/io/c/tmpfile it is binary
	 * for POSIX code, so match the behavior here: */
	mode_str += std::string("b");
	m_impl = ::fopen(filename, mode_str.c_str());
	/* If it were not "const" we might assign it. But got no big need to.
	 *   m_name = std::string(filename);
	 */
#else
	/* TOTHINK: How to make this use m_tmp_dir? Is it possible generally?  */
	/* Safer than tmpnam() but we don't know the filename here.
	 * Not that we need it, system should auto-delete it. */
	m_impl = ::tmpfile();
#endif

	if (nullptr == m_impl) {
		int err_code = errno;

		std::stringstream e;
		e << "Failed to create temporary file: " << err_code
				<< ": " << ::strerror(err_code);

#ifdef WIN32
		e << ": tried using temporary location " << m_tmp_dir;
		if (filename[0] != '\0')
			e << ": OS suggested filename " << filename;
#endif

		throw std::runtime_error(e.str());
	}
}


bool NutFile::exists(int & err_code, std::string & err_msg) const
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	struct stat info;
	int status;

	/* An opened file pointer is assumed to be backed by a file at this
	 * moment (even if unlinked, temporary with unknown name, etc.) */
	if (m_impl != nullptr)
		return true;

	/* Can not stat an unknown file name */
	if (m_name.empty())
		return false;

	status = ::stat(m_name.c_str(), &info);

	if (!status)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


const char * NutFile::strAccessMode(access_t mode)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	/* NOTE: Currently we use OS-default binary/text choice of content mode
	 * since we primarily expect to manipulate user-editable config files */
	static const char *read_only        = "r";
	static const char *write_only       = "w";
	static const char *read_write       = "r+";
	static const char *read_write_clear = "w+";
	static const char *append_only      = "a";
	static const char *read_append      = "a+";

	const char *mode_str = nullptr;

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

	assert(nullptr != mode_str);

	return mode_str;
}

bool NutFile::open(access_t mode, int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	const char *mode_str;

	/* Can not open an unknown file name */
	if (m_name.empty()) {
		err_code = ENOENT;
		err_msg  = std::string("No file name was specified");
		return false;
	}

	if (nullptr != m_impl) {
		/* TOTHINK: Should we care about errors in this close()? */
		::fclose(m_impl);
	}

#ifdef WIN32
	/* This currently fails with mingw due to looking at POSIXified paths:
	 *   - Failed to open file /c/Users/abuild/Documents/FOSS/nut/conf/nut.conf.sample: 2: No such file or directory
	 * while the file does exist (for git-bash and mingw shells):
	 *   $ ls -la /c/Users/abuild/Documents/FOSS/nut/conf/nut.conf.sample
	 *   -rw-r--r-- 1 abuild Users 4774 Jan 28 03:38 /c/Users/abuild/Documents/FOSS/nut/conf/nut.conf.sample
	 *
	 * For the test suite it is not a great problem, can be fixed
	 * by using `cygpath` or `pwd -W` in `configure` script.
	 * The run-time behavior is more troublesome: per discussion at
	 * https://sourceforge.net/p/mingw/mailman/mingw-users/thread/gq8fi0$pk0$2@ger.gmane.org/
	 * mingw uses fopen() from msvcrt directly, and it does not know
	 * such paths (e.g. '/c/Users/...' means "C:\\c\\Users\\..." to it).
	 * Paths coming from MSYS shell arguments are handled by the shell,
	 * and this is more than about slash type (WINNT is okay with both),
	 * but also e.g. prefixing an msys installation path 'C:\\msys64' or
	 * similar when using absolute POSIX-style paths. Do we need a private
	 * converter?.. Would an end user have MSYS installed at all?
	 */
#endif

	mode_str = strAccessMode(mode);
	m_impl = ::fopen(m_name.c_str(), mode_str);

	if (nullptr != m_impl)
		return true;

	err_code = errno;
	err_msg  = "Failed to open file '" + m_name + "': "
			+ std::string(::strerror(err_code));

	return false;
}


bool NutFile::flush(int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	if (nullptr == m_impl) {
		err_code = EBADF;
		err_msg = std::string(::strerror(err_code));
		return false;
	}

	err_code = ::fflush(m_impl);

	if (0 != err_code) {
		err_msg = std::string(::strerror(err_code));

		return false;
	}

	return true;
}


bool NutFile::close(int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	/* Already closed (or never opened) - goal achieved */
	if (nullptr == m_impl) {
		return true;
	}

	err_code = ::fclose(m_impl);

	if (0 != err_code) {
		err_msg = std::string(::strerror(err_code));

		return false;
	}

	m_impl = nullptr;

	return true;
}


bool NutFile::remove(int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	/* Can not unlink an unknown file name */
	if (m_name.empty()) {
		err_code = ENOENT;
		err_msg  = std::string("No file name was specified");
		return false;
	}

	/* FWIW, note that if the file descriptor is opened by this
	 * or any other process(es), it remains usable only by them
	 * after un-linking, and the file system resources would be
	 * released only when everyone closes it. */
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
	m_impl(nullptr),
	m_current_ch('\0'),
	m_current_ch_valid(false)
{
	openx(mode);
}


NutFile::NutFile(access_t mode):
			NutFile(ANONYMOUS)
{
	const char *mode_str = strAccessMode(mode);
	m_impl = ::freopen(nullptr, mode_str, m_impl);

	if (nullptr == m_impl) {
		int err_code = errno;

		std::stringstream e;
		e << "Failed to re-open temporary file with mode '" << mode_str
				<< "': " << err_code
				<< ": " << ::strerror(err_code);

		throw std::runtime_error(e.str());
	}
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
	assert(nullptr != file);

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


NutStream::status_t NutFile::getChar(char & ch)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	if (m_current_ch_valid) {
		ch = m_current_ch;

		return NUTS_OK;
	}

	if (nullptr == m_impl)
		return NUTS_ERROR;

	status_t status = fgetcWrapper(m_impl, ch);

	if (NUTS_OK != status)
		return status;

	// Cache the character for future reference
	m_current_ch       = ch;
	m_current_ch_valid = true;

	return NUTS_OK;
}


void NutFile::readChar()
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	m_current_ch_valid = false;
}


NutStream::status_t NutFile::getString(std::string & str)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	if (m_current_ch_valid)
		str += m_current_ch;

	m_current_ch_valid = false;

	if (nullptr == m_impl)
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


NutStream::status_t NutFile::putChar(char ch)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	int c;

	if (nullptr == m_impl)
		return NUTS_ERROR;

	c = ::fputc(static_cast<int>(ch), m_impl);

	return EOF == c ? NUTS_ERROR : NUTS_OK;
}


NutStream::status_t NutFile::putString(const std::string & str)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	int c;

	if (nullptr == m_impl)
		return NUTS_ERROR;

	c = ::fputs(str.c_str(), m_impl);

	return EOF == c ? NUTS_ERROR : NUTS_OK;
}


NutStream::status_t NutFile::putData(const std::string & data)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
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
	if (nullptr != m_impl)
		closex();
}


void NutSocket::Address::init_unix(Address & addr, const std::string & path) {
#ifdef WIN32
	/* FIXME: Windows pipes where available? See e.g. clone drivers */
	NUT_UNUSED_VARIABLE(addr);
	NUT_UNUSED_VARIABLE(path);

	std::stringstream e;
	e << "Unix sockets not implemented for this platform yet: " << path;
//			addr.str() << ":" << path;

	throw std::logic_error(e.str());
#else
	struct sockaddr_un * un_addr = reinterpret_cast<struct sockaddr_un *>(::malloc(sizeof(struct sockaddr_un)));

	if (nullptr == un_addr)
		throw std::bad_alloc();

	un_addr->sun_family = AF_UNIX;

	assert(sizeof(un_addr->sun_path) / sizeof(char) > path.size());

	for (size_t i = 0; i < path.size(); ++i)
		un_addr->sun_path[i] = path.at(i);

	un_addr->sun_path[path.size()] = '\0';

	addr.m_sock_addr = reinterpret_cast<struct sockaddr *>(un_addr);
	addr.m_length    = sizeof(*un_addr);
#endif
}


void NutSocket::Address::init_ipv4(Address & addr, const std::vector<unsigned char> & qb, uint16_t port) {
	assert(4 == qb.size());

	uint32_t packed_qb = 0;

	struct sockaddr_in * in4_addr = reinterpret_cast<struct sockaddr_in *>(::malloc(sizeof(struct sockaddr_in)));

	if (nullptr == in4_addr)
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

	struct sockaddr_in6 * in6_addr = reinterpret_cast<struct sockaddr_in6 *>(::malloc(sizeof(struct sockaddr_in6)));

	if (nullptr == in6_addr)
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
#if (defined __cplusplus) && (__cplusplus < 201100)
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


NutSocket::Address::Address(const Address & orig): m_sock_addr(nullptr), m_length(orig.m_length) {
	void * copy = ::malloc(m_length);

	if (nullptr == copy)
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

	ss << (packed       & 0x000000ff) << ".";
	ss << (packed >>  8 & 0x000000ff) << ".";
	ss << (packed >> 16 & 0x000000ff) << ".";
	ss << (packed >> 24 & 0x000000ff);

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
	// TODO: ommition (REVIEW: omission?) of lengthy zero word strings
	ss << std::uppercase << std::hex << std::setfill('0');

	for (size_t i = 0; ; ) {
		uint16_t w = static_cast<uint16_t>(static_cast<uint16_t>(bytes[2 * i]) << 8) | static_cast<uint16_t>(bytes[2 * i + 1]);

		ss << std::setw(4) << w;

		if (8 == ++i)
			break;

		ss << ':';
	}

	return ss.str();
}


std::string NutSocket::Address::str() const {
	assert(nullptr != m_sock_addr);

	/* Note: we do not cache a copy of "family" because
	 * its data type varies per platform, easier to just
	 * request it each time - not a hot spot anyway. */
	std::stringstream ss;
	ss << "nut::NutSocket::Address(family: " << m_sock_addr->sa_family;

	switch (m_sock_addr->sa_family) {
#ifndef WIN32
		/* FIXME: Add support for pipes on Windows? */
		case AF_UNIX: {
			struct sockaddr_un * addr = reinterpret_cast<struct sockaddr_un *>(m_sock_addr);

			ss << " (UNIX domain socket), file: " << addr->sun_path;

			break;
		}
#endif

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
			e << "NOT IMPLEMENTED: Socket address family " << m_sock_addr->sa_family << " unsupported";

			throw std::logic_error(e.str());
		}
	}

	ss << ")";

	return ss.str();
}


NutSocket::Address::~Address() {
	::free(m_sock_addr);
}


/* NOTE: static class method */
bool NutSocket::accept(
	NutSocket &       sock,
	const NutSocket & listen_sock,
	int &             err_code,
	std::string &     err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw(std::logic_error)
#endif
{
	assert(-1 == sock.m_impl);

	struct sockaddr sock_addr;
	socklen_t       sock_addr_size = sizeof(sock_addr);

	sock.m_impl = ::accept(listen_sock.m_impl, &sock_addr, &sock_addr_size);
	sock.m_domain = listen_sock.m_domain;
	sock.m_type = listen_sock.m_type;

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
		default:
			break;
	}

	std::stringstream e;
	e << "Failed to accept connection: " << err_code << ": " << err_msg;

	throw std::logic_error(e.str());
}


NutSocket::NutSocket(domain_t dom, type_t type, proto_t proto):
	m_impl(-1),
	m_domain(dom),
	m_type(type),
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


bool NutSocket::bind(const Address & addr, int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	if (m_domain == NUTSOCKD_UNDEFINED) {
		m_domain = static_cast<NutSocket::domain_t>(addr.m_sock_addr->sa_family);
	}
	else if (static_cast<int>(m_domain) != static_cast<int>(addr.m_sock_addr->sa_family)) {
		err_code = EINVAL;
		err_msg  = std::string(::strerror(err_code)) +
				": bind() with a different socket address family than this object was created for";
	}

	if (m_type == NUTSOCKT_UNDEFINED) {
		/* We should have this from constructor or accept() */
		err_code = EINVAL;
		err_msg  = std::string(::strerror(err_code)) +
				": bind() with bad socket type";
	}

	err_code = ::bind(m_impl, addr.m_sock_addr, addr.m_length);

	if (0 == err_code)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutSocket::listen(int backlog, int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	err_code = ::listen(m_impl, backlog);

	if (0 == err_code)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutSocket::connect(const Address & addr, int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	if (m_domain == NUTSOCKD_UNDEFINED) {
		m_domain = static_cast<NutSocket::domain_t>(addr.m_sock_addr->sa_family);
	}
	else if (static_cast<int>(m_domain) != static_cast<int>(addr.m_sock_addr->sa_family)) {
		err_code = EINVAL;
		err_msg  = std::string(::strerror(err_code)) +
				": connect() with a different socket address family than this object was created for";
	}

	if (m_type == NUTSOCKT_UNDEFINED) {
		/* We should have this from constructor or accept() */
		err_code = EINVAL;
		err_msg  = std::string(::strerror(err_code)) +
				": connect() with bad socket type";
	}

	err_code = sktconnect(m_impl, addr.m_sock_addr, addr.m_length);

	if (0 == err_code)
		return true;

	err_code = errno;
	err_msg  = std::string(::strerror(err_code));

	return false;
}


bool NutSocket::flush(int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	if (-1 == m_impl) {
		err_code = EBADF;
		err_msg = std::string(::strerror(err_code));
		return false;
	}

	if (m_type == NUTSOCKT_STREAM && m_domain == NUTSOCKD_INETv4) {
		/* Assume IPv4 TCP: https://stackoverflow.com/a/71417876/4715872 */
		int flag = 1;
		if (!setsockopt(m_impl, IPPROTO_TCP, TCP_NODELAY, SOCK_OPT_CAST(&flag), sizeof(int))) {
			err_code = errno;
			err_msg = std::string(::strerror(err_code));
			return false;
		}
		if (!sktwrite(m_impl, nullptr, 0)) {
			err_code = errno;
			err_msg = std::string(::strerror(err_code));
			return false;
		}
		flag = 0;
		if (!setsockopt(m_impl, IPPROTO_TCP, TCP_NODELAY, SOCK_OPT_CAST(&flag), sizeof(int))) {
			err_code = errno;
			err_msg = std::string(::strerror(err_code));
			return false;
		}
	} /* else Unix, UDP (or several other socket families generally); PRs welcome */

	return true;
}


bool NutSocket::close(int & err_code, std::string & err_msg)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	err_code = sktclose(m_impl);

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


NutStream::status_t NutSocket::getChar(char & ch)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
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


void NutSocket::readChar()
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	m_current_ch_valid = false;
}


NutStream::status_t NutSocket::getString(std::string & str)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	if (m_current_ch_valid)
		str += m_current_ch;

	m_current_ch_valid = false;

	char buffer[512];

	for (;;) {
		ssize_t read_cnt = sktread(m_impl, buffer, sizeof(buffer) / sizeof(buffer[0]));

		if (read_cnt < 0)
			return NUTS_ERROR;

		if (0 == read_cnt)
			return NUTS_OK;

		str.append(buffer, static_cast<size_t>(read_cnt));
	}
}


NutStream::status_t NutSocket::putChar(char ch)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	ssize_t write_cnt = sktwrite(m_impl, &ch, 1);

	if (1 == write_cnt)
		return NUTS_OK;

	assert(-1 == write_cnt);

	// TODO: At least logging of the error (errno), if not propagation

	return NUTS_ERROR;
}


NutStream::status_t NutSocket::putString(const std::string & str)
#if (defined __cplusplus) && (__cplusplus < 201100)
		throw()
#endif
{
	size_t str_len = str.size();

	// Avoid the costly system call unless necessary
	if (0 == str_len)
		return NUTS_OK;

	ssize_t write_cnt = sktwrite(m_impl, str.data(), str_len);

	// TODO: Under certain circumstances, less than the whole
	// string might be written
	// Review the code if async. I/O is supported (in which case
	// the function shall have to implement the blocking using
	// select/poll/epoll on its own (probably select for portability)

	assert(write_cnt > 0);

	if (static_cast<size_t>(write_cnt) == str_len)
		return NUTS_OK;

	// TODO: At least logging of the error (errno), if not propagation

	return NUTS_ERROR;
}

}  // end of namespace nut
