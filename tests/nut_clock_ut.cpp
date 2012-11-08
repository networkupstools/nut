/*
NUT clock interface unit test

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

extern "C" {
#include "nut_platform.h"
#include "nut_clock.h"

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
}

#include <cppunit/extensions/HelperMacros.h>

#include <cstdio>
#include <cassert>


/**
 *  \brief  Absolute value
 *
 *  \param  x  Value
 *
 *  \return |x|
 */
#define abs(x) ((x) < 0 ? -(x) : (x))


/**
 *  \brief  NUT clock interface unit test suite
 */
class NUTClockUnitTest : public CppUnit::TestFixture {
	private:

	/** Maximal relative timestamp difference drift (10%) */
	static const double max_relative_timediff_drift = 0.1;

	/** Maximal absolute realtime difference (10%) */
	static const double max_absolute_realtime_diff = 1.0;

	CPPUNIT_TEST_SUITE(NUTClockUnitTest);
		CPPUNIT_TEST(timestampDiffDriftAVGCheck);
		CPPUNIT_TEST(realtimeDiffAVGCheck);
	CPPUNIT_TEST_SUITE_END();

	private:

	double timestampDiffDrift(unsigned sec);
	double timestampDiffDriftAVG();

	int execute(const char *bin, char * const argv[], std::string & out);
	unsigned dateAuth(const char *bin, char * const bin_argv[]);
	unsigned date();
	unsigned perl_print_time();
	double realtimeDiff();
	double realtimeDiffAVG();

	public:

	void setUp();
	void tearDown();

	void timestampDiffDriftAVGCheck();
	void realtimeDiffAVGCheck();
};


// Register the test suite
CPPUNIT_TEST_SUITE_REGISTRATION(NUTClockUnitTest);


/** UT setup routine */
void NUTClockUnitTest::setUp() {
}


/** UT teardown routine */
void NUTClockUnitTest::tearDown() {
}


/**
 *  \brief  Measure timestamp difference relative drift
 *
 *  Measure drift between timestamp difference computed by
 *  nut_clock_* iface and an authority (select timeout).
 *  The drift is proportional to the timestamp difference.
 *
 *  \param  sec  Expected timestamp difference (in seconds)
 *
 *  \return Timestamp difference drift
 */
double NUTClockUnitTest::timestampDiffDrift(unsigned sec) {
	nut_time_t start;
	double     diff;
	double     drift;

	struct timeval timeout;
	int            st_select;

	timeout.tv_sec  = sec;
	timeout.tv_usec = 0;

	// Take the timestamp
	nut_clock_timestamp(&start);

	// Sleep
	st_select = select(0, NULL, NULL, NULL, &timeout);

	// Timestamp difference
	diff = nut_clock_sec_since(&start);

	// Sanity check
	CPPUNIT_ASSERT_EQUAL(st_select, 0);

	// Timestamp diff drift (absolute)
	drift = sec - diff;

	// Timestamp diff drift (relative)
	return abs(drift) / (double)sec;
}


/**
 *  \brief  Measure timestamp difference average relative drift
 *
 *  Perform the relative drift measure via
 *  \ref timestampDiffDrift
 *  multiple times and return average.
 *
 *  \return Timestamp difference average drift
 */
double NUTClockUnitTest::timestampDiffDriftAVG() {
	static const unsigned diffs[]  = { 2, 4, 1, 3, 5, 2, 1, 3, 2, 1 };
	static const unsigned diff_cnt = sizeof(diffs) / sizeof(unsigned);

	// Accumulate timestamp diff average drift (takes time)
	double drift = 0.0;

	for (unsigned i = 0; i < diff_cnt; ++i) {
		drift += timestampDiffDrift(diffs[i]);
	}

	return drift /= (double)diff_cnt;
}


/**
 *  \brief  Check timestamp diff average drift
 *
 *  Runs the \ref timestampDiffDrift and checks whether
 *  the drift is within bounds.
 *  Performs multiple runs if the drift is out of bounds
 *  (to avoid false-positive reports).
 */
void NUTClockUnitTest::timestampDiffDriftAVGCheck() {
	double drift;

	for (unsigned i = 0; i < 3; ++i) {
		drift = timestampDiffDriftAVG();

		if (drift < max_relative_timediff_drift)
			return;
	}

	CPPUNIT_FAIL("Average relative timestamp difference drift is too high");
}


/**
 *  \brief  Execute command
 *
 *  The method executes a program in a child process.
 *  The program binary may be specified either with full
 *  absolute path, using a relative path or even without
 *  path (current process environment variable PATH is
 *  considered).
 *
 *  \param  bin   Program binary file name
 *  \param  argv  Program arguments (including $0)
 *  \param  out   Program output
 *
 *  \return The command standard output
 */
int NUTClockUnitTest::execute(const char *bin, char * const argv[], std::string & out) {
	assert(NULL != argv);
	assert(NULL != argv[0]);

	int   status;
	int   ipc_pipe[2];
	pid_t pid, wpid;
	int   child_exit;

	// Create inter-process comm. pipe
	status = pipe(ipc_pipe);
	CPPUNIT_ASSERT(0 == status);

	// Fork child
	pid = fork();
	if (!pid) {
		// Make the child write to the pipe instead of to stdout
		status = dup2(ipc_pipe[1], 1);
		CPPUNIT_ASSERT(-1 != status);

		// Close pipe reading end
		status = close(ipc_pipe[0]);
		CPPUNIT_ASSERT(0 == status);

		status = execvp(bin, argv);
		CPPUNIT_FAIL((std::string("Failed to execute \"") + bin + "\"").c_str());
	}

	// Close pipe writing end
	status = close(ipc_pipe[1]);
	CPPUNIT_ASSERT(0 == status);

	// Read from pipe
	for (;;) {
		char    buffer[512];
		ssize_t ch_read;

		ch_read = read(ipc_pipe[0], buffer, sizeof(buffer) / sizeof(char));

		if (0 == ch_read)
			break;

		CPPUNIT_ASSERT(ch_read > 0);

		out += std::string(buffer, ch_read);
	}

	// Wait for the child to exit
	wpid = waitpid(pid, &child_exit, 0);
	CPPUNIT_ASSERT(wpid == pid);

	// Close pipe reading end
	status = close(ipc_pipe[0]);
	CPPUNIT_ASSERT(0 == status);

	return child_exit;
}


/**
 *  \brief  Get date via executing an external authority
 *
 *  \param  bin       Binary (absolute/relative/no path allowed)
 *  \param  bin_argv  Command-line arguments for the binary
 *
 *  \return Seconds since the Epoch or 0 in case of error
 */
unsigned NUTClockUnitTest::dateAuth(const char *bin, char * const bin_argv[]) {
	unsigned sec_since_epoch = 0;

	std::string bin_output;
	int         bin_exit;

	bin_exit = execute(bin, bin_argv, bin_output);

	if (0 == bin_exit) {
	    std::stringstream ss(bin_output);

	    ss >> sec_since_epoch;
	    CPPUNIT_ASSERT(0 != sec_since_epoch);
        }

	return sec_since_epoch;
}


/**
 *  \brief  Get date via executing "date +%s"
 *
 *  \return Seconds since the Epoch
 */
unsigned NUTClockUnitTest::date() {
	static char * const date_argv[] = { (char *)"date", (char *)"+%s", NULL };

	unsigned sec_since_epoch;

	sec_since_epoch = dateAuth("date", date_argv);

	return sec_since_epoch;
}


/**
 *  \brief  Get date via executing "perl -e 'print time();'"
 *
 *  \return Seconds since the Epoch
 */
unsigned NUTClockUnitTest::perl_print_time() {
	static char * const perl_argv[] = {
		(char *)"perl", (char *)"-e", (char *)"print time();", NULL };

	unsigned sec_since_epoch;

	sec_since_epoch = dateAuth("perl", perl_argv);

	return sec_since_epoch;
}


/**
 *  \brief  Measure absolute realtime difference
 *
 *  Measure absolute difference between realtime provided by
 *  nut_clock_* iface and an authority (date exec).
 *
 *  \return Absolute realtime difference
 */
double NUTClockUnitTest::realtimeDiff() {
	double diff;

#if (defined NUT_PLATFORM_SOLARIS)
	diff = (double)perl_print_time();
#else
	diff = (double)date();
#endif

	diff -= nut_clock_sec_since_epoch();

	return abs(diff);
}


/**
 *  \brief  Measure average realtime difference
 *
 *  Perform the absolute difference measure via
 *  \ref realtimeDiff
 *  multiple times and return average.
 *
 *  \return Average absolute realtime difference
 */
double NUTClockUnitTest::realtimeDiffAVG() {
	static const unsigned sleep_times[] = { 0, 2, 4, 2, 1, 1 };
	static const unsigned diff_cnt      = sizeof(sleep_times) / sizeof(unsigned);

	// Accumulate realtime difference (takes time)
	double diff = 0.0;

	for (unsigned i = 0; i < diff_cnt; ++i) {
		sleep(sleep_times[i]);

		diff += realtimeDiff();
	}

	return diff /= (double)diff_cnt;
}


/**
 *  \brief  Check average realtime difference
 *
 *  Runs the \ref realtimeDiffAVG and checks whether
 *  the difference is within bounds (10%).
 *  Performs multiple runs if the drift is out of bounds
 *  (to avoid false-positive reports).
 */
void NUTClockUnitTest::realtimeDiffAVGCheck() {
	double diff;

	for (unsigned i = 0; i < 3; ++i) {
		diff = realtimeDiffAVG();

		if (diff < max_absolute_realtime_diff)
			return;
	}

	CPPUNIT_FAIL("Average absolute realtime difference is too high");
}
