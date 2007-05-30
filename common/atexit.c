/* atexit()  Mark Powell <medp@primagraphics.co.uk> */
/* Implemented in terms of on_exit() for old BSD-style systems, like SunOS4 */

#include "config.h"

#ifndef HAVE_ATEXIT

#include <errno.h>

int atexit(fn)
    void (*fn)();
{
#ifdef HAVE_ON_EXIT
    return on_exit(fn, 0);
#else
    /* Choose some errno thats likely to exist on lots of systems */
    errno = EPERM;
    return (-1);
#endif /* HAVE_ON_EXIT */
}

#endif /* HAVE_ATEXIT */
