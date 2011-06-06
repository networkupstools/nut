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

void scan_xml_http()
{
        char *scanMsg = "<SCAN_REQUEST/>";
        int port = 4679;
        int peerSocket;
        int sockopt_on = 1;
        struct sockaddr_in sockAddress;
        socklen_t sockAddressLength = sizeof(sockAddress);
        memset(&sockAddress, 0, sizeof(sockAddress));

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
                        /*printf("Eaton <SCAN_REQUEST/> sent\n");*/
                        /* FIXME: handle replies */
                        ;
                }
        }
        else
        {
                fprintf(stderr,"Error creating socket\n");
        }

	/*XML*/
	//        while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {

	/* UPS <upsname> <description> */
	//                if (numa < 3) {
	//                        fprintf(stderr,"Error: insufficient data (got %d args, need at least 3)\n", numa);
	//                        return;
	//                }
	/* FIXME: check for duplication by getting driver.port and device.serial
	 * for comparison with other busses results */
	/* FIXME:
	 * - also print answer[2] if != "Unavailable"?
	 * - for upsmon.conf or ups.conf (using dummy-ups)? */
	//                printf("\t%s@%s\n", answer[1], hostname);
	//        }

}

