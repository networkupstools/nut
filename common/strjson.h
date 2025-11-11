/* strjson.h - common JSON string formatting helper

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#ifndef NUT_STRJSON_H_SEEN
#define NUT_STRJSON_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/**
 * @brief Helper function to print a string to stdout as a JSON-safe string.
 * This function prints the escaped string *without* surrounding quotes.
 * @param in The raw C-string to escape and print.
 */
void json_print_esc(const char *in);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_STRJSON_H_SEEN */