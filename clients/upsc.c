/* upsc - simple "client" to test communications 

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

#include "common.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "upsclient.h"

static void help(const char *prog)
{
	printf("Network UPS Tools upsc %s\n\n", UPS_VERSION);

	printf("usage: %s <ups> [<variable>]\n", prog);

	printf("\nDemo program to display UPS variables.\n\n");

	printf("  <ups>      - upsd server, <upsname>@<hostname>[:<port>] form\n");
	printf("  <variable> - optional, display this variable only.\n");
	printf("               Default: list all variables for <host>\n");

	exit(EXIT_SUCCESS);
}

static void clean_exit(UPSCONN *ups, char *upsname, char *hostname, int code)
{
	if (upsname)
		free(upsname);

	if (hostname)
		free(hostname);

	upscli_disconnect(ups);

	exit(code);
}

static int printvar(UPSCONN *ups, const char *upsname, const char *var)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[4];
	char	**answer;

	/* old-style variable name? */
	if (!strchr(var, '.')) {
		fprintf(stderr, "Error: old-style variable names are not supported\n");
		return EXIT_FAILURE;
	}

	if (!upsname) {
		fprintf(stderr, "Error: ups name must be defined\n");
		return EXIT_FAILURE;
	}

	query[0] = "VAR";
	query[1] = upsname;
	query[2] = var;

	numq = 3;

	ret = upscli_get(ups, numq, query, &numa, &answer);

	if (ret < 0) {

		/* new var and old upsd?  try to explain the situation */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			fprintf(stderr, "Error: variable unknown (old upsd detected)\n");
			return EXIT_FAILURE;
		}

		fprintf(stderr, "Error: %s\n", upscli_strerror(ups));
		return EXIT_FAILURE;
	}

	if (numa < numq) {
		fprintf(stderr, "Error: insufficient data "
			"(got %d args, need at least %d)\n", numa, numq);
		return EXIT_FAILURE;
	}

	printf("%s\n", answer[3]);
	return EXIT_SUCCESS;
}

static int list_vars(UPSCONN *ups, const char *upsname)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[4];
	char	**answer;

	if (!upsname) {
		fprintf(stderr, "Error: a UPS name must be specified (upsname@hostname)\n");
		return EXIT_FAILURE;
	}

	query[0] = "VAR";
	query[1] = upsname;
	numq = 2;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {

		/* check for an old upsd */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			fprintf(stderr, "Error: upsd is too old to support this query\n");
			return EXIT_FAILURE;
		}

		fprintf(stderr, "Error: %s\n", upscli_strerror(ups));
		return EXIT_FAILURE;
	}

	ret = upscli_list_next(ups, numq, query, &numa, &answer);

	while (ret == 1) {

		/* VAR <upsname> <varname> <val> */
		if (numa < 4) {
			fprintf(stderr, "Error: insufficient data "
				"(got %d args, need at least 4)\n", numa);
			return EXIT_FAILURE;
		}

		printf("%s: %s\n", answer[2], answer[3]);

		ret = upscli_list_next(ups, numq, query, &numa, &answer);
	}

	return EXIT_SUCCESS;
}

static void check_upsdef(const char *ups)
{
	char	*ptr;

	ptr = strchr(ups, '@');

	if (ptr)
		return;

	fprintf(stderr, "Error: invalid UPS definition.  Required format: upsname@hostname[:port]\n");

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int	port, ret;
	char	*upsname, *hostname;
	UPSCONN	ups;

	if (argc < 2)
		help(argv[0]);

	/* special cases since we're not using getopt */
	if (!strcmp(argv[1], "-V")) {
		printf("Network UPS Tools upsc %s\n", UPS_VERSION);
		exit(EXIT_SUCCESS);
	}

	if (!strcmp(argv[1], "-h"))
		help(argv[0]);

	check_upsdef(argv[1]);

	upsname = hostname = NULL;

	if (upscli_splitname(argv[1], &upsname, &hostname, &port) != 0)
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);

	if (upscli_connect(&ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0) {
		fprintf(stderr, "Error: %s\n", upscli_strerror(&ups));

		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	if (argc >= 3) {
		ret = printvar(&ups, upsname, argv[2]);
		clean_exit(&ups, upsname, hostname, ret);
	}

	ret = list_vars(&ups, upsname);
	clean_exit(&ups, upsname, hostname, ret);

	/* NOTREACHED */
	exit(EXIT_FAILURE);
}
