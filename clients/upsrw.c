/* upsrw - simple client for read/write variable access (formerly upsct2)

   Copyright (C)
     1999  Russell Kroll <rkroll@exploits.org>
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
#include <netinet/in.h>
#include <sys/socket.h>
#include <limits.h>

#include "upsclient.h"
#include "extstate.h"

static char			*upsname = NULL, *hostname = NULL;
static UPSCONN_t	*ups = NULL;
static int			tracking_enabled = 0;
static unsigned int	timeout = DEFAULT_TRACKING_TIMEOUT;

struct list_t {
	char	*name;
	struct	list_t	*next;
};

static void usage(const char *prog)
{
	printf("Network UPS Tools %s %s\n\n", prog, UPS_VERSION);
	printf("usage: %s [-h]\n", prog);
	printf("       %s [-s <variable>] [-u <username>] [-p <password>] [-w] [-t <timeout>] <ups>\n\n", prog);
	printf("Demo program to set variables within UPS hardware.\n");
	printf("\n");
	printf("  -h            display this help text\n");
	printf("  -s <variable>	specify variable to be changed\n");
	printf("		use -s VAR=VALUE to avoid prompting for value\n");
	printf("  -u <username> set username for command authentication\n");
	printf("  -p <password> set password for command authentication\n");
	printf("  -w            wait for the completion of setting by the driver\n");
	printf("                and return its actual result from the device\n");
	printf("  -t <timeout>	set a timeout when using -w (in seconds, default: %u)\n", DEFAULT_TRACKING_TIMEOUT);
	printf("\n");
	printf("  <ups>         UPS identifier - <upsname>[@<hostname>[:<port>]]\n");
	printf("\n");
	printf("Call without -s to show all possible read/write variables.\n");
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

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic push
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
static void do_set(const char *varname, const char *newval)
{
	int		cmd_complete = 0;
	char	buf[SMALLBUF], enc[SMALLBUF];
	char	tracking_id[UUID4_LEN];
	time_t	start, now;

	snprintf(buf, sizeof(buf), "SET VAR %s %s \"%s\"\n", upsname, varname, pconf_encode(newval, enc, sizeof(enc)));

	if (upscli_sendline(ups, buf, strlen(buf)) < 0) {
		fatalx(EXIT_FAILURE, "Can't set variable: %s", upscli_strerror(ups));
	}

	if (upscli_readline(ups, buf, sizeof(buf)) < 0) {
		fatalx(EXIT_FAILURE, "Set variable failed: %s", upscli_strerror(ups));
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

static void do_setvar(const char *varname, char *uin, const char *pass)
{
	char	newval[SMALLBUF], temp[SMALLBUF * 2], user[SMALLBUF], *ptr;
	struct passwd	*pw;

	if (uin) {
		snprintf(user, sizeof(user), "%s", uin);
	} else {
		memset(user, '\0', sizeof(user));

		pw = getpwuid(getuid());

		if (pw) {
			printf("Username (%s): ", pw->pw_name);
		} else {
			printf("Username: ");
		}

		if (fgets(user, sizeof(user), stdin) == NULL) {
			upsdebug_with_errno(LOG_INFO, "%s", __func__);
		}

		/* deal with that pesky newline */
		if (strlen(user) > 1) {
			user[strlen(user) - 1] = '\0';
		} else {
			if (!pw) {
				fatalx(EXIT_FAILURE, "No username available - even tried getpwuid");
			}

			snprintf(user, sizeof(user), "%s", pw->pw_name);
		}
	}

	/* leaks - use -p when running in valgrind */
	if (!pass) {
		pass = GETPASS("Password: " );

		if (!pass) {
			fatal_with_errno(EXIT_FAILURE, "getpass failed");
		}
	}

	/* Check if varname is in VAR=VALUE form */
	if ((ptr = strchr(varname, '=')) != NULL) {
		*ptr++ = 0;
		snprintf(newval, sizeof(newval), "%s", ptr);
	} else {
		printf("Enter new value for %s: ", varname);
		fflush(stdout);
		if (fgets(newval, sizeof(newval), stdin) == NULL) {
			upsdebug_with_errno(LOG_INFO, "%s", __func__);
		}
		newval[strlen(newval) - 1] = '\0';
	}

	snprintf(temp, sizeof(temp), "USERNAME %s\n", user);

	if (upscli_sendline(ups, temp, strlen(temp)) < 0) {
		fatalx(EXIT_FAILURE, "Can't set username: %s", upscli_strerror(ups));
	}

	if (upscli_readline(ups, temp, sizeof(temp)) < 0) {

		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			fatalx(EXIT_FAILURE, "Set username failed due to an unknown command. You probably need to upgrade upsd.");
		}

		fatalx(EXIT_FAILURE, "Set username failed: %s", upscli_strerror(ups));
	}

	snprintf(temp, sizeof(temp), "PASSWORD %s\n", pass);

	if (upscli_sendline(ups, temp, strlen(temp)) < 0) {
		fatalx(EXIT_FAILURE, "Can't set password: %s", upscli_strerror(ups));
	}

	if (upscli_readline(ups, temp, sizeof(temp)) < 0) {
		fatalx(EXIT_FAILURE, "Set password failed: %s", upscli_strerror(ups));
	}

	/* no upsname means die */
	if (!upsname) {
		fatalx(EXIT_FAILURE, "Error: a UPS name must be specified (upsname[@hostname[:port]])");
	}

	/* old variable names are no longer supported */
	if (!strchr(varname, '.')) {
		fatalx(EXIT_FAILURE, "Error: old variable names are not supported");
	}

	/* enable status tracking ID */
	if (tracking_enabled) {

		snprintf(temp, sizeof(temp), "SET TRACKING ON\n");

		if (upscli_sendline(ups, temp, strlen(temp)) < 0) {
			fatalx(EXIT_FAILURE, "Can't enable set variable status tracking: %s", upscli_strerror(ups));
		}

		if (upscli_readline(ups, temp, sizeof(temp)) < 0) {
			fatalx(EXIT_FAILURE, "Enabling set variable status tracking failed: %s", upscli_strerror(ups));
		}

		/* Verify the result */
		if (strncmp(temp, "OK", 2) != 0) {
			fatalx(EXIT_FAILURE, "Enabling set variable status tracking failed. upsd answered: %s", temp);
		}
	}

	do_set(varname, newval);
}

static const char *get_data(const char *type, const char *varname)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer;
	const char	*query[4];

	query[0] = type;
	query[1] = upsname;
	query[2] = varname;

	numq = 3;

	ret = upscli_get(ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq)) {
		return NULL;
	}

	/* <type> <upsname> <varname> <desc> */
	return answer[3];
}

static void do_string(const char *varname, const int len)
{
	const char	*val;

	val = get_data("VAR", varname);

	if (!val) {
		fatalx(EXIT_FAILURE, "do_string: can't get current value of %s", varname);
	}

	printf("Type: STRING\n");
	printf("Maximum length: %d\n", len);
	printf("Value: %s\n", val);
}

static void do_number(const char *varname)
{
	const char	*val;

	val = get_data("VAR", varname);

	if (!val) {
		fatalx(EXIT_FAILURE, "do_number: can't get current value of %s", varname);
	}

	printf("Type: NUMBER\n");
	printf("Value: %s\n", val);
}

/**
 * Display ENUM information
 * @param varname the name of the NUT variable
 * @param vartype the type of the NUT variable (ST_FLAG_STRING, ST_FLAG_NUMBER
 * @param len the length of the NUT variable, if type == ST_FLAG_STRING
 */
static void do_enum(const char *varname, const int vartype, const int len)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer, buf[SMALLBUF];
	const char	*query[4], *val;

	/* get current value */
	val = get_data("VAR", varname);

	if (!val) {
		fatalx(EXIT_FAILURE, "do_enum: can't get current value of %s", varname);
	}

	snprintf(buf, sizeof(buf), "%s", val);

	query[0] = "ENUM";
	query[1] = upsname;
	query[2] = varname;
	numq = 3;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {
		fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
	}

	ret = upscli_list_next(ups, numq, query, &numa, &answer);

	/* Fallback for older upsd versions */
	if (vartype != ST_FLAG_NONE)
		printf("Type: ENUM %s\n", (vartype == ST_FLAG_STRING)?"STRING":"NUMBER");
	else
		printf("Type: ENUM\n");

	if (vartype == ST_FLAG_STRING)
		printf("Maximum length: %d\n", len);

	while (ret == 1) {

		/* ENUM <upsname> <varname> <value> */

		if (numa < 4) {
			fatalx(EXIT_FAILURE, "Error: insufficient data (got %d args, need at least 4)", numa);
		}

		printf("Option: \"%s\"", answer[3]);

		if (!strcmp(answer[3], buf)) {
			printf(" SELECTED");
		}

		printf("\n");

		ret = upscli_list_next(ups, numq, query, &numa, &answer);
	}
}

static void do_range(const char *varname)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer;
	const char	*query[4], *val;
	int ival, min, max;

	/* get current value */
	val = get_data("VAR", varname);

	if (!val) {
		fatalx(EXIT_FAILURE, "do_range: can't get current value of %s", varname);
	}

	ival = atoi(val);

	query[0] = "RANGE";
	query[1] = upsname;
	query[2] = varname;
	numq = 3;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {
		fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
	}

	ret = upscli_list_next(ups, numq, query, &numa, &answer);

	/* Ranges implies a type "NUMBER" */
	printf("Type: RANGE NUMBER\n");

	while (ret == 1) {

		/* RANGE <upsname> <varname> <min> <max> */

		if (numa < 5) {
			fatalx(EXIT_FAILURE, "Error: insufficient data (got %d args, need at least 4)", numa);
		}

		min = atoi(answer[3]);
		max = atoi(answer[4]);

		printf("Option: \"%i-%i\"", min, max);

		if ((ival >= min) && (ival <= max)) {
			printf(" SELECTED");
		}

		printf("\n");

		ret = upscli_list_next(ups, numq, query, &numa, &answer);
	}
}

static void do_type(const char *varname)
{
	int	ret;
	int is_enum = 0; /* 1 if ENUM; FIXME: add a boolean type in common.h */
	unsigned int	i, numq, numa;
	char	**answer;
	const char	*query[4];

	query[0] = "TYPE";
	query[1] = upsname;
	query[2] = varname;
	numq = 3;

	ret = upscli_get(ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq)) {
		printf("Unknown type\n");
		return;
	}

	/* TYPE <upsname> <varname> <type>... */
	for (i = 3; i < numa; i++) {

		/* ENUM can be NUMBER or STRING
		 * just flag it for latter processing */
		if (!strcasecmp(answer[i], "ENUM")) {
			is_enum = 1;
			continue;
		}

		if (!strcasecmp(answer[i], "RANGE")) {
			do_range(varname);
			return;
		}

		if (!strncasecmp(answer[i], "STRING:", 7)) {

			char	*len = answer[i] + 7;
			int	length = strtol(len, NULL, 10);

			if (is_enum == 1)
				do_enum(varname, ST_FLAG_STRING, length);
			else
				do_string(varname, length);
			return;

		}

		if (!strcasecmp(answer[i], "NUMBER")) {
			if (is_enum == 1)
				do_enum(varname, ST_FLAG_NUMBER, 0);
			else
				do_number(varname);
			return;
		}

		/* ignore this one */
		if (!strcasecmp(answer[i], "RW")) {
			continue;
		}

		printf("Type: %s (unrecognized)\n", answer[i]);
	}
	/* Fallback for older upsd versions, where STRING|NUMBER is not
	 * appended to ENUM */
	if (is_enum == 1)
		do_enum(varname, ST_FLAG_NONE, 0);
}

static void print_rw(const char *varname)
{
	const char	*tmp;

	printf("[%s]\n", varname);

	tmp = get_data("DESC", varname);

	if (tmp) {
		printf("%s\n", tmp);
	} else {
		printf("Description unavailable\n");
	}

	do_type(varname);

	printf("\n");
}

static void print_rwlist(void)
{
	int	ret;
	unsigned int	numq, numa;
	const char	*query[2];
	char	**answer;
	struct	list_t	*lhead, *llast, *ltmp, *lnext;

	/* the upsname is now required */
	if (!upsname) {
		fatalx(EXIT_FAILURE, "Error: a UPS name must be specified (upsname[@hostname[:port]])");
	}

	llast = lhead = NULL;

	query[0] = "RW";
	query[1] = upsname;
	numq = 2;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {

		/* old upsd --> fall back on old LISTRW technique */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			fatalx(EXIT_FAILURE, "Error: upsd is too old to support this query");
		}

		fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
	}

	ret = upscli_list_next(ups, numq, query, &numa, &answer);

	while (ret == 1) {

		/* RW <upsname> <varname> <value> */
		if (numa < 4) {
			fatalx(EXIT_FAILURE, "Error: insufficient data (got %d args, need at least 4)", numa);
		}

		/* sock this entry away for later */

		ltmp = xmalloc(sizeof(struct list_t));
		ltmp->name = xstrdup(answer[2]);
		ltmp->next = NULL;

		if (llast) {
			llast->next = ltmp;
		} else {
			lhead = ltmp;
		}

		llast = ltmp;

		ret = upscli_list_next(ups, numq, query, &numa, &answer);
	}

	/* use the list to get descriptions and types */

	ltmp = lhead;

	while (ltmp) {
		lnext = ltmp->next;

		print_rw(ltmp->name);

		free(ltmp->name);
		free(ltmp);
		ltmp = lnext;
	}
}

int main(int argc, char **argv)
{
	int	i, port;
	const char	*prog = xbasename(argv[0]);
	char	*password = NULL, *username = NULL, *setvar = NULL;

	while ((i = getopt(argc, argv, "+hs:p:t:u:wV")) != -1) {
		switch (i)
		{
		case 's':
			setvar = optarg;
			break;
		case 'p':
			password = optarg;
			break;
		case 't':
			if (!str_to_uint(optarg, &timeout, 10))
				fatal_with_errno(EXIT_FAILURE, "Could not convert the provided value for timeout ('-t' option) to unsigned int");
			break;
		case 'u':
			username = optarg;
			break;
		case 'w':
			tracking_enabled = 1;
			break;
		case 'V':
			printf("Network UPS Tools %s %s\n", prog, UPS_VERSION);
			exit(EXIT_SUCCESS);
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

	if (setvar) {
		/* setting a variable */
		do_setvar(setvar, username, password);
	} else {
		/* if not, get the list of supported read/write variables */
		print_rwlist();
	}

	exit(EXIT_SUCCESS);
}


/* Formal do_upsconf_args implementation to satisfy linker on AIX */
#if (defined NUT_PLATFORM_AIX)
void do_upsconf_args(char *upsname, char *var, char *val) {
        fatalx(EXIT_FAILURE, "INTERNAL ERROR: formal do_upsconf_args called");
}
#endif  /* end of #if (defined NUT_PLATFORM_AIX) */
