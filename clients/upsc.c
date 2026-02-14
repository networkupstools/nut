/* upsc - simple "client" to test communications

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2012  Arnaud Quette <arnaud.quette@free.fr>
   Copyright (C) 2020-2025  Jim Klimov <jimklimov+nut@gmail.com>

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
#include "nut_platform.h"

#ifndef WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif	/* !WIN32 */

#include "nut_stdint.h"
#include "upsclient.h"
#include "strjson.h"

/* network timeout for initial connection, in seconds */
#define UPSCLI_DEFAULT_CONNECT_TIMEOUT	"10"

static char		*upsname = NULL, *hostname = NULL;
static UPSCONN_t	*ups = NULL;
static int	output_json = 0;

static void fatalx_error_json_simple(int msg_is_simple, const char *msg)
	__attribute__((noreturn));

static void fatalx_error_json_simple(int msg_is_simple, const char *msg) {
	/* To be used in simpler cases, where the possible JSON
	 * message is not embedded into other lists/objects */
	if (output_json) {
		if (msg_is_simple) {
			/* Caller knows there is nothing to escape here, pass through */
			printf("{\"error\": \"%s\"}\n", msg);
		} else {
			printf("{\"error\": \"");
			json_print_esc(msg);
			printf("\"}\n");
		}
	}
	fatalx(EXIT_FAILURE, "Error: %s", msg);
}

static void usage(const char *prog)
{
	print_banner_once(prog, 2);
	printf("NUT read-only client program to display UPS variables.\n");

	printf("\nusage: %s [-j] -l | -L [<hostname>[:port]]\n", prog);
	printf("       %s [-j] <ups> [<variable>]\n", prog);
	printf("       %s [-j] -c <ups>\n", prog);

	printf("\nOption:\n");
	printf("  -j         - display output in JSON format\n");

	printf("\nFirst form (lists UPSes):\n");
	printf("  -l         - lists each UPS on <hostname>, one per line.\n");
	printf("  -L         - lists each UPS followed by its description (from ups.conf).\n");
	printf("               Default hostname: localhost\n");

	printf("\nSecond form (lists variables and values):\n");
	printf("  <ups>      - upsd server, <upsname>[@<hostname>[:<port>]] form\n");
	printf("  <variable> - optional, display this variable only.\n");
	printf("               Default: list all variables for <host>\n");

	printf("\nThird form (lists clients connected to a device):\n");
	printf("  -c         - lists each client connected on <ups>, one per line.\n");
	printf("  <ups>      - upsd server, <upsname>[@<hostname>[:<port>]] form\n");

	printf("\nCommon arguments:\n");
	printf("  -V         - display the version of this software\n");
	printf("  -W <secs>  - network timeout for initial connections (default: %s)\n",
		UPSCLI_DEFAULT_CONNECT_TIMEOUT);
	printf("  -h         - display this help text\n");

	nut_report_config_flags();

	printf("\n%s", suggest_doc_links(prog, NULL));
}

static void printvar(const char *var)
{
	int		ret;
	size_t	numq, numa;
	const char	*query[4];
	char		**answer;

	/* old-style variable name? */
	if (!strchr(var, '.')) {
		fatalx_error_json_simple(1, "old-style variable names are not supported");
	}

	query[0] = "VAR";
	query[1] = upsname;
	query[2] = var;

	numq = 3;

	ret = upscli_get(ups, numq, query, &numa, &answer);

	if (ret < 0) {
		const char	*msg = NULL;
		int	msg_is_simple = 1;

		/* new var and old upsd?  try to explain the situation */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			msg = "variable unknown (old upsd detected)";
		} else {
			msg = upscli_strerror(ups);
			msg_is_simple = 0;
		}

		fatalx_error_json_simple(msg_is_simple, msg);
	}

	if (numa < numq) {
		char	msg[LARGEBUF];
		snprintf(msg, sizeof(msg), "insufficient data (got %" PRIuSIZE " args, need at least %" PRIuSIZE ")", numa, numq);
		fatalx_error_json_simple(1, msg);
	}

	if (output_json) {
		printf("\"");
		json_print_esc(answer[3]);
		printf("\"\n");
	} else {
		printf("%s\n", answer[3]);
	}
}

static void list_vars(void)
{
	int		ret;
	int		first = 1;
	size_t	numq, numa;
	const char	*query[4];
	char		**answer;

	query[0] = "VAR";
	query[1] = upsname;
	numq = 2;

	if (output_json) {
		printf("{\n");
	}

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {
		const char	*msg = NULL;
		int	msg_is_simple = 1;

		/* check for an old upsd */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			msg = "upsd is too old to support this query";
		} else {
			msg = upscli_strerror(ups);
			msg_is_simple = 0;
		}

		if (output_json) {
			if (msg_is_simple) {
				printf("  \"error\": \"%s\"\n}\n", msg);
			} else {
				printf("  \"error\": \"");
				json_print_esc(msg);
				printf("\"\n}\n");
			}
		}
		fatalx(EXIT_FAILURE, "Error: %s", msg);
	}

	while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {

		/* VAR <upsname> <varname> <val> */
		if (numa < 4) {
			char	msg[LARGEBUF];
			snprintf(msg, sizeof(msg), "insufficient data (got %" PRIuSIZE " args, need at least 4)", numa);
			if (output_json) {
				printf("  \"error\": \"%s\"\n}\n", msg);
			}
			fatalx(EXIT_FAILURE, "Error: %s", msg);
		}

		if (output_json) {
			if (!first) {
				printf(",\n");
			}
			printf("  \"");
			json_print_esc(answer[2]);
			printf("\": \"");
			json_print_esc(answer[3]);
			printf("\"");
			first = 0;
		} else {
			printf("%s: %s\n", answer[2], answer[3]);
		}
	}

	if (output_json) {
		printf("\n}\n");
	}
}

static void list_upses(int verbose)
{
	int		ret;
	int		first = 1;
	size_t	numq, numa;
	const char	*query[4];
	char		**answer;

	query[0] = "UPS";
	numq = 1;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {
		const char	*msg = NULL;
		int	msg_is_simple = 1;

		/* check for an old upsd */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			msg = "upsd is too old to support this query";
		} else {
			msg = upscli_strerror(ups);
			msg_is_simple = 0;
		}

		fatalx_error_json_simple(msg_is_simple, msg);
	}

	if (output_json) {
		if (verbose) {
			printf("{\n");
		} else {
			printf("[\n");
		}
	}

	while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {

		/* UPS <upsname> <description> */
		if (numa < 3) {
			char	msg[LARGEBUF];
			snprintf(msg, sizeof(msg), "insufficient data (got %" PRIuSIZE " args, need at least 3)", numa);
			if (output_json) {
				printf("  %s", verbose ? "" : "{");
				printf("\"error\": \"%s\"\n}\n", msg);
				if (!verbose)
					printf("]\n");
			}
			fatalx(EXIT_FAILURE, "Error: %s", msg);
		}

		if (output_json) {
			if (!first) {
				printf(",\n");
			}
			printf("  ");
			if (verbose) {
				printf("\"");
				json_print_esc(answer[1]);
				printf("\": \"");
				json_print_esc(answer[2]);
				printf("\"");
			} else {
				printf("\"");
				json_print_esc(answer[1]);
				printf("\"");
			}
			first = 0;
		} else if(verbose) {
			printf("%s: %s\n", answer[1], answer[2]);
		} else {
			printf("%s\n", answer[1]);
		}
	}

	if (output_json) {
		if (verbose) {
			printf("\n}\n");
		} else {
			printf("\n]\n");
		}
	}
}

static void list_clients(const char *devname)
{
	int		ret;
	int		first = 1;
	size_t	numq, numa;
	const char	*query[4];
	char		**answer;

	query[0] = "CLIENT";
	query[1] = devname;
	numq = 2;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {
		const char	*msg = NULL;
		int	msg_is_simple = 1;

		/* check for an old upsd */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			msg = "upsd is too old to support this query";
		} else {
			msg = upscli_strerror(ups);
			msg_is_simple = 0;
		}

		fatalx_error_json_simple(msg_is_simple, msg);
	}

	if (output_json) {
		printf("[\n");
	}

	while ((ret=upscli_list_next(ups, numq, query, &numa, &answer)) == 1) {

		/* CLIENT <upsname> <address> */
		if (numa < 3) {
			char	msg[LARGEBUF];
			snprintf(msg, sizeof(msg), "insufficient data (got %" PRIuSIZE " args, need at least 3)", numa);
			if (output_json) {
				printf("  {\"error\": \"%s\"\n}\n]\n", msg);
			}
			fatalx(EXIT_FAILURE, "Error: %s", msg);
		}

		if (output_json) {
			if (!first) {
				printf(",\n");
			}
			printf("  \"");
			json_print_esc(answer[2]);
			printf("\"");
			first = 0;
		} else {
			printf("%s\n", answer[2]);
		}
	}

	if (output_json) {
		printf("\n]\n");
	}
}

static void clean_exit(void)
{
	if (ups) {
		upscli_disconnect(ups);
	}

	free(upsname);
	free(hostname);
	free(ups);
}

int main(int argc, char **argv)
{
	int	i = 0;
	uint16_t	port;
	int	varlist = 0, clientlist = 0, verbose = 0;
	const char	*prog = xbasename(argv[0]);
	const char	*net_connect_timeout = NULL;
	char	*s = NULL;

	/* NOTE: Caller must `export NUT_DEBUG_LEVEL` to see debugs for upsc
	 * and NUT methods called from it. This line aims to just initialize
	 * the subsystem, and set initial timestamp. Debugging the client is
	 * primarily of use to developers, so is not exposed via `-D` args.
	 */
	s = getenv("NUT_DEBUG_LEVEL");
	if (s && str_to_int(s, &i, 10) && i > 0) {
		nut_debug_level = i;
	}
	upsdebugx(1, "Starting NUT client: %s", prog);

#if (defined NUT_PLATFORM_AIX) && (defined ENABLE_SHARED_PRIVATE_LIBS) && ENABLE_SHARED_PRIVATE_LIBS
	callback_upsconf_args = do_upsconf_args;
#endif

	while ((i = getopt(argc, argv, "+hlLcVW:j")) != -1) {

		switch (i)
		{
		case 'L':
			verbose = 1;
			goto fallthrough_case_l;
		case 'l':
		fallthrough_case_l:
			varlist = 1;
			break;

		case 'c':
			clientlist = 1;
			break;

		case 'j':
			output_json = 1;
			break;

		case 'V':
			/* just show the version and optional
			 * CONFIG_FLAGS banner if available */
			print_banner_once(prog, 1);
			nut_report_config_flags();
			exit(EXIT_SUCCESS);

		case 'W':
			net_connect_timeout = optarg;
			break;

		case 'h':
		default:
			usage(prog);
			exit(EXIT_SUCCESS);
		}
	}

	if (upscli_init_default_connect_timeout(net_connect_timeout, NULL, UPSCLI_DEFAULT_CONNECT_TIMEOUT) < 0) {
		char	msg[LARGEBUF];
		snprintf(msg, sizeof(msg), "invalid network timeout: %s", net_connect_timeout);
		fatalx_error_json_simple(0, msg);
	}

	argc -= optind;
	argv += optind;

	/* be a good little client that cleans up after itself */
	atexit(clean_exit);

	if (varlist) {
		if (upscli_splitaddr(argv[0] ? argv[0] : "localhost", &hostname, &port) != 0) {
			fatalx_error_json_simple(0, "invalid hostname.\nRequired format: [hostname[:port]]");
		}
	} else {
		if (upscli_splitname(argv[0], &upsname, &hostname, &port) != 0) {
			fatalx_error_json_simple(0, "invalid UPS definition.\nRequired format: upsname[@hostname[:port]]");
		}
	}
	upsdebugx(1, "upsname='%s' hostname='%s' port='%" PRIu16 "'",
		NUT_STRARG(upsname), NUT_STRARG(hostname), port);

	ups = (UPSCONN_t *)xmalloc(sizeof(*ups));

	if (upscli_connect(ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0) {
		fatalx_error_json_simple(0, upscli_strerror(ups));
	}

	if (varlist) {
		upsdebugx(1, "Calling list_upses()");
		list_upses(verbose);
		exit(EXIT_SUCCESS);
	}

	if (clientlist) {
		upsdebugx(1, "Calling list_clients()");
		list_clients(upsname);
		exit(EXIT_SUCCESS);
	}

	if (argc > 1) {
		upsdebugx(1, "Calling printvar(%s)", argv[1]);
		printvar(argv[1]);
	} else {
		upsdebugx(1, "Calling list_vars()");
		list_vars();
	}

	exit(EXIT_SUCCESS);
}


/* Formal do_upsconf_args implementation to satisfy linker on AIX */
#if (defined NUT_PLATFORM_AIX)
void do_upsconf_args(char *upsname, char *var, char *val) {
	fatalx(EXIT_FAILURE, "INTERNAL ERROR: formal do_upsconf_args called");
}
#endif  /* end of #if (defined NUT_PLATFORM_AIX) */
