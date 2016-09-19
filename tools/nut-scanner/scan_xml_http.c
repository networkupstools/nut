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
		/* netxml-ups currently only supports XML version 3 (for UPS),
		 * and not version 4 (for UPS and PDU)! */
		upsdebugx(5,"startelm_cb() : parent=%d nspace='%s' name='%s' atts[%d]='%s' atts[%d]='%s'",
			parent, nspace, name, i, atts[i], (i+1), atts[i+1]);
// This test from mge-xml.c works when parsing a product.xml file.
// Unfortunately, different products serve it over different URIs
// and over HTTP, so porting its support here would be complicated.
// As an indirect way to detect the version, the XMLv4 devices serve
// the "/product.xml" with protocol version in it, and XMLv3 ones
// serve the "/mgeups/product.xml" without protocol info, by spec...
/*
		if (parent == NE_XML_STATEROOT && !strcasecmp(name, "PRODUCT_INFO") && !strcasecmp(atts[i], "protocol")) {
			if (!strcasecmp(atts[i+1], "XML.V4")) {
				fprintf(stderr, "ERROR: XML v4 protocol is not supported by current NUT drivers, skipping device!\n");
				return -1;
			}
		}
*/
// The Eaton/MGE ePDUs almost exclusively support only XMLv4 protocol
// (only the very first generation of G2/G3 NMCs supported an older
// protocol, but all should have been FW upgraded by now), which NUT
// drivers don't yet support. To avoid failing drivers later, the
// nut-scanner should not suggest netxml-ups configuration for ePDUs
// at this time.
		if(strcmp(atts[i],"class") == 0 && strcmp(atts[i+1],"DEV.PDU") == 0 ) {
			upsdebugx(1, "XML v4 protocol is not supported by current NUT drivers, skipping device!");
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
				int parserFailed = (*nut_ne_xml_failed)(parser); // 0 = ok, nonzero = fail
				(*nut_ne_xml_destroy)(parser);

				if (parserFailed == 0) {
					nut_dev->driver = strdup("netxml-ups");
					sprintf(buf,"http://%s",string);
					nut_dev->port = strdup(buf);

					current_nut_dev = nutscan_add_device_to_device(
						current_nut_dev,nut_dev);
				}
				else
				{
					fprintf(stderr,"Device replied with NetXML but was not deemed compatible\n");
					return NULL;
				}

			}
		}
	}
	else
	{
		fprintf(stderr,"Error creating socket\n");
	}


	return nutscan_rewind_device(current_nut_dev);
}
nutscan_device_t * ETN_nutscan_scan_xml_http(const char * start_ip, long usec_timeout, nutscan_xml_t * sec)
{
	char *scanMsg = "<SCAN_REQUEST/>";
	int port = 4679;
	int peerSocket;
	struct sockaddr_in sockAddress;
	socklen_t sockAddressLength = sizeof(sockAddress);
	memset(&sockAddress, 0, sizeof(sockAddress));
	fd_set fds;
	struct timeval timeout;
	int ret;
	char buf[SMALLBUF];
	char string[SMALLBUF];
	ssize_t recv_size;
	int i;

	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;

	if( !nutscan_avail_xml_http ) {
		return NULL;
	}

	if((peerSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr,"Error creating socket\n");
		return NULL;
	}
#define MAX 3
	for (i = 0; i != MAX && current_nut_dev == NULL; i++) {
		/* Initialize socket */
		sockAddress.sin_family = AF_INET;
		//sockAddress.sin_addr.s_addr = INADDR_BROADCAST;
		inet_pton(AF_INET, start_ip, &(sockAddress.sin_addr));
		sockAddress.sin_port = htons(port);
		//setsockopt(peerSocket, SOL_SOCKET, SO_BROADCAST, &sockopt_on,
		//		sizeof(sockopt_on));

		/* Send scan request */
		if(sendto(peerSocket, scanMsg, strlen(scanMsg), 0,
					(struct sockaddr *)&sockAddress,
					sockAddressLength) <= 0)
		{
			fprintf(stderr,"Error sending Eaton <SCAN_REQUEST/>, #%d/%d\n", i, MAX);
			usleep(usec_timeout);
			continue;
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
						socket: %d, #%d/%d\n",errno, i, MAX);
					usleep(usec_timeout);
					continue;
				}

				if( getnameinfo(
					(struct sockaddr *)&sockAddress,
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
				int parserFailed = (*nut_ne_xml_failed)(parser); // 0 = ok, nonzero = fail
				(*nut_ne_xml_destroy)(parser);

				if (parserFailed == 0) {
					nut_dev->driver = strdup("netxml-ups");
					sprintf(buf,"http://%s",string);
					nut_dev->port = strdup(buf);

					current_nut_dev = nutscan_add_device_to_device(
						current_nut_dev,nut_dev);
				}
				else
				{
					fprintf(stderr,"Device replied with NetXML but was not deemed compatible\n");
					close(peerSocket);
					return NULL; // XXX: Perhaps revise when/if we learn to scan many devices
				}

				//XXX: quick and dirty change - now we scanned exactly ONE IP address,
				//     which is exactly the amount we wanted
				goto end;
			}
		}
	}

end:
	close(peerSocket);
	return nutscan_rewind_device(current_nut_dev);
}
#else /* WITH_NEON */
nutscan_device_t * nutscan_scan_xml_http(long usec_timeout)
{
	return NULL;
}
nutscan_device_t * ETN_nutscan_scan_xml_http(const char * start_ip, long usec_timeout, nutscan_xml_t * sec)
{
	return NULL;
}
#endif /* WITH_NEON */
