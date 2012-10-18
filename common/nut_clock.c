/**
 *  \brief  Portable clock
 *
 *  IMPLEMENTATION NOTES:
 *
 *  If any new platform-specific clock is added, please
 *  make sure that the following hold:
 *  1/ Do NOT mix the platform-specific code with any other
 *     platform-specific code (keep things separate and transparent)
 *  2/ Keep semantics intact (make sure that you indeed implement
 *     the described interface so that the behaviour is consistent
 *     for all platforms)
 *
 *  \author  Vaclav Krpec  <VaclavKrpec@Eaton.com>
 *  \date    2012/10/02
 */

#include "nut_clock.h"
#include "common.h"

#include <assert.h>
#include <errno.h>


/* POSIX clock available */
#if (defined USE_POSIX_CLOCK)
	/* Prefere raw monotonic clock on Linux */
	#if (defined NUT_PLATFORM_LINUX && defined CLOCK_MONOTONIC_RAW)
		#define POSIX_CLOCK_MONOTONIC_IMPL CLOCK_MONOTONIC_RAW

	/* BSD-specific monotonic clock */
	#elif (defined NUT_PLATFORM_BSD && defined CLOCK_MONOTONIC_PRECISE)
		#define POSIX_CLOCK_MONOTONIC_IMPL CLOCK_MONOTONIC_PRECISE

	/* Solaris-specific monotonic clock */
	#elif (defined NUT_PLATFORM_SOLARIS && defined CLOCK_HIGHRES)
		#define POSIX_CLOCK_MONOTONIC_IMPL CLOCK_HIGHRES

	/* Use POSIX monotonic clock */
	#elif (defined CLOCK_MONOTONIC)
		#define POSIX_CLOCK_MONOTONIC_IMPL CLOCK_MONOTONIC

	#endif  /* end of platform-specific monotonic clocks selection */

/* Apple Mac OS X: Mach ukernel clocks services are available */
#elif (defined USE_APPLE_MACH_CLOCK)
	/* Based on an info from Apple dev. mailing lists and clock_types.h */
	#if (defined CALENDAR_CLOCK && defined SYSTEM_CLOCK)
		#define MACH_CLOCK_REALTIME_IMPL  CALENDAR_CLOCK
		#define MACH_CLOCK_MONOTONIC_IMPL SYSTEM_CLOCK
	#else
		#error "Compile-time error: Apple Mac OS X / Mach clocks unavailable"
	#endif  /* end of Apple Mac OS X Mach ukernel specific clocks selection */

/* MS Windows FILETIME-based clock */
#elif (defined USE_WINDOWS_CLOCK)
	/* Windows 8, Windows Server 2012 and later */
	#if (defined NUT_PLATFORM_MS_WINDOWS8)
		#define USE_WIN8_CLOCK
	#endif

/* ADD OTHER IMPLEMENTATION-SPECIFIC CHECKS & DEFINITIONS, HERE */
/* #elif () ... */

/* Good old C89 time_t fallback */
#elif (defined USE_TIME_T_CLOCK)
	/* Nothing more to do */

#else  /* Implementation undecided, code broken */
	#error "Compile-time error: Clock implementation is undecided"
#endif  /* end of impl-specific checks & definitions */

/* HP-UX speciality: combining POSIX RTC and non-POSIX monotonic clock */
#if (defined USE_HPUX_GETHRTIME)
	#if (defined USE_POSIX_CLOCK)
		#define USE_HPUX_POSIX_RTC_AND_GETHRTIME_MONOTONIC_CLOCK

	/* No POSIX RTC on HP-UX */
	#else
		/* Let's find out whether such a combination is even possible */
		#error "Compile-time error: HP-UX clock implementation is undecided"
	#endif
#endif  /* end of HP-UX specific definitions */


int nut_clock_gettime(nut_clock_mode_t mode, nut_time_t *tm) {
	assert(NULL != tm);

/* IMPORTANT NOTE: the following case MUST PRECEDE the USE_POSIX_CLOCK case */
#if (defined USE_HPUX_POSIX_RTC_AND_GETHRTIME_MONOTONIC_CLOCK)
	int posix_clk_st;
	hrtime_t nsec;

	switch (mode) {
		case MONOTONIC_PREF:
			/* BEWARE! Intentional fall-through to case MONOTONIC */
			mode = MONOTONIC;

		case MONOTONIC:
			/* Convert gethrtime ns-res. timestamp to struct timespec */
			nsec = gethrtime();

			tm->impl.tv_sec  = (time_t)(nsec / 1000000000);
			tm->impl.tv_nsec = (long)(nsec - (hrtime_t)tm->impl.tv_sec * 1000000000);

			break;

		case RTC:
			posix_clk_st = clock_gettime(CLOCK_REALTIME, &tm->impl);
			if (posix_clk_st)
				return errno;

			break;
	}

#elif (defined USE_POSIX_CLOCK)
	int posix_clk_st;
	#if (defined POSIX_CLOCK_MONOTONIC_IMPL)
	int err_no;
	#endif

	switch (mode) {
	#if (defined POSIX_CLOCK_MONOTONIC_IMPL)
		case MONOTONIC:
		case MONOTONIC_PREF:
			posix_clk_st = clock_gettime(POSIX_CLOCK_MONOTONIC_IMPL, &tm->impl);
			if (0 == posix_clk_st) {
				/* We have monotonic clock */
				mode = MONOTONIC;
				break;
			}

			/* An error occurred */
			err_no = errno;

			/* Try falling back to RTC if allowed */
			if (EINVAL == err_no && MONOTONIC_PREF == mode)
				/* BEWARE! Intentional fall-through to case RTC */
				mode = RTC;

			/* All other cases are erroneous */
			else return err_no;

	#else  /* POSIX monotonic clock not supported */
		case MONOTONIC:
			return EINVAL;

		case MONOTONIC_PREF:
			/* Fallback to RTC is allowed */
			mode = RTC;

			/* BEWARE! Intentional fall-through to case RTC */
	#endif

		case RTC:
			posix_clk_st = clock_gettime(CLOCK_REALTIME, &tm->impl);
			if (0 == posix_clk_st)
				break;  /* OK */

			/* Error is unrecoverable in this case */
			return errno;
	}

#elif (defined USE_APPLE_MACH_CLOCK)
	kern_return_t mach_st;
	clock_id_t    clock_id = MACH_CLOCK_REALTIME_IMPL;
	clock_serv_t  clock;

	switch (mode) {
		case RTC:
			break;

		case MONOTONIC_PREF:
			/* BEWARE! Intentional fall-through to case MONOTONIC */
			mode = MONOTONIC;

		case MONOTONIC:
			clock_id = MACH_CLOCK_MONOTONIC_IMPL;

			break;
	}

	/* Get clock service port (should be there, no excuses) */
	mach_st = host_get_clock_service(mach_host_self(), clock_id, &clock);
	if (KERN_SUCCESS != mach_st)
		return EFAULT;

	/* Get time-stamp (must not fail) */
	mach_st = clock_get_time(clock, &tm->impl);
	if (KERN_SUCCESS != mach_st)
		return EFAULT;

	/* Cleanup (must not fail) */
	mach_st = mach_port_deallocate(mach_task_self(), clock);
	if (KERN_SUCCESS != mach_st)
		return EFAULT;

#elif (defined USE_WINDOWS_CLOCK)
	/* Monotonic clock support not implemented (don't know how) */
	if (MONOTONIC == mode)
		return EINVAL;

	#if (defined USE_WIN8_CLOCK)
	GetSystemTimePreciseAsFileTime(&tm->impl);
	#else
	GetSystemTimeAsFileTime(&tm->impl);
	#endif

	mode = RTC;

#elif (defined USE_TIME_T_CLOCK)
	/* time_t doesn't support monotonic clock */
	if (MONOTONIC == mode)
		return EINVAL;

	time(&tm->impl);
	mode = RTC;

#else
	#error "Compile-time error: nut_clock_gettime: platform-specific implementation missing"
#endif

	/* Keep mode flag for safety reasons */
	tm->mode = mode;

	return 0;
}


void nut_clock_gettimex(nut_clock_mode_t mode, nut_time_t *tm) {
	int clock_st = nut_clock_gettime(mode, tm);
	if (clock_st)
		fatalx(EXIT_FAILURE, "Failed to get time stamp (mode %d): %d", mode, clock_st);
}


int nut_clock_mintime(nut_clock_mode_t mode, nut_time_t *tm) {
	assert(NULL != tm);

#if (defined USE_POSIX_CLOCK)
	#if (defined POSIX_CLOCK_MONOTONIC_IMPL)
	if (MONOTONIC_PREF == mode)
		mode = MONOTONIC;
	#else
	if (MONOTONIC == mode)
		return EINVAL;

	mode = RTC;
	#endif

	tm->impl.tv_sec  = 0;
	tm->impl.tv_nsec = 0;

#elif (defined USE_APPLE_MACH_CLOCK)
	if (MONOTONIC_PREF == mode)
		mode = MONOTONIC;

	tm->impl.tv_sec  = 0;
	tm->impl.tv_nsec = 0;

#elif (defined USE_WINDOWS_CLOCK)
	if (MONOTONIC == mode)
		return EINVAL;

	mode = RTC;

	tm->impl.dwHighDateTime = 0;
	tm->impl.dwLowDateTime  = 0;

#elif (defined USE_TIME_T_CLOCK)
	if (MONOTONIC == mode)
		return EINVAL;

	mode = RTC;
	tm->impl = 0;

#else
	#error "Compile-time error: nut_clock_mintime: platform-specific implementation missing"
#endif

	/* Keep mode flag for safety reasons */
	tm->mode = mode;

	return 0;
}


void nut_clock_mintimex(nut_clock_mode_t mode, nut_time_t *tm) {
	int clock_st = nut_clock_mintime(mode, tm);
	if (clock_st)
		fatalx(EXIT_FAILURE, "Failed to set minimal time stamp (mode %d): %d", mode, clock_st);
}


double nut_clock_difftime(const nut_time_t *tm1, const nut_time_t *tm2) {
	double     diff;
	nut_time_t now;

#if (defined USE_WINDOWS_CLOCK)
	ULONGLONG hns1;
	ULONGLONG hns2;
#endif

	/* Make sure that no argument is missing */
	if (NULL == tm1) {
		if (NULL == tm2)
			return 0.0;  /* now is exactly 0 secs from now */

		nut_clock_gettimex(tm2->mode, &now);
		tm1 = &now;
	}
	else if (NULL == tm2) {
		nut_clock_gettimex(tm1->mode, &now);
		tm2 = &now;
	}

	/* Sanity checks */
	assert(NULL != tm1);
	assert(NULL != tm2);
	assert(tm1->mode == tm2->mode);

#if (defined USE_POSIX_CLOCK)
	/* Seconds diff */
	diff = difftime(tm1->impl.tv_sec, tm2->impl.tv_sec);

	/* Nanoseconds diff */
	diff += (double)(tm1->impl.tv_nsec - tm2->impl.tv_nsec) / 1000000000.0;

#elif (defined USE_APPLE_MACH_CLOCK)
	/* Seconds diff (seconds are unsigned int) */
	diff = (double)((long long int)(tm1->impl.tv_sec) - (long long int)(tm2->impl.tv_sec));

	/* Nanoseconds diff (ns are int) */
	diff += (double)(tm1->impl.tv_nsec - tm2->impl.tv_nsec) / 1000000000.0;

#elif (defined USE_WINDOWS_CLOCK)
	/* Apparently, result of the following is hecto-nanoseconds clock resolution */
	hns1 = ((ULONGLONG)tm1->impl.dwHighDateTime << 32) | (ULONGLONG)tm1->impl.dwLowDateTime;
	hns2 = ((ULONGLONG)tm2->impl.dwHighDateTime << 32) | (ULONGLONG)tm2->impl.dwLowDateTime;

	/* Be careful with unsigned subtraction */
	if (hns1 > hns2) {
		diff = hns1 - hns2;
	}
	else {
		diff = hns2 - hns1;
		diff = -diff;
	}

	diff /= 10000000.0;

#elif (defined USE_TIME_T_CLOCK)
	diff = difftime(tm1->impl, tm2->impl);

#else
	#error "Compile-time error: nut_clock_difftime: platform-specific implementation missing"
#endif

	return diff;
}


void nut_clock_copytime(nut_time_t *copy, const nut_time_t *orig) {
	assert(NULL != copy);
	assert(NULL != orig);

	copy->mode = orig->mode;

#if (defined USE_POSIX_CLOCK)
	copy->impl.tv_sec  = orig->impl.tv_sec;
	copy->impl.tv_nsec = orig->impl.tv_nsec;

#elif (defined USE_APPLE_MACH_CLOCK)
	copy->impl.tv_sec  = orig->impl.tv_sec;
	copy->impl.tv_nsec = orig->impl.tv_nsec;

#elif (defined USE_WINDOWS_CLOCK)
	copy->impl.dwHighDateTime = orig->impl.dwHighDateTime;
	copy->impl.dwLowDateTime  = orig->impl.dwLowDateTime;

#elif (defined USE_TIME_T_CLOCK)
	copy->impl = orig->impl;

#else
	#error "Compile-time error: nut_clock_copytime: platform-specific implementation missing"
#endif
}


int nut_clock_cmptime_sigma(const nut_time_t *tm1, const nut_time_t *tm2, double sigma) {
	assert(0.0 <= sigma);

	double diff = nut_clock_difftime(tm1, tm2);

	if (diff < -sigma)
		return -1;
	if (diff >  sigma)
		return  1;

	return 0;
}


int nut_clock_cmptime(const nut_time_t *tm1, const nut_time_t *tm2) {
	return nut_clock_cmptime_sigma(tm1, tm2, 0.0);
}
