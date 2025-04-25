#include <string.h>

/* Simple implem courtesy of https://stackoverflow.com/a/58244503
 * Note: like the (BSD) standard implem, this changes the original stringp!
 * Result is undefined (segfault here) if stringp==NULL
 * (it is supposed to be address of string after all)
 */
char *strsep(char **stringp, const char *delim) {
	char *rv = *stringp;
	if (rv) {
		*stringp += strcspn(*stringp, delim);
		if (**stringp)
			*(*stringp)++ = '\0';
		else
			*stringp = NULL;
	}
	return rv;
}

