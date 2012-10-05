#ifndef nut_common_clock_h
#define nut_common_clock_h

/**
 *  \brief  Portable clock
 *
 *  The module provides portable clock implementation.
 *
 *  The main reason of creation of this module is to provide
 *  possibility of usage of monotonic clock (if available
 *  on the platform).
 *  Since different platforms use different implementations,
 *  the unification and encapsulation is provided by this
 *  module.
 *
 *  \author  Vaclav Krpec  <VaclavKrpec@Eaton.com>
 *  \date    2012/10/02
 */

#include <unistd.h>
#include <time.h>


/* POSIX clock (clock_*time functions) available */
#if (defined _POSIX_TIMERS && 0 < _POSIX_TIMERS)
	#define USE_POSIX_CLOCK

/* ADD OTHER IMPLEMENTATION-SPECIFIC CHECKS & DEFINITIONS, HERE */
/* #elif () ... */

/* Good old C89 time_t fallback */
#else
	#define USE_TIME_T_CLOCK
#endif  /* end of clock implementation selection */


/** Clock mode */
typedef enum {
	RTC,		/**< Real-time clock (best implementation available)    */
	MONOTONIC_PREF,	/**< Monotonic clock (preferred, RTC fallback accepted) */
	MONOTONIC,	/**< Monotonic clock (enforced)                         */
} nut_clock_mode_t;  /* end of typedef enum */


typedef struct nut_time nut_time_t;	/** Time specification */

/** Implementation of time specification */
struct nut_time {
	nut_clock_mode_t	mode;	/**< Clock mode              */
#if defined USE_POSIX_CLOCK
	struct timespec		impl;	/**< POSIX implementation    */
#elif defined USE_TIME_T_CLOCK
	time_t			impl;	/**< Fallback implementation */
#endif
};  /* end of struct nut_time */


/**
 *  \brief  Get time stamp
 *
 *  The macro expands to \ref nut_clock_gettimex call
 *  using the \c MONOTONIC_PREF clock mode to request
 *  current time stamp (ideally from monotonic clock).
 *  Typical usage is for measuring time that passed since
 *  a certain moment (see \ref nut_clock_sec_since).
 *
 *  \param  tm  Time stamp
 */
#define nut_clock_timestamp(tm) nut_clock_gettimex(MONOTONIC_PREF, (tm))


/**
 *  \brief  Set minimal time stamp
 *
 *  The macro expands to \ref nut_clock_mintimes call
 *  using the \c MONOTONIC_PREF clock mode to set
 *  minimal time stamp (ideally from monotonic clock).
 *  Typical usage is for initialisation of a timer
 *  that should expire the very 1st time it's checked
 *  and is set to current time stamp at every expiration.
 *
 *  \param  tm  Time stamp
 */
#define nut_clock_mintimestamp(tm) nut_clock_mintimex(MONOTONIC_PREF, (tm))


/**
 *  \brief  Get time passed since a time stamp
 *
 *  The macro expands to difference of current time
 *  and a provided time stamp.
 *  See \ref nut_clock_difftime for more info.
 *
 *  Typical usage:
 *  \code
 *
 *  nut_time_t time_stamp;
 *  nut_clock_timestamp(time_stamp);
 *
 *  // Time flies... (but thou shalt not use // since we're plain old C ;-)
 *
 *  if (nut_clock_sec_since(time_stamp) >= time_offset) {
 *      // Execute scheduled action
 *  }
 *
 *  \endcode
 *
 *  \param  tm  Time stamp
 *
 *  \return Seconds passed since \c tm (generally with floating point)
 */
#define nut_clock_sec_since(tm) nut_clock_difftime(NULL, (tm))


/**
 *  \brief  Get current time stamp
 *
 *  Note that the provided time stamp resolution depends on the actually
 *  used implementation of the clock.
 *
 *  \param  mode  Clock mode
 *  \param  tm    Provided time
 *
 *  \retval 0      in case of success
 *  \retval EINVAL if clock mode isn't supported
 *  \retval EFAULT in case of internal error
 */
int nut_clock_gettime(nut_clock_mode_t mode, nut_time_t *tm);


/**
 *  \brief  Get current time stamp or die
 *
 *  The function wraps around \ref nut_clock_gettime so that if
 *  the time stamp can't be obtained, a fatal message is logged
 *  and the process aborts.
 *  Note that this should never happen for \c mode of \c RTC
 *  or \c MONOTONIC_PREF, since the RTC should always be supported
 *  and \c MONOTONIC_PREF may fall-back to RTC if monotonic clock
 *  isn't available.
 *
 *  \param  mode  Clock mode
 *  \param  tm    Provided time
 */
void nut_clock_gettimex(nut_clock_mode_t mode, nut_time_t *tm);


/**
 *  \brief  Get minimal time stamp
 *
 *  The function sets the time stamp so that its time specification
 *  is minimal in terms of \ref nut_clock_difftime
 *  (i.e. \c nut_clock_difftime(min,any) <= 0.0).
 *
 *  The function should be used if the time stamp isn't initialised
 *  directly with \ref nut_clock_gettime (or its warpper).
 *
 *  \param  mode  Clock mode
 *  \param  tm    Time stamp
 *
 *  \retval 0      in case of success
 *  \retval EINVAL if clock mode isn't supported
 */
int nut_clock_mintime(nut_clock_mode_t mode, nut_time_t *tm);


/**
 *  \brief  Get minimal time stamp or die
 *
 *  The function wraps around \ref nut_clock_mintime so that if
 *  the time stamp can't be obtained, a fatal message is logged
 *  and the process aborts.
 *  The same as for \ref nut_clock_gettimex applies.
 *
 *  \param  mode  Clock mode
 *  \param  tm    Time stamp
 */
void nut_clock_mintimex(nut_clock_mode_t mode, nut_time_t *tm);


/**
 *  \brief  Compute time difference
 *
 *  The function returns difference (in seconds) between
 *  two time points as if they were subtracted (2nd from the 1st):
 *  result = \c tm1 - \c tm2
 *
 *  Note that either argument may be omitted (i.e. == \c NULL in C).
 *  In that case, the function shall behave as if the argument
 *  was containing current time obtained using the same clock mode
 *  as the other.
 *  This should always be possible (since it was done before).
 *  If the arguments are of different modes, the function shall abort
 *  the process since such usage is logically erroneous.
 *
 *  Typical usage would therefore be:
 *  \code
 *
 *  nut_time_t time_stamp;
 *
 *  nut_clock_gettimex(MONOTONIC_PREF, &time_stamp);
 *
 *  // Time flies...
 *
 *  // Get positive time that passed since time_stamp was taken (in seconds)
 *  double sec_spent = nut_clock_difftime(NULL, time_stamp);
 *
 *  \endcode
 *
 *  \param  tm1  Time stamp
 *  \param  tm2  Time stamp
 *
 *  \return Difference between the time stamps in seconds
 */
double nut_clock_difftime(const nut_time_t *tm1, const nut_time_t *tm2);


/**
 *  \brief  Copy time stamp
 *
 *  \c copy = \c orig
 *
 *  \param  copy  Copying target
 *  \param  orig  Copying source
 */
void nut_clock_copytime(nut_time_t *copy, const nut_time_t *orig);


/**
 *  \brief  Compare time stamps (loosely)
 *
 *  The function simply calls \ref nut_clock_difftime
 *  and returns -1, 0 or 1 depending on whether the
 *  returned time stamp difference is less then \c -sigma,
 *  within the 0-centered \c sigma interval
 *  or greater than \c sigma, respectively.
 *
 *  Note that for the arguments, everything applying to
 *  \ref nut_clock_difftime applies as well.
 *
 *  Also note that for \c sigma = 0.0, this function becomes
 *  a standard comparison function (the restriction wrapper
 *  is already provided: \ref nut_clock_cmptime.
 *
 *  \param  tm1    Time stamp
 *  \param  tm2    Time stamp
 *  \param  sigma  Sigma parameter (defines the comparison precision)
 *
 *  \retval -1 if \c tm1 <  \c tm2 (except for difference less than \c sigma)
 *  \retval  0 if \c tm1 == \c tm2 (except for difference less than \c sigma)
 *  \retval  1 if \c tm1 >  \c tm2 (except for difference less than \c sigma)
 */
int nut_clock_cmptime_sigma(const nut_time_t *tm1, const nut_time_t *tm2, double sigma);


/**
 *  \brief  Compare time stamps (strictly)
 *
 *  The function wraps around \ref nut_clock_cmptime_sigma,
 *  fixing the \c sigma parameter to 0.0.
 *  (i.e. works as a standard semantics comparison
 *  function).
 *
 *  Also note that for time stamps with high resolution,
 *  comparison such as this might be too strict;
 *  you might want to consider \ref nut_clock_cmptime_sigma
 *  function, instead.
 *
 *  \param  tm1  Time stamp
 *  \param  tm2  Time stamp
 *
 *  \retval -1 if \c tm1 <  \c tm2
 *  \retval  0 if \c tm1 == \c tm2
 *  \retval  1 if \c tm1 >  \c tm2
 */
int nut_clock_cmptime(const nut_time_t *tm1, const nut_time_t *tm2);

#endif  /* end of #ifndef nut_common_clock_h */
