/* test_upsdconf - numeric configuration parsing and storage for upsd
 *
 * Copyright (C) 2026 Network UPS Tools project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "conf.c"
#include "sstate.c"

static int failures = 0, checks = 0;

/* Storage normally owned by upsd.c and netssl.c. Keep the public types. */
int maxage = 15, tracking_delay = 3600;
int allow_no_device = 0, allow_not_all_listeners = 0;
nfds_t maxconn = 0;
char *statepath = NULL, *datapath = NULL;
upstype_t *firstups = NULL;
nut_ctype_t *firstclient = NULL;
char *certfile = NULL, *certpath = NULL, *certname = NULL, *certpasswd = NULL;
int disable_weak_ssl = 0;
#ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
int certrequest = 0;
#endif

/* These daemon operations must not be reached by the configuration tests. */
static void unexpected_call(const char *name)
{
	upslogx(LOG_ERR, "Unexpected %s", name);
	failures++;
}

upstype_t *get_ups_ptr(const char *name)
{
	NUT_UNUSED_VARIABLE(name);
	unexpected_call("get_ups_ptr");
	return NULL;
}

void listen_add(const char *addr, const char *port)
{
	NUT_UNUSED_VARIABLE(addr);
	NUT_UNUSED_VARIABLE(port);
	unexpected_call("listen_add");
}

void kick_login_clients(const char *name)
{
	NUT_UNUSED_VARIABLE(name);
	unexpected_call("kick_login_clients");
}

void close_oldest_client(void) { unexpected_call("close_oldest_client"); }
void user_flush(void) { unexpected_call("user_flush"); }
void user_load(void) { unexpected_call("user_load"); }

int tracking_set(const char *id, const char *value)
{
	NUT_UNUSED_VARIABLE(id);
	NUT_UNUSED_VARIABLE(value);
	unexpected_call("tracking_set");
	return 0;
}

char *tracking_get(const char *id)
{
	NUT_UNUSED_VARIABLE(id);
	unexpected_call("tracking_get");
	return NULL;
}

static char config_dir[128], config_file[160];

static void cleanup(void)
{
	unlink(config_file);
	rmdir(config_dir);
}

static uintmax_t stored_value(const char *option)
{
	if (!strcmp(option, "MAXAGE")) return (uintmax_t)maxage;
	if (!strcmp(option, "TRACKINGDELAY")) return (uintmax_t)tracking_delay;
	if (!strcmp(option, "MAXCONN")) return (uintmax_t)maxconn;
#if defined(WITH_SSL) && defined(WITH_CLIENT_CERTIFICATE_VALIDATION)
	if (!strcmp(option, "CERTREQUEST")) return (uintmax_t)certrequest;
#endif
	unexpected_call(option);
	return 0;
}

static void check_value(const char *option, const char *value, int accepted, uintmax_t expected)
{
	char *args[2];
	int result, reloading;
	FILE *f;
	uintmax_t actual;

	maxage = tracking_delay = 17;
	maxconn = 17;
#ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
	certrequest = 2;
#endif
	args[0] = xstrdup(option);
	args[1] = xstrdup(value);
	result = parse_upsd_conf_args(2, args);
	actual = stored_value(option);
	checks++;
	if (result != accepted || actual != expected) {
		upslogx(LOG_ERR, "FAIL %s '%s': accepted=%d, value=%" PRIuMAX
			"; expected %d, %" PRIuMAX, option, value, result, actual, accepted, expected);
		failures++;
	}
	free(args[0]);
	free(args[1]);

	/* parseconf discards control characters even inside quotes. The raw
	 * argument rejects a leading tab, but the file loader sees just "1". */
	if (!strcmp(value, "\t1")) expected = 1;

	/* Use the real file loader and tokenizer on startup and reload. A valid
	 * preceding directive must survive a rejected duplicate. */
	for (reloading = 0; reloading <= 1; reloading++) {
		f = fopen(config_file, "w");
		if (!f) fatal_with_errno(EXIT_FAILURE, "fopen test configuration");
		fprintf(f, "MAXAGE 17\nTRACKINGDELAY 17\nMAXCONN 17\n");
#if defined(WITH_SSL) && defined(WITH_CLIENT_CERTIFICATE_VALIDATION)
		fprintf(f, "CERTREQUEST REQUIRE\n");
#endif
		fprintf(f, "%s \"%s\"\n", option, value);
		if (fclose(f)) fatal_with_errno(EXIT_FAILURE, "fclose test configuration");
		load_upsdconf(reloading);
		actual = stored_value(option);
		checks++;
		if (actual != expected) {
			upslogx(LOG_ERR, "FAIL load_upsdconf(%d), %s '%s': value=%" PRIuMAX
				"; expected %" PRIuMAX, reloading, option, value, actual, expected);
			failures++;
		}
	}
}

int main(void)
{
	static const char *options[] = { "MAXAGE", "TRACKINGDELAY", "MAXCONN",
#if defined(WITH_SSL) && defined(WITH_CLIENT_CERTIFICATE_VALIDATION)
		"CERTREQUEST",
#endif
	};
	static const char *invalid[] = { "", " ", "\t", " 1", "\t1", "+1", "-1", "-0",
		"+", "-", "++1", "--1", "1x", "1.5", "1e2", "0x10", "1 2",
		"9999999999999999999999999999999999999999",
		"-9999999999999999999999999999999999999999" };
	size_t i, j;
	char number[128];
	uintmax_t limit, previous;
	FILE *f;
#ifndef WIN32
	upstype_t ups;
#endif

#ifndef WIN32
	umask(077);
#endif
	snprintf(config_dir, sizeof(config_dir), "test_upsdconf-%ld", (long)getpid());
#ifdef WIN32
	if (mkdir(config_dir))
#else
	if (mkdir(config_dir, 0700))
#endif
		fatal_with_errno(EXIT_FAILURE, "mkdir test configuration");
	snprintf(config_file, sizeof(config_file), "%s/upsd.conf", config_dir);
	atexit(cleanup);
	if (setenv("NUT_CONFPATH", config_dir, 1))
		fatal_with_errno(EXIT_FAILURE, "setenv NUT_CONFPATH");

	printf("Widths: int=%" PRIuSIZE ", long=%" PRIuSIZE ", nfds_t=%" PRIuSIZE "\n",
		sizeof(int), sizeof(long), sizeof(nfds_t));
	for (i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
		previous = !strcmp(options[i], "CERTREQUEST") ? 2 : 17;
		check_value(options[i], "0", 1, 0);
		check_value(options[i], "1", 1, 1);
		check_value(options[i], "2", 1, 2);
		check_value(options[i], "00015", 1, 15);
		check_value(options[i], "32 \t", 1, 32);
		check_value(options[i], "3600", 1, 3600);
		for (j = 0; j < sizeof(invalid) / sizeof(invalid[0]); j++)
			check_value(options[i], invalid[j], 0, previous);

		limit = (uintmax_t)INT_MAX;
		if (!strcmp(options[i], "MAXCONN")) {
			limit = (uintmax_t)((nfds_t)-1);
			if (limit > (uintmax_t)LONG_MAX) limit = (uintmax_t)LONG_MAX;
		}
		snprintf(number, sizeof(number), "%" PRIuMAX, limit - 1);
		check_value(options[i], number, 1, limit - 1);
		snprintf(number, sizeof(number), "%" PRIuMAX, limit);
		check_value(options[i], number, 1, limit);
		snprintf(number, sizeof(number), "%" PRIuMAX, limit + 1);
		check_value(options[i], number, 0, previous);
		snprintf(number, sizeof(number), "%" PRIuMAX, (uintmax_t)LONG_MAX + 1);
		check_value(options[i], number, 0, previous);
	}
#if defined(WITH_SSL) && defined(WITH_CLIENT_CERTIFICATE_VALIDATION)
	check_value("CERTREQUEST", "NO", 1, 0);
	check_value("CERTREQUEST", "REQUEST", 1, 1);
	check_value("CERTREQUEST", "REQUIRE", 1, 2);
	check_value("CERTREQUEST", "require", 0, 2);
	/* ssl_init still owns the 0..2 domain check and its fatal error policy. */
	check_value("CERTREQUEST", "3", 1, 3);
#endif

	f = fopen(config_file, "w");
	if (!f) fatal_with_errno(EXIT_FAILURE, "fopen whitespace configuration");
	fprintf(f, "\tMAXAGE\t31 \t# comment\nMAXAGE\n");
	if (fclose(f)) fatal_with_errno(EXIT_FAILURE, "fclose whitespace configuration");
	load_upsdconf(0);
	checks++;
	if (maxage != 31) failures++;

#ifndef WIN32
	/* No socket I/O is needed: fresh timestamps suppress the ping path. */
	memset(&ups, 0, sizeof(ups));
	ups.sock_fd = 0;
	ups.name = xstrdup("test");
	time(&ups.last_heard);
	ups.last_ping = ups.last_heard;
	checks++;
	if (sstate_dead(&ups, INT_MAX)) {
		upslogx(LOG_ERR, "FAIL MAXAGE INT_MAX: fresh driver data is stale");
		failures++;
	}
	free(ups.name);
#endif
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
