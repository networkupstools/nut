/* authconf.c - handling NUT client authentication configuration parsing
 *
 * Copyright (C) 2026 Jim Klimov <jimklimov+nut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "common.h"

#include "authconf.h"
#include "parseconf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static upscli_authconf_t	*authconf_list = NULL;
static upscli_authconf_t	*current_section = NULL;
static upscli_authconf_t	*global_defaults = NULL;
static int	current_section_with_fixed_username = 0;

upscli_authconf_t *upscli_get_authconf_list(void)
{
	return authconf_list;
}

static upscli_authconf_t *upscli_add_authconf(const char *section)
{
	upscli_authconf_t	*node = xcalloc(1, sizeof(upscli_authconf_t));

	if (section) {
		node->section = xstrdup(section);
	}
	node->certverify = -1;
	node->forcessl = -1;

	/* Append to list */
	if (!authconf_list) {
		authconf_list = node;
	} else {
		upscli_authconf_t	*tmp = authconf_list;
		while (tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = node;
	}

	return node;
}

upscli_authconf_t *upscli_free_authconf(upscli_authconf_t *node)
{
	if (node) {
		upscli_authconf_t	*next = node->next;

		free(node->section);
		free(node->user);
		free(node->pass);
		free(node->certpath);
		free(node->certfile);
		free(node->certident);
		free(node->certpasswd);
		free(node->ssl_backend);

		free(node);

		return next;
	}

	return NULL;
}

int upscli_dump_authconf(FILE *restrict stream, upscli_authconf_t *node)
{
	if (!node)
		return -1;

	if (!stream)
		stream = stdout;

	return fprintf(stream,
		"[%s]\n\tUSER = \"%s\"\n\tPASS = \"%s\"\n"
		"\tCERTPATH = \"%s\"\n\tCERTFILE = \"%s\"\n"
		"\tCERTIDENT_NAME = \"%s\"\n\tCERTIDENT_PASS = \"%s\"\n"
		"\tSSLBACKEND = \"%s\"\n"
		"\tCERTVERIFY = %i\n\tFORCESSL = %i\n\n",
		NUT_STRARG(node->section),
		NUT_STRARG(node->user),
		NUT_STRARG(node->pass),
		NUT_STRARG(node->certpath),
		NUT_STRARG(node->certfile),
		NUT_STRARG(node->certident),
		NUT_STRARG(node->certpasswd),
		NUT_STRARG(node->ssl_backend),
		node->certverify,
		node->forcessl
	);
}

size_t upscli_dump_authconf_list(FILE *restrict stream)
{
	upscli_authconf_t	*node = authconf_list;
	size_t	count = 0;

	while (node) {
		count++;
		upscli_dump_authconf(stream, node);
		node = node->next;
	}

	return count;
}

void upscli_free_authconf_list(void)
{
	upscli_authconf_t	*node = authconf_list;

	while (node) {
		node = upscli_free_authconf(node);
	}

	authconf_list = NULL;
	current_section = NULL;
	global_defaults = NULL;
}

static void set_authconf_val(upscli_authconf_t *conf, const char *var, const char *val)
{
	if (!conf || !var)
		return;

	if (!strcasecmp(var, "user")) {
		if (current_section_with_fixed_username && conf->user
		 && (!val || (val && strcmp(conf->user, val)))
		) {
			upslogx(LOG_WARNING, "USER keyword ignored for a section named like 'user@host:port'");
			return;
		}
		free(conf->user);
		conf->user = val ? xstrdup(val) : NULL;
	} else if (!strcasecmp(var, "pass") || !strcasecmp(var, "password")) {
		free(conf->pass);
		conf->pass = val ? xstrdup(val) : NULL;
	} else if (!strcmp(var, "CERTPATH")) {
		free(conf->certpath);
		conf->certpath = val ? xstrdup(val) : NULL;
	} else if (!strcmp(var, "CERTFILE")) {
		free(conf->certfile);
		conf->certfile = val ? xstrdup(val) : NULL;
	} else if (!strcmp(var, "CERTIDENT_NAME")) {
		free(conf->certident);
		conf->certident = val ? xstrdup(val) : NULL;
	} else if (!strcmp(var, "CERTIDENT_PASS")) {
		free(conf->certpasswd);
		conf->certpasswd = val ? xstrdup(val) : NULL;
	} else if (!strcmp(var, "SSLBACKEND")) {
		free(conf->ssl_backend);
		conf->ssl_backend = val ? xstrdup(val) : NULL;
	} else if (!strcmp(var, "CERTVERIFY")) {
		if (val) {
			if (!strcasecmp(val, "on") || !strcasecmp(val, "yes") || !strcmp(val, "1"))
				conf->certverify = 1;
			else if (!strcasecmp(val, "off") || !strcasecmp(val, "no") || !strcmp(val, "0"))
				conf->certverify = 0;
		}
	} else if (!strcmp(var, "FORCESSL")) {
		if (val) {
			if (!strcasecmp(val, "on") || !strcasecmp(val, "yes") || !strcmp(val, "1"))
				conf->forcessl = 1;
			else if (!strcasecmp(val, "off") || !strcasecmp(val, "no") || !strcmp(val, "0"))
				conf->forcessl = 0;
		}
	} else {
		upslogx(LOG_WARNING, "Unrecognized authconf keyword: '%s'", var);
	}
}

static void authconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Error in parseconf(authconf): %s", errmsg);
}

static int parse_authconf_file(const char *filename, int fatal_errors, int global_scope);

static void handle_authconf_args(size_t numargs, char **arg, int global_scope)
{
	/* Property: var = val */
	const char	*var = NULL, *val = NULL;

	if (numargs < 1)
		return;

	/* Section header [section] */
	if (arg[0][0] == '[' && arg[0][strlen(arg[0])-1] == ']') {
		char	*sect_name = NULL, *at = NULL, *colon = NULL, *sect_user = NULL, *sect_host = NULL, *sect_port = NULL;
		char	normalized_sect_name[LARGEBUF];
		upscli_authconf_t	*tmp = NULL;

		if (!global_scope) {
			upslogx(LOG_WARNING, "Section header ignored in included file with section-scope");
			return;
		}

		sect_name = xstrdup(&arg[0][1]);	/* forget leading '[' */
		sect_name[strlen(sect_name)-1] = '\0';	/* forget trailing ']' */

		at = strchr(sect_name, '@');
		colon = strchr(sect_name, ':');
		if (at && colon && colon < at) {
			fatalx(LOG_WARNING, "Invalid section header: colon ':' before at '@'");
		}

		current_section_with_fixed_username = (at && at != sect_name);
		if (current_section_with_fixed_username) {
			/* If section matched user@host:port, ensure user is set to this user */
			sect_user = xstrdup(sect_name);
			sect_user[at - sect_name] = '\0';
		}

		if (at) {
			if (at + 1 != colon) {
				sect_host = xstrdup(at + 1);
				if (colon) {
					sect_host[colon - at - 1] = '\0';
				}
			}	/* else keep NULL */
		} else {
			if (sect_name + 1 != colon) {
				sect_host = xstrdup(sect_name);
				if (colon) {
					sect_host[colon - at - 1] = '\0';
				}
			}	/* else keep NULL */
		}

		if (colon && colon[1]) {
			sect_port = xstrdup(colon + 1);
		}

		if (!sect_host || !*sect_host)
			sect_host = xstrdup("localhost");

		if (!sect_port || !*sect_port) {
			sect_port = xcalloc(6, sizeof(char));
			snprintf(sect_port, 6, "%u", NUT_PORT);
		}

		snprintf(normalized_sect_name, sizeof(normalized_sect_name), "%s@%s:%s",
			sect_user ? sect_user : "",
			sect_host,
			sect_port);

		/* Find if section already exists */
		tmp = authconf_list;
		current_section = NULL;
		while (tmp) {
			if (tmp->section && !strcmp(tmp->section, normalized_sect_name)) {
				current_section = tmp;
				break;
			}
			tmp = tmp->next;
		}

		if (!current_section) {
			current_section = upscli_add_authconf(normalized_sect_name);

			if (current_section_with_fixed_username && sect_user && *sect_user) {
				/* If section matched user@host:port, ensure user is set to this user */
				current_section->user = xstrdup(sect_user);
			}

			/* Copy global defaults to new section */
			if (global_defaults) {
				if (!(current_section->user) && global_defaults->user) current_section->user = xstrdup(global_defaults->user);
				if (global_defaults->pass) current_section->pass = xstrdup(global_defaults->pass);
				if (global_defaults->certpath) current_section->certpath = xstrdup(global_defaults->certpath);
				if (global_defaults->certfile) current_section->certfile = xstrdup(global_defaults->certfile);
				if (global_defaults->certident) current_section->certident = xstrdup(global_defaults->certident);
				if (global_defaults->certpasswd) current_section->certpasswd = xstrdup(global_defaults->certpasswd);
				if (global_defaults->ssl_backend) current_section->ssl_backend = xstrdup(global_defaults->ssl_backend);
				current_section->certverify = global_defaults->certverify;
				current_section->forcessl = global_defaults->forcessl;
			}
		}

		free(sect_name);
		free(sect_user);
		free(sect_host);
		free(sect_port);
		return;
	}

	/* INCLUDE support */
	if (!strcasecmp(arg[0], "INCLUDE_REQUIRED")) {
		if (numargs < 2) {
			fatalx(EXIT_FAILURE, "INCLUDE_REQUIRED missing filename");
		}

		/* If we are in global scope (current_section == NULL), sub-includes are global scope.
		 * If we are in a section, sub-includes are section scope.
		 */
		parse_authconf_file(arg[1], 1, (current_section == NULL));
		return;
	}

	if (!strcasecmp(arg[0], "INCLUDE")) {
		if (numargs < 2) {
			upslogx(LOG_ERR, "INCLUDE missing filename");
			return;
		}

		/* If we are in global scope (current_section == NULL), sub-includes are global scope.
		 * If we are in a section, sub-includes are section scope.
		 */
		parse_authconf_file(arg[1], 0, (current_section == NULL));
		return;
	}

	/* While above we technically also handled possible arg[0] values,
	 * they were not variable names - and so were not called that. */
	var = arg[0];
	if (numargs >= 3 && !strcmp(arg[1], "=")) {
		val = arg[2];
	} else if (numargs == 1) {
		/* Flag property? */
		val = "1";
	}

	if (current_section) {
		set_authconf_val(current_section, var, val);
	} else {
		/* Modifying global defaults */
		if (!global_defaults) {
			global_defaults = upscli_add_authconf(NULL);
		}

		set_authconf_val(global_defaults, var, val);
		/* Spec says global-scope includes may modify global default items,
		 * as well as define new sections or overlay items in existing sections.
		 * My implementation handles this via current_section state.
		 */
	}
}

static int parse_authconf_file(const char *filename, int fatal_errors, int global_scope)
{
	PCONF_CTX_t	ctx;

	if (!pconf_init(&ctx, authconf_err)) {
		if (fatal_errors) {
			exit(EXIT_FAILURE);
		}
		return -1;
	}

	if (!pconf_file_begin(&ctx, filename)) {
		if (fatal_errors) {
			fatalx(EXIT_FAILURE, "Can't open %s: %s", filename, ctx.errmsg);
		} else {
			upslogx(LOG_WARNING, "Can't open %s: %s", filename, ctx.errmsg);
			pconf_finish(&ctx);
			return -1;
		}
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s", filename, ctx.linenum, ctx.errmsg);
			continue;
		}
		handle_authconf_args(ctx.numargs, ctx.arglist, global_scope);
	}

	pconf_finish(&ctx);
	return 1;
}

int upscli_read_authconf(const char *filename, int fatal_errors)
{
	char	fn[NUT_PATH_MAX + 1];

	if (!filename) {
		snprintf(fn, sizeof(fn), "%s/nutauth.conf", confpath());
		filename = fn;
	}

	/* Ensure we start fresh if called multiple times */
	upscli_free_authconf_list();

	return parse_authconf_file(filename, fatal_errors, 1);
}

upscli_authconf_t *upscli_find_authconf(const char *user, const char *host, const char *port)
{
	char	target_user_host_port[LARGEBUF];
	char	target_host_port[SMALLBUF];

	if (!host && !port && !user) {
		/* Global section only */
		upscli_authconf_t	*tmp = authconf_list;
		while (tmp) {
			if (!tmp->section || !*(tmp->section)) {
				return tmp;
			}
			tmp = tmp->next;
		}
		return NULL;
	}

	if (host && port && *host && *port) {
		snprintf(target_host_port, sizeof(target_host_port), "@%s:%s", host, port);
	} else if (host && *host) {
		snprintf(target_host_port, sizeof(target_host_port), "@%s:%u", host, (unsigned int)NUT_PORT);
	} else if (port && *port) {
		snprintf(target_host_port, sizeof(target_host_port), "@localhost:%s", port);
	} else {
		snprintf(target_host_port, sizeof(target_host_port), "@localhost:%u", (unsigned int)NUT_PORT);
	}

	snprintf(target_user_host_port, sizeof(target_user_host_port), "%s%s",
		((user && *user) ? user : ""),
		target_host_port	/* Note: includes the '@' */
	);

	/* 1. Try exact user@host:port */
	if (target_user_host_port[0]) {
		upscli_authconf_t	*tmp = authconf_list;
		while (tmp) {
			upsdebugx(2, "%s: matching '%s' against '%s'", __func__, target_user_host_port, NUT_STRARG(tmp->section));
			if (tmp->section && !strcmp(tmp->section, target_user_host_port)) {
				return tmp;
			}
			tmp = tmp->next;
		}
	}

	/* 2. Try @host:port (host defaults) */
	if (target_host_port[0]) {
		upscli_authconf_t	*tmp = authconf_list;
		while (tmp) {
			upsdebugx(2, "%s: matching '%s' against '%s'", __func__, target_host_port, NUT_STRARG(tmp->section));
			if (tmp->section && !strcmp(tmp->section, target_host_port)) {
				return tmp;
			}
			tmp = tmp->next;
		}
	}

	/* 3. Global defaults (section == NULL) */
	return global_defaults;
}
