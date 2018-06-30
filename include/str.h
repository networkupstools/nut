/**
 * @file
 * @brief	Common string-related functions.
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

#ifndef STR_H
#define STR_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#include "bool.h"

/** @name Character trimming
 * Remove all
 *
 * - leading and trailing (str_trim[_m]())
 * - leading (str_ltrim[_m]())
 * - trailing (str_rtrim[_m]())
 *
 * instances of
 *
 * - *character* (plain versions)
 * - each character in *characters* ('_m' versions)
 *
 * from a string.
 *
 * @param[in,out]	string		null-terminated byte string from which characters are to be removed.
 * @param[in]		character	character that has to be removed from *string*.
 * @param[in]		characters	null-terminated byte string of characters to be removed from string.
 * @return		`NULL`, if *string* is `NULL`,
 * @return		*string* without the specified characters (upto an empty string), otherwise.
 * @{ */
char	*str_trim(char *string, const char character);
char	*str_trim_m(char *string, const char *characters);
char	*str_ltrim(char *string, const char character);
char	*str_ltrim_m(char *string, const char *characters);
char	*str_rtrim(char *string, const char character);
char	*str_rtrim_m(char *string, const char *characters);
/** @} */

/** @name Space trimming
 * Remove all
 *
 * - leading and trailing (str_trim_space())
 * - leading (str_ltrim_space())
 * - trailing (str_rtrim_space())
 *
 * spaces (as identified by isspace()) from a string.
 *
 * @param[in,out]	string	null-terminated byte string from which spaces are to be removed.
 *
 * @return		`NULL`, if *string* is `NULL`,
 * @return		*string* without the specified spaces (upto an empty string), otherwise.
 * @{ */
char	*str_trim_space(char *string);
char	*str_ltrim_space(char *string);
char	*str_rtrim_space(char *string);
/** @} */

/** @name String-is-number? tests
 * Tell whether a string can be converted to a number of type str_is_<type>[_strict]().
 *
 * @param[in]	string	the null-terminated byte string to check.
 * @param[in]	base	the base the string must conform to.
 *
 * @note	The same restrictions of the corresponding str_to_<type>[_strict]() functions apply.
 *
 * @return	@ref TRUE, if *string* can be converted to a valid number of type <type>,
 * @return	@ref FALSE, otherwise, with `errno` set to:
 *		- `EINVAL`, if the value of *base* is not supported or no conversion could be performed;
 *		- `ERANGE`, if the converted value would be out of the acceptable range of <type>.
 * @{ */
bool_t	str_is_short(const char *string, const int base);
bool_t	str_is_short_strict(const char *string, const int base);
bool_t	str_is_ushort(const char *string, const int base);
bool_t	str_is_ushort_strict(const char *string, const int base);
bool_t	str_is_int(const char *string, const int base);
bool_t	str_is_int_strict(const char *string, const int base);
bool_t	str_is_uint(const char *string, const int base);
bool_t	str_is_uint_strict(const char *string, const int base);
bool_t	str_is_long(const char *string, const int base);
bool_t	str_is_long_strict(const char *string, const int base);
bool_t	str_is_ulong(const char *string, const int base);
bool_t	str_is_ulong_strict(const char *string, const int base);
bool_t	str_is_double(const char *string, const int base);
bool_t	str_is_double_strict(const char *string, const int base);
/** @} */

/** @name String-to-number conversions
 * Convert a string to a number of type str_to_<type>[_strict]().
 *
 * @param[in]	string	the null-terminated byte string to convert:
 *			- 'strict' versions' strings shall not contain spaces (as identified by isspace()),
 *			- short, int, long: strtol()'s restrictions apply,
 *			- ushort, uint, ulong: strtoul()'s restrictions apply, plus:
 *			  - plus (`+`) and minus (`-`) signs (and hence negative values) are not supported,
 *			- double: strtod()'s restrictions apply, plus:
 *			  - infinity and nan are not supported,
 *			  - radix character (decimal point character) must be a period (`.`).
 * @param[out]	number	storage location of the appropriate <type> for the converted string, that will be filled upon execution.
 * @param[in]	base	the base the string must conform to:
 *			- short, ushort, int, uint, long, ulong: acceptable values as in strtol()/strtoul(),
 *			- double: 0 for auto-select, 10 or 16.
 *
 * @return	@ref TRUE, on success, with *number* being the result of the conversion of *string*,
 * @return	@ref FALSE, on failure, with *number* being 0 and `errno` set to:
 *		- `EINVAL`, if the value of *base* is not supported or no conversion can be performed;
 *		- `ERANGE`, if the converted value is out of the acceptable range of <type>.
 * @{ */
bool_t	str_to_short(const char *string, short *number, const int base);
bool_t	str_to_short_strict(const char *string, short *number, const int base);
bool_t	str_to_ushort(const char *string, unsigned short *number, const int base);
bool_t	str_to_ushort_strict(const char *string, unsigned short *number, const int base);
bool_t	str_to_int(const char *string, int *number, const int base);
bool_t	str_to_int_strict(const char *string, int *number, const int base);
bool_t	str_to_uint(const char *string, unsigned int *number, const int base);
bool_t	str_to_uint_strict(const char *string, unsigned int *number, const int base);
bool_t	str_to_long(const char *string, long *number, const int base);
bool_t	str_to_long_strict(const char *string, long *number, const int base);
bool_t	str_to_ulong(const char *string, unsigned long *number, const int base);
bool_t	str_to_ulong_strict(const char *string, unsigned long *number, const int base);
bool_t	str_to_double(const char *string, double *number, const int base);
bool_t	str_to_double_strict(const char *string, double *number, const int base);
/** @} */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* STR_H */
