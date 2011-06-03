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


/* FIXME: SSL support */
static void list_nut_devices(char *target_hostname)
{
        int port;
        unsigned int numq, numa;
        const char *query[4];
        char **answer;
        char *hostname = NULL;
        UPSCONN_t *ups = malloc(sizeof(*ups));

        query[0] = "UPS";
        numq = 1;


        if (upscli_splitaddr(target_hostname ? target_hostname : "localhost", &hostname, &port) != 0) {
                return;
        }

        if (upscli_connect(ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0) {

                fprintf(stderr,"Error: %s\n", upscli_strerror(ups));
                return;
        }

        if(upscli_list_start(ups, numq, query) < 0) {

                fprintf(stderr,"Error: %s\n", upscli_strerror(ups));
                return;
        }

        while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {
//
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
			if (numa >= 3) {
				printf("\t%s@%s\n", answer[1], hostname);
			}
        }
}

/* #ifdef nothing apart libupsclient! */
void scan_nut()
{
        /* try on localhost first */
        list_nut_devices(NULL);

        /* FIXME: network range iterator IPv4 and IPv6*/

}

