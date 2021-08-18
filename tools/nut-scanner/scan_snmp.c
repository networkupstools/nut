/*
 *  Copyright (C) 2011-2017 Eaton
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

/*! \file scan_snmp.c
    \brief detect NUT supported SNMP devices
    \author Frederic Bohe <FredericBohe@Eaton.com>
    \author Arnaud Quette <ArnaudQuette@Eaton.com>
*/

#include "common.h"
#include "nut-scan.h"

#ifdef WITH_SNMP

#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <ltdl.h>

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

/* Address API change */
#ifndef usmAESPrivProtocol
#define USMAESPRIVPROTOCOL "usmAES128PrivProtocol"
#else
#define USMAESPRIVPROTOCOL "usmAESPrivProtocol"
#endif

#define SysOID ".1.3.6.1.2.1.1.2.0"

static nutscan_device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif
long g_usec_timeout ;

/* dynamic link library stuff */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;

static void (*nut_init_snmp)(const char *type);
static void (*nut_snmp_sess_init)(netsnmp_session * session);
static void * (*nut_snmp_sess_open)(struct snmp_session *session);
static int (*nut_snmp_sess_close)(void *handle);
static struct snmp_session * (*nut_snmp_sess_session)(void *handle); 
static void * (*nut_snmp_parse_oid)(const char *input, oid *objid,
		size_t *objidlen);
static struct snmp_pdu * (*nut_snmp_pdu_create) (int command );
netsnmp_variable_list * (*nut_snmp_add_null_var)(netsnmp_pdu *pdu,
			const oid *objid, size_t objidlen);
static int (*nut_snmp_sess_synch_response) (void *sessp, netsnmp_pdu *pdu,
			netsnmp_pdu **response);
static int (*nut_snmp_oid_compare) (const oid *in_name1, size_t len1,
			const oid *in_name2, size_t len2);
static void (*nut_snmp_free_pdu) (netsnmp_pdu *pdu);
static int (*nut_generate_Ku)(const oid * hashtype, u_int hashtype_len,
			u_char * P, size_t pplen, u_char * Ku, size_t * kulen);
static char* (*nut_snmp_out_toggle_options)(char *options);
static const char * (*nut_snmp_api_errstring) (int snmp_errnumber);
static int (*nut_snmp_errno);
static oid (*nut_usmAESPrivProtocol);
static oid (*nut_usmHMACMD5AuthProtocol);
static oid (*nut_usmHMACSHA1AuthProtocol);
static oid (*nut_usmDESPrivProtocol);

/* return 0 on error */
int nutscan_load_snmp_library(const char *libname_path)
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
		upsdebugx(1, "SNMP library not found. SNMP search disabled");
		return 0;
	}

	if( lt_dlinit() != 0 ) {
		upsdebugx(1, "Error initializing lt_init");
		return 0;
	}

	dl_handle = lt_dlopen(libname_path);
	if (!dl_handle) {
		dl_error = lt_dlerror();
		goto err;
	}

	lt_dlerror();	/* Clear any existing error */
	*(void **) (&nut_init_snmp) = lt_dlsym(dl_handle, "init_snmp");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_sess_init) = lt_dlsym(dl_handle,
							"snmp_sess_init");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_sess_open) = lt_dlsym(dl_handle,
							"snmp_sess_open");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_sess_close) = lt_dlsym(dl_handle,
							"snmp_sess_close");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_sess_session) = lt_dlsym(dl_handle,
							"snmp_sess_session");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_parse_oid) = lt_dlsym(dl_handle,
							"snmp_parse_oid");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_pdu_create) = lt_dlsym(dl_handle,
							"snmp_pdu_create");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_add_null_var) = lt_dlsym(dl_handle,
							"snmp_add_null_var");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_sess_synch_response) = lt_dlsym(dl_handle,
						"snmp_sess_synch_response");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_oid_compare) = lt_dlsym(dl_handle,
							"snmp_oid_compare");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_free_pdu) = lt_dlsym(dl_handle,"snmp_free_pdu");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_generate_Ku) = lt_dlsym(dl_handle, "generate_Ku");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_out_toggle_options) = lt_dlsym(dl_handle,
							"snmp_out_toggle_options");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_api_errstring) = lt_dlsym(dl_handle,
							"snmp_api_errstring");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_snmp_errno) = lt_dlsym(dl_handle, "snmp_errno");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_usmAESPrivProtocol) = lt_dlsym(dl_handle,
							USMAESPRIVPROTOCOL);
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_usmHMACMD5AuthProtocol) = lt_dlsym(dl_handle,
						"usmHMACMD5AuthProtocol");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_usmHMACSHA1AuthProtocol) = lt_dlsym(dl_handle,
						"usmHMACSHA1AuthProtocol");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_usmDESPrivProtocol) = lt_dlsym(dl_handle,
						"usmDESPrivProtocol");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	return 1;
err:
	fprintf(stderr, "Cannot load SNMP library (%s) : %s. SNMP search disabled.\n", libname_path, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}
/* end of dynamic link library stuff */

static void scan_snmp_add_device(nutscan_snmp_t * sec, struct snmp_pdu *response,char * mib)
{
	nutscan_device_t * dev = NULL;
	struct snmp_session * session;
	char * buf;

	session = (*nut_snmp_sess_session)(sec->handle);
	if(session == NULL) {
		return;
	}
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
		nutscan_add_option_to_device(dev,"snmp_version","v3");

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
	if (!(*nut_snmp_parse_oid)(oid_str, name, &name_len)) {
		index++;
		return NULL;
	}

	pdu = (*nut_snmp_pdu_create)(SNMP_MSG_GET);

	if (pdu == NULL) {
		index++;
		return NULL;
	}

	(*nut_snmp_add_null_var)(pdu, name, name_len);

	status = (*nut_snmp_sess_synch_response)(handle,pdu, &response);
	if( response == NULL ) {
		index++;
		return NULL;
	}

	if(status!=STAT_SUCCESS||response->errstat!=SNMP_ERR_NOERROR||
			response->variables == NULL ||
			response->variables->name == NULL ||
			(*nut_snmp_oid_compare)(response->variables->name,
				response->variables->name_length,
				name, name_len) != 0 || 
			response->variables->val.string == NULL ) {
		(*nut_snmp_free_pdu)(response);
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

		(*nut_snmp_free_pdu)(response);
		response = NULL;

		index++;
	}
}

static int init_session(struct snmp_session * snmp_sess, nutscan_snmp_t * sec)
{
	(*nut_snmp_sess_init)(snmp_sess);

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
		snmp_sess->securityAuthProto = nut_usmHMACMD5AuthProtocol;
		snmp_sess->securityAuthProtoLen =
				sizeof(usmHMACMD5AuthProtocol)/
				sizeof(oid);

		if( sec->authProtocol ) {
			if (strcmp(sec->authProtocol, "SHA") == 0) {
				snmp_sess->securityAuthProto = nut_usmHMACSHA1AuthProtocol;
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
		if ((*nut_generate_Ku)(snmp_sess->securityAuthProto,
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
		snmp_sess->securityPrivProto = nut_usmDESPrivProtocol;
		snmp_sess->securityPrivProtoLen =
			sizeof(usmDESPrivProtocol)/sizeof(oid);

		if( sec->privProtocol ) {
			if (strcmp(sec->privProtocol, "AES") == 0) {
				snmp_sess->securityPrivProto = nut_usmAESPrivProtocol;
				snmp_sess->securityPrivProtoLen = 
					sizeof(usmAESPrivProtocol)/
					sizeof(oid);
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
		if ((*nut_generate_Ku)(snmp_sess->securityAuthProto,
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
	struct snmp_pdu *pdu, *response = NULL, *resp = NULL;
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
	handle = (*nut_snmp_sess_open)(&snmp_sess); /* establish the session */
	if (handle == NULL) {
		fprintf(stderr,"Failed to open SNMP session for %s.\n",
			sec->peername);
		goto try_SysOID_free;
	}

	/* create and send request. */
	if (!(*nut_snmp_parse_oid)(SysOID, name, &name_len)) {
		fprintf(stderr,"SNMP errors: %s\n",
				(*nut_snmp_api_errstring)((*nut_snmp_errno)));
		(*nut_snmp_sess_close)(handle);
		goto try_SysOID_free;
	}

	pdu = (*nut_snmp_pdu_create)(SNMP_MSG_GET);

	if (pdu == NULL) {
		fprintf(stderr,"Not enough memory\n");
		(*nut_snmp_sess_close)(handle);
		goto try_SysOID_free;
	}

	(*nut_snmp_add_null_var)(pdu, name, name_len);

	(*nut_snmp_sess_synch_response)(handle,
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
				if (!(*nut_snmp_parse_oid)(
					snmp_device_table[index].sysoid,
					name, &name_len)) {
					index++;
					continue;
				}

				if ( (*nut_snmp_oid_compare)(
					response->variables->val.objid,
					response->variables->val_len/sizeof(oid),
					name, name_len) == 0 ) {
					/* we have found a relevent sysoid */
					resp = scan_snmp_get_manufacturer(
						snmp_device_table[index].oid,
						handle);
					if( resp != NULL ) {
						scan_snmp_add_device(sec,resp,
							snmp_device_table[index].mib);
						sysoid_found = 1;
						(*nut_snmp_free_pdu)(resp);
					}
				}
				index++;
			}
		}

		/* try a list of known OID */
		if( !sysoid_found ) {
			try_all_oid(sec);
		}

		(*nut_snmp_free_pdu)(response);
		response = NULL;
	}

	(*nut_snmp_sess_close)(handle);

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
	pthread_t * thread_array = NULL;
	int thread_count = 0;

	pthread_mutex_init(&dev_mutex,NULL);
#endif

	if( !nutscan_avail_snmp ) {
		return NULL;
	}

	g_usec_timeout = usec_timeout;

	/* Force numeric OIDs resolution (ie, do not resolve to textual names)
	 * This is mostly for the convenience of debug output */
	if (nut_snmp_out_toggle_options("n") != NULL) {
		upsdebugx(1, "Failed to enable numeric OIDs resolution");
	}

	/* Initialize the SNMP library */
	(*nut_init_snmp)("nut-scanner");

	ip_str = nutscan_ip_iter_init(&ip, start_ip, stop_ip);

	while(ip_str != NULL) {
		tmp_sec = malloc(sizeof(nutscan_snmp_t));
		memcpy(tmp_sec, sec, sizeof(nutscan_snmp_t));
		tmp_sec->peername = ip_str;

#ifdef HAVE_PTHREAD
		if (pthread_create(&thread,NULL,try_SysOID,(void*)tmp_sec)==0){
			thread_count++;
			pthread_t *new_thread_array = realloc(thread_array,
						thread_count*sizeof(pthread_t));
			if (new_thread_array == NULL) {
				upsdebugx(1, "%s: Failed to realloc thread", __func__);
				break;
			}
			else {
				thread_array = new_thread_array;
			}
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
	nutscan_device_t * result = nutscan_rewind_device(dev_ret);
	dev_ret = NULL;
	return result;
}
#else /* WITH_SNMP */
nutscan_device_t * nutscan_scan_snmp(const char * start_ip, const char * stop_ip,long usec_timeout, nutscan_snmp_t * sec)
{
	return NULL;
}
#endif /* WITH_SNMP */


