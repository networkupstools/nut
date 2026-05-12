/* test_authconf.c - test program for client/authconf.c
 *
 * Copyright (C) 2026 Jim Klimov <jimklimov+nut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include "common.h"
#include "authconf.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int main(int argc, char **argv)
{
	const char	*test_conf = "test_nutauth.conf";
	const char	*include_conf = "test_include.conf";
	FILE	*f;
	upscli_authconf_t	*ac;
	size_t	num_sections;
	char	buf[512];

	if (argc > 1) {
		upsdebugx(1, "Args ignored: '%s' etc.", argv[0]);
	}

	/* Create dummy config files */
	f = fopen(test_conf, "w");
	if (!f) {
		perror("fopen test_nutauth.conf");
		return 1;
	}
	fprintf(f, "USER = globaluser\n");
	fprintf(f, "PASS = globalpass\n");
	fprintf(f, "CERTVERIFY = 1\n");
	fprintf(f, "INCLUDE %s\n", include_conf);
	fprintf(f, "[@localhost:12345]\n");
	fprintf(f, "  USER = hostuser\n");
	fprintf(f, "  FORCESSL = 1\n");
	fprintf(f, "[admin@localhost:12345]\n");
	fprintf(f, "  PASS = adminpass\n");
	fprintf(f, "  FORCESSL = 1\n");
	fclose(f);

	f = fopen(include_conf, "w");
	if (!f) {
		perror("fopen test_include.conf");
		return 1;
	}
	fprintf(f, "[@otherhost]\n");
	fprintf(f, "  USER = otheruser\n");
	fclose(f);

	if (upscli_read_authconf(test_conf, 1) != 1) {
		fprintf(stderr, "read_authconf failed\n");
		return 1;
	}

	printf("=== Parsed configuration:\n");
	num_sections = upscli_dump_authconf_list(NULL);
	printf("===== Collected %" PRIuSIZE " sections\n\n", num_sections);

	/* Test matching */
	printf("=== Testing matches...\n");

	/* 1. Global match (no specific section for this host) */
	printf("Checking global match for '@somehost:port'...\n");
	ac = upscli_find_authconf(NULL, "somehost", "port");
	if (ac) {
		printf("Global match got user=%s\n", ac->user ? ac->user : "NULL");
		if (ac->user && strcmp(ac->user, "globaluser") == 0) {
			printf("Global match OK\n");
		} else {
			printf("Global match FAILED (wrong user)\n");
			return 1;
		}
	} else {
		printf("Global match FAILED (no ac)\n");
		return 1;
	}

	/* 2. Host default match */
	printf("Checking host default match for '@localhost:12345'\n");
	ac = upscli_find_authconf(NULL, "localhost", "12345");
	if (ac && strcmp(ac->user, "hostuser") == 0 && ac->forcessl == 1 && ac->certverify == 1) {
		printf("Host default match OK\n");
	} else {
		printf("Host default match FAILED\n");
		return 1;
	}

	/* 3. Exact match */
	printf("Checking exact match for 'admin@localhost:12345'\n");
	ac = upscli_find_authconf("admin", "localhost", "12345");
	if (ac) {
		printf("Exact match: got user=%s pass=%s forcessl=%d\n",
			ac->user ? ac->user : "NULL",
			ac->pass ? ac->pass : "NULL",
			ac->forcessl);

		if (ac->user && strcmp(ac->user, "admin") == 0
		 && ac->pass && strcmp(ac->pass, "adminpass") == 0
		 && ac->forcessl == 1
		) {
			printf("Exact match OK\n");
		} else {
			printf("Exact match FAILED (wrong values): expecting user='%s' pass='%s'\n", "admin", "adminpass");
			return 1;
		}
	} else {
		printf("Exact match FAILED (no ac)\n");
		return 1;
	}

	/* 4. Include match */
	printf("Checking include match for '@otherhost'\n");
	ac = upscli_find_authconf(NULL, "otherhost", NULL);
	snprintf(buf, sizeof(buf), "@otherhost:%u", (unsigned int)NUT_PORT);
	if (ac
	 && ac->section && strcmp(ac->section, buf) == 0
	 && ac->user && strcmp(ac->user, "otheruser") == 0
	) {
		printf("Include match OK\n");
	} else {
		if (ac) {
			printf("Include match FAILED: got section=%s user=%s\n",
				ac->section ? ac->section : "NULL",
				ac->user ? ac->user : "NULL");
		} else {
			printf("Include match FAILED: no ac\n");
		}
		return 1;
	}

	/* 5. No bogus hits */
	printf("Checking NO match for '@otherhost:portnum' other than global section\n");
	ac = upscli_find_authconf(NULL, "otherhost", "portnum");
	if (ac) {
		if (!(ac->section) || !*(ac->section)) {
			printf("No bogus match OK: got global section\n");
		} else {
			printf("No bogus match FAILED: had a hit\n");
			upscli_dump_authconf(NULL, ac);
			return 1;
		}
	} else {
		printf("No bogus match kind of OK: got no ac\n");
	}

	upscli_free_authconf_list();
	unlink(test_conf);
	unlink(include_conf);

	printf("All tests passed!\n");
	return 0;
}
