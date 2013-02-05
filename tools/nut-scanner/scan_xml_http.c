/* scan_xml_http.c: detect NUT supported XML HTTP devices
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

/* dynamic link library stuff */
static char * libname = "libneon";
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;

static void (*nut_ne_xml_push_handler)(ne_xml_parser *p,
                         ne_xml_startelm_cb *startelm,
                         ne_xml_cdata_cb *cdata,
                         ne_xml_endelm_cb *endelm,
                         void *userdata);
static void (*nut_ne_xml_destroy)(ne_xml_parser *p);
static ne_xml_parser * (*nut_ne_xml_create)(void);
static int (*nut_ne_xml_parse)(ne_xml_parser *p, const char *block, size_t len);

/* return 0 on error */
int nutscan_load_neon_library()
{

        if( dl_handle != NULL ) {
                /* if previous init failed */
                if( dl_handle == (void *)1 ) {
                        return 0;
                }
                /* init has already been done */
                return 1;
        }

        if( lt_dlinit() != 0 ) {
                fprintf(stderr, "Error initializing lt_init\n");
                return 0;
        }

        dl_handle = lt_dlopenext(libname);
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

        return 1;
err:
        fprintf(stderr, "Cannot load XML library (%s) : %s. XML search disabled.\n", libname, dl_error);
        dl_handle = (void *)1;
	lt_dlexit();
        return 0;
}

static int startelm_cb(void *userdata, int parent, const char *nspace, const char *name, const char **atts) {
	nutscan_device_t * dev = (nutscan_device_t *)userdata;
	char buf[SMALLBUF];
	int i = 0;
	while( atts[i] != NULL ) {
		if(strcmp(atts[i],"type") == 0) {
			snprintf(buf,sizeof(buf),"%s",atts[i+1]);
			nutscan_add_option_to_device(dev,"desc",buf);
			return 0;
		}
		i=i+2;
	}
	return 0;
}

nutscan_device_t * nutscan_scan_xml_http(long usec_timeout)
{
	char *scanMsg = "<SCAN_REQUEST/>";
	int port = 4679;
	int peerSocket;
	int sockopt_on = 1;
	struct sockaddr_in sockAddress;
	socklen_t sockAddressLength = sizeof(sockAddress);
	memset(&sockAddress, 0, sizeof(sockAddress));
	fd_set fds;
	struct timeval timeout;
	int ret;
	char buf[SMALLBUF];
	char string[SMALLBUF];
	ssize_t recv_size;

	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;

        if( !nutscan_avail_xml_http ) {
                return NULL;
        }

	if((peerSocket = socket(AF_INET, SOCK_DGRAM, 0)) != -1)
	{
		/* Initialize socket */
		sockAddress.sin_family = AF_INET;
		sockAddress.sin_addr.s_addr = INADDR_BROADCAST;
		sockAddress.sin_port = htons(port);
		setsockopt(peerSocket, SOL_SOCKET, SO_BROADCAST, &sockopt_on,
				sizeof(sockopt_on));

		/* Send scan request */
		if(sendto(peerSocket, scanMsg, strlen(scanMsg), 0,
					(struct sockaddr *)&sockAddress,
					sockAddressLength) <= 0)
		{
			fprintf(stderr,"Error sending Eaton <SCAN_REQUEST/>\n");
		}
		else
		{
			FD_ZERO(&fds);
			FD_SET(peerSocket,&fds);

			timeout.tv_sec = usec_timeout / 1000000;
			timeout.tv_usec = usec_timeout % 1000000;

			while ((ret=select(peerSocket+1,&fds,NULL,NULL,
						&timeout) )) {

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
						(struct sockaddr *)&sockAddress,
						&sockAddressLength);

				if(recv_size==-1) {
					fprintf(stderr,
						"Error reading \
						socket: %d\n",errno);
					continue;
				}

			        if( getnameinfo(
					(struct sockaddr *)&sockAddress,
                               		sizeof(struct sockaddr_in),string,
	                                sizeof(string),NULL,0,
					NI_NUMERICHOST) != 0) {

					fprintf(stderr,
						"Error converting IP address \
						: %d\n",errno);
					continue;
				}

                                nut_dev = nutscan_new_device();
                                if(nut_dev == NULL) {
                                        fprintf(stderr,"Memory allocation \
					error\n");
                                        return NULL;
                                }

                                nut_dev->type = TYPE_XML;
				/* Try to read device type */
				ne_xml_parser *parser = (*nut_ne_xml_create)();
				(*nut_ne_xml_push_handler)(parser, startelm_cb,
							NULL, NULL, nut_dev);
				(*nut_ne_xml_parse)(parser, buf, recv_size);
				(*nut_ne_xml_destroy)(parser);

				nut_dev->driver = strdup("netxml-ups");
				sprintf(buf,"http://%s",string);
				nut_dev->port = strdup(buf);

				current_nut_dev = nutscan_add_device_to_device(
						current_nut_dev,nut_dev);

			}
		}
	}
	else
	{
		fprintf(stderr,"Error creating socket\n");
	}


	return nutscan_rewind_device(current_nut_dev);
}
#else /* WITH_NEON */
nutscan_device_t * nutscan_scan_xml_http(long usec_timeout)
{
	return NULL;
}
#endif /* WITH_NEON */
