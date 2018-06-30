/**
 * @file
 * @brief	Actual implementation of our common string-related functions.
 * @copyright	@parblock
 * Copyright (C):
 * - 2000      -- Russell Kroll (<rkroll@exploits.org>)
 * - 2015-2018 -- Daniele Pezzini (<hyouko@gmail.com>)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *		@endparblock
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

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

bool_t	str_is_short(const char *string, const int base)
{
	short	number;

	return str_to_short(string, &number, base);
}

bool_t	str_is_short_strict(const char *string, const int base)
{
	short	number;

	return str_to_short_strict(string, &number, base);
}

bool_t	str_is_ushort(const char *string, const int base)
{
	unsigned short	number;

	return str_to_ushort(string, &number, base);
}

bool_t	str_is_ushort_strict(const char *string, const int base)
{
	unsigned short	number;

	return str_to_ushort_strict(string, &number, base);
}

bool_t	str_is_int(const char *string, const int base)
{
	int	number;

	return str_to_int(string, &number, base);
}

bool_t	str_is_int_strict(const char *string, const int base)
{
	int	number;

	return str_to_int_strict(string, &number, base);
}

bool_t	str_is_uint(const char *string, const int base)
{
	unsigned int	number;

	return str_to_uint(string, &number, base);
}

bool_t	str_is_uint_strict(const char *string, const int base)
{
	unsigned int	number;

	return str_to_uint_strict(string, &number, base);
}

bool_t	str_is_long(const char *string, const int base)
{
	long	number;

	return str_to_long(string, &number, base);
}

bool_t	str_is_long_strict(const char *string, const int base)
{
	long	number;

	return str_to_long_strict(string, &number, base);
}

bool_t	str_is_ulong(const char *string, const int base)
{
	unsigned long	number;

	return str_to_ulong(string, &number, base);
}

bool_t	str_is_ulong_strict(const char *string, const int base)
{
	unsigned long	number;

	return str_to_ulong_strict(string, &number, base);
}

bool_t	str_is_double(const char *string, const int base)
{
	double	number;

	return str_to_double(string, &number, base);
}

bool_t	str_is_double_strict(const char *string, const int base)
{
	double	number;

	return str_to_double_strict(string, &number, base);
}

bool_t	str_to_short(const char *string, short *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long(string, &num, base))
		return FALSE;

	if (
		num < SHRT_MIN ||
		num > SHRT_MAX
	) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_short_strict(const char *string, short *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long_strict(string, &num, base))
		return FALSE;

	if (
		num < SHRT_MIN ||
		num > SHRT_MAX
	) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_ushort(const char *string, unsigned short *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong(string, &num, base))
		return FALSE;

	if (num > USHRT_MAX) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_ushort_strict(const char *string, unsigned short *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong_strict(string, &num, base))
		return FALSE;

	if (num > USHRT_MAX) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_int(const char *string, int *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long(string, &num, base))
		return FALSE;

	if (
		num < INT_MIN ||
		num > INT_MAX
	) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_int_strict(const char *string, int *number, const int base)
{
	long	num;

	*number = 0;

	if (!str_to_long_strict(string, &num, base))
		return FALSE;

	if (
		num < INT_MIN ||
		num > INT_MAX
	) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_uint(const char *string, unsigned int *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong(string, &num, base))
		return FALSE;

	if (num > UINT_MAX) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_uint_strict(const char *string, unsigned int *number, const int base)
{
	unsigned long	num;

	*number = 0;

	if (!str_to_ulong_strict(string, &num, base))
		return FALSE;

	if (num > UINT_MAX) {
		errno = ERANGE;
		return FALSE;
	}

	*number = num;
	return TRUE;
}

bool_t	str_to_long(const char *string, long *number, const int base)
{
	char		*ptr = NULL;
	const char	*end;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return FALSE;
	}

	end = string + strlen(string);
	while (
		end > string &&
		isspace(*(end - 1))
	)
		end--;

	if (end == string) {
		errno = EINVAL;
		return FALSE;
	}

	errno = 0;
	*number = strtol(string, &ptr, base);

	if (
		errno == EINVAL ||
		ptr != end
	) {
		*number = 0;
		errno = EINVAL;
		return FALSE;
	}

	if (errno == ERANGE) {
		*number = 0;
		return FALSE;
	}

	return TRUE;
}

bool_t	str_to_long_strict(const char *string, long *number, const int base)
{
	const char	*str;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return FALSE;
	}

	for (str = string; *str != '\0'; str++)
		if (isspace(*str)) {
			errno = EINVAL;
			return FALSE;
		}

	return str_to_long(string, number, base);
}

bool_t	str_to_ulong(const char *string, unsigned long *number, const int base)
{
	char		*ptr = NULL;
	const char	*end;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0' ||
		strchr(string, '+') != NULL ||
		strchr(string, '-') != NULL
	) {
		errno = EINVAL;
		return FALSE;
	}

	end = string + strlen(string);
	while (
		end > string &&
		isspace(*(end - 1))
	)
		end--;

	if (end == string) {
		errno = EINVAL;
		return FALSE;
	}

	errno = 0;
	*number = strtoul(string, &ptr, base);

	if (
		errno == EINVAL ||
		ptr != end
	) {
		*number = 0;
		errno = EINVAL;
		return FALSE;
	}

	if (errno == ERANGE) {
		*number = 0;
		return FALSE;
	}

	return TRUE;
}

bool_t	str_to_ulong_strict(const char *string, unsigned long *number, const int base)
{
	const char	*str;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return FALSE;
	}

	for (str = string; *str != '\0'; str++)
		if (isspace(*str)) {
			errno = EINVAL;
			return FALSE;
		}

	return str_to_ulong(string, number, base);
}

bool_t	str_to_double(const char *string, double *number, const int base)
{
	char		*ptr = NULL;
	const char	*start,
			*end;
	size_t		 length;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return FALSE;
	}

	start = string;
	while (
		*start != '\0' &&
		isspace(*start)
	)
		start++;

	end = start + strlen(start);
	while (
		end > start &&
		isspace(*(end - 1))
	)
		end--;

	length = end - start;

	if (!length) {
		errno = EINVAL;
		return FALSE;
	}

	switch (base)
	{
	case 10:
		if (length != strspn(start, "-+.0123456789Ee")) {
			errno = EINVAL;
			return FALSE;
		}
		break;
	case  0:
	case 16:
		if (length != strspn(start, "-+.0123456789ABCDEFabcdefXxPp")) {
			errno = EINVAL;
			return FALSE;
		}
		break;
	default:
		errno = EINVAL;
		return FALSE;
	}

	errno = 0;
	*number = strtod(start, &ptr);

	if (
		errno == EINVAL ||
		ptr != end
	) {
		*number = 0;
		errno = EINVAL;
		return FALSE;
	}

	if (errno == ERANGE) {
		*number = 0;
		return FALSE;
	}

	return TRUE;
}

bool_t	str_to_double_strict(const char *string, double *number, const int base)
{
	const char	*str;

	*number = 0;

	if (
		string == NULL ||
		*string == '\0'
	) {
		errno = EINVAL;
		return FALSE;
	}

	for (str = string; *str != '\0'; str++)
		if (isspace(*str)) {
			errno = EINVAL;
			return FALSE;
		}

	return str_to_double(string, number, base);
}
