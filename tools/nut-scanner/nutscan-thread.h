/* Internal ownership helpers for parallel scanner workers.
 * Copyright (C) 2026 NUT contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NUTSCAN_THREAD_H
#define NUTSCAN_THREAD_H

#include "nut-scan.h"

#ifdef HAVE_PTHREAD

/* Allocate the tracking entry before transferring ownership of arg.
 * Return -1 for allocation failure, a pthread error, or 0 on success.
 * On any failure the caller still owns arg and any admission permits.
 */
static int nutscan_thread_create(nutscan_thread_t **array, size_t *count,
	void *(*worker)(void *), void *arg)
{
	nutscan_thread_t *grown;
	int ret;

	if (*count >= SIZE_MAX / sizeof(**array)) {
		upsdebugx(1, "%s: Thread array is too large", __func__);
		return -1;
	}

	grown = (nutscan_thread_t *)realloc(*array, (*count + 1) * sizeof(**array));
	if (grown == NULL) {
		upsdebugx(1, "%s: Failed to realloc thread array", __func__);
		return -1;
	}
	*array = grown;
	ret = pthread_create(&grown[*count].thread, NULL, worker, arg);
	if (ret != 0) {
		upsdebugx(1, "%s: pthread_create() returned code %i", __func__, ret);
		return ret;
	}
	grown[*count].active = 1;
	(*count)++;

# if defined HAVE_PTHREAD_TRYJOIN && !defined HAVE_SEMAPHORE_UNNAMED && !defined HAVE_SEMAPHORE_NAMED
	pthread_mutex_lock(&threadcount_mutex);
	curr_threads++;
	pthread_mutex_unlock(&threadcount_mutex);
# endif
	return 0;
}

# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
static void nutscan_semaphore_release(sem_t *global, sem_t *protocol, size_t limit)
{
	sem_post(global);
	if (limit > 0) {
		sem_post(protocol);
	}
}

/* Preserve blocking admission for an empty batch and try/reap for a
 * non-empty batch. A disabled protocol limit never accesses protocol.
 * Return 1 with both permits owned, 0 if busy, or -1 for other errors.
 */
static int nutscan_semaphore_acquire(sem_t *global, sem_t *protocol,
	size_t limit, int wait)
{
	int ret, saved_errno;

	if (limit > 0) {
		do {
			ret = wait ? sem_wait(protocol) : sem_trywait(protocol);
		} while (ret != 0 && errno == EINTR);
		if (ret != 0) {
			return errno == EAGAIN ? 0 : -1;
		}
	}

	do {
		ret = wait ? sem_wait(global) : sem_trywait(global);
	} while (ret != 0 && errno == EINTR);
	if (ret != 0) {
		saved_errno = errno;
		if (limit > 0) {
			sem_post(protocol);
		}
		errno = saved_errno;
		return errno == EAGAIN ? 0 : -1;
	}
	return 1;
}
# endif /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
#endif /* HAVE_PTHREAD */
#endif /* NUTSCAN_THREAD_H */
