/* dummycons.c - a simple testing driver for the console

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>

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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include "config.h"
#include "proto.h"
#include "version.h"
#include "common.h"
#include "parseconf.h"
#include "dstate.h"
#include "extstate.h"
#include "state.h"

#define DC_PROMPT "\nCommand (? for help): "

	/* signal handling */
	static	int	exit_flag = 0;
	static	sigset_t		nut_dummycons_sigmask;
	static	struct	sigaction	dc_sa;

	static	PCONF_CTX	ctx;

static void set_exit_flag(int sig)
{
	exit_flag = sig;
}

static void setup_signals(void)
{
	sigemptyset(&nut_dummycons_sigmask);
	dc_sa.sa_mask = nut_dummycons_sigmask;
	dc_sa.sa_flags = 0;

	dc_sa.sa_handler = set_exit_flag;
	sigaction(SIGTERM, &dc_sa, NULL);
	sigaction(SIGINT, &dc_sa, NULL);
	sigaction(SIGQUIT, &dc_sa, NULL);

	dc_sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &dc_sa, NULL);
	sigaction(SIGPIPE, &dc_sa, NULL);
}

static void initinfo(void)
{
	dstate_setinfo("driver.name", "dummycons");
	dstate_setinfo("driver.version", "%s", UPS_VERSION);
	dstate_setinfo("ups.mfr", "Console testing");
	dstate_setinfo("ups.model", "Dummy UPS");
	dstate_setinfo("ups.status", "OL");
	dstate_dataok();
}

static void do_help(void)
{
	printf("\nNetwork UPS Tools dummycons %s help\n\n", UPS_VERSION);
	printf("? / h / help	- show this help\n");
	printf("ac CMD		- add command name CMD\n");
	printf("ae VAR ENUM	- add enum value ENUM to VAR\n");
	printf("alc             - call dstate alarm_commit()\n");
	printf("ali             - call dstate alarm_init()\n");
	printf("als ALARM       - pass ALARM to dstate alarm_set()\n");
	printf("d VAR		- delete variable VAR\n");
	printf("dc CMD		- delete command name CMD\n");
	printf("de VAR ENUM	- delete enum value ENUM from VAR\n");
	printf("l / list	- show current variables in use\n");
	printf("lc / listcmd	- show current commands\n");
	printf("s / set VAR VAL - set variable VAR to VAL\n");
	printf("                  example: s ups.status OL\n");
	printf("sa VAR NUM	- set auxdata value NUM on variable VAR\n");
	printf("sf VAR FLAG	- set flag FLAG on variable VAR\n");
	printf("stale / dataok	- control staleness flag\n");
	printf("q / quit	- quit\n");
	printf("\n");
	printf("Values can use \"quotes\" to store values with embedded spaces.\n");
	printf("Example: s ups.status \"OL LB\"\n");
}

static void do_set(const char *var, const char *val)
{
	if ((!var) || (!val)) {
		printf("Error: need variable and desired values as arguments.\n");
		return;
	}

	dstate_setinfo(var, "%s", val);
}

static void do_listcmd(void)
{
	const	struct	cmdlist_t	*cmd;

	cmd = dstate_getcmdlist();

	if (!cmd)
		return;

	printf("\nCommands:\n\n");

	while (cmd) {
		printf("%s\n", cmd->name);

		cmd = cmd->next;
	}
}

static void tree_dump(const struct st_tree_t *node)
{
	struct	enum_t	*etmp;

	if (!node)
		return;

	if (node->left)
		tree_dump(node->left);

	printf("%-50s = %s\n", node->var, node->val);

	if (node->aux != 0)
		printf(" Aux data: %d\n", node->aux);

	if (node->flags != 0) {
		printf(" Flags:");

		if (node->flags & ST_FLAG_RW)
			printf(" RW");
		if (node->flags & ST_FLAG_STRING)
			printf(" STRING");

		printf("\n");
	}

	etmp = node->enum_list;

	if (etmp) {
		printf(" ENUM values:");

		while (etmp) {
			printf(" [%s]", etmp->val);
			etmp = etmp->next;
		}

		printf("\n");
	}

	if (node->right)
		tree_dump(node->right);
}

static void do_list(void)
{
	printf("\n");

	tree_dump(dstate_getroot());
}	

/* for future cleanup activities */
static void do_quit(void)
{
	exit(EXIT_SUCCESS);
}

static void fake_shutdown(void)
{
	printf("dummycons: not a driver, nothing to shutdown...\n");
	exit(EXIT_SUCCESS);
}

static void help(const char *progname)
{
	printf("Dummy UPS driver - for testing and development\n\n");

	printf("usage: %s OPTIONS [<portname>]\n\n", progname);

	printf("  -h		- display this help\n");
	printf("  -k		- fake a shutdown\n");
	printf("  -u USER	- switch to USER if started as root\n");
	printf("  <portname>	- fake port name (for socket))\n");

	exit(EXIT_SUCCESS);
}

static int parse_args(int numargs, char **arg)
{
	if (numargs < 1)
		return 0;

	if ((!strcmp(arg[0], "q")) || (!strcmp(arg[0], "quit"))) {
		do_quit();
		/* NOTREACHED */
	}

	if ((!strcmp(arg[0], "l")) || (!strcmp(arg[0], "list"))) {
		do_list();
		return 1;
	}

	if ((!strcmp(arg[0], "lc")) || (!strcmp(arg[0], "listcmd"))) {
		do_listcmd();
		return 1;
	}

	if ((!strcmp(arg[0], "h")) || (!strcmp(arg[0], "help")) ||
		(!strcmp(arg[0], "?"))) {
		do_help();
		return 1;
	}

	if (!strcmp(arg[0], "stale")) {
		printf("Setting stale flag\n");
		dstate_datastale();
		return 1;
	}

	if (!strcmp(arg[0], "dataok")) {
		printf("Clearing stale flag\n");
		dstate_dataok();
		return 1;
	}

	if (!strcmp(arg[0], "ali")) {
		printf("alarm_init()\n");
		alarm_init();
		return 1;
	}

	if (!strcmp(arg[0], "alc")) {
		printf("alarm_commit()\n");
		alarm_commit();
		return 1;
	}

	if (numargs < 2)
		return 0;

	if (!strcmp(arg[0], "als")) {
		printf("alarm_set(%s)\n", arg[1]);
		alarm_set(arg[1]);
		return 1;
	}

	if ((!strcmp(arg[0], "d")) || (!strcmp(arg[0], "delete"))) {
		if (!dstate_delinfo(arg[1]))
			printf("Can't delete (%s) - not found\n", arg[1]);
		else
			printf("Deleted %s\n", arg[1]);

		return 1;
	}

	if ((!strcmp(arg[0], "ac")) || (!strcmp(arg[0], "addcmd"))) {
		dstate_addcmd(arg[1]);
		printf("Added cmd %s\n", arg[1]);
		return 1;
	}

	if ((!strcmp(arg[0], "dc")) || (!strcmp(arg[0], "delcmd"))) {
		if (!dstate_delcmd(arg[1]))
			printf("Can't delete cmd (%s) - not found\n", arg[1]);
		else
			printf("Deleted cmd %s\n", arg[1]);

		return 1;
	}

	if (numargs < 3)
		return 0;

	if ((!strcmp(arg[0], "s")) || (!strcmp(arg[0], "set")) ||
		(!strcmp(arg[0], "a")) || (!strcmp(arg[0], "add"))) {
		do_set(arg[1], arg[2]);
		return 1;
	}

	if ((!strcmp(arg[0], "de")) || (!strcmp(arg[0], "delenum"))) {
		if (!dstate_delenum(arg[1], arg[2]))
			printf("Can't delete enum (%s,%s) - not found\n", 
				arg[1], arg[2]);
		else
			printf("Deleted enum %s,%s\n", 
				arg[1], arg[2]);

		return 1;
	}

	if ((!strcmp(arg[0], "ae")) || (!strcmp(arg[0], "addenum"))) {
		if (!dstate_addenum(arg[1], "%s", arg[2]))
			printf("Can't add enum (%s,%s) - not found\n", 
				arg[1], arg[2]);
		else
			printf("Added enum %s,%s\n", 
				arg[1], arg[2]);

		return 1;
	}

	if ((!strcmp(arg[0], "sa")) || (!strcmp(arg[0], "setaux"))) {
		int	aux;

		aux = strtol(arg[2], (char **) NULL, 10);

		dstate_setaux(arg[1], aux);
		return 1;
	}

	if ((!strcmp(arg[0], "sf")) || (!strcmp(arg[0], "setflags"))) {
		int	i, stflags;

		stflags = 0;

		for (i = 2; i < numargs; i++) {

			if (!strcasecmp(arg[i], "RW"))
				stflags |= ST_FLAG_RW;

			if (!strcasecmp(arg[i], "STRING"))
				stflags |= ST_FLAG_STRING;
		}

		dstate_setflags(arg[1], stflags);

		printf("Flags (%s) = %04x\n", arg[1], stflags);

		return 1;
	}

	return 0;
}

static int dc_setvar(const char *varname, const char *val)
{
	printf("SETVAR from socket: [%s] [%s]\n", varname, val);
	return STAT_SET_UNKNOWN;
}

static int dc_instcmd(const char *cmdname, const char *extra)
{
	printf("INSTCMD from socket: [%s]\n", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static void exit_cleanup(void)
{
	pconf_finish(&ctx);
	dstate_free();
}

int main(int argc, char **argv)
{
	int	ret, i;
	char	buf[128], *prog;
	const	char	*portname, *user;
	struct	passwd	*new_uid = NULL;

	printf("Network UPS Tools - Dummy console UPS driver %s\n", UPS_VERSION);

	user = RUN_AS_USER;
	prog = argv[0];

	while ((i = getopt(argc, argv, "+hku:V")) != EOF) {
		switch(i) {
			case 'k':
				fake_shutdown();
				break;

			case 'h':
				help(prog);
				break;

			case 'u':
				user = optarg;
				break;

			case 'V':
				exit(EXIT_SUCCESS);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		portname = "null";
	else
		portname = argv[0];

	printf("Socket: %s/dummycons-%s\n", STATEPATH, portname);

	if ((new_uid = get_user_pwent(user)) == NULL)
		fatal("getpwnam(%s)", user);

	become_user(new_uid);

	initinfo();

	setup_signals();
	atexit(exit_cleanup);

	/* clear out callback handler data */
	memset(&upsh, '\0', sizeof(upsh));

	/* install dummy handlers for SET and INSTCMD */
	upsh.setvar = dc_setvar;
	upsh.instcmd = dc_instcmd;

	dstate_init("dummycons", portname);

	printf(DC_PROMPT);
	fflush(stdout);

	pconf_init(&ctx, NULL);

	while (exit_flag == 0) {
		ret = dstate_poll_fds(30, fileno(stdin));

		/* idle */
		if (ret == 0)
			continue;

		fgets(buf, sizeof(buf), stdin);

		/* split into usable chunks */
		if (pconf_line(&ctx, buf)) {
			if (pconf_parse_error(&ctx))
				printf("Parse error: %s\n", ctx.errmsg);
			else {
				
				if (!parse_args(ctx.numargs, ctx.arglist))
					printf("Unrecognized command\n");
			}
		}

		printf(DC_PROMPT);
		fflush(stdout);

	}	/* while (exit_flag == 0) */

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
	exit(EXIT_SUCCESS);
}
