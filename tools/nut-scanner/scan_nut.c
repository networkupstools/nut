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

/*! \file scan_nut.c
    \brief detect remote NUT services
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#include "common.h"
#include "upsclient.h"
#include "nut-scan.h"
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif
#include <ltdl.h>

/* dynamic link library stuff */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;

static int (*nut_upscli_splitaddr)(const char *buf,char **hostname, int *port);
static int (*nut_upscli_tryconnect)(UPSCONN_t *ups, const char *host, int port,
					int flags,struct timeval * timeout);
static int (*nut_upscli_list_start)(UPSCONN_t *ups, unsigned int numq,
					const char **query);
static int (*nut_upscli_list_next)(UPSCONN_t *ups, unsigned int numq,
			const char **query,unsigned int *numa, char ***answer);
static int (*nut_upscli_disconnect)(UPSCONN_t *ups);

static nutscan_device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif

struct scan_nut_arg {
	char * hostname;
	long timeout;
};

/* return 0 on error; visible externally */
int nutscan_load_upsclient_library(const char *libname_path);
int nutscan_load_upsclient_library(const char *libname_path)
{
	if( dl_handle != NULL ) {
			/* if previous init failed */
			if( dl_handle == (void *)1 ) {
					return 0;
			}
			/* init has already been done */
			return 1;
	}

	if (libname_path == NULL) {
		fprintf(stderr, "NUT client library not found. NUT search disabled.\n");
		return 0;
	}

	if( lt_dlinit() != 0 ) {
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
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_upscli_tryconnect) = lt_dlsym(dl_handle,
						"upscli_tryconnect");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_upscli_list_start) = lt_dlsym(dl_handle,
						"upscli_list_start");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_upscli_list_next) = lt_dlsym(dl_handle,
						"upscli_list_next");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_upscli_disconnect) = lt_dlsym(dl_handle,
						"upscli_disconnect");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	return 1;
err:
	fprintf(stderr, "Cannot load NUT library (%s) : %s. NUT search disabled.\n", libname_path, dl_error);
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
	int port;
	unsigned int numq, numa;
	const char *query[4];
	char **answer;
	char *hostname = NULL;
	UPSCONN_t *ups = malloc(sizeof(*ups));
	nutscan_device_t * dev = NULL;
	int buf_size;

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

	if ((*nut_upscli_tryconnect)(ups, hostname, port,UPSCLI_CONN_TRYSSL,&tv) < 0) {
		free(target_hostname);
		free(nut_arg);
		free(ups);
		return NULL;
	}

	if((*nut_upscli_list_start)(ups, numq, query) < 0) {
		(*nut_upscli_disconnect)(ups);
		free(target_hostname);
		free(nut_arg);
		free(ups);
		return NULL;
	}

	while ((*nut_upscli_list_next)(ups,numq, query, &numa, &answer) == 1) {
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
		dev->driver = strdup("nutclient");
		/* +1+1 is for '@' character and terminating 0 */
		buf_size = strlen(answer[1])+strlen(hostname)+1+1;
		dev->port = malloc(buf_size);
		if( dev->port ) {
			snprintf(dev->port,buf_size,"%s@%s",answer[1],
					hostname);
#ifdef HAVE_PTHREAD
			pthread_mutex_lock(&dev_mutex);
#endif
			dev_ret = nutscan_add_device_to_device(dev_ret,dev);
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

nutscan_device_t * nutscan_scan_nut(const char* startIP, const char* stopIP, const char* port,long usec_timeout)
{
	nutscan_ip_iter_t ip;
	char * ip_str = NULL;
	char * ip_dest = NULL;
	char buf[SMALLBUF];
	struct sigaction oldact;
	int change_action_handler = 0;
	int i;
	struct scan_nut_arg *nut_arg;
#ifdef HAVE_PTHREAD
	pthread_t thread;
	pthread_t * thread_array = NULL;
	int thread_count = 0;

	pthread_mutex_init(&dev_mutex,NULL);
#endif

        if( !nutscan_avail_nut ) {
                return NULL;
        }

	/* Ignore SIGPIPE if the caller hasn't set a handler for it yet */
	if( sigaction(SIGPIPE, NULL, &oldact) == 0 ) {
		if( oldact.sa_handler == SIG_DFL ) {
			change_action_handler = 1;
			signal(SIGPIPE,SIG_IGN);
		}
	}

	ip_str = nutscan_ip_iter_init(&ip,startIP,stopIP);

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
		list_nut_devices(nut_arg);
#endif
		free(ip_str);
		ip_str = nutscan_ip_iter_inc(&ip);
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

	return nutscan_rewind_device(dev_ret);
}
