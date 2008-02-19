/* netxml-ups.c	Driver routines for network XML UPS units 

   Copyright (C)
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ne_request.h>
#include <ne_basic.h>
#include <ne_props.h>
#include <ne_uri.h>

#include "netxml-ups.h"
#include "mge-xml.h"

static subdriver_t	*subdriver = &mge_xml_subdriver;

int main(int argc, char **argv)
{
	ne_session	*session;
	ne_xml_parser	*parser;
	ne_request	*request;
	ne_uri		uri;
	int		ret;

	if (argc != 2) {
		printf("Usage: %s uri\n", basename(argv[0]));
		return -1;
	}

	/* Initialize socket libraries */
	if (ne_sock_init()) {
		printf("nget: Failed to initialize socket libraries.\n");
		return -1;
	}

	/* Parse the URI argument. */
	if (ne_uri_parse(argv[1], &uri) || uri.host == NULL) {
		printf("nget: Invalid URI `%s'\n", argv[1]);
		return -1;
	}

	if (uri.scheme == NULL) {
		uri.scheme = "http";
	}

	if (uri.port == 0) {
		uri.port = ne_uri_defaultport(uri.scheme);
	}

	/* create the session */
	session = ne_session_create(uri.scheme, uri.host, uri.port);

#if 0
	/* Load default CAs if using SSL. */
	if (strcasecmp(uri.scheme, "https") == 0)
		if (ne_ssl_load_default_ca(session))
			fprintf(stdout, "Failed to load default CAs.\n");
#endif

	/* Create an XML parser. */
	parser = ne_xml_create();

	/* Push a new handler on the parser stack */
	ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

	request = ne_request_create(session, "GET", subdriver->initups);

	fprintf(stderr, "ne_request_create(session, \"GET\", \"%s\"\n", subdriver->initups);

	ret = ne_xml_dispatch_request(request, parser);

	ne_request_destroy(request);
	ne_xml_destroy(parser);

	if (ret != NE_OK) {
		fprintf(stdout, "%s: Failed: %s\n", basename(argv[0]), ne_get_error(session));
	}

	if (strlen(subdriver->initinfo) == 0) {
		fprintf(stderr, "%s: Don't know how to read status\n", basename(argv[0]));
		goto cleanup_exit;
	}

	/* Create an XML parser. */
	parser = ne_xml_create();

	/* Push a new handler on the parser stack */
	ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

	request = ne_request_create(session, "GET", subdriver->initinfo);

	fprintf(stderr, "ne_request_create(session, \"GET\", \"%s\"\n", subdriver->initinfo);

	ret = ne_xml_dispatch_request(request, parser);

	ne_request_destroy(request);
	ne_xml_destroy(parser);

	if (ret != NE_OK) {
		fprintf(stdout, "%s: Failed: %s\n", basename(argv[0]), ne_get_error(session));
	}

cleanup_exit:
	ne_session_destroy(session);

	return ret;
}
