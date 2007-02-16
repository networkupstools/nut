/* user.c - user handling functions for upsd

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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "parseconf.h"

#include "user.h"
#include "user-data.h"
#include "access.h"

	ulist_t	*users = NULL;

	static	ulist_t	*curr_user;

/* create a new user entry */
static void user_add(const char *un)
{
	ulist_t	*tmp, *last;

	if (!un)
		return;
	
	tmp = last = users;

	while (tmp) {
		last = tmp;

		if (!strcmp(tmp->username, un)) {
			fprintf(stderr, "Ignoring duplicate user %s\n", un);
			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ulist_t));
	tmp->username = xstrdup(un);
	tmp->firstacl = NULL;
	tmp->password = NULL;
	tmp->firstcmd = NULL;
	tmp->firstaction = NULL;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		users = tmp;	

	/* remember who we're working on */
	curr_user = tmp;
}

static acllist_t *addallow(acllist_t *base, const char *acl)
{
	acllist_t	*tmp, *last;

	if (!acl)
		return base;

	tmp = last = base;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(acllist_t));
	tmp->aclname = xstrdup(acl);
	tmp->next = NULL;

	if (last) {
		last->next = tmp;
		return base;
	}

	return tmp;
}

/* attach allowed hosts to user */
static void user_add_allow(const char *host)
{
	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring allowfrom definition outside "
			"user section");
		return;
	}

	curr_user->firstacl = addallow(curr_user->firstacl, host);
}

/* set password */
static void user_password(const char *pw)
{
	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring password definition outside "
			"user section");
		return;
	}

	if (!pw)
		return;

	if (curr_user->password) {
		fprintf(stderr, "Ignoring duplicate password for %s\n", 
			curr_user->username);
		return;
	}

	curr_user->password = xstrdup(pw);
}

/* attach allowed instcmds to user */
static void user_add_instcmd(const char *cmd)
{
	instcmdlist_t	*tmp, *last;

	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring instcmd definition outside "
			"user section");
		return;
	}

	if (!cmd)
		return;

	tmp = curr_user->firstcmd;
	last = NULL;

	while (tmp) {
		last = tmp;

		/* ignore duplicates */
		if (!strcasecmp(tmp->cmd, cmd))
			return;

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(instcmdlist_t));

	tmp->cmd = xstrdup(cmd);
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		curr_user->firstcmd = tmp;
}

static actionlist_t *addaction(actionlist_t *base, const char *action)
{
	actionlist_t	*tmp, *last;

	if (!action)
		return base;

	tmp = last = base;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(actionlist_t));
	tmp->action = xstrdup(action);
	tmp->next = NULL;

	if (last) {
		last->next = tmp;
		return base;
	}

	return tmp;
}

/* attach allowed actions to user */
static void user_add_action(const char *act)
{
	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring action definition outside "
			"user section");
		return;
	}

	curr_user->firstaction = addaction(curr_user->firstaction, act);
}

static void flushacl(acllist_t *first)
{
	acllist_t	*ptr, *next;

	ptr = first;

	while (ptr) {
		next = ptr->next;

		free(ptr->aclname);
		free(ptr);

		ptr = next;
	}
}

static void flushcmd(instcmdlist_t *first)
{
	instcmdlist_t	*ptr, *next;

	ptr = first;

	while (ptr) {
		next = ptr->next;

		free(ptr->cmd);
		free(ptr);

		ptr = next;
	}
}

static void flushaction(actionlist_t *first)
{
	actionlist_t	*ptr, *next;

	ptr = first;

	while (ptr) {
		next = ptr->next;

		free(ptr->action);
		free(ptr);

		ptr = next;
	}
}

/* flush all user attributes - used during reload */
void user_flush(void)
{
	ulist_t	*ptr, *next;

	ptr = users;

	while (ptr) {
		next = ptr->next;

		if (ptr->firstacl)
			flushacl(ptr->firstacl);

		if (ptr->firstcmd)
			flushcmd(ptr->firstcmd);

		if (ptr->firstaction)
			flushaction(ptr->firstaction);

		free(ptr->username);
		free(ptr->password);
		free(ptr);

		ptr = next;
	}

	users = NULL;
}	

#ifndef	HAVE_IPV6
static int user_matchacl(ulist_t *user, const struct sockaddr_in *addr)
#else
static int user_matchacl(ulist_t *user, const struct sockaddr_storage *addr)
#endif
{
	acllist_t	*tmp;

	tmp = user->firstacl;

	/* no acls means no access (fail-safe) */
	if (!tmp)
		return 0;		/* good */

	while (tmp) {
		if (acl_check(tmp->aclname, addr) == 1)
			return 1;	/* good */
	
		tmp = tmp->next;
	}

	return 0;	/* fail */
}

static int user_matchinstcmd(ulist_t *user, const char * cmd)
{
	instcmdlist_t	*tmp = user->firstcmd;

	/* no commands means no access (fail-safe) */
	if (!tmp)
		return 0;	/* fail */

	while (tmp) {
		if ((!strcasecmp(tmp->cmd, cmd)) || 
			(!strcasecmp(tmp->cmd, "all")))
			return 1;	/* good */
		tmp = tmp->next;
	}

	return 0;	/* fail */
}

#ifndef	HAVE_IPV6
int user_checkinstcmd(const struct sockaddr_in *addr, 
#else
int user_checkinstcmd(const struct sockaddr_storage *addr, 
#endif
	const char *un, const char *pw, const char *cmd)
{
	ulist_t	*tmp = users;

	if ((!un) || (!pw) || (!cmd))
		return 0;	/* failed */

	while (tmp) {

		/* let's be paranoid before we call strcmp */

		if ((!tmp->username) || (!tmp->password)) {
			tmp = tmp->next;
			continue;
		}

		if (!strcmp(tmp->username, un)) {
			if (!strcmp(tmp->password, pw)) {
				if (!user_matchacl(tmp, addr))
					return 0;		/* fail */

				if (!user_matchinstcmd(tmp, cmd))
					return 0;		/* fail */

				/* passed all checks */
				return 1;	/* good */
			}

			/* password mismatch */
			return 0;	/* fail */
		}

		tmp = tmp->next;
	}		

	/* username not found */
	return 0;	/* fail */
}

static int user_matchaction(ulist_t *user, const char *action)
{
	actionlist_t	*tmp = user->firstaction;

	/* no actions means no access (fail-safe) */
	if (!tmp)
		return 0;	/* fail */

	while (tmp) {
		if (!strcasecmp(tmp->action, action))
			return 1;	/* good */
		tmp = tmp->next;
	}

	return 0;	/* fail */
}

#ifndef	HAVE_IPV6
int user_checkaction(const struct sockaddr_in *addr, 
#else
int user_checkaction(const struct sockaddr_storage *addr, 
#endif
	const char *un, const char *pw, const char *action)
{
	ulist_t	*tmp = users;

	if ((!un) || (!pw) || (!action))
		return 0;	/* failed */

	while (tmp) {

		/* let's be paranoid before we call strcmp */

		if ((!tmp->username) || (!tmp->password)) {
			tmp = tmp->next;
			continue;
		}

		if (!strcmp(tmp->username, un)) {
			if (!strcmp(tmp->password, pw)) {

				if (!user_matchacl(tmp, addr)) {
					upsdebugx(2, "user_matchacl: failed");
					return 0;		/* fail */
				}

				if (!user_matchaction(tmp, action)) {
					upsdebugx(2, "user_matchaction: failed");
					return 0;		/* fail */
				}

				/* passed all checks */
				return 1;	/* good */
			}

			/* password mismatch */
			upsdebugx(2, "user_checkaction: password mismatch");
			return 0;	/* fail */
		}

		tmp = tmp->next;
	}		

	/* username not found */
	return 0;	/* fail */
}

/* handle "upsmon master" and "upsmon slave" for nicer configurations */
static void set_upsmon_type(char *type)
{
	/* master: login, master, fsd */
	if (!strcasecmp(type, "master")) {
		user_add_action("login");
		user_add_action("master");
		user_add_action("fsd");
		return;
	}

	/* slave: just login */
	if (!strcasecmp(type, "slave")) {
		user_add_action("login");
		return;
	}

	upslogx(LOG_WARNING, "Unknown upsmon type %s", type);
}

/* actually do something with the variable + value pairs */
static void parse_var(char *var, char *val)
{
	if (!strcasecmp(var, "password")) {
		user_password(val);
		return;
	}

	if (!strcasecmp(var, "instcmds")) {
		user_add_instcmd(val);
		return;
	}

	if (!strcasecmp(var, "actions")) {
		user_add_action(val);
		return;
	}

	if (!strcasecmp(var, "allowfrom")) {
		user_add_allow(val);
		return;
	}

	/* someone did 'upsmon = type' - allow it anyway */
	if (!strcasecmp(var, "upsmon")) {
		set_upsmon_type(val);
		return;
	}

	upslogx(LOG_NOTICE, "Unrecognized user setting %s", var);
}

/* parse first var+val pair, then flip through remaining vals */
static void parse_rest(char *var, char *fval, char **arg, int next, int left)
{
	int	i;

	/* no globals supported yet, so there's no sense in continuing */
	if (!curr_user)
		return;

	parse_var(var, fval);

	if (left == 0)
		return;

	for (i = 0; i < left; i++)
		parse_var(var, arg[next + i]);
}

static void user_parse_arg(int numargs, char **arg)
{
	char	*ep;

	if ((numargs == 0) || (!arg))
		return;

	/* ignore old file format */
	if (!strcasecmp(arg[0], "user"))
		return;

	/* handle 'foo=bar' (compressed form) */

	ep = strchr(arg[0], '=');
	if (ep) {
		*ep = '\0';

		/* parse first var/val, plus subsequent values (if any) */

		/*      0       1       2  ... */
		/* foo=bar <rest1> <rest2> ... */

		parse_rest(arg[0], ep+1, arg, 1, numargs - 1);
		return;
	}

	/* look for section headers - [username] */
	if ((arg[0][0] == '[') && (arg[0][strlen(arg[0])-1] == ']')) {
		arg[0][strlen(arg[0])-1] = '\0';
		user_add(&arg[0][1]);

		return;
	}

	if (numargs < 2)
		return;

	if (!strcasecmp(arg[0], "upsmon"))
		set_upsmon_type(arg[1]);

	/* everything after here needs arg[1] and arg[2] */
	if (numargs < 3)
		return;

	/* handle 'foo = bar' (split form) */
	if (!strcmp(arg[1], "=")) {

		/*   0 1   2      3       4  ... */
		/* foo = bar <rest1> <rest2> ... */

		/* parse first var/val, plus subsequent values (if any) */
		
		parse_rest(arg[0], arg[2], arg, 3, numargs - 3);
		return;
	}

	/* ... unhandled ... */
}

/* called for fatal errors in parseconf like malloc failures */
static void upsd_user_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(upsd.users): %s", errmsg);
}

void user_load(void)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	curr_user = NULL;

	snprintf(fn, sizeof(fn), "%s/upsd.users", confpath());

	check_perms(fn);

	pconf_init(&ctx, upsd_user_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		upslogx(LOG_WARNING, "%s", ctx.errmsg);
		return;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		user_parse_arg(ctx.numargs, ctx.arglist);
	}

	pconf_finish(&ctx);
}
