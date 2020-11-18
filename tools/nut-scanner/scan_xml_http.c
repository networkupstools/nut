/*
 *  Copyright (C) 2011 - EATON
 *  Copyright (C) 2016 - EATON - IP addressed XML scan
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

/*! \file scan_xml_http.c
    \brief detect NUT supported XML HTTP devices
    \author Frederic Bohe <fredericbohe@eaton.com>
    \author Michal Vyskocil <MichalVyskocil@eaton.com>
    \author Jim Klimov <EvgenyKlimov@eaton.com>
*/

#include "common.h"
#include "nut-scan.h"
#ifdef WITH_NEON
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <errno.h>
#include <ne_xml.h>
#include <ltdl.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

/* dynamic link library stuff */
static char * libname = "libneon"; /* Note: this is for info messages, not the SONAME */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;

static void (*nut_ne_xml_push_handler)(ne_xml_parser *p,
                         ne_xml_startelm_cb *startelm,
                         ne_xml_cdata_cb *cdata,
                         ne_xml_endelm_cb *endelm,
                         void *userdata);
static void (*nut_ne_xml_destroy)(ne_xml_parser *p);
static int (*nut_ne_xml_failed)(ne_xml_parser *p);
static ne_xml_parser * (*nut_ne_xml_create)(void);
static int (*nut_ne_xml_parse)(ne_xml_parser *p, const char *block, size_t len);

static nutscan_device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif

/* return 0 on error */
int nutscan_load_neon_library(const char *libname_path)
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
		fprintf(stderr, "Neon library not found. XML search disabled.\n");
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
	*(void **) (&nut_ne_xml_push_handler) = lt_dlsym(dl_handle,
						"ne_xml_push_handler");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_ne_xml_destroy) = lt_dlsym(dl_handle,"ne_xml_destroy");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_ne_xml_create) = lt_dlsym(dl_handle,"ne_xml_create");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_ne_xml_parse) = lt_dlsym(dl_handle,"ne_xml_parse");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_ne_xml_failed) = lt_dlsym(dl_handle,"ne_xml_failed");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	return 1;
err:
	fprintf(stderr, "Cannot load XML library (%s) : %s. XML search disabled.\n", libname, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}

/* A start-element callback for element with given namespace/name. */
static int startelm_cb(void *userdata, int parent, const char *nspace, const char *name, const char **atts) {
	nutscan_device_t * dev = (nutscan_device_t *)userdata;
	char buf[SMALLBUF];
	int i = 0;
	int result = -1;
	while( atts[i] != NULL ) {
		upsdebugx(5,"startelm_cb() : parent=%d nspace='%s' name='%s' atts[%d]='%s' atts[%d]='%s'",
			parent, nspace, name, i, atts[i], (i+1), atts[i+1]);
		/* The Eaton/MGE ePDUs almost exclusively support only XMLv4 protocol
		 * (only the very first generation of G2/G3 NMCs supported an older
		 * protocol, but all should have been FW upgraded by now), which NUT
		 * drivers don't yet support. To avoid failing drivers later, the
		 * nut-scanner should not suggest netxml-ups configuration for ePDUs
		 * at this time. */
		if(strcmp(atts[i],"class") == 0 && strcmp(atts[i+1],"DEV.PDU") == 0 ) {
			upsdebugx(3, "startelm_cb() : XML v4 protocol is not supported by current NUT drivers, skipping device!");
			/* netxml-ups currently only supports XML version 3 (for UPS),
			 * and not version 4 (for UPS and PDU)! */
			return -1;
		}
		if(strcmp(atts[i],"type") == 0) {
			snprintf(buf,sizeof(buf),"%s",atts[i+1]);
			nutscan_add_option_to_device(dev,"desc",buf);
			result = 0;
		}
		i=i+2;
	}
	return result;
}

static void * nutscan_scan_xml_http_generic(void * arg)
{
	nutscan_xml_t * sec = (nutscan_xml_t *)arg;
	char *scanMsg = "<SCAN_REQUEST/>";
/* Note: at this time the HTTP/XML scan is in fact not implemented - just the UDP part */
/*	int port_http = 80; */
	int port_udp = 4679;
/* A NULL "ip" causes a broadcast scan; otherwise the single ip address is queried directly */
	char *ip = NULL;
	long usec_timeout = -1;
	int peerSocket;
	int sockopt_on = 1;
	struct sockaddr_in sockAddress_udp;
	socklen_t sockAddressLength = sizeof(sockAddress_udp);
	memset(&sockAddress_udp, 0, sizeof(sockAddress_udp));
	fd_set fds;
	struct timeval timeout;
	int ret;
	char buf[SMALLBUF + 8];
	char string[SMALLBUF];
	ssize_t recv_size;
	int i;

	nutscan_device_t * nut_dev = NULL;
	if(sec != NULL) {
/*		if (sec->port_http > 0 && sec->port_http <= 65534)
 *			port_http = sec->port_http; */
		if (sec->port_udp > 0 && sec->port_udp <= 65534)
			port_udp = sec->port_udp;
		if (sec->usec_timeout > 0)
			usec_timeout = sec->usec_timeout;
		ip = sec->peername; /* NULL or not... */
	}

	if (usec_timeout <= 0)
		usec_timeout = 5000000; /* Driver default : 5sec */

	if( !nutscan_avail_xml_http ) {
		return NULL;
	}

	if((peerSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr,"Error creating socket\n");
		return NULL;
	}

/* FIXME : Per http://stackoverflow.com/questions/683624/udp-broadcast-on-all-interfaces
 * A single sendto() generates a single packet, so one must iterate all known interfaces... */
#define MAX_RETRIES 3
	for (i = 0; i != MAX_RETRIES ; i++) {
		/* Initialize socket */
		sockAddress_udp.sin_family = AF_INET;
		if (ip == NULL) {
			upsdebugx(2, "nutscan_scan_xml_http_generic() : scanning connected network segment(s) with a broadcast, attempt %d of %d with a timeout of %ld usec", (i+1), MAX_RETRIES, usec_timeout);
			sockAddress_udp.sin_addr.s_addr = INADDR_BROADCAST;
			setsockopt(peerSocket, SOL_SOCKET, SO_BROADCAST, &sockopt_on,
				sizeof(sockopt_on));
		} else {
			upsdebugx(2, "nutscan_scan_xml_http_generic() : scanning IP '%s' with a unicast, attempt %d of %d with a timeout of %ld usec", ip, (i+1), MAX_RETRIES, usec_timeout);
			inet_pton(AF_INET, ip, &(sockAddress_udp.sin_addr));
		}
		sockAddress_udp.sin_port = htons(port_udp);

		/* Send scan request */
		if(sendto(peerSocket, scanMsg, strlen(scanMsg), 0,
					(struct sockaddr *)&sockAddress_udp,
					sockAddressLength) <= 0)
		{
			fprintf(stderr,"Error sending Eaton <SCAN_REQUEST/> to %s, #%d/%d\n", ip ? ip : "<broadcast>", (i+1), MAX_RETRIES);
			usleep(usec_timeout);
			continue;
		}
		else
		{
			int retNum = 0;
			FD_ZERO(&fds);
			FD_SET(peerSocket,&fds);

			timeout.tv_sec = usec_timeout / 1000000;
			timeout.tv_usec = usec_timeout % 1000000;

			upsdebugx(5, "nutscan_scan_xml_http_generic() : sent request to %s, loop #%d/%d, waiting for responses", ip ? ip : "<broadcast>", (i+1), MAX_RETRIES);
			while ((ret=select(peerSocket+1,&fds,NULL,NULL,
						&timeout) )) {
				retNum ++;
				upsdebugx(5, "nutscan_scan_xml_http_generic() : request to %s, loop #%d/%d, response #%d", ip ? ip : "<broadcast>", (i+1), MAX_RETRIES, retNum);

				timeout.tv_sec = usec_timeout / 1000000;
				timeout.tv_usec = usec_timeout % 1000000;

				if( ret == -1 ) {
					fprintf(stderr,
						"Error waiting on \
						socket: %d\n",errno);
					break;
				}

				sockAddressLength = sizeof(struct sockaddr_in);
				recv_size = recvfrom(peerSocket,buf,
						sizeof(buf),0,
						(struct sockaddr *)&sockAddress_udp,
						&sockAddressLength);

				if(recv_size==-1) {
					fprintf(stderr,
						"Error reading \
						socket: %d, #%d/%d\n",errno, (i+1), MAX_RETRIES);
					usleep(usec_timeout);
					continue;
				}

				if( getnameinfo(
					(struct sockaddr *)&sockAddress_udp,
									sizeof(struct sockaddr_in),string,
									sizeof(string),NULL,0,
					NI_NUMERICHOST) != 0) {

					fprintf(stderr,
						"Error converting IP address: %d\n",errno);
					usleep(usec_timeout);
					continue;
				}

				nut_dev = nutscan_new_device();
				if(nut_dev == NULL) {
					fprintf(stderr,"Memory allocation error\n");
					goto end_abort;
				}

#ifdef HAVE_PTHREAD
				pthread_mutex_lock(&dev_mutex);
#endif
				upsdebugx(5, "Some host at IP %s replied to NetXML UDP request on port %d, inspecting the response...", string, port_udp);
				nut_dev->type = TYPE_XML;
				/* Try to read device type */
				ne_xml_parser *parser = (*nut_ne_xml_create)();
				(*nut_ne_xml_push_handler)(parser, startelm_cb,
							NULL, NULL, nut_dev);
				(*nut_ne_xml_parse)(parser, buf, recv_size);
				int parserFailed = (*nut_ne_xml_failed)(parser); /* 0 = ok, nonzero = fail */
				(*nut_ne_xml_destroy)(parser);

				if (parserFailed == 0) {
					nut_dev->driver = strdup("netxml-ups");
					sprintf(buf, "http://%s", string);
					nut_dev->port = strdup(buf);
					upsdebugx(3,"nutscan_scan_xml_http_generic(): Adding configuration for driver='%s' port='%s'", nut_dev->driver, nut_dev->port);
					dev_ret = nutscan_add_device_to_device(
						dev_ret,nut_dev);
#ifdef HAVE_PTHREAD
					pthread_mutex_unlock(&dev_mutex);
#endif
				}
				else
				{
					fprintf(stderr,"Device at IP %s replied with NetXML but was not deemed compatible with 'netxml-ups' driver (unsupported protocol version, etc.)\n", string);
					nutscan_free_device(nut_dev);
					nut_dev = NULL;
#ifdef HAVE_PTHREAD
					pthread_mutex_unlock(&dev_mutex);
#endif
					if (ip == NULL)
						continue; /* skip this device; note that for broadcast scan there may be more in the loop's queue */
				}

				if (ip != NULL) {
					upsdebugx(2,"nutscan_scan_xml_http_generic(): we collected one reply to unicast for %s (repsponse from %s), done", ip, string);
					goto end;
				}
			} /* while select() responses */
			if (ip == NULL && dev_ret != NULL) {
				upsdebugx(2,"nutscan_scan_xml_http_generic(): we collected one round of replies to broadcast with no errors, done");
				goto end;
			}
		}
	}
	upsdebugx(2,"nutscan_scan_xml_http_generic(): no replies collected for %s, done", ip ? ip : "<broadcast>");
	goto end;

end_abort:
	upsdebugx(1,"Had to abort nutscan_scan_xml_http_generic() for %s, see fatal details above", ip ? ip : "<broadcast>");
end:
	if (ip != NULL) /* do not free "ip", it comes from caller */
		close(peerSocket);
	return NULL;
}

nutscan_device_t * nutscan_scan_xml_http_range(const char * start_ip, const char * end_ip, long usec_timeout, nutscan_xml_t * sec)
{
	nutscan_xml_t * tmp_sec = NULL;
	nutscan_device_t * result = NULL;
	int i;

	if( !nutscan_avail_xml_http ) {
		return NULL;
	}

	if (start_ip == NULL && end_ip != NULL) {
		start_ip = end_ip;
	}

	if (start_ip == NULL ) {
		upsdebugx(1,"Scanning XML/HTTP bus using broadcast.");
	} else {
		if ( (start_ip == end_ip) || (end_ip == NULL) || (strncmp(start_ip,end_ip,128)==0) ) {
			upsdebugx(1,"Scanning XML/HTTP bus for single IP (%s).", start_ip);
		} else {
			/* Iterate the range of IPs to scan */
			nutscan_ip_iter_t ip;
			char * ip_str = NULL;
#ifdef HAVE_PTHREAD
			pthread_t thread;
			pthread_t * thread_array = NULL;
			int thread_count = 0;

			pthread_mutex_init(&dev_mutex,NULL);
#endif

			ip_str = nutscan_ip_iter_init(&ip, start_ip, end_ip);

			while(ip_str != NULL) {
				tmp_sec = malloc(sizeof(nutscan_xml_t));
				if (tmp_sec == NULL) {
					fprintf(stderr,"Memory allocation \
						error\n");
					return NULL;
				}
				memcpy(tmp_sec, sec, sizeof(nutscan_xml_t));
				tmp_sec->peername = ip_str;
				if (tmp_sec->usec_timeout < 0) tmp_sec->usec_timeout = usec_timeout;

#ifdef HAVE_PTHREAD
				if (pthread_create(&thread,NULL,nutscan_scan_xml_http_generic, (void *)tmp_sec)==0){
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
				nutscan_scan_xml_http_generic((void *)tmp_sec);
#endif
/*				free(ip_str); */ /* One of these free()s seems to cause a double-free */
				ip_str = nutscan_ip_iter_inc(&ip);
/*				free(tmp_sec); */
			};

#ifdef HAVE_PTHREAD
			if (thread_array != NULL) {
				for ( i=0; i < thread_count ; i++) {
						pthread_join(thread_array[i],NULL);
				}
				free(thread_array);
			}
			pthread_mutex_destroy(&dev_mutex);
#endif
			result = nutscan_rewind_device(dev_ret);
			dev_ret = NULL;
			return result;
		}
	}

	tmp_sec = malloc(sizeof(nutscan_xml_t));
	if (tmp_sec == NULL) {
		fprintf(stderr,"Memory allocation \
			error\n");
		return NULL;
	}

	memcpy(tmp_sec, sec, sizeof(nutscan_xml_t));
	if (start_ip == NULL) {
		tmp_sec->peername = NULL;
	} else {
		tmp_sec->peername = strdup(start_ip);
	}
	if (tmp_sec->usec_timeout < 0) tmp_sec->usec_timeout = usec_timeout;
	nutscan_scan_xml_http_generic(tmp_sec);
	result = nutscan_rewind_device(dev_ret);
	dev_ret = NULL;
	free(tmp_sec);
	return result;
}
#else /* WITH_NEON */
nutscan_device_t * nutscan_scan_xml_http_range(const char * start_ip, const char * end_ip, long usec_timeout, nutscan_xml_t * sec)
{
	return NULL;
}
#endif /* WITH_NEON */
