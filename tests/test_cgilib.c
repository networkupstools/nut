/* test_cgilib - focused checks for CGI output encoders

   Copyright (C) 2026 Network UPS Tools project

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "common.h"

#ifdef WIN32
# include <io.h>
# define dup _dup
# define dup2 _dup2
# define fileno _fileno
# define close _close
#else
# include <unistd.h>
#endif

#include "cgilib.h"

void parsearg(char *var, char *value)
{
	NUT_UNUSED_VARIABLE(var);
	NUT_UNUSED_VARIABLE(value);
}

int main(void)
{
	static const char expected[] =
		"A&amp;&lt;&gt;&quot;&#39; \303\251\n"
		"AZaz09-._~%20%25%2B%C3%A9%26%3D\n";
	char output[sizeof(expected) + 1];
	FILE *capture;
	int saved_stdout;
	size_t len;

	capture = tmpfile();
	if (!capture) {
		perror("tmpfile");
		return EXIT_FAILURE;
	}

	fflush(stdout);
	saved_stdout = dup(fileno(stdout));
	if (saved_stdout < 0 || dup2(fileno(capture), fileno(stdout)) < 0) {
		perror("redirect stdout");
		fclose(capture);
		return EXIT_FAILURE;
	}

	html_print_esc("A&<>\"' \303\251");
	html_print_esc(NULL);
	putchar('\n');
	url_print_esc("AZaz09-._~ %+\303\251&=");
	url_print_esc(NULL);
	putchar('\n');
	fflush(stdout);

	if (dup2(saved_stdout, fileno(stdout)) < 0) {
		perror("restore stdout");
		close(saved_stdout);
		fclose(capture);
		return EXIT_FAILURE;
	}
	close(saved_stdout);

	rewind(capture);
	len = fread(output, 1, sizeof(output) - 1, capture);
	output[len] = '\0';
	fclose(capture);

	if (strcmp(output, expected)) {
		fprintf(stderr, "unexpected CGI encoder output:\n%s", output);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
