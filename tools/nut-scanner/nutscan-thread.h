/* Internal ownership helpers for parallel scanner workers.
 * Copyright (C)
 *    2016 - 2021  EATON - Various threads-related improvements
 *    2020 - 2026  Jim Klimov <jimklimov+nut@gmail.com>
 *    2026         NUT contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NUTSCAN_THREAD_H
#define NUTSCAN_THREAD_H

#include "nut-scan.h"

/* Keep these internal helpers outside the exported nutscan_ namespace. */
#ifdef HAVE_PTHREAD
int nut_scanner_thread_create(nutscan_thread_t **array, size_t *count,
	void *(*worker)(void *), void *arg);

# if defined HAVE_SEMAPHORE_UNNAMED || defined HAVE_SEMAPHORE_NAMED
void nut_scanner_thread_mutex_init(void);
void nut_scanner_thread_mutex_free(void);
void nut_scanner_semaphore_release(sem_t *global, sem_t *protocol, size_t limit);
int nut_scanner_semaphore_acquire(sem_t *global, sem_t *protocol,
	size_t limit, int wait);
# endif /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
#endif /* HAVE_PTHREAD */
#endif /* NUTSCAN_THREAD_H */
