/* scan_snmp.c: detect NUT supported SNMP devices
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
#include "device.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <pthread.h>
#include "nutscan-snmp.h"
#include "common.h"

#ifdef HAVE_NET_SNMP_NET_SNMP_CONFIG_H

#define SysOID ".1.3.6.1.2.1.1.2"

device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
pthread_mutex_t dev_mutex;
pthread_t * thread_array = NULL;
int thread_count = 0;
#endif
long g_usec_timeout ;

enum network_type {
	IPv4,
	IPv6
};

void try_all_oid(void * handle)
{
        oid name[MAX_OID_LEN];
        size_t name_len = MAX_OID_LEN;
	struct snmp_pdu *pdu, *response = NULL;
	int index = 0;
	device_t * dev = NULL;
	struct snmp_session * session;

	while(snmp_device_table[index].oid != NULL) {

		/* create and send request. */
		name_len = MAX_OID_LEN;
		if (!snmp_parse_oid(snmp_device_table[index].oid, name, &name_len)) {
			index++;
			continue;
		}

		pdu = snmp_pdu_create(SNMP_MSG_GET);

		if (pdu == NULL) {
			index++;
			continue;
		}

		snmp_add_null_var(pdu, name, name_len);

		snmp_sess_synch_response(handle,pdu, &response);
		if( response == NULL ) {
			index++;
			continue;
		}

		if( response->errstat != SNMP_ERR_NOERROR) {
			snmp_free_pdu(response);
			response = NULL;
			index++;
			continue;
		}

		session = snmp_sess_session(handle);
		/* SNMP device found */
		dev = new_device();
		dev->type = TYPE_SNMP;
		dev->driver = strdup("snmp-ups");
		dev->port = strdup(session->peername);
		add_option_to_device(dev,"mibs",snmp_device_table[index].mib);

#ifdef HAVE_PTHREAD
		pthread_mutex_lock(&dev_mutex);
#endif
		dev_ret = add_device_to_device(dev_ret,dev);
#ifdef HAVE_PTHREAD
		pthread_mutex_unlock(&dev_mutex);
#endif

		snmp_free_pdu(response);
		response = NULL;

		index++;
	}
}

void * try_SysOID(void * arg)
{
	struct snmp_session snmp_sess;
	void * handle;
	struct snmp_pdu *pdu, *response = NULL;
        oid name[MAX_OID_LEN];
        size_t name_len = MAX_OID_LEN;
	char * peername = (char *)arg;

	/* Initialize session */
	snmp_sess_init(&snmp_sess);

	snmp_sess.peername = peername;

	snmp_sess.version = SNMP_VERSION_1;
	snmp_sess.community = (unsigned char *)"public";
	snmp_sess.community_len = strlen("public");

	snmp_sess.retries = 0;
	snmp_sess.timeout = g_usec_timeout;

	/* Open the session */
	handle = snmp_sess_open(&snmp_sess); /* establish the session */
	if (handle == NULL) {
		fprintf(stderr,"Failed to open SNMP session for %s.\n",
			peername);
		free(peername);
		return NULL;
	}

	/* create and send request. */
	if (!snmp_parse_oid(SysOID, name, &name_len)) {
		fprintf(stderr,"SNMP errors: %s\n",
				snmp_api_errstring(snmp_errno));
		snmp_sess_close(handle);
		free(peername);
		return NULL;
	}

	pdu = snmp_pdu_create(SNMP_MSG_GET);

	if (pdu == NULL) {
		fprintf(stderr,"Not enough memory\n");
		free(peername);
		return NULL;
	}

	snmp_add_null_var(pdu, name, name_len);

	snmp_sess_synch_response(handle,
			pdu, &response);

	if (response) {
		/* SNMP device found */
		/* SysOID is supposed to give the required MIB. */
		/* FIXME: until we are sure it gives the MIB for all devices, use the old method: */
		/* try a list of known OID */
		try_all_oid(handle);

		snmp_free_pdu(response);
		response = NULL;
	}

	snmp_sess_close(handle);
	free(peername);

	return NULL;
}

void increment_IPv6(struct in6_addr * addr)
{
	addr->s6_addr32[3]=htonl((ntohl(addr->s6_addr32[3])+1));
	if( addr->s6_addr32[3] == 0 ) {
		addr->s6_addr32[2] = htonl((ntohl(addr->s6_addr32[2])+1));
		if( addr->s6_addr32[2] == 0 ) {
			addr->s6_addr32[1]=htonl((ntohl(addr->s6_addr32[1])+1));
			if( addr->s6_addr32[1] == 0 ) {
				addr->s6_addr32[0] =
					htonl((ntohl(addr->s6_addr32[0])+1));
			}
		}
	}
}

void invert_IPv6(struct in6_addr * addr1, struct in6_addr * addr2)
{
	int i;
	unsigned long addr;

	for( i=0; i<4; i++) {
		addr = addr1->s6_addr32[i];
		addr1->s6_addr32[i] = addr2->s6_addr32[i];
		addr2->s6_addr32[i] = addr;
	}
}

device_t * scan_snmp(char * start_ip, char * stop_ip,long usec_timeout)
{
	int addr;
	struct in_addr current_addr;
	struct in_addr stop_addr;
	struct in6_addr current_addr6;
	struct in6_addr stop_addr6;
	char * peername = NULL;
	enum network_type type = IPv4;
	char buf[SMALLBUF];
	int i;
#ifdef HAVE_PTHREAD
	pthread_t thread;
#endif

	g_usec_timeout = usec_timeout;

	if( start_ip == NULL ) {
		return NULL;
	}

	if( stop_ip == NULL ) {
		stop_ip = start_ip;
	}

	if(!inet_aton(start_ip, &current_addr)) {
		/*Try IPv6 detection */
		type = IPv6;
		if(!inet_pton(AF_INET6, start_ip, &current_addr6)){
			fprintf(stderr,"Invalid address : %s\n",start_ip);
			return NULL;
		}
	}

	if( type == IPv4 ) {
		if(!inet_aton(stop_ip, &stop_addr)) {
			fprintf(stderr,"Invalid address : %s\n",stop_ip);
			return NULL;
		}
	}
	else {
		if(!inet_pton(AF_INET6, stop_ip, &stop_addr6)){
			fprintf(stderr,"Invalid address : %s\n",stop_ip);
			return NULL;
		}
	}

#ifdef HAVE_PTHREAD
	pthread_mutex_init(&dev_mutex,NULL);
#endif

	/* Make sure current addr is lesser than stop addr */
	if( type == IPv4 ) {
		if( ntohl(current_addr.s_addr) > ntohl(stop_addr.s_addr) ) {
			addr = current_addr.s_addr;
			current_addr.s_addr = stop_addr.s_addr;
			stop_addr.s_addr = addr;
		}
	}
	else { /* IPv6 */
		for( i=0; i<4; i++ ) {
			if( ntohl(current_addr6.s6_addr32[i]) !=
				ntohl(stop_addr6.s6_addr32[i]) ) {
				if( ntohl(current_addr6.s6_addr32[i]) >
					ntohl(stop_addr6.s6_addr32[i])) {
					invert_IPv6(&current_addr6,
							&stop_addr6);
				}
				break;
			}
		}
	}

	/* Initialize the SNMP library */
	init_snmp("nut-scanner");

	while(1) {

		if( type == IPv4 ) {
			peername = strdup(inet_ntoa(current_addr));
		}
		else { /* IPv6 */
			peername = strdup(inet_ntop(AF_INET6,&current_addr6,buf,
							sizeof(buf)));
		}
#ifdef HAVE_PTHREAD
		if (pthread_create(&thread,NULL,try_SysOID,(void*)peername)==0){
			thread_count++;
			thread_array = realloc(thread_array,
						thread_count*sizeof(pthread_t));
			thread_array[thread_count-1] = thread;
		}
#else
		try_SysOID((void *)peername);
#endif
		if( type == IPv4 ) {
			/* Check if this is the last address to scan */
			if(current_addr.s_addr == stop_addr.s_addr) {
				break;
			}
			/* increment the address (need to pass address in host
			   byte order, then pass back in network byte order */
			current_addr.s_addr = htonl((ntohl(current_addr.s_addr)+
								1));
		}
		else {
			/* Check if this is the last address to scan */
			if(current_addr6.s6_addr32[0]==stop_addr6.s6_addr32[0]&&
			current_addr6.s6_addr32[1]==stop_addr6.s6_addr32[1]&&
			current_addr6.s6_addr32[2]==stop_addr6.s6_addr32[2]&&
			current_addr6.s6_addr32[3]==stop_addr6.s6_addr32[3]){
				break;
			}

			increment_IPv6(&current_addr6);
		}

	};

#ifdef HAVE_PTHREAD
	for ( i=0; i < thread_count ; i++) {
		pthread_join(thread_array[i],NULL);
	}
	pthread_mutex_destroy(&dev_mutex);
#endif

	return dev_ret;
}
#endif /* HAVE_NET_SNMP_NET_SNMP_CONFIG_H */


