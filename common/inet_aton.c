/* inet_aton.c Ben Collver <collver@softhome.net> */
/* This works if inet_addr exists like it does in CYGWIN */
#include <stddef.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long)-1)
#endif

/* Solaris 5.x defines extern inet_aton() in /usr/include/arpa/inet.h
   However, inet_aton is not in the C library!?  Solaris has inet_addr()
   but does not define INADDR_NONE
 */

#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

int inet_aton(const char *cp, struct in_addr *addr)
{
	unsigned long	retval;

	if (addr == NULL || cp == NULL)
		return 0;
	retval = inet_addr(cp);
	if (retval == INADDR_NONE) {
		return 0;
	}
	addr->s_addr = retval;
	return 1;
}
