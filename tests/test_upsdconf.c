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
#include "common.h"
#include "parseconf.h"

static FILE *test_fopen(const char *path, const char *mode);
static int test_pconf_file_begin(PCONF_CTX_t *ctx, const char *path);
static int test_pconf_init(PCONF_CTX_t *ctx, void errhandler(const char *));
static void test_pconf_finish(PCONF_CTX_t *ctx);

#define fopen test_fopen
#define pconf_file_begin test_pconf_file_begin
#include "conf.c"
#undef fopen
#define pconf_init test_pconf_init
#define pconf_finish test_pconf_finish
#include "../common/upsconf.c"
#undef pconf_init
#undef pconf_finish
#undef pconf_file_begin
#include "sstate.c"
#ifndef WIN32
#include <sys/wait.h>
#endif

static int failures = 0, checks = 0;
static int open_error = 0, reclaimable_clients = 0;
static int close_calls = 0, expected_close_calls = 0;
static int client_releases_file = 1, parser_contexts = 0;
static pid_t test_pid;
#ifndef WIN32
static int reclaim_fd = -1;
#endif

static int test_pconf_init(PCONF_CTX_t *ctx, void errhandler(const char *))
{
	parser_contexts++;
	return pconf_init(ctx, errhandler);
}

static void test_pconf_finish(PCONF_CTX_t *ctx)
{
	pconf_finish(ctx);
	parser_contexts--;
	/* Cleanup must not replace the error used by the reload caller. */
	errno = EINVAL;
}

static int test_open_error(void)
{
#ifndef WIN32
	if (reclaim_fd >= 0 && fcntl(reclaim_fd, F_GETFD) == -1 && errno == EBADF) {
		open_error = 0;
	}
#endif
	return open_error;
}

static FILE *test_fopen(const char *path, const char *mode)
{
	if (test_open_error()) {
		errno = open_error;
		return NULL;
	}
	return fopen(path, mode);
}

static int test_pconf_file_begin(PCONF_CTX_t *ctx, const char *path)
{
	if (test_open_error()) {
		snprintf(ctx->errmsg, PCONF_ERR_LEN, "Injected configuration open failure");
		errno = open_error;
		return 0;
	}
	return pconf_file_begin(ctx, path);
}

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

/* Unrelated daemon operations must not be reached by these tests. */
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

int close_oldest_client(void)
{
	close_calls++;
	/* Bound the test even if the loader regresses to an endless retry. */
	if (close_calls > expected_close_calls) {
		fatalx(2, "Unexpected configuration-open retry");
	}
	if (reclaimable_clients) {
		reclaimable_clients--;
		if (client_releases_file) {
			open_error = 0;
		}
		return 1;
	}
	return 0;
}
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
	static char result[] = "unexpected tracking_get";
	NUT_UNUSED_VARIABLE(id);
	unexpected_call("tracking_get");
	return result;
}

static char config_dir[128], config_file[160];

static void cleanup(void)
{
	char	fn[160];

	if (getpid() != test_pid) {
		return;
	}
	unlink(config_file);
	snprintf(fn, sizeof(fn), "%s/ups.conf", config_dir);
	unlink(fn);
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

static void check_open_failure(int error, int clients, int reloading, int expected)
{
	open_error = error;
	reclaimable_clients = clients;
	close_calls = 0;
	expected_close_calls = (error == EMFILE && reloading == 2);
	maxage = 17;
	load_upsdconf(reloading);
	checks++;
	if (maxage != expected || close_calls != expected_close_calls) {
		upslogx(LOG_ERR, "FAIL reload open error %d, clients %d, mode %d",
			error, clients, reloading);
		failures++;
	}

	open_error = error;
	reclaimable_clients = clients;
	close_calls = 0;
	expected_close_calls = (error == EMFILE);
	checks++;
	if (check_file("upsd.conf") != (error == EMFILE && clients > 0)
	||  close_calls != expected_close_calls
	) {
		upslogx(LOG_ERR, "FAIL precheck open error %d, clients %d", error, clients);
		failures++;
	}
	open_error = 0;
}

#ifndef WIN32
/* Exercise the real driver cleanup with file descriptors owned by this test.
 * Open failures persist until the selected descriptor has actually closed.
 */
static void check_driver_reclaim(int loader, int clients, int effective_client, int recover)
{
	upstype_t	oldest, newer, disconnected;
	int	result = 0, status;
	pid_t	child;

	memset(&oldest, 0, sizeof(oldest));
	memset(&newer, 0, sizeof(newer));
	memset(&disconnected, 0, sizeof(disconnected));
	oldest.name = "oldest";
	newer.name = "newer";
	disconnected.name = "disconnected";
	oldest.sock_fd = open(config_file, O_RDONLY);
	newer.sock_fd = open(config_file, O_RDONLY);
	if (INVALID_FD(oldest.sock_fd) || INVALID_FD(newer.sock_fd)) {
		fatal_with_errno(EXIT_FAILURE, "open test driver descriptor");
	}
	disconnected.sock_fd = ERROR_FD;
	oldest.last_heard = 10;
	newer.last_heard = 20;
	disconnected.next = &newer;
	newer.next = &oldest;
	firstups = &disconnected;
	oldest.numlogins = 3;
	oldest.fsd = oldest.retain = 1;
	pconf_init(&oldest.sock_ctx, NULL);
	pconf_init(&newer.sock_ctx, NULL);
	state_setinfo(&oldest.inforoot, "ups.status", "OB");
	state_addcmd(&oldest.cmdlist, "test.command");

	open_error = EMFILE;
	reclaimable_clients = clients;
	client_releases_file = effective_client;
	close_calls = 0;
	expected_close_calls = effective_client ? 1 : clients + (recover ? 1 : 3);
	reclaim_fd = recover ? oldest.sock_fd : -1;
	maxage = 17;
	if (loader == 0) {
		load_upsdconf(2);
		result = (maxage == 31);
	} else if (loader == 1) {
		result = check_file("upsd.conf");
	} else if (recover) {
		result = (load_upsconf(2) == 1);
	} else {
		fflush(NULL);
		child = fork();
		if (child == -1) {
			fatal_with_errno(EXIT_FAILURE, "fork reload failure test");
		}
		if (child == 0) {
			load_upsconf(2);
			_exit(0);
		}
		if (waitpid(child, &status, 0) != child) {
			fatal_with_errno(EXIT_FAILURE, "waitpid reload failure test");
		}
		checks++;
		if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_FAILURE) {
			failures++;
		}
		/* The child closed its own descriptors and retained fatal policy. */
		goto cleanup;
	}
	checks++;
	if (result != recover || close_calls != expected_close_calls
	|| parser_contexts != 0 || oldest.numlogins != 3 || !oldest.fsd || !oldest.retain
	|| (!effective_client && (VALID_FD(oldest.sock_fd) || oldest.inforoot || oldest.cmdlist))
	|| (effective_client && (INVALID_FD(oldest.sock_fd) || !oldest.inforoot || !oldest.cmdlist))
	|| (recover && INVALID_FD(newer.sock_fd))
	|| (!recover && VALID_FD(newer.sock_fd))
	) {
		upslogx(LOG_ERR, "FAIL driver reclaim: loader %d, clients %d, effective %d, recover %d",
			loader, clients, effective_client, recover);
		failures++;
	}

cleanup:
	sstate_disconnect(&oldest);
	sstate_disconnect(&newer);
	firstups = NULL;
	reclaim_fd = -1;
	open_error = 0;
	client_releases_file = 1;
}
#endif

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
	char upsconf_file[160];
#ifndef WIN32
	upstype_t ups;
#endif

#ifndef WIN32
	umask(077);
#endif
	test_pid = getpid();
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
	check_open_failure(EMFILE, 1, 2, 31);
	check_open_failure(EMFILE, 0, 2, 17);
	check_open_failure(EMFILE, 1, 1, 17);
	check_open_failure(EACCES, 1, 2, 17);

	/* The shared nonfatal reader must release its parser allocation and
	 * retain the original errno even when cleanup changes it.
	 */
	for (i = 0; i < 2; i++) {
		open_error = i ? EACCES : EMFILE;
		checks++;
		if (read_upsconf(0) != -1 || errno != open_error || parser_contexts != 0) {
			upslogx(LOG_ERR, "FAIL nonfatal ups.conf cleanup, error %d", open_error);
			failures++;
		}
	}
	open_error = 0;
	snprintf(upsconf_file, sizeof(upsconf_file), "%s/ups.conf", config_dir);
	f = fopen(upsconf_file, "w");
	if (!f || fclose(f)) {
		fatal_with_errno(EXIT_FAILURE, "create empty ups.conf");
	}

#ifndef WIN32
	for (i = 0; i < 3; i++) {
		check_driver_reclaim((int)i, 12, 0, 1);
		check_driver_reclaim((int)i, 2, 1, 1);
		check_driver_reclaim((int)i, 0, 0, 0);
	}
#else
	/* Windows retains the existing ten-client precheck limit. */
	open_error = EMFILE;
	reclaimable_clients = 12;
	client_releases_file = 0;
	close_calls = 0;
	expected_close_calls = 10;
	checks++;
	if (check_file("upsd.conf") || close_calls != 10 || reclaimable_clients != 2) {
		upslogx(LOG_ERR, "FAIL Windows configuration precheck retry limit");
		failures++;
	}
	open_error = 0;
	client_releases_file = 1;
#endif

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
