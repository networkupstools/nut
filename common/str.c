/* str.c - Common string-related functions
 *
 * Copyright (C)
 *   2000 Russell Kroll <rkroll@exploits.org>
 *   2015 Daniele Pezzini <hyouko@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>	/* get the va_* routines */

#include "str.h"

char	*str_trim(char *string, const char character)
{
	return str_rtrim(str_ltrim(string, character), character);
}

char	*str_trim_m(char *string, const char *characters)
{
	return str_rtrim_m(str_ltrim_m(string, characters), characters);
}

char	*str_ltrim(char *string, const char character)
{
	char	characters[2] = { character, '\0' };

	return str_ltrim_m(string, characters);
}

char	*str_ltrim_m(char *string, const char *characters)
{
	if (
		string == NULL ||
		*string == '\0' ||
		characters == NULL ||
		*characters == '\0'
	)
		return string;

	while (
		*string != '\0' &&
		strchr(characters, *string) != NULL
	)
		memmove(string, string + 1, strlen(string));

	return string;
}

char	*str_rtrim(char *string, const char character)
{
	char	characters[2] = { character, '\0' };

	return str_rtrim_m(string, characters);
}

char	*str_rtrim_m(char *string, const char *characters)
{
	char	*ptr;

	if (
		string == NULL ||
		*string == '\0' ||
		characters == NULL ||
		*characters == '\0'
	)
		return string;

	ptr = &string[strlen(string) - 1];

	while (
		ptr >= string &&
		strchr(characters, *ptr) != NULL
	)
		*ptr-- = '\0';

	return string;
}

char	*str_trim_space(char *string)
{
	return str_rtrim_space(str_ltrim_space(string));
}

char	*str_ltrim_space(char *string)
{
	if (
		string == NULL ||
		*string == '\0'
	)
		return string;

	while (
		*string != '\0' &&
		isspace(*string)
	)
		memmove(string, string + 1, strlen(string));

	return string;
}

char	*str_rtrim_space(char *string)
{
	char	*ptr;

	if (
		string == NULL ||
		*string == '\0'
	)
		return string;

	ptr = &string[strlen(string) - 1];

	while (
		ptr >= string &&
		isspace(*ptr)
	)
		*ptr-- = '\0';

	return string;
}

int	str_is_short(const char *string, const int base)
{
	short	number;

	return str_to_short(string, &number, base);
}

int	str_is_short_strict(const char *string, const int base)
{
	short	number;

	return str_to_short_strict(string, &number, base);
}

int	str_is_ushort(const char *string, const int base)
{
	unsigned short	number;

	return str_to_ushort(string, &number, base);
}

int	str_is_ushort_strict(const char *string, const int base)
{
	unsigned short	number;

	return str_to_ushort_strict(string, &number, base);
}

int	str_is_int(const char *string, const int base)
{
	int	number;

	return str_to_int(string, &number, base);
}

int	str_is_int_strict(const char *string, const int base)
{
	int	number;

	return str_to_int_strict(string, &number, base);
}

int	str_is_uint(const char *string, const int base)
{
	unsigned int	number;

	return str_to_uint(string, &number, base);
}

int	str_is_uint_strict(const char *string, const int base)
{
	unsigned int	number;

	return str_to_uint_strict(string, &number, base);
}

int	str_is_long(const char *string, const int base)
{
	long	number;

	return str_to_long(string, &number, base);
}

int	str_is_long_strict(const char *string, const int base)
{
	long	number;

	return str_to_long_strict(string, &number, base);
}

int	str_is_ulong(const char *string, const int base)
{
	unsigned long	number;

	return str_to_ulong(string, &number, base);
}

int	str_is_ulong_strict(const char *string, const int base)
{
	unsigned long	number;

	return str_to_ulong_strict(string, &number, base);
}

int	str_is_double(const char *string, const int base)
{
	double	number;

	return str_to_double(string, &number, base);
}

int	str_is_double_strict(const char *string, const int base)
{
	double	number;

	return str_to_double_strict(string, &number, base);
}

int	str_to_short(const char *string, short *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long(string, &num, base))
		return 0;

	if (
		num < SHRT_MIN ||
		num > SHRT_MAX
	) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_short_strict(const char *string, short *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long_strict(string, &num, base))
		return 0;

	if (
		num < SHRT_MIN ||
		num > SHRT_MAX
	) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_ushort(const char *string, unsigned short *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong(string, &num, base))
		return 0;

	if (num > USHRT_MAX) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_ushort_strict(const char *string, unsigned short *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong_strict(string, &num, base))
		return 0;

	if (num > USHRT_MAX) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_int(const char *string, int *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long(string, &num, base))
		return 0;

	if (
		num < INT_MIN ||
		num > INT_MAX
	) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_int_strict(const char *string, int *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long_strict(string, &num, base))
		return 0;

	if (
		num < INT_MIN ||
		num > INT_MAX
	) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_uint(const char *string, unsigned int *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong(string, &num, base))
		return 0;

	if (num > UINT_MAX) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_uint_strict(const char *string, unsigned int *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong_strict(string, &num, base))
		return 0;

	if (num > UINT_MAX) {
		errno = ERANGE;
		return 0;
	}

	*number = num;
	return 1;
}

int	str_to_long(const char *string, long *number, const int base)
{
	char	*str;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return 0;
	}

	str = strdup(string);
	if (str == NULL)
		return 0;

	str_trim_space(str);

	if (!str_to_long_strict(str, number, base)) {
		free(str);
		return 0;
	}

	free(str);
	return 1;
}

int	str_to_long_strict(const char *string, long *number, const int base)
{
	char	*ptr = NULL;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0' ||
		isspace(*string)
	) {
		errno = EINVAL;
		return 0;
	}

	errno = 0;
	*number = strtol(string, &ptr, base);

	if (
		errno == EINVAL ||
		*ptr != '\0'
	) {
		*number = 0;
		errno = EINVAL;
		return 0;
	}

	if (errno == ERANGE) {
		*number = 0;
		return 0;
	}

	return 1;
}

int	str_to_ulong(const char *string, unsigned long *number, const int base)
{
	char	*str;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return 0;
	}

	str = strdup(string);
	if (str == NULL)
		return 0;

	str_trim_space(str);

	if (!str_to_ulong_strict(str, number, base)) {
		free(str);
		return 0;
	}

	free(str);
	return 1;
}

int	str_to_ulong_strict(const char *string, unsigned long *number, const int base)
{
	char	*ptr = NULL;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0' ||
		*string == '+' ||
		*string == '-' ||
		isspace(*string)
	) {
		errno = EINVAL;
		return 0;
	}

	errno = 0;
	*number = strtoul(string, &ptr, base);

	if (
		errno == EINVAL ||
		*ptr != '\0'
	) {
		*number = 0;
		errno = EINVAL;
		return 0;
	}

	if (errno == ERANGE) {
		*number = 0;
		return 0;
	}

	return 1;
}

int	str_to_double(const char *string, double *number, const int base)
{
	char	*str;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return 0;
	}

	str = strdup(string);
	if (str == NULL)
		return 0;

	str_trim_space(str);

	if (!str_to_double_strict(str, number, base)) {
		free(str);
		return 0;
	}

	free(str);
	return 1;
}

int	str_to_double_strict(const char *string, double *number, const int base)
{
	char	*ptr = NULL;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0' ||
		isspace(*string)
	) {
		errno = EINVAL;
		return 0;
	}

	switch (base)
	{
	case  0:
		break;
	case 10:
		if (strlen(string) != strspn(string, "-+.0123456789Ee")) {
			errno = EINVAL;
			return 0;
		}
		break;
	case 16:
		if (strlen(string) != strspn(string, "-+.0123456789ABCDEFabcdefXxPp")) {
			errno = EINVAL;
			return 0;
		}
		break;
	default:
		errno = EINVAL;
		return 0;
	}

	errno = 0;
	*number = strtod(string, &ptr);

	if (
		errno == EINVAL ||
		*ptr != '\0'
	) {
		*number = 0;
		errno = EINVAL;
		return 0;
	}

	if (errno == ERANGE) {
		*number = 0;
		return 0;
	}

	return 1;
}

/* Based on code by "mmdemirbas" posted "Jul 9 '12 at 11:41" to forum page
 * http://stackoverflow.com/questions/8465006/how-to-concatenate-2-strings-in-c
 * This concatenates the given number of strings into one freshly allocated
 * heap object; NOTE that it is up to the caller to free the object afterwards.
 */
char *	str_concat(size_t count, ...)
{
	va_list ap;
	size_t i, len, null_pos;
	char* merged = NULL;

	/* Find required length to store merged string */
	va_start(ap, count);
	len = 1; /* room for '\0' in the end */
	for(i=0 ; i<count ; i++)
		len += strlen(va_arg(ap, char*));
	va_end(ap);

	/* Allocate memory to concat strings */
	merged = (char*)calloc(len,sizeof(char));
	if (merged == NULL)
		return merged;

	/* Actually concatenate strings */
	va_start(ap, count);
	null_pos = 0;
	for(i=0 ; i<count ; i++)
	{
		char *s = va_arg(ap, char*);
		strcpy(merged+null_pos, s);
		null_pos += strlen(s);
	}
	va_end(ap);

	return merged;
}
