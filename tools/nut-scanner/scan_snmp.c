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

#include "common.h"

#ifdef HAVE_NET_SNMP_NET_SNMP_CONFIG_H

#include "nut-scan.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

/* workaround for buggy Net-SNMP config
 * from drivers/snmp-ups.h */
#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif

#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif

#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif

#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif
#include "nutscan-snmp.h"


#define SysOID ".1.3.6.1.2.1.1.2.0"

static nutscan_device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
static pthread_t * thread_array = NULL;
static int thread_count = 0;
#endif
long g_usec_timeout ;

static void scan_snmp_add_device(nutscan_snmp_t * sec, struct snmp_pdu *response,char * mib)
{
	nutscan_device_t * dev = NULL;
	struct snmp_session * session;
	char * buf;

	session = snmp_sess_session(sec->handle);
	/* SNMP device found */
	dev = nutscan_new_device();
	dev->type = TYPE_SNMP;
	dev->driver = strdup("snmp-ups");
	dev->port = strdup(session->peername);
	buf = malloc( response->variables->val_len + 1 );
	if( buf ) {
		memcpy(buf,response->variables->val.string,
			response->variables->val_len);
		buf[response->variables->val_len]=0;
		nutscan_add_option_to_device(dev,"desc",buf);
		free(buf);
	}
	nutscan_add_option_to_device(dev,"mibs",mib);
	/* SNMP v3 */
	if( session->community == NULL || session->community[0] == 0) {
		if( sec->secLevel ) {
			nutscan_add_option_to_device(dev,"secLevel",
					sec->secLevel);
		}
		if( sec->secName ) {
			nutscan_add_option_to_device(dev,"secName",
					sec->secName);
		}
		if( sec->authPassword ) {
			nutscan_add_option_to_device(dev,"authPassword",
					sec->authPassword);
		}
		if( sec->privPassword ) {
			nutscan_add_option_to_device(dev,"privPassword",
					sec->privPassword);
		}
		if( sec->authProtocol ) {
			nutscan_add_option_to_device(dev,"authProtocol",
					sec->authProtocol);
		}
		if( sec->privProtocol ) {
			nutscan_add_option_to_device(dev,"privProtocol",
					sec->privProtocol);
		}
	}
	else {
		buf = malloc( session->community_len + 1 );
		if( buf ) {
			memcpy(buf,session->community,
				session->community_len);
			buf[session->community_len]=0;
			nutscan_add_option_to_device(dev,"community",buf);
			free(buf);
		}
	}

#ifdef HAVE_PTHREAD
	pthread_mutex_lock(&dev_mutex);
#endif
	dev_ret = nutscan_add_device_to_device(dev_ret,dev);
#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&dev_mutex);
#endif

}

static struct snmp_pdu * scan_snmp_get_manufacturer(char* oid_str,void* handle)
{
        size_t name_len;
        oid name[MAX_OID_LEN];
	struct snmp_pdu *pdu, *response = NULL;
	int status;
	int index = 0;

	/* create and send request. */
	name_len = MAX_OID_LEN;
	if (!snmp_parse_oid(oid_str, name, &name_len)) {
		index++;
		return NULL;
	}

	pdu = snmp_pdu_create(SNMP_MSG_GET);

	if (pdu == NULL) {
		index++;
		return NULL;
	}

	snmp_add_null_var(pdu, name, name_len);

	status = snmp_sess_synch_response(handle,pdu, &response);
	if( response == NULL ) {
		index++;
		return NULL;
	}

	if(status!=STAT_SUCCESS||response->errstat!=SNMP_ERR_NOERROR||
			response->variables == NULL ||
			response->variables->name == NULL ||
			snmp_oid_compare(response->variables->name,
				response->variables->name_length,
				name, name_len) != 0 || 
			response->variables->val.string == NULL ) {
		snmp_free_pdu(response);
		index++;
		return NULL;
	}

	return response;
}

static void try_all_oid(void * arg)
{
	struct snmp_pdu *response = NULL;
	int index = 0;
	nutscan_snmp_t * sec = (nutscan_snmp_t *)arg;

	while(snmp_device_table[index].oid != NULL) {

		response = scan_snmp_get_manufacturer(snmp_device_table[index].oid,sec->handle);
		if( response == NULL ) {
			index++;
			continue;
		}

		scan_snmp_add_device(sec,response,snmp_device_table[index].mib);

		snmp_free_pdu(response);
		response = NULL;

		index++;
	}
}

static int init_session(struct snmp_session * snmp_sess, nutscan_snmp_t * sec)
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

static void * try_SysOID(void * arg)
{
	struct snmp_session snmp_sess;
	void * handle;
	struct snmp_pdu *pdu, *response = NULL;
        oid name[MAX_OID_LEN];
        size_t name_len = MAX_OID_LEN;
	nutscan_snmp_t * sec = (nutscan_snmp_t *)arg;
	int index = 0;
	int sysoid_found = 0;

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
		sec->handle = handle;

		/* SNMP device found */
		/* SysOID is supposed to give the required MIB. */

		/* Check if the received OID match with a known sysOID */
		if(response->variables != NULL &&
				response->variables->val.objid != NULL){
			while(snmp_device_table[index].oid != NULL) {
				if(snmp_device_table[index].sysoid == NULL ) {
					index++;
					continue;
				}
				name_len = MAX_OID_LEN;
				if (!snmp_parse_oid(
					snmp_device_table[index].sysoid,
					name, &name_len)) {
					index++;
					continue;
				}

				if ( snmp_oid_compare(
					response->variables->val.objid,
					response->variables->val_len/sizeof(oid),
					name, name_len) == 0 ) {
					/* we have found a relevent sysoid */
					snmp_free_pdu(response);
					response = scan_snmp_get_manufacturer(
						snmp_device_table[index].oid,
						handle);
					scan_snmp_add_device(sec,response,
						snmp_device_table[index].mib);
					sysoid_found = 1;
				}
				index++;
			}
		}

		/* try a list of known OID */
		if( !sysoid_found ) {
			try_all_oid(sec);
		}

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

nutscan_device_t * nutscan_scan_snmp(const char * start_ip, const char * stop_ip,long usec_timeout, nutscan_snmp_t * sec)
{
	int i;
	nutscan_snmp_t * tmp_sec;
	nutscan_ip_iter_t ip;
	char * ip_str = NULL;
#ifdef HAVE_PTHREAD
	pthread_t thread;

	pthread_mutex_init(&dev_mutex,NULL);
#endif

	g_usec_timeout = usec_timeout;

	/* Initialize the SNMP library */
	init_snmp("nut-scanner");

	ip_str = nutscan_ip_iter_init(&ip, start_ip, stop_ip);

	while(ip_str != NULL) {
		tmp_sec = malloc(sizeof(nutscan_snmp_t));
		memcpy(tmp_sec, sec, sizeof(nutscan_snmp_t));
		tmp_sec->peername = ip_str;

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
		ip_str = nutscan_ip_iter_inc(&ip);
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


