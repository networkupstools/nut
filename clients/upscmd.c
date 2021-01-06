/* upscmd - simple "client" to test instant commands via upsd

   Copyright (C)
     2000  Russell Kroll <rkroll@exploits.org>
     2019  EATON (author: Arnaud Quette <ArnaudQuette@eaton.com>)

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

#include <pwd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "nut_stdint.h"
#include "upsclient.h"

static char			*upsname = NULL, *hostname = NULL;
static UPSCONN_t	*ups = NULL;
static int			tracking_enabled = 0;
static unsigned int	timeout = DEFAULT_TRACKING_TIMEOUT;

struct list_t {
	char	*name;
	struct list_t	*next;
};

static void usage(const char *prog)
{
	printf("Network UPS Tools upscmd %s\n\n", UPS_VERSION);
	printf("usage: %s [-h]\n", prog);
	printf("       %s [-l <ups>]\n", prog);
	printf("       %s [-u <username>] [-p <password>] [-w] [-t <timeout>] <ups> <command> [<value>]\n\n", prog);
	printf("Administration program to initiate instant commands on UPS hardware.\n");
	printf("\n");
	printf("  -h		display this help text\n");
	printf("  -l <ups>	show available commands on UPS <ups>\n");
	printf("  -u <username>	set username for command authentication\n");
	printf("  -p <password>	set password for command authentication\n");
	printf("  -w            wait for the completion of command by the driver\n");
	printf("                and return its actual result from the device\n");
	printf("  -t <timeout>	set a timeout when using -w (in seconds, default: %u)\n", DEFAULT_TRACKING_TIMEOUT);
	printf("\n");
	printf("  <ups>		UPS identifier - <upsname>[@<hostname>[:<port>]]\n");
	printf("  <command>	Valid instant command - test.panel.start, etc.\n");
	printf("  [<value>]	Additional data for command - number of seconds, etc.\n");
}

static void print_cmd(char *cmdname)
{
	int		ret;
	unsigned int	numq, numa;
	const char	*query[4];
	char		**answer;

	query[0] = "CMDDESC";
	query[1] = upsname;
	query[2] = cmdname;
	numq = 3;

	ret = upscli_get(ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq)) {
		printf("%s\n", cmdname);
		return;
	}

	/* CMDDESC <upsname> <cmdname> <desc> */
	printf("%s - %s\n", cmdname, answer[3]);
}

static void listcmds(void)
{
	int		ret;
	unsigned int	numq, numa;
	const char	*query[4];
	char		**answer;
	struct list_t	*lhead = NULL, *llast = NULL, *ltmp, *lnext;

	query[0] = "CMD";
	query[1] = upsname;
	numq = 2;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {

		/* old upsd = no way to continue */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			fatalx(EXIT_FAILURE, "Error: upsd is too old to support this query");
		}

		fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
	}

	while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {

		/* CMD <upsname> <cmdname> */
		if (numa < 3) {
			fatalx(EXIT_FAILURE, "Error: insufficient data (got %d args, need at least 3)", numa);
		}

		/* we must first read the entire list of commands,
		   before we can start reading the descriptions */

		ltmp = xcalloc(1, sizeof(*ltmp));
		ltmp->name = xstrdup(answer[2]);

		if (llast) {
			llast->next = ltmp;
		} else {
			lhead = ltmp;
		}

		llast = ltmp;
	}

	/* walk the list and try to get descriptions, freeing as we go */
	printf("Instant commands supported on UPS [%s]:\n\n", upsname);

	for (ltmp = lhead; ltmp; ltmp = lnext) {
		lnext = ltmp->next;

		print_cmd(ltmp->name);

		free(ltmp->name);
		free(ltmp);
	}
}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic push
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
static void do_cmd(char **argv, const int argc)
{
	int		cmd_complete = 0;
	char	buf[SMALLBUF];
	char	tracking_id[UUID4_LEN];
	time_t	start, now;

	if (argc > 1) {
		snprintf(buf, sizeof(buf), "INSTCMD %s %s %s\n", upsname, argv[0], argv[1]);
	} else {
		snprintf(buf, sizeof(buf), "INSTCMD %s %s\n", upsname, argv[0]);
	}

	if (upscli_sendline(ups, buf, strlen(buf)) < 0) {
		fatalx(EXIT_FAILURE, "Can't send instant command: %s", upscli_strerror(ups));
	}

	if (upscli_readline(ups, buf, sizeof(buf)) < 0) {
		fatalx(EXIT_FAILURE, "Instant command failed: %s", upscli_strerror(ups));
	}

	/* verify answer */
	if (strncmp(buf, "OK", 2) != 0) {
		fatalx(EXIT_FAILURE, "Unexpected response from upsd: %s", buf);
	}

	/* check for status tracking id */
	if (
		!tracking_enabled ||
		/* sanity check on the size: "OK TRACKING " + UUID4_LEN */
		strlen(buf) != (UUID4_LEN - 1 + strlen("OK TRACKING "))
	) {
		/* reply as usual */
		fprintf(stderr, "%s\n", buf);
		return;
	}

	snprintf(tracking_id, sizeof(tracking_id), "%s", buf + strlen("OK TRACKING "));
	time(&start);

	/* send status tracking request, looping if status is PENDING */
	while (!cmd_complete) {

		/* check for timeout */
		time(&now);
		if (difftime(now, start) >= timeout)
			fatalx(EXIT_FAILURE, "Can't receive status tracking information: timeout");

		snprintf(buf, sizeof(buf), "GET TRACKING %s\n", tracking_id);

		if (upscli_sendline(ups, buf, strlen(buf)) < 0)
			fatalx(EXIT_FAILURE, "Can't send status tracking request: %s", upscli_strerror(ups));

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
/* Note for gating macros above: unsuffixed HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP
 * means support of contexts both inside and outside function body, so the push
 * above and pop below (outside this finction) are not used.
 */
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
/* Note that the individual warning pragmas for use inside function bodies
 * are named without a _INSIDEFUNC suffix, for simplicity and legacy reasons
 */
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
		/* and get status tracking reply */
		assert(timeout < LONG_MAX);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic pop
#endif

		if (upscli_readline_timeout(ups, buf, sizeof(buf), (long)timeout) < 0)
			fatalx(EXIT_FAILURE, "Can't receive status tracking information: %s", upscli_strerror(ups));

		if (strncmp(buf, "PENDING", 7))
			cmd_complete = 1;
		else
			/* wait a second before retrying */
			sleep(1);
	}

	fprintf(stderr, "%s\n", buf);
}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic pop
#endif

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
	int	i, ret, port;
	int	have_un = 0, have_pw = 0, cmdlist = 0;
	char	buf[SMALLBUF * 2], username[SMALLBUF], password[SMALLBUF];
	const char	*prog = xbasename(argv[0]);

	while ((i = getopt(argc, argv, "+lhu:p:t:wV")) != -1) {

		switch (i)
		{
		case 'l':
			cmdlist = 1;
			break;

		case 'u':
			snprintf(username, sizeof(username), "%s", optarg);
			have_un = 1;
			break;

		case 'p':
			snprintf(password, sizeof(password), "%s", optarg);
			have_pw = 1;
			break;

		case 't':
			if (!str_to_uint(optarg, &timeout, 10))
				fatal_with_errno(EXIT_FAILURE, "Could not convert the provided value for timeout ('-t' option) to unsigned int");
			break;

		case 'w':
			tracking_enabled = 1;
			break;

		case 'V':
			fatalx(EXIT_SUCCESS, "Network UPS Tools upscmd %s", UPS_VERSION);
#ifndef HAVE___ATTRIBUTE__NORETURN
			exit(EXIT_SUCCESS);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif

		case 'h':
		default:
			usage(prog);
			exit(EXIT_SUCCESS);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage(prog);
		exit(EXIT_SUCCESS);
	}

	/* be a good little client that cleans up after itself */
	atexit(clean_exit);

	if (upscli_splitname(argv[0], &upsname, &hostname, &port) != 0) {
		fatalx(EXIT_FAILURE, "Error: invalid UPS definition.  Required format: upsname[@hostname[:port]]");
	}

	ups = xcalloc(1, sizeof(*ups));

	if (upscli_connect(ups, hostname, port, 0) < 0) {
		fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
	}

	if (cmdlist) {
		listcmds();
		exit(EXIT_SUCCESS);
	}

	if (argc < 2) {
		usage(prog);
		exit(EXIT_SUCCESS);
	}

	/* also fallback for old command names */
	if (!strchr(argv[1], '.')) {
		fatalx(EXIT_FAILURE, "Error: old command names are not supported");
	}

	if (!have_un) {
		struct passwd	*pw;

		memset(username, '\0', sizeof(username));
		pw = getpwuid(getuid());

		if (pw) {
			printf("Username (%s): ", pw->pw_name);
		} else {
			printf("Username: ");
		}

		if (!fgets(username, sizeof(username), stdin)) {
			fatalx(EXIT_FAILURE, "Error reading from stdin!");
		}

		/* deal with that pesky newline */
		if (strlen(username) > 1) {
			username[strlen(username) - 1] = '\0';
		} else {
			if (!pw) {
				fatalx(EXIT_FAILURE, "No username available - even tried getpwuid");
			}

			snprintf(username, sizeof(username), "%s", pw->pw_name);
		}
	}

	/* getpass leaks slightly - use -p when testing in valgrind */
	if (!have_pw) {
		/* using getpass or getpass_r might not be a
		   good idea here (marked obsolete in POSIX) */
		char	*pwtmp = GETPASS("Password: ");

		if (!pwtmp) {
			fatalx(EXIT_FAILURE, "getpass failed: %s", strerror(errno));
		}

		snprintf(password, sizeof(password), "%s", pwtmp);
	}

	snprintf(buf, sizeof(buf), "USERNAME %s\n", username);

	if (upscli_sendline(ups, buf, strlen(buf)) < 0) {
		fatalx(EXIT_FAILURE, "Can't set username: %s", upscli_strerror(ups));
	}

	ret = upscli_readline(ups, buf, sizeof(buf));

	if (ret < 0) {
		if (upscli_upserror(ups) != UPSCLI_ERR_UNKCOMMAND) {
			fatalx(EXIT_FAILURE, "Set username failed: %s", upscli_strerror(ups));
		}

		fatalx(EXIT_FAILURE,
			"Set username failed due to an unknown command.\n"
			"You probably need to upgrade upsd.");
	}

	snprintf(buf, sizeof(buf), "PASSWORD %s\n", password);

	if (upscli_sendline(ups, buf, strlen(buf)) < 0) {
		fatalx(EXIT_FAILURE, "Can't set password: %s", upscli_strerror(ups));
	}

	if (upscli_readline(ups, buf, sizeof(buf)) < 0) {
		fatalx(EXIT_FAILURE, "Set password failed: %s", upscli_strerror(ups));
	}

	/* enable status tracking ID */
	if (tracking_enabled) {

		snprintf(buf, sizeof(buf), "SET TRACKING ON\n");

		if (upscli_sendline(ups, buf, strlen(buf)) < 0) {
			fatalx(EXIT_FAILURE, "Can't enable command status tracking: %s", upscli_strerror(ups));
		}

		if (upscli_readline(ups, buf, sizeof(buf)) < 0) {
			fatalx(EXIT_FAILURE, "Enabling command status tracking failed: %s", upscli_strerror(ups));
		}

		/* Verify the result */
		if (strncmp(buf, "OK", 2) != 0) {
			fatalx(EXIT_FAILURE, "Enabling command status tracking failed. upsd answered: %s", buf);
		}
	}

	do_cmd(&argv[1], argc - 1);

	exit(EXIT_SUCCESS);
}


/* Formal do_upsconf_args implementation to satisfy linker on AIX */
#if (defined NUT_PLATFORM_AIX)
void do_upsconf_args(char *upsname, char *var, char *val) {
        fatalx(EXIT_FAILURE, "INTERNAL ERROR: formal do_upsconf_args called");
}
#endif  /* end of #if (defined NUT_PLATFORM_AIX) */
