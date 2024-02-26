/*
 *  Copyright (C) 2012 - EATON
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

/*! \file scan_eaton_serial.c
    \brief detect Eaton serial XCP, SHUT and Q1 devices
    \author Arnaud Quette <ArnaudQuette@eaton.com>
    \author Jim Klimov <EvgenyKlimov@eaton.com>
*/

#include "common.h"
#include "nut-scan.h"
#include "nut_stdint.h"

/* Need this on AIX when using xlc to get alloca */
#ifdef _AIX
#pragma alloca
#endif /* _AIX */

#include <fcntl.h>
#include <stdio.h>
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic pop
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "serial.h"
#include "bcmxcp_io.h"
#include "bcmxcp_ser.h"
#include "bcmxcp.h"
#include "nutscan-serial.h"

/* SHUT header */
#define SHUT_SYNC 0x16
#define MAX_TRY   4

/* BCMXCP header defines these externs now: */
/*
extern unsigned char BCMXCP_AUTHCMD[4];
extern struct pw_baud_rate {
	int rate;
	int name;
} pw_baud_rates[];
*/

/* Local list of found devices */
static nutscan_device_t * dev_ret = NULL;

/* Remap some functions to avoid undesired behavior (drivers/main.c) */
char *getval(const char *var)
{
	NUT_UNUSED_VARIABLE(var);
	return NULL;
}

#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif

/* Drivers name */
#define SHUT_DRIVER_NAME  "mge-shut"
#define XCP_DRIVER_NAME   "bcmxcp"
#define Q1_DRIVER_NAME    "blazer_ser"

/* Fake driver main, for using serial functions, needed for bcmxcp_ser.c */
char  *device_path;
TYPE_FD   upsfd;
int   exit_flag = 0;
int   do_lock_port;

/* Functions extracted from drivers/bcmxcp.c, to avoid pulling too many things
 * lightweight function to calculate the 8-bit
 * two's complement checksum of buf, using XCP data length (including header)
 * the result must be 0 for the sequence data to be valid */
int checksum_test(const unsigned char *buf)
{
	unsigned char checksum = 0;
	int i, length;
	/* buf[2] is the length of the XCP frame ; add 5 for the header */
	length = (int)(buf[2]) + 5;

	for (i = 0; i < length; i++) {
		checksum += buf[i];
	}
	/* Compute the 8-bit, Two's Complement checksum now and return it */
	checksum = ((0x100 - checksum) & 0xFF);
	return (checksum == 0);
}


unsigned char calc_checksum(const unsigned char *buf)
{
	unsigned char c;
	int i;

	c = 0;
	for (i = 0; i < 2 + buf[1]; i++)
		c -= buf[i];

	return c;
}

/*******************************************************************************
 * SHUT functions (MGE legacy, but Eaton path forward)
 ******************************************************************************/

/* Light version of of drivers/libshut.c->shut_synchronise()
 * return 1 if OK, 0 otherwise */
static int shut_synchronise(TYPE_FD_SER arg_upsfd)
{
	int try;
	unsigned char reply = '\0';
	/* FIXME? Should we save "arg_upsfd" into global "upsfd" variable?
	 * This was previously shadowed by function argument named "upsfd"...
	 */
	/* upsfd = arg_upsfd; */

	/* Sync with the UPS according to notification */
	for (try = 0; try < MAX_TRY; try++) {
		if ((ser_send_char(arg_upsfd, SHUT_SYNC)) == -1) {
			continue;
		}

		ser_get_char(arg_upsfd, &reply, 1, 0);
		if (reply == SHUT_SYNC) {
			return 1;
		}
	}
	return 0;
}

/* SHUT scan:
 *   send SYNC token (0x16) and receive the SYNC token back
 *   FIXME: maybe try to get device descriptor?!
 */
static nutscan_device_t * nutscan_scan_eaton_serial_shut(const char* port_name)
{
	nutscan_device_t * dev = NULL;
	TYPE_FD_SER devfd = ser_open_nf(port_name);

	if (VALID_FD_SER(devfd)) {
		/* set RTS to off and DTR to on to allow correct behavior
		 * with UPS using PnP feature */
		if (ser_set_dtr(devfd, 1) != -1) {

			ser_set_rts(devfd, 0);
			ser_set_speed_nf(devfd, port_name, B2400);

			if (shut_synchronise(devfd)) {

				/* Communication established successfully! */
				dev = nutscan_new_device();
				dev->type = TYPE_EATON_SERIAL;
				dev->driver = strdup(SHUT_DRIVER_NAME);
				dev->port = strdup(port_name);
#ifdef HAVE_PTHREAD
				pthread_mutex_lock(&dev_mutex);
#endif
				dev_ret = nutscan_add_device_to_device(dev_ret, dev);
#ifdef HAVE_PTHREAD
				pthread_mutex_unlock(&dev_mutex);
#endif
			}
		}
		/* Close the device */
		ser_close(devfd, NULL);
	}

	return dev;
}

/*******************************************************************************
 * XCP functions (Eaton Powerware legacy)
 ******************************************************************************/

/* XCP scan:
 *   baudrate nego (...)
 *   Send ESC to take it out of menu
 *   Wait 90ms
 *   Send auth command (AUTHOR[4] = {0xCF, 0x69, 0xE8, 0xD5};)
 *   Wait 500ms (or less?)
 *   Send PW_SET_REQ_ONLY_MODE command (0xA0) and wait for response
 *   [Get ID Block (PW_ID_BLOCK_REQ) (0x31)]
 */
static nutscan_device_t * nutscan_scan_eaton_serial_xcp(const char* port_name)
{
	nutscan_device_t * dev = NULL;
	int i;
	ssize_t ret;
	unsigned char	answer[256];
	unsigned char	sbuf[128];
	TYPE_FD_SER devfd = ser_open_nf(port_name);

	memset(sbuf, 0, 128);

	if (VALID_FD_SER(devfd)) {
#ifdef HAVE_PTHREAD
		pthread_mutex_lock(&dev_mutex);
#endif
		upsfd = devfd;
#ifdef HAVE_PTHREAD
		pthread_mutex_unlock(&dev_mutex);
#endif

		for (i = 0; (pw_baud_rates[i].rate != 0) && (dev == NULL); i++)
		{
			memset(answer, 0, 256);

			if (ser_set_speed_nf(devfd, port_name, pw_baud_rates[i].rate) == -1)
				break;

			ret = ser_send_char(devfd, 0x1d);	/* send ESC to take it out of menu */
			if (ret <= 0)
				break;

			usleep(90000);
			send_write_command(BCMXCP_AUTHCMD, 4);
			usleep(500000);

			/* Discovery with Baud Hunting (XCP protocol spec. §4.1.2)
			 * sending PW_SET_REQ_ONLY_MODE should be enough, since
			 * the unit should send back Identification block */
			sbuf[0] = PW_COMMAND_START_BYTE;
			sbuf[1] = (unsigned char)1;
			sbuf[2] = PW_SET_REQ_ONLY_MODE;
			sbuf[3] = calc_checksum(sbuf);
			ret = ser_send_buf_pace(devfd, 1000, sbuf, 4);

			/* Read PW_COMMAND_START_BYTE byte */
			ret = ser_get_char(devfd, answer, 1, 0);

#if 0
			/* FIXME: seems not needed, but requires testing with more devices! */
			if (ret <= 0) {
				usleep(250000); /* 500000? */
				memset(answer, 0, 256);
				ret = command_sequence(&id_command, 1, answer);
			}
#endif

			if ((ret > 0) && (answer[0] == PW_COMMAND_START_BYTE)) {
				dev = nutscan_new_device();
				dev->type = TYPE_EATON_SERIAL;
				dev->driver = strdup(XCP_DRIVER_NAME);
				dev->port = strdup(port_name);
#ifdef HAVE_PTHREAD
				pthread_mutex_lock(&dev_mutex);
#endif
				dev_ret = nutscan_add_device_to_device(dev_ret, dev);
#ifdef HAVE_PTHREAD
				pthread_mutex_unlock(&dev_mutex);
#endif
				break;
			}
			usleep(100000);
		}
		/* Close the device */
		ser_close(devfd, NULL);
	}

	return dev;
}

/*******************************************************************************
 * Q1 functions (Phoenixtec/Centralion/Santak, still Eaton path forward)
 ******************************************************************************/

#define SER_WAIT_SEC  1  /* 3 seconds for Best UPS */
#define MAXTRIES      3

/* Q1 scan:
 *   - open the serial port and set the speed to 2400 baud
 *   - simply try to get Q1 (status) string
 *   - check its size and first char. which should be '('
 */
static nutscan_device_t * nutscan_scan_eaton_serial_q1(const char* port_name)
{
	nutscan_device_t * dev = NULL;
	struct termios tio;
	ssize_t ret = 0;
	int retry;
	char buf[128];
	TYPE_FD_SER devfd = ser_open_nf(port_name);

	if (VALID_FD_SER(devfd)) {
		if (ser_set_speed_nf(devfd, port_name, B2400) != -1) {

			if (!tcgetattr(devfd, &tio)) {

				/* Use canonical mode input processing (to read reply line) */
				tio.c_lflag |= ICANON;	/* Canonical input (erase and kill processing) */

				tio.c_cc[VEOF]   = _POSIX_VDISABLE;
				tio.c_cc[VEOL]   = '\r';
				tio.c_cc[VERASE] = _POSIX_VDISABLE;
				tio.c_cc[VINTR]  = _POSIX_VDISABLE;
				tio.c_cc[VKILL]  = _POSIX_VDISABLE;
				tio.c_cc[VQUIT]  = _POSIX_VDISABLE;
				tio.c_cc[VSUSP]  = _POSIX_VDISABLE;
				tio.c_cc[VSTART] = _POSIX_VDISABLE;
				tio.c_cc[VSTOP]  = _POSIX_VDISABLE;

				if (!tcsetattr(devfd, TCSANOW, &tio)) {

					/* Set the default (normal) cablepower */
					ser_set_dtr(devfd, 1);
					ser_set_rts(devfd, 0);

					/* Allow some time to settle for the cablepower */
					usleep(100000);

					/* Only try pure 'Q1', not older ones like 'D' or 'QS'
					 * > [Q1\r]
					 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
					 */
					for (retry = 1; retry <= MAXTRIES; retry++) {

						/* simplified code */
						ser_flush_io(devfd);
						if ((ret = ser_send(devfd, "Q1\r")) > 0) {

							/* Get Q1 reply */
							if ((ret = ser_get_buf(devfd, buf, sizeof(buf), SER_WAIT_SEC, 0)) > 0) {

								/* Check answer */
								/* should at least (and most) be 46 chars */
								if (ret >= 46) {
									if (buf[0] == '(') {

										dev = nutscan_new_device();
										dev->type = TYPE_EATON_SERIAL;
										dev->driver = strdup(Q1_DRIVER_NAME);
										dev->port = strdup(port_name);
#ifdef HAVE_PTHREAD
										pthread_mutex_lock(&dev_mutex);
#endif
										dev_ret = nutscan_add_device_to_device(dev_ret, dev);
#ifdef HAVE_PTHREAD
										pthread_mutex_unlock(&dev_mutex);
#endif
										break;
									}
								}
							}
						}
					}
				}
			}
		}
		/* Close the device */
		ser_close(devfd, NULL);
	}
	return dev;
}

static void * nutscan_scan_eaton_serial_device(void * port_arg)
{
	nutscan_device_t * dev = NULL;
	char* port_name = (char*) port_arg;

	/* Try SHUT first */
	if ((dev = nutscan_scan_eaton_serial_shut(port_name)) == NULL) {
		usleep(100000);
		/* Else, try XCP */
		if ((dev = nutscan_scan_eaton_serial_xcp(port_name)) == NULL) {
			/* Else, try Q1 */
			usleep(100000);
			dev = nutscan_scan_eaton_serial_q1(port_name);
		}
		/* Else try UTalk? */
	}
	return dev;
}

nutscan_device_t * nutscan_scan_eaton_serial(const char* ports_range)
{
	bool_t pass = TRUE; /* Track that we may spawn a scanning thread */
#ifndef WIN32
	struct sigaction oldact;
	int change_action_handler = 0;
#endif
	char *current_port_name = NULL;
	char **serial_ports_list;
	int  current_port_nb;
#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE
	sem_t * semaphore = nutscan_semaphore();
# endif
	pthread_t thread;
	nutscan_thread_t * thread_array = NULL;
	size_t thread_count = 0, i;

	pthread_mutex_init(&dev_mutex, NULL);
#endif /* HAVE_PTHREAD */

	/* 1) Get ports_list */
	serial_ports_list = nutscan_get_serial_ports_list(ports_range);
	if (serial_ports_list == NULL) {
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

	/* port(s) iterator */
	current_port_nb = 0;
	while (serial_ports_list[current_port_nb] != NULL) {
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
			sem_wait(semaphore);
			pass = TRUE;
		} else {
			pass = (sem_trywait(semaphore) == 0);
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
		if (curr_threads >= max_threads) {
			upsdebugx(2, "%s: already running %" PRIuSIZE " scanning threads "
				"(launched overall: %" PRIuSIZE "), "
				"waiting until some would finish",
				__func__, curr_threads, thread_count);
			while (curr_threads >= max_threads) {
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

				if (curr_threads >= max_threads) {
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
			current_port_name = serial_ports_list[current_port_nb];

#ifdef HAVE_PTHREAD
			if (pthread_create(&thread, NULL, nutscan_scan_eaton_serial_device, (void*)current_port_name) == 0) {
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
#else   /* if not HAVE_PTHREAD */
			nutscan_scan_eaton_serial_device(current_port_name);
#endif  /* if HAVE_PTHREAD */
			current_port_nb++;
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
						continue;
					}
					thread_array[i].active = FALSE;
					ret = pthread_join(thread_array[i].thread, NULL);
					if (ret != 0) {
						upsdebugx(0, "WARNING: %s: Midway clean-up: pthread_join() returned code %i",
							__func__, ret);
					}
					sem_post(semaphore);
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
#endif /* WIN32 */

	/* free everything... */
	i = 0;
	while (serial_ports_list[i] != NULL) {
		free(serial_ports_list[i]);
		i++;
	}
	free( serial_ports_list);
	return nutscan_rewind_device(dev_ret);
}
