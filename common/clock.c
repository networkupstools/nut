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

#include "clock.h"
#include "common.h"

#include <assert.h>


/* POSIX clock available */
#if (defined USE_POSIX_CLOCK)
	/* Raw monotonic clock is preferred */
	#if (defined USE_LINUX_RAW_MONOTONIC_CLOCK)
		#define POSIX_CLOCK_MONOTONIC_IMPL CLOCK_MONOTONIC_RAW

	/* Standard monotonic clock is available */
	#elif (defined USE_POSIX_MONOTONIC_CLOCK)
		#define POSIX_CLOCK_MONOTONIC_IMPL CLOCK_MONOTONIC
	#endif
#endif


int nut_clock_gettime(nut_clock_mode_t mode, nut_time_t *tm) {
	assert(NULL != tm);

#if defined USE_POSIX_CLOCK
	int posix_clk_st;
	int err_no;

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
				/* BEWARE! intentional fall-through to case RTC */
				mode = RTC;

			/* All other cases are erroneous */
			else return err_no;

	#else  /* POSIX monotonic clock not supported */
		case MONOTONIC:
			return EINVAL;

		case MONOTONIC_PREF:
			/* Fallback to RTC is allowed */
			mode = RTC;

			/* BEWARE! intentional fall-through to case RTC */
	#endif

		case RTC:
			posix_clk_st = clock_gettime(CLOCK_REALTIME, &tm->impl);
			if (0 == posix_clk_st)
				break;  /* OK */

			/* Error is unrecoverable in this case */
			return errno;
	}

#elif defined USE_TIME_T_CLOCK
	/* time_t doesn't support monotonic clock */
	if (MONOTONIC == mode)
		return EINVAL;

	time(&tm->impl);
	mode = RTC;

#else  /* Implementation undecided, code broken */
	#error "Compile-time error: Clock implementation is undecided"
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

#if defined USE_POSIX_CLOCK
	#if (defined POSIX_CLOCK_MONOTONIC_IMPL)
	if (MONOTONIC_PREF == mode)
		mode = MONOTONIC;
	#else
	if (MONOTONIC == mode)
		return EINVAL;

	if (MONOTONIC_PREF == mode)
		mode = RTC;
	#endif

	tm->impl.tv_sec  = 0;
	tm->impl.tv_nsec = 0;

#elif defined USE_TIME_T_CLOCK
	mode = RTC;
	tm->impl = 0;

#else  /* Implementation undecided, code broken */
	#error "Compile-time error: Clock implementation is undecided"
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

#if defined USE_POSIX_CLOCK
	/* Seconds diff */
	diff = difftime(tm1->impl.tv_sec, tm2->impl.tv_sec);

	/* Nanoseconds diff */
	diff += (double)(tm1->impl.tv_nsec - tm2->impl.tv_nsec) / 1000000000.0;

#elif defined USE_TIME_T_CLOCK
	diff = difftime(tm1->impl, tm2->impl);

#else  /* Implementation undecided, code broken */
	#error "Compile-time error: Clock implementation is undecided"
#endif

	return diff;
}


void nut_clock_copytime(nut_time_t *copy, const nut_time_t *orig) {
	assert(NULL != copy);
	assert(NULL != orig);

	copy->mode = orig->mode;

#if defined USE_POSIX_CLOCK
	copy->impl.tv_sec  = orig->impl.tv_sec;
	copy->impl.tv_nsec = orig->impl.tv_nsec;

#elif defined USE_TIME_T_CLOCK
	copy->impl = orig->impl;

#else  /* Implementation undecided, code broken */
	#error "Compile-time error: Clock implementation is undecided"
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
