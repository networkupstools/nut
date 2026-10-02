/*
    NUT stream unit test

    Copyright (C)
        2012	Vaclav Krpec <VaclavKrpec@Eaton.com>
        2024-2025	Jim Klimov <jimklimov+nut@gmail.com>

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

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <exception>

extern "C" {
#include <pthread.h>

extern bool verbose;
}

#include "cppunit-warnings.h"

#include <cppunit/extensions/HelperMacros.h>

namespace nut {

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
static bool readTestData(nut::NutStream * stream, size_t * read_count = nullptr) {
	assert(nullptr != stream);

	// Read characters from the stream
	for (size_t pos = 0, iter = 0; ; ++iter) {
		char ch;

		nut::NutStream::status_t status = stream->getChar(ch);

		if (nut::NutStream::NUTS_ERROR == status) {
			if (verbose)
				std::cerr << "readTestData(): status==nut::NutStream::NUTS_ERROR" << std::endl;
			return false;
		}

		if (nut::NutStream::NUTS_EOF == status) {
			if (read_count != nullptr)
				*read_count = pos;
			break;
		}

		if (nut::NutStream::NUTS_OK != status) {
			if (verbose)
				std::cerr << "readTestData(): status!=nut::NutStream::NUTS_OK: " << status << std::endl;
			return false;
		}

		if (ch != test_data.at(pos)) {
			if (verbose)
				std::cerr << "readTestData(): unexpected char '"
						<< ch << "' at pos " << pos << ": want '"
						<< test_data.at(pos) << "'" << std::endl;
			return false;
		}

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

		if (nut::NutStream::NUTS_OK != status) {
			if (verbose)
				std::cerr << "writeTestData(): status!=nut::NutStream::NUTS_OK: " << status << std::endl;
			return false;
		}
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
	nut::NutFile fstream(nut::NutFile::ANONYMOUS);

	writex(&fstream);
	fstream.flushx();
	readx(&fstream);
}


/**
 *  \brief  NUT socket stream unit test suite
 */
class NutSocketUnitTest: public NutStreamUnitTest {
	private:

	CPPUNIT_TEST_SUITE(NutSocketUnitTest);
		CPPUNIT_TEST(test);
		CPPUNIT_TEST(testString);
	CPPUNIT_TEST_SUITE_END();

	void checkSocket(bool characters);

	public:

	inline void setUp() override {
#ifdef WIN32
		WSADATA wsaData;
		CPPUNIT_ASSERT(0 == WSAStartup(MAKEWORD(2, 2), &wsaData));
#endif
	}

	inline void tearDown() override {
#ifdef WIN32
		CPPUNIT_ASSERT(0 == WSACleanup());
#endif
	}

	virtual void test();
	void testString();

};  // end of class NutSocketUnitTest


void NutSocketUnitTest::test() {
	checkSocket(true);
}


void NutSocketUnitTest::testString() {
	checkSocket(false);
}


void NutSocketUnitTest::checkSocket(bool characters) {
	nut::NutSocket listen_sock;
	uint16_t port = 0;
	bool bound = false;

	/* Keep the socket bound so parallel tests cannot take its port. */
	for (int tries = 0; tries < 100 && !bound; ++tries) {
		port = static_cast<uint16_t>(10000 + std::rand() % 40000);
		bound = listen_sock.bind(nut::NutSocket::Address(127, 0, 0, 1, port));
	}

	CPPUNIT_ASSERT(bound);
	listen_sock.listenx(1);

	/* Establish both ends before starting the reader. */
	nut::NutSocket writer;
	writer.connectx(nut::NutSocket::Address(127, 0, 0, 1, port));
	nut::NutSocket reader(nut::NutSocket::ACCEPT, listen_sock);
	struct ReadData {
		nut::NutSocket * stream;
		bool characters;
		bool ok;
		std::exception_ptr error;
	} read_data = { &reader, characters, false, std::exception_ptr() };
	bool write_ok = false;
	std::exception_ptr write_error;
	pthread_t read_thread;

	int status = pthread_create(&read_thread, nullptr, [](void * arg) -> void * {
		ReadData & data = *static_cast<ReadData *>(arg);
		try {
			if (data.characters) {
				size_t read_count = 0;
				data.ok = readTestData(data.stream, &read_count)
					&& read_count == test_data.size();
			} else {
				std::string text;
				data.ok = data.stream->getString(text) == nut::NutStream::NUTS_OK
					&& text == test_data;
			}
		} catch (...) {
			data.error = std::current_exception();
		}
		return nullptr;
	}, &read_data);
	CPPUNIT_ASSERT(0 == status);

	try {
		write_ok = writeTestData(&writer);
	} catch (...) {
		write_error = std::current_exception();
	}

	/* Closing the writer delivers EOF, including after a failed write.
	 * Join before asserting or propagating either thread's exception. */
	bool closed = writer.close();
	status = pthread_join(read_thread, nullptr);

	CPPUNIT_ASSERT(0 == status);
	if (write_error)
		std::rethrow_exception(write_error);
	if (read_data.error)
		std::rethrow_exception(read_data.error);

	CPPUNIT_ASSERT(closed);
	CPPUNIT_ASSERT(write_ok);
	CPPUNIT_ASSERT(read_data.ok);
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

} // namespace nut {}

#include "cppunit-warnings-end.h"
