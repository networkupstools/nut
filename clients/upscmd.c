/* upscmd - simple "client" to test instant commands via upsd

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>

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
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "upsclient.h"

struct list_t {
	char	*name;
	struct	list_t	*next;
};

static void usage(char *prog)
{
	printf("Network UPS Tools upscmd %s\n\n", UPS_VERSION);
	printf("usage: %s [-h]\n", prog);
	printf("       %s [-l <ups>]\n", prog);
	printf("       %s [-u <username>] [-p <password>] <ups> <command>\n\n", prog);
	printf("Administration program to initiate instant commands on UPS hardware.\n");
	printf("\n");
	printf("  -h		display this help text\n");
	printf("  -l <ups>	show available commands on UPS <ups>\n");
	printf("  -u <username>	set username for command authentication\n");
	printf("  -p <password>	set password for command authentication\n");
	printf("\n");
	printf("  <ups>		UPS identifier - <upsname>[@<hostname>[:<port>]]\n");
	printf("  <command>	Valid instant command - test.panel.start, etc.\n");

	exit(EXIT_SUCCESS);
}

static void clean_exit(UPSCONN_t *ups, char *upsname, char *hostname, int code)
{
	free(upsname);
	free(hostname);

	upscli_sendline(ups, "LOGOUT\n", 7);
	upscli_disconnect(ups);

	exit(code);
}

static void print_cmd(UPSCONN_t *ups, const char *upsname, char *cmdname)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer;
	const	char	*query[4];

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

static void listcmds(char *rawname)
{
	int	ret, port;
	unsigned int	numq, numa;
	char	*upsname, *hostname, **answer;
	const	char	*query[4];
	UPSCONN_t	ups;
	struct	list_t	*lhead, *llast, *ltmp, *lnext;

	upsname = hostname = NULL;

	if (upscli_splitname(rawname, &upsname, &hostname, &port) != 0) {
		fprintf(stderr, "Error: invalid UPS definition.  Required format: upsname[@hostname[:port]]\n");
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	if (upscli_connect(&ups, hostname, port, 0) < 0) {
		fprintf(stderr, "Error: %s\n", upscli_strerror(&ups));
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	if (!upsname) {
		fprintf(stderr, "Error: a UPS name must be specified (upsname[@hostname[:port]])\n");
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	llast = lhead = NULL;

	query[0] = "CMD";
	query[1] = upsname;
	numq = 2;

	ret = upscli_list_start(&ups, numq, query);

	if (ret < 0) {

		/* old upsd = no way to continue */
		if (upscli_upserror(&ups) == UPSCLI_ERR_UNKCOMMAND) {
			fprintf(stderr, "Error: upsd is too old to support this query\n");
			clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
		}

		fprintf(stderr, "Error: %s\n", upscli_strerror(&ups));
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	printf("Instant commands supported on UPS [%s]:\n\n", rawname);

	ret = upscli_list_next(&ups, numq, query, &numa, &answer);

	while (ret == 1) {

		/* CMD <upsname> <cmdname> */

		if (numa < 3) {
			fprintf(stderr, "Error: insufficient data "
				"(got %d args, need at least 3)\n", numa);
			clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
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

		ret = upscli_list_next(&ups, numq, query, &numa, &answer);
	}

	/* walk the list and try to get descriptions, freeing as we go */

	ltmp = lhead;

	while (ltmp) {
		lnext = ltmp->next;

		print_cmd(&ups, upsname, ltmp->name);

		free(ltmp->name);
		free(ltmp);
		ltmp = lnext;
	}

	clean_exit(&ups, upsname, hostname, EXIT_SUCCESS);
}

static int do_cmd(UPSCONN_t *ups, const char *upsname, const char *cmd)
{
	char 	buf[SMALLBUF];

	snprintf(buf, sizeof(buf), "INSTCMD %s %s\n", upsname, cmd);

	if (upscli_sendline(ups, buf, strlen(buf)) < 0) {
		fprintf(stderr, "Can't send instant command: %s\n",
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	if (upscli_readline(ups, buf, sizeof(buf)) < 0) {
		fprintf(stderr, "Instant command failed: %s\n", 
			upscli_strerror(ups));

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int	i, ret, have_un, have_pw;
	char 	*prog, buf[SMALLBUF], username[SMALLBUF], password[SMALLBUF],
		*pwtmp;

	int	port;
	char	*upsname, *hostname;
	UPSCONN_t	ups;

	prog = argv[0];

	have_un = have_pw = 0;

	while ((i = getopt(argc, argv, "+l:hu:p:V")) != -1) {
		switch (i) {
			case 'l':
				listcmds(optarg);
				break;

			case 'u':
				snprintf(username, sizeof(username), "%s", 
					optarg);
				have_un = 1;
				break;

			case 'p':
				snprintf(password, sizeof(password), "%s", 
					optarg);
				have_pw = 1;
				break;

			case 'V':
				printf("Network UPS Tools upscmd %s\n",
					UPS_VERSION);
				exit(EXIT_SUCCESS);

			case 'h':
			default:
				usage(prog);
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage(prog);

	upsname = hostname = NULL;

	if (upscli_splitname(argv[0], &upsname, &hostname, &port) != 0) {
		fprintf(stderr, "Error: invalid UPS definition.  Required format: upsname[@hostname[:port]]\n");
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	if (upscli_connect(&ups, hostname, port, 0) < 0) {
		fprintf(stderr, "Error: %s\n", upscli_strerror(&ups));
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	if (!have_un) {
		struct	passwd	*pw;

		memset(username, '\0', sizeof(username));
		pw = getpwuid(getuid());

		if (pw)
			printf("Username (%s): ", pw->pw_name);
		else
			printf("Username: ");

		fgets(username, SMALLBUF, stdin);

		/* deal with that pesky newline */
		if (strlen(username) > 1)
			username[strlen(username) - 1] = '\0';
		else {
			if (!pw)
				fatalx(EXIT_FAILURE, "No username available - even tried getpwuid");

			snprintf(username, sizeof(username), "%s", pw->pw_name);
		}
	}

	/* getpass leaks slightly - use -p when testing in valgrind */
	if (!have_pw) {
		pwtmp = GETPASS("Password: ");

		if (!pwtmp) {
			fprintf(stderr, "getpass failed: %s\n", strerror(errno));
			clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
		}
			
		snprintf(password, sizeof(password), "%s", pwtmp);
	}

	snprintf(buf, sizeof(buf), "USERNAME %s\n", username);

	if (upscli_sendline(&ups, buf, strlen(buf)) < 0) {
		fprintf(stderr, "Can't set username: %s\n", 
			upscli_strerror(&ups));

		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	ret = upscli_readline(&ups, buf, sizeof(buf));

	if (ret < 0) {
		if (upscli_upserror(&ups) == UPSCLI_ERR_UNKCOMMAND) {
			fprintf(stderr, "Set username failed due to an "
				"unknown command.\n");

			fprintf(stderr, "You probably need to upgrade upsd.\n");
			clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
		}

		fprintf(stderr, "Set username failed: %s\n", 
			upscli_strerror(&ups));

		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	snprintf(buf, sizeof(buf), "PASSWORD %s\n", password);

	if (upscli_sendline(&ups, buf, strlen(buf)) < 0) {
		fprintf(stderr, "Can't set password: %s\n", 
			upscli_strerror(&ups));
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	if (upscli_readline(&ups, buf, sizeof(buf)) < 0) {
		fprintf(stderr, "Set password failed: %s\n", 
			upscli_strerror(&ups));

		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	/* no upsname means die here */
	if (!upsname) {
		fprintf(stderr, "Error: a UPS name must be specified (upsname[@hostname[:port]])\n");
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	/* also fallback for old command names */
	if (!strchr(argv[1], '.')) {
		fprintf(stderr, "Error: old command names are not supported\n");
		clean_exit(&ups, upsname, hostname, EXIT_FAILURE);
	}

	ret = do_cmd(&ups, upsname, argv[1]);
	clean_exit(&ups, upsname, hostname, ret);

	/* NOTREACHED */
	exit(EXIT_FAILURE);
}
