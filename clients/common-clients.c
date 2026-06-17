#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "common-clients.h"
#include "proto.h"
#ifdef WIN32
#include "wincompat.h"
#endif	/* WIN32 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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


char * read_passwordfile(const char *filename, char *buffer, size_t buffer_size)
{
    FILE *fp;
    char *p;

    fp = fopen(filename, "r");
    if (fp == NULL)
        return NULL;

    p = fgets(buffer, buffer_size, fp);
    if (p)
    {
        /* trim trailing spaces & newlines */
        char *end = buffer + strlen(buffer) - 1;
        while (end > buffer && isspace(*end))
            --end;

        *++end = '\0';
        /* fail if buffer now empty */
        if (*buffer == '\0')
            p = NULL;
    }

    fclose(fp);
    return p;
}
