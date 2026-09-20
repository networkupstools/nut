/* Offline checks of SNMP session-open recovery through the actual scanner.
 * Copyright (C) 2026 NUT contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "common.h"
#include "nut-scan.h"
/* Test setup, execution and checks must run even with -DNDEBUG. */
#undef NDEBUG
#include <assert.h>

static void *test_malloc(size_t size);
static void test_free(void *ptr);
#ifdef HAVE_PTHREAD
static void *test_realloc(void *ptr, size_t size);
static int test_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*worker)(void *), void *arg);
# define realloc test_realloc
# define pthread_create test_create
# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
static int test_wait(sem_t *sem);
#  define sem_wait test_wait
# endif
#endif
#define malloc test_malloc
#define free test_free
#include "nutscan-thread.c"
#include "scan_snmp.c"
#undef malloc
#undef free
#undef realloc
#undef pthread_create
#undef sem_wait

#ifdef HAVE_PTHREAD
static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
# ifdef HAVE_PTHREAD_TRYJOIN
pthread_mutex_t threadcount_mutex = PTHREAD_MUTEX_INITIALIZER;
# endif
# define LOCK() pthread_mutex_lock(&test_mutex)
# define UNLOCK() pthread_mutex_unlock(&test_mutex)
#else
# define LOCK() ((void)0)
# define UNLOCK() ((void)0)
#endif

static int mode, attempts[5], closed[5], malloc_calls, fail_malloc;
static void *allocations[16];
static int test_snmp_error;
#ifdef HAVE_PTHREAD
static int realloc_calls, create_calls;
#endif

static void *test_malloc(size_t size)
{
	void *ptr;
	size_t i;

	LOCK();
	malloc_calls++;
	if (malloc_calls == fail_malloc) {
		UNLOCK();
		return NULL;
	}
	ptr = malloc(size);
	assert(ptr != NULL);
	for (i = 0; i < sizeof(allocations) / sizeof(allocations[0]); i++) {
		if (allocations[i] == NULL) {
			allocations[i] = ptr;
			UNLOCK();
			return ptr;
		}
	}
	abort();
}

static void test_free(void *ptr)
{
	size_t i;

	LOCK();
	for (i = 0; i < sizeof(allocations) / sizeof(allocations[0]); i++) {
		if (allocations[i] == ptr) {
			allocations[i] = NULL;
			break;
		}
	}
	free(ptr);
	UNLOCK();
}

#ifdef HAVE_PTHREAD
static void *test_realloc(void *ptr, size_t size)
{
	if (++realloc_calls == 3 && fail_malloc == -2) {
		return NULL;
	}
	return realloc(ptr, size);
}

static int test_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*worker)(void *), void *arg)
{
	if (++create_calls == 3 && fail_malloc == -3) {
		return EAGAIN;
	}
	return pthread_create(thread, attr, worker, arg);
}

# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
static int test_wait(sem_t *sem)
{
	if (malloc_calls > 5 && fail_malloc == -4) {
		errno = EINVAL;
		return -1;
	}
	return sem_wait(sem);
}
# endif
#endif

static int host_index(const char *host)
{
	int index = host[strlen(host) - 1] - '1';
	assert(index >= 0 && index < 5);
	return index;
}

static void session_init(netsnmp_session *session)
{
	memset(session, 0, sizeof(*session));
	/* An unrelated earlier call must not cause a false retry. */
	errno = EMFILE;
}

static void *session_open(netsnmp_session *session)
{
	int index = host_index(session->peername);
	int attempt;

	LOCK();
	attempt = ++attempts[index];
	UNLOCK();
	if (mode == 3) {
		session->s_errno = EACCES;
		errno = EMFILE;
		return NULL;
	}
	if (mode == 4) {
		/* Leave errno untouched after the caller cleared stale EMFILE. */
		return NULL;
	}
	if (mode == 2 || (mode != 0 && attempt == 1)) {
		session->s_errno = mode == 6 ? 0 : EMFILE;
		errno = mode == 5 ? EACCES : EMFILE;
		return NULL;
	}
	return session->peername;
}

static int session_close(void *handle)
{
	LOCK();
	closed[host_index((const char *)handle)]++;
	UNLOCK();
	return 1;
}

static void *parse_oid(const char *input, oid *output, size_t *length)
{
	NUT_UNUSED_VARIABLE(input);
	NUT_UNUSED_VARIABLE(output);
	NUT_UNUSED_VARIABLE(length);
	/* Stop after a successful session open. Protocol discovery is separate. */
	return NULL;
}

static char *toggle_options(char *options)
{
	NUT_UNUSED_VARIABLE(options);
	return NULL;
}

static const char *error_string(int error)
{
	NUT_UNUSED_VARIABLE(error);
	return "offline probe complete";
}

static void scenario(int error_mode, unsigned int limit, int allocation_failure)
{
	nutscan_ip_range_list_t ranges;
	nutscan_snmp_t settings;
	size_t i;
#if defined HAVE_PTHREAD && (defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED)
	sem_t *global;
# if !defined HAVE_SEMAPHORE_UNNAMED
	char name[128];
# endif
#endif
	mode = error_mode;
	memset(attempts, 0, sizeof(attempts));
	memset(closed, 0, sizeof(closed));
	malloc_calls = 0;
#ifdef HAVE_PTHREAD
	realloc_calls = create_calls = 0;
#endif
	fail_malloc = allocation_failure;
	memset(&settings, 0, sizeof(settings));
	settings.community = "public";
	nutscan_init_ip_ranges(&ranges);
	nutscan_add_ip_range(&ranges, strdup("192.0.2.1"), strdup("192.0.2.5"));
#if defined HAVE_PTHREAD && (defined HAVE_PTHREAD_TRYJOIN || defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED)
	max_threads = 2;
	max_threads_netsnmp = limit;
	curr_threads = 0;
#else
	NUT_UNUSED_VARIABLE(limit);
#endif
#if defined HAVE_PTHREAD && (defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED)
# if defined HAVE_SEMAPHORE_UNNAMED
	global = nutscan_semaphore();
	assert(sem_init(global, 0, 2) == 0);
# else
	snprintf(name, sizeof(name), "/nut-snmp-retry-%ld", (long)getpid());
	global = sem_open(name, O_CREAT | O_EXCL, 0600, 2);
	assert(global != SEM_FAILED);
	assert(sem_unlink(name) == 0);
	nutscan_semaphore_set(global);
# endif
#endif
	assert(nutscan_scan_ip_range_snmp(&ranges, 1, &settings) == NULL);
	nutscan_free_ip_ranges(&ranges);
	for (i = 0; i < sizeof(allocations) / sizeof(allocations[0]); i++) {
		assert(allocations[i] == NULL);
	}
#if defined HAVE_PTHREAD && (defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED)
	assert(sem_trywait(global) == 0);
	assert(sem_trywait(global) == 0);
	assert(sem_trywait(global) == -1 && errno == EAGAIN);
# if defined HAVE_SEMAPHORE_UNNAMED
	assert(sem_destroy(global) == 0);
# else
	assert(sem_close(global) == 0);
	nutscan_semaphore_set(NULL);
# endif
#endif
#if defined HAVE_PTHREAD && (defined HAVE_PTHREAD_TRYJOIN || defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED)
	assert(curr_threads == 0);
#endif
	for (i = 0; i < 5; i++) {
		int expected_attempts = 1;
		int expected_closed = mode == 0 ? 1 : 0;
#ifdef HAVE_PTHREAD
		if (mode == 1 || mode == 2 || mode == 5 || mode == 6) {
			expected_attempts = 2;
			expected_closed = mode == 2 ? 0 : 1;
		}
#endif
		if (allocation_failure == 2 && i > 0) {
			expected_attempts = expected_closed = 0;
		}
#ifdef HAVE_PTHREAD
		if ((allocation_failure == -2 && i >= 2)
		|| (allocation_failure == -3 && i == 2)
		) {
			expected_attempts = expected_closed = 0;
		}
		if (allocation_failure == 6 || allocation_failure == -4) {
			expected_attempts = 1;
			expected_closed = 0;
		}
#endif
		assert(attempts[i] == expected_attempts);
		assert(closed[i] == expected_closed);
	}
	printf("mode=%d limit=%u allocation_failure=%d: PASS\n", mode, limit, allocation_failure);
}

int main(void)
{
	int error_mode;
	unsigned int limit;

	nutscan_avail_snmp = 1;
	nut_initialized_snmp = 1;
	nut_snmp_sess_init = session_init;
	nut_snmp_sess_open = session_open;
	nut_snmp_sess_close = session_close;
	nut_snmp_parse_oid = parse_oid;
	nut_snmp_out_toggle_options = toggle_options;
	nut_snmp_api_errstring = error_string;
	nut_snmp_errno = &test_snmp_error;
	for (limit = 0; limit <= 3; limit++) {
		for (error_mode = 0; error_mode <= 6; error_mode++) {
			scenario(error_mode, limit, 0);
		}
		scenario(1, limit, 2);
#ifdef HAVE_PTHREAD
		scenario(1, limit, 6);
		scenario(1, limit, -2);
		scenario(1, limit, -3);
# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
		scenario(1, limit, -4);
# endif
#endif
	}
	return EXIT_SUCCESS;
}
