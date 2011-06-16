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
#include "nut-scan.h"
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

void try_all_oid(void * arg)
{
        oid name[MAX_OID_LEN];
        size_t name_len = MAX_OID_LEN;
	struct snmp_pdu *pdu, *response = NULL;
	int index = 0;
	device_t * dev = NULL;
	struct snmp_session * session;
	snmp_security_t * sec = (snmp_security_t *)arg;
	int status;
	char buf[SMALLBUF];

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

		status = snmp_sess_synch_response(sec->handle,pdu, &response);
		if( response == NULL ) {
			index++;
			continue;
		}

		if(status!=STAT_SUCCESS||response->errstat!=SNMP_ERR_NOERROR||
			response->variables == NULL ||
			response->variables->name == NULL ||
			snmp_oid_compare(response->variables->name,
				response->variables->name_length,
				name, name_len) != 0 || 
			response->variables->val.string == NULL ) {
			snmp_free_pdu(response);
			response = NULL;
			index++;
			continue;
		}

		session = snmp_sess_session(sec->handle);
		/* SNMP device found */
		dev = new_device();
		dev->type = TYPE_SNMP;
		dev->driver = strdup("snmp-ups");
		dev->port = strdup(session->peername);
		snprintf(buf,sizeof(buf),"\"%s\"",
				 response->variables->val.string);
		add_option_to_device(dev,"desc",buf);
		add_option_to_device(dev,"mibs",snmp_device_table[index].mib);
		/* SNMP v3 */
		if( session->community == NULL || session->community[0] == 0) {
			if( sec->secLevel ) {
				add_option_to_device(dev,"secLevel",
							sec->secLevel);
			}
			if( sec->secName ) {
				add_option_to_device(dev,"secName",
							sec->secName);
			}
			if( sec->authPassword ) {
				add_option_to_device(dev,"authPassword",
							sec->authPassword);
			}
			if( sec->privPassword ) {
				add_option_to_device(dev,"privPassword",
							sec->privPassword);
			}
			if( sec->authProtocol ) {
				add_option_to_device(dev,"authProtocol",
							sec->authProtocol);
			}
			if( sec->privProtocol ) {
				add_option_to_device(dev,"privProtocol",
							sec->privProtocol);
			}
		}
		else {
			add_option_to_device(dev,"community",
						(char *)session->community);
		}

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

int init_session(struct snmp_session * snmp_sess, snmp_security_t * sec)
{
	snmp_sess_init(snmp_sess);

	snmp_sess->peername = sec->peername;

	if( sec->community != NULL || sec->secLevel == NULL ) {
		snmp_sess->version = SNMP_VERSION_1;
		if( sec->community != NULL ) {
			snmp_sess->community = (unsigned char *)sec->community;
			snmp_sess->community_len = strlen(sec->community);
		}
		else {
			snmp_sess->community = (unsigned char *)"public";
			snmp_sess->community_len = strlen("public");
		}
	}
	else { /* SNMP v3 */ 
		snmp_sess->version = SNMP_VERSION_3;

		/* Security level */
		if (strcmp(sec->secLevel, "noAuthNoPriv") == 0)
			snmp_sess->securityLevel = SNMP_SEC_LEVEL_NOAUTH;
		else if (strcmp(sec->secLevel, "authNoPriv") == 0)
			snmp_sess->securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
		else if (strcmp(sec->secLevel, "authPriv") == 0)
			snmp_sess->securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
		else {
			fprintf(stderr,"Bad SNMPv3 securityLevel: %s\n",
								sec->secLevel);
			return 0;
		}

		/* Security name */
		if( sec->secName == NULL ) {
			fprintf(stderr,"securityName is required for SNMPv3\n");
			return 0;
		}
		snmp_sess->securityName = strdup(sec->secName);
		snmp_sess->securityNameLen = strlen(snmp_sess->securityName);

		/* Everything is ready for NOAUTH */
		if( snmp_sess->securityLevel == SNMP_SEC_LEVEL_NOAUTH ) {
			return 1;
		}

		/* Process mandatory fields, based on the security level */
                switch (snmp_sess->securityLevel) {
                        case SNMP_SEC_LEVEL_AUTHNOPRIV:
                                if (sec->authPassword == NULL) {
                                        fprintf(stderr,
			"authPassword is required for SNMPv3 in %s mode\n",
						sec->secLevel);
					return 0;
				}
				break;
                        case SNMP_SEC_LEVEL_AUTHPRIV:
                                if ((sec->authPassword == NULL) ||
					(sec->privPassword == NULL)) {
                                        fprintf(stderr,
	"authPassword and privPassword are required for SNMPv3 in %s mode\n",
						sec->secLevel);
					return 0;
				}
				break;
                        default:
                                /* nothing else needed */
                                break;
                }

                /* Process authentication protocol and key */
               	snmp_sess->securityAuthKeyLen = USM_AUTH_KU_LEN;

		/* default to MD5 */
		snmp_sess->securityAuthProto = usmHMACMD5AuthProtocol;
		snmp_sess->securityAuthProtoLen =sizeof(usmHMACMD5AuthProtocol)/
							sizeof(oid);

		if( sec->authProtocol ) {
			if (strcmp(sec->authProtocol, "SHA") == 0) {
				snmp_sess->securityAuthProto =
							usmHMACSHA1AuthProtocol;
				snmp_sess->securityAuthProtoLen =
					sizeof(usmHMACSHA1AuthProtocol)/
					sizeof(oid);
			}
			else {
				if (strcmp(sec->authProtocol, "MD5") != 0) {
					fprintf(stderr,
						"Bad SNMPv3 authProtocol: %s",
						sec->authProtocol);
					return 0;
				}
			}
		}

		/* set the authentication key to a MD5/SHA1 hashed version of
		 * our passphrase (must be at least 8 characters long) */
		if (generate_Ku(snmp_sess->securityAuthProto,
					snmp_sess->securityAuthProtoLen,
					(u_char *) sec->authPassword,
					strlen(sec->authPassword),
					snmp_sess->securityAuthKey,
					&snmp_sess->securityAuthKeyLen)
					!= SNMPERR_SUCCESS) {
							fprintf(stderr,
		"Error generating Ku from authentication pass phrase\n");
							return 0;
		}

		/* Everything is ready for AUTHNOPRIV */
		if( snmp_sess->securityLevel == SNMP_SEC_LEVEL_AUTHNOPRIV ) {
			return 1;
		}

		/* default to DES */
		snmp_sess->securityPrivProto=usmDESPrivProtocol;
		snmp_sess->securityPrivProtoLen =
			sizeof(usmDESPrivProtocol)/sizeof(oid);

		if( sec->privProtocol ) {
			if (strcmp(sec->privProtocol, "AES") == 0) {
				snmp_sess->securityPrivProto=usmAESPrivProtocol;
				snmp_sess->securityPrivProtoLen = 
					sizeof(usmAESPrivProtocol)/sizeof(oid);
			}
			else {
				if (strcmp(sec->privProtocol, "DES") != 0) {
					fprintf(stderr,
						"Bad SNMPv3 authProtocol: %s\n"
						,sec->authProtocol);
				return 0;
				}
			}
		}

		/* set the private key to a MD5/SHA hashed version of
		 * our passphrase (must be at least 8 characters long) */
		snmp_sess->securityPrivKeyLen = USM_PRIV_KU_LEN;
		if (generate_Ku(snmp_sess->securityAuthProto,
					snmp_sess->securityAuthProtoLen,
					(u_char *) sec->privPassword,
					strlen(sec->privPassword),
					snmp_sess->securityPrivKey,
					&snmp_sess->securityPrivKeyLen)
					!= SNMPERR_SUCCESS) {
							fprintf(stderr,
		"Error generating Ku from private pass phrase\n");
							return 0;
		}

	}

	return 1;
}

void * try_SysOID(void * arg)
{
	struct snmp_session snmp_sess;
	void * handle;
	struct snmp_pdu *pdu, *response = NULL;
        oid name[MAX_OID_LEN];
        size_t name_len = MAX_OID_LEN;
	snmp_security_t * sec = (snmp_security_t *)arg;

	/* Initialize session */
	if( !init_session(&snmp_sess,sec) ) {
		goto try_SysOID_free;
	}
	
	snmp_sess.retries = 0;
	snmp_sess.timeout = g_usec_timeout;

	/* Open the session */
	handle = snmp_sess_open(&snmp_sess); /* establish the session */
	if (handle == NULL) {
		fprintf(stderr,"Failed to open SNMP session for %s.\n",
			sec->peername);
		goto try_SysOID_free;
	}

	/* create and send request. */
	if (!snmp_parse_oid(SysOID, name, &name_len)) {
		fprintf(stderr,"SNMP errors: %s\n",
				snmp_api_errstring(snmp_errno));
		snmp_sess_close(handle);
		goto try_SysOID_free;
	}

	pdu = snmp_pdu_create(SNMP_MSG_GET);

	if (pdu == NULL) {
		fprintf(stderr,"Not enough memory\n");
		snmp_sess_close(handle);
		goto try_SysOID_free;
	}

	snmp_add_null_var(pdu, name, name_len);

	snmp_sess_synch_response(handle,
			pdu, &response);

	if (response) {
		/* SNMP device found */
		/* SysOID is supposed to give the required MIB. */
		/* FIXME: until we are sure it gives the MIB for all devices, use the old method: */
		/* try a list of known OID */
		sec->handle = handle;
		try_all_oid(sec);

		snmp_free_pdu(response);
		response = NULL;
	}

	snmp_sess_close(handle);

try_SysOID_free:
	if( sec->peername ) {
		free(sec->peername);
	}
	free(sec);

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

device_t * scan_snmp(char * start_ip, char * stop_ip,long usec_timeout, snmp_security_t * sec)
{
	int addr;
	struct in_addr current_addr;
	struct in_addr stop_addr;
	struct in6_addr current_addr6;
	struct in6_addr stop_addr6;
	enum network_type type = IPv4;
	char buf[SMALLBUF];
	int i;
	snmp_security_t * tmp_sec;
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
		tmp_sec = malloc(sizeof(snmp_security_t));
		memcpy(tmp_sec, sec, sizeof(snmp_security_t));
		if( type == IPv4 ) {
			tmp_sec->peername = strdup(inet_ntoa(current_addr));
		}
		else { /* IPv6 */
			tmp_sec->peername = strdup(inet_ntop(AF_INET6,&current_addr6,buf,
							sizeof(buf)));
		}
#ifdef HAVE_PTHREAD
		if (pthread_create(&thread,NULL,try_SysOID,(void*)tmp_sec)==0){
			thread_count++;
			thread_array = realloc(thread_array,
						thread_count*sizeof(pthread_t));
			thread_array[thread_count-1] = thread;
		}
#else
		try_SysOID((void *)tmp_sec);
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
	free(thread_array);
#endif

	return dev_ret;
}
#endif /* HAVE_NET_SNMP_NET_SNMP_CONFIG_H */


