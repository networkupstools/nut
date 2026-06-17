#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "common-clients.h"

#if defined(HAVE_BSD_READPASSPHRASE_H)
# include <bsd/readpassphrase.h>
#elif defined(HAVE_READPASSPHRASE_H)
# include <readpassphrase.h>
#elif !defined(WIN32) && defined(HAVE_UNISTD_H)
# include <unistd.h>	/* getpass */
#endif


char * read_password(char *buffer, size_t buffer_size)
{
#if defined(HAVE_READPASSPHRASE_H) || defined(HAVE_BSD_READPASSPHRASE_H)
    return readpassphrase("Password: ", buffer, buffer_size, RPP_ECHO_OFF);
#else
    /* fallback to getpass, win32 has a version in wincompat.c */

	char *pwtmp = GETPASS("Password: ");
    if (pwtmp == NULL)
        return NULL;

    strncpy(buffer, pwtmp, buffer_size);
    buffer[buffer_size - 1] = '\0';

    memset(pwtmp, 0, strlen(pwtmp));
    return buffer;
#endif
}
