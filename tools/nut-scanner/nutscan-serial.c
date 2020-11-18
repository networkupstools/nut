/*
 *  Copyright (C) 2011 - EATON
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

/*! \file nutscan-serial.c
    \brief helper functions to get serial devices name
    \author Frederic Bohe <fredericbohe@eaton.com>
    \author Arnaud Quette <arnaud.quette@free.fr>
*/

#include "nutscan-serial.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nut_platform.h"
#include "common.h"

#ifdef WIN32
/* Windows: all serial port names start with "COM" */
#define SERIAL_PORT_PREFIX "COM"
#else
/* Unix: all serial port names start with "/dev/tty" */
#define SERIAL_PORT_PREFIX "/dev/tty"
#endif

#define ERR_OUT_OF_BOUND "Serial port range out of bound (must be 0 to 9 or a to z depending on your system)\n"

typedef struct {
	char * name;
	char auto_start_port;
	char auto_stop_port;
} device_portname_t;

device_portname_t device_portname[] = {
#ifdef NUT_PLATFORM_HPUX
	/* the first number seems to be a card instance, the second number seems
	to be a port number */
	{ "/dev/tty0p%c", '0', '9' },
	{ "/dev/tty1p%c", '0', '9' },
	/* osf/1 and Digital UNIX style */
	{ "/dev/tty0%c", '0', '9' },
#endif
#ifdef NUT_PLATFORM_SOLARIS
	{ "/dev/tty%c", 'a', 'z' },
#endif
#ifdef NUT_PLATFORM_AIX
	{ "/dev/tty%c", '0', '9' },
#endif
#ifdef NUT_PLATFORM_LINUX
	{ "/dev/ttyS%c", '0', '9' },
	{ "/dev/ttyUSB%c", '0', '9' },
#endif
#ifdef NUT_PLATFORM_MS_WINDOWS
	{ "COM%c",  '1', '9'},
#endif
	/* SGI IRIX */
	/*      { "/dev/ttyd%i", "=" }, */
	/*      { "/dev/ttyf%i", "=" }, */
	/* FIXME: Mac OS X has no serial port, but maybe ttyUSB? */
	{ NULL, 0, 0 }
};

/* Return 1 if port_name is a full path name to a serial port,
 * as per SERIAL_PORT_PREFIX */
static int is_serial_port_path(const char * port_name)
{
	if (!strncmp(port_name, SERIAL_PORT_PREFIX, strlen(SERIAL_PORT_PREFIX))) {
		return 1;
	}
	return 0;
}

/* Add "port" to "list" */
static char ** add_port(char ** list, char * port)
{
	char ** res;
	int count = 0;

	if(list == NULL) {
		count = 0;
	}
	else {
		while(list[count] != NULL) {
			count++;
		}
	}

	/*+1 to get the number of port from the index nb_ports*/
	/*+1 for the terminal NULL */
	res = realloc(list,(count+1+1)*sizeof(char*));
	if( res == NULL ) {
		upsdebugx(1, "%s: Failed to realloc port list", __func__);
		return list;
	}
	res[count] = strdup(port);
	res[count+1] = NULL;

	return res;
}

/* Return a list of serial ports name, in 'ports_list', according to the OS,
 * the provided 'ports_range', and the number of available ports */
char ** nutscan_get_serial_ports_list(const char *ports_range)
{
	char start_port = 0;
	char stop_port = 0;
	char current_port = 0;
	char * list_sep_ptr = NULL;
	char ** ports_list = NULL;
	char str_tmp[128];
	char * tok;
	device_portname_t *cur_device = NULL;
	char * saveptr = NULL;
	char * range;
	int flag_auto = 0;

	/* 1) check ports_list */
	if ((ports_range == NULL) || (!strncmp(ports_range, "auto", 4))) {
		flag_auto = 1;
	}
	else {
		range = strdup(ports_range);
		/* we have a list:
		 * - single element: X (digit) or port name (COM1, /dev/ttyS0, ...)
		 * - range list: X-Y
		 * - multiple elements (coma separated): /dev/ttyS0,/dev/ttyUSB0 */
		if ( (list_sep_ptr = strchr(range, '-')) != NULL ) {
			tok = strtok_r(range,"-",&saveptr);
			if( tok[1] != 0 ) {
				fprintf(stderr,ERR_OUT_OF_BOUND);
				free(range);
				return NULL;
			}
			start_port = tok[0];
			tok = strtok_r(NULL,"-",&saveptr);
			if( tok != NULL ) {
				if( tok[1] != 0 ) {
					fprintf(stderr,ERR_OUT_OF_BOUND);
					free(range);
					return NULL;
				}
				stop_port = tok[0];
			}
			else {
				stop_port = start_port;
			}
		}
		else if ( ((list_sep_ptr = strchr(ports_range, ',')) != NULL )
				&& (is_serial_port_path(ports_range)) ) {
			tok = strtok_r(range,",",&saveptr);
			while( tok != NULL ) {
				ports_list = add_port(ports_list,tok);
				tok = strtok_r(NULL,",",&saveptr);
			}
		}
		else {
			/* we have been provided a single port name */
			/* it's a full device name */
			if( ports_range[1] != 0 ) {
				ports_list = add_port(ports_list,range);
			}
			/* it's device number */
			else {
				start_port = stop_port = ports_range[0];
			}
		}
		free(range);
	}


	if( start_port == 0 && !flag_auto) {
		return ports_list;
	}

	for (cur_device=device_portname;cur_device->name!= NULL;cur_device++) {
		if( flag_auto ) {
			start_port = cur_device->auto_start_port;
			stop_port = cur_device->auto_stop_port;
		}
		for( current_port=start_port; current_port <= stop_port;
				current_port++) {
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			/* We actually have a format string in the name,
			 * see the device_portname[] definition above */
			snprintf(str_tmp, sizeof(str_tmp), cur_device->name,
					current_port);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic pop
#endif

			ports_list = add_port(ports_list,str_tmp);
		}
	}
	return ports_list;
}

