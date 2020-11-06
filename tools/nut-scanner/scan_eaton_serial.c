/*
 *  Copyright (C) 2012 - EATON
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
*/

#include "common.h"

/* Need this on AIX when using xlc to get alloca */
#ifdef _AIX
#pragma alloca
#endif /* _AIX */

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nut-scan.h"
#include "serial.h"
#include "bcmxcp_io.h"
#include "bcmxcp.h"
#include "nutscan-serial.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

/* SHUT header */
#define SHUT_SYNC 0x16
#define MAX_TRY   4

/* BCMXCP header */
extern unsigned char AUT[4];
extern struct pw_baud_rate {
        int rate;
        int name;
} pw_baud_rates[];

/* Local list of found devices */
static nutscan_device_t * dev_ret = NULL;

/* Remap some functions to avoid undesired behavior (drivers/main.c) */
char *getval(const char * NUT_UNUSED(var)) { return NULL; }

#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif

/* Drivers name */
#define SHUT_DRIVER_NAME  "mge-shut"
#define XCP_DRIVER_NAME   "bcmxcp"
#define Q1_DRIVER_NAME    "blazer_ser"

/* Fake driver main, for using serial functions, needed for bcmxcp_ser.c */
char  *device_path;
int   upsfd;
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
	for(i = 0; i < 2 + buf[1]; i++)
		c -= buf[i];

	return c;
}

/*******************************************************************************
 * SHUT functions (MGE legacy, but Eaton path forward)
 ******************************************************************************/

/* Light version of of drivers/libshut.c->shut_synchronise()
 * return 1 if OK, 0 otherwise */
int shut_synchronise(int upsfd)
{
	int try;
	u_char reply = '\0';

	/* Sync with the UPS according to notification */
	for (try = 0; try < MAX_TRY; try++) {
		if ((ser_send_char(upsfd, SHUT_SYNC)) == -1) {
			continue;
		}

		ser_get_char(upsfd, &reply, 1, 0);
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
nutscan_device_t * nutscan_scan_eaton_serial_shut(const char* port_name)
{
	nutscan_device_t * dev = NULL;
	int devfd = -1;

	if ( (devfd = ser_open_nf(port_name)) != -1 ) {
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
nutscan_device_t * nutscan_scan_eaton_serial_xcp(const char* port_name)
{
	nutscan_device_t * dev = NULL;
	int i, ret, devfd = -1;
	unsigned char	answer[256];
	unsigned char	sbuf[128];

	memset(sbuf, 0, 128);

	if ( (devfd = ser_open_nf(port_name)) != -1 ) {
#ifdef HAVE_PTHREAD
		pthread_mutex_lock(&dev_mutex);
#endif
		upsfd = devfd;
#ifdef HAVE_PTHREAD
		pthread_mutex_unlock(&dev_mutex);
#endif

		for (i=0; (pw_baud_rates[i].rate != 0) && (dev == NULL); i++)
		{
			memset(answer, 0, 256);

			if (ser_set_speed_nf(devfd, port_name, pw_baud_rates[i].rate) == -1)
				break;

			ret = ser_send_char(devfd, 0x1d);	/* send ESC to take it out of menu */
			if (ret <= 0)
				break;

			usleep(90000);
			send_write_command(AUT, 4);
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

			if ( (ret > 0) && (answer[0] == PW_COMMAND_START_BYTE) ) {
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
nutscan_device_t * nutscan_scan_eaton_serial_q1(const char* port_name)
{
	nutscan_device_t * dev = NULL;
	struct termios tio;
	int ret = 0, retry;
	int devfd = -1;
	char buf[128];

	if ( (devfd = ser_open_nf(port_name)) != -1 ) {
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
						if ( (ret = ser_send(devfd, "Q1\r")) > 0) {

							/* Get Q1 reply */
							if ( (ret = ser_get_buf(devfd, buf, sizeof(buf), SER_WAIT_SEC, 0)) > 0) {

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
	if ( (dev = nutscan_scan_eaton_serial_shut(port_name)) == NULL) {
		usleep(100000);
		/* Else, try XCP */
		if ( (dev = nutscan_scan_eaton_serial_xcp(port_name)) == NULL) {
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
	struct sigaction oldact;
	int change_action_handler = 0;
	char *current_port_name = NULL;
	char **serial_ports_list;
	int  current_port_nb;
	int i;
#ifdef HAVE_PTHREAD
	pthread_t thread;
	pthread_t * thread_array = NULL;
	int thread_count = 0;

	pthread_mutex_init(&dev_mutex,NULL);
#endif

	/* 1) Get ports_list */
	serial_ports_list = nutscan_get_serial_ports_list(ports_range);
	if( serial_ports_list == NULL ) {
		return NULL;
	}

	/* Ignore SIGPIPE if the caller hasn't set a handler for it yet */
	if( sigaction(SIGPIPE, NULL, &oldact) == 0 ) {
		if( oldact.sa_handler == SIG_DFL ) {
			change_action_handler = 1;
			signal(SIGPIPE,SIG_IGN);
		}
	}

	/* port(s) iterator */
	current_port_nb = 0;
	while(serial_ports_list[current_port_nb] != NULL) {
		current_port_name = serial_ports_list[current_port_nb];
#ifdef HAVE_PTHREAD
		if (pthread_create(&thread, NULL, nutscan_scan_eaton_serial_device, (void*)current_port_name) == 0){
			thread_count++;
			pthread_t *new_thread_array = realloc(thread_array,
						thread_count*sizeof(pthread_t));
			if (new_thread_array == NULL) {
				upsdebugx(1, "%s: Failed to realloc thread", __func__);
				break;
			}
			else {
				thread_array = new_thread_array;
			}
			thread_array[thread_count-1] = thread;
		}
#else
		nutscan_scan_eaton_serial_device(current_port_name);
#endif
		current_port_nb++;
	}

#ifdef HAVE_PTHREAD
	for ( i = 0; i < thread_count ; i++) {
		pthread_join(thread_array[i],NULL);
	}
	pthread_mutex_destroy(&dev_mutex);
	free(thread_array);
#endif

	if(change_action_handler) {
		signal(SIGPIPE,SIG_DFL);
	}

	/* free everything... */
	i=0;
	while(serial_ports_list[i] != NULL) {
	 	free(serial_ports_list[i]);
		i++;
	}
	free( serial_ports_list);
	return nutscan_rewind_device(dev_ret);
}
