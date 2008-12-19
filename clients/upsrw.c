/* upsrw - simple client for read/write variable access (formerly upsct2)

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

#include <pwd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "upsclient.h"

struct list_t {
	char	*name;
	struct	list_t	*next;
};

static void usage(const char *prog)
{
	printf("Network UPS Tools upsrw %s\n\n", UPS_VERSION);
	printf("usage: %s [-h]\n", prog);
	printf("       %s [-s <variable>] [-u <username>] [-p <password>] <ups>\n\n", prog);
	printf("Demo program to set variables within UPS hardware.\n");
	printf("\n");
	printf("  -h            display this help text\n");
	printf("  -s <variable>	specify variable to be changed\n");
	printf("		use -s VAR=VALUE to avoid prompting for value\n");
	printf("  -u <username> set username for command authentication\n");
	printf("  -p <password> set password for command authentication\n");
	printf("\n");
	printf("  <ups>         UPS identifier - <upsname>[@<hostname>[:<port>]]\n");
	printf("\n");
	printf("Call without -s to show all possible read/write variables.\n");

	exit(EXIT_SUCCESS);
}

static void clean_exit(UPSCONN_t *ups, char *upsname, char *hostname, int code)
{
	free(upsname);
	free(hostname);

	upscli_disconnect(ups);

	exit(code);
}

static int do_set(UPSCONN_t *ups, const char *upsname, const char *varname, 
	const char *newval)
{
	char	buf[SMALLBUF], enc[SMALLBUF];

	snprintf(buf, sizeof(buf), "SET VAR %s %s \"%s\"\n",
		upsname, varname, pconf_encode(newval, enc, sizeof(enc)));

	if (upscli_sendline(ups, buf, strlen(buf)) < 0) {
		fprintf(stderr, "Can't set variable: %s\n", 
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	if (upscli_readline(ups, buf, sizeof(buf)) < 0) {
		fprintf(stderr, "Set variable failed: %s\n", 
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	/* FUTURE: status cookies will tie in here */
	if (strncmp(buf, "OK", 2) != 0) {
		printf("Unexpected response from upsd: %s\n", buf);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int do_setvar(UPSCONN_t *ups, const char *varname, char *uin,
		const char *pass, char *upsname, char *hostname)
{
	char	newval[SMALLBUF], temp[SMALLBUF], user[SMALLBUF], *ptr;
	struct	passwd	*pw;

	if (uin) {
		snprintf(user, sizeof(user), "%s", uin);
	} else {
		memset(user, '\0', sizeof(user));

		pw = getpwuid(getuid());

		if (pw)
			printf("Username (%s): ", pw->pw_name);
		else
			printf("Username: ");

		if (fgets(user, sizeof(user), stdin) == NULL) {
			upsdebug_with_errno(LOG_INFO, "%s", __func__);
		}

		/* deal with that pesky newline */
		if (strlen(user) > 1)
			user[strlen(user) - 1] = '\0';
		else {
			if (!pw)
				fatalx(EXIT_FAILURE, "No username available - even tried getpwuid");
	
			snprintf(user, sizeof(user), "%s", pw->pw_name);
		}
	}

	/* leaks - use -p when running in valgrind */
	if (!pass) {
		pass = GETPASS("Password: " );

		if (!pass) {
			fprintf(stderr, "getpass failed: %s\n", 
				strerror(errno));

			return EXIT_FAILURE;
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
		fprintf(stderr, "Can't set username: %s\n", 
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	if (upscli_readline(ups, temp, sizeof(temp)) < 0) {

		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			fprintf(stderr, "Set username failed due to an "
				"unknown command.\n");

			fprintf(stderr, "You probably need to upgrade upsd.\n");

			clean_exit(ups, upsname, hostname, EXIT_FAILURE);
		}

		fprintf(stderr, "Set username failed: %s\n",
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	snprintf(temp, sizeof(temp), "PASSWORD %s\n", pass);

	if (upscli_sendline(ups, temp, strlen(temp)) < 0) {
		fprintf(stderr, "Can't set password: %s\n", 
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	if (upscli_readline(ups, temp, sizeof(temp)) < 0) {
		fprintf(stderr, "Set password failed: %s\n", 
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	/* no upsname means die */
	if (!upsname) {
		fprintf(stderr, "Error: a UPS name must be specified (upsname[@hostname[:port]])\n");
		return EXIT_FAILURE;
	}

	/* old variable names are no longer supported */
	if (!strchr(varname, '.')) {
		fprintf(stderr, "Error: old variable names are not supported\n");
		return EXIT_FAILURE;
	}

	return do_set(ups, upsname, varname, newval);
}	

static const char *get_data(const char *type, UPSCONN_t *ups, 
	const char *upsname, const char *varname)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer;
	const	char	*query[4];

	query[0] = type;
	query[1] = upsname;
	query[2] = varname;
	numq = 3;

	ret = upscli_get(ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq))
		return NULL;

	/* <type> <upsname> <varname> <desc> */
	return answer[3];
}

static void do_string(UPSCONN_t *ups, const char *upsname, const char *varname)
{
	const	char	*val;

	val = get_data("VAR", ups, upsname, varname);

	if (!val) {
		fprintf(stderr, "do_string: can't get current value of %s\n",
			varname);
		return;
	}

	printf("Type: STRING\n");
	printf("Value: %s\n", val);
}

static void do_enum(UPSCONN_t *ups, const char *upsname, const char *varname)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer, *val;
	const	char	*query[4], *tmp;

	/* get current value */
	tmp = get_data("VAR", ups, upsname, varname);

	if (!tmp) {
		fprintf(stderr, "do_enum: can't get current value of %s\n",
			varname);
		return;
	}

	/* tmp is a pointer into answer - have to save it somewhere else */
	val = xstrdup(tmp);

	query[0] = "ENUM";
	query[1] = upsname;
	query[2] = varname;
	numq = 3;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {
		fprintf(stderr, "Error: %s\n", upscli_strerror(ups));
		return;
	}

	ret = upscli_list_next(ups, numq, query, &numa, &answer);

	printf("Type: ENUM\n");

	while (ret == 1) {

		/* ENUM <upsname> <varname> <value> */

		if (numa < 4) {
			fprintf(stderr, "Error: insufficient data "
				"(got %d args, need at least 4)\n", numa);

			free(val);
			return;
		}

		printf("Option: \"%s\"", answer[3]);

		if (!strcmp(answer[3], val))
			printf(" SELECTED");

		printf("\n");

		ret = upscli_list_next(ups, numq, query, &numa, &answer);
	}

	free(val);
}

static void do_type(UPSCONN_t *ups, const char *upsname, const char *varname)
{
	int	ret;
	unsigned int	i, numq, numa;
	char	**answer;
	const	char	*query[4];

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

		if (!strcasecmp(answer[i], "ENUM")) {
			do_enum(ups, upsname, varname);
			return;
		}

		if (!strncasecmp(answer[i], "STRING:", 7)) {
			do_string(ups, upsname, varname);
			return;
		}

		/* ignore this one */
		if (!strcasecmp(answer[i], "RW"))
			continue;

		printf("Type: %s (unrecognized)\n", answer[i]);
	}

}

static void print_rw(UPSCONN_t *ups, const char *upsname, const char *varname)
{
	const	char	*tmp;

	printf("[%s]\n", varname);

	tmp = get_data("DESC", ups, upsname, varname);

	if (tmp)
		printf("%s\n", tmp);
	else
		printf("Description unavailable\n");

	do_type(ups, upsname, varname);

	printf("\n");
}	

static int print_rwlist(UPSCONN_t *ups, const char *upsname)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[2];
	char	**answer;
	struct	list_t	*lhead, *llast, *ltmp, *lnext;

	/* the upsname is now required */
	if (!upsname) {
		fprintf(stderr, "Error: a UPS name must be specified (upsname[@hostname[:port]])\n");
		return EXIT_FAILURE;
	}

	llast = lhead = NULL;

	query[0] = "RW";
	query[1] = upsname;
	numq = 2;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {

		/* old upsd --> fall back on old LISTRW technique */
		if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
			fprintf(stderr, "Error: upsd is too old to support this query\n");
			return EXIT_FAILURE;
		}

		fprintf(stderr, "Error: %s\n", upscli_strerror(ups));
		return EXIT_FAILURE;
	}

	ret = upscli_list_next(ups, numq, query, &numa, &answer);

	while (ret == 1) {

		/* RW <upsname> <varname> <value> */
		if (numa < 4) {
			fprintf(stderr, "Error: insufficient data "
				"(got %d args, need at least 4)\n", numa);
			return EXIT_FAILURE;
		}

		/* sock this entry away for later */

		ltmp = xmalloc(sizeof(struct list_t));
		ltmp->name = xstrdup(answer[2]);
		ltmp->next = NULL;

		if (llast)
			llast->next = ltmp;
		else
			lhead = ltmp;

		llast = ltmp;

		ret = upscli_list_next(ups, numq, query, &numa, &answer);
	}

	/* use the list to get descriptions and types */

	ltmp = lhead;

	while (ltmp) {
		lnext = ltmp->next;

		print_rw(ups, upsname, ltmp->name);

		free(ltmp->name);
		free(ltmp);
		ltmp = lnext;
	}

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int	i, port, ret;
	char	*upsname, *hostname, *setvar, *prog;
	char	*password = NULL, *username = NULL;
	UPSCONN_t	ups;

	setvar = username = NULL;
	prog = argv[0];

	while ((i = getopt(argc, argv, "+s:p:u:V")) != -1) {
		switch (i) {
		case 's':
			setvar = optarg;
			break;
		case 'p':
			password = optarg;
			break;
		case 'u':
			username = optarg;
			break;
		case 'V':
			printf("Network UPS Tools upsrw %s\n", UPS_VERSION);
			exit(EXIT_SUCCESS);
		default:
			usage(prog);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage(prog);

	upsname = hostname = NULL;

	if (upscli_splitname(argv[0], &upsname, &hostname, &port) != 0) {
		fprintf(stderr, "Error: invalid UPS definition.  Required format: upsname[@hostname[:port]]\n");
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	if (upscli_connect(&ups, hostname, port, 0) < 0) {
		fprintf(stderr, "Can't connect: %s\n", upscli_strerror(&ups));
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	/* setting a variable? */
	if (setvar) {
		ret = do_setvar(&ups, setvar, username, password, upsname,
			hostname);

		clean_exit(&ups, upsname, hostname, ret);
	}

	/* if not, get the list of supported read/write variables */
	ret = print_rwlist(&ups, upsname);

	clean_exit(&ups, upsname, hostname, ret);

	/* NOTREACHED */
	exit(EXIT_FAILURE);
}
