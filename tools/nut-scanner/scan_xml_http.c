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

#include "config.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <errno.h>
#include <arpa/inet.h>
#include "device.h"

#define BUF_SIZE	1024

device_t * scan_xml_http()
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
	char buf[BUF_SIZE];
	char string[BUF_SIZE];
	ssize_t recv_size;

	device_t * nut_dev = NULL;
	device_t * current_nut_dev = NULL;


	if((peerSocket = socket(AF_INET, SOCK_DGRAM, 0)) != -1)
	{
		/* Initialize socket */
		sockAddress.sin_family = AF_INET;
		sockAddress.sin_addr.s_addr = INADDR_BROADCAST;
		sockAddress.sin_port = htons(port);
		setsockopt(peerSocket, SOL_SOCKET, SO_BROADCAST, &sockopt_on, sizeof(sockopt_on));

		/* Send scan request */
		if(sendto(peerSocket, scanMsg, strlen(scanMsg), 0,
					(struct sockaddr *)&sockAddress, sockAddressLength) <= 0)
		{
			fprintf(stderr,"Error sending Eaton <SCAN_REQUEST/>\n");
		}
		else
		{
#define SELECT_TIMEOUT 2 /*FIXME : should be editable by user, not hard coded*/

			FD_ZERO(&fds);
			FD_SET(peerSocket,&fds);

			timeout.tv_sec = SELECT_TIMEOUT;
			timeout.tv_usec = 0;

			while ((ret=select(peerSocket+1,&fds,NULL,NULL,
						&timeout) )) {

				timeout.tv_sec = SELECT_TIMEOUT;
				timeout.tv_usec = 0;

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

				if( inet_ntop(AF_INET,
						&(sockAddress.sin_addr),
						buf,sizeof(buf)) == NULL ) {
					fprintf(stderr,
						"Error converting IP address \
						: %d\n",errno);
					continue;
				}

                                nut_dev = new_device();
                                if(nut_dev == NULL) {
                                        fprintf(stderr,"Memory allocation error\n");
                                        return NULL;
                                }

                                nut_dev->type = TYPE_XML;
				nut_dev->driver = strdup("netxml-ups");
				sprintf(string,"http://%s",buf);
				nut_dev->port = strdup(string);

				current_nut_dev = add_device_to_device(
						current_nut_dev,nut_dev);

			}
		}
	}
	else
	{
		fprintf(stderr,"Error creating socket\n");
	}


	return current_nut_dev;
}

