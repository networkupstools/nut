/* upssched-list-timers-test - preserve the width of timer expiry timestamps
 *
 * Copyright (C) 2026 Network UPS Tools project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

int upssched_main(int argc, char **argv);
#define main upssched_main
#include "upssched.c"
#undef main

#ifdef WIN32
# include <io.h>
# include <fcntl.h>
#endif

static int check_timestamp(uintmax_t seconds, const char *expected)
{
	ttype_t timer;
	conn_t conn;
	FILE *output;
	char *command[] = { (char *)"LIST-TIMERS" };
	char line[US_SOCK_BUF_LEN];
	int pipefd[2];
	int failed = 0;

	memset(&timer, 0, sizeof(timer));
	memset(&conn, 0, sizeof(conn));
	timer.name = (char *)"test";
	timer.etime = (time_t)seconds;
	if ((uintmax_t)timer.etime != seconds) {
		printf("SKIP: %s is outside the range of time_t\n", expected);
		return 0;
	}

#ifdef WIN32
	if (_pipe(pipefd, 2 * US_SOCK_BUF_LEN, _O_BINARY) != 0) {
		fatal_with_errno(EXIT_FAILURE, "_pipe");
	}
	conn.fd = (HANDLE)_get_osfhandle(pipefd[1]);
	output = _fdopen(pipefd[0], "rb");
#else
	if (pipe(pipefd) != 0) {
		fatal_with_errno(EXIT_FAILURE, "pipe");
	}
	conn.fd = pipefd[1];
	output = fdopen(pipefd[0], "r");
#endif
	if (!output) {
		fatal_with_errno(EXIT_FAILURE, "fdopen");
	}
	conn.ctx.numargs = 1;
	conn.ctx.arglist = command;
	thead = &timer;
	/* One short row and the protocol markers fit in the pipe buffer. */
	if (!sock_arg(&conn)) {
		fatalx(EXIT_FAILURE, "LIST-TIMERS failed");
	}
	thead = NULL;
#ifdef WIN32
	_close(pipefd[1]);
#else
	close(pipefd[1]);
#endif
	line[0] = '\0';
	if (!fgets(line, sizeof(line), output)
	||  strcmp(line, "BEGIN LIST TIMERS\n")
	||  !fgets(line, sizeof(line), output)
	||  strncmp(line, expected, strlen(expected))
	) {
		printf("FAIL: expected prefix '%s', got '%s'\n", expected, line);
		failed = 1;
	} else {
		printf("PASS: %s", line);
	}
	fclose(output);
	return failed;
}

int main(void)
{
	int failures = 0;

	printf("sizeof(time_t)=%" PRIuSIZE ", sizeof(long)=%" PRIuSIZE "\n",
		sizeof(time_t), sizeof(long));
	failures += check_timestamp(0, "test\t0\t");
	failures += check_timestamp(2147483647UL, "test\t2147483647\t");
	failures += check_timestamp(2147483648UL, "test\t2147483648\t");
	failures += check_timestamp(4102444800UL, "test\t4102444800\t");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
