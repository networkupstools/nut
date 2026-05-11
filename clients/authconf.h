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

/** Get the list of all parsed authentication configurations */
upscli_authconf_t *upscli_get_authconf_list(void);

/** Free an authentication configuration item (if not NULL) and return its "next" pointer */
upscli_authconf_t *upscli_free_authconf(upscli_authconf_t *node);

/** Free the list of authentication configurations */
void upscli_free_authconf_list(void);

/** Read the authentication configuration file (usually nutauth.conf)
 * returns -1 on error, 1 on success
 */
int upscli_read_authconf(const char *filename, int fatal_errors);

/** Find the best matching authconf for a given connection string;
 * if all args are NULL, return the global section or NULL if none such in the list.
 */
upscli_authconf_t *upscli_find_authconf(const char *user, const char *host, const char *port);

/** Print ultimate configuration to specified stream (stdout if NULL)
 * and return the number of nodes in the current authconf list */
size_t upscli_dump_authconf_list(FILE *restrict stream);

/** Print one node to specified stream (stdout if NULL), return fprintf() return code;
 * used from upscli_dump_authconf_list() */
int upscli_dump_authconf(FILE *restrict stream, upscli_authconf_t *node);

#ifdef __cplusplus
}
#endif

#endif /* NUT_AUTHCONF_H_SEEN */
