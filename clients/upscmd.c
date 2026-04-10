/* upscmd - simple "client" to test instant commands via upsd

   Copyright (C)
     2000  Russell Kroll <rkroll@exploits.org>
     2019  EATON (author: Arnaud Quette <ArnaudQuette@eaton.com>)
     2020-2026  Jim Klimov <jimklimov+nut@gmail.com>

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
#include <pwd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#else	/* WIN32 */
#include "wincompat.h"
#endif	/* WIN32 */

#include "nut_stdint.h"
#include "upsclient.h"

/* name-swap in libupsclient consumer to simplify the look of code base */
#define builtin_setproctag(x)	setproctag(x)
#define setproctag(x)	do { builtin_setproctag(x); upscli_upslog_setproctag(x, nut_common_cookie()); } while(0)

/* network timeout for initial connection, in seconds */
#define UPSCLI_DEFAULT_CONNECT_TIMEOUT	"10"

static char			*upsname = NULL, *hostname = NULL;
static UPSCONN_t	*ups = NULL;
static int			tracking_enabled = 0;
static unsigned int	timeout = DEFAULT_TRACKING_TIMEOUT;

struct list_t {
	char	*name;
	struct list_t	*next;
};

/* For getopt loops; should match usage documented below: */
static const char	optstring[] = "+Dlhu:p:t:wVW:";

static void help(const char *prog)
{
	print_banner_once(prog, 2);
	printf("NUT administration client program to initiate instant commands on UPS hardware.\n");

	printf("\nusage: %s [-h]\n", prog);
	printf("       %s [-l <ups>]\n", prog);
	printf("       %s [-u <username>] [-p <password>] [-w] [-t <timeout>] <ups> <command> [<value>]\n\n", prog);
	printf("\n");
	printf("  -l <ups>	show available commands on UPS <ups>\n");
	printf("  -u <username>	set username for command authentication\n");
	printf("  -p <password>	set password for command authentication\n");
	printf("  -w            wait for the completion of command by the driver\n");
	printf("                and return its actual result from the device\n");
	printf("  -t <timeout>	set a timeout when using -w (in seconds, default: %d)\n", DEFAULT_TRACKING_TIMEOUT);
	printf("\n");
	printf("  <ups>		UPS identifier - <upsname>[@<hostname>[:<port>]]\n");
	printf("  <command>	Valid instant command - test.panel.start, etc.\n");
	printf("  [<value>]	Additional data for command - number of seconds, etc.\n");
	printf("\nCommon arguments:\n");
	printf("  -V         - display the version of this software\n");
	printf("  -W <secs>  - network timeout for initial connections (default: %s)\n",
		UPSCLI_DEFAULT_CONNECT_TIMEOUT);
	printf("  -D         - raise debugging level\n");
	printf("  -h         - display this help text\n");

	nut_report_config_flags();
	upscli_report_build_details();

	printf("\n%s", suggest_doc_links(prog, "upsd.users"));
}

static void print_cmd(char *cmdname)
{
	int		ret;
	size_t	numq, numa;
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
	size_t	numq, numa;
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
			fatalx(EXIT_FAILURE, "Error: insufficient data (got %" PRIuSIZE " args, need at least 3)", numa);
		}

		/* we must first read the entire list of commands,
		 * before we can start reading the descriptions */

		ltmp = (struct list_t *)xcalloc(1, sizeof(*ltmp));
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
		char	*e = getenv("NUT_QUIET_OK_NOTRACKING");
		int	lvl = 0;	/* Visible by default */

		if (e && !strcmp(e, "true"))
			lvl = 1;	/* Hide into debuging if asked to */

		/* reply as usual */
		fprintf(stderr, "%s\n", buf);
		upsdebugx(lvl, "%s: 'OK' only means the NUT data server accepted\n"
			"the request as valid, but as we did not wait for result,\n"
			"we do not know if it was handled in fact.%s",
			lvl ? __func__ : "WARNING",
			lvl ? "" :
			"\nYou can export NUT_QUIET_OK_NOTRACKING=true to hide this message,\n"
			"or use -w [-t SEC] option(s) to track the actual outcome."
			);
		return;
	}

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
	/* From the check above, we know that we have exactly UUID4_LEN chars
	 * (aka sizeof(tracking_id)) in the buf after "OK TRACKING " prefix,
	 * plus the null-byte.
	 */
	assert (UUID4_LEN == 1 + snprintf(tracking_id, sizeof(tracking_id), "%s", buf + strlen("OK TRACKING ")));
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic pop
#endif
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

	upscli_cleanup();

	upsdebugx(1, "%s: finished, exiting", __func__);
}

int main(int argc, char **argv)
{
	/* Make sure all related logs (copies of code that may
	 * be spread in different NUT common libs) start on the
	 * same note; execute this call before everything else,
	 * at the cost of a temporary otherwise useless variable. */
	const struct timeval	*upslog_start_tmp = upscli_upslog_start_sync(upslog_start_sync(NULL), nut_common_cookie());
	int	opt_ret = 0;
	uint16_t	port;
	ssize_t	ret;
	int	have_un = 0, have_pw = 0, cmdlist = 0;
	char	buf[SMALLBUF * 2], username[SMALLBUF], password[SMALLBUF];
	const char	*prog = getprogname_argv0_default(argc > 0 ? argv[0] : NULL, "upscmd");
	const char	*net_connect_timeout = NULL;

	NUT_UNUSED_VARIABLE(upslog_start_tmp);
	upscli_upslog_setprocname(xstrdup(getmyprocname()), nut_common_cookie());

	/* NOTE: Debugging the client is primarily of use to developers, so
	 *  it was not at all exposed via `-D[D...]` args until NUT v2.8.5.
	 *  Since earlier 2.8.x releases, caller could `export NUT_DEBUG_LEVEL`
	 *  to see debugs for the client and for NUT methods called from it.
	 */

	/* Parse command line options -- First loop: only get debug level */
	/* Suppress error messages, for now -- leave them to the second loop. */
	opterr = 0;
	while ((opt_ret = getopt(argc, argv, optstring)) != -1) {
		if (opt_ret == 'D')
			nut_debug_level++;
	}

	if (!nut_debug_level) {
		char	*s = getenv("NUT_DEBUG_LEVEL");
		int	l;
		if (s && str_to_int(s, &l, 10) && l > 0) {
			nut_debug_level = l;
			upsdebugx(1, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
				"since none was requested by command-line options", l);
		}	/* else follow -D settings */
	}

	/* These lines aim to just initialize the logging subsystem, and set
	 * initial timestamp, for the eventuality that debugs would be printed:
	 */
	upscli_upslog_set_debug_level(nut_debug_level, nut_common_cookie());
	setproctag(prog);
	upsdebugx(1, "Starting NUT client: %s", prog);

#if (defined NUT_PLATFORM_AIX) && (defined ENABLE_SHARED_PRIVATE_LIBS) && ENABLE_SHARED_PRIVATE_LIBS
	callback_upsconf_args = do_upsconf_args;
#endif

	/* Parse command line options -- Second loop: everything else */
	/* Restore error messages... */
	opterr = 1;
	/* ...and index of the item to be processed by getopt(). */
	optind = 1;
	while ((opt_ret = getopt(argc, argv, optstring)) != -1) {

		switch (opt_ret)
		{
		case 'D': break;	/* See nut_debug_level handled above */
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
			help(prog);
			exit(EXIT_SUCCESS);
		}
	}

	if (upscli_init_default_connect_timeout(net_connect_timeout, NULL, UPSCLI_DEFAULT_CONNECT_TIMEOUT) < 0) {
		fatalx(EXIT_FAILURE, "Error: invalid network timeout: %s",
			net_connect_timeout);
	}

	/* Simplify offset numbering to look at command-line
	 * arguments (if any) after the options checked above */
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		help(prog);
		exit(EXIT_SUCCESS);
	}

	/* be a good little client that cleans up after itself */
	atexit(clean_exit);

	if (upscli_splitname(argv[0], &upsname, &hostname, &port) != 0) {
		fatalx(EXIT_FAILURE, "Error: invalid UPS definition.  Required format: upsname[@hostname[:port]]");
	}
	setproctag(argv[0]);	/* ups[@host[:port]] */

	ups = (UPSCONN_t *)xcalloc(1, sizeof(*ups));

	if (upscli_connect(ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0) {
		fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
	}

	if (cmdlist) {
		listcmds();
		exit(EXIT_SUCCESS);
	}

	if (argc < 2) {
		help(prog);
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
		 * good idea here (marked obsolete in POSIX) */
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

	/* Not a sub-process (do not let common::proctag_cleanup() mis-report us as such) */
	setproctag(prog);
	exit(EXIT_SUCCESS);
}


/* Formal do_upsconf_args implementation to satisfy linker on AIX */
#if (defined NUT_PLATFORM_AIX)
void do_upsconf_args(char *upsname, char *var, char *val) {
	fatalx(EXIT_FAILURE, "INTERNAL ERROR: formal do_upsconf_args called");
}
#endif  /* end of #if (defined NUT_PLATFORM_AIX) */
