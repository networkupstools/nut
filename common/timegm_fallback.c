/* Fallback timegm() for systems that lack one.
 * Algorithm: http://howardhinnant.github.io/date_algorithms.html
 * https://stackoverflow.com/a/58037981/4715872
 */

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

static int days_from_epoch_1970(int y, int m, int d)
{
    y -= m <= 2;
    int era = y / 400;
    int yoe = y - era * 400;                                   // [0, 399]
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
    return era * 146097 + doe - 719468;
}

/* It does not modify broken-down time */
time_t timegm_fallback(struct tm const* t)     
{
    int year = t->tm_year + 1900;
    int month = t->tm_mon;          // 0-11
    int days_since_epoch_1970;

    if (month > 11)
    {
        year += month / 12;
        month %= 12;
    }
    else if (month < 0)
    {
        int years_diff = (11 - month) / 12;
        year -= years_diff;
        month += 12 * years_diff;
    }
    days_since_epoch_1970 = days_from_epoch_1970(year, month + 1, t->tm_mday);

    return 60 * (60 * (24L * days_since_epoch_1970 + t->tm_hour) + t->tm_min) + t->tm_sec;
}

