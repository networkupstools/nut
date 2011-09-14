/* ip.c: iterator for IPv4 or IPv6 addresses
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

#include "nutscan-ip.h"
#include <stdio.h>
#include "common.h"

static void increment_IPv6(struct in6_addr * addr)
{
	int i;

	for( i=15 ; i>= 0 ; i--) {
		addr->s6_addr[i]++;
		if( addr->s6_addr[i] != 0) {
			break;
		}
	}
}

static void invert_IPv6(struct in6_addr * addr1, struct in6_addr * addr2)
{
	struct in6_addr addr;

	memcpy(addr.s6_addr,addr1->s6_addr,sizeof(addr.s6_addr));
	memcpy(addr1->s6_addr,addr2->s6_addr,sizeof(addr.s6_addr));
	memcpy(addr2->s6_addr,addr.s6_addr,sizeof(addr.s6_addr));
}

/* Return the first ip or NULL if error */
char * nutscan_ip_iter_init(nutscan_ip_iter_t * ip, const char * startIP, const char * stopIP)
{
	int addr;
	int i;
	char buf[SMALLBUF];

	if( startIP == NULL ) {
		return NULL;
	}

	if(stopIP == NULL ) {
		stopIP = startIP;
	}

	ip->type = IPv4;
	/* Detecting IPv4 vs IPv6 */
	if(!inet_aton(startIP, &ip->start)) {
		/*Try IPv6 detection */
		ip->type = IPv6;
		if(!inet_pton(AF_INET6, startIP, &ip->start6)){
			fprintf(stderr,"Invalid address : %s\n",startIP);
			return NULL;
		}
	}

	/* Compute stop IP */
        if( ip->type == IPv4 ) {
                if(!inet_aton(stopIP, &ip->stop)) {
                        fprintf(stderr,"Invalid address : %s\n",stopIP);
                        return NULL;
                }
        }
        else {
                if(!inet_pton(AF_INET6, stopIP, &ip->stop6)){
                        fprintf(stderr,"Invalid address : %s\n",stopIP);
                        return NULL;
                }
        }

        /* Make sure start IP is lesser than stop IP */
        if( ip->type == IPv4 ) {
                if( ntohl(ip->start.s_addr) > ntohl(ip->stop.s_addr) ) {
                        addr = ip->start.s_addr;
                        ip->start.s_addr = ip->stop.s_addr;
                        ip->stop.s_addr = addr;
                }
		return strdup(inet_ntoa(ip->start));
        }
        else { /* IPv6 */
                for( i=0; i<16; i++ ) {
                        if( ip->start6.s6_addr[i] !=ip->stop6.s6_addr[i] ) {
                                if(ip->start6.s6_addr[i]>ip->stop6.s6_addr[i]){
                                        invert_IPv6(&ip->start6,&ip->stop6);
                                }
                                break;
                        }
                }
		return strdup(inet_ntop(AF_INET6,&ip->start6,buf,sizeof(buf)));
        }


}

/* return the next IP
return NULL if there is no more IP
*/
char * nutscan_ip_iter_inc(nutscan_ip_iter_t * ip)
{
	char buf[SMALLBUF];

	if( ip->type == IPv4 ) {
		/* Check if this is the last address to scan */
		if(ip->start.s_addr == ip->stop.s_addr) {
			return NULL;
		}
		/* increment the address (need to pass address in host
		   byte order, then pass back in network byte order */
		ip->start.s_addr = htonl((ntohl(ip->start.s_addr)+1));

		return strdup(inet_ntoa(ip->start));
	}
	else {
		/* Check if this is the last address to scan */
		if( memcmp(&ip->start6.s6_addr, &ip->stop6.s6_addr,
				sizeof(ip->start6.s6_addr)) == 0 ) {
			return NULL;
		}

		increment_IPv6(&ip->start6);

		return strdup(inet_ntop(AF_INET6,&ip->start6,buf,sizeof(buf)));
	}
}

int nutscan_cidr_to_ip(const char * cidr, char ** start_ip, char ** stop_ip)
{
	char * cidr_tok;
	char * first_ip;
	char * mask;
	char * saveptr = NULL;
	nutscan_ip_iter_t ip;
	int mask_val;
	int mask_byte;
	long mask_bit;
	char buf[SMALLBUF];

	*start_ip = NULL;
	*stop_ip = NULL;

	cidr_tok = strdup(cidr);
	first_ip = strdup(strtok_r(cidr_tok,"/",&saveptr));
	if( first_ip == NULL) {
		return 0;
	}
	mask = strtok_r(NULL,"/",&saveptr);
	if( mask == NULL ) {
		return 0;
	}
	free(cidr_tok);

	mask_val = atoi(mask);

        /* Detecting IPv4 vs IPv6 */
	ip.type = IPv4;
        if(!inet_aton(first_ip, &ip.start)) {
                /*Try IPv6 detection */
                ip.type = IPv6;
                if(!inet_pton(AF_INET6, first_ip, &ip.start6)){
			free(first_ip);
                        return 0;
                }
        }

	if( ip.type == IPv4 ) {

		if( mask_val > 0 ) {
			mask_val --;
			mask_bit = 0x80000000;
			mask_bit >>= mask_val;
			mask_bit--;
		}
		else {
			mask_bit = 0xffffffff;
		}
		ip.stop.s_addr = htonl(ntohl(ip.start.s_addr)|mask_bit);
		ip.start.s_addr = htonl(ntohl(ip.start.s_addr)&(~mask_bit));

		*start_ip = strdup(inet_ntoa(ip.start));
		*stop_ip = strdup(inet_ntoa(ip.stop));
		free(first_ip);
		return 1;
	}
	else {
                inet_pton(AF_INET6, first_ip, &ip.stop6);

		mask_byte = mask_val / 8;
		if( mask_byte < 16 ) {
			memset( &(ip.stop6.s6_addr[mask_byte+1]), 0xFF, 15 - mask_byte);
			memset( &(ip.start6.s6_addr[mask_byte+1]), 0x00, 15 - mask_byte);

			mask_bit = (0x100 >> mask_val%8)-1;
			ip.stop6.s6_addr[mask_byte] |= mask_bit;
			ip.start6.s6_addr[mask_byte] &= (~mask_bit);
		}

		inet_ntop(AF_INET6,&ip.start6,buf,sizeof(buf));
		*start_ip = strdup(buf);
		inet_ntop(AF_INET6,&ip.stop6,buf,sizeof(buf));
		*stop_ip = strdup(buf);
	}

	free(first_ip);
	return 1;
}
