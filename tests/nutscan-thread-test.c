/* Offline regression checks for scanner admission and worker ownership.
 * Copyright (C) 2026 NUT contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "common.h"
#include "nut-scan.h"
#include "upsclient.h"
/* Test setup, execution and checks must run even with -DNDEBUG. */
#undef NDEBUG
#include <assert.h>

#if defined WIN32 && NUTSCAN_TEST_PROTOCOL == 4
/* Keep the allocation wrappers out of Windows inline socket helpers. */
# include <ws2tcpip.h>
# include <wspiapi.h>
#endif

#if defined HAVE_PTHREAD && (defined HAVE_PTHREAD_TRYJOIN || defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED)
static void *test_malloc(size_t size);
static void *test_realloc(void *ptr, size_t size);
static void test_free(void *ptr);
static int test_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*worker)(void *), void *arg);
# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
static int test_wait(sem_t *sem);
static int test_trywait(sem_t *sem);
#  ifdef HAVE_SEMAPHORE_UNNAMED
static int test_init(sem_t *sem, int shared, unsigned int value);
static int test_destroy(sem_t *sem);
#  else
static sem_t *test_open(const char *name, int flags, ...);
static int test_close(sem_t *sem);
static int test_unlink(const char *name);
#  endif
# endif

/* Compile the actual scanner loop. Only probes and injected resource
 * failures are replaced; threads, semaphore accounting and cleanup run.
 */
# define malloc test_malloc
# define realloc test_realloc
# define free test_free
# define pthread_create test_create
# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
#  define sem_wait test_wait
#  define sem_trywait test_trywait
#  ifdef HAVE_SEMAPHORE_UNNAMED
#   define sem_init test_init
#   define sem_destroy test_destroy
#  else
#   define sem_open test_open
#   define sem_close test_close
#   define sem_unlink test_unlink
#  endif
# endif

/* Intercept the shared helpers as well as the scanner loop. */
# include "nutscan-thread.c"

# if NUTSCAN_TEST_PROTOCOL == 1
#  include "scan_snmp.c"
# elif NUTSCAN_TEST_PROTOCOL == 2
#  include "scan_ipmi.c"
# elif NUTSCAN_TEST_PROTOCOL == 3
#  include "scan_nut.c"
# else
#  include "scan_xml_http.c"
# endif

# undef malloc
# undef realloc
# undef free
# undef pthread_create
# undef sem_wait
# undef sem_trywait
# undef sem_init
# undef sem_destroy
# undef sem_open
# undef sem_close
# undef sem_unlink

/* The library keeps this counter mutex private. The included scanner
 * owns the test instance and never runs a library scanner. */
# ifdef HAVE_PTHREAD_TRYJOIN
pthread_mutex_t threadcount_mutex;
# endif

static int fail_malloc, fail_realloc, fail_create, create_calls, completed;
static int allocations, auth_allocations;
static void *arguments[32];
static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_done = PTHREAD_COND_INITIALIZER;
static int completed_before_return;

static void *test_malloc(size_t size)
{
	void *ptr;
	size_t i;

	if (fail_malloc && --fail_malloc == 0) {
		return NULL;
	}
	ptr = malloc(size);
	assert(ptr != NULL);
	for (i = 0; i < sizeof(arguments) / sizeof(arguments[0]); i++) {
		if (arguments[i] == NULL) {
			arguments[i] = ptr;
			allocations++;
			return ptr;
		}
	}
	abort();
}

static void *test_realloc(void *ptr, size_t size)
{
	if (fail_realloc && --fail_realloc == 0) {
		return NULL;
	}
	return realloc(ptr, size);
}

static void test_free(void *ptr)
{
	size_t i;

	if (ptr != NULL) {
		for (i = 0; i < sizeof(arguments) / sizeof(arguments[0]); i++) {
			if (arguments[i] == ptr) {
				arguments[i] = NULL;
				allocations--;
				break;
			}
		}
	}
	free(ptr);
}

struct test_worker {
	void *(*worker)(void *);
	void *arg;
};

static void *offline_worker(void *opaque)
{
	struct test_worker *work = (struct test_worker *)opaque;

# if NUTSCAN_TEST_PROTOCOL == 3
	work->worker(work->arg);
# else
#  if NUTSCAN_TEST_PROTOCOL == 1
	nutscan_snmp_t *arg = (nutscan_snmp_t *)work->arg;
#  elif NUTSCAN_TEST_PROTOCOL == 2
	nutscan_ipmi_t *arg = (nutscan_ipmi_t *)work->arg;
#  else
	nutscan_xml_t *arg = (nutscan_xml_t *)work->arg;
#  endif
	test_free(arg->peername);
	test_free(arg);
# endif
	free(work);
	pthread_mutex_lock(&test_mutex);
	completed++;
	pthread_cond_signal(&test_done);
	pthread_mutex_unlock(&test_mutex);
	return NULL;
}

static int test_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*worker)(void *), void *arg)
{
	struct test_worker *work;
	int ret, previous;

	create_calls++;
	if (fail_create && --fail_create == 0) {
		return EAGAIN;
	}
	work = (struct test_worker *)malloc(sizeof(*work));
	assert(work != NULL);
	work->worker = worker;
	work->arg = arg;
	pthread_mutex_lock(&test_mutex);
	previous = completed;
	ret = pthread_create(thread, attr, offline_worker, work);
	if (ret != 0) {
		free(work);
	}
	while (ret == 0 && completed_before_return && completed == previous) {
		pthread_cond_wait(&test_done, &test_mutex);
	}
	pthread_mutex_unlock(&test_mutex);
	return ret;
}

# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
/* Borrowed only until the scanner destroys or closes its semaphore. */
static sem_t *global_sem, *protocol_sem;
static unsigned int protocol_capacity;
static int fail_init, interrupt_wait, fail_wait, fail_try_global, fail_try_protocol;
#  if defined HAVE_SEMAPHORE_NAMED && !defined HAVE_SEMAPHORE_UNNAMED
static char protocol_name[128];
#  endif

/* sem_getvalue() is unavailable on some supported systems. */
static void check_capacity(sem_t *sem, unsigned int expected)
{
	unsigned int actual = 0, i;

	while (sem_trywait(sem) == 0) {
		actual++;
	}
	assert(errno == EAGAIN);
	assert(actual == expected);
	for (i = 0; i < actual; i++) {
		assert(sem_post(sem) == 0);
	}
}

static int test_wait(sem_t *sem)
{
	assert(sem == global_sem || (protocol_sem != NULL && sem == protocol_sem));
	if (interrupt_wait && --interrupt_wait == 0) {
		errno = EINTR;
		return -1;
	}
	if (fail_wait && --fail_wait == 0) {
		errno = EINVAL;
		return -1;
	}
	return sem_wait(sem);
}

static int test_trywait(sem_t *sem)
{
	assert(sem == global_sem || (protocol_sem != NULL && sem == protocol_sem));
	if ((sem == global_sem && fail_try_global && --fail_try_global == 0)
	|| (sem == protocol_sem && fail_try_protocol && --fail_try_protocol == 0)
	) {
		errno = EAGAIN;
		return -1;
	}
	return sem_trywait(sem);
}

#  ifdef HAVE_SEMAPHORE_UNNAMED
static int test_init(sem_t *sem, int shared, unsigned int value)
{
	int ret;

	assert(sem != NULL && sem != global_sem && protocol_sem == NULL);
	if (fail_init) {
		errno = ENOSPC;
		return -1;
	}
	ret = sem_init(sem, shared, value);
	if (ret == 0) {
		protocol_sem = sem;
		protocol_capacity = value;
	}
	return ret;
}

static int test_destroy(sem_t *sem)
{
	int ret;

	assert(protocol_sem != NULL && sem == protocol_sem);
	check_capacity(sem, protocol_capacity);
	ret = sem_destroy(sem);
	assert(ret == 0);
	protocol_sem = NULL;
	return ret;
}
#  else
static sem_t *test_open(const char *name, int flags, ...)
{
	va_list ap;
	unsigned int value;

	NUT_UNUSED_VARIABLE(name);
	NUT_UNUSED_VARIABLE(flags);
	assert(protocol_sem == NULL);
	if (fail_init) {
		errno = ENOSPC;
		return SEM_FAILED;
	}
	va_start(ap, flags);
	(void)va_arg(ap, int);
	value = va_arg(ap, unsigned int);
	va_end(ap);
	snprintf(protocol_name, sizeof(protocol_name), "/nut-test-proto-%ld", (long)getpid());
	protocol_sem = sem_open(protocol_name, O_CREAT | O_EXCL, 0600, value);
	assert(protocol_sem != SEM_FAILED);
	assert(sem_unlink(protocol_name) == 0);
	protocol_capacity = value;
	return protocol_sem;
}

static int test_close(sem_t *sem)
{
	int ret;

	assert(protocol_sem != NULL && sem == protocol_sem);
	check_capacity(sem, protocol_capacity);
	ret = sem_close(sem);
	assert(ret == 0);
	protocol_sem = NULL;
	return ret;
}

static int test_unlink(const char *name)
{
	NUT_UNUSED_VARIABLE(name);
	return 0;
}
#  endif
# endif /* semaphore support */

# if NUTSCAN_TEST_PROTOCOL == 1
static char *offline_snmp_options(char *options)
{
	NUT_UNUSED_VARIABLE(options);
	return NULL;
}
# endif

# if NUTSCAN_TEST_PROTOCOL == 3
static upscli_authconf_t *offline_auth_get(const char *user, const char *host,
	const char *port, int add_to_list)
{
	upscli_authconf_t *auth = (upscli_authconf_t *)calloc(1, sizeof(*auth));

	NUT_UNUSED_VARIABLE(user);
	NUT_UNUSED_VARIABLE(host);
	NUT_UNUSED_VARIABLE(port);
	NUT_UNUSED_VARIABLE(add_to_list);
	assert(auth != NULL);
	auth_allocations++;
	return auth;
}

static upscli_authconf_t *offline_auth_find(const char *user, const char *host, const char *port)
{
	NUT_UNUSED_VARIABLE(user);
	NUT_UNUSED_VARIABLE(host);
	NUT_UNUSED_VARIABLE(port);
	return NULL;
}

static int offline_auth_init(upscli_authconf_t *auth)
{
	assert(auth != NULL);
	return 0;
}

static void offline_auth_free(upscli_authconf_t *auth)
{
	assert(auth != NULL && auth_allocations > 0);
	auth_allocations--;
	free(auth);
}

static int offline_auth_read(const char *filename, int fatal_errors, int debug_level)
{
	NUT_UNUSED_VARIABLE(filename);
	NUT_UNUSED_VARIABLE(fatal_errors);
	NUT_UNUSED_VARIABLE(debug_level);
	return 0;
}

static int offline_authenticate(UPSCONN_t *ups, upscli_authconf_t *auth)
{
	NUT_UNUSED_VARIABLE(ups);
	assert(auth != NULL && auth_allocations > 0);
	return 0;
}

static int offline_splitaddr(const char *target, char **host, uint16_t *port)
{
	assert(target != NULL && strlen(target) > 0);
	assert(auth_allocations > 0);
	NUT_UNUSED_VARIABLE(host);
	NUT_UNUSED_VARIABLE(port);
	return -1;
}

static void offline_free_cert(const char *host, const char *cert)
{
	assert(host != NULL && strlen(host) > 0);
	NUT_UNUSED_VARIABLE(cert);
}
# endif

static void scan(unsigned int limit)
{
	nutscan_ip_range_list_t ranges;
# if NUTSCAN_TEST_PROTOCOL == 1
	nutscan_snmp_t settings;
# elif NUTSCAN_TEST_PROTOCOL == 2
	nutscan_ipmi_t settings;
# elif NUTSCAN_TEST_PROTOCOL == 3
	nutscan_nut_authconf_t settings;
# else
	nutscan_xml_t settings;
# endif

	memset(&settings, 0, sizeof(settings));
	nutscan_init_ip_ranges(&ranges);
	nutscan_add_ip_range(&ranges, strdup("192.0.2.1"), strdup("192.0.2.5"));
# if NUTSCAN_TEST_PROTOCOL == 1
	max_threads_netsnmp = limit;
	nutscan_avail_snmp = 1;
	nut_initialized_snmp = 1;
	nut_snmp_out_toggle_options = offline_snmp_options;
	assert(nutscan_scan_ip_range_snmp(&ranges, 1, &settings) == NULL);
# elif NUTSCAN_TEST_PROTOCOL == 2
	max_threads_ipmi = limit;
	nutscan_avail_ipmi = 1;
	assert(nutscan_scan_ip_range_ipmi(&ranges, &settings) == NULL);
# elif NUTSCAN_TEST_PROTOCOL == 3
	max_threads_oldnut = limit;
	nutscan_avail_nut = 1;
	nut_upscli_splitaddr = offline_splitaddr;
	nut_upscli_get_authconf_item = offline_auth_get;
	nut_upscli_find_authconf_item = offline_auth_find;
	nut_upscli_init_authconf = offline_auth_init;
	nut_upscli_free_authconf_item = offline_auth_free;
	nut_upscli_read_authconf_file = offline_auth_read;
	nut_upscli_authenticate_authconf = offline_authenticate;
	nut_upscli_free_host_cert = offline_free_cert;
	settings.authconf_file = "none";
	assert(nutscan_scan_ip_range_nut_authconf(&ranges, &settings) == NULL);
# else
	max_threads_netxml = limit;
	nutscan_avail_xml_http = 1;
	assert(nutscan_scan_ip_range_xml_http(&ranges, 1, &settings) == NULL);
# endif
	nutscan_free_ip_ranges(&ranges);
}

static void scenario(unsigned int limit, int failure)
{
# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
#  if defined HAVE_SEMAPHORE_NAMED && !defined HAVE_SEMAPHORE_UNNAMED
	char name[128];
#  endif
# endif
	fail_malloc = fail_realloc = fail_create = 0;
	create_calls = completed = 0;
	completed_before_return = 1;
	assert(allocations == 0);
	assert(auth_allocations == 0);
	if (failure == 1) {
		fail_malloc = 1;
	} else if (failure == 2) {
		fail_realloc = 1;
	} else if (failure == 3) {
		fail_realloc = 2;
	} else if (failure == 4) {
		fail_create = 1;
	} else if (failure == 5) {
		fail_create = 2;
	}
	max_threads = 2;
	curr_threads = 0;
# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
	assert(protocol_sem == NULL);
	fail_init = failure == 6;
	interrupt_wait = failure == 7 ? 1 : 0;
	fail_wait = failure == 8 ? 1 : (failure == 9 ? 2 : 0);
	fail_try_global = failure == 10 ? 1 : 0;
	fail_try_protocol = failure == 11 ? 1 : 0;
#  ifdef HAVE_SEMAPHORE_UNNAMED
	global_sem = nutscan_semaphore();
	assert(sem_init(global_sem, 0, 2) == 0);
#  else
	snprintf(name, sizeof(name), "/nut-test-global-%ld", (long)getpid());
	global_sem = sem_open(name, O_CREAT | O_EXCL, 0600, 2);
	assert(global_sem != SEM_FAILED);
	assert(sem_unlink(name) == 0);
	nutscan_semaphore_set(global_sem);
#  endif
# endif
# ifdef HAVE_PTHREAD_TRYJOIN
	assert(pthread_mutex_init(&threadcount_mutex, NULL) == 0);
# endif
	scan(limit);
	assert(allocations == 0);
	assert(auth_allocations == 0);
# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
	check_capacity(global_sem, 2);
	assert(protocol_sem == NULL);
#  ifdef HAVE_SEMAPHORE_UNNAMED
	assert(sem_destroy(global_sem) == 0);
#  else
	assert(sem_close(global_sem) == 0);
	nutscan_semaphore_set(NULL);
#  endif
	global_sem = NULL;
# endif
	assert(curr_threads == 0);
	if (failure == 1 || failure == 2 || failure == 8 || failure == 9) {
		assert(completed == 0);
	} else if (failure == 3) {
		assert(completed == 1);
	} else if (failure == 4 || failure == 5) {
		assert(completed == 4 && create_calls == 5);
	} else {
		assert(completed == 5 && create_calls == 5);
	}
# ifdef HAVE_PTHREAD_TRYJOIN
	assert(pthread_mutex_destroy(&threadcount_mutex) == 0);
# endif
	printf("protocol=%d limit=%u failure=%d: PASS\n", NUTSCAN_TEST_PROTOCOL, limit, failure);
}
#endif /* HAVE_PTHREAD */

int main(int argc, char **argv)
{
#if defined HAVE_PTHREAD && (defined HAVE_PTHREAD_TRYJOIN || defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED)
	unsigned int limits[] = {0, 1, 3};
	size_t i;
	int failure, repeat;
# ifdef WIN32
	WSADATA wsa_data;

	assert(WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0);
# endif

	if (argc == 3) {
		scenario((unsigned int)atoi(argv[1]), atoi(argv[2]));
# ifdef WIN32
		assert(WSACleanup() == 0);
# endif
		return EXIT_SUCCESS;
	}

	for (repeat = 0; repeat < 2; repeat++) {
		for (i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
			for (failure = 0; failure < 12; failure++) {
# if !defined HAVE_SEMAPHORE_UNNAMED && !defined HAVE_SEMAPHORE_NAMED
				if (failure >= 6) {
					continue;
				}
# endif
				if (limits[i] == 0 && (failure == 9 || failure == 11)) {
					continue;
				}
				scenario(limits[i], failure);
			}
		}
	}
# ifdef WIN32
	assert(WSACleanup() == 0);
# endif
	return EXIT_SUCCESS;
#else
	NUT_UNUSED_VARIABLE(argc);
	NUT_UNUSED_VARIABLE(argv);
	return 77;
#endif
}
