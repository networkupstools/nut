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
#include "nutscan-snmp.h"

#ifdef HAVE_NET_SNMP_NET_SNMP_CONFIG_H
/* FIXME : add IPv6 */

#define SysOID ".1.3.6.1.2.1.1.2"

device_t * try_all_oid(struct snmp_session * session)
{
        oid name[MAX_OID_LEN];
        size_t name_len = MAX_OID_LEN;
	struct snmp_pdu *pdu, *response = NULL;
	int index = 0;
	device_t * dev = NULL;

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

		snmp_synch_response(session,pdu, &response);
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

		/* SNMP device found */
		dev = new_device();
		dev->type = TYPE_SNMP;
		dev->driver = strdup("snmp-ups");
		dev->port = strdup(session->peername);
		add_option_to_device(dev,"mibs",snmp_device_table[index].mib);

		snmp_free_pdu(response);
		response = NULL;

		return dev;

	}

	return NULL;
}

device_t * scan_snmp(char * start_ip, char * stop_ip,long usec_timeout)
{
	int addr;
	struct in_addr current_addr;
	struct in_addr stop_addr;
	struct snmp_session snmp_sess, *snmp_sess_p;
	struct snmp_pdu *pdu, *response = NULL;
        oid name[MAX_OID_LEN];
        size_t name_len = MAX_OID_LEN;
	device_t * dev = NULL;
	device_t * ret_dev = NULL;

	if( start_ip == NULL ) {
		return NULL;
	}

	if( stop_ip == NULL ) {
		stop_ip = start_ip;
	}

	if(!inet_aton(start_ip, &current_addr)) {
		fprintf(stderr,"Invalid address : %s\n",start_ip);
		return NULL;
	}
	if(!inet_aton(stop_ip, &stop_addr)) {
		fprintf(stderr,"Invalid address : %s\n",stop_ip);
		return NULL;
	}

	/* Make sure current addr is lesser than stop addr */
	if( ntohl(current_addr.s_addr) > ntohl(stop_addr.s_addr) ) {
		addr = current_addr.s_addr;
		current_addr.s_addr = stop_addr.s_addr;
		stop_addr.s_addr = addr;
	}

	while(1) {

		/* Initialize the SNMP library */
		init_snmp("nut-scanner");

		/* Initialize session */
		snmp_sess_init(&snmp_sess);

		snmp_sess.peername = strdup(inet_ntoa(current_addr));

		snmp_sess.version = SNMP_VERSION_1;
		snmp_sess.community = (unsigned char *)"public";
		snmp_sess.community_len = strlen("public");

		snmp_sess.retries = 0;
		snmp_sess.timeout = usec_timeout;
		
		/* Open the session */
		snmp_sess_p = snmp_open(&snmp_sess); /* establish the session */
		if (snmp_sess_p == NULL) {
			fprintf(stderr,"Failed to open SNMP session for %s.\n",snmp_sess.peername);
			free(snmp_sess.peername);
			continue;
		}

		/* create and send request. */
		if (!snmp_parse_oid(SysOID, name, &name_len)) {
			fprintf(stderr,"SNMP errors: %s\n",
					snmp_api_errstring(snmp_errno));
			snmp_close(snmp_sess_p);
			free(snmp_sess.peername);
			continue;
		}

		pdu = snmp_pdu_create(SNMP_MSG_GET);

		if (pdu == NULL) {
			fprintf(stderr,"Not enough memory\n");
			exit(-1);
		}

		snmp_add_null_var(pdu, name, name_len);

		snmp_synch_response(snmp_sess_p,
				pdu, &response);

		if (response) {
			/* SNMP device found */
			/* SysOID is supposed to give the required MIB. */
			/* FIXME: until we are sure it gives the MIB for all devices, use the old method: */
			/* try a list of known OID */
			ret_dev = try_all_oid(snmp_sess_p);
			dev = add_device_to_device(dev,ret_dev);

			snmp_free_pdu(response);
			response = NULL;
		}

		snmp_close(snmp_sess_p);
		free(snmp_sess.peername);
		
		/* Check if this is the last address to scan */
		if(current_addr.s_addr == stop_addr.s_addr) {
			break;
		}
		/* increment the address (need to pass address in host byte
		   order, then pass back in network byte order */
		current_addr.s_addr = htonl((ntohl(current_addr.s_addr)+1));

	};

	return dev;
}
#endif /* HAVE_NET_SNMP_NET_SNMP_CONFIG_H */


