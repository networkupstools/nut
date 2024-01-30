/*
    NUT stream unit test

    Copyright (C)
        2012	Vaclav Krpec <VaclavKrpec@Eaton.com>

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

#include <cppunit/extensions/HelperMacros.h>

#include <cstdio>
#include <cstdlib>
#include <cassert>

extern "C" {
#ifndef WIN32
# include <sys/select.h>
# include <sys/wait.h>
#else
# if !(defined random) && !(defined HAVE_RANDOM)
   /* WIN32 names it differently: */
#  define random() rand()
# endif
#endif	/* WIN32 */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
}


/** Test data */
static const std::string test_data(
	"And the mother of Jesus said unto the Lord, ""They have no more wine.""\n"
	"And Jesus said unto the servants, ""Fill six water pots with water.""\n"
	"And they did so.\n"
	"And when the steward of the feast did taste of the water from the pots, it had become wine.\n"
	"And he knew not whence it had come.\n"
	"But the servants did know, and they applauded loudly in the kitchen.\n"
	"And they said unto the Lord, ""How the Hell did you do that!?""\n"
	"And inquired of him ""Do you do children’s parties?""\n"
	"And the Lord said ""No.""\n"
	);


/**
 *  \brief  Read and check test data from a stream
 *
 *  \param  stream  Input stream
 *
 *  \retval true  in case of success
 *  \retval false in case of failure
 */
static bool readTestData(nut::NutStream * stream) {
	assert(nullptr != stream);

	// Read characters from the stream
	for (size_t pos = 0, iter = 0; ; ++iter) {
		char ch;

		nut::NutStream::status_t status = stream->getChar(ch);

		if (nut::NutStream::NUTS_ERROR == status)
			return false;

		if (nut::NutStream::NUTS_EOF == status)
			break;

		if (nut::NutStream::NUTS_OK != status)
			return false;

		if (ch != test_data.at(pos))
			return false;

		// Every other character shall be checked twice
		if (0 == iter % 8)
			continue;

		// Consume current character
		stream->readChar();

		++pos;
	}

	return true;
}


/**
 *  \brief  Write test data to a stream
 *
 *  \param  stream  Output stream
 *
 *  \retval true  in case of success
 *  \retval false in case of failure
 */
static bool writeTestData(nut::NutStream * stream) {
	assert(nullptr != stream);

	size_t pivot = static_cast<size_t>(0.5 * static_cast<double>(test_data.size()));

	// Write characters to the stream
	for (size_t i = 0; i < pivot; ++i) {
		char ch = test_data.at(i);

		nut::NutStream::status_t status = stream->putChar(ch);

		if (nut::NutStream::NUTS_OK != status)
			return false;
	}

	// Write string to the stream
	const std::string str = test_data.substr(pivot);

	nut::NutStream::status_t status = stream->putString(str);

	CPPUNIT_ASSERT(nut::NutStream::NUTS_OK == status);

	return true;
}


/**
 *  \brief  NUT stream unit test suite (abstract)
 */
class NutStreamUnitTest: public CppUnit::TestFixture {
	protected:

	/**
	 *  \brief  Read test data from stream
	 *
	 *  \c CPPUNIT_ASSERT macro is used to resolve error.
	 *
	 *  \param  stream  Input stream
	 */
	inline void readx(nut::NutStream * stream) {
		CPPUNIT_ASSERT(readTestData(stream));
	}

	/**
	 *  \brief  Write test data to stream
	 *
	 *  \c CPPUNIT_ASSERT macro is used to resolve error.
	 *
	 *  \param  stream  Output stream
	 */
	inline void writex(nut::NutStream * stream) {
		CPPUNIT_ASSERT(writeTestData(stream));
	}

	virtual ~NutStreamUnitTest() override;
};  // end of class NutStreamUnitTest


/**
 *  \brief  NUT memory stream unit test suite
 */
class NutMemoryUnitTest: public NutStreamUnitTest {
	private:

	CPPUNIT_TEST_SUITE(NutMemoryUnitTest);
		CPPUNIT_TEST(test);
	CPPUNIT_TEST_SUITE_END();

	public:

	inline void setUp() override {}
	inline void tearDown() override {}

	virtual void test();

};  // end of class NutMemoryUnitTest


void NutMemoryUnitTest::test() {
	nut::NutMemory input_mstream(test_data);
	nut::NutMemory output_mstream;

	readx(&input_mstream);
	writex(&output_mstream);
	readx(&output_mstream);
}


/**
 *  \brief  NUT file stream unit test suite
 */
class NutFileUnitTest: public NutStreamUnitTest {
	private:

	CPPUNIT_TEST_SUITE(NutFileUnitTest);
		CPPUNIT_TEST(test);
	CPPUNIT_TEST_SUITE_END();

	public:

	inline void setUp() override {}
	inline void tearDown() override {}

	virtual void test();

};  // end of class NutFileUnitTest


void NutFileUnitTest::test() {
#ifdef WIN32
	/* FIXME: It seems that in the absence of envvars for temporary location,
	 * mingw (or Windows implem behind it?) can choose C:\ or C:\WINDOWS and
	 * generally we lack permissions to write there. */
	std::cout << "NutFileUnitTest::test(): skipped on this platform" << std::endl;
#else
	nut::NutFile fstream(nut::NutFile::ANONYMOUS);

	writex(&fstream);
	readx(&fstream);
#endif	/* WIN32 */
}


/**
 *  \brief  NUT socket stream unit test suite
 */
class NutSocketUnitTest: public NutStreamUnitTest {
	private:

	/** NUT socket stream unit test: writer */
	class Writer {
		private:

		/** Remote listen address */
		nut::NutSocket::Address m_remote_address;

		public:

		/**
		 *  \brief  Constructor
		 *
		 *  \param  addr  Remote address
		 */
		Writer(const nut::NutSocket::Address & addr):
			m_remote_address(addr)
		{}

		/**
		 *  \brief  Writer routine
		 *
		 *  Writer shall write contents of the test data
		 *  to its connection socket.
		 *
		 *  \retval true  in case of success
		 *  \retval false otherwise
		 */
		bool run();

	};  // end of class Writer

	/** TCP listen address IPv4 */
	static const nut::NutSocket::Address m_listen_address;

	CPPUNIT_TEST_SUITE(NutSocketUnitTest);
		CPPUNIT_TEST(test);
	CPPUNIT_TEST_SUITE_END();

	public:

	inline void setUp() override {}
	inline void tearDown() override {}

	virtual void test();

};  // end of class NutSocketUnitTest


/* Randomize to try avoiding collisions in parallel testing */
const nut::NutSocket::Address NutSocketUnitTest::m_listen_address(
		127, 0, 0, 1,
		10000 + static_cast<uint16_t>(::random() % 40000));


bool NutSocketUnitTest::Writer::run() {
	nut::NutSocket conn_sock;

	if (!conn_sock.connect(m_remote_address))
		return false;

	if (!writeTestData(&conn_sock))
		return false;

	if (!conn_sock.close())
		return false;

	return true;
}


void NutSocketUnitTest::test() {
#ifdef WIN32
	/* FIXME: get Process working in the first place */
	std::cout << "NutSocketUnitTest::test(): skipped on this platform" << std::endl;
#else
	// Fork writer
	pid_t writer_pid = ::fork();

	if (!writer_pid) {
		// Wait for listen socket
		::sleep(1);

		// Run writer
		CPPUNIT_ASSERT(Writer(m_listen_address).run());

		return;
	}

	// Listen
	nut::NutSocket listen_sock;

	CPPUNIT_ASSERT(listen_sock.bind(m_listen_address));
	CPPUNIT_ASSERT(listen_sock.listen(10));

	// Accept connection
	nut::NutSocket conn_sock(nut::NutSocket::ACCEPT, listen_sock);

	// Read the test data
	readx(&conn_sock);

	// Wait for writer
	int   writer_exit;
	pid_t wpid = ::waitpid(writer_pid, &writer_exit, 0);

	CPPUNIT_ASSERT(wpid == writer_pid);
	CPPUNIT_ASSERT(0    == writer_exit);
#endif	/* WIN32 */
}


// Register the test suite
CPPUNIT_TEST_SUITE_REGISTRATION(NutMemoryUnitTest);
CPPUNIT_TEST_SUITE_REGISTRATION(NutFileUnitTest);
CPPUNIT_TEST_SUITE_REGISTRATION(NutSocketUnitTest);

// Implement out of class declaration to avoid
//   error: 'SomeClass' has no out-of-line virtual method
//   definitions; its vtable will be emitted in every translation unit
//   [-Werror,-Wweak-vtables]
NutStreamUnitTest::~NutStreamUnitTest() {}
