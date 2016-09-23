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

nutscan_device_t * nutscan_scan_xml_http_generic(const char *ip, long usec_timeout, nutscan_xml_t * sec)
{
/* A NULL "ip" causes a broadcast scan; otherwise the ip address is queried directly */
/* Note: at this time the HTTP/XML scan is in fact not implemented - just the UDP part */
	char *scanMsg = "<SCAN_REQUEST/>";
//	int port_http = 80;
	int port_udp = 4679;
	int peerSocket;
	int sockopt_on = 1;
	struct sockaddr_in sockAddress_udp;
	socklen_t sockAddressLength = sizeof(sockAddress_udp);
	memset(&sockAddress_udp, 0, sizeof(sockAddress_udp));
	fd_set fds;
	struct timeval timeout;
	int ret;
	char buf[SMALLBUF];
	char string[SMALLBUF];
	ssize_t recv_size;
	int i;

	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;
	if(sec != NULL) {
//		if (sec->port_http > 0 && sec->port_http <= 65534)
//			port_http = sec->port_http;
		if (sec->port_udp > 0 && sec->port_udp <= 65534)
			port_udp = sec->port_udp;
	}

	if( !nutscan_avail_xml_http ) {
		return NULL;
	}

	if((peerSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr,"Error creating socket\n");
		return NULL;
	}

#define MAX_RETRIES 3
	for (i = 0; i != MAX_RETRIES && current_nut_dev == NULL; i++) {
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
			fprintf(stderr,"Error sending Eaton <SCAN_REQUEST/>, #%d/%d\n", (i+1), MAX_RETRIES);
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
					fprintf(stderr,"Memory allocation \
						error\n");
					return NULL;
				}

				upsdebugx(5, "Some host at IP %s replied to UDP request on port %d, inspecting the response...", string, port_udp);
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
					fprintf(stderr,"Device at IP %s replied with NetXML but was not deemed compatible with 'netxml-ups' driver (unsupported protocol version, etc.)\n", string);
					if (ip != NULL) {
						close(peerSocket);
						return NULL; // XXX: Perhaps revise when/if we learn to scan many devices
					}
					continue; // skip this device; note that for broadcast scan there may be more in the loop's queue
				}

				//XXX: quick and dirty change - now we scanned exactly ONE IP address,
				//     which is exactly the amount we wanted
				goto end;
			}
		}
	}

end:
	if (ip != NULL)
		close(peerSocket);
	return nutscan_rewind_device(current_nut_dev);
}

nutscan_device_t * nutscan_scan_xml_http_range(const char * start_ip, const char * end_ip, long usec_timeout, nutscan_xml_t * sec)
{
	if (start_ip == NULL && end_ip != NULL) {
		start_ip = end_ip;
	}

	if (start_ip != NULL ) {
		upsdebugx(1,"Scanning XML/HTTP bus for single IP (%s).", start_ip);
		if ( (start_ip != end_ip) || (strncmp(start_ip,end_ip,128)!=0) )
			upsdebugx(1,"WARN: single IP scanning of XML/HTTP bus currently ignores range requests (will not iterate up to %s).", end_ip);
// FIXME: Add scanning of ranges or subnets (needs a way to iterate IP addresses)
	} else {
		upsdebugx(1,"Scanning XML/HTTP bus using broadcast.");
	}

	return nutscan_scan_xml_http_generic(start_ip, usec_timeout, sec);
}
#else /* WITH_NEON */
nutscan_device_t * nutscan_scan_xml_http_generic(const char * ip, long usec_timeout, nutscan_xml_t * sec)
{
	return NULL;
}

nutscan_device_t * nutscan_scan_xml_http_range(const char * start_ip, const char * end_ip, long usec_timeout, nutscan_xml_t * sec)
{
	return NULL;
}
#endif /* WITH_NEON */
