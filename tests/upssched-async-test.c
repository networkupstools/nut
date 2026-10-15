/* upssched-async-test - timer command ownership and collection
 *
 * Copyright (C) 2026 Network UPS Tools project
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

/* Include the implementation to test private command ownership. */
int upssched_main(int argc, char **argv);
#define main upssched_main
#include "upssched.c"
#undef main

int main(int argc, char **argv)
{
	command_t *child;
	int count = 0, attempts;
	char *script[3];

	if (argc == 3 && !strcmp(argv[1], "--command")) {
		if (!strcmp(argv[2], "slow")) {
			sleep(8);
		}
		return 0;
	}

	script[0] = argv[0];
	script[1] = (char *)"--command";
	script[2] = NULL;
	cmdscript_argv = script;
	cmdscript_argc = 2;
	cmdscript_concat = argv[0];

	exec_cmd("slow", 1);
	exec_cmd("fast", 1);
	for (attempts = 0; attempts < 3; attempts++) {
		sleep(1);
		reap_commands();
		count = 0;
		for (child = commands; child; child = child->next) {
			count++;
		}
		if (count == 1) {
			break;
		}
	}
	if (count != 1 || strcmp(commands->name, "slow")) {
		fprintf(stderr, "FAIL: fast command must finish while slow command runs\n");
		return 1;
	}
	for (attempts = 0; commands && attempts < 10; attempts++) {
		sleep(1);
		reap_commands();
	}
	if (commands) {
		fprintf(stderr, "FAIL: completed commands must be collected\n");
		return 1;
	}
	puts("PASS: independent timer command execution and collection");
	return 0;
}
