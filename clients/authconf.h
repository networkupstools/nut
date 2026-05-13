/* authconf.h - prototypes and structures for NUT client authentication configuration
 *
 * Copyright (C) 2026 Jim Klimov <jimklimov+nut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef NUT_AUTHCONF_H_SEEN
#define NUT_AUTHCONF_H_SEEN 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdio.h>

#include "nut_stdint.h"

typedef struct upscli_authconf_s {
	char	*section;	/* [@host:port] or [user@host:port], or NULL for global */
	char	*user;
	char	*pass;
	char	*certpath;
	char	*certfile;
	char	*certident;
	char	*certpasswd;	/* Password for key/cert storage */
	char	*ssl_backend;	/* openssl/nss */
	int	certverify;	/* -1 = unset, 0 = off, 1 = on */
	int	forcessl;	/* -1 = unset, 0 = off, 1 = on */

	struct upscli_authconf_s	*next;
} upscli_authconf_t;

/** Get the one global list of all parsed authentication configurations */
upscli_authconf_t *upscli_get_authconf_list(void);

/** Create a one-off configuration item, upscli_free_authconf_item() it manually */
upscli_authconf_t *upscli_create_authconf_item(const char *section);

/** Free an authentication configuration item (if not NULL) and return its "next" pointer */
upscli_authconf_t *upscli_free_authconf_item(upscli_authconf_t *node);

/** Free the list of authentication configurations */
void upscli_free_authconf_list(void);

/** Read the authentication configuration file (usually nutauth.conf)
 * If filename==NULL, tries to locate per-user ${HOME}/.config/nut/nutauth.conf
 * and ${HOME}/.nutauth.conf, or site default ${nutconfdir}/nutauth.conf
 * (whichever is found first); then one can follow `INCLUDE` trail if needed.
 * Returns -1 on error, 1 on success
 */
int upscli_read_authconf_file(const char *filename, int fatal_errors);

/** All p_* args must be non-NULL pointers to `char *` string variables
 * which may be freed and re-allocated to return normalized values
 * (original strings may themselves be NULL).
 * The out_* values are optional and may be NULL if you do not want
 * those data points returned.
 */
int upscli_normalize_authconf_section_parts(
	char **out_normalized_sect_name,
	char **p_sect_user,
	int  *out_fixed_sect_user,
	char **p_sect_host,
	char **p_sect_port);

/** Take raw sect_name as input (e.g. a user-written string from config files).
 * Normalize it by splitting into user, host, and port components (populating absent values).
 * Return normalized components and reconstructed section name in output parameters (if not NULL),
 * and 0 for successful completion or -1 if any error happened along the way.
 */
int upscli_split_authconf_section(const char *sect_name,
	char **normalized_sect_name,
	char **normalized_sect_user,
	int    *out_fixed_sect_user,
	char **normalized_sect_host,
	char **normalized_sect_port);

/** Find the best matching authconf for a given connection string;
 * if all args are NULL, return the global section or NULL if none such in the list.
 */
upscli_authconf_t *upscli_find_authconf_item(const char *user, const char *host, const char *port);

/** Print one node to the specified stream (stdout if NULL),
 * return code similar to fprintf() - sum of printed characters.
 *
 * The for_debug value controls the verbosity of the output:
 * 0 - do not print NULL strings, do not indent global section
 * 1 - print <null> strings, indent global [<null>] section as any other
 * 2 - like 1, but do not escape special characters in strings (only double-quote them).
 *
 * Used from upscli_dump_authconf_list() */
int upscli_dump_authconf_item(FILE *restrict stream, upscli_authconf_t *node, int for_debug);

/** Print ultimate configuration to the specified stream (stdout if NULL)
 * and return the number of nodes in the current authconf list */
size_t upscli_dump_authconf_list(FILE *restrict stream, int for_debug);

#ifdef __cplusplus
}
#endif

#endif /* NUT_AUTHCONF_H_SEEN */
