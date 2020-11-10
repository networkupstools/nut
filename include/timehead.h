/* from the autoconf docs: sanely include the right time headers everywhere */

#ifndef NUT_TIMEHEAD_H_SEEN
#define NUT_TIMEHEAD_H_SEEN 1

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#endif	/* NUT_TIMEHEAD_H_SEEN */
