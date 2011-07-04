/* scan_nut.c: detect remote NUT services
 * 
 *  Copyright (C) 2011 - Frederic Bohe <fredericbohe@eaton.com>
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

#include "config.h"
#include "upsclient.h"
#include "nut-scan.h"
#include "common.h"
#include <pthread.h>

static device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
static pthread_t * thread_array = NULL;
static int thread_count = 0;
#endif

struct scan_nut_arg {
	char * hostname;
	long timeout;
};

/* FIXME: SSL support */
static void * list_nut_devices(void * arg)
{
	struct scan_nut_arg * nut_arg = (struct scan_nut_arg*)arg;
	char *target_hostname = nut_arg->hostname;
	struct timeval tv;
	int port;
	unsigned int numq, numa;
	const char *query[4];
	char **answer;
	char *hostname = NULL;
	UPSCONN_t *ups = malloc(sizeof(*ups));
	device_t * dev = NULL;

	tv.tv_sec = nut_arg->timeout / (1000*1000);
	tv.tv_usec = nut_arg->timeout % (1000*1000);

	query[0] = "UPS";
	numq = 1;

	if (upscli_splitaddr(target_hostname, &hostname, &port) != 0) {
		free(target_hostname);
		free(nut_arg);
		return NULL;
	}
	if (upscli_tryconnect(ups, hostname, port,UPSCLI_CONN_TRYSSL,&tv) < 0) {
		free(target_hostname);
		free(nut_arg);
		return NULL;
	}

	if(upscli_list_start(ups, numq, query) < 0) {
		free(target_hostname);
		free(nut_arg);
		return NULL;
	}

	while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {
		/* UPS <upsname> <description> */
		if (numa < 3) {
			free(target_hostname);
			free(nut_arg);
			return NULL;
		}
		/* FIXME: check for duplication by getting driver.port and device.serial
		 * for comparison with other busses results */
		/* FIXME:
		 * - also print answer[2] if != "Unavailable"?
		 * - for upsmon.conf or ups.conf (using dummy-ups)? */
		if (numa >= 3) {
			dev = new_device();
			dev->type = TYPE_NUT;
			dev->driver = strdup("nutclient");
			if( asprintf(&dev->port,"\"%s@%s\"",answer[1],hostname)
					!= -1) {
#ifdef HAVE_PTHREAD
				pthread_mutex_lock(&dev_mutex);
#endif
				dev_ret = add_device_to_device(dev_ret,dev);
#ifdef HAVE_PTHREAD
				pthread_mutex_unlock(&dev_mutex);
#endif
			}

		}
	}

	free(target_hostname);
	free(nut_arg);
	return NULL;
}

device_t * scan_nut(char* startIP, char* stopIP, char* port,long usec_timeout)
{
	ip_iter_t ip;
	char * ip_str = NULL;
	char * ip_dest = NULL;
	char buf[SMALLBUF];
	struct sigaction oldact;
	int change_action_handler = 0;
	int i;
	struct scan_nut_arg *nut_arg;

#ifdef HAVE_PTHREAD
	pthread_t thread;

	pthread_mutex_init(&dev_mutex,NULL);
#endif

	/* Ignore SIGPIPE if the caller hasn't set a handler for it yet */
	if( sigaction(SIGPIPE, NULL, &oldact) == 0 ) {
		if( oldact.sa_handler == SIG_DFL ) {
			change_action_handler = 1;
			signal(SIGPIPE,SIG_IGN);
		}
	}

	ip_str = ip_iter_init(&ip,startIP,stopIP);

	while( ip_str != NULL )
	{
		if( port ) {
			if( ip.type == IPv4 ) {
				snprintf(buf,sizeof(buf),"%s:%s",ip_str,port);
			}
			else {
				snprintf(buf,sizeof(buf),"[%s]:%s",ip_str,port);
			}

			ip_dest = strdup(buf);
		}
		else {
			ip_dest = strdup(ip_str);
		}

		if((nut_arg = malloc(sizeof(struct scan_nut_arg))) == NULL ) {
			free(ip_dest);
			break;
		}

		nut_arg->timeout = usec_timeout;
		nut_arg->hostname = ip_dest;
#ifdef HAVE_PTHREAD
		if (pthread_create(&thread,NULL,list_nut_devices,(void*)nut_arg)==0){
			thread_count++;
			thread_array = realloc(thread_array,
					thread_count*sizeof(pthread_t));
			thread_array[thread_count-1] = thread;
		}
#else
		list_nut_devices(nut_arg);
#endif
		free(ip_str);
		ip_str = ip_iter_inc(&ip);
	}

#ifdef HAVE_PTHREAD
	for ( i=0; i < thread_count ; i++) {
		pthread_join(thread_array[i],NULL);
	}
	pthread_mutex_destroy(&dev_mutex);
	free(thread_array);
#endif

	if(change_action_handler) {
		signal(SIGPIPE,SIG_DFL);
	}

	return dev_ret;
}
