/* usb-common.c - common useful USB functions

   Copyright (C) 2008  Arnaud Quette <arnaud.quette@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "common.h"
#include "usb-common.h"

int is_usb_device_supported(usb_device_id_t *usb_device_id_list, USBDevice_t *device)
{
	int retval = NOT_SUPPORTED;
	usb_device_id_t *usbdev;

	for (usbdev = usb_device_id_list; usbdev->vendorID != -1; usbdev++) {

		if (usbdev->vendorID != device->VendorID) {
			continue;
		}

		/* flag as possibly supported if we see a known vendor */
		retval = POSSIBLY_SUPPORTED;

		if (usbdev->productID != device->ProductID) {
			continue;
		}

		/* call the specific handler, if it exists */
		if (usbdev->fun != NULL) {
			(*usbdev->fun)(device);
		}

		return SUPPORTED;
	}

	return retval;
}

/* ---------------------------------------------------------------------- */
/* matchers */

/* helper function: version of strcmp that tolerates NULL
 * pointers. NULL is considered to come before all other strings
 * alphabetically.
 */
static int strcmp_null(char *s1, char *s2)
{
	if (s1 == NULL && s2 == NULL) {
		return 0;
	}

	if (s1 == NULL) {
		return -1;
	}

	if (s2 == NULL) {
		return 1;
	}

	return strcmp(s1, s2);
}

/* private callback function for exact matches
 */
static int match_function_exact(USBDevice_t *hd, void *privdata)
{
	USBDevice_t	*data = (USBDevice_t *)privdata;

	if (hd->VendorID != data->VendorID) {
		return 0;
	}

	if (hd->ProductID != data->ProductID) {
		return 0;
	}

	if (strcmp_null(hd->Vendor, data->Vendor) != 0) {
		return 0;
	}

	if (strcmp_null(hd->Product, data->Product) != 0) {
		return 0;
	}

	if (strcmp_null(hd->Serial, data->Serial) != 0) {
		return 0;
	}
#ifdef DEBUG_EXACT_MATCH_BUS
	if (strcmp_null(hd->Bus, data->Bus) != 0) {
		return 0;
	}
#endif
	return 1;
}

/* constructor: create an exact matcher that matches the device.
 * On success, return 0 and store the matcher in *matcher. On
 * error, return -1 with errno set
 */
int USBNewExactMatcher(USBDeviceMatcher_t **matcher, USBDevice_t *hd)
{
	USBDeviceMatcher_t	*m;
	USBDevice_t		*data;

	m = malloc(sizeof(*m));
	if (!m) {
		return -1;
	}

	data = calloc(1, sizeof(*data));
	if (!data) {
		free(m);
		return -1;
	}

	m->match_function = &match_function_exact;
	m->privdata = (void *)data;
	m->next = NULL;

	data->VendorID = hd->VendorID;
	data->ProductID = hd->ProductID;
	data->Vendor = hd->Vendor ? strdup(hd->Vendor) : NULL;
	data->Product = hd->Product ? strdup(hd->Product) : NULL;
	data->Serial = hd->Serial ? strdup(hd->Serial) : NULL;
#ifdef DEBUG_EXACT_MATCH_BUS
	data->Bus = hd->Bus ? strdup(hd->Bus) : NULL;
#endif
	*matcher = m;

	return 0;
}

/* destructor: free matcher previously created with USBNewExactMatcher */
void USBFreeExactMatcher(USBDeviceMatcher_t *matcher)
{
	USBDevice_t	*data;

	if (!matcher) {
		return;
	}

	data = (USBDevice_t *)matcher->privdata;

	free(data->Vendor);
	free(data->Product);
	free(data->Serial);
#ifdef DEBUG_EXACT_MATCH_BUS
	free(data->Bus);
#endif
	free(data);
	free(matcher);
}

/* Private function for compiling a regular expression. On success,
 * store the compiled regular expression (or NULL) in *compiled, and
 * return 0. On error with errno set, return -1. If the supplied
 * regular expression is unparseable, return -2 (an error message can
 * then be retrieved with regerror(3)). Note that *compiled will be an
 * allocated value, and must be freed with regfree(), then free(), see
 * regex(3). As a special case, if regex==NULL, then set
 * *compiled=NULL (regular expression NULL is intended to match
 * anything).
 */
static int compile_regex(regex_t **compiled, char *regex, int cflags)
{
	int	r;
	regex_t	*preg;

	if (regex == NULL) {
		*compiled = NULL;
		return 0;
	}

	preg = malloc(sizeof(*preg));
	if (!preg) {
		return -1;
	}

	r = regcomp(preg, regex, cflags);
	if (r) {
		free(preg);
		return -2;
	}

	*compiled = preg;

	return 0;
}

/* Private function for regular expression matching. Check if the
 * entire string str (minus any initial and trailing whitespace)
 * matches the compiled regular expression preg. Return 1 if it
 * matches, 0 if not. Return -1 on error with errno set. Special
 * cases: if preg==NULL, it matches everything (no contraint).  If
 * str==NULL, then it is treated as "".
 */
static int match_regex(regex_t *preg, char *str)
{
	int	r;
	size_t	len = 0;
	char	*string;
	regmatch_t	match;

	if (!preg) {
		return 1;
	}

	if (!str) {
		string = xstrdup("");
	} else {
		/* skip leading whitespace */
		for (len = 0; len < strlen(str); len++) {

			if (!strchr(" \t\n", str[len])) {
				break;
			}
		}

		string = xstrdup(str+len);

		/* skip trailing whitespace */
		for (len = strlen(string); len > 0; len--) {

			if (!strchr(" \t\n", string[len-1])) {
				break;
			}
		}

		string[len] = '\0';
	}

	/* test the regular expression */
	r = regexec(preg, string, 1, &match, 0);
	free(string);
	if (r) {
		return 0;
	}

	/* check that the match is the entire string */
	if ((match.rm_so != 0) || (match.rm_eo != (int)len)) {
		return 0;
	}

	return 1;
}

/* Private function, similar to match_regex, but the argument being
 * matched is a (hexadecimal) number, rather than a string. It is
 * converted to a 4-digit hexadecimal string. */
static int match_regex_hex(regex_t *preg, int n)
{
	char	buf[10];

	snprintf(buf, sizeof(buf), "%04x", n);

	return match_regex(preg, buf);
}

/* private data type: hold a set of compiled regular expressions. */
typedef struct regex_matcher_data_s {
	regex_t	*regex[6];
} regex_matcher_data_t;

/* private callback function for regex matches */
static int match_function_regex(USBDevice_t *hd, void *privdata)
{
	regex_matcher_data_t	*data = (regex_matcher_data_t *)privdata;
	int r;

	r = match_regex_hex(data->regex[0], hd->VendorID);
	if (r != 1) {
		return r;
	}

	r = match_regex_hex(data->regex[1], hd->ProductID);
	if (r != 1) {
		return r;
	}

	r = match_regex(data->regex[2], hd->Vendor);
	if (r != 1) {
		return r;
	}

	r = match_regex(data->regex[3], hd->Product);
	if (r != 1) {
		return r;
	}

	r = match_regex(data->regex[4], hd->Serial);
	if (r != 1) {
		return r;
	}

	r = match_regex(data->regex[5], hd->Bus);
	if (r != 1) {
		return r;
	}
	return 1;
}

/* constructor: create a regular expression matcher. This matcher is
 * based on six regular expression strings in regex_array[0..5],
 * corresponding to: vendorid, productid, vendor, product, serial,
 * bus. Any of these strings can be NULL, which matches
 * everything. Cflags are as in regcomp(3). Typical values for cflags
 * are REG_ICASE (case insensitive matching) and REG_EXTENDED (use
 * extended regular expressions).  On success, return 0 and store the
 * matcher in *matcher. On error, return -1 with errno set, or return
 * i=1--6 to indicate that the regular expression regex_array[i-1] was
 * ill-formed (an error message can then be retrieved with
 * regerror(3)).
 */
int USBNewRegexMatcher(USBDeviceMatcher_t **matcher, char **regex, int cflags)
{
	int	r, i;
	USBDeviceMatcher_t	*m;
	regex_matcher_data_t	*data;

	m = malloc(sizeof(*m));
	if (!m) {
		return -1;
	}

	data = calloc(1, sizeof(*data));
	if (!data) {
		free(m);
		return -1;
	}

	m->match_function = &match_function_regex;
	m->privdata = (void *)data;
	m->next = NULL;

	for (i=0; i<6; i++) {
		r = compile_regex(&data->regex[i], regex[i], cflags);
		if (r == -2) {
			r = i+1;
		}
		if (r) {
			USBFreeRegexMatcher(m);
			return r;
		}
	}

	*matcher = m;

	return 0;
}

void USBFreeRegexMatcher(USBDeviceMatcher_t *matcher)
{
	int	i;
	regex_matcher_data_t	*data;

	if (!matcher) {
		return;
	}

	data = (regex_matcher_data_t *)matcher->privdata;

	for (i = 0; i < 6; i++) {
		if (!data->regex[i]) {
			continue;
		}

		regfree(data->regex[i]);
		free(data->regex[i]);
	}

	free(data);
	free(matcher);
}
