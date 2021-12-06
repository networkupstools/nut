/*
 *  Copyright (C) 2011 - EATON
 *  Copyright (C) 2016-2021 - EATON - Various threads-related improvements
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
    \author Jim Klimov <EvgenyKlimov@eaton.com>
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
#include "nutscan-snmp.h"

/* Address API change */
#if ( ! NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol ) && ( ! defined usmAESPrivProtocol )
#define USMAESPRIVPROTOCOL "usmAES128PrivProtocol"
#else
#define USMAESPRIVPROTOCOL "usmAESPrivProtocol"
#endif

#define SysOID ".1.3.6.1.2.1.1.2.0"

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE = 0, TRUE } bool_t;
#else
typedef int bool_t;
#endif

static nutscan_device_t * dev_ret = NULL;
#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif
static useconds_t g_usec_timeout ;

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
static struct snmp_pdu * (*nut_snmp_pdu_create) (int command);
static netsnmp_variable_list * (*nut_snmp_add_null_var)(netsnmp_pdu *pdu,
			const oid *objid, size_t objidlen);
static int (*nut_snmp_sess_synch_response) (void *sessp, netsnmp_pdu *pdu,
			netsnmp_pdu **response);
static int (*nut_snmp_oid_compare) (const oid *in_name1, size_t len1,
			const oid *in_name2, size_t len2);
static void (*nut_snmp_free_pdu) (netsnmp_pdu *pdu);

/* NOTE: Net-SNMP headers just are weird like that, in the same release:
net-snmp/types.h:              size_t securityAuthProtoLen;
net-snmp/library/keytools.h:   int    generate_Ku(const oid * hashtype, u_int hashtype_len, ...
 * Should we match in configure like for "getnameinfo()" arg types?
 * Currently we cast one to another below (detecting target type could help).
 */
static int (*nut_generate_Ku)(const oid * hashtype, u_int hashtype_len,
			unsigned char * P, size_t pplen, unsigned char * Ku, size_t * kulen);
static char* (*nut_snmp_out_toggle_options)(char *options);
static const char * (*nut_snmp_api_errstring) (int snmp_errnumber);

/* Variables (not methods) exported by libnet-snmp: */
static int *nut_snmp_errno;
#if NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol || NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol
static oid *nut_usmAESPrivProtocol; /* might be usmAES128PrivProtocol on some systems */
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol
static oid *nut_usmHMACMD5AuthProtocol;
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol
static oid *nut_usmHMACSHA1AuthProtocol;
#endif
#if NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol
static oid *nut_usmDESPrivProtocol;
#endif
#if NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04
# if NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol
static oid *nut_usmAES192PrivProtocol;
# endif
# if NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol
static oid *nut_usmAES256PrivProtocol;
# endif
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol
static oid *nut_usmHMAC192SHA256AuthProtocol;
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol
static oid *nut_usmHMAC256SHA384AuthProtocol;
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol
static oid *nut_usmHMAC384SHA512AuthProtocol;
#endif

/* return 0 on error; visible externally */
int nutscan_load_snmp_library(const char *libname_path);

int nutscan_load_snmp_library(const char *libname_path)
{
	if (dl_handle != NULL) {
		/* if previous init failed */
		if (dl_handle == (void *)1) {
			return 0;
		}
		/* init has already been done */
		return 1;
	}

	if (libname_path == NULL) {
		upsdebugx(1, "SNMP library not found. SNMP search disabled");
		return 0;
	}

	if (lt_dlinit() != 0) {
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
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_sess_init) = lt_dlsym(dl_handle,
							"snmp_sess_init");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_sess_open) = lt_dlsym(dl_handle,
							"snmp_sess_open");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_sess_close) = lt_dlsym(dl_handle,
							"snmp_sess_close");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_sess_session) = lt_dlsym(dl_handle,
							"snmp_sess_session");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_parse_oid) = lt_dlsym(dl_handle,
							"snmp_parse_oid");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_pdu_create) = lt_dlsym(dl_handle,
							"snmp_pdu_create");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_add_null_var) = lt_dlsym(dl_handle,
							"snmp_add_null_var");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_sess_synch_response) = lt_dlsym(dl_handle,
						"snmp_sess_synch_response");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_oid_compare) = lt_dlsym(dl_handle,
							"snmp_oid_compare");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_free_pdu) = lt_dlsym(dl_handle, "snmp_free_pdu");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_generate_Ku) = lt_dlsym(dl_handle, "generate_Ku");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_out_toggle_options) = lt_dlsym(dl_handle,
							"snmp_out_toggle_options");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_api_errstring) = lt_dlsym(dl_handle,
							"snmp_api_errstring");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

	*(void **) (&nut_snmp_errno) = lt_dlsym(dl_handle, "snmp_errno");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}

#if NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol || NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol
	*(void **) (&nut_usmAESPrivProtocol) = lt_dlsym(dl_handle,
							USMAESPRIVPROTOCOL);
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
#endif /* NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol || NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol */

#if NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol
	*(void **) (&nut_usmHMACMD5AuthProtocol) = lt_dlsym(dl_handle,
						"usmHMACMD5AuthProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
#endif /* NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol */

#if NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol
	*(void **) (&nut_usmHMACSHA1AuthProtocol) = lt_dlsym(dl_handle,
						"usmHMACSHA1AuthProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
#endif /* NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol */

#if NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol
	*(void **) (&nut_usmDESPrivProtocol) = lt_dlsym(dl_handle,
						"usmDESPrivProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
#endif /* NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol */

#if NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04
# if NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol
	*(void **) (&nut_usmAES192PrivProtocol) = lt_dlsym(dl_handle,
						"usmAES192PrivProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
# endif /* NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol */

# if NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol
	*(void **) (&nut_usmAES256PrivProtocol) = lt_dlsym(dl_handle,
						"usmAES256PrivProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
# endif /* NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol */
#endif /* NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04 */

#if NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol
	*(void **) (&nut_usmHMAC192SHA256AuthProtocol) = lt_dlsym(dl_handle,
						"usmHMAC192SHA256AuthProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
#endif /* NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol */

#if NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol
	*(void **) (&nut_usmHMAC256SHA384AuthProtocol) = lt_dlsym(dl_handle,
						"usmHMAC256SHA384AuthProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
#endif /* NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol */

#if NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol
	*(void **) (&nut_usmHMAC384SHA512AuthProtocol) = lt_dlsym(dl_handle,
						"usmHMAC384SHA512AuthProtocol");
	if ((dl_error = lt_dlerror()) != NULL) {
		goto err;
	}
#endif /* NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol */

	return 1;
err:
	fprintf(stderr, "Cannot load SNMP library (%s) : %s. SNMP search disabled.\n",
		libname_path, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}
/* end of dynamic link library stuff */

static void scan_snmp_add_device(nutscan_snmp_t * sec, struct snmp_pdu *response, char * mib)
{
	nutscan_device_t * dev = NULL;
	struct snmp_session * session;
	char * buf;

	session = (*nut_snmp_sess_session)(sec->handle);
	if (session == NULL) {
		return;
	}
	/* SNMP device found */
	dev = nutscan_new_device();
	dev->type = TYPE_SNMP;
	dev->driver = strdup("snmp-ups");
	dev->port = strdup(session->peername);
	if (response != NULL) {
		buf = malloc (response->variables->val_len + 1);
		if (buf) {
			memcpy(buf, response->variables->val.string,
				response->variables->val_len);
			buf[response->variables->val_len] = 0;
			nutscan_add_option_to_device(dev, "desc", buf);
			free(buf);
		}
	}
	nutscan_add_option_to_device(dev, "mibs", mib);
	/* SNMP v3 */
	if (session->community == NULL || session->community[0] == 0) {
		nutscan_add_option_to_device(dev, "snmp_version", "v3");

		if (sec->secLevel) {
			nutscan_add_option_to_device(dev, "secLevel",
				sec->secLevel);
		}
		if (sec->secName) {
			nutscan_add_option_to_device(dev, "secName",
				sec->secName);
		}
		if (sec->authPassword) {
			nutscan_add_option_to_device(dev, "authPassword",
				sec->authPassword);
		}
		if (sec->privPassword) {
			nutscan_add_option_to_device(dev, "privPassword",
				sec->privPassword);
		}
		if (sec->authProtocol) {
			nutscan_add_option_to_device(dev, "authProtocol",
				sec->authProtocol);
		}
		if (sec->privProtocol) {
			nutscan_add_option_to_device(dev, "privProtocol",
				sec->privProtocol);
		}
	}
	else {
		buf = malloc (session->community_len + 1);
		if (buf) {
			memcpy(buf, session->community,
				session->community_len);
			buf[session->community_len] = 0;
			nutscan_add_option_to_device(dev, "community", buf);
			free(buf);
		}
	}

#ifdef HAVE_PTHREAD
	pthread_mutex_lock(&dev_mutex);
#endif
	dev_ret = nutscan_add_device_to_device(dev_ret, dev);
#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&dev_mutex);
#endif

}

static struct snmp_pdu * scan_snmp_get_oid(char* oid_str, void* handle)
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

	status = (*nut_snmp_sess_synch_response)(handle, pdu, &response);
	if (response == NULL) {
		index++;
		return NULL;
	}

	if (status != STAT_SUCCESS
	 || response->errstat != SNMP_ERR_NOERROR
	 || response->variables == NULL
	 || response->variables->name == NULL
	 || ((*nut_snmp_oid_compare)(response->variables->name,
	        response->variables->name_length,
	        name, name_len) != 0)
	 || response->variables->val.string == NULL
	) {
		(*nut_snmp_free_pdu)(response);
		index++;
		return NULL;
	}

	return response;
}

static void try_all_oid(void * arg, const char * mib_found)
{
	struct snmp_pdu *response = NULL;
	int index = 0;
	nutscan_snmp_t * sec = (nutscan_snmp_t *)arg;

	upsdebugx(2, "%s", __func__);

	while (snmp_device_table[index].mib != NULL) {

		if (snmp_device_table[index].oid == NULL
		||  snmp_device_table[index].oid[0] == '\0'
		) {
			index++;
			continue;
		}

		response = scan_snmp_get_oid(snmp_device_table[index].oid, sec->handle);
		if (response == NULL) {
			index++;
			continue;
		}

		/* add device only if not yet detected with the same mib */
		if (mib_found == NULL || (strcmp(mib_found, snmp_device_table[index].mib) != 0)) {
			scan_snmp_add_device(sec, response, snmp_device_table[index].mib);
			upsdebugx(3, "Found another match for device with MIB '%s'",
				snmp_device_table[index].mib);
		}
		else {
			upsdebugx(3, "Skip duplicated device %s", snmp_device_table[index].mib);
		}

		(*nut_snmp_free_pdu)(response);
		response = NULL;

		index++;
	}
}

static int init_session(struct snmp_session * snmp_sess, nutscan_snmp_t * sec)
{
	(*nut_snmp_sess_init)(snmp_sess);

	snmp_sess->peername = sec->peername;

	if (sec->community != NULL || sec->secLevel == NULL) {
		snmp_sess->version = SNMP_VERSION_1;
		if (sec->community != NULL) {
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
			fprintf(stderr,
				"Bad SNMPv3 securityLevel: %s\n",
				sec->secLevel);
			return 0;
		}

		/* Security name */
		if (sec->secName == NULL) {
			fprintf(stderr, "securityName is required for SNMPv3\n");
			return 0;
		}
		snmp_sess->securityName = strdup(sec->secName);
		snmp_sess->securityNameLen = strlen(snmp_sess->securityName);

		/* Everything is ready for NOAUTH */
		if (snmp_sess->securityLevel == SNMP_SEC_LEVEL_NOAUTH) {
			return 1;
		}

		/* Process mandatory fields, based on the security level */
		switch (snmp_sess->securityLevel) {
			case SNMP_SEC_LEVEL_AUTHNOPRIV:
				if (sec->authPassword == NULL) {
					fprintf(stderr,
						"authPassword is required "
						"for SNMPv3 in %s mode\n",
						sec->secLevel);
					return 0;
				}
				break;
			case SNMP_SEC_LEVEL_AUTHPRIV:
				if ((sec->authPassword == NULL) ||
					(sec->privPassword == NULL)) {
					fprintf(stderr,
						"authPassword and privPassword are "
						"required for SNMPv3 in %s mode\n",
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

#if NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol
		/* default to MD5 */
		snmp_sess->securityAuthProto = nut_usmHMACMD5AuthProtocol;
		snmp_sess->securityAuthProtoLen =
			sizeof(usmHMACMD5AuthProtocol)/
			sizeof(oid);
#endif

		if (sec->authProtocol) {
#if NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol
			if (strncmp(sec->authProtocol, "SHA", 3) == 0) {
				snmp_sess->securityAuthProto = nut_usmHMACSHA1AuthProtocol;
				snmp_sess->securityAuthProtoLen =
					sizeof(usmHMACSHA1AuthProtocol)/
					sizeof(oid);
			}
			else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol
			if (strcmp(sec->authProtocol, "SHA256") == 0) {
				snmp_sess->securityAuthProto = nut_usmHMAC192SHA256AuthProtocol;
				snmp_sess->securityAuthProtoLen =
					sizeof(usmHMAC192SHA256AuthProtocol)/
					sizeof(oid);
			}
			else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol
			if (strcmp(sec->authProtocol, "SHA384") == 0) {
				snmp_sess->securityAuthProto = nut_usmHMAC256SHA384AuthProtocol;
				snmp_sess->securityAuthProtoLen =
					sizeof(usmHMAC256SHA384AuthProtocol)/
					sizeof(oid);
			}
			else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol
			if (strcmp(sec->authProtocol, "SHA512") == 0) {
				snmp_sess->securityAuthProto = nut_usmHMAC384SHA512AuthProtocol;
				snmp_sess->securityAuthProtoLen =
					sizeof(usmHMAC384SHA512AuthProtocol)/
					sizeof(oid);
			}
			else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol
			if (strncmp(sec->authProtocol, "MD5", 3) != 0) {
#else
			{
#endif
				fprintf(stderr,
					"Bad SNMPv3 authProtocol: %s\n",
					sec->authProtocol);
				return 0;
			}
		}

		/* set the authentication key to a MD5/SHA1 hashed version of
		 * our passphrase (must be at least 8 characters long) */
		if ((uintmax_t)snmp_sess->securityAuthProtoLen > UINT_MAX) {
			fprintf(stderr, "Bad SNMPv3 securityAuthProtoLen: %zu",
				snmp_sess->securityAuthProtoLen);
			return 0;
		}
		if ((*nut_generate_Ku)(snmp_sess->securityAuthProto,
					(u_int)snmp_sess->securityAuthProtoLen,
					(unsigned char *) sec->authPassword,
					strlen(sec->authPassword),
					snmp_sess->securityAuthKey,
					&snmp_sess->securityAuthKeyLen)
					!= SNMPERR_SUCCESS
		) {
			fprintf(stderr,
				"Error generating Ku from "
				"authentication pass phrase\n");
			return 0;
		}

		/* Everything is ready for AUTHNOPRIV */
		if (snmp_sess->securityLevel == SNMP_SEC_LEVEL_AUTHNOPRIV) {
			return 1;
		}

#if NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol
		/* default to DES */
		snmp_sess->securityPrivProto = nut_usmDESPrivProtocol;
		snmp_sess->securityPrivProtoLen =
			sizeof(usmDESPrivProtocol)/sizeof(oid);
#endif

		if (sec->privProtocol) {
#if NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol || NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol
			if (strncmp(sec->privProtocol, "AES", 3) == 0) {
				snmp_sess->securityPrivProto = nut_usmAESPrivProtocol;
				snmp_sess->securityPrivProtoLen =
					sizeof(usmAESPrivProtocol)/
					sizeof(oid);
			}
			else
#endif
#if NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04
# if NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol
			if (strcmp(sec->privProtocol, "AES192") == 0) {
				snmp_sess->securityPrivProto = nut_usmAES192PrivProtocol;
				snmp_sess->securityPrivProtoLen =
					sizeof(usmAES192PrivProtocol)/
					sizeof(oid);
			}
			else
# endif
# if NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol
			if (strcmp(sec->privProtocol, "AES256") == 0) {
				snmp_sess->securityPrivProto = nut_usmAES256PrivProtocol;
				snmp_sess->securityPrivProtoLen =
					sizeof(usmAES256PrivProtocol)/
					sizeof(oid);
			}
			else
# endif
#endif /* NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04 */
#if NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol
			if (strncmp(sec->privProtocol, "DES", 3) != 0) {
#else
			{
#endif
				fprintf(stderr,
					"Bad SNMPv3 privProtocol: %s\n",
					sec->privProtocol);
				return 0;
			}
		}

		/* set the private key to a MD5/SHA hashed version of
		 * our passphrase (must be at least 8 characters long) */
		snmp_sess->securityPrivKeyLen = USM_PRIV_KU_LEN;
		if ((uintmax_t)snmp_sess->securityAuthProtoLen > UINT_MAX) {
			fprintf(stderr, "Bad SNMPv3 securityAuthProtoLen: %zu",
				snmp_sess->securityAuthProtoLen);
			return 0;
		}
		if ((*nut_generate_Ku)(snmp_sess->securityAuthProto,
					(u_int)snmp_sess->securityAuthProtoLen,
					(unsigned char *) sec->privPassword,
					strlen(sec->privPassword),
					snmp_sess->securityPrivKey,
					&snmp_sess->securityPrivKeyLen)
					!= SNMPERR_SUCCESS
		) {
			fprintf(stderr,
				"Error generating Ku from "
				"private pass phrase\n");
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
	char *mib_found = NULL;

	upsdebugx(2, "%s", __func__);

	/* Initialize session */
	if (!init_session(&snmp_sess, sec)) {
		goto try_SysOID_free;
	}

	snmp_sess.retries = 0;
	/* netsnmp timeout is accounted in uS, but typed as long
	 * and not useconds_t (which is at most long per POSIX)
	 */
	snmp_sess.timeout = (long)g_usec_timeout;

	/* Open the session */
	handle = (*nut_snmp_sess_open)(&snmp_sess); /* establish the session */
	if (handle == NULL) {
		fprintf(stderr,
			"Failed to open SNMP session for %s.\n",
			sec->peername);
		goto try_SysOID_free;
	}

	/* create and send request. */
	if (!(*nut_snmp_parse_oid)(SysOID, name, &name_len)) {
		fprintf(stderr,
			"SNMP errors: %s\n",
			(*nut_snmp_api_errstring)((*nut_snmp_errno)));
		(*nut_snmp_sess_close)(handle);
		goto try_SysOID_free;
	}

	pdu = (*nut_snmp_pdu_create)(SNMP_MSG_GET);

	if (pdu == NULL) {
		fprintf(stderr, "Not enough memory\n");
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
		if (response->variables != NULL &&
				response->variables->val.objid != NULL
		) {
			while (snmp_device_table[index].mib != NULL) {
				if (snmp_device_table[index].sysoid == NULL) {
					index++;
					continue;
				}
				name_len = MAX_OID_LEN;

				if (!(*nut_snmp_parse_oid)(
					snmp_device_table[index].sysoid,
					name, &name_len)
				) {
					index++;
					continue;
				}

				if ((*nut_snmp_oid_compare)(
					response->variables->val.objid,
					response->variables->val_len/sizeof(oid),
					name, name_len) == 0
				) {
					/* we have found a relevant sysoid */

					/* add mib if no complementary oid is present */
					/* FIXME: No desc defined when add device */
					if (snmp_device_table[index].oid == NULL
					||  snmp_device_table[index].oid[0] == '\0'
					) {
						scan_snmp_add_device(sec, NULL, snmp_device_table[index].mib);
						mib_found = snmp_device_table[index].sysoid;
					}
					/* else test complementary oid before adding mib */
					else {
						resp = scan_snmp_get_oid(
							snmp_device_table[index].oid,
							handle);
						if (resp != NULL) {
							scan_snmp_add_device(sec, resp, snmp_device_table[index].mib);
							mib_found = snmp_device_table[index].mib;
							(*nut_snmp_free_pdu)(resp);
						}
					}
				}
				index++;
			}
		}

		/* try a list of known OID, if no device was found otherwise */
		if (mib_found == NULL)
			try_all_oid(sec, mib_found);

		(*nut_snmp_free_pdu)(response);
		response = NULL;
	}

	(*nut_snmp_sess_close)(handle);

try_SysOID_free:
	if (sec->peername) {
		free(sec->peername);
	}
	free(sec);

	return NULL;
}

nutscan_device_t * nutscan_scan_snmp(const char * start_ip, const char * stop_ip,
                                     useconds_t usec_timeout, nutscan_snmp_t * sec)
{
	bool_t pass = TRUE; /* Track that we may spawn a scanning thread */
	nutscan_snmp_t * tmp_sec;
	nutscan_ip_iter_t ip;
	char * ip_str = NULL;
#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE
	sem_t * semaphore = nutscan_semaphore();
	sem_t   semaphore_scantype_inst;
	sem_t * semaphore_scantype = &semaphore_scantype_inst;
# endif /* HAVE_SEMAPHORE */
	pthread_t thread;
	nutscan_thread_t * thread_array = NULL;
	size_t thread_count = 0, i;
# if (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE)
	size_t  max_threads_scantype = max_threads_netsnmp;
# endif

	pthread_mutex_init(&dev_mutex, NULL);

# ifdef HAVE_SEMAPHORE
	if (max_threads_scantype > 0) {
		if (SIZE_MAX > UINT_MAX && max_threads_scantype > UINT_MAX) {
			upsdebugx(1,
				"WARNING: %s: Limiting max_threads_scantype to range acceptable for sem_init()",
				__func__);
			max_threads_scantype = UINT_MAX - 1;
		}
		sem_init(semaphore_scantype, 0, (unsigned int)max_threads_scantype);
	}
# endif /* HAVE_SEMAPHORE */

#endif /* HAVE_PTHREAD */

	if (!nutscan_avail_snmp) {
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

	while (ip_str != NULL) {
#ifdef HAVE_PTHREAD
		/* NOTE: With many enough targets to scan, this can crash
		 * by spawning too many children; add a limit and loop to
		 * "reap" some already done with their work. And probably
		 * account them in thread_array[] as something to not wait
		 * for below in pthread_join()...
		 */

# ifdef HAVE_SEMAPHORE
		/* Just wait for someone to free a semaphored slot,
		 * if none are available, and then/otherwise grab one
		 */
		if (thread_array == NULL) {
			/* Starting point, or after a wait to complete
			 * all earlier runners */
			if (max_threads_scantype > 0)
				sem_wait(semaphore_scantype);
			sem_wait(semaphore);
			pass = TRUE;
		} else {
			pass = ((max_threads_scantype == 0 || sem_trywait(semaphore_scantype) == 0) &&
			        sem_trywait(semaphore) == 0);
		}
# else
#  ifdef HAVE_PTHREAD_TRYJOIN
		/* A somewhat naive and brute-force solution for
		 * systems without a semaphore.h. This may suffer
		 * some off-by-one errors, using a few more threads
		 * than intended (if we race a bit at the wrong time,
		 * probably up to one per enabled scanner routine).
		 */

		/* TOTHINK: Should there be a threadcount_mutex when
		 * we just read the value in if() and while() below?
		 * At worst we would overflow the limit a bit due to
		 * other protocol scanners...
		 */
		if (curr_threads >= max_threads
		|| (curr_threads >= max_threads_scantype && max_threads_scantype > 0)
		) {
			upsdebugx(2, "%s: already running %zu scanning threads "
				"(launched overall: %zu), "
				"waiting until some would finish",
				__func__, curr_threads, thread_count);
			while (curr_threads >= max_threads
			   || (curr_threads >= max_threads_scantype && max_threads_scantype > 0)
			) {
				for (i = 0; i < thread_count ; i++) {
					int ret;

					if (!thread_array[i].active) continue;

					pthread_mutex_lock(&threadcount_mutex);
					upsdebugx(3, "%s: Trying to join thread #%i...", __func__, i);
					ret = pthread_tryjoin_np(thread_array[i].thread, NULL);
					switch (ret) {
						case ESRCH:     // No thread with the ID thread could be found - already "joined"?
							upsdebugx(5, "%s: Was thread #%zu joined earlier?", __func__, i);
							break;
						case 0:         // thread exited
							if (curr_threads > 0) {
								curr_threads --;
								upsdebugx(4, "%s: Joined a finished thread #%zu", __func__, i);
							} else {
								/* threadcount_mutex fault? */
								upsdebugx(0, "WARNING: %s: Accounting of thread count "
									"says we are already at 0", __func__);
							}
							thread_array[i].active = FALSE;
							break;
						case EBUSY:     // actively running
							upsdebugx(6, "%s: thread #%zu still busy (%i)",
								__func__, i, ret);
							break;
						case EDEADLK:   // Errors with thread interactions... bail out?
						case EINVAL:    // Errors with thread interactions... bail out?
						default:        // new pthreads abilities?
							upsdebugx(5, "%s: thread #%zu reported code %i",
								__func__, i, ret);
							break;
					}
					pthread_mutex_unlock(&threadcount_mutex);
				}

				if (curr_threads >= max_threads
				|| (curr_threads >= max_threads_scantype && max_threads_scantype > 0)
				) {
					usleep (10000); // microSec's, so 0.01s here
				}
			}
			upsdebugx(2, "%s: proceeding with scan", __func__);
		}
		/* NOTE: No change to default "pass" in this ifdef:
		 * if we got to this line, we have a slot to use */
#  endif /* HAVE_PTHREAD_TRYJOIN */
# endif  /* HAVE_SEMAPHORE */
#endif   /* HAVE_PTHREAD */

		if (pass) {
			tmp_sec = malloc(sizeof(nutscan_snmp_t));
			memcpy(tmp_sec, sec, sizeof(nutscan_snmp_t));
			tmp_sec->peername = ip_str;

#ifdef HAVE_PTHREAD
			if (pthread_create(&thread, NULL, try_SysOID, (void*)tmp_sec) == 0) {
# ifdef HAVE_PTHREAD_TRYJOIN
				pthread_mutex_lock(&threadcount_mutex);
				curr_threads++;
# endif /* HAVE_PTHREAD_TRYJOIN */

				thread_count++;
				nutscan_thread_t *new_thread_array = realloc(thread_array,
					thread_count * sizeof(nutscan_thread_t));
				if (new_thread_array == NULL) {
					upsdebugx(1, "%s: Failed to realloc thread array", __func__);
					break;
				}
				else {
					thread_array = new_thread_array;
				}
				thread_array[thread_count - 1].thread = thread;
				thread_array[thread_count - 1].active = TRUE;

# ifdef HAVE_PTHREAD_TRYJOIN
				pthread_mutex_unlock(&threadcount_mutex);
# endif /* HAVE_PTHREAD_TRYJOIN */
			}
#else   /* not HAVE_PTHREAD */
			try_SysOID((void *)tmp_sec);
#endif  /* if HAVE_PTHREAD */
/*			free(ip_str); */ /* Do not free() here - seems to cause a double-free instead */
			ip_str = nutscan_ip_iter_inc(&ip);
/*			free(tmp_sec); */
		} else { /* if not pass -- all slots busy */
#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE
			/* Wait for all current scans to complete */
			if (thread_array != NULL) {
				upsdebugx (2, "%s: Running too many scanning threads, "
					"waiting until older ones would finish",
					__func__);
				for (i = 0; i < thread_count ; i++) {
					int ret;
					if (!thread_array[i].active) {
						/* Probably should not get here,
						 * but handle it just in case */
						upsdebugx(0, "WARNING: %s: Midway clean-up: did not expect thread %zu to be not active",
							__func__, i);
						sem_post(semaphore);
						if (max_threads_scantype > 0)
							sem_post(semaphore_scantype);
						continue;
					}
					thread_array[i].active = FALSE;
					ret = pthread_join(thread_array[i].thread, NULL);
					if (ret != 0) {
						upsdebugx(0, "WARNING: %s: Midway clean-up: pthread_join() returned code %i",
							__func__, ret);
					}
					sem_post(semaphore);
					if (max_threads_scantype > 0)
						sem_post(semaphore_scantype);
				}
				thread_count = 0;
				free(thread_array);
				thread_array = NULL;
			}
# else
#  ifdef HAVE_PTHREAD_TRYJOIN
		/* TODO: Move the wait-loop for TRYJOIN here? */
#  endif /* HAVE_PTHREAD_TRYJOIN */
# endif  /* HAVE_SEMAPHORE */
#endif   /* HAVE_PTHREAD */
		} /* if: could we "pass" or not? */
	} /* while */

#ifdef HAVE_PTHREAD
	if (thread_array != NULL) {
		upsdebugx(2, "%s: all planned scans launched, waiting for threads to complete", __func__);
		for (i = 0; i < thread_count; i++) {
			int ret;

			if (!thread_array[i].active) continue;

			ret = pthread_join(thread_array[i].thread, NULL);
			if (ret != 0) {
				upsdebugx(0, "WARNING: %s: Clean-up: pthread_join() returned code %i",
					__func__, ret);
			}
			thread_array[i].active = FALSE;
# ifdef HAVE_SEMAPHORE
			sem_post(semaphore);
			if (max_threads_scantype > 0)
				sem_post(semaphore_scantype);
# else
#  ifdef HAVE_PTHREAD_TRYJOIN
			pthread_mutex_lock(&threadcount_mutex);
			if (curr_threads > 0) {
				curr_threads --;
				upsdebugx(5, "%s: Clean-up: Joined a finished thread #%zu",
					__func__, i);
			} else {
				upsdebugx(0, "WARNING: %s: Clean-up: Accounting of thread count "
					"says we are already at 0", __func__);
			}
			pthread_mutex_unlock(&threadcount_mutex);
#  endif /* HAVE_PTHREAD_TRYJOIN */
# endif /* HAVE_SEMAPHORE */
		}
		free(thread_array);
		upsdebugx(2, "%s: all threads freed", __func__);
	}
	pthread_mutex_destroy(&dev_mutex);

# ifdef HAVE_SEMAPHORE
	if (max_threads_scantype > 0)
		sem_destroy(semaphore_scantype);
# endif /* HAVE_SEMAPHORE */
#endif /* HAVE_PTHREAD */

	nutscan_device_t * result = nutscan_rewind_device(dev_ret);
	dev_ret = NULL;
	return result;
}

#else /* WITH_SNMP */

nutscan_device_t * nutscan_scan_snmp(const char * start_ip, const char * stop_ip,
                                     useconds_t usec_timeout, nutscan_snmp_t * sec)
{
	NUT_UNUSED_VARIABLE(start_ip);
	NUT_UNUSED_VARIABLE(stop_ip);
	NUT_UNUSED_VARIABLE(usec_timeout);
	NUT_UNUSED_VARIABLE(sec);
	return NULL;
}

#endif /* WITH_SNMP */
