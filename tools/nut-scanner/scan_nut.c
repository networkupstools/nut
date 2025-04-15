/*
 *  Copyright (C) 2011 - 2023 Arnaud Quette (Design and part of implementation)
 *  Copyright (C) 2011 - EATON
 *  Copyright (C) 2016-2021 - EATON - Various threads-related improvements
 *  Copyright (C) 2020-2024 - Jim Klimov <jimklimov+nut@gmail.com> - support and modernization of codebase
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

/* externally visible to nutscan-init */
int nutscan_unload_upsclient_library(void);

#include <ltdl.h>

#define SCAN_NUT_DRIVERNAME "dummy-ups"

/* dynamic link library stuff */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;
static char *dl_saved_libname = NULL;

static int (*nut_upscli_splitaddr)(const char *buf, char **hostname, uint16_t *port);
static int (*nut_upscli_tryconnect)(UPSCONN_t *ups, const char *host, uint16_t port,
					int flags, struct timeval * timeout);
static int (*nut_upscli_list_start)(UPSCONN_t *ups, size_t numq,
					const char **query);
static int (*nut_upscli_list_next)(UPSCONN_t *ups, size_t numq,
			const char **query, size_t *numa, char ***answer);
static int (*nut_upscli_disconnect)(UPSCONN_t *ups);

/* This variable collects device(s) from a sequential or parallel scan,
 * is returned to caller, and cleared to allow subsequent independent scans */
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
	/* String includes square brackets around host/IP
	 * address, and/or :port suffix (if customized so): */
	char * hostname;
	useconds_t timeout;
};

/* Return 0 on success, -1 on error e.g. "was not loaded";
 * other values may be possible if lt_dlclose() errors set them;
 * visible externally */
int nutscan_unload_library(int *avail, lt_dlhandle *pdl_handle, char **libpath);
int nutscan_unload_upsclient_library(void)
{
	return nutscan_unload_library(&nutscan_avail_nut, &dl_handle, &dl_saved_libname);
}

/* Return 0 on error; visible externally */
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
		upsdebugx(0, "NUT client library not found. NUT search disabled.");
		return 0;
	}

	if (lt_dlinit() != 0) {
		upsdebugx(0, "%s: Error initializing lt_dlinit", __func__);
		return 0;
	}

	dl_handle = lt_dlopen(libname_path);
	if (!dl_handle) {
		dl_error = lt_dlerror();
		goto err;
	}

	/* Clear any existing error */
	lt_dlerror();

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

	if (dl_saved_libname)
		free(dl_saved_libname);
	dl_saved_libname = xstrdup(libname_path);

	return 1;

err:
	upsdebugx(0,
		"Cannot load NUT library (%s) : %s. NUT search disabled.",
		libname_path, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	if (dl_saved_libname) {
		free(dl_saved_libname);
		dl_saved_libname = NULL;
	}
	return 0;
}
/* end of dynamic link library stuff */

/* FIXME: SSL support */
/* Performs a (parallel-able) NUT protocol scan of one remote host:port.
 * Returns NULL, updates global dev_ret when a scan is successful.
 * FREES the caller's copy of "nut_arg" and "hostname" in it, if applicable.
 */
static void * list_nut_devices_thready(void * arg)
{
	struct scan_nut_arg * nut_arg = (struct scan_nut_arg*)arg;
	char *target_hostname = nut_arg->hostname;
	struct timeval tv;
	uint16_t port;
	size_t numq, numa;
	const char *query[4];
	char **answer = NULL;
	char *hostname = NULL;
	UPSCONN_t *ups = xcalloc(1, sizeof(*ups));
	nutscan_device_t * dev = NULL;
	size_t buf_size;

	tv.tv_sec = nut_arg->timeout / (1000*1000);
	tv.tv_usec = nut_arg->timeout % (1000*1000);

	query[0] = "UPS";
	numq = 1;

	upsdebugx(2, "Entering %s for %s", __func__, target_hostname);

	if ((*nut_upscli_splitaddr)(target_hostname, &hostname, &port) != 0) {
		/* Avoid disconnect from not connected ups */
		if (ups) {
			if (ups->host)
				free(ups->host);
			free(ups);
		}
		ups = NULL;
		goto end;
	}

	if ((*nut_upscli_tryconnect)(ups, hostname, port, UPSCLI_CONN_TRYSSL, &tv) < 0) {
		/* Avoid disconnect from not connected ups */
		if (ups) {
			if (ups->host)
				free(ups->host);
			free(ups);
		}
		ups = NULL;
		goto end;
	}

	if ((*nut_upscli_list_start)(ups, numq, query) < 0) {
		goto end;
	}

	while ((*nut_upscli_list_next)(ups, numq, query, &numa, &answer) == 1) {
		/* UPS <upsname> <description> */
		if (numa < 3) {
			goto end;
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
		/* +1+1 is for '@' character and terminating 0,
		 * and the other +1+1 is for possible '[' and ']'
		 * around the host name:
		 */
		buf_size = strlen(answer[1]) + strlen(hostname) + 1 + 1 + 1 + 1;
		if (port != PORT) {
			/* colon and up to 5 digits */
			buf_size += 6;
		}

		dev->port = malloc(buf_size);

		if (dev->port) {
			/* Check if IPv6 and needs brackets */
			char	*hostname_colon = strchr(hostname, ':');

			if (hostname_colon && *hostname_colon == '\0')
				hostname_colon = NULL;
			if (*hostname == '[')
				hostname_colon = NULL;

			if (port != PORT) {
				if (hostname_colon) {
					snprintf(dev->port, buf_size, "%s@[%s]:%" PRIu16,
						answer[1], hostname, port);
				} else {
					snprintf(dev->port, buf_size, "%s@%s:%" PRIu16,
						answer[1], hostname, port);
				}
			} else {
				/* Standard port, not suffixed */
				if (hostname_colon) {
					snprintf(dev->port, buf_size, "%s@[%s]",
						answer[1], hostname);
				} else {
					snprintf(dev->port, buf_size, "%s@%s",
						answer[1], hostname);
				}
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

end:
	if (ups) {
		(*nut_upscli_disconnect)(ups);
		if (ups->host)
			free(ups->host);
		free(ups);
	}

	if (target_hostname)
		free(target_hostname);
	if (hostname)
		free(hostname);
	if (nut_arg)
		free(nut_arg);

	return NULL;
}

nutscan_device_t * nutscan_scan_nut(const char* start_ip, const char* stop_ip, const char* port, useconds_t usec_timeout)
{
	nutscan_device_t	*ndret;
	nutscan_ip_range_list_t irl;

	nutscan_init_ip_ranges(&irl);
	nutscan_add_ip_range(&irl, (char *)start_ip, (char *)stop_ip);

	ndret = nutscan_scan_ip_range_nut(&irl, port, usec_timeout);

	/* Avoid nuking caller's strings here */
	irl.ip_ranges->start_ip = NULL;
	irl.ip_ranges->end_ip = NULL;
	nutscan_free_ip_ranges(&irl);

	return ndret;
}

nutscan_device_t * nutscan_scan_ip_range_nut(nutscan_ip_range_list_t * irl, const char* port, useconds_t usec_timeout)
{
	bool_t pass = TRUE; /* Track that we may spawn a scanning thread */
	nutscan_ip_range_list_iter_t ip;
	char * ip_str = NULL;
	char * ip_dest = NULL;
	char buf[SMALLBUF];
#ifndef WIN32
	struct sigaction oldact;
	int change_action_handler = 0;
#endif	/* !WIN32 */
	struct scan_nut_arg *nut_arg;

#ifdef HAVE_PTHREAD
# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
	sem_t * semaphore = nutscan_semaphore();
#  if (defined HAVE_SEMAPHORE_UNNAMED)
	sem_t   semaphore_scantype_inst;
	sem_t * semaphore_scantype = &semaphore_scantype_inst;
#  elif (defined HAVE_SEMAPHORE_NAMED)
	sem_t * semaphore_scantype = NULL;
#  endif
# endif /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
	pthread_t thread;
	nutscan_thread_t * thread_array = NULL;
	size_t thread_count = 0, i;
# if (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
	size_t  max_threads_scantype = max_threads_oldnut;
# endif

	pthread_mutex_init(&dev_mutex, NULL);

# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
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
				"WARNING: %s: Limiting max_threads_scantype to range acceptable for " REPORT_SEM_INIT_METHOD "()",
				__func__);
			max_threads_scantype = UINT_MAX - 1;
		}

		upsdebugx(4, "%s: " REPORT_SEM_INIT_METHOD "() for %" PRIuSIZE " threads", __func__, max_threads_scantype);
#  if (defined HAVE_SEMAPHORE_UNNAMED)
		if (sem_init(semaphore_scantype, 0, (unsigned int)max_threads_scantype)) {
			upsdebug_with_errno(4, "%s: " REPORT_SEM_INIT_METHOD "() failed", __func__);
			max_threads_scantype = 0;
		}
#  elif (defined HAVE_SEMAPHORE_NAMED)
		if (SEM_FAILED == (semaphore_scantype = sem_open(SEMNAME_UPSCLIENT, O_CREAT, 0644, (unsigned int)max_threads_scantype))) {
			upsdebug_with_errno(4, "%s: " REPORT_SEM_INIT_METHOD "() failed", __func__);
			semaphore_scantype = NULL;
			max_threads_scantype = 0;
		}
#  endif
	}
# endif /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */

#endif /* HAVE_PTHREAD */

	if (!nutscan_avail_nut) {
		return NULL;
	}

	if (irl == NULL || irl->ip_ranges == NULL) {
		return NULL;
	}

	if (!irl->ip_ranges->start_ip) {
		upsdebugx(1, "%s: no starting IP address specified", __func__);
	} else if (irl->ip_ranges_count == 1
		&& (irl->ip_ranges->start_ip == irl->ip_ranges->end_ip
		    || !strcmp(irl->ip_ranges->start_ip, irl->ip_ranges->end_ip)
	)) {
		upsdebugx(1, "%s: Scanning \"Old NUT\" bus for single IP address: %s",
			__func__, irl->ip_ranges->start_ip);
	} else {
		upsdebugx(1, "%s: Scanning \"Old NUT\" bus for IP address range(s): %s",
			__func__, nutscan_stringify_ip_ranges(irl));
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
#endif	/* !WIN32 */

	ip_str = nutscan_ip_ranges_iter_init(&ip, irl);

	while (ip_str != NULL) {
#ifdef HAVE_PTHREAD
		/* NOTE: With many enough targets to scan, this can crash
		 * by spawning too many children; add a limit and loop to
		 * "reap" some already done with their work. And probably
		 * account them in thread_array[] as something to not wait
		 * for below in pthread_join()...
		 */

# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
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
			/* If successful (the lock was acquired),
			 * sem_wait() and sem_trywait() will return 0.
			 * Otherwise, -1 is returned and errno is set,
			 * and the state of the semaphore is unchanged.
			 */
			int	stwST = sem_trywait(semaphore_scantype);
			int	stwS  = sem_trywait(semaphore);
			pass = ((max_threads_scantype == 0 || stwST == 0) && stwS == 0);
			upsdebugx(4, "%s: max_threads_scantype=%" PRIuSIZE
				" curr_threads=%" PRIuSIZE
				" thread_count=%" PRIuSIZE
				" stwST=%d stwS=%d pass=%u",
				__func__, max_threads_scantype,
				curr_threads, thread_count,
				stwST, stwS, pass
			);
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
					upsdebugx(3, "%s: Trying to join thread #%" PRIuSIZE "...", __func__, i);
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
# endif  /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
#endif   /* HAVE_PTHREAD */

		if (pass) {
			if (port) {
				if (ip.curr_ip_iter.type == IPv4) {
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
				upsdebugx(0, "%s: Memory allocation error", __func__);
				free(ip_dest);
				break;
			}

			nut_arg->timeout = usec_timeout;
			nut_arg->hostname = ip_dest;

#ifdef HAVE_PTHREAD
			if (pthread_create(&thread, NULL, list_nut_devices_thready, (void*)nut_arg) == 0) {
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
#else  /* if not HAVE_PTHREAD */
			list_nut_devices_thready(nut_arg);
#endif /* if HAVE_PTHREAD */

			/* Prepare the next iteration; note that
			 * nutscan_scan_ipmi_device_thready()
			 * takes care of freeing "tmp_sec" and its
			 * copy (note strdup!) of "ip_str" as
			 * hostname, possibly suffixed with a port.
			 */
			free(ip_str);
			ip_str = nutscan_ip_ranges_iter_inc(&ip);
		} else { /* if not pass -- all slots busy */
#ifdef HAVE_PTHREAD
# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
			/* Wait for all current scans to complete */
			if (thread_array != NULL) {
				upsdebugx (2, "%s: Running too many scanning threads (%"
					PRIuSIZE "), "
					"waiting until older ones would finish",
					__func__, thread_count);
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
# endif  /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
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
# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
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
# endif /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
		}
		free(thread_array);
		upsdebugx(2, "%s: all threads freed", __func__);
	}
	pthread_mutex_destroy(&dev_mutex);

# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
	if (max_threads_scantype > 0) {
#  if (defined HAVE_SEMAPHORE_UNNAMED)
		sem_destroy(semaphore_scantype);
#  elif (defined HAVE_SEMAPHORE_NAMED)
		if (semaphore_scantype) {
			sem_unlink(SEMNAME_UPSCLIENT);
			sem_close(semaphore_scantype);
			semaphore_scantype = NULL;
		}
#  endif
	}
# endif /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
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
#endif	/* !WIN32 */

	return nutscan_rewind_device(dev_ret);
}
