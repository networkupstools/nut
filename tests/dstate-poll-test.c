/* dstate-poll-test.c - driver polling clock and retry regression tests
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "common.h"
#include "main.h"
#include "dstate.h"

static struct timeval wall_now;
static st_tree_timespec_t monotonic_now;
static double waited;
static int monotonic_error, monotonic_reads, failures;
#ifndef WIN32
static int wait_result;
#endif

static int poll_test_gettimeofday(struct timeval *now, void *tz)
{
	NUT_UNUSED_VARIABLE(tz);
	*now = wall_now;
	return 0;
}

static int poll_test_timestamp(st_tree_timespec_t *now)
{
	monotonic_reads++;
	if (monotonic_error) {
		errno = EIO;
		return -1;
	}
	*now = monotonic_now;
	return 0;
}

#ifndef WIN32
static int poll_test_select(int nfds, fd_set *readfds, fd_set *writefds,
	fd_set *exceptfds, struct timeval *timeout)
{
	NUT_UNUSED_VARIABLE(nfds);
	NUT_UNUSED_VARIABLE(writefds);
	NUT_UNUSED_VARIABLE(exceptfds);
	waited = timeout->tv_sec + timeout->tv_usec / 1000000.0;
	FD_ZERO(readfds);
	if (wait_result < 0) {
		errno = EINTR;
	}
	return wait_result;
}
# define select poll_test_select
#else	/* WIN32 */
static DWORD poll_test_wait(DWORD count, const HANDLE *handles,
	BOOL all, DWORD timeout)
{
	NUT_UNUSED_VARIABLE(count);
	NUT_UNUSED_VARIABLE(handles);
	NUT_UNUSED_VARIABLE(all);
	waited = timeout / 1000.0;
	return WAIT_TIMEOUT;
}
# define WaitForMultipleObjects poll_test_wait
#endif	/* WIN32 */

#define gettimeofday poll_test_gettimeofday
#define state_get_timestamp poll_test_timestamp
#include "../drivers/dstate.c"
#undef gettimeofday
#undef state_get_timestamp
#ifndef WIN32
# undef select
#else
# undef WaitForMultipleObjects
#endif

upsdrv_info_t upsdrv_info = {
	"Polling test driver", "0.01", "NUT developers", DRV_EXPERIMENTAL, { NULL }
};
void upsdrv_cleanup(void) {}
void upsdrv_shutdown(void) {}
void upsdrv_initups(void) {}
void upsdrv_initinfo(void) {}
void upsdrv_makevartable(void) {}
void upsdrv_tweak_prognames(void) {}
void upsdrv_updateinfo(void) {}
void upsdrv_help(void) {}

static void check(int condition, const char *message)
{
	upslogx(condition ? LOG_INFO : LOG_ERR, "%s: %s", condition ? "PASS" : "FAIL", message);
	if (!condition) {
		failures++;
	}
}

static void clocks(time_t wall, time_t monotonic, long microseconds)
{
	wall_now.tv_sec = wall;
	wall_now.tv_usec = microseconds;
	monotonic_now.tv_sec = monotonic;
#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
	monotonic_now.tv_nsec = microseconds * 1000;
#else
	monotonic_now.tv_usec = microseconds;
#endif
}

static void check_wait(dstate_poll_t *poll, double expected, int result, const char *message)
{
	int actual = dstate_poll_fds(poll, ERROR_FD);
#ifndef WIN32
	double tolerance = 0.000002;
#else
	double tolerance = 0.002;
#endif
	check(actual == result && waited >= expected - tolerance
		&& waited <= expected + tolerance, message);
}

int main(void)
{
	dstate_poll_t poll;
	int reads;

#ifndef WIN32
	/* Only the wait seam uses this descriptor, never the real OS. */
	sockfd = 0;
#endif
	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(1000, 100, 750000);
	check_wait(&poll, 1.75, 1, "ordinary fractional wait");
	clocks(1002, 102, 250000);
	check_wait(&poll, 0.25, 1, "subsecond deadline");

#ifndef WIN32
	wait_result = -1;
	clocks(1001, 101, 0);
	check_wait(&poll, 1.5, 0, "interrupted wait retains original deadline");
	clocks(1002, 102, 500000);
	check_wait(&poll, 0, 1, "interrupted wait at exact expiry ends poll cycle");
	wait_result = 1;
	check_wait(&poll, 0, 1, "readiness return at expiry cannot postpone next update");
	wait_result = 0;
#endif

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	check(poll.use_monotonic, "monotonic provider selected");
	clocks(1004, 100, 750000);
	check_wait(&poll, 1.75, 1, "small forward wall adjustment does not shorten wait");
	clocks(996, 100, 750000);
	check_wait(&poll, 1.75, 1, "small backward wall adjustment does not extend wait");
	clocks(1005, 100, 750000);
	check_wait(&poll, 1.75, 1, "positive five-second tolerance boundary");
	wall_now.tv_usec--;
	check_wait(&poll, 1.75, 1, "positive jump just inside tolerance keeps waiting");
	wall_now.tv_usec++;
	monotonic_now.tv_nsec++;
	check_wait(&poll, 1.75, 1, "positive nanosecond inside tolerance keeps waiting");
	clocks(995, 100, 750000);
	check_wait(&poll, 1.75, 1, "negative five-second tolerance boundary");
	wall_now.tv_usec++;
	check_wait(&poll, 1.75, 1, "negative jump just inside tolerance keeps waiting");
	wall_now.tv_usec--;
	monotonic_now.tv_nsec--;
	check_wait(&poll, 1.75, 1, "negative nanosecond inside tolerance keeps waiting");
	monotonic_now.tv_nsec++;
	wall_now.tv_usec--;
	check_wait(&poll, 0, 1, "negative jump just outside tolerance requests fresh data");
	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(1005, 100, 750000);
	wall_now.tv_usec++;
	check_wait(&poll, 0, 1, "positive jump just outside tolerance requests fresh data");
	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(1005, 100, 750000);
	monotonic_now.tv_nsec--;
	check_wait(&poll, 0, 1, "positive nanosecond outside tolerance requests fresh data");
	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(995, 100, 750000);
	monotonic_now.tv_nsec++;
	check_wait(&poll, 0, 1, "negative nanosecond outside tolerance requests fresh data");
	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(1600, 100, 750000);
	check_wait(&poll, 0, 1, "large forward jump or suspend requests fresh data");
	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(400, 100, 750000);
	check_wait(&poll, 0, 1, "large backward jump requests fresh data");

	clocks(1000, 100, 500000);
	dstate_poll_start(&poll, 2);
	monotonic_error = 1;
	check_wait(&poll, 0, 1, "runtime monotonic failure requests fresh data");
	reads = monotonic_reads;
	monotonic_error = 0;
	clocks(2000, 300, 500000);
	dstate_poll_start(&poll, 2);
	check(!poll.use_monotonic, "next poll falls back to wall time");
	clocks(2000, 900, 750000);
	check_wait(&poll, 1.75, 1, "fallback uses new wall epoch only");
	check(monotonic_reads == reads, "failed provider is not retried");

	poll_monotonic_failed = 0;
	monotonic_error = 1;
	clocks(3000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(3000, 100, 750000);
	check_wait(&poll, 1.75, 1, "initial monotonic failure uses wall time");
#else
	reads = monotonic_reads;
	check(!poll.use_monotonic && reads == 0, "build without monotonic uses wall time");
#endif

	clocks(4000, 100, 500000);
	dstate_poll_start(&poll, 2);
	clocks(4001, 100, 0);
	check_wait(&poll, 1.5, 1, "wall-only positive elapsed time follows wall clock");
	clocks(3999, 100, 500000);
	check_wait(&poll, 0, 1, "wall clock before interval start requests fresh data");
	clocks(4000, 100, 500000);
	dstate_poll_start(&poll, 0);
	check_wait(&poll, 0, 1, "zero interval still services descriptors without waiting");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
