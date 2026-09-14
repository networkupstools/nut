/*!
 * @file strcasestr-static.h
 * @brief Fallback implementation of strcasestr() as a static method included
 *        into a few sources on a need-to-know basis
 *
 * @author Copyright (C)
 *  2022 - 2026 Jim Klimov <jimklimov+nut@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

#ifndef NUT_STRCASESTR_STATIC_H_SEEN
#define NUT_STRCASESTR_STATIC_H_SEEN 1

#include "config.h"	/* Did configure script discover what we miss and need? */

# if (!(defined(HAVE_STRCASESTR) && HAVE_STRCASESTR)) && (HAVE_STRSTR && HAVE_STRLWR && HAVE_STRDUP)
/** Only used in a few file of all NUT codebase (libusb0.c, upsclient.c),
 * so not published in str.{c,h} where it happens to conflict with an
 * optional netsnmp-provided variant for some of our build products.
 * Here it is just included into the few "victims".
 */
static char *strcasestr(const char *haystack, const char *needle)
{
	/* work around "const char *" and guarantee the original is not
	 * touched... not efficient but we have few uses for this method */
	char * dH = NULL, *dN = NULL, *lH = NULL, *lN = NULL, *first = NULL;

	dH = strdup(haystack);
	if (dH == NULL) goto err;
	dN = strdup(needle);
	if (dN == NULL) goto err;
	lH = strlwr(dH);
	if (lH == NULL) goto err;
	lN = strlwr(dN);
	if (lN == NULL) goto err;
	first = strstr(lH, lN);

err:
	if (dH != NULL) free(dH);
	if (dN != NULL) free(dN);
	/* Does this implementation of strlwr() change original buffer? */
	if (lH != dH && lH != NULL) free(lH);
	if (lN != dN && lN != NULL) free(lN);
	if (first == NULL) {
		return NULL;
	}

	/* Pointer to first char of the needle found in original haystack */
	return (char *)(haystack + (first - lH));
}

# ifdef HAVE_STRCASESTR
#  undef HAVE_STRCASESTR
# endif
# define HAVE_STRCASESTR 1

# endif	/* (!HAVE_STRCASESTR) && (HAVE_STRSTR && HAVE_STRLWR && HAVE_STRDUP) */

#endif	/* NUT_STRCASESTR_STATIC_H_SEEN */
