/*  snmp-ups.c - NUT Meta SNMP driver (support different MIBS)
 *
 *  Based on NetSNMP API (Simple Network Management Protocol V1-2)
 *
 *  Copyright (C)
 *	2002 - 2014	Arnaud Quette <arnaud.quette@free.fr>
 *	2015		Arnaud Quette <ArnaudQuette@Eaton.com>
 *	2002 - 2006	Dmitry Frolov <frolov@riss-telecom.ru>
 *			J.W. Hoogervorst <jeroen@hoogervorst.net>
 *			Niels Baggesen <niels@baggesen.net>
 *	2009 - 2010	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 *  Sponsored by Eaton <http://www.eaton.com>
 *   and originally by MGE UPS SYSTEMS <http://www.mgeups.com/>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <limits.h>

/* NUT SNMP common functions */
#include "main.h"
#include "snmp-ups.h"
#include "parseconf.h"

/* include all known mib2nut lookup tables */
#include "apc-mib.h"
#include "mge-mib.h"
#include "netvision-mib.h"
#include "powerware-mib.h"
#include "eaton-mib.h"
#include "raritan-pdu-mib.h"
#include "baytech-mib.h"
#include "compaq-mib.h"
#include "bestpower-mib.h"
#include "cyberpower-mib.h"
#include "delta_ups-mib.h"
#include "huawei-mib.h"
#include "ietf-mib.h"
#include "xppc-mib.h"

/* Address API change */
#ifndef usmAESPrivProtocol
#define usmAESPrivProtocol usmAES128PrivProtocol
#endif

static mib2nut_info_t *mib2nut[] = {
	&apc,
	&mge,
	&netvision,
	&powerware,
	&pxgx_ups,
	&aphel_genesisII,
	&aphel_revelation,
	&eaton_marlin,
	&pulizzi_switched1,
	&pulizzi_switched2,
	&raritan,
	&baytech,
	&compaq,
	&bestpower,
	&cyberpower,
	&delta_ups,
	&xppc,
	&huawei,
	/*
	 * Prepend vendor specific MIB mappings before IETF, so that
	 * if a device supports both IETF and vendor specific MIB,
	 * the vendor specific one takes precedence (when mib=auto)
	 */
	&ietf,
	/* end of structure. */
	NULL
};

struct snmp_session g_snmp_sess, *g_snmp_sess_p;
const char *OID_pwr_status;
int g_pwr_battery;
int pollfreq; /* polling frequency */
int input_phases, output_phases, bypass_phases;

/* pointer to the Snmp2Nut lookup table */
mib2nut_info_t *mib2nut_info;
/* FIXME: to be trashed */
snmp_info_t *snmp_info;
alarms_info_t *alarms_info;
const char *mibname;
const char *mibvers;

#define DRIVER_NAME	"Generic SNMP UPS driver"
#define DRIVER_VERSION		"0.83"

/* driver description structure */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@free.fr>\n" \
	"Dmitry Frolov <frolov@riss-telecom.ru>\n" \
	"J.W. Hoogervorst <jeroen@hoogervorst.net>\n" \
	"Niels Baggesen <niels@baggesen.net>\n" \
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_STABLE,
	{ NULL }
};
/* FIXME: integrate MIBs info? do the same as for usbhid-ups! */

time_t lastpoll = 0;

/* template OIDs index start with 0 or 1 (estimated stable for a MIB),
 * automatically guessed at the first pass */
int template_index_base = -1;

/* sysOID location */
#define SYSOID_OID	".1.3.6.1.2.1.1.2.0"

/* Forward functions declarations */
static void disable_transfer_oids(void);
bool_t get_and_process_data(int mode, snmp_info_t *su_info_p);
int extract_template_number(int template_type, const char* varname);

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
void upsdrv_initinfo(void)
{
	snmp_info_t *su_info_p;

	upsdebugx(1, "SNMP UPS driver : entering upsdrv_initinfo()");

	dstate_setinfo("driver.version.data", "%s MIB %s", mibname, mibvers);

	/* add instant commands to the info database.
	 * outlet (and groups) commands are processed later, during initial walk */
	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)
	{
		su_info_p->flags |= SU_FLAG_OK;
		if ((SU_TYPE(su_info_p) == SU_TYPE_CMD)
			&& !(su_info_p->flags & SU_OUTLET)
			&& !(su_info_p->flags & SU_OUTLET_GROUP)) {
			/* first check that this OID actually exists */
			if (nut_snmp_get(su_info_p->OID) != NULL) {
				dstate_addcmd(su_info_p->info_type);
				upsdebugx(1, "upsdrv_initinfo(): adding command '%s'", su_info_p->info_type);
			}
		}
	}

	if (testvar("notransferoids"))
		disable_transfer_oids();

	/* initialize all other INFO_ fields from list */
	if (snmp_ups_walk(SU_WALKMODE_INIT))
		dstate_dataok();
	else
		dstate_datastale();

	/* setup handlers for instcmd and setvar functions */
	upsh.setvar = su_setvar;
	upsh.instcmd = su_instcmd;
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1,"SNMP UPS driver : entering upsdrv_updateinfo()");

	/* only update every pollfreq */
	/* FIXME: only update status (SU_STATUS_*), à la usbhid-ups, in between */
	if (time(NULL) > (lastpoll + pollfreq)) {

		alarm_init();
		status_init();

		/* update all dynamic info fields */
		if (snmp_ups_walk(SU_WALKMODE_UPDATE))
			dstate_dataok();
		else
			dstate_datastale();

		alarm_commit();
		status_commit();

		/* store timestamp */
		lastpoll = time(NULL);
	}
}

void upsdrv_shutdown(void)
{
	/*
	This driver will probably never support this. In order to
	be any use, the driver should be called near the end of
	the system halt script. By that time we in all likelyhood
	we won't have network capabilities anymore, so we could
	never send this command to the UPS. This is not an error,
	but a limitation of the interface used.
	*/

	upsdebugx(1, "upsdrv_shutdown...");

	/* Try to shutdown with delay */
	if (su_instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	/* If the above doesn't work, try shutdown.reboot */
	if (su_instcmd("shutdown.reboot", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	/* If the above doesn't work, try load.off.delay */
	if (su_instcmd("load.off.delay", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	fatalx(EXIT_FAILURE, "Shutdown failed!");
}

void upsdrv_help(void)
{
	upsdebugx(1, "entering upsdrv_help");
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	upsdebugx(1, "entering upsdrv_makevartable()");

	addvar(VAR_VALUE, SU_VAR_MIBS,
		"Set MIB compliance (default=ietf, allowed: mge,apcc,netvision,pw,cpqpower,...)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_COMMUNITY,
		"Set community name (default=public)");
	addvar(VAR_VALUE, SU_VAR_VERSION,
		"Set SNMP version (default=v1, allowed v2c)");
	addvar(VAR_VALUE, SU_VAR_POLLFREQ,
		"Set polling frequency in seconds, to reduce network flow (default=30)");
	addvar(VAR_VALUE, SU_VAR_RETRIES,
		"Specifies the number of Net-SNMP retries to be used in the requests (default=5)");
	addvar(VAR_VALUE, SU_VAR_TIMEOUT,
		"Specifies the Net-SNMP timeout in seconds between retries (default=1)");
	addvar(VAR_FLAG, "notransferoids",
		"Disable transfer OIDs (use on APCC Symmetras)");
	addvar(VAR_VALUE, SU_VAR_SECLEVEL,
		"Set the securityLevel used for SNMPv3 messages (default=noAuthNoPriv, allowed: authNoPriv,authPriv)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_SECNAME,
		"Set the securityName used for authenticated SNMPv3 messages (no default)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_AUTHPASSWD,
		"Set the authentication pass phrase used for authenticated SNMPv3 messages (no default)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_PRIVPASSWD,
		"Set  the privacy pass phrase used for encrypted SNMPv3 messages (no default)");
	addvar(VAR_VALUE, SU_VAR_AUTHPROT,
		"Set the authentication protocol (MD5 or SHA) used for authenticated SNMPv3 messages (default=MD5)");
	addvar(VAR_VALUE, SU_VAR_PRIVPROT,
		"Set the privacy protocol (DES or AES) used for encrypted SNMPv3 messages (default=DES)");
}

void upsdrv_initups(void)
{
	snmp_info_t *su_info_p;
	char model[SU_INFOSIZE];
	bool_t status;
	const char *mibs;

	upsdebugx(1, "SNMP UPS driver : entering upsdrv_initups()");

	/* Retrieve user's parameters */
	mibs = testvar(SU_VAR_MIBS) ? getval(SU_VAR_MIBS) : "auto";

	/* init SNMP library, etc... */
	nut_snmp_init(progname, device_path);

	/* FIXME: first test if the device is reachable to avoid timeouts! */

	/* Load the SNMP to NUT translation data */
	load_mib2nut(mibs);

	/* init polling frequency */
	if (getval(SU_VAR_POLLFREQ))
		pollfreq = atoi(getval(SU_VAR_POLLFREQ));
	else
		pollfreq = DEFAULT_POLLFREQ;

	/* Get UPS Model node to see if there's a MIB */
	su_info_p = su_find_info("ups.model");
	status = nut_snmp_get_str(su_info_p->OID, model, sizeof(model), NULL);

	if (status == TRUE)
		upslogx(0, "Detected %s on host %s (mib: %s %s)",
			 model, device_path, mibname, mibvers);
	else
		fatalx(EXIT_FAILURE, "%s MIB wasn't found on %s", mibs, g_snmp_sess.peername);
		/* FIXME: "No supported device detected" */

	if (su_find_info("load.off.delay")) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.off");
	}

	if (su_find_info("load.on.delay")) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.on");
	}

	if (su_find_info("load.off.delay") && su_find_info("load.on.delay")) {
		/* Add composite instcmds (require setting multiple OID values) */
		dstate_addcmd("shutdown.return");
		dstate_addcmd("shutdown.stayoff");
	}
}

void upsdrv_cleanup(void)
{
	nut_snmp_cleanup();
}

/* -----------------------------------------------------------
 * SNMP functions.
 * ----------------------------------------------------------- */

void nut_snmp_init(const char *type, const char *hostname)
{
	char *ns_options = NULL;
	const char *community, *version;
	const char *secLevel = NULL, *authPassword, *privPassword;
	const char *authProtocol, *privProtocol;
	int snmp_retries = DEFAULT_NETSNMP_RETRIES;
	long snmp_timeout = DEFAULT_NETSNMP_TIMEOUT;

	upsdebugx(2, "SNMP UPS driver : entering nut_snmp_init(%s)", type);

	/* Force numeric OIDs resolution (ie, do not resolve to textual names)
	 * This is mostly for the convenience of debug output */
	ns_options = snmp_out_toggle_options("n");
	if (ns_options != NULL) {
		upsdebugx(2, "Failed to enable numeric OIDs resolution");
	}

	/* Initialize the SNMP library */
	init_snmp(type);

	/* Initialize session */
	snmp_sess_init(&g_snmp_sess);

	g_snmp_sess.peername = xstrdup(hostname);

	/* Net-SNMP timeout and retries */
	if (testvar(SU_VAR_RETRIES)) {
		snmp_retries = atoi(getval(SU_VAR_RETRIES));
	}
	g_snmp_sess.retries = snmp_retries;
	upsdebugx(2, "Setting SNMP retries to %i", snmp_retries);

	if (testvar(SU_VAR_TIMEOUT)) {
		snmp_timeout = atol(getval(SU_VAR_TIMEOUT));
	}
	/* We have to convert from seconds to microseconds */
	g_snmp_sess.timeout = snmp_timeout * ONE_SEC;
	upsdebugx(2, "Setting SNMP timeout to %ld second(s)", snmp_timeout);

	/* Retrieve user parameters */
	version = testvar(SU_VAR_VERSION) ? getval(SU_VAR_VERSION) : "v1";
	
	if ((strcmp(version, "v1") == 0) || (strcmp(version, "v2c") == 0)) {
		g_snmp_sess.version = (strcmp(version, "v1") == 0) ? SNMP_VERSION_1 : SNMP_VERSION_2c;
		community = testvar(SU_VAR_COMMUNITY) ? getval(SU_VAR_COMMUNITY) : "public";
		g_snmp_sess.community = (unsigned char *)xstrdup(community);
		g_snmp_sess.community_len = strlen(community);
	}
	else if (strcmp(version, "v3") == 0) {
		/* SNMP v3 related init */
		g_snmp_sess.version = SNMP_VERSION_3;

		/* Security level */
		if (testvar(SU_VAR_SECLEVEL)) {
			secLevel = getval(SU_VAR_SECLEVEL);

			if (strcmp(secLevel, "noAuthNoPriv") == 0)
				g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_NOAUTH;
			else if (strcmp(secLevel, "authNoPriv") == 0)
				g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
			else if (strcmp(secLevel, "authPriv") == 0)
				g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
			else
				fatalx(EXIT_FAILURE, "Bad SNMPv3 securityLevel: %s", secLevel);
		}
		else
			g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_NOAUTH;

		/* Security name */
		if (testvar(SU_VAR_SECNAME)) {
			g_snmp_sess.securityName = xstrdup(getval(SU_VAR_SECNAME));
			g_snmp_sess.securityNameLen = strlen(g_snmp_sess.securityName);
		}
		else
			fatalx(EXIT_FAILURE, "securityName is required for SNMPv3");

		/* Process mandatory fields, based on the security level */
		authPassword = testvar(SU_VAR_AUTHPASSWD) ? getval(SU_VAR_AUTHPASSWD) : NULL;
		privPassword = testvar(SU_VAR_PRIVPASSWD) ? getval(SU_VAR_PRIVPASSWD) : NULL;

		switch (g_snmp_sess.securityLevel) {
			case SNMP_SEC_LEVEL_AUTHNOPRIV:
				if (authPassword == NULL)
					fatalx(EXIT_FAILURE, "authPassword is required for SNMPv3 in %s mode", secLevel);
				break;
			case SNMP_SEC_LEVEL_AUTHPRIV:
				if ((authPassword == NULL) || (privPassword == NULL))
					fatalx(EXIT_FAILURE, "authPassword and privPassword are required for SNMPv3 in %s mode", secLevel);
				break;
			default:
			case SNMP_SEC_LEVEL_NOAUTH:
				/* nothing else needed */
				break;
		}

		/* Process authentication protocol and key */
		g_snmp_sess.securityAuthKeyLen = USM_AUTH_KU_LEN;
		authProtocol = testvar(SU_VAR_AUTHPROT) ? getval(SU_VAR_AUTHPROT) : "MD5";

		if (strcmp(authProtocol, "MD5") == 0) {
			g_snmp_sess.securityAuthProto = usmHMACMD5AuthProtocol;
			g_snmp_sess.securityAuthProtoLen = sizeof(usmHMACMD5AuthProtocol)/sizeof(oid);
		}
		else if (strcmp(authProtocol, "SHA") == 0) {
			g_snmp_sess.securityAuthProto = usmHMACSHA1AuthProtocol;
			g_snmp_sess.securityAuthProtoLen = sizeof(usmHMACSHA1AuthProtocol)/sizeof(oid);
		}
		else
			fatalx(EXIT_FAILURE, "Bad SNMPv3 authProtocol: %s", authProtocol);

		/* set the authentication key to a MD5/SHA1 hashed version of our
		 * passphrase (must be at least 8 characters long) */
		if(g_snmp_sess.securityLevel != SNMP_SEC_LEVEL_NOAUTH) {
			if (generate_Ku(g_snmp_sess.securityAuthProto,
				g_snmp_sess.securityAuthProtoLen,
				(u_char *) authPassword, strlen(authPassword),
				g_snmp_sess.securityAuthKey,
				&g_snmp_sess.securityAuthKeyLen) !=
				SNMPERR_SUCCESS) {
				fatalx(EXIT_FAILURE, "Error generating Ku from authentication pass phrase");
			}
		}

		privProtocol = testvar(SU_VAR_PRIVPROT) ? getval(SU_VAR_PRIVPROT) : "DES";

		if (strcmp(privProtocol, "DES") == 0) {
			g_snmp_sess.securityPrivProto = usmDESPrivProtocol;
			g_snmp_sess.securityPrivProtoLen =  sizeof(usmDESPrivProtocol)/sizeof(oid);
		}
		else if (strcmp(privProtocol, "AES") == 0) {
			g_snmp_sess.securityPrivProto = usmAESPrivProtocol;
			g_snmp_sess.securityPrivProtoLen =  sizeof(usmAESPrivProtocol)/sizeof(oid);
		}
		else
			fatalx(EXIT_FAILURE, "Bad SNMPv3 authProtocol: %s", authProtocol);

		/* set the privacy key to a MD5/SHA1 hashed version of our
		 * passphrase (must be at least 8 characters long) */
		if(g_snmp_sess.securityLevel == SNMP_SEC_LEVEL_AUTHPRIV) {
			g_snmp_sess.securityPrivKeyLen = USM_PRIV_KU_LEN;
			if (generate_Ku(g_snmp_sess.securityAuthProto,
				g_snmp_sess.securityAuthProtoLen,
				(u_char *) privPassword, strlen(privPassword),
				g_snmp_sess.securityPrivKey,
				&g_snmp_sess.securityPrivKeyLen) !=
				SNMPERR_SUCCESS) {
				fatalx(EXIT_FAILURE, "Error generating Ku from privacy pass phrase");
			}
		}
	}
	else
		fatalx(EXIT_FAILURE, "Bad SNMP version: %s", version);

	/* Open the session */
	SOCK_STARTUP; /* MS Windows wrapper, not really needed on Unix! */
	g_snmp_sess_p = snmp_open(&g_snmp_sess);	/* establish the session */
	if (g_snmp_sess_p == NULL) {
		nut_snmp_perror(&g_snmp_sess, 0, NULL, "nut_snmp_init: snmp_open");
		fatalx(EXIT_FAILURE, "Unable to establish communication");
	}
}

void nut_snmp_cleanup(void)
{
	/* close snmp session. */
	if (g_snmp_sess_p) {
		snmp_close(g_snmp_sess_p);
		g_snmp_sess_p = NULL;
	}
	SOCK_CLEANUP; /* wrapper not needed on Unix! */
}

/* Free a struct snmp_pdu * returned by nut_snmp_walk */
void nut_snmp_free(struct snmp_pdu ** array_to_free)
{
	struct snmp_pdu ** current_element;

	current_element = array_to_free;

	while (*current_element != NULL) {
		snmp_free_pdu(*current_element);
		current_element++;
	}

	free( array_to_free );
}

/* Return a NULL terminated array of snmp_pdu * */
struct snmp_pdu **nut_snmp_walk(const char *OID, int max_iteration)
{
	int status;
	struct snmp_pdu *pdu, *response = NULL;
	oid name[MAX_OID_LEN];
	size_t name_len = MAX_OID_LEN;
	oid * current_name;
	size_t current_name_len;
	static unsigned int numerr = 0;
	int nb_iteration = 0;
	struct snmp_pdu ** ret_array = NULL;
	int type = SNMP_MSG_GET;

	upsdebugx(3, "nut_snmp_walk(%s)", OID);

	/* create and send request. */
	if (!snmp_parse_oid(OID, name, &name_len)) {
		upsdebugx(2, "[%s] nut_snmp_walk: %s: %s",
			upsname?upsname:device_name, OID, snmp_api_errstring(snmp_errno));
		return NULL;
	}

	current_name = name;
	current_name_len = name_len;

	while( nb_iteration < max_iteration ) {
		/* Going to a shorter OID means we are outside our sub-tree */
		if( current_name_len < name_len ) {
			break;
		}

		pdu = snmp_pdu_create(type);

		if (pdu == NULL) {
			fatalx(EXIT_FAILURE, "Not enough memory");
		}

		snmp_add_null_var(pdu, current_name, current_name_len);

		status = snmp_synch_response(g_snmp_sess_p, pdu, &response);

		if (!response) {
			break;
		}

		if (!((status == STAT_SUCCESS) && (response->errstat == SNMP_ERR_NOERROR))) {
			if (mibname == NULL) {
				/* We are probing for proper mib - ignore errors */
				snmp_free_pdu(response);
				return NULL;
			}

			numerr++;

			if ((numerr == SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0)) {
				upslogx(LOG_WARNING, "[%s] Warning: excessive poll "
						"failures, limiting error reporting (OID = %s)",
						upsname?upsname:device_name, OID);
			}

			if ((numerr < SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0)) {
				if (type == SNMP_MSG_GETNEXT) {
					upsdebugx(2, "=> No more OID, walk complete");
				}
				else {
					nut_snmp_perror(g_snmp_sess_p, status, response,
							"nut_snmp_walk: %s", OID);
				}
			}

			snmp_free_pdu(response);
			break;
		} else {
			numerr = 0;
		}

		nb_iteration++;
		/* +1 is for the terminating NULL */
		ret_array = realloc(ret_array,sizeof(struct snmp_pdu*)*(nb_iteration+1));
		ret_array[nb_iteration-1] = response;
		ret_array[nb_iteration]=NULL;

		current_name = response->variables->name;
		current_name_len = response->variables->name_length;

		type = SNMP_MSG_GETNEXT;
	}

	return ret_array;
}

struct snmp_pdu *nut_snmp_get(const char *OID)
{
	struct snmp_pdu ** pdu_array;
	struct snmp_pdu * ret_pdu;

	if (OID == NULL)
		return NULL;

	upsdebugx(3, "nut_snmp_get(%s)", OID);

	pdu_array = nut_snmp_walk(OID,1);

	if(pdu_array == NULL) {
		return NULL;
	}

	ret_pdu = snmp_clone_pdu(*pdu_array);

	nut_snmp_free(pdu_array);

	return ret_pdu;
}

static bool_t decode_str(struct snmp_pdu *pdu, char *buf, size_t buf_len, info_lkp_t *oid2info) {
	size_t len = 0;

	/* zero out buffer. */
	memset(buf, 0, buf_len);

	switch (pdu->variables->type) {
	case ASN_OCTET_STR:
	case ASN_OPAQUE:
		len = pdu->variables->val_len > buf_len - 1 ?
			buf_len - 1 : pdu->variables->val_len;
		memcpy(buf, pdu->variables->val.string, len);
		buf[len] = '\0';
		break;
	case ASN_INTEGER:
	case ASN_COUNTER:
	case ASN_GAUGE:
		if(oid2info) {
			const char *str;
			if((str=su_find_infoval(oid2info, *pdu->variables->val.integer))) {
				strncpy(buf, str, buf_len-1);
			}
			else {
				strncpy(buf, "UNKNOWN", buf_len-1);
			}
			buf[buf_len-1]='\0';
		}
		else {
			len = snprintf(buf, buf_len, "%ld", *pdu->variables->val.integer);
		}
		break;
	case ASN_TIMETICKS:
		/* convert timeticks to seconds */
		len = snprintf(buf, buf_len, "%ld", *pdu->variables->val.integer / 100);
		break;
	case ASN_OBJECT_ID:
		len = snprint_objid (buf, buf_len, pdu->variables->val.objid, pdu->variables->val_len / sizeof(oid));
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

bool_t nut_snmp_get_str(const char *OID, char *buf, size_t buf_len, info_lkp_t *oid2info)
{
	struct snmp_pdu *pdu;
	bool_t ret;

	upsdebugx(3, "Entering nut_snmp_get_str()");

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

	ret = decode_str(pdu,buf,buf_len,oid2info);

	if(ret == FALSE) {
		upsdebugx(2, "[%s] unhandled ASN 0x%x received from %s",
			upsname?upsname:device_name, pdu->variables->type, OID);
	}

	snmp_free_pdu(pdu);

	return ret;
}

bool_t nut_snmp_get_int(const char *OID, long *pval)
{
	struct snmp_pdu *pdu;
	long value;
	char *buf;

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

	switch (pdu->variables->type) {
	case ASN_OCTET_STR:
	case ASN_OPAQUE:
		buf = xmalloc(pdu->variables->val_len + 1);
		memcpy(buf, pdu->variables->val.string, pdu->variables->val_len);
		buf[pdu->variables->val_len] = '\0';
		value = strtol(buf, NULL, 0);
		free(buf);
		break;
	case ASN_INTEGER:
	case ASN_COUNTER:
	case ASN_GAUGE:
		value = *pdu->variables->val.integer;
		break;
	case ASN_TIMETICKS:
		/* convert timeticks to seconds */
		value = *pdu->variables->val.integer / 100;
		break;
	default:
		upslogx(LOG_ERR, "[%s] unhandled ASN 0x%x received from %s",
			upsname?upsname:device_name, pdu->variables->type, OID);
		return FALSE;
		break;
	}

	snmp_free_pdu(pdu);

	if (pval != NULL)
		*pval = value;

	return TRUE;
}

bool_t nut_snmp_set(const char *OID, char type, const char *value)
{
	int status;
	bool_t ret = FALSE;
	struct snmp_pdu *pdu, *response = NULL;
	oid name[MAX_OID_LEN];
	size_t name_len = MAX_OID_LEN;

	upsdebugx(1, "entering nut_snmp_set (%s, %c, %s)", OID, type, value);

	if (!snmp_parse_oid(OID, name, &name_len)) {
		upslogx(LOG_ERR, "[%s] nut_snmp_set: %s: %s",
			upsname?upsname:device_name, OID, snmp_api_errstring(snmp_errno));
		return FALSE;
	}

	pdu = snmp_pdu_create(SNMP_MSG_SET);
	if (pdu == NULL)
		fatalx(EXIT_FAILURE, "Not enough memory");

	if (snmp_add_var(pdu, name, name_len, type, value)) {
		upslogx(LOG_ERR, "[%s] nut_snmp_set: %s: %s",
			upsname?upsname:device_name, OID, snmp_api_errstring(snmp_errno));

		return FALSE;
	}

	status = snmp_synch_response(g_snmp_sess_p, pdu, &response);

	if ((status == STAT_SUCCESS) && (response->errstat == SNMP_ERR_NOERROR))
		ret = TRUE;
	else
		nut_snmp_perror(g_snmp_sess_p, status, response,
			"nut_snmp_set: can't set %s", OID);

	snmp_free_pdu(response);
	return ret;
}

bool_t nut_snmp_set_str(const char *OID, const char *value)
{
	return nut_snmp_set(OID, 's', value);
}

bool_t nut_snmp_set_int(const char *OID, long value)
{
	char buf[SU_BUFSIZE];

	snprintf(buf, sizeof(buf), "%ld", value);
	return nut_snmp_set(OID, 'i', buf);
}

bool_t nut_snmp_set_time(const char *OID, long value)
{
	char buf[SU_BUFSIZE];

	snprintf(buf, SU_BUFSIZE, "%ld", value * 100);
	return nut_snmp_set(OID, 't', buf);
}

/* log descriptive SNMP error message. */
void nut_snmp_perror(struct snmp_session *sess, int status,
	struct snmp_pdu *response, const char *fmt, ...)
{
	va_list va;
	int cliberr, snmperr;
	char *snmperrstr;
	char buf[SU_LARGEBUF];

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	if (response == NULL) {
		snmp_error(sess, &cliberr, &snmperr, &snmperrstr);
		upslogx(LOG_ERR, "[%s] %s: %s",
			upsname?upsname:device_name, buf, snmperrstr);
		free(snmperrstr);
	} else if (status == STAT_SUCCESS) {
		switch (response->errstat)
		{
		case SNMP_ERR_NOERROR:
			break;
		case SNMP_ERR_NOSUCHNAME:	/* harmless */
			upsdebugx(2, "[%s] %s: %s",
					 upsname?upsname:device_name, buf, snmp_errstring(response->errstat));
			break;
		default:
			upslogx(LOG_ERR, "[%s] %s: Error in packet: %s",
				upsname?upsname:device_name, buf, snmp_errstring(response->errstat));
			break;
		}
	} else if (status == STAT_TIMEOUT) {
		upslogx(LOG_ERR, "[%s] %s: Timeout: no response from %s",
			upsname?upsname:device_name, buf, sess->peername);
	} else {
		snmp_sess_error(sess, &cliberr, &snmperr, &snmperrstr);
		upslogx(LOG_ERR, "[%s] %s: %s",
			upsname?upsname:device_name, buf, snmperrstr);
		free(snmperrstr);
	}
}

/* -----------------------------------------------------------
 * utility functions.
 * ----------------------------------------------------------- */

/* deal with APCC weirdness on Symmetras */
static void disable_transfer_oids(void)
{
	snmp_info_t *su_info_p;

	upslogx(LOG_INFO, "Disabling transfer OIDs");

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++) {
		if (!strcasecmp(su_info_p->info_type, "input.transfer.low")) {
			su_info_p->flags &= ~SU_FLAG_OK;
			continue;
		}

		if (!strcasecmp(su_info_p->info_type, "input.transfer.high")) {
			su_info_p->flags &= ~SU_FLAG_OK;
			continue;
		}
	}
}

/* universal function to add or update info element. */
void su_setinfo(snmp_info_t *su_info_p, const char *value)
{
	upsdebugx(1, "entering su_setinfo(%s)", su_info_p->info_type);

	if (SU_TYPE(su_info_p) == SU_TYPE_CMD)
		return;

	/* ups.status and {ups, Lx, outlet, outlet.group}.alarm have special
	 * handling, not here! */
	if ((strcasecmp(su_info_p->info_type, "ups.status"))
		&& (strcasecmp(strrchr(su_info_p->info_type, '.'), ".alarm")))
	{
		if (value != NULL)
			dstate_setinfo(su_info_p->info_type, "%s", value);
		else
			dstate_setinfo(su_info_p->info_type, "%s", su_info_p->dfl);

		dstate_setflags(su_info_p->info_type, su_info_p->info_flags);
		dstate_setaux(su_info_p->info_type, su_info_p->info_len);

		/* Commit the current value, to avoid staleness with huge
		 * data collections on slow devices */
		 dstate_dataok();
	}
}

void su_status_set(snmp_info_t *su_info_p, long value)
{
	const char *info_value = NULL;

	upsdebugx(2, "SNMP UPS driver : entering su_status_set()");

	if ((info_value = su_find_infoval(su_info_p->oid2info, value)) != NULL)
	{
		if (strcmp(info_value, "")) {
			status_set(info_value);
		}
	}
	/* TODO: else */
}

void su_alarm_set(snmp_info_t *su_info_p, long value)
{
	const char *info_value = NULL;
	char alarm_info_value[SU_LARGEBUF];
	/* number of the outlet or phase */
	int item_number = -1;

	upsdebugx(2, "SNMP UPS driver : entering su_alarm_set(%s)", su_info_p->info_type);

	if ((info_value = su_find_infoval(su_info_p->oid2info, value)) != NULL)
	{
		if (strcmp(info_value, "")) {
			/* Special handling for outlet & outlet groups alarms */
			if ((su_info_p->flags & SU_OUTLET)
				|| (su_info_p->flags & SU_OUTLET_GROUP)) {
				/* Extract template number */
				item_number = extract_template_number(su_info_p->flags, su_info_p->info_type);

				/* Inject in the alarm string */
				sprintf(alarm_info_value, info_value, 
					(su_info_p->flags & SU_OUTLET_GROUP)?"outlet group ":"outlet ",
					item_number);
				info_value = &alarm_info_value[0];
			}
			/* Special handling for phase alarms
			 * Note that SU_*PHASE flags are cleared, so match the 'Lx'
			 * start of path */
			if (su_info_p->info_type[0] == 'L') {
				/* Extract phase number */
				item_number = atoi(strchr(su_info_p->info_type, 'L')+1);

				/* Inject in the alarm string */
				sprintf(alarm_info_value, info_value, "phase L", item_number);
				info_value = &alarm_info_value[0];
			}

			/* Set the alarm value */
			alarm_set(info_value);
		}
	}
	/* TODO: else */
}

/* find info element definition in my info array. */
snmp_info_t *su_find_info(const char *type)
{
	snmp_info_t *su_info_p;

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)
		if (!strcasecmp(su_info_p->info_type, type)) {
			upsdebugx(3, "su_find_info: \"%s\" found", type);
			return su_info_p;
		}

	upsdebugx(3, "su_find_info: unknown info type (%s)", type);
	return NULL;
}

/* Try to find the MIB using sysOID matching.
 * Return a pointer to a mib2nut definition if found, NULL otherwise */
mib2nut_info_t *match_sysoid()
{
	char sysOID_buf[LARGEBUF];
	oid device_sysOID[MAX_OID_LEN];
	size_t device_sysOID_len = MAX_OID_LEN;
	oid mib2nut_sysOID[MAX_OID_LEN];
	size_t mib2nut_sysOID_len = MAX_OID_LEN;
	int i;

	/* Retrieve sysOID value of this device */
	if (nut_snmp_get_str(SYSOID_OID, sysOID_buf, sizeof(sysOID_buf), NULL))
	{
		upsdebugx(1, "match_sysoid: device sysOID value = %s", sysOID_buf);

		/* Build OIDs for comparison */
		if (!read_objid(sysOID_buf, device_sysOID, &device_sysOID_len))
		{
			upsdebugx(2, "match_sysoid: can't build device_sysOID %s: %s",
				sysOID_buf, snmp_api_errstring(snmp_errno));

			return FALSE;
		}

		/* Now, iterate on mib2nut definitions */
		for (i = 0; mib2nut[i] != NULL; i++)
		{
			upsdebugx(1, "match_sysoid: checking MIB %s", mib2nut[i]->mib_name);

			if (mib2nut[i]->sysOID == NULL)
				continue;

			/* Clear variables */
			memset(mib2nut_sysOID, 0, MAX_OID_LEN);
			mib2nut_sysOID_len = MAX_OID_LEN;

			if (!read_objid(mib2nut[i]->sysOID, mib2nut_sysOID, &mib2nut_sysOID_len))
			{
				upsdebugx(2, "match_sysoid: can't build OID %s: %s",
					sysOID_buf, snmp_api_errstring(snmp_errno));

				/* Try to continue anyway! */
				continue;
			}
			/* Now compare these */
			upsdebugx(1, "match_sysoid: comparing %s with %s", sysOID_buf, mib2nut[i]->sysOID);
			if (!netsnmp_oid_equals(device_sysOID, device_sysOID_len, mib2nut_sysOID, mib2nut_sysOID_len))
			{
				upsdebugx(2, "match_sysoid: sysOID matches MIB '%s'!", mib2nut[i]->mib_name);
				return mib2nut[i];
			}
		}
		/* Yell all to call for user report */
		upslogx(LOG_ERR, "No matching MIB found for sysOID '%s'!\n" \
			"Please report it to NUT developers, with an 'upsc' output for your device.\n" \
			"Going back to the classic MIB detection method.",
			sysOID_buf);
	}
	else
		upsdebugx(2, "Can't get sysOID value");

	return NULL;
}

/* Load the right snmp_info_t structure matching mib parameter */
bool_t load_mib2nut(const char *mib)
{
	int	i;
	char	buf[LARGEBUF];
	mib2nut_info_t *m2n = NULL;

	upsdebugx(2, "SNMP UPS driver : entering load_mib2nut(%s)", mib);

	/* First, try to match against sysOID, if no MIB was provided.
	 * This should speed up init stage
	 * (Note: sysOID points the device main MIB entry point) */
	if (!strcmp(mib, "auto"))
	{
		upsdebugx(1, "trying the new match_sysoid() method");
		m2n = match_sysoid();
	}

	/* Otherwise, revert to the classic method */
	if (m2n == NULL)
	{
		for (i = 0; mib2nut[i] != NULL; i++) {
			/* Is there already a MIB name provided? */
			if (strcmp(mib, "auto") && strcmp(mib, mib2nut[i]->mib_name)) {
				continue;
			}
			upsdebugx(1, "load_mib2nut: trying classic method with '%s' mib", mib2nut[i]->mib_name);

			/* Classic method: test an OID specific to this MIB */
			if (!nut_snmp_get_str(mib2nut[i]->oid_auto_check, buf, sizeof(buf), NULL)) {
				continue;
			}
			/* MIB found */
			m2n = mib2nut[i];
			break;
		}
	}

	/* Store the result, if any */
	if (m2n != NULL)
	{
		snmp_info = m2n->snmp_info;
		OID_pwr_status = m2n->oid_pwr_status;
		mibname = m2n->mib_name;
		mibvers = m2n->mib_version;
		alarms_info = m2n->alarms_info;
		upsdebugx(1, "load_mib2nut: using %s mib", mibname);
		return TRUE;
	}

	/* Did we find something or is it really an unknown mib */
	if (strcmp(mib, "auto") != 0) {
		fatalx(EXIT_FAILURE, "Unknown mibs value: %s", mib);
	} else {
		fatalx(EXIT_FAILURE, "No supported device detected");
	}
}

/* find the OID value matching that INFO_* value */
long su_find_valinfo(info_lkp_t *oid2info, const char* value)
{
	info_lkp_t *info_lkp;

	for (info_lkp = oid2info; (info_lkp != NULL) &&
		(strcmp(info_lkp->info_value, "NULL")); info_lkp++) {

		if (!(strcmp(info_lkp->info_value, value))) {
			upsdebugx(1, "su_find_valinfo: found %s (value: %s)",
					info_lkp->info_value, value);

			return info_lkp->oid_value;
		}
	}
	upsdebugx(1, "su_find_valinfo: no matching INFO_* value for this OID value (%s)", value);
	return -1;
}

/* find the INFO_* value matching that OID value */
const char *su_find_infoval(info_lkp_t *oid2info, long value)
{
	info_lkp_t *info_lkp;

	for (info_lkp = oid2info; (info_lkp != NULL) &&
		(strcmp(info_lkp->info_value, "NULL")) && (info_lkp->info_value != NULL); info_lkp++) {

		if (info_lkp->oid_value == value) {
			upsdebugx(1, "su_find_infoval: found %s (value: %ld)",
					info_lkp->info_value, value);

			return info_lkp->info_value;
		}
	}
	upsdebugx(1, "su_find_infoval: no matching INFO_* value for this OID value (%ld)", value);
	return NULL;
}

static void disable_competition(snmp_info_t *entry)
{
	snmp_info_t	*p;

	for(p=snmp_info; p->info_type!=NULL; p++) {
		if(p!=entry && !strcmp(p->info_type, entry->info_type)) {
			upsdebugx(2, "disable_competition: disabling %s %s",
					p->info_type, p->OID);
			p->flags &= ~SU_FLAG_OK;
		}
	}
}

/***********************************************************************
 * Template handling functions
 **********************************************************************/

/* Instantiate an snmp_info_t from a template.
 * Useful for outlet and outlet.group templates.
 * Note: remember to adapt info_type, OID and optionaly dfl */
snmp_info_t *instantiate_info(snmp_info_t *info_template, snmp_info_t *new_instance)
{
	/* sanity check */
	if (info_template == NULL)
		return NULL;

	if (new_instance == NULL)
		new_instance = (snmp_info_t *)xmalloc(sizeof(snmp_info_t));

	new_instance->info_type = (char *)xmalloc(SU_INFOSIZE);
	if (info_template->OID != NULL)
		new_instance->OID = (char *)xmalloc(SU_INFOSIZE);
	else
		new_instance->OID = NULL;
	new_instance->info_flags = info_template->info_flags;
	new_instance->info_len = info_template->info_len;
	/* FIXME: check if we need to adapt this one... */
	new_instance->dfl = info_template->dfl;
	new_instance->flags = info_template->flags;
	new_instance->oid2info = info_template->oid2info;
	new_instance->setvar = info_template->setvar;

	return new_instance;
}

/* Free a dynamically allocated snmp_info_t.
 * Useful for outlet and outlet.group templates */
void free_info(snmp_info_t *su_info_p)
{
	/* sanity check */
	if (su_info_p == NULL)
		return;

	if (su_info_p->info_type != NULL)
		free ((char *)su_info_p->info_type);

	if (su_info_p->OID != NULL)
		free ((char *)su_info_p->OID);

	free (su_info_p);
}

/* return the base SNMP index (0 or 1) to start template iteration on
 * the MIB, based on a test using a template OID */
int base_snmp_template_index(const char *OID_template)
{
	int base_index = template_index_base;
	char test_OID[SU_INFOSIZE];

	if (template_index_base == -1)
	{
		/* not initialised yet */
		for (base_index = 0 ; base_index < 2 ; base_index++) {
			sprintf(test_OID, OID_template, base_index);
			if (nut_snmp_get(test_OID) != NULL)
				break;
		}
		template_index_base = base_index;
	}
	upsdebugx(3, "base_snmp_template_index: %i", template_index_base);
	return base_index;
}

/* return the NUT offset (increment) based on template_index_base
 * ie (template_index_base == 0) => increment +1
 *    (template_index_base == 1) => increment +0 */
int base_nut_template_offset(void)
{
	return (template_index_base==0)?1:0;
}

/* Try to determine the number of items (outlets, outlet groups, ...),
 * using a template definition. Walk through the template until we can't
 * get anymore values. I.e., if we can iterate up to 8 item, return 8 */
static int guestimate_template_count(const char *OID_template)
{
	int base_index = 0;
	char test_OID[SU_INFOSIZE];
	int base_count;

	upsdebugx(1, "guestimate_template_count(%s)", OID_template);

	/* Determine if OID index starts from 0 or 1? */
	sprintf(test_OID, OID_template, base_index);
	if (nut_snmp_get(test_OID) == NULL)
		base_index++;

	/* Now, actually iterate */
	for (base_count = 0 ;  ; base_count++) {
		sprintf(test_OID, OID_template, base_index + base_count);
		if (nut_snmp_get(test_OID) == NULL)
			break;
	}

	upsdebugx(3, "guestimate_template_count: %i", base_count);
	return base_count;
}

/* Process template definition, instantiate and get data or register
 * command
 * type: outlet, outlet.group */
bool_t process_template(int mode, const char* type, snmp_info_t *su_info_p)
{
	/* Default to TRUE, and leave to get_and_process_data() to set
	 * to FALSE when actually getting data from devices, to avoid false
	 * negative with server side data */
	bool_t status = TRUE;
	int cur_template_number = 1;
	int cur_nut_index = 0;
	int template_count = 0;
	snmp_info_t cur_info_p;
	char template_count_var[SU_BUFSIZE];

	upsdebugx(1, "%s template definition found (%s)...", type, su_info_p->info_type);

	sprintf(template_count_var, "%s.count", type);

	if(dstate_getinfo(template_count_var) == NULL) {
		/* FIXME: should we disable it?
		 * su_info_p->flags &= ~SU_FLAG_OK;
		 * or rely on guestimation? */
		template_count = guestimate_template_count(su_info_p->OID);
		/* Publish the count estimation */
		dstate_setinfo(template_count_var, "%i", template_count);
	}
	else {
		template_count = atoi(dstate_getinfo(template_count_var));
	}

	/* Only instantiate templates if needed! */
	if (template_count > 0) {
		/* general init of data using the template */
		instantiate_info(su_info_p, &cur_info_p);

		for (cur_template_number = base_snmp_template_index(su_info_p->OID) ;
				cur_template_number < (template_count + base_snmp_template_index(su_info_p->OID)) ;
				cur_template_number++)
		{
			cur_nut_index = cur_template_number + base_nut_template_offset();
			sprintf((char*)cur_info_p.info_type, su_info_p->info_type,
					cur_nut_index);

			/* check if default value is also a template */
			if ((cur_info_p.dfl != NULL) &&
				(strstr(su_info_p->dfl, "%i") != NULL)) {
				cur_info_p.dfl = (char *)xmalloc(SU_INFOSIZE);
				sprintf((char *)cur_info_p.dfl, su_info_p->dfl, cur_nut_index);
			}

			if (cur_info_p.OID != NULL) {
				sprintf((char *)cur_info_p.OID, su_info_p->OID, cur_template_number);

				/* add instant commands to the info database. */
				if (SU_TYPE(su_info_p) == SU_TYPE_CMD) {
					/* FIXME: only add if "su_ups_get(cur_info_p) == TRUE" */
					if (mode == SU_WALKMODE_INIT)
						dstate_addcmd(cur_info_p.info_type);
				}
				else /* get and process this data */
					status = get_and_process_data(mode, &cur_info_p);
			} else {
				/* server side (ABSENT) data */
				su_setinfo(&cur_info_p, NULL);
			}
			/* set back the flag */
			su_info_p->flags = cur_info_p.flags;
		}
		free((char*)cur_info_p.info_type);
		if (cur_info_p.OID != NULL)
			free((char*)cur_info_p.OID);
		if ((cur_info_p.dfl != NULL) &&
			(strstr(su_info_p->dfl, "%i") != NULL))
			free((char*)cur_info_p.dfl);
	}
	else {
		upsdebugx(1, "No %s present, discarding template definition...", type);
	}
	return status;
}

/* Return the type of template, according to a variable name.
 * Return: SU_OUTLET_GROUP, SU_OUTLET or 0 if not a template */
int get_template_type(const char* varname)
{
	/* Check if it is outlet / outlet.group */
	if (!strncmp(varname, "outlet.group", 12)) {
		return SU_OUTLET_GROUP;
	}
	else if (!strncmp(varname, "outlet", 6)) {
		return SU_OUTLET_GROUP;
	}
	else {
		upsdebugx(2, "Unknow template type: %s", varname);
		return 0;
	}
}

/* Extract the id number of an instantiated template.
 * Example: return '1' for type = 'outlet.1.desc', -1 if unknown */
int extract_template_number(int template_type, const char* varname)
{
	const char* item_number_ptr = NULL;
	int item_number = -1;

	if (template_type & SU_OUTLET_GROUP)
		item_number_ptr = &varname[12];
	else if (template_type & SU_OUTLET)
		item_number_ptr = &varname[6];
	else
		return -1;

	item_number = atoi(++item_number_ptr);
	upsdebugx(3, "extract_template_number: item %i", item_number);
	return item_number;
}

/* Extract the id number of a template from a variable name.
 * Example: return '1' for type = 'outlet.1.desc' */
int extract_template_number_from_snmp_info_t(const char* varname)
{
	return extract_template_number(get_template_type(varname), varname);
}

/* process a single data from a walk */
bool_t get_and_process_data(int mode, snmp_info_t *su_info_p)
{
	bool_t status = FALSE;

	upsdebugx(1, "getting data: %s (%s)", su_info_p->info_type, su_info_p->OID);

	/* ok, update this element. */
	status = su_ups_get(su_info_p);

	/* set stale flag if data is stale, clear if not. */
	if (status == TRUE) {
		if (su_info_p->flags & SU_FLAG_STALE) {
			upslogx(LOG_INFO, "[%s] snmp_ups_walk: data resumed for %s",
				upsname?upsname:device_name, su_info_p->info_type);
			su_info_p->flags &= ~SU_FLAG_STALE;
		}
		if(su_info_p->flags & SU_FLAG_UNIQUE) {
			/* We should be the only provider of this */
			disable_competition(su_info_p);
			su_info_p->flags &= ~SU_FLAG_UNIQUE;
		}
		dstate_dataok();
	} else {
		if (mode == SU_WALKMODE_INIT) {
			/* handle unsupported vars */
			su_info_p->flags &= ~SU_FLAG_OK;
		} else	{
			if (!(su_info_p->flags & SU_FLAG_STALE)) {
				upslogx(LOG_INFO, "[%s] snmp_ups_walk: data stale for %s",
					upsname?upsname:device_name, su_info_p->info_type);
				su_info_p->flags |= SU_FLAG_STALE;
			}
			dstate_datastale();
		}
	}
	return status;
}

/* walk ups variables and set elements of the info array. */
bool_t snmp_ups_walk(int mode)
{
	static unsigned long iterations = 0;
	snmp_info_t *su_info_p;
	bool_t status = FALSE;

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++) {

		/* Check if we are asked to stop (reactivity++) */
		if (exit_flag != 0)
			return TRUE;

		/* skip instcmd, not linked to outlets */
		if ((SU_TYPE(su_info_p) == SU_TYPE_CMD)
			&& !(su_info_p->flags & SU_OUTLET)
			&& !(su_info_p->flags & SU_OUTLET_GROUP)) {
			upsdebugx(1, "SU_CMD_MASK => %s", su_info_p->OID);
			continue;
		}
		/* skip elements we shouldn't show */
		if (!(su_info_p->flags & SU_FLAG_OK))
			continue;

		/* skip static elements in update mode */
		if (mode == SU_WALKMODE_UPDATE &&
				su_info_p->flags & SU_FLAG_STATIC)
			continue;

		/* Set default value if we cannot fetch it */
		/* and set static flag on this element.
		 * Not applicable to outlets (need SU_FLAG_STATIC tagging) */
		if ((su_info_p->flags & SU_FLAG_ABSENT)
			&& !(su_info_p->flags & SU_OUTLET)
			&& !(su_info_p->flags & SU_OUTLET_GROUP)) {
			if (mode == SU_WALKMODE_INIT) {
				if (su_info_p->dfl) {
					/* Set default value if we cannot fetch it from ups. */
					su_setinfo(su_info_p, NULL);
				}
				su_info_p->flags |= SU_FLAG_STATIC;
			}
			continue;
		}

		/* check stale elements only on each PN_STALE_RETRY iteration. */
		if ((su_info_p->flags & SU_FLAG_STALE) &&
				(iterations % SU_STALE_RETRY) != 0)
			continue;

		/* Filter 1-phase Vs 3-phase according to {input,output}.phase.
		 * Non matching items are disabled, and flags are cleared at
		 * init time */
		if (su_info_p->flags & SU_INPHASES) {
			upsdebugx(1, "Check input_phases (%i)", input_phases);
			if (input_phases == 0) {
				/* FIXME: to get from input.phases
				 * this would avoid the use of the SU_FLAG_SETINT flag
				 * and potential human-error to not declare the right way.
				 * It would also free the slot for SU_OUTLET_GROUP */
				continue;
			}
			if (su_info_p->flags & SU_INPUT_1) {
				if (input_phases == 1) {
					upsdebugx(1, "input_phases is 1");
					su_info_p->flags &= ~SU_INPHASES;
				} else {
					upsdebugx(1, "input_phases is not 1");
					su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			} else if (su_info_p->flags & SU_INPUT_3) {
			    if (input_phases == 3) {
					upsdebugx(1, "input_phases is 3");
					su_info_p->flags &= ~SU_INPHASES;
				} else {
					upsdebugx(1, "input_phases is not 3");
					su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			} else {
				upsdebugx(1, "input_phases is %d", input_phases);
			}
		}

		if (su_info_p->flags & SU_OUTPHASES) {
			upsdebugx(1, "Check output_phases");
			if (output_phases == 0) {
				/* FIXME: same as for input_phases */
				continue;
			}
			if (su_info_p->flags & SU_OUTPUT_1) {
				if (output_phases == 1) {
					upsdebugx(1, "output_phases is 1");
					su_info_p->flags &= ~SU_OUTPHASES;
				} else {
					upsdebugx(1, "output_phases is not 1");
					su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			} else if (su_info_p->flags & SU_OUTPUT_3) {
				if (output_phases == 3) {
					upsdebugx(1, "output_phases is 3");
					su_info_p->flags &= ~SU_OUTPHASES;
				} else {
					upsdebugx(1, "output_phases is not 3");
					su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			} else {
				upsdebugx(1, "output_phases is %d", output_phases);
			}
		}

		if (su_info_p->flags & SU_BYPPHASES) {
			upsdebugx(1, "Check bypass_phases");
			if (bypass_phases == 0) {
				/* FIXME: same as for input_phases */
				continue;
			}
			if (su_info_p->flags & SU_BYPASS_1) {
				if (bypass_phases == 1) {
					upsdebugx(1, "bypass_phases is 1");
					su_info_p->flags &= ~SU_BYPPHASES;
				} else {
					upsdebugx(1, "bypass_phases is not 1");
					su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			} else if (su_info_p->flags & SU_BYPASS_3) {
				if (input_phases == 3) {
					upsdebugx(1, "bypass_phases is 3");
					su_info_p->flags &= ~SU_BYPPHASES;
				} else {
					upsdebugx(1, "bypass_phases is not 3");
					su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			} else {
				upsdebugx(1, "bypass_phases is %d", bypass_phases);
			}
		}

		/* process outlet template definition */
		if (su_info_p->flags & SU_OUTLET) {
			status = process_template(mode, "outlet", su_info_p);
		}
		else if (su_info_p->flags & SU_OUTLET_GROUP) {
			status = process_template(mode, "outlet.group", su_info_p);
		}
		else {
			/* get and process this data */
			status = get_and_process_data(mode, su_info_p);
		}
	}	/* for (su_info_p... */

	iterations++;
	return status;
}

bool_t su_ups_get(snmp_info_t *su_info_p)
{
	static char buf[SU_INFOSIZE];
	bool_t status;
	long value;
	struct snmp_pdu ** pdu_array;
	struct snmp_pdu * current_pdu;
	alarms_info_t * alarms;
	int index = 0;

	upsdebugx(2, "su_ups_get: %s %s", su_info_p->info_type, su_info_p->OID);

	if (!strcasecmp(su_info_p->info_type, "ups.status")) {

		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE)
		{
			su_status_set(su_info_p, value);
			upsdebugx(2, "=> value: %ld", value);
		}
		else upsdebugx(2, "=> Failed");

		return status;
	}

	/* Handle 'ups.alarm', 'outlet.n.alarm' and 3phase 'Lx.alarm',
	 * nothing else! */
	if (!strcmp(strrchr(su_info_p->info_type, '.'), ".alarm")) {

		upsdebugx(2, "Processing alarm: %s", su_info_p->info_type);

		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE)
		{
			su_alarm_set(su_info_p, value);
			upsdebugx(2, "=> value: %ld", value);
		}
		else upsdebugx(2, "=> Failed");

		return status;
	}

	/* FIXME: this is not compliant nor coherent with ups.alarm and
	 * MUST be reworked!
	 * Only present in powerware-mib.c */
	if (!strcasecmp(su_info_p->info_type, "ups.alarms")) {
		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE) {
			upsdebugx(2, "=> value: %ld", value);
			if( value > 0 ) {
				pdu_array = nut_snmp_walk(su_info_p->OID,INT_MAX);
				if(pdu_array == NULL) {
					upsdebugx(2, "=> Walk failed");
					return FALSE;
				}

				current_pdu = pdu_array[index];
				while(current_pdu) {
					decode_str(current_pdu,buf,sizeof(buf),NULL);
					alarms = alarms_info;
					while( alarms->OID ) {
						if(!strcmp(buf+1,alarms_info->OID)) {
							upsdebugx(3, "Alarm OID %s found => %s", alarms->OID, alarms->info_value);
							status_set(alarms->info_value);
							break;
						}
						alarms++;
					}
					index++;
					current_pdu = pdu_array[index];
				}
				nut_snmp_free(pdu_array);
			}
		}
		else {
			upsdebugx(2, "=> Failed");
		}

		return status;
	}

	/* another special case */
	if (!strcasecmp(su_info_p->info_type, "ambient.temperature")) {
		float temp=0;

		status = nut_snmp_get_int(su_info_p->OID, &value);

		if(status != TRUE) {
			return status;
		}

		/* only do this if using the IEM sensor */
		if (!strcmp(su_info_p->OID, APCC_OID_IEM_TEMP)) {
			int	su;
			long	units;

			su = nut_snmp_get_int(APCC_OID_IEM_TEMP_UNIT, &units);

			/* no response, or units == F */
			if ((su == FALSE) || (units == APCC_IEM_FAHRENHEIT))
				temp = (value - 32) / 1.8;
			else
				temp = value;
		}
		else {
			temp = value * su_info_p->info_len;
		}

		snprintf(buf, sizeof(buf), "%.1f", temp);
		su_setinfo(su_info_p, buf);

		return TRUE;
	}

	if (su_info_p->info_flags & ST_FLAG_STRING) {
		status = nut_snmp_get_str(su_info_p->OID, buf, sizeof(buf), su_info_p->oid2info);
	} else {
		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE) {
			if (su_info_p->flags&SU_FLAG_NEGINVALID && value<0) {
				su_info_p->flags &= ~SU_FLAG_OK;
				if(su_info_p->flags&SU_FLAG_UNIQUE) {
					disable_competition(su_info_p);
					su_info_p->flags &= ~SU_FLAG_UNIQUE;
				}
				return FALSE;
			}
			if (su_info_p->flags & SU_FLAG_SETINT) {
			    	upsdebugx(1, "setvar %s", su_info_p->OID);
			    	*su_info_p->setvar = value;
			}
			snprintf(buf, sizeof(buf), "%.2f", value * su_info_p->info_len);
		}
	}

	if (status == TRUE) {
		su_setinfo(su_info_p, buf);
		upsdebugx(2, "=> value: %s", buf);
	}
	else
		upsdebugx(2, "=> Failed");

	return status;
}

/* set r/w INFO_ element to a value. */
int su_setvar(const char *varname, const char *val)
{
	snmp_info_t *su_info_p = NULL;
	bool_t status;
	int retval = STAT_SET_FAILED;
	long value = -1;
	/* normal (default), outlet, or outlet group variable */
	int vartype = get_template_type(varname);

	upsdebugx(2, "entering su_setvar(%s, %s)", varname, val);

	/* Check if it is outlet / outlet.group */
	if (strncmp(varname, "outlet", 6))
		su_info_p = su_find_info(varname);
	else {
		snmp_info_t *tmp_info_p;
		/* Point the outlet or outlet group number in the string */
		const char *item_number_ptr = NULL;
		/* Store the target outlet or group number */
		int item_number = extract_template_number_from_snmp_info_t(varname);
		/* Store the total number of outlets or outlet groups */
		int total_items = -1;

		/* Check if it is outlet / outlet.group */
		if (vartype == SU_OUTLET_GROUP) {
			total_items = atoi(dstate_getinfo("outlet.group.count"));
			item_number_ptr = &varname[12];
		}
		else {
			total_items = atoi(dstate_getinfo("outlet.count"));
			item_number_ptr = &varname[6];
		}

		item_number = atoi(++item_number_ptr);
		upsdebugx(3, "su_setvar: item %i / %i", item_number, total_items);

		/* ensure the item number is supported (filtered upstream though)! */
		if (item_number > total_items) {
			/* out of bound item number */
			upsdebugx(2, "su_setvar: item is out of bound (%i / %i)",
				item_number, total_items);
			return STAT_SET_INVALID;
		}
		/* find back the item template */
		char *item_varname = (char *)xmalloc(SU_INFOSIZE);
		sprintf(item_varname, "%s.%s%s",
				(vartype == SU_OUTLET)?"outlet":"outlet.group",
				"%i", strchr(item_number_ptr++, '.'));

		upsdebugx(3, "su_setvar: searching for template\"%s\"", item_varname);
		tmp_info_p = su_find_info(item_varname);
		free(item_varname);

		/* for an snmp_info_t instance */
		su_info_p = instantiate_info(tmp_info_p, su_info_p);

		/* check if default value is also a template */
		if ((su_info_p->dfl != NULL) &&
			(strstr(tmp_info_p->dfl, "%i") != NULL)) {
			su_info_p->dfl = (char *)xmalloc(SU_INFOSIZE);
			sprintf((char *)su_info_p->dfl, tmp_info_p->dfl,
				item_number - base_nut_template_offset());
		}
		/* adapt the OID */
		if (su_info_p->OID != NULL) {
			sprintf((char *)su_info_p->OID, tmp_info_p->OID,
				item_number - base_nut_template_offset());
		}
		/* else, don't return STAT_SET_INVALID since we can be setting
		 * a server side variable! */

		/* adapt info_type */
		if (su_info_p->info_type != NULL)
			sprintf((char *)su_info_p->info_type, "%s", varname);
	}

	if (!su_info_p || !su_info_p->info_type || !(su_info_p->flags & SU_FLAG_OK)) {
		upsdebugx(2, "su_setvar: info element unavailable %s", varname);

		/* Free template (outlet and outlet.group) */
		if (vartype != 0)
			free_info(su_info_p);

		return STAT_SET_UNKNOWN;
	}

	if (!(su_info_p->info_flags & ST_FLAG_RW) || su_info_p->OID == NULL) {
		upsdebugx(2, "su_setvar: not writable %s", varname);

		/* Free template (outlet and outlet.group) */
		if (vartype != 0)
			free_info(su_info_p);

		return STAT_SET_INVALID;
	}

	/* set value into the device */
	if (su_info_p->info_flags & ST_FLAG_STRING) {
		status = nut_snmp_set_str(su_info_p->OID, val);
	} else {
		/* non string data may imply a value lookup */
		if (su_info_p->oid2info) {
			value = su_find_valinfo(su_info_p->oid2info, val);
		}
		else {
			/* Convert value and apply multiplier */
			value = atof(val) / su_info_p->info_len;
		}
		/* Actually apply the new value */
		status = nut_snmp_set_int(su_info_p->OID, value);
	}

	if (status == FALSE)
		upsdebugx(1, "su_setvar: cannot set value %s for %s", val, su_info_p->OID);
	else {
		retval = STAT_SET_HANDLED;
		upsdebugx(1, "su_setvar: successfully set %s to \"%s\"", varname, val);

		/* update info array */
		su_setinfo(su_info_p, val);
	}
	/* Free template (outlet and outlet.group) */
	if (vartype != 0)
		free_info(su_info_p);

	return retval;
}

/* process instant command and take action. */
int su_instcmd(const char *cmdname, const char *extradata)
{
	snmp_info_t *su_info_p = NULL;
	int status;
	int retval = STAT_INSTCMD_FAILED;
	int cmd_offset = 0;

	upsdebugx(2, "entering su_instcmd(%s, %s)", cmdname, extradata);

	/* FIXME: this should only apply if strchr(%)! */
	if (strncmp(cmdname, "outlet", 6)) {
		su_info_p = su_find_info(cmdname);
	}
	else {
		snmp_info_t *tmp_info_p;
		char *outlet_number_ptr = strchr(cmdname, '.');
		int outlet_number = atoi(++outlet_number_ptr);
		if (dstate_getinfo("outlet.count") == NULL) {
			upsdebugx(2, "su_instcmd: can't get outlet.count...");
			return STAT_INSTCMD_UNKNOWN;
		}

		upsdebugx(3, "su_instcmd: outlet %i / %i", outlet_number,
			atoi(dstate_getinfo("outlet.count")));

		/* ensure the outlet number is supported! */
		if (outlet_number > atoi(dstate_getinfo("outlet.count"))) {
			/* out of bound outlet number */
			upsdebugx(2, "su_instcmd: outlet is out of bound (%i / %s)",
				outlet_number, dstate_getinfo("outlet.count"));
			return STAT_INSTCMD_INVALID;
		}

		/* find back the outlet template */
		char *outlet_cmdname = (char *)xmalloc(SU_INFOSIZE);
		sprintf(outlet_cmdname, "outlet.%s%s", "%i", strchr(outlet_number_ptr++, '.'));
		upsdebugx(3, "su_instcmd: searching for template\"%s\"", outlet_cmdname);
		tmp_info_p = su_find_info(outlet_cmdname);
		free(outlet_cmdname);

		/* for an snmp_info_t instance */
		su_info_p = instantiate_info(tmp_info_p, su_info_p);

		/* check if default value is also a template */
		if ((su_info_p->dfl != NULL) &&
			(strstr(tmp_info_p->dfl, "%i") != NULL)) {
			su_info_p->dfl = (char *)xmalloc(SU_INFOSIZE);
			sprintf((char *)su_info_p->dfl, tmp_info_p->dfl,
				outlet_number - base_nut_template_offset());
		}
		/* adapt the OID */
		if (su_info_p->OID != NULL) {
			/* Workaround buggy Eaton Pulizzi implementation
			 * which have different offsets index for data & commands! */
			if (su_info_p->flags & SU_CMD_OFFSET) {
				upsdebugx(3, "Adding command offset");
				cmd_offset++;
			}

			sprintf((char *)su_info_p->OID, tmp_info_p->OID,
				outlet_number - base_nut_template_offset() + cmd_offset);
		} else {
			free_info(su_info_p);
			return STAT_INSTCMD_UNKNOWN;
		}
	}

	/* Sanity check */
	if (!su_info_p || !su_info_p->info_type || !(su_info_p->flags & SU_FLAG_OK)) {

		/* Check for composite commands */
		if (!strcasecmp(cmdname, "load.on")) {
			return su_instcmd("load.on.delay", "0");
		}

		if (!strcasecmp(cmdname, "load.off")) {
			return su_instcmd("load.off.delay", "0");
		}

		if (!strcasecmp(cmdname, "shutdown.return")) {
			int	ret;

			/* Ensure "ups.start.auto" is set to "yes", if supported */
			if (dstate_getinfo("ups.start.auto")) {
				su_setvar("ups.start.auto", "yes");
			}

			ret = su_instcmd("load.on.delay", dstate_getinfo("ups.delay.start"));
			if (ret != STAT_INSTCMD_HANDLED) {
				return ret;
			}

			return su_instcmd("load.off.delay", dstate_getinfo("ups.delay.shutdown"));
		}

		if (!strcasecmp(cmdname, "shutdown.stayoff")) {
			int	ret;

			/* Ensure "ups.start.auto" is set to "no", if supported */
			if (dstate_getinfo("ups.start.auto")) {
				su_setvar("ups.start.auto", "no");
			}

			ret = su_instcmd("load.on.delay", "-1");
			if (ret != STAT_INSTCMD_HANDLED) {
				return ret;
			}

			return su_instcmd("load.off.delay", dstate_getinfo("ups.delay.shutdown"));
		}

		upsdebugx(2, "su_instcmd: %s unavailable", cmdname);

		if (!strncmp(cmdname, "outlet", 6))
			free_info(su_info_p);

		return STAT_INSTCMD_UNKNOWN;
	}

	/* set value, using the provided one, or the default one otherwise */
	if (su_info_p->info_flags & ST_FLAG_STRING) {
		status = nut_snmp_set_str(su_info_p->OID, extradata ? extradata : su_info_p->dfl);
	} else {
		status = nut_snmp_set_int(su_info_p->OID, extradata ? atoi(extradata) : su_info_p->info_len);
	}

	if (status == FALSE)
		upsdebugx(1, "su_instcmd: cannot set value for %s", cmdname);
	else {
		retval = STAT_INSTCMD_HANDLED;
		upsdebugx(1, "su_instcmd: successfully sent command %s", cmdname);
	}

	if (!strncmp(cmdname, "outlet", 6))
		free_info(su_info_p);

	return retval;
}

/* FIXME: the below functions can be removed since these were for loading
 * the mib2nut information from a file instead of the .h definitions... */
/* return 1 if usable, 0 if not */
static int parse_mibconf_args(int numargs, char **arg)
{
	bool_t ret;

	/* everything below here uses up through arg[1] */
	if (numargs < 6)
		return 0;

	/* <info type> <info flags> <info len> <OID name> <default value> <value lookup> */

	/* special case for setting some OIDs value at driver startup */
	if (!strcmp(arg[0], "init")) {
		/* set value. */
		if (!strcmp(arg[1], "str")) {
			ret = nut_snmp_set_str(arg[3], arg[4]);
		} else {
			ret = nut_snmp_set_int(arg[3], strtol(arg[4], NULL, 0));
		}

		if (ret == FALSE)
			upslogx(LOG_ERR, "su_setvar: cannot set value %s for %s", arg[4], arg[3]);
		else
			upsdebugx(1, "su_setvar: successfully set %s to \"%s\"", arg[0], arg[4]);

		return 1;
	}

	/* TODO: create the lookup table */
	upsdebugx(2, "%s, %s, %s, %s, %s, %s", arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);

	return 1;
}
/* called for fatal errors in parseconf like malloc failures */
static void mibconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf (*mib.conf): %s", errmsg);
}
/* load *mib.conf into an snmp_info_t structure */
void read_mibconf(char *mib)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	upsdebugx(2, "SNMP UPS driver : entering read_mibconf(%s)", mib);

	snprintf(fn, sizeof(fn), "%s/snmp/%s.conf", CONFPATH, mib);

	pconf_init(&ctx, mibconf_err);

	if (!pconf_file_begin(&ctx, fn))
		fatalx(EXIT_FAILURE, "%s", ctx.errmsg);

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_mibconf_args(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg),
				"mib.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s",
					ctx.arglist[i]);

			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}
	pconf_finish(&ctx);
}
