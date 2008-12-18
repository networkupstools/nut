/*  snmp-ups.c - NUT Meta SNMP driver (support different MIBS)
 *
 *  Based on NetSNMP API (Simple Network Management Protocol V1-2)
 *
 *  Copyright (C) 
 *   2002-2008  Arnaud Quette <arnaud.quette@free.fr>
 *   2002-2006	Dmitry Frolov <frolov@riss-telecom.ru>
 *  			J.W. Hoogervorst <jeroen@hoogervorst.net>
 *  			Niels Baggesen <niels@baggesen.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://opensource.mgeups.com/>
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

/* NUT SNMP common functions */
#include "snmp-ups.h"
#include "main.h"
#include "parseconf.h"

/* include all known mib2nut lookup tables */
#include "apccmib.h"
#include "eaton-aphel-mib.h"
#include "raritan-mib.h"
#include "ietfmib.h"
#include "mgemib.h"
#include "netvisionmib.h"
#include "pwmib.h"

mib2nut_info_t mib2nut[] = {
	{ "apcc", APCC_MIB_VERSION, APCC_OID_POWER_STATUS,
		".1.3.6.1.4.1.318.1.1.1.1.1.1.0", apcc_mib },
	{ "mge", MGE_MIB_VERSION, "",
		MGE_OID_MODEL_NAME, mge_mib },
	{ "netvision", NETVISION_MIB_VERSION, "",
		NETVISION_OID_UPSIDENTMODEL, netvision_mib },
	{ "pw", PW_MIB_VERSION, "",
		PW_OID_MODEL_NAME, pw_mib },
	{ "ietf", IETF_MIB_VERSION, IETF_OID_POWER_STATUS,
		IETF_OID_MFR_NAME, ietf_mib },
	{ "aphel_genesisII", EATON_APHEL_MIB_VERSION, "",
		APHEL1_OID_MODEL_NAME, eaton_aphel_genesisII_mib },
	{ "aphel_revelation", EATON_APHEL_MIB_VERSION, "",
		APHEL2_OID_MODEL_NAME, eaton_aphel_revelation_mib },
	{ "raritan", RARITAN_MIB_VERSION, "",
		RARITAN_OID_MODEL_NAME, raritan_mib },
	{ NULL }
};

/* pointer to the Snmp2Nut lookup table */
snmp_info_t *snmp_info;
const char *mibname;
const char *mibvers;

#define DRIVER_NAME	"Generic SNMP UPS driver"
#define DRIVER_VERSION		"0.44"

/* driver description structure */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@free.fr>\n" \
	"Dmitry Frolov <frolov@riss-telecom.ru>\n" \
	"J.W. Hoogervorst <jeroen@hoogervorst.net>\n" \
	"Niels Baggesen <niels@baggesen.net>",
	DRV_STABLE,
	{ NULL }
};
/* FIXME: integrate MIBs info? do the same as for usbhid-ups! */

time_t lastpoll = 0;

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
void upsdrv_initinfo(void)
{
	snmp_info_t *su_info_p;
	char version[128];

	upsdebugx(1, "SNMP UPS driver : entering upsdrv_initinfo()");

	snprintf(version, sizeof version, "%s (mib: %s %s)",
		DRIVER_VERSION, mibname, mibvers);
	dstate_setinfo("driver.version.internal", "%s", version);

	/* add instant commands to the info database. */
	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)			
	{
		su_info_p->flags |= SU_FLAG_OK;
		if (SU_TYPE(su_info_p) == SU_TYPE_CMD)
			dstate_addcmd(su_info_p->info_type);
	}

	/* setup handlers for instcmd and setvar functions */
	upsh.setvar = su_setvar;
	upsh.instcmd = su_instcmd;

	if (testvar("notransferoids"))
		disable_transfer_oids();

	/* initialize all other INFO_ fields from list */
	if (snmp_ups_walk(SU_WALKMODE_INIT))
		dstate_dataok();
	else		
		dstate_datastale();
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1,"SNMP UPS driver : entering upsdrv_updateinfo()");
	
	/* only update every pollfreq */
	if (time(NULL) > (lastpoll + pollfreq)) {

		status_init();

		/* update all dynamic info fields */
		if (snmp_ups_walk(SU_WALKMODE_UPDATE))
			dstate_dataok();
		else		
			dstate_datastale();	

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
	fatalx(EXIT_SUCCESS, "SNMP doesn't support shutdown in system halt script");
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
	    "Set MIB compliance (default=ietf, allowed mge,apcc,netvision,pw)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_COMMUNITY,
	    "Set community name (default=public)");
	addvar(VAR_VALUE, SU_VAR_VERSION,
	    "Set SNMP version (default=v1, allowed v2c)");
	addvar(VAR_VALUE, SU_VAR_POLLFREQ,
	    "Set polling frequency in seconds, to reduce network flow (default=30)");
	addvar(VAR_FLAG, "notransferoids",
	    "Disable transfer OIDs (use on APCC Symmetras)");
}

void upsdrv_initups(void)
{
	snmp_info_t *su_info_p;
	char model[SU_INFOSIZE];
	bool_t status;
	const char *community, *version, *mibs;

	upsdebugx(1, "SNMP UPS driver : entering upsdrv_initups()");
	
	community = testvar(SU_VAR_COMMUNITY) ? getval(SU_VAR_COMMUNITY) : "public";
	version = testvar(SU_VAR_VERSION) ? getval(SU_VAR_VERSION) : "v1";
	mibs = testvar(SU_VAR_MIBS) ? getval(SU_VAR_MIBS) : "auto";

	/* init SNMP library, etc... */
	nut_snmp_init(progname, device_path, version, community);

	/* Load the SNMP to NUT translation data */
	/* read_mibconf(SU_VAR_MIBS) ? getval(SU_VAR_MIBS) : "ietf"); */
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
		/* No supported device detected */
}

void upsdrv_cleanup(void)
{
	nut_snmp_cleanup();
}

/* -----------------------------------------------------------
 * SNMP functions.
 * ----------------------------------------------------------- */

void nut_snmp_init(const char *type, const char *hostname, const char *version,
		const char *community)
{  
	upsdebugx(2, "SNMP UPS driver : entering nut_snmp_init(%s, %s, %s, %s)",
		type, hostname, version, community);

	/* Initialize the SNMP library */
	init_snmp(type);

	/* Initialize session */
	snmp_sess_init(&g_snmp_sess);

	g_snmp_sess.peername = xstrdup(hostname);
	g_snmp_sess.community = (unsigned char *)xstrdup(community);
	g_snmp_sess.community_len = strlen(community);
	if (strcmp(version, "v1") == 0)
		g_snmp_sess.version = SNMP_VERSION_1;
	else if (strcmp(version, "v2c") == 0)
		g_snmp_sess.version = SNMP_VERSION_2c;
	else
		fatalx(EXIT_FAILURE, "Bad SNMP version: %s", version);

	/* Open the session */
	SOCK_STARTUP;
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
	SOCK_CLEANUP;
}

struct snmp_pdu *nut_snmp_get(const char *OID)
{
	int status;
	struct snmp_pdu *pdu, *response = NULL;
	oid name[MAX_OID_LEN];
	size_t name_len = MAX_OID_LEN;
	static unsigned int numerr = 0;

	upsdebugx(3, "nut_snmp_get(%s)", OID);

	/* create and send request. */
	if (!snmp_parse_oid(OID, name, &name_len)) {
		upsdebugx(2, "[%s] nut_snmp_get: %s: %s",
			upsname?upsname:device_name, OID, snmp_api_errstring(snmp_errno));
		return NULL;
	}

	pdu = snmp_pdu_create(SNMP_MSG_GET);
	
	if (pdu == NULL)
		fatalx(EXIT_FAILURE, "Not enough memory");
	
	snmp_add_null_var(pdu, name, name_len);

	status = snmp_synch_response(g_snmp_sess_p, pdu, &response);

	if (!response)
		return NULL;

	if (!((status == STAT_SUCCESS) && (response->errstat == SNMP_ERR_NOERROR)))
	{
		if (mibname == NULL) {
			/* We are probing for proper mib - ignore errors */
			snmp_free_pdu(response);
			return NULL;
		}

		numerr++;

		if ((numerr == SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0))
			upslogx(LOG_WARNING, "[%s] Warning: excessive poll "
				"failures, limiting error reporting",
				upsname?upsname:device_name);

		if ((numerr < SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0))
			nut_snmp_perror(g_snmp_sess_p, status, response, 
				"nut_snmp_get: %s", OID);

		snmp_free_pdu(response);
		response = NULL;
	} else {
		numerr = 0;
	}

	return response;
}

bool_t nut_snmp_get_str(const char *OID, char *buf, size_t buf_len, info_lkp_t *oid2info)
{
	size_t len = 0;
	struct snmp_pdu *pdu;
	
	/* zero out buffer. */
	memset(buf, 0, buf_len);

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

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
	default:
		upslogx(LOG_ERR, "[%s] unhandled ASN 0x%x recieved from %s",
			upsname?upsname:device_name, pdu->variables->type, OID);
		return FALSE;
		break;
	}

	snmp_free_pdu(pdu);

	return TRUE;
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
		upslogx(LOG_ERR, "[%s] unhandled ASN 0x%x recieved from %s",
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
		if (response->errstat != SNMP_ERR_NOERROR)
			upslogx(LOG_ERR, "[%s] %s: Error in packet: %s",
				upsname?upsname:device_name, buf, snmp_errstring(response->errstat));
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
void su_setinfo(const char *type, const char *value, int flags, int auxdata)
{
	snmp_info_t *su_info_p;
	
	upsdebugx(1, "SNMP UPS driver : entering su_setinfo(%s)", type);
			
	su_info_p = su_find_info(type);
	
	if (SU_TYPE(su_info_p) == SU_TYPE_CMD)
		return;

	if (strcasecmp(type, "ups.status")) {
		dstate_setinfo(type, "%s", value);
		dstate_setflags(type, flags);
		dstate_setaux(type, auxdata);
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

/* find info element definition in my info array. */
snmp_info_t *su_find_info(const char *type)
{
	snmp_info_t *su_info_p;

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)
		if (!strcasecmp(su_info_p->info_type, type))
			return su_info_p;
		
	fatalx(EXIT_FAILURE, "nut_snmp_find_info: unknown info type: %s", type);
	return NULL;
}

/* Load the right snmp_info_t structure matching mib parameter */
bool_t load_mib2nut(const char *mib)
{
	bool_t ret = FALSE;
	mib2nut_info_t *mp = mib2nut;
	upsdebugx(2, "SNMP UPS driver : entering load_mib2nut(%s)", mib);
	
/*	read_mibconf(mib); */
	
	while (mp->mib_name) {
		if (strcmp(mib, mp->mib_name) == 0)
			break;
		else if (strcmp(mib, "auto") == 0) {
			int status;
			char buf[1024];
			upsdebugx(1, "load_mib2nut: trying %s mib", mp->mib_name);
			status = nut_snmp_get_str(mp->oid_auto_check,
						buf, sizeof buf, NULL);
			if (status) {
				ret = TRUE;
				break;
			}
		}
		mp++;
	}
	if (mp->mib_name) {
		snmp_info = mp->snmp_info;
		OID_pwr_status = mp->oid_pwr_status;
		mibname = mp->mib_name;
		mibvers = mp->mib_version;
		upsdebugx(1, "load_mib2nut: using %s mib", mibname);
	}
	else {
		/* Did we find something or is it really an unknown mib */
		/* we assume (ret == FALSE) */
		if (strcmp(mib, "auto") != 0)
			fatalx(EXIT_FAILURE, "Unknown mibs value: %s", mib);
		else
			fatalx(EXIT_FAILURE, "No supported device detected");
	}
	return ret;
}

/* find the OID value matching that INFO_* value */
long su_find_valinfo(info_lkp_t *oid2info, char* value)
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
		(strcmp(info_lkp->info_value, "NULL")); info_lkp++) {
			
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

/* walk ups variables and set elements of the info array. */
bool_t snmp_ups_walk(int mode)
{
	static unsigned long iterations = 0;
	snmp_info_t *su_info_p;
	bool_t status = FALSE;
	
	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++) {

		/* Check if we are asked to stop (reactivity++). */
		if (exit_flag != 0)
			return TRUE;

		/* skip instcmd. */
		if (SU_TYPE(su_info_p) == SU_TYPE_CMD) {
			upsdebugx(1, "SU_CMD_MASK => %s", su_info_p->OID);
			continue;
		}
		/* skip elements we shouldn't show. */
		if (!(su_info_p->flags & SU_FLAG_OK))
			continue;
		
		/* skip static elements in update mode. */
		if (mode == SU_WALKMODE_UPDATE && 
				su_info_p->flags & SU_FLAG_STATIC)
			continue;

		/* set default value if we cannot fetch it */
		/* and set static flag on this element. */
		if (su_info_p->flags & SU_FLAG_ABSENT) {
			if (mode == SU_WALKMODE_INIT) {
				if (su_info_p->dfl) {
					/* Set default value if we cannot fetch it from ups. */
					su_setinfo(su_info_p->info_type, su_info_p->dfl,
						su_info_p->info_flags, su_info_p->info_len);
				}
				su_info_p->flags |= SU_FLAG_STATIC;
			}
			continue;
		}

		/* check stale elements only on each PN_STALE_RETRY iteration. */
		if ((su_info_p->flags & SU_FLAG_STALE) &&
				(iterations % SU_STALE_RETRY) != 0)
			continue;

		if (su_info_p->flags & SU_INPHASES) {
			upsdebugx(1, "Check inphases");
		    	if (input_phases == 0) continue;
			upsdebugx(1, "inphases is set");
			if (su_info_p->flags & SU_INPUT_1) {
			    	if (input_phases == 1)
					su_info_p->flags &= ~SU_INPHASES;
				else {
					upsdebugx(1, "inphases is not 1");
				    	su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			}
			else if (su_info_p->flags & SU_INPUT_3) {
			    	if (input_phases == 3)
					su_info_p->flags &= ~SU_INPHASES;
				else {
					upsdebugx(1, "inphases is not 3");
				    	su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			}
		}

		if (su_info_p->flags & SU_OUTPHASES) {
			upsdebugx(1, "Check outphases");
		    	if (output_phases == 0) continue;
			upsdebugx(1, "outphases is set");
			if (su_info_p->flags & SU_OUTPUT_1) {
			    	if (output_phases == 1)
					su_info_p->flags &= ~SU_OUTPHASES;
				else {
					upsdebugx(1, "outphases is not 1");
					su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			}
			else if (su_info_p->flags & SU_OUTPUT_3) {
			    	if (output_phases == 3)
					su_info_p->flags &= ~SU_OUTPHASES;
				else {
					upsdebugx(1, "outphases is not 3");
				    	su_info_p->flags &= ~SU_FLAG_OK;
					continue;
				}
			}
		}

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
	}	/* for (su_info_p... */

	iterations++;
	
	return status;	
}

bool_t su_ups_get(snmp_info_t *su_info_p)
{
	static char buf[SU_INFOSIZE];
	bool_t status;
	long value;

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
		su_setinfo(su_info_p->info_type, buf,
			su_info_p->info_flags, su_info_p->info_len);

		return TRUE;
	}			

	if (su_info_p->info_flags == 0) {
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
	} else {
		status = nut_snmp_get_str(su_info_p->OID, buf, sizeof(buf), su_info_p->oid2info);
	}
		
	if (status == TRUE) {
		su_setinfo(su_info_p->info_type, buf,
			su_info_p->info_flags, su_info_p->info_len);
		
		upsdebugx(2, "=> value: %s", buf);
	}
	else upsdebugx(2, "=> Failed");
	return status;
}

/* set r/w INFO_ element to a value. */
int su_setvar(const char *varname, const char *val)
{
	snmp_info_t *su_info_p;
	bool_t ret;

	upsdebugx(2, "entering su_setvar()");

	su_info_p = su_find_info(varname);

	if (!su_info_p || !su_info_p->info_type || !(su_info_p->flags & SU_FLAG_OK)) {
		upsdebugx(2, "su_setvar: info element unavailable %s", varname);
		return STAT_SET_UNKNOWN;
	}

	if (!(su_info_p->info_flags & ST_FLAG_RW) || su_info_p->OID == NULL) {
		upsdebugx(2, "su_setvar: not writable %s", varname);
		return STAT_SET_INVALID;
	}

	/* set value. */
	if (SU_TYPE(su_info_p) == SU_TYPE_STRING) {
		ret = nut_snmp_set_str(su_info_p->OID, val);
	} else {
		ret = nut_snmp_set_int(su_info_p->OID, strtol(val, NULL, 0));
	}

	if (ret == FALSE) {
		upsdebugx(1, "su_setvar: cannot set value %s for %s", val, su_info_p->OID);
		return STAT_SET_FAILED;
	}

	upsdebugx(1, "su_setvar: sucessfully set %s to \"%s\"", su_info_p->info_type, val);

	/* update info array. */
	su_setinfo(varname, val, su_info_p->info_flags, su_info_p->info_len);

	/* TODO: check su_setinfo() retcode */
	return STAT_SET_HANDLED;
}

/* process instant command and take action. */
int su_instcmd(const char *cmdname, const char *extradata)
{
	snmp_info_t *su_info_p;
	int status;

	upsdebugx(2, "entering su_instcmd()");

	su_info_p = su_find_info(cmdname);

	if (!su_info_p || !su_info_p->info_type || !(su_info_p->flags & SU_FLAG_OK)) {
		upsdebugx(2, "su_instcmd: %s unavailable", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	if (extradata) {
		int	ret = STAT_SET_INVALID;

		if (strcasecmp(cmdname, "shutdown.return")) {
			ret = su_setvar("ups.delay.start", extradata);
		}
		if (strcasecmp(cmdname, "shutdown.stayoff")) {
			ret = su_setvar("ups.delay.shutdown", extradata);
		}
		if (strcasecmp(cmdname, "shutdown.reboot")) {
			ret = su_setvar("ups.delay.reboot", extradata);
		}

		if (ret == STAT_SET_HANDLED) {
			extradata = NULL;
		}
	}

	/* set value. */
	if (su_info_p->info_flags & ST_FLAG_STRING) {
		status = nut_snmp_set_str(su_info_p->OID, extradata ? extradata : su_info_p->dfl);
	} else {
		status = nut_snmp_set_int(su_info_p->OID, extradata ? atoi(extradata) : su_info_p->info_len);
	}

	if (status == FALSE) {
		upsdebugx(1, "su_instcmd: cannot set value for %s", cmdname);
		return STAT_INSTCMD_FAILED;
	}

	upsdebugx(1, "su_instcmd: successfully sent command %s", cmdname);
	return STAT_INSTCMD_HANDLED;
}

/* TODO: complete rewrite */
void su_shutdown_ups(void)
{
	int sdtype = 0;
	long pwr_status;

	if (nut_snmp_get_int(OID_pwr_status, &pwr_status) == FALSE)
		fatalx(EXIT_FAILURE, "cannot determine UPS status");

	if (testvar(SU_VAR_SDTYPE))
		sdtype = atoi(getval(SU_VAR_SDTYPE));

	/* logic from newapc.c */
	switch (sdtype) {
	case 3:		/* shutdown with grace period */
		upslogx(LOG_INFO, "sending delayed power off command to UPS");
		su_instcmd("shutdown.stayoff", "0");
		break;
	case 2:		/* instant shutdown */
		upslogx(LOG_INFO, "sending power off command to UPS");
		su_instcmd("load.off", "0");
		break;
	case 1:
		/* Send a combined set of shutdown commands which can work better */
		/* if the UPS gets power during shutdown process */
		/* Specifically it sends both the soft shutdown 'S' */
		/* and the powerdown after grace period - '@000' commands */
/*		upslogx(LOG_INFO, "UPS - sending shutdown/powerdown");
		if (pwr_status == g_pwr_battery)
			su_ups_instcmd(CMD_SOFTDOWN, 0, 0);
		su_ups_instcmd(CMD_SDRET, 0, 0);
		break;
*/	
	default:
		/* if on battery... */
/*		if (pwr_status == su_find_valinfo(info_lkp_t *oid2info, "OB")) {
			upslogx(LOG_INFO,
				"UPS is on battery, sending shutdown command...");
			su_ups_instcmd(CMD_SOFTDOWN, 0, 0);
		} else {
			upslogx(LOG_INFO, "UPS is online, sending shutdown+return command...");
			su_ups_instcmd(CMD_SDRET, 0, 0);
		}
*/
		break;
	}
}

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
			upsdebugx(1, "su_setvar: sucessfully set %s to \"%s\"", arg[0], arg[4]);

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
