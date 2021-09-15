/* conf.c - configuration handlers for upsd

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "upsd.h"
#include "conf.h"
#include "upsconf.h"
#include "sstate.h"
#include "user.h"
#include "netssl.h"
#include <ctype.h>

static ups_t	*upstable = NULL;
int	num_ups = 0;

/* add another UPS for monitoring from ups.conf */
static void ups_create(const char *fn, const char *name, const char *desc)
{
	upstype_t	*temp;

	for (temp = firstups; temp != NULL; temp = temp->next) {
		if (!strcasecmp(temp->name, name)) {
			upslogx(LOG_ERR, "UPS name [%s] is already in use!", name);
			return;
		}
	}

	/* grab some memory and add the info */
	temp = xcalloc(1, sizeof(*temp));
	temp->fn = xstrdup(fn);
	temp->name = xstrdup(name);

	if (desc) {
		temp->desc = xstrdup(desc);
	}

	temp->stale = 1;
	temp->retain = 1;
	temp->sock_fd = sstate_connect(temp);

	/* preload this to the current time to avoid false staleness */
	time(&temp->last_heard);

	temp->next = firstups;
	firstups = temp;
	num_ups++;
}

/* change the configuration of an existing UPS (used during reloads) */
static void ups_update(const char *fn, const char *name, const char *desc)
{
	upstype_t	*temp;

	temp = get_ups_ptr(name);

	if (!temp) {
		upslogx(LOG_ERR, "UPS %s disappeared during reload", name);
		return;
	}

	/* paranoia */
	if (!temp->fn) {
		upslogx(LOG_ERR, "UPS %s had a NULL filename!", name);

		/* let's give it something quick to use later */
		temp->fn = xstrdup("");
	}

	/* when the filename changes, force a reconnect */
	if (strcmp(temp->fn, fn) != 0) {

		upslogx(LOG_NOTICE, "Redefined UPS [%s]", name);

		/* release all data */
		sstate_infofree(temp);
		sstate_cmdfree(temp);
		pconf_finish(&temp->sock_ctx);

		close(temp->sock_fd);
		temp->sock_fd = -1;
		temp->dumpdone = 0;

		/* now redefine the filename and wrap up */
		free(temp->fn);
		temp->fn = xstrdup(fn);
	}

	/* update the description */

	free(temp->desc);

	if (desc)
		temp->desc = xstrdup(desc);
	else
		temp->desc = NULL;

	/* always set this on reload */
	temp->retain = 1;
}

/* return 1 if usable, 0 if not */
static int parse_upsd_conf_args(size_t numargs, char **arg)
{
	/* everything below here uses up through arg[1] */
	if (numargs < 2)
		return 0;

	/* MAXAGE <seconds> */
	if (!strcmp(arg[0], "MAXAGE")) {
		if (isdigit(arg[1][0])) {
			maxage = atoi(arg[1]);
			return 1;
		}
		else {
			upslogx(LOG_ERR, "MAXAGE has non numeric value (%s)!", arg[1]);
			return 0;
		}
	}

	/* TRACKINGDELAY <seconds> */
	if (!strcmp(arg[0], "TRACKINGDELAY")) {
		if (isdigit(arg[1][0])) {
			tracking_delay = atoi(arg[1]);
			return 1;
		}
		else {
			upslogx(LOG_ERR, "TRACKINGDELAY has non numeric value (%s)!", arg[1]);
			return 0;
		}
	}

	/* ALLOW_NO_DEVICE <seconds> */
	if (!strcmp(arg[0], "ALLOW_NO_DEVICE")) {
		if (isdigit(arg[1][0])) {
			allow_no_device = (atoi(arg[1]) != 0); // non-zero arg is true here
			return 1;
		}
		else {
			if ( (!strcasecmp(arg[1], "true")) || (!strcasecmp(arg[1], "on")) || (!strcasecmp(arg[1], "yes"))) {
				allow_no_device = 1;
				return 1;
			}
			if ( (!strcasecmp(arg[1], "false")) || (!strcasecmp(arg[1], "off")) || (!strcasecmp(arg[1], "no"))) {
				allow_no_device = 0;
				return 1;
			}
			upslogx(LOG_ERR, "ALLOW_NO_DEVICE has non numeric and non boolean value (%s)!", arg[1]);
			return 0;
		}
	}

	/* MAXCONN <connections> */
	if (!strcmp(arg[0], "MAXCONN")) {
		if (isdigit(arg[1][0])) {
			maxconn = atol(arg[1]);
			return 1;
		}
		else {
			upslogx(LOG_ERR, "MAXCONN has non numeric value (%s)!", arg[1]);
			return 0;
		}
	}

	/* STATEPATH <dir> */
	if (!strcmp(arg[0], "STATEPATH")) {
		free(statepath);
		statepath = xstrdup(arg[1]);
		return 1;
	}

	/* DATAPATH <dir> */
	if (!strcmp(arg[0], "DATAPATH")) {
		free(datapath);
		datapath = xstrdup(arg[1]);
		return 1;
	}

#ifdef WITH_OPENSSL
	/* CERTFILE <dir> */
	if (!strcmp(arg[0], "CERTFILE")) {
		free(certfile);
		certfile = xstrdup(arg[1]);
		return 1;
	}
#elif (defined WITH_NSS) /* WITH_OPENSSL */
	/* CERTPATH <dir> */
	if (!strcmp(arg[0], "CERTPATH")) {
		free(certfile);
		certfile = xstrdup(arg[1]);
		return 1;
	}
#ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
	/* CERTREQUEST (0 | 1 | 2) */
	if (!strcmp(arg[0], "CERTREQUEST")) {
		if (isdigit(arg[1][0])) {
			certrequest = atoi(arg[1]);
			return 1;
		}
		else {
			upslogx(LOG_ERR, "CERTREQUEST has non numeric value (%s)!", arg[1]);
			return 0;
		}
	}
#endif /* WITH_CLIENT_CERTIFICATE_VALIDATION */
#endif /* WITH_OPENSSL | WITH_NSS */

	/* ACCEPT <aclname> [<aclname>...] */
	if (!strcmp(arg[0], "ACCEPT")) {
		upslogx(LOG_WARNING, "ACCEPT in upsd.conf is no longer supported - switch to LISTEN");
		return 1;
	}

	/* REJECT <aclname> [<aclname>...] */
	if (!strcmp(arg[0], "REJECT")) {
		upslogx(LOG_WARNING, "REJECT in upsd.conf is no longer supported - switch to LISTEN");
		return 1;
	}

	/* LISTEN <address> [<port>] */
	if (!strcmp(arg[0], "LISTEN")) {
		if (numargs < 3)
			listen_add(arg[1], string_const(PORT));
		else
			listen_add(arg[1], arg[2]);
		return 1;
	}

	/* everything below here uses up through arg[2] */
	if (numargs < 3)
		return 0;

	/* ACL <aclname> <ip block> */
	if (!strcmp(arg[0], "ACL")) {
		upslogx(LOG_WARNING, "ACL in upsd.conf is no longer supported - switch to LISTEN");
		return 1;
	}

#ifdef WITH_NSS
	/* CERTIDENT <name> <passwd> */
	if (!strcmp(arg[0], "CERTIDENT")) {
		free(certname);
		certname = xstrdup(arg[1]);
		free(certpasswd);
		certpasswd = xstrdup(arg[2]);
		return 1;
	}
#endif /* WITH_NSS */

	/* not recognized */
	return 0;
}

/* called for fatal errors in parseconf like malloc failures */
static void upsd_conf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf (upsd.conf): %s", errmsg);
}

void load_upsdconf(int reloading)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	snprintf(fn, sizeof(fn), "%s/upsd.conf", confpath());

	check_perms(fn);

	pconf_init(&ctx, upsd_conf_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		if (!reloading)
			fatalx(EXIT_FAILURE, "%s", ctx.errmsg);

		upslogx(LOG_ERR, "Reload failed: %s", ctx.errmsg);
		return;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_upsd_conf_args(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg),
				"upsd.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s",
					ctx.arglist[i]);

			upslogx(LOG_WARNING, "%s", errmsg);
		}

	}

	pconf_finish(&ctx);
}

/* callback during parsing of ups.conf */
void do_upsconf_args(char *upsname, char *var, char *val)
{
	ups_t	*temp;

	/* no "global" stuff for us */
	if (!upsname) {
		return;
	}

	/* check if UPS is already listed */
	for (temp = upstable; temp != NULL; temp = temp->next) {
		if (!strcmp(temp->upsname, upsname)) {
			break;
		}
	}

	/* if not listed, create a new entry and prepend it to the list */
	if (temp == NULL) {
		temp = xcalloc(1, sizeof(*temp));
		temp->upsname = xstrdup(upsname);
		temp->next = upstable;
		upstable = temp;
	}

	if (!strcmp(var, "driver")) {
		free(temp->driver);
		temp->driver = xstrdup(val);
	} else if (!strcmp(var, "port")) {
		free(temp->port);
		temp->port = xstrdup(val);
	} else if (!strcmp(var, "desc")) {
		free(temp->desc);
		temp->desc = xstrdup(val);
	}
}

/* add valid UPSes from ups.conf to the internal structures */
void upsconf_add(int reloading)
{
	ups_t	*tmp = upstable, *next;
	char	statefn[SMALLBUF];

	if (!tmp) {
		upslogx(LOG_WARNING, "Warning: no UPS definitions in ups.conf");
		return;
	}

	while (tmp) {

		/* save for later, since we delete as we go along */
		next = tmp->next;

		/* this should always be set, but better safe than sorry */
		if (!tmp->upsname) {
			tmp = tmp->next;
			continue;
		}

		/* don't accept an entry that's missing items */
		if ((!tmp->driver) || (!tmp->port)) {
			upslogx(LOG_WARNING, "Warning: ignoring incomplete configuration for UPS [%s]\n",
				tmp->upsname);
		} else {
			snprintf(statefn, sizeof(statefn), "%s-%s",
				tmp->driver, tmp->upsname);

			/* if a UPS exists, update it, else add it as new */
			if ((reloading) && (get_ups_ptr(tmp->upsname) != NULL))
				ups_update(statefn, tmp->upsname, tmp->desc);
			else
				ups_create(statefn, tmp->upsname, tmp->desc);
		}

		/* free tmp's resources */

		free(tmp->driver);
		free(tmp->port);
		free(tmp->desc);
		free(tmp->upsname);
		free(tmp);

		tmp = next;
	}

	/* upstable should be completely gone by this point */
	upstable = NULL;
}

/* remove a UPS from the linked list */
static void delete_ups(upstype_t *target)
{
	upstype_t	*ptr, *last;

	if (!target)
		return;

	ptr = last = firstups;

	while (ptr) {
		if (ptr == target) {
			upslogx(LOG_NOTICE, "Deleting UPS [%s]", target->name);

			/* make sure nobody stays logged into this thing */
			kick_login_clients(target->name);

			/* about to delete the first ups? */
			if (ptr == last)
				firstups = ptr->next;
			else
				last->next = ptr->next;

			if (ptr->sock_fd != -1)
				close(ptr->sock_fd);

			/* release memory */
			sstate_infofree(ptr);
			sstate_cmdfree(ptr);
			pconf_finish(&ptr->sock_ctx);

			free(ptr->fn);
			free(ptr->name);
			free(ptr->desc);
			free(ptr);

			return;
		}

		last = ptr;
		ptr = ptr->next;
	}

	/* shouldn't happen */
	upslogx(LOG_ERR, "delete_ups: UPS not found");
}

/* see if we can open a file */
static int check_file(const char *fn)
{
	char	chkfn[SMALLBUF];
	FILE	*f;

	snprintf(chkfn, sizeof(chkfn), "%s/%s", confpath(), fn);

	f = fopen(chkfn, "r");

	if (!f) {
		upslog_with_errno(LOG_ERR, "Reload failed: can't open %s", chkfn);
		return 0;	/* failed */
	}

	fclose(f);
	return 1;	/* OK */
}

/* called after SIGHUP */
void conf_reload(void)
{
	upstype_t	*upstmp, *upsnext;

	upslogx(LOG_INFO, "SIGHUP: reloading configuration");

	/* see if we can access upsd.conf before blowing away the config */
	if (!check_file("upsd.conf"))
		return;

	/* reset retain flags on all known UPS entries */
	upstmp = firstups;
	while (upstmp) {
		upstmp->retain = 0;
		upstmp = upstmp->next;
	}

	/* reload from ups.conf */
	read_upsconf();
	upsconf_add(1);			/* 1 = reloading */

	/* now reread upsd.conf */
	load_upsdconf(1);		/* 1 = reloading */

	/* now delete all UPS entries that didn't get reloaded */

	upstmp = firstups;

	while (upstmp) {
		/* upstmp may be deleted during this pass */
		upsnext = upstmp->next;

		if (upstmp->retain == 0)
			delete_ups(upstmp);

		upstmp = upsnext;
	}

	/* did they actually delete the last UPS? */
	if (firstups == NULL)
		upslogx(LOG_WARNING, "Warning: no UPSes currently defined!");

	/* and also make sure upsd.users can be read... */
	if (!check_file("upsd.users"))
		return;

	/* delete all users */
	user_flush();

	/* and finally reread from upsd.users */
	user_load();
}
