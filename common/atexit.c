/* atexit()  Mark Powell <medp@primagraphics.co.uk> */
/* Implemented in terms of on_exit() for old BSD-style systems, like SunOS4 */

#include "config.h"

#ifndef HAVE_ATEXIT

#include <errno.h>
#include "common.h"

int atexit(fn)
    void (*fn)();
{
#ifdef HAVE_ON_EXIT
    return on_exit(fn, 0);
#else
    NUT_UNUSED_VARIABLE(fn);
    /* Choose some errno thats likely to exist on lots of systems */
    errno = EPERM;
    return (-1);
#endif /* HAVE_ON_EXIT */
}

#endif /* HAVE_ATEXIT */
