/*
    NUT IPC unit test

        Copyright (C) 2012

            \author Vaclav Krpec <VaclavKrpec@Eaton.com>

        Copyright (C) 2024-2025

            \author Jim Klimov <jimklimov+nut@gmail.com>

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

#include "nutipc.hpp"
#include "nutstream.hpp"

#include <sstream>

extern "C" {
#include <unistd.h>
#include <signal.h>
#include <string.h>

extern bool verbose;
}

/* Current CPPUnit offends the honor of C++98 and maybe later versions */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC)
#pragma GCC diagnostic push
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS
#  pragma GCC diagnostic ignored "-Wglobal-constructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS
#  pragma GCC diagnostic ignored "-Wexit-time-destructors"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wsuggest-override"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wsuggest-destructor-override"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wweak-vtables"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wextra-semi"
# endif
# ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC
#  pragma GCC diagnostic ignored "-Wold-style-cast"
# endif
#endif
#if (defined __clang__) && (defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS)
# ifdef HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push "-Wdeprecated-declarations"
# endif
#endif

#include <cppunit/extensions/HelperMacros.h>

/**
 *  \brief  NUT IPC module unit test
 */
class NutIPCUnitTest: public CppUnit::TestFixture {
	private:

	CPPUNIT_TEST_SUITE(NutIPCUnitTest);
		CPPUNIT_TEST( testExec );
		CPPUNIT_TEST( testSignalSend );
		CPPUNIT_TEST( testSignalRecvQuick );
		CPPUNIT_TEST( testSignalRecvStaggered );
	CPPUNIT_TEST_SUITE_END();

	/**
	 *  \brief  Test signal handler
	 *
	 *  \param  signal  Signal caught
	 */
	static void testSignalHandler(int signal);

	public:

	/** External command execution test */
	void testExec();

	/** Signal sending test */
	void testSignalSend();

	/** Signal receiving test */
	void testSignalRecvQuick();
	void testSignalRecvStaggered();

	inline void setUp() override {}
	inline void tearDown() override {}

	virtual ~NutIPCUnitTest() override;
};  // end of class NutIPCUnitTest

// Register the test suite
CPPUNIT_TEST_SUITE_REGISTRATION(NutIPCUnitTest);


void NutIPCUnitTest::testExec() {
#ifdef WIN32
	/* FIXME NUT_WIN32_INCOMPLETE:
	 *  Some other program, maybe NUT's "message" handler, or "cmd -k" etc.?
	 *  And get Process working in the first place */
	std::cout << "NutIPCUnitTest::testExec(): skipped on this platform" << std::endl;
#else	/* !WIN32 */
	static const std::string bin = "/bin/sh";

	nut::Process::Executor::Arguments args;

	args.push_back("-c");
	args.push_back("exit 123");

	nut::Process::Execution child(bin, args);

	CPPUNIT_ASSERT(123 == child.wait());

	CPPUNIT_ASSERT(0 == nut::Process::execute("test 'Hello world' = 'Hello world'"));
#endif	/* !WIN32 */
}


/** Last signal caught */
static int signal_caught = 0;

void NutIPCUnitTest::testSignalHandler(int signal) {
	signal_caught = signal;
}

void NutIPCUnitTest::testSignalSend() {
#ifdef WIN32
	/* FIXME NUT_WIN32_INCOMPLETE:
	 *  Needs implementation for signals via pipes */
	std::cout << "NutIPCUnitTest::testSignalSend(): skipped on this platform" << std::endl;
#else	/* !WIN32 */
	struct sigaction action;

	pid_t my_pid = nut::Process::getPID();

	// Set SIGUSR1 signal handler
	::memset(&action, 0, sizeof(action));
# ifdef sigemptyset
	// no :: here because macro
	sigemptyset(&action.sa_mask);
# else
	::sigemptyset(&action.sa_mask);
# endif

	action.sa_handler = &testSignalHandler;

	CPPUNIT_ASSERT(0 == ::sigaction(static_cast<int>(nut::Signal::USER1), &action, nullptr));

	// Send signal directly
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));

	CPPUNIT_ASSERT(static_cast<int>(nut::Signal::USER1) == signal_caught);

	signal_caught = 0;

	std::stringstream my_pid_ss;

	// Save PID to a PIDfile; use an unique filename as much as we can
	// (avoid conflicts in parallel running tests on NUT CI farm, etc.)
	my_pid_ss << nut::NutFile::tmp_dir() << nut::NutFile::path_sep()
		<< "nutipc_ut_" << my_pid << ".pid";
	static const std::string pid_file_name(my_pid_ss.str());

	my_pid_ss.str("");
	my_pid_ss.clear();
	my_pid_ss << my_pid;

	if (verbose)
		std::cerr << "NutIPCUnitTest::testSignalSend(): using PID file '"
		<< pid_file_name << "' for PID " << my_pid
		<< " to store string '" << my_pid_ss.str() << "'"
		<< std::endl << std::flush;

	nut::NutFile pid_file(pid_file_name, nut::NutFile::WRITE_ONLY);

	pid_file.putString(my_pid_ss.str());

	pid_file.closex();

	// Send signal to process via the PIDfile
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, pid_file_name));

	CPPUNIT_ASSERT(static_cast<int>(nut::Signal::USER1) == signal_caught);

	pid_file.removex();

	signal_caught = 0;
#endif	/* !WIN32 */
}


/** Caught signal list */
static nut::Signal::List caught_signals;

/** Signal handler routine */
class TestSignalHandler: public nut::Signal::Handler {
	public:

	void operator () (nut::Signal::enum_t signal) override {
		caught_signals.push_back(signal);
	}

	virtual ~TestSignalHandler() override;
};  // end of class TestSignalHandler

// \todo Describe the point of this test.
void NutIPCUnitTest::testSignalRecvQuick() {
#ifdef WIN32
	/* FIXME NUT_WIN32_INCOMPLETE:
	 *  Needs implementation for signals via pipes */
	std::cout << "NutIPCUnitTest::testSignalRecvQuick(): skipped on this platform" << std::endl;
#else	/* !WIN32 */
	// Create signal handler thread
	nut::Signal::List signals;
	caught_signals.clear();

	signals.push_back(nut::Signal::USER1);
	signals.push_back(nut::Signal::USER2);

	nut::Signal::HandlerThread<TestSignalHandler> sig_handler(signals);

	pid_t my_pid = nut::Process::getPID();

	/*
	 * POSIX does not require signals to be delivered in order.
	 * It does not require that signals are like messages, but
	 * rather views them as a software version of hardware
	 * interrupts.  Two sent signals might result in only one
	 * handler invocation.  However, we (and most other signal
	 * users) expect that signals are usually in order and usually
	 * relatively promptly.
	 *
	 * For now, insist on beyond-POSIX behavior, as a canary that
	 * if triggered, we should examine nut's use of signals.
	 */

	/* Send two signals, and pause briefly to allow delivery. */
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER2, my_pid));
	::sleep(1);

	/* Send two signals in the other order, and again pause briefly. */
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER2, my_pid));
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));
	::sleep(1);

	/* Send a single signal. */
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));

	/*
	 * Sleep 1s, assuming that is long enough for all signals to
	 * be delivered (really, the last one) and the handler to have
	 * run to completion.
	 */
	::sleep(1);

	/*
	 * Check that all 5 sent were received.  Note that strictly,
	 * an OS on which USER1 and USER2 are each received once is
	 * not a failure to conform.  But a delay of 1s in signal
	 * delivery would generally be seen as not ok.
	 */
	CPPUNIT_ASSERT(caught_signals.size() == 5);

	/*
	 * Loop over the received signal records.  Count the number of
	 * USER1 and USER2, and assert that no signals other than
	 * those two were received.
	 */
	int countUSER1 = 0;
	int countUSER2 = 0;
	while (!caught_signals.empty()) {
		nut::Signal::enum_t signal = caught_signals.front();
		caught_signals.pop_front();

		if (signal == nut::Signal::USER1) {
			countUSER1++;
		} else if (signal == nut::Signal::USER2) {
			countUSER2++;
		} else {
			std::stringstream msg;
			msg << "Unexpected signal was received: " << signal;
			CPPUNIT_ASSERT_MESSAGE(msg.str(), 0);
		}
	}

	/* Check that received count matches sent count from code above. */
	CPPUNIT_ASSERT(countUSER1 == 3);
	CPPUNIT_ASSERT(countUSER2 == 2);
#endif	/* !WIN32 */
}

void NutIPCUnitTest::testSignalRecvStaggered() {
#ifdef WIN32
	/* FIXME NUT_WIN32_INCOMPLETE:
	 *  Needs implementation for signals via pipes */
	std::cout << "NutIPCUnitTest::testSignalRecvStaggered(): skipped on this platform" << std::endl;
#else	/* !WIN32 */
	// Create signal handler thread
	nut::Signal::List signals;
	caught_signals.clear();

	signals.push_back(nut::Signal::USER1);
	signals.push_back(nut::Signal::USER2);

	nut::Signal::HandlerThread<TestSignalHandler> sig_handler(signals);

	pid_t my_pid = nut::Process::getPID();

	/* NOTE: The signal order delivery is not specified by POSIX if several
	 * ones arrive nearly simultaneously (and/or get confused by multi-CPU
	 * routing). Linux tends to deliver lower-numbered signals first, so we
	 * expect USER1 (10) before USER2 (12) to be consistent. Otherwise CI
	 * builds tend to mess this up a bit.
	 */
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));
	::sleep(1);

	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER2, my_pid));
	::sleep(1);

	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER2, my_pid));
	::sleep(1);

	/* Help ensure ordered (one-by-one) delivery before re-posting a
	 * presumably lower-numbered signal after some higher-numbered ones.
	 */
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));

	// Let the sig. handler thread finish...
	::sleep(1);

	CPPUNIT_ASSERT(caught_signals.size() == 4);

	CPPUNIT_ASSERT(caught_signals.front() == nut::Signal::USER1);

	caught_signals.pop_front();

	CPPUNIT_ASSERT(caught_signals.front() == nut::Signal::USER2);

	caught_signals.pop_front();

	CPPUNIT_ASSERT(caught_signals.front() == nut::Signal::USER2);

	caught_signals.pop_front();

	CPPUNIT_ASSERT(caught_signals.front() == nut::Signal::USER1);
#endif	/* !WIN32 */
}

// Implement out of class declaration to avoid
//   error: 'SomeClass' has no out-of-line virtual method
//   definitions; its vtable will be emitted in every translation unit
//   [-Werror,-Wweak-vtables]
TestSignalHandler::~TestSignalHandler() {}
NutIPCUnitTest::~NutIPCUnitTest() {}

#if (defined __clang__) && (defined HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS)
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXIT_TIME_DESTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_GLOBAL_CONSTRUCTORS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DECLARATIONS || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SUGGEST_DESTRUCTOR_OVERRIDE_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_WEAK_VTABLES_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_DEPRECATED_DYNAMIC_EXCEPTION_SPEC_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_EXTRA_SEMI_BESIDEFUNC || defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_OLD_STYLE_CAST_BESIDEFUNC)
# pragma GCC diagnostic pop
#endif
