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
	upscli_authconf_t	*ac, *ac5, *ac7, *ac8, *ac9, *ac12;
	size_t	num_sections, expected_sections = 0;
	char	buf[512], *s;
	int	l, testnum = 0;

	s = getenv("NUT_DEBUG_LEVEL");
	if (s && str_to_int(s, &l, 10) && l > 0) {
		nut_debug_level = l;
		upsdebugx(1, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
			"since none was requested by command-line options", l);
	}

	if (argc > 1) {
		upsdebugx(1, "Args ignored: '%s' etc.", argv[0]);
	}

	/* Create dummy config files */
	f = fopen(test_conf, "w");
	if (!f) {
		perror("fopen test_nutauth.conf");
		return 1;
	}

	expected_sections++;
	fprintf(f, "USER = globaluser\n");
	fprintf(f, "PASS = globalpass\n");
	fprintf(f, "CERTVERIFY = 1\n");
	fprintf(f, "INCLUDE %s\n", include_conf);

	expected_sections++;
	fprintf(f, "[@localhost:12345]\n");
	fprintf(f, "  USER = hostuser\n");
	fprintf(f, "  FORCESSL = 1\n");

	expected_sections++;
	fprintf(f, "[admin@localhost:12345]\n");
	fprintf(f, "  PASS = adminpass\n");
	fprintf(f, "  FORCESSL = 1\n");

	expected_sections++;
	/* localhost should get defaulted here: */
	fprintf(f, "[bigadmin@:12345]\n");
	fprintf(f, "  PASS = adminpass\n");
	fprintf(f, "  FORCESSL = 1\n");
	fclose(f);

	f = fopen(include_conf, "w");
	if (!f) {
		perror("fopen test_include.conf");
		return 1;
	}

	/* NOTE: Non-commented tokens are probably also ignored */
	expected_sections++;
	fprintf(f, "[@otherhost] # Other (commented) tokens ignored\n");
	fprintf(f, "  USER = otheruser\n");
	fprintf(f, "  CERTHOST = \"Other Server\"\n");

	/* A link-local (MAC address based) IPv6 address as colon-separated hexes in square brackets
	 * Here and below - essentially a test for upscli_split_authconf_section() method.
	 * A missing '@' should mean the section name is wholly the host name (maybe with port) */
	expected_sections++;
	fprintf(f, "[[fe80::215:5dff:fea4:f780]]\n");
	fprintf(f, "  CERTHOST = \"An IPv6 Server\"\n");

	expected_sections++;
	fprintf(f, "[ipv6user@[fe80::215:5dff:fea4:f780]]\n");
	fprintf(f, "  PASS = \"ipv6pass\"\n");

	expected_sections++;
	fprintf(f, "[[fe80::215:5dff:fea4:f781]:3495]\n");
	fprintf(f, "  CERTHOST = \"An IPv6 Server on port 3495\"\n");

	/* FIXME: With proper IPv6 parsing, these two should collapse into the same section
	 *  or error out in case of conflicts/redefinitions */
	expected_sections++;
	fprintf(f, "[@[0::1]:12345]\n");
	fprintf(f, "  USER = IPv6user\n");
	fprintf(f, "  CERTHOST = \"An IPv6 localhost Server\"\n");
	expected_sections++;
	fprintf(f, "[@[::1]:12345]\n");
	fprintf(f, "  USER = IPv6user2\n");
	fprintf(f, "  CERTHOST = \"An IPv6 localhost Server\"\n");

	fclose(f);

	if ((s = getenv("NUT_AUTHCONF_FILE"))) {
		printf("=== FYI: Trying NUT_AUTHCONF_FILE='%s' just for kicks\n", s);
		if (upscli_read_authconf_file(NULL, 0) != 1) {
			fprintf(stderr, "INFO: Default read_authconf failed (user-provided config parsing failed)\n");
		} else {
			printf("=== Parsed user configuration (debug view):\n");
			/* With "for_debug", show all fields (highlight NULLs) */
			num_sections = upscli_dump_authconf_list(NULL, 1, 1);
			printf("===== Collected %" PRIuSIZE " sections\n\n", num_sections);
		}
	}

	/* 1. Expected file read */
	printf("=== Reading '%s' generated for this test\n", test_conf);
	if (upscli_read_authconf_file(test_conf, 1) != 1) {
		fprintf(stderr, "not ok %d - read_authconf failed\n", ++testnum);
		return 1;
	}
	printf("ok %d - read_authconf did not fail\n", ++testnum);

	/* 2. Expected printout 1 */
	printf("=== Parsed configuration (production view):\n");
	/* Not "for_debug", but how would this info look in a config file */
	num_sections = upscli_dump_authconf_list(NULL, 0, 1);
	printf("===== Collected %" PRIuSIZE " sections\n\n", num_sections);
	printf("%sok %d - parsed %" PRIuSIZE " sections (including global)\n", num_sections == expected_sections ? "" : "not ", ++testnum, expected_sections);

	/* 3. Expected printout 2 */
	printf("=== Parsed configuration (debug view):\n");
	/* With "for_debug", show all fields (highlight NULLs) */
	num_sections = upscli_dump_authconf_list(NULL, 1, 1);
	printf("===== Collected %" PRIuSIZE " sections\n\n", num_sections);
	printf("%sok %d - parsed %" PRIuSIZE " sections (including global)\n", num_sections == expected_sections ? "" : "not ", ++testnum, expected_sections);

	/* Test matching */
	printf("=== Testing matches...\n");

	/* 4. Global match (no specific section for this host) */
	printf("Checking global match for '@somehost:port', and adding it to the list...\n");
	ac = upscli_get_authconf_item(NULL, "somehost", "port", 1);
	expected_sections++;
	if (ac) {
		printf("Global match got user=%s\n", ac->user ? ac->user : "NULL");
		if (ac->user && strcmp(ac->user, "globaluser") == 0) {
			printf("ok %d - Global match OK\n", ++testnum);
		} else {
			printf("not ok %d - Global match FAILED (wrong user)\n", ++testnum);
			return 1;
		}
	} else {
		printf("not ok %d - Global match FAILED (no ac)\n", ++testnum);
		return 1;
	}

	/* 5. Host default match, not saved */
	printf("Checking host default match for '@localhost:12345', not saved into list\n");
	ac5 = upscli_get_authconf_item(NULL, "localhost", "12345", 0);
	if (ac5 && strcmp(ac5->user, "hostuser") == 0 && ac5->forcessl == 1 && ac5->certverify == 1) {
		printf("ok %d - Host default match OK\n", ++testnum);
	} else {
		printf("not ok %d - Host default match FAILED\n", ++testnum);
		if (ac5)
			upscli_free_authconf_item(ac5);
		return 1;
	}

	/* 6. Exact match */
	printf("Checking exact match for 'admin@localhost:12345'\n");
	ac = upscli_get_authconf_item("admin", "localhost", "12345", 1);
	if (ac) {
		printf("Exact match: got user=%s pass=%s forcessl=%d\n",
			ac->user ? ac->user : "NULL",
			ac->pass ? ac->pass : "NULL",
			ac->forcessl);

		if (ac->user && strcmp(ac->user, "admin") == 0
		 && ac->pass && strcmp(ac->pass, "adminpass") == 0
		 && ac->forcessl == 1
		) {
			printf("ok %d - Exact match OK\n", ++testnum);
		} else {
			printf("not ok %d - Exact match FAILED (wrong values): expecting user='%s' pass='%s'\n",
				++testnum, "admin", "adminpass");
			return 1;
		}
	} else {
		printf("not ok %d - Exact match FAILED (no ac)\n", ++testnum);
		return 1;
	}

	/* 7. Non-exact match */
	printf("Checking non-exact match for 'somebody@localhost:12345'\n");
	ac7 = upscli_get_authconf_item("somebody", "localhost", "12345", 0);
	if (ac7) {
		printf("Non-exact match: got user=%s pass=%s forcessl=%d\n",
			ac7->user ? ac7->user : "NULL",
			ac7->pass ? ac7->pass : "NULL",
			ac7->forcessl);

		if (ac7->user && strcmp(ac7->user, "somebody") == 0
		 && ac7->pass && strcmp(ac7->pass, "globalpass") == 0	/* replaced from NULL originally in @localhost:12345 */
		 && ac7->forcessl == 1
		) {
			printf("ok %d - Non-exact match OK\n", ++testnum);
		} else {
			printf("not ok %d - Non-exact match FAILED (wrong values): expecting user='%s' pass='%s'\n",
				++testnum, "somebody", "globalpass");
			return 1;
		}
	} else {
		printf("not ok %d - Non-exact match FAILED (no ac)\n", ++testnum);
		return 1;
	}

	/* 8. Host default match, saved to list (already is there) */
	printf("Checking host default match for '@localhost:12345' and saving into list\n");
	ac8 = upscli_get_authconf_item(NULL, "localhost", "12345", 1);
	if (ac8 && strcmp(ac8->user, "hostuser") == 0 && ac8->forcessl == 1 && ac8->certverify == 1) {
		printf("ok %d - Host default match OK\n", ++testnum);
	} else {
		printf("not ok %d - Host default match FAILED\n", ++testnum);
		return 1;
	}

	/* 9. Non-exact match, take 2 */
	printf("Checking non-exact match for 'somebody@localhost:12345' after list modification, and adding it to the list\n");
	ac9 = upscli_get_authconf_item("somebody", "localhost", "12345", 1);
	expected_sections++;
	if (ac9) {
		printf("Non-exact match: got user=%s pass=%s forcessl=%d\n",
			ac9->user ? ac9->user : "NULL",
			ac9->pass ? ac9->pass : "NULL",
			ac9->forcessl);

		if (ac9->user && strcmp(ac9->user, "somebody") == 0
		 && ac9->pass && strcmp(ac9->pass, "globalpass") == 0	/* replaced from NULL originally in @localhost:12345 */
		 && ac9->forcessl == 1
		) {
			printf("ok %d - Non-exact match OK\n", ++testnum);
		} else {
			printf("not ok %d - Non-exact match FAILED (wrong values): expecting user='%s' pass='%s'\n",
				++testnum, "somebody", "globalpass");
			return 1;
		}
	} else {
		printf("not ok %d - Non-exact match FAILED (no ac)\n", ++testnum);
		return 1;
	}

	/* 10. Same non-exact match */
	printf("Checking non-exact match for 'somebody@localhost:12345' after list modification, should be same pointer\n");
	ac = upscli_get_authconf_item("somebody", "localhost", "12345", 1);
	if (ac) {
		printf("Non-exact match: got user=%s pass=%s forcessl=%d\n",
			ac->user ? ac->user : "NULL",
			ac->pass ? ac->pass : "NULL",
			ac->forcessl);

		if (ac->user && strcmp(ac->user, "somebody") == 0
		 && ac->pass && strcmp(ac->pass, "globalpass") == 0	/* replaced from NULL originally in @localhost:12345 */
		 && ac->forcessl == 1
		) {
			printf("ok %d - Non-exact match OK\n", ++testnum);
		} else {
			printf("not ok %d - Non-exact match FAILED (wrong values): expecting user='%s' pass='%s'\n",
				++testnum, "somebody", "globalpass");
			return 1;
		}
	} else {
		printf("not ok %d - Non-exact match FAILED (no ac)\n", ++testnum);
		return 1;
	}
	/* 11. Same non-exact match - continued */
	if (ac == ac9) {
		printf("ok %d - Non-exact match OK and returned same pointer to item in the list\n", ++testnum);
	} else {
		printf("not ok %d - Non-exact match FAILED (did not return same pointer to item in the list)\n", ++testnum);
		return 1;
	}

	/* 12. Same non-exact match but not in the list */
	printf("Checking non-exact match for 'somebody@localhost:12345' after list modification, but not adding to list, should be a different pointer\n");
	ac12 = upscli_get_authconf_item("somebody", "localhost", "12345", 0);
	if (ac12) {
		printf("Non-exact match: got user=%s pass=%s forcessl=%d\n",
			ac12->user ? ac12->user : "NULL",
			ac12->pass ? ac12->pass : "NULL",
			ac12->forcessl);

		if (ac12->user && strcmp(ac12->user, "somebody") == 0
		 && ac12->pass && strcmp(ac12->pass, "globalpass") == 0	/* replaced from NULL originally in @localhost:12345 */
		 && ac12->forcessl == 1
		) {
			printf("ok %d - Non-exact match OK\n", ++testnum);
		} else {
			printf("not ok %d - Non-exact match FAILED (wrong values): expecting user='%s' pass='%s'\n",
				++testnum, "somebody", "globalpass");
			return 1;
		}
	} else {
		printf("not ok %d - Non-exact match FAILED (no ac12)\n", ++testnum);
		return 1;
	}
	/* 13. Same non-exact match - continued */
	if (ac12 != ac9) {
		printf("ok %d - Non-exact match OK and did not return same pointer to item in the list\n", ++testnum);
	} else {
		printf("not ok %d - Non-exact match FAILED (returned same pointer to item in the list but should have been a clone)\n", ++testnum);
		return 1;
	}

	/* 14. Include match */
	printf("Checking include match for '@otherhost'\n");
	ac = upscli_get_authconf_item(NULL, "otherhost", NULL, 1);
	snprintf(buf, sizeof(buf), "@otherhost:%u", (unsigned int)NUT_PORT);
	if (ac
	 && ac->section && strcmp(ac->section, buf) == 0
	 && ac->user && strcmp(ac->user, "otheruser") == 0
	 && ac->certhost && strcmp(ac->certhost, "Other Server") == 0
	) {
		printf("ok %d - Include match OK\n", ++testnum);
	} else {
		if (ac) {
			printf("not ok %d - Include match FAILED: got section=%s user=%s\n",
				++testnum,
				ac->section ? ac->section : "NULL",
				ac->user ? ac->user : "NULL");
		} else {
			printf("not ok %d - Include match FAILED: no ac\n", ++testnum);
		}
		return 1;
	}

	/* 15. No bogus hits */
	printf("Checking NO match for '@otherhost:portnum' other than global section\n");
	ac = upscli_find_authconf_item(NULL, "otherhost", "portnum");
	if (ac) {
		if (!(ac->section) || !*(ac->section)) {
			printf("ok %d - No bogus match OK: got global section\n", ++testnum);
		} else {
			printf("not ok %d - No bogus match FAILED: had a hit\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("ok %d - No bogus match kind of OK: got no ac\n", ++testnum);
	}

	/* 16. IPv6#1 in brackets */
	printf("Checking IPv6#1 match for a link-local address section\n");
	ac = upscli_find_authconf_item(NULL, "[fe80::215:5dff:fea4:f780]", NULL);
	if (ac) {
		/* Normalized, with default port injected */
		if (ac->section && !strcmp(ac->section, "@[fe80::215:5dff:fea4:f780]:3493")) {
			printf("ok %d - got expected bracketed IPv6#1 address as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected bracketed IPv6#1 address as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected bracketed IPv6#1 address\n", ++testnum);
		return 1;
	}

	/* 17. Data in that IPv6#1 section... */
	if (ac->certhost && !strcmp(ac->certhost, "An IPv6 Server")) {
		printf("ok %d - got expected CERTHOST in IPv6#1 section\n", ++testnum);
	} else {
		printf("not ok %d - did not get expected CERTHOST in IPv6#1 section\n", ++testnum);
		upscli_dump_authconf_item(NULL, ac, 1, 1);
		return 1;
	}
	/* FIXME: Find a host_cert for the bracketed IPv6 address, make sure it is the same as the one in the section */

	/* 18. user@IPv6#1 in brackets */
	printf("Checking user@IPv6#1 match for a link-local address section\n");
	ac = upscli_find_authconf_item("ipv6user", "[fe80::215:5dff:fea4:f780]", NULL);
	if (ac) {
		if (ac->section && !strcmp(ac->section, "ipv6user@[fe80::215:5dff:fea4:f780]:3493")) {
			printf("ok %d - got expected bracketed user@IPv6#1 address as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected bracketed user@IPv6#1 address as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected bracketed user@IPv6#1 address\n", ++testnum);
		return 1;
	}

	/* 19. Data in that user@IPv6#1 section, unique... */
	if (ac->pass && !strcmp(ac->pass, "ipv6pass")) {
		printf("ok %d - got expected PASS in user@IPv6#1 section\n", ++testnum);
	} else {
		printf("not ok %d - did not get expected PASS in user@IPv6#1 section\n", ++testnum);
		upscli_dump_authconf_item(NULL, ac, 1, 1);
		return 1;
	}

	/* 20. Data in that user@IPv6#1 section, inherited - should be none for find method... */
	if (!(ac->certhost)) {
		printf("ok %d - no expected CERTHOST in user@IPv6#1 section\n", ++testnum);
	} else {
		printf("not ok %d - got an unexpected CERTHOST in user@IPv6#1 section\n", ++testnum);
		upscli_dump_authconf_item(NULL, ac, 1, 1);
		return 1;
	}

	/* 21. Re-probe with get method */
	printf("Checking user@IPv6#1 CERTHOST match for a link-local address section after upscli_get_authconf_item(), already in list - updated in place\n");
	ac = upscli_get_authconf_item("ipv6user", "[fe80::215:5dff:fea4:f780]", NULL, 1);
	/* NOTE: Not bumping expected_sections because the section is already in the list */
	if (ac) {
		if (ac->section && !strcmp(ac->section, "ipv6user@[fe80::215:5dff:fea4:f780]:3493")) {
			printf("ok %d - got expected bracketed user@IPv6#1 address as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected bracketed user@IPv6#1 address as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected bracketed user@IPv6#1 address\n", ++testnum);
		return 1;
	}

	/* 22. Data in that user@IPv6#1 section, now inherited... */
	if (ac->certhost && !strcmp(ac->certhost, "An IPv6 Server")) {
		printf("ok %d - got expected CERTHOST in user@IPv6#1 section\n", ++testnum);
	} else {
		printf("not ok %d - did not get expected CERTHOST in user@IPv6#1 section\n", ++testnum);
		upscli_dump_authconf_item(NULL, ac, 1, 1);
		return 1;
	}

	/* 23. IPv6#2 in brackets */
	printf("Checking NO IPv6#2 match for a link-local address section without a port\n");
	ac = upscli_find_authconf_item(NULL, "[fe80::215:5dff:fea4:f781]", NULL);
	if (ac) {
		if (ac->section) {
			printf("not ok %d - got a hit by expected bracketed IPv6#2 address as the section name but without asking for the custom port\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		} else {
			printf("ok %d - got the global section by expected bracketed IPv6#2 address when not asking for the custom port\n", ++testnum);
		}
	} else {
		printf("ok %d - (sort of OK - expected global section) did not get any section by expected bracketed IPv6#2 address when not asking for the custom port\n", ++testnum);
	}

	/* 24. IPv6#2 in brackets */
	printf("Checking IPv6#2 match for a link-local address section\n");
	ac = upscli_find_authconf_item(NULL, "[fe80::215:5dff:fea4:f781]", "3495");
	if (ac) {
		if (ac->section && !strcmp(ac->section, "@[fe80::215:5dff:fea4:f781]:3495")) {
			printf("ok %d - got expected bracketed IPv6#2 address and port as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected bracketed IPv6#2 address and port as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected bracketed IPv6#2 address and port\n", ++testnum);
		return 1;
	}

	/* 25. Data in that IPv6#2 section... */
	if (ac->certhost && !strcmp(ac->certhost, "An IPv6 Server on port 3495")) {
		printf("ok %d - got expected CERTHOST in IPv6#2 section\n", ++testnum);
	} else {
		printf("not ok %d - did not get expected CERTHOST in IPv6#2 section\n", ++testnum);
		upscli_dump_authconf_item(NULL, ac, 1, 1);
		return 1;
	}
	/* FIXME: Find a host_port_cert for the bracketed IPv6 address, make sure it is the same
	 *  as the one in the section, and there are no hits for any other ports */

	/* 26. IPv6#3 in brackets */
	printf("Checking NO IPv6#3 match for a localhost address section without a port\n");
	ac = upscli_find_authconf_item(NULL, "[0::1]", NULL);
	if (ac) {
		if (ac->section) {
			printf("not ok %d - got a hit by expected bracketed IPv6#3 address as the section name but without asking for the custom port\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		} else {
			printf("ok %d - (sort of) got the global section by expected bracketed IPv6#3 address when not asking for the custom port\n", ++testnum);
		}
	} else {
		printf("ok %d - did not get any section by expected bracketed IPv6#3 address when not asking for the custom port\n", ++testnum);
	}

	/* 27. IPv6#3a in brackets */
	printf("Checking IPv6#3a match for a localhost address section\n");
	ac = upscli_find_authconf_item(NULL, "[::1]", "12345");
	if (ac) {
		if (ac->section && !strcmp(ac->section, "@[::1]:12345")) {
			printf("ok %d - got expected bracketed IPv6#3a address and port as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected bracketed IPv6#3a address and port as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected bracketed IPv6#3a address and port\n", ++testnum);
		return 1;
	}

	/* 28. Data in that IPv6#3 section... */
	/* FIXME: With proper IPv6 parsing, [::1] and [0::1] should collapse into the same section
	 *  or error out in case of conflicts/redefinitions; for now they are treated as separate */
	if (ac->user && !strcmp(ac->user, "IPv6user2")) {
		printf("ok %d - got expected USERNAME in IPv6#3a section\n", ++testnum);
	} else {
		printf("not ok %d - did not get expected USERNAME in IPv6#3a section\n", ++testnum);
		upscli_dump_authconf_item(NULL, ac, 1, 1);
		return 1;
	}

	/* 29. IPv6#3b in brackets */
	printf("Checking IPv6#3b match for a localhost address section\n");
	ac = upscli_find_authconf_item(NULL, "[0::1]", "12345");
	if (ac) {
		if (ac->section && !strcmp(ac->section, "@[0::1]:12345")) {
			printf("ok %d - got expected bracketed IPv6#3b address and port as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected bracketed IPv6#3b address and port as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected bracketed IPv6#3a address and port\n", ++testnum);
		return 1;
	}

	/* 30. Data in that IPv6#3 section... */
	/* FIXME: see above */
	if (ac->user && !strcmp(ac->user, "IPv6user")) {
		printf("ok %d - got expected USERNAME in IPv6#3b section\n", ++testnum);
	} else {
		printf("not ok %d - did not get expected USERNAME in IPv6#3b section\n", ++testnum);
		upscli_dump_authconf_item(NULL, ac, 1, 1);
		return 1;
	}

	/* 31. Empty hostname [bigadmin@:12345] should be saved by parser with "localhost" */
	printf("Checking empty hostname in address section becomes localhost\n");
	ac = upscli_find_authconf_item("bigadmin", "localhost", "12345");
	if (ac) {
		if (ac->section && !strcmp(ac->section, "bigadmin@localhost:12345")) {
			printf("ok %d - got expected user, address and port as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected user, address and port as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected user, address and port\n", ++testnum);
		return 1;
	}

	/* 32. Empty hostname [bigadmin@:12345] */
	printf("Checking empty hostname in address section becomes localhost\n");
	ac = upscli_find_authconf_item("bigadmin", "", "12345");
	if (ac) {
		if (ac->section && !strcmp(ac->section, "bigadmin@localhost:12345")) {
			printf("ok %d - got expected user, address (normalized from empty) and port as the section name\n", ++testnum);
		} else {
			printf("not ok %d - did not get expected user, address (normalized from empty) and port as the section name\n", ++testnum);
			upscli_dump_authconf_item(NULL, ac, 1, 1);
			return 1;
		}
	} else {
		printf("not ok %d - did not get any section by expected user, address (normalized from empty) and port\n", ++testnum);
		return 1;
	}

	/* 33. Expected printout 3 */
	printf("=== Parsed configuration (production view) after several 'get' operations with results caching:\n");
	/* Not "for_debug", but how would this info look in a config file */
	num_sections = upscli_dump_authconf_list(NULL, 0, 1);
	printf("===== Collected %" PRIuSIZE " sections\n\n", num_sections);
	/* Added '@somehost:port' and 'somebody@...' */
	printf("%sok %d - parsed %" PRIuSIZE " sections (including global and cached results), expected %" PRIuSIZE "\n",
		num_sections == expected_sections ? "" : "not ", ++testnum, num_sections, expected_sections);

	upscli_free_authconf_item(ac5);
	upscli_free_authconf_item(ac7);
	upscli_free_authconf_item(ac12);
	/* do not free ac8 and ac9 - they are added to list */

	upscli_free_authconf_list();
	unlink(test_conf);
	unlink(include_conf);

	printf("All tests passed!\n");
	return 0;
}
