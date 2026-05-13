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
#include <sys/stat.h>
#include <ctype.h>

#ifndef WIN32
# include <netdb.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/ioctl.h>
#else /* => WIN32 */
/* Those 2 files for support of getaddrinfo, getnameinfo and freeaddrinfo
   on Windows 2000 and older versions */
# include <ws2tcpip.h>
# include <wspiapi.h>
/* This override network system calls to adapt to Windows specificity */
# define W32_NETWORK_CALL_OVERRIDE
# include "wincompat.h"
# undef W32_NETWORK_CALL_OVERRIDE
#endif	/* WIN32 */

static upscli_authconf_t	*authconf_list = NULL;
static upscli_authconf_t	*current_section = NULL;
static upscli_authconf_t	*global_defaults = NULL;
static int	current_section_with_fixed_username = 0;

static int parse_authconf_file(const char *filename, int fatal_errors, int global_scope);

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

static int upscli_dump_authconf_line_str(FILE *restrict stream, const char *var, const char *val, const char *indent, int for_debug)
{
	/* Assume sane inputs from upscli_dump_authconf(); val may be NULL */
	int	res = 0;
	if (!val) {
		if (for_debug) {
			res = fprintf(stream,
				"%s%s = <null>\n",
				indent, var
				);
		}
		return 0;
	} else {
		if (for_debug == 1 && *val) {
			char	enc[LARGEBUF];
			res = fprintf(stream,
				"%s%s = \"%s\"\n",
				indent, var, pconf_encode(val, enc, sizeof(enc))
				);
		} else {
			res = fprintf(stream,
				"%s%s = \"%s\"\n",
				indent, var, val
				);
		}
	}

	if (res < 0) {
		upsdebugx(5, "%s: failed (%d) to effectively print %s='%s'", __func__, res, NUT_STRARG(var), NUT_STRARG(val));
	}
	return res;
}

static int upscli_dump_authconf_line_int(FILE *restrict stream, const char *var, int val, const char *indent, int for_debug)
{
	/* Assume sane inputs from upscli_dump_authconf(); val may be NULL */
	int res;

	/* TOTHINK: Print "-1" values when not running "for_debug"?
	 *  We do parse them to hop over to a better preference... */
	NUT_UNUSED_VARIABLE(for_debug);

	res = fprintf(stream,
		"%s%s = %d\n",
		indent, var, (int)val
		);

	if (res < 0) {
		upsdebugx(5, "%s: failed (%d) to effectively print %s=%d", __func__, res, NUT_STRARG(var), val);
	}
	return res;
}

int upscli_dump_authconf(FILE *restrict stream, upscli_authconf_t *node, int for_debug)
{
	char	*indent = NULL;
	int	res = 0, ret = 0;

	if (!node)
		return -1;

	if (!stream)
		stream = stdout;

	if (node->section && *(node->section)) {
		indent = "\t";
		res = fprintf(stream, "[%s]\n", node->section);
	} else {
		/* Global section */
		if (for_debug) {
			indent = "\t";
			res = fprintf(stream, "[<null>]\n");
		} else {
			indent = "";
			res = 0;
		}
	}

	if (res < 0)
		return res;
	ret += res;

	res = upscli_dump_authconf_line_str(stream, "USER", node->user, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_str(stream, "PASS", node->pass, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_str(stream, "CERTPATH", node->certpath, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_str(stream, "CERTFILE", node->certfile, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_str(stream, "CERTIDENT_NAME", node->certident, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_str(stream, "CERTIDENT_PASS", node->certpasswd, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_str(stream, "SSLBACKEND", node->ssl_backend, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_int(stream, "CERTVERIFY", node->certverify, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	res = upscli_dump_authconf_line_int(stream, "FORCESSL", node->forcessl, indent, for_debug);
	if (res < 0)
		return ret;
	ret += res;

	return ret;
}

size_t upscli_dump_authconf_list(FILE *restrict stream, int for_debug)
{
	upscli_authconf_t	*node = authconf_list;
	size_t	count = 0;

	while (node) {
		count++;
		upscli_dump_authconf(stream, node, for_debug);
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

int upscli_normalize_auth_section_parts(
	char **out_normalized_sect_name,
	char **p_sect_user,
	int  *out_fixed_sect_user,
	char **p_sect_host,
	char **p_sect_port)
{
	char	*sect_user = NULL, *sect_host = NULL, *sect_port = NULL;

	/* All p_* args must be non-NULL pointers to `char *` string variables
	 * which may be freed and re-allocated to return normalized values
	 * (original strings may themselves be NULL).
	 * The out_* values are optional and may be NULL if you do not want
	 * those data points returned.
	 */
	if (!p_sect_user || !p_sect_host || !p_sect_port) {
		upslogx(LOG_ERR, "upscli_normalize_auth_section_parts: NULL pointer-to-string argument provided");
		return -1;
	}

	/* No changes imposed here */
	sect_user = *p_sect_user;

	sect_host = *p_sect_host;
	if (!sect_host || !*sect_host) {
		sect_host = xstrdup("localhost");
		if (!sect_host) goto failed;
	}

	sect_port = *p_sect_port;
	if (sect_port && *sect_port) {
		/* As port is a string, resolve it (if not a number,
		 * try to get one via "services" naming database) */
		char	*p = sect_port;
		int	is_numeric = 1;

		while (*p) {
			if (!isdigit((unsigned char)*p)) {
				is_numeric = 0;
				break;
			}
			p++;
		}

		if (!is_numeric) {
			struct servent	*se = getservbyname(sect_port, "tcp");

			if (se) {
				char	portbuf[16];
				if (snprintf(portbuf, sizeof(portbuf), "%u", (unsigned int)ntohs(se->s_port)) < 1) {
					upsdebugx(1, "%s: Failed to construct port number from service name", __func__);
					goto failed;
				}
				sect_port = xstrdup(portbuf);
				if (!sect_port) goto failed;
			} else {
				upslogx(LOG_WARNING, "%s: Failed to resolve port number from service name '%s', "
					"keeping original string but it is likely useless", __func__, sect_port);
			}
		}
	} else {
		char	portbuf[16];
		if (snprintf(portbuf, sizeof(portbuf), "%u", (unsigned int)NUT_PORT) < 1) {
			upsdebugx(1, "%s: Failed to construct default port number", __func__);
			goto failed;
		}
		sect_port = xstrdup(portbuf);
		if (!sect_port) goto failed;
	}

	/* Only now that we (almost) do not expect failures, we can
	 * consistently populate caller's output variables (if any) */
	if (out_normalized_sect_name) {
		char	normalized_sect_name_buf[LARGEBUF];

		if (snprintf(normalized_sect_name_buf, sizeof(normalized_sect_name_buf), "%s@%s:%s",
			sect_user ? sect_user : "",
			sect_host,
			sect_port) > 0
		) {
			free(*out_normalized_sect_name);
			*out_normalized_sect_name = xstrdup(normalized_sect_name_buf);
		} else {
			upsdebugx(1, "%s: Failed to reconstruct normalized section header", __func__);
			goto failed;
		}
	}

	if (out_fixed_sect_user)
		*out_fixed_sect_user = (sect_user && *sect_user);

	/* Different pointers? */
	if (*p_sect_host != sect_host) {
		free(*p_sect_host);
		*p_sect_host = sect_host;
	}

	if (*p_sect_port != sect_port) {
		free(*p_sect_port);
		*p_sect_port = sect_port;
	}

	return 0;

failed:
	free(sect_user);
	free(sect_host);
	free(sect_port);

	return -1;
}

int upscli_split_auth_section(const char *sect_name,
	char **normalized_sect_name,
	char **normalized_sect_user,
	int    *out_fixed_sect_user,
	char **normalized_sect_host,
	char **normalized_sect_port)
{
	/* Take raw sect_name as input (e.g. a user-written string from config files).
	 * Normalize it by splitting into user, host, and port components (populating absent values).
	 * Return normalized components and reconstructed section name in output parameters (if not NULL).
	 */
	const char	*at = NULL, *colon = NULL;
	char	*sect_user = NULL, *sect_host = NULL, *sect_port = NULL;
	int	fixed_sect_user = 0;

	if (!sect_name) {
		upsdebugx(1, "%s: sect_name is NULL", __func__);
		return -1;
	}

	if (!(*sect_name)) {
		/* TOTHINK: Should this mean `localhost@NUT_PORT`? Or global? Probably neither. */
		upsdebugx(1, "%s: sect_name is empty", __func__);
		return -1;
	}

	at = strchr(sect_name, '@');
	colon = strchr(sect_name, ':');
	if (at && colon && colon < at) {
		upsdebugx(1, "%s: Invalid section header: colon ':' before at '@': '%s'", __func__, sect_name);
		return -1;
	}

	fixed_sect_user = (at && at != sect_name);
	if (fixed_sect_user) {
		/* If section matched user@host:port, ensure user is set to this user */
		sect_user = xstrdup(sect_name);
		if (!sect_user) goto failed;
		sect_user[at - sect_name] = '\0';
	}

	if (at) {
		if (at + 1 != colon) {
			sect_host = xstrdup(at + 1);
			if (!sect_host) goto failed;
			if (colon) {
				sect_host[colon - at - 1] = '\0';
			}
		}	/* else keep NULL */
	} else {
		if (sect_name + 1 != colon) {
			sect_host = xstrdup(sect_name);
			if (!sect_host) goto failed;
			if (colon) {
				sect_host[colon - sect_name] = '\0';
			}
		}	/* else keep NULL */
	}

	if (colon && colon[1]) {
		/* May get re-normalized below */
		sect_port = xstrdup(colon + 1);
		if (!sect_port) goto failed;
	}

	if (upscli_normalize_auth_section_parts(
			normalized_sect_name,
			&sect_user, &fixed_sect_user,
			&sect_host, &sect_port) < 0
	) goto failed;

	if (out_fixed_sect_user)
		*out_fixed_sect_user = fixed_sect_user;

	if (normalized_sect_user) {
		*normalized_sect_user = sect_user;
	} else {
		free(sect_user);
	}

	if (normalized_sect_host) {
		*normalized_sect_host = sect_host;
	} else {
		free(sect_host);
	}

	if (normalized_sect_port) {
		*normalized_sect_port = sect_port;
	} else {
		free(sect_port);
	}

	return 0;

failed:
	free(sect_user);
	free(sect_host);
	free(sect_port);

	return -1;
}

static void handle_authconf_args(size_t numargs, char **arg, int global_scope)
{
	/* Property: var = val */
	const char	*var = NULL, *val = NULL;

	if (numargs < 1)
		return;

	/* Section header [section] */
	if (arg[0][0] == '[' && arg[0][strlen(arg[0])-1] == ']') {
		char	*sect_name = NULL, *sect_user = NULL, *sect_host = NULL, *sect_port = NULL, *normalized_sect_name = NULL;
		const char	*end_bracket = NULL;
		upscli_authconf_t	*tmp = NULL;

		if (!global_scope) {
			upslogx(LOG_WARNING, "Section header ignored in included file with section-scope");
			return;
		}

		sect_name = xstrdup(&arg[0][1]);	/* forget leading '[' */
		end_bracket = strchr(sect_name, ']');
		if (!end_bracket) {
			free(sect_name);
			fatalx(EXIT_FAILURE, "%s: Invalid section header format: %s", __func__, arg[0]);
		}
		*(char *)(end_bracket) = '\0';	/* forget trailing ']' and any characters after it (comments etc.) */

		if (upscli_split_auth_section(sect_name, &normalized_sect_name,
			&sect_user, &current_section_with_fixed_username,
			&sect_host, &sect_port) < 0
		) {
			free(normalized_sect_name);
			free(sect_name);
			free(sect_user);
			free(sect_host);
			free(sect_port);
			fatalx(EXIT_FAILURE, "Invalid nutauth section header: %s", NUT_STRARG(arg[0]));
		}

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

		free(normalized_sect_name);
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

	check_perms(filename);

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

	/* Ensure we start fresh if called multiple times */
	upscli_free_authconf_list();

	if (!filename) {
		/* Select a starting point - whichever default expected file exists;
		 * it may INCLUDE further files as wanted by user or site sysadmin.
		 */
		struct stat	st;
		char	*s = NULL;

		s = getenv("HOME");
		if (s) {
			if (snprintf(fn, sizeof(fn), "%s/.config/nut/nutauth.conf", s) > 0) {
				if (stat(fn, &st) == 0) {
					filename = fn;
					goto found;
				}
				upsdebugx(5, "%s: tried to default '%s' but it was not there", __func__, fn);
			}

			if (snprintf(fn, sizeof(fn), "%s/.nutauth.conf", s) > 0) {
				if (stat(fn, &st) == 0) {
					filename = fn;
					goto found;
				}
				upsdebugx(5, "%s: tried to default '%s' but it was not there", __func__, fn);
			}
		}

		if (snprintf(fn, sizeof(fn), "%s/nutauth.conf", confpath()) > 0) {
			if (stat(fn, &st) == 0) {
				filename = fn;
				goto found;
			}
			upsdebugx(5, "%s: tried to default '%s' but it was not there", __func__, fn);
		}

found:
		if (filename) {
			upsdebugx(1, "%s: defaulted to %s", __func__, filename);
		} else {
			if (fatal_errors) {
				fatalx(EXIT_FAILURE, "Can't open a user/site-provided default nutauth.conf file");
			} else {
				upslogx(LOG_WARNING, "Can't open a user/site-provided default nutauth.conf file");
				return -1;
			}
		}
	}

	return parse_authconf_file(filename, fatal_errors, 1);
}

upscli_authconf_t *upscli_find_authconf(const char *user, const char *host, const char *port)
{
	if (!host && !port && !user) {
		/* Global section only */
		/* Should we just return global_defaults? */
		upscli_authconf_t	*tmp = authconf_list;
		while (tmp) {
			if (!tmp->section || !*(tmp->section)) {
				return tmp;
			}
			tmp = tmp->next;
		}
		return NULL;
	} else {
		char	*sect_user = (user ? xstrdup(user) : NULL),
				*sect_host = (host ? xstrdup(host) : NULL),
				*sect_port = (port ? xstrdup(port) : NULL),
				*normalized_sect_name = NULL;
		int	fixed_sect_user = 0;
		upscli_authconf_t	*retval = global_defaults, *tmp = NULL;

		if (upscli_normalize_auth_section_parts(
			&normalized_sect_name,
			&sect_user,
			&fixed_sect_user,
			&sect_host,
			&sect_port) < 0)
				goto finished;	/* return default */

		/* 1. Try exactly the best info we have: user@host:port (user may be or not be empty) */
		tmp = authconf_list;
		while (tmp) {
			upsdebugx(2, "%s: matching '%s' against '%s'", __func__, normalized_sect_name, NUT_STRARG(tmp->section));
			if (tmp->section && !strcmp(tmp->section, normalized_sect_name)) {
				retval = tmp;
				goto finished;
			}
			tmp = tmp->next;
		}

		/* 2. Retry @host:port (host defaults) if that can help? */
		if (fixed_sect_user) {
			const char	*target_host_port = strchr(normalized_sect_name, '@');

			if (target_host_port[1]) {
				target_host_port++;
				upsdebugx(2, "%s: retry with shorter '@host:port' for host defaults (without the user part)", __func__);

				tmp = authconf_list;
				while (tmp) {
					upsdebugx(2, "%s: matching '%s' against '%s'", __func__, target_host_port, NUT_STRARG(tmp->section));
					if (tmp->section && !strcmp(tmp->section, target_host_port)) {
						retval = tmp;
						goto finished;
					}
					tmp = tmp->next;
				}
			}
		}

		/* 3. Global defaults (section == NULL) */
		retval = global_defaults;

finished:
		free(sect_user);
		free(sect_host);
		free(sect_port);
		free(normalized_sect_name);
		return retval;
	}
}
