/*
 *  Copyright (C) 2011 - 2023 Arnaud Quette (Design and part of implementation)
 *  Copyright (C) 2011 - EATON
 *  Copyright (C) 2016-2021 - EATON - Various threads-related improvements
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*! \file scan_nut.c
    \brief detect remote NUT services
    \author Frederic Bohe <fredericbohe@eaton.com>
    \author Jim Klimov <EvgenyKlimov@eaton.com>
    \author Arnaud Quette <arnaudquette@free.fr>
*/

#include "common.h"
#include "upsclient.h"
#include "nut-scan.h"
#include "nut_stdint.h"
#include <ltdl.h>

#define SCAN_NUT_DRIVERNAME "dummy-ups"

/* dynamic link library stuff */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;

static int (*nut_upscli_splitaddr)(const char *buf, char **hostname, uint16_t *port);
static int (*nut_upscli_tryconnect)(UPSCONN_t *ups, const char *host, uint16_t port,
					int flags, struct timeval * timeout);
static int (*nut_upscli_list_start)(UPSCONN_t *ups, size_t numq,
					const char **query);
static int (*nut_upscli_list_next)(UPSCONN_t *ups, size_t numq,
			const char **query, size_t *numa, char ***answer);
static int (*nut_upscli_disconnect)(UPSCONN_t *ups);

static nutscan_device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE = 0, TRUE } bool_t;
#else
typedef int bool_t;
#endif

struct scan_nut_arg {
	char * hostname;
	useconds_t timeout;
};

/* return 0 on error; visible externally */
int nutscan_load_upsclient_library(const char *libname_path);
int nutscan_load_upsclient_library(const char *libname_path)
{
	if (dl_handle != NULL) {
			/* if previous init failed */
			if (dl_handle == (void *)1) {
					return 0;
			}
			/* init has already been done */
			return 1;
	}

	if (libname_path == NULL) {
		fprintf(stderr, "NUT client library not found. NUT search disabled.\n");
		return 0;
	}

	if (lt_dlinit() != 0) {
			fprintf(stderr, "Error initializing lt_init\n");
			return 0;
	}

	dl_handle = lt_dlopen(libname_path);
	if (!dl_handle) {
			dl_error = lt_dlerror();
			goto err;
	}

	lt_dlerror();      /* Clear any existing error */

	*(void **) (&nut_upscli_splitaddr) = lt_dlsym(dl_handle,
						"upscli_splitaddr");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_upscli_tryconnect) = lt_dlsym(dl_handle,
						"upscli_tryconnect");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_upscli_list_start) = lt_dlsym(dl_handle,
						"upscli_list_start");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_upscli_list_next) = lt_dlsym(dl_handle,
						"upscli_list_next");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	*(void **) (&nut_upscli_disconnect) = lt_dlsym(dl_handle,
						"upscli_disconnect");
	if ((dl_error = lt_dlerror()) != NULL) {
			goto err;
	}

	return 1;

err:
	fprintf(stderr,
		"Cannot load NUT library (%s) : %s. NUT search disabled.\n",
		libname_path, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}

/* FIXME: SSL support */
static void * list_nut_devices(void * arg)
{
	struct scan_nut_arg * nut_arg = (struct scan_nut_arg*)arg;
	char *target_hostname = nut_arg->hostname;
	struct timeval tv;
	uint16_t port;
	size_t numq, numa;
	const char *query[4];
	char **answer;
	char *hostname = NULL;
	UPSCONN_t *ups = malloc(sizeof(*ups));
	nutscan_device_t * dev = NULL;
	size_t buf_size;

	tv.tv_sec = nut_arg->timeout / (1000*1000);
	tv.tv_usec = nut_arg->timeout % (1000*1000);

	query[0] = "UPS";
	numq = 1;

	if ((*nut_upscli_splitaddr)(target_hostname, &hostname, &port) != 0) {
		free(target_hostname);
		free(nut_arg);
		free(ups);
		return NULL;
	}

	if ((*nut_upscli_tryconnect)(ups, hostname, port, UPSCLI_CONN_TRYSSL, &tv) < 0) {
		free(target_hostname);
		free(nut_arg);
		free(ups);
		return NULL;
	}

	if ((*nut_upscli_list_start)(ups, numq, query) < 0) {
		(*nut_upscli_disconnect)(ups);
		free(target_hostname);
		free(nut_arg);
		free(ups);
		return NULL;
	}

	while ((*nut_upscli_list_next)(ups, numq, query, &numa, &answer) == 1) {
		/* UPS <upsname> <description> */
		if (numa < 3) {
			(*nut_upscli_disconnect)(ups);
			free(target_hostname);
			free(nut_arg);
			free(ups);
			return NULL;
		}
		/* FIXME: check for duplication by getting driver.port and device.serial
		 * for comparison with other busses results */
		/* FIXME:
		 * - also print answer[2] if != "Unavailable"?
		 * - for upsmon.conf or ups.conf (using dummy-ups)? */
		dev = nutscan_new_device();
		dev->type = TYPE_NUT;
		/* NOTE: There is no driver by such name, in practice it could
		 * be a dummy-ups relay, a clone driver, or part of upsmon config */
		dev->driver = strdup(SCAN_NUT_DRIVERNAME);
		/* +1+1 is for '@' character and terminating 0 */
		buf_size = strlen(answer[1]) + strlen(hostname) + 1 + 1;
		if (port != PORT) {
			/* colon and up to 5 digits */
			buf_size += 6;
		}

		dev->port = malloc(buf_size);

		if (dev->port) {
			if (port != PORT) {
				snprintf(dev->port, buf_size, "%s@%s:%" PRIu16,
					answer[1], hostname, port);
			} else {
				/* Standard port, not suffixed */
				snprintf(dev->port, buf_size, "%s@%s",
					answer[1], hostname);
			}
#ifdef HAVE_PTHREAD
			pthread_mutex_lock(&dev_mutex);
#endif
			dev_ret = nutscan_add_device_to_device(dev_ret, dev);
#ifdef HAVE_PTHREAD
			pthread_mutex_unlock(&dev_mutex);
#endif
		}

	}

	(*nut_upscli_disconnect)(ups);
	free(target_hostname);
	free(nut_arg);
	free(ups);
	return NULL;
}

nutscan_device_t * nutscan_scan_nut(const char* startIP, const char* stopIP, const char* port, useconds_t usec_timeout)
{
	bool_t pass = TRUE; /* Track that we may spawn a scanning thread */
	nutscan_ip_iter_t ip;
	char * ip_str = NULL;
	char * ip_dest = NULL;
	char buf[SMALLBUF];
#ifndef WIN32
	struct sigaction oldact;
	int change_action_handler = 0;
#endif
	struct scan_nut_arg *nut_arg;
#ifdef WIN32
	WSADATA WSAdata;
	WSAStartup(2,&WSAdata);
	atexit((void(*)(void))WSACleanup);
#endif

#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE
	sem_t * semaphore = nutscan_semaphore();
	sem_t   semaphore_scantype_inst;
	sem_t * semaphore_scantype = &semaphore_scantype_inst;
# endif /* HAVE_SEMAPHORE */
	pthread_t thread;
	nutscan_thread_t * thread_array = NULL;
	size_t thread_count = 0, i;
# if (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE)
	size_t  max_threads_scantype = max_threads_oldnut;
# endif

	pthread_mutex_init(&dev_mutex, NULL);

# ifdef HAVE_SEMAPHORE
	if (max_threads_scantype > 0) {
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
		/* Different platforms, different sizes, none fits all... */
		if (SIZE_MAX > UINT_MAX && max_threads_scantype > UINT_MAX) {
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif
			upsdebugx(1,
				"WARNING: %s: Limiting max_threads_scantype to range acceptable for sem_init()",
				__func__);
			max_threads_scantype = UINT_MAX - 1;
		}
		sem_init(semaphore_scantype, 0, (unsigned int)max_threads_scantype);
	}
# endif /* HAVE_SEMAPHORE */

#endif /* HAVE_PTHREAD */

	if (!nutscan_avail_nut) {
		return NULL;
	}

#ifndef WIN32
	/* Ignore SIGPIPE if the caller hasn't set a handler for it yet */
	if (sigaction(SIGPIPE, NULL, &oldact) == 0) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
		if (oldact.sa_handler == SIG_DFL) {
			change_action_handler = 1;
			signal(SIGPIPE, SIG_IGN);
		}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic pop
#endif
	}
#endif

	ip_str = nutscan_ip_iter_init(&ip, startIP, stopIP);

	while (ip_str != NULL) {
#ifdef HAVE_PTHREAD
		/* NOTE: With many enough targets to scan, this can crash
		 * by spawning too many children; add a limit and loop to
		 * "reap" some already done with their work. And probably
		 * account them in thread_array[] as something to not wait
		 * for below in pthread_join()...
		 */

# ifdef HAVE_SEMAPHORE
		/* Just wait for someone to free a semaphored slot,
		 * if none are available, and then/otherwise grab one
		 */
		if (thread_array == NULL) {
			/* Starting point, or after a wait to complete
			 * all earlier runners */
			if (max_threads_scantype > 0)
				sem_wait(semaphore_scantype);
			sem_wait(semaphore);
			pass = TRUE;
		} else {
			pass = ((max_threads_scantype == 0 || sem_trywait(semaphore_scantype) == 0) &&
			        sem_trywait(semaphore) == 0);
		}
# else
#  ifdef HAVE_PTHREAD_TRYJOIN
		/* A somewhat naive and brute-force solution for
		 * systems without a semaphore.h. This may suffer
		 * some off-by-one errors, using a few more threads
		 * than intended (if we race a bit at the wrong time,
		 * probably up to one per enabled scanner routine).
		 */

		/* TOTHINK: Should there be a threadcount_mutex when
		 * we just read the value in if() and while() below?
		 * At worst we would overflow the limit a bit due to
		 * other protocol scanners...
		 */
		if (curr_threads >= max_threads
		|| (curr_threads >= max_threads_scantype && max_threads_scantype > 0)
		) {
			upsdebugx(2, "%s: already running %" PRIuSIZE " scanning threads "
				"(launched overall: %" PRIuSIZE "), "
				"waiting until some would finish",
				__func__, curr_threads, thread_count);
			while (curr_threads >= max_threads
			   || (curr_threads >= max_threads_scantype && max_threads_scantype > 0)
			) {
				for (i = 0; i < thread_count ; i++) {
					int ret;

					if (!thread_array[i].active) continue;

					pthread_mutex_lock(&threadcount_mutex);
					upsdebugx(3, "%s: Trying to join thread #%i...", __func__, i);
					ret = pthread_tryjoin_np(thread_array[i].thread, NULL);
					switch (ret) {
						case ESRCH:     /* No thread with the ID thread could be found - already "joined"? */
							upsdebugx(5, "%s: Was thread #%" PRIuSIZE " joined earlier?", __func__, i);
							break;
						case 0:         /* thread exited */
							if (curr_threads > 0) {
								curr_threads --;
								upsdebugx(4, "%s: Joined a finished thread #%" PRIuSIZE, __func__, i);
							} else {
								/* threadcount_mutex fault? */
								upsdebugx(0, "WARNING: %s: Accounting of thread count "
									"says we are already at 0", __func__);
							}
							thread_array[i].active = FALSE;
							break;
						case EBUSY:     /* actively running */
							upsdebugx(6, "%s: thread #%" PRIuSIZE " still busy (%i)",
								__func__, i, ret);
							break;
						case EDEADLK:   /* Errors with thread interactions... bail out? */
						case EINVAL:    /* Errors with thread interactions... bail out? */
						default:        /* new pthreads abilities? */
							upsdebugx(5, "%s: thread #%" PRIuSIZE " reported code %i",
								__func__, i, ret);
							break;
					}
					pthread_mutex_unlock(&threadcount_mutex);
				}

				if (curr_threads >= max_threads
				|| (curr_threads >= max_threads_scantype && max_threads_scantype > 0)
				) {
					usleep (10000); /* microSec's, so 0.01s here */
				}
			}
			upsdebugx(2, "%s: proceeding with scan", __func__);
		}
		/* NOTE: No change to default "pass" in this ifdef:
		 * if we got to this line, we have a slot to use */
#  endif /* HAVE_PTHREAD_TRYJOIN */
# endif  /* HAVE_SEMAPHORE */
#endif   /* HAVE_PTHREAD */

		if (pass) {
			if (port) {
				if (ip.type == IPv4) {
					snprintf(buf, sizeof(buf), "%s:%s", ip_str, port);
				}
				else {
					snprintf(buf, sizeof(buf), "[%s]:%s", ip_str, port);
				}

				ip_dest = strdup(buf);
			}
			else {
				ip_dest = strdup(ip_str);
			}

			if ((nut_arg = malloc(sizeof(struct scan_nut_arg))) == NULL) {
				free(ip_dest);
				break;
			}

			nut_arg->timeout = usec_timeout;
			nut_arg->hostname = ip_dest;

#ifdef HAVE_PTHREAD
			if (pthread_create(&thread, NULL, list_nut_devices, (void*)nut_arg) == 0) {
				nutscan_thread_t	*new_thread_array;
# ifdef HAVE_PTHREAD_TRYJOIN
				pthread_mutex_lock(&threadcount_mutex);
				curr_threads++;
# endif /* HAVE_PTHREAD_TRYJOIN */

				thread_count++;
				new_thread_array = realloc(thread_array,
					thread_count * sizeof(nutscan_thread_t));
				if (new_thread_array == NULL) {
					upsdebugx(1, "%s: Failed to realloc thread array", __func__);
					break;
				}
				else {
					thread_array = new_thread_array;
				}
				thread_array[thread_count - 1].thread = thread;
				thread_array[thread_count - 1].active = TRUE;

# ifdef HAVE_PTHREAD_TRYJOIN
				pthread_mutex_unlock(&threadcount_mutex);
# endif /* HAVE_PTHREAD_TRYJOIN */
			}
#else  /* not HAVE_PTHREAD */
			list_nut_devices(nut_arg);
#endif /* if HAVE_PTHREAD */
			free(ip_str);
			ip_str = nutscan_ip_iter_inc(&ip);
		} else { /* if not pass -- all slots busy */
#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE
			/* Wait for all current scans to complete */
			if (thread_array != NULL) {
				upsdebugx (2, "%s: Running too many scanning threads, "
					"waiting until older ones would finish",
					__func__);
				for (i = 0; i < thread_count ; i++) {
					int ret;
					if (!thread_array[i].active) {
						/* Probably should not get here,
						 * but handle it just in case */
						upsdebugx(0, "WARNING: %s: Midway clean-up: did not expect thread %" PRIuSIZE " to be not active",
							__func__, i);
						sem_post(semaphore);
						if (max_threads_scantype > 0)
							sem_post(semaphore_scantype);
						continue;
					}
					thread_array[i].active = FALSE;
					ret = pthread_join(thread_array[i].thread, NULL);
					if (ret != 0) {
						upsdebugx(0, "WARNING: %s: Midway clean-up: pthread_join() returned code %i",
							__func__, ret);
					}
					sem_post(semaphore);
					if (max_threads_scantype > 0)
						sem_post(semaphore_scantype);
				}
				thread_count = 0;
				free(thread_array);
				thread_array = NULL;
			}
# else
#  ifdef HAVE_PTHREAD_TRYJOIN
		/* TODO: Move the wait-loop for TRYJOIN here? */
#  endif /* HAVE_PTHREAD_TRYJOIN */
# endif  /* HAVE_SEMAPHORE */
#endif   /* HAVE_PTHREAD */
		} /* if: could we "pass" or not? */
	} /* while */

#ifdef HAVE_PTHREAD
	if (thread_array != NULL) {
		upsdebugx(2, "%s: all planned scans launched, waiting for threads to complete", __func__);
		for (i = 0; i < thread_count; i++) {
			int ret;

			if (!thread_array[i].active) continue;

			ret = pthread_join(thread_array[i].thread, NULL);
			if (ret != 0) {
				upsdebugx(0, "WARNING: %s: Clean-up: pthread_join() returned code %i",
					__func__, ret);
			}
			thread_array[i].active = FALSE;
# ifdef HAVE_SEMAPHORE
			sem_post(semaphore);
			if (max_threads_scantype > 0)
				sem_post(semaphore_scantype);
# else
#  ifdef HAVE_PTHREAD_TRYJOIN
			pthread_mutex_lock(&threadcount_mutex);
			if (curr_threads > 0) {
				curr_threads --;
				upsdebugx(5, "%s: Clean-up: Joined a finished thread #%" PRIuSIZE,
					__func__, i);
			} else {
				upsdebugx(0, "WARNING: %s: Clean-up: Accounting of thread count "
					"says we are already at 0", __func__);
			}
			pthread_mutex_unlock(&threadcount_mutex);
#  endif /* HAVE_PTHREAD_TRYJOIN */
# endif /* HAVE_SEMAPHORE */
		}
		free(thread_array);
		upsdebugx(2, "%s: all threads freed", __func__);
	}
	pthread_mutex_destroy(&dev_mutex);

# ifdef HAVE_SEMAPHORE
	if (max_threads_scantype > 0)
		sem_destroy(semaphore_scantype);
# endif /* HAVE_SEMAPHORE */
#endif /* HAVE_PTHREAD */

#ifndef WIN32
	if (change_action_handler) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
		signal(SIGPIPE, SIG_DFL);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic pop
#endif
	}
#endif

	return nutscan_rewind_device(dev_ret);
}
