/* strjson.c - common JSON string formatting helper

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "common.h"
#include "strjson.h"

/**
 * @brief Helper function to print a string to stdout as a JSON-safe string.
 * This function prints the escaped string *without* surrounding quotes.
 * @param in The raw C-string to escape and print.
 */
void json_print_esc(const char *in) {
	if (!in) {
		printf("null");
		return;
	}

	while (*in) {
		switch (*in) {
			case '\"': printf("\\\""); break;
			case '\\': printf("\\\\"); break;
			case '\b': printf("\\b"); break;
			case '\f': printf("\\f"); break;
			case '\n': printf("\\n"); break;
			case '\r': printf("\\r"); break;
			case '\t': printf("\\t"); break;
			default:
				if ((unsigned char)*in < 32) {
					/* Print control characters as unicode */
					printf("\\u%04x", (unsigned int)*in);
				} else {
					putchar(*in);
				}
				break;
		}
		in++;
	}
}

