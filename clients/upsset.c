/* upsset - CGI program to manage read/write variables

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
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "upsclient.h"
#include "cgilib.h"
#include "parseconf.h"

struct list_t {
	char	*name;
	struct	list_t	*next;
};

/* see the stock upsset.conf for the whole rant on what this is */
#define MAGIC_ENABLE_STRING "I_HAVE_SECURED_MY_CGI_DIRECTORY"

#define HARD_UPSVAR_LIMIT_NUM	64
#define HARD_UPSVAR_LIMIT_LEN	256

	char	*monups, *username, *password, *function, *upscommand;

	/* set once the MAGIC_ENABLE_STRING is found in the upsset.conf */
	int	magic_string_set = 0;

static	int	port;
static	char	*upsname, *hostname;
static	UPSCONN_t	ups;

typedef struct {
	char	*var;
	char	*value;
	void	*next;
}	uvtype_t;

	uvtype_t	*firstuv = NULL;

void parsearg(char *var, char *value)
{
	char	*ptr;
	uvtype_t	*last, *tmp = NULL;
	static	int upsvc = 0;

	/* store variables from a SET command for the later commit */
	if (!strncmp(var, "UPSVAR_", 7)) {

		/* if someone bombs us with variables, stop at some point */
		if (upsvc > HARD_UPSVAR_LIMIT_NUM)
			return;

		/* same idea: throw out anything that's much too long */
		if (strlen(value) > HARD_UPSVAR_LIMIT_LEN)
			return;

		ptr = strchr(var, '_');

		if (!ptr)		/* sanity check */
			return;

		ptr++;

		tmp = last = firstuv;
		while (tmp) {
			last = tmp;
			tmp = tmp->next;
		}

		tmp = xmalloc(sizeof(uvtype_t));
		tmp->var = xstrdup(ptr);
		tmp->value = xstrdup(value);
		tmp->next = NULL;

		if (last)
			last->next = tmp;
		else
			firstuv = tmp;

		upsvc++;

		return;
	}

	if (!strcmp(var, "username")) {
		free(username);
		username = xstrdup(value);
	}

	if (!strcmp(var, "password")) {
		free(password);
		password = xstrdup(value);
	}

	if (!strcmp(var, "function")) {
		free(function);
		function = xstrdup(value);
	}

	if (!strcmp(var, "monups")) {
		free(monups);
		monups = xstrdup(value);
	}

	if (!strcmp(var, "upscommand")) {
		free(upscommand);
		upscommand = xstrdup(value);
	}
}

static void do_header(const char *title)
{
	printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n");
	printf("	\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n");
	printf("<HTML>\n");
	printf("<HEAD><TITLE>upsset: %s</TITLE></HEAD>\n", title);

	printf("<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#0000EE\" VLINK=\"#551A8B\">\n");

	printf("<TABLE BGCOLOR=\"#50A0A0\" ALIGN=\"CENTER\">\n");
	printf("<TR><TD>\n");
}

static void start_table(void)
{
	printf("<TABLE CELLPADDING=\"5\" CELLSPACING=\"0\" ALIGN=\"CENTER\" WIDTH=\"100%%\">\n");
	printf("<TR><TH COLSPAN=2 BGCOLOR=\"#60B0B0\">\n");
	printf("<FONT SIZE=\"+2\">Network UPS Tools upsset %s</FONT>\n",
		UPS_VERSION);
	printf("</TH></TR>\n");
}

/* propagate login details across pages - no cookies here! */
static void do_hidden(const char *next)
{
	printf("<INPUT TYPE=\"HIDDEN\" NAME=\"username\" VALUE=\"%s\">\n",
		username);
	printf("<INPUT TYPE=\"HIDDEN\" NAME=\"password\" VALUE=\"%s\">\n",
		password);

	if (next)
		printf("<INPUT TYPE=\"HIDDEN\" NAME=\"function\" VALUE=\"%s\">\n",
			next);
}

/* generate SELECT chooser from hosts.conf entries */
static void upslist_arg(int numargs, char **arg)
{
	if (numargs < 3)
		return;

	/* MONITOR <ups> <description> */
	if (!strcmp(arg[0], "MONITOR")) {
		printf("<OPTION VALUE=\"%s\"", arg[1]);

		if (monups)
			if (!strcmp(monups, arg[1]))
				printf("SELECTED");

		printf(">%s</OPTION>\n", arg[2]);
	}
}

/* called for fatal errors in parseconf like malloc failures */
static void upsset_hosts_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(hosts.conf): %s", errmsg);
}

/* this defaults to wherever we are now, ups and function-wise */
static void do_pickups(const char *currfunc)
{
	char	hostfn[SMALLBUF];
	PCONF_CTX_t	ctx;

	snprintf(hostfn, sizeof(hostfn), "%s/hosts.conf", confpath());

	printf("<FORM METHOD=\"POST\" ACTION=\"upsset.cgi\">\n");

	printf("Select UPS and function:\n<BR>\n");

	pconf_init(&ctx, upsset_hosts_err);

	if (!pconf_file_begin(&ctx, hostfn)) {
		pconf_finish(&ctx);

		printf("Error: hosts.conf unavailable\n");
		printf("</FORM>\n");

		/* stderr is for the admin - should wind up in error.log */
		fprintf(stderr, "upsset: %s\n", ctx.errmsg);

		return;
	}

	printf("<SELECT NAME=\"monups\">\n");

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				hostfn, ctx.linenum, ctx.errmsg);

			continue;
		}

		upslist_arg(ctx.numargs, ctx.arglist);
	}

	pconf_finish(&ctx);

	printf("</SELECT>\n");

	printf("<SELECT NAME=\"function\">\n");

	/* FUTURE */
	/*	printf("<OPTION VALUE=\"showstatus\">Status</OPTION>\n");  */

	/* TODO: clean this up */

	if (!strcmp(currfunc, "showsettings"))
		printf("<OPTION VALUE=\"showsettings\" SELECTED>Settings</OPTION>\n");
	else
		printf("<OPTION VALUE=\"showsettings\">Settings</OPTION>\n");

	if (!strcmp(currfunc, "showcmds"))
		printf("<OPTION VALUE=\"showcmds\" SELECTED>Commands</OPTION>\n");
	else
		printf("<OPTION VALUE=\"showcmds\">Commands</OPTION>\n");

	printf("</SELECT>\n");
	do_hidden(NULL);

	printf("<INPUT TYPE=\"SUBMIT\" VALUE=\"View\">\n");
	printf("</FORM>\n");
}

static void error_page(const char *next, const char *title,
	const char *fmt, ...)
{
	char	msg[SMALLBUF];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	do_header(title);

	start_table();
	printf("<TR><TH COLSPAN=2 BGCOLOR=\"#60B0B0\">\n");
	printf("Error: %s\n", msg);
	printf("</TH></TR>\n");

	printf("<TR><TD ALIGN=\"CENTER\" COLSPAN=2>\n");
	do_pickups(next);
	printf("</TD></TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");
	printf("</BODY></HTML>\n");

	upscli_disconnect(&ups);
	exit(EXIT_SUCCESS);
}

static void loginscreen(void)
{
	do_header("Login");
	printf("<FORM METHOD=\"POST\" ACTION=\"upsset.cgi\">\n");
	start_table();

	printf("<TR BGCOLOR=\"#60B0B0\">\n");
	printf("<TH>Username</TH>\n");
	printf("<TD><INPUT TYPE=\"TEXT\" NAME=\"username\" VALUE=\"\"></TD>\n");
	printf("</TR>\n");

	printf("<TR BGCOLOR=\"#60B0B0\">\n");
	printf("<TH>Password</TH>\n");
	printf("<TD><INPUT TYPE=\"PASSWORD\" NAME=\"password\" VALUE=\"\"></TD>\n");
	printf("</TR>\n");

	printf("<TR><TD COLSPAN=2 ALIGN=\"CENTER\">\n");
	printf("<INPUT TYPE=\"HIDDEN\" NAME=\"function\" VALUE=\"pickups\">\n");
	printf("<INPUT TYPE=\"SUBMIT\" VALUE=\"Login\">\n");
	printf("<INPUT TYPE=\"RESET\" VALUE=\"Reset fields\">\n");
	printf("</TD></TR></TABLE>\n");
	printf("</FORM>\n");
	printf("</TD></TR></TABLE>\n");
	printf("</BODY></HTML>\n");

	upscli_disconnect(&ups);
	exit(EXIT_SUCCESS);
}

/* try to connect to upsd - generate an error page if it fails */
static void upsd_connect(void)
{
	if (upscli_splitname(monups, &upsname, &hostname, &port) != 0) {
		error_page("showsettings", "UPS name is unusable",
			"Unable to split UPS name [%s]", monups);
		/* NOTREACHED */
	}

	if (upscli_connect(&ups, hostname, port, 0) < 0) {
		error_page("showsettings", "Connect failure",
			"Unable to connect to %s: %s",
			monups, upscli_strerror(&ups));
		/* NOTREACHED */
	}
}

static void print_cmd(const char *cmd)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer;
	const	char	*query[4];

	query[0] = "CMDDESC";
	query[1] = upsname;
	query[2] = cmd;
	numq = 3;

	ret = upscli_get(&ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq))
		return;

	/* CMDDESC <upsname> <cmdname> <desc> */

	printf("<OPTION VALUE=\"%s\">%s</OPTION>\n", cmd, answer[3]);
}

/* generate a list of instant commands */
static void showcmds(void)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[2];
	char	**answer;
	struct	list_t	*lhead, *llast, *ltmp, *lnext;
	char	*desc;

	if (!checkhost(monups, &desc))
		error_page("showsettings", "Access denied",
			"Access to that host is not authorized");

	upsd_connect();

	llast = lhead = NULL;

	query[0] = "CMD";
	query[1] = upsname;
	numq = 2;

	ret = upscli_list_start(&ups, numq, query);

	if (ret < 0) {
		fprintf(stderr, "LIST CMD %s failed: %s\n",
			upsname, upscli_strerror(&ups));

		error_page("showcmds", "Server protocol error",
			"LIST CMD command failed");

		/* NOTREACHED */
	}

	ret = upscli_list_next(&ups, numq, query, &numa, &answer);

	while (ret == 1) {

		/* CMD upsname cmdname */
		if (numa < 3) {
			fprintf(stderr, "Error: insufficient data "
				"(got %u args, need at least 3)\n", numa);

			return;
		}

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

	if (!lhead)
		error_page("showcmds", "No instant commands supported",
			"This UPS doesn't support any instant commands.");

	do_header("Instant commands");
	printf("<FORM ACTION=\"upsset.cgi\" METHOD=\"POST\">\n");
	start_table();

	/* include the description from checkhost() if present */
	if (desc)
		printf("<TR><TH BGCOLOR=\"#60B0B0\"COLSPAN=2>%s</TH></TR>\n",
			desc);

	printf("<TR BGCOLOR=\"#60B0B0\" ALIGN=\"CENTER\">\n");
	printf("<TD>Instant commands</TD>\n");

	printf("<TD>\n");
	printf("<SELECT NAME=\"upscommand\">\n");

	/* provide a dummy do-nothing default choice */
	printf("<OPTION VALUE=\"\" SELECTED></OPTION>\n");

	ltmp = lhead;

	while (ltmp) {
		lnext = ltmp->next;

		print_cmd(ltmp->name);

		free(ltmp->name);
		free(ltmp);
		ltmp = lnext;
	}

	printf("</SELECT>\n");
	printf("</TD></TR>\n");

	printf("<TR BGCOLOR=\"#60B0B0\">\n");
	printf("<TD COLSPAN=\"2\" ALIGN=\"CENTER\">\n");
	do_hidden("docmd");
	printf("<INPUT TYPE=\"HIDDEN\" NAME=\"monups\" VALUE=\"%s\">\n", monups);
	printf("<INPUT TYPE=\"SUBMIT\" VALUE=\"Issue command\">\n");
	printf("<INPUT TYPE=\"RESET\" VALUE=\"Reset\">\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");
	printf("</FORM>\n");

	printf("<TR><TD ALIGN=\"CENTER\">\n");
	do_pickups("showcmds");
	printf("</TD></TR>\n");

	printf("</TABLE>\n");
	printf("</BODY></HTML>\n");

	upscli_disconnect(&ups);
	exit(EXIT_SUCCESS);
}

/* handle setting authentication data in the server */
static void send_auth(const char *next)
{
	char	buf[SMALLBUF];

	snprintf(buf, sizeof(buf), "USERNAME %s\n", username);

	if (upscli_sendline(&ups, buf, strlen(buf)) < 0) {
		fprintf(stderr, "Can't set username: %s\n",
			upscli_strerror(&ups));

		error_page(next, "Can't set username",
			"Set username failed: %s", upscli_strerror(&ups));
	}

	if (upscli_readline(&ups, buf, sizeof(buf)) < 0) {

		/* test for old upsd that doesn't do USERNAME */
		if (upscli_upserror(&ups) == UPSCLI_ERR_UNKCOMMAND) {
			error_page(next, "Protocol mismatch",
				"upsd version too old - USERNAME not supported");
		}

		error_page(next, "Can't set user name",
			"Set user name failed: %s", upscli_strerror(&ups));
	}

	snprintf(buf, sizeof(buf), "PASSWORD %s\n", password);

	if (upscli_sendline(&ups, buf, strlen(buf)) < 0)
		error_page(next, "Can't set password",
			"Password set failed: %s", upscli_strerror(&ups));

	if (upscli_readline(&ups, buf, sizeof(buf)) < 0)
		error_page(next, "Can't set password",
			"Password set failed: %s", upscli_strerror(&ups));
}

static void docmd(void)
{
	char	buf[SMALLBUF], *desc;

	if (!checkhost(monups, &desc))
		error_page("showsettings", "Access denied",
			"Access to that host is not authorized");

	/* the user is messing with us */
	if (!upscommand)
		error_page("showcmds", "Form error",
			"No instant command selected");

	/* (l)user took the default blank option */
	if (strlen(upscommand) == 0)
		error_page("showcmds", "Form error",
			"No instant command selected");

	upsd_connect();

	send_auth("showcmds");

	snprintf(buf, sizeof(buf), "INSTCMD %s %s\n", upsname, upscommand);

	if (upscli_sendline(&ups, buf, strlen(buf)) < 0) {
		do_header("Error while issuing command");

		start_table();

		printf("<TR><TD>Error sending command: %s\n</TD></TR>",
			upscli_strerror(&ups));

		printf("<TR><TD ALIGN=\"CENTER\" COLSPAN=2>\n");
		do_pickups("showcmds");
		printf("</TD></TR>\n");

		printf("</TABLE>\n");

		printf("</TD></TR></TABLE>\n");
		printf("</BODY></HTML>\n");

		upscli_disconnect(&ups);
		exit(EXIT_SUCCESS);
	}

	if (upscli_readline(&ups, buf, sizeof(buf)) < 0) {
		do_header("Error while reading command response");

		start_table();

		printf("<TR><TD>Error reading command response: %s\n</TD></TR>",
			upscli_strerror(&ups));

		printf("<TR><TD ALIGN=\"CENTER\" COLSPAN=2>\n");
		do_pickups("showcmds");
		printf("</TD></TR>\n");

		printf("</TABLE>\n");

		printf("</TD></TR></TABLE>\n");
		printf("</BODY></HTML>\n");

		upscli_disconnect(&ups);
		exit(EXIT_SUCCESS);
	}

	do_header("Issuing command");
	start_table();

	printf("<TR><TD><PRE>\n");
	printf("Sending command: %s\n", upscommand);
	printf("Response: %s\n", buf);
	printf("</PRE></TD></TR>\n");

	printf("<TR><TD ALIGN=\"CENTER\" COLSPAN=2>\n");
	do_pickups("showcmds");
	printf("</TD></TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");
	printf("</BODY></HTML>\n");

	upscli_disconnect(&ups);
	exit(EXIT_SUCCESS);
}

static const char *get_data(const char *type, const char *varname)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer;
	const	char	*query[4];

	query[0] = type;
	query[1] = upsname;
	query[2] = varname;
	numq = 3;

	ret = upscli_get(&ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq))
		return NULL;

	/* <type> <upsname> <varname> <desc> */
	return answer[3];
}

static void do_string(const char *varname, int maxlen)
{
	const	char	*val;

	val = get_data("VAR", varname);

	if (!val) {
		printf("Unavailable\n");
		fprintf(stderr, "do_string: can't get current value of %s\n",
			varname);
		return;
	}

	printf("<INPUT TYPE=\"TEXT\" NAME=\"UPSVAR_%s\" VALUE=\"%s\" "
		"SIZE=\"%d\">\n", varname, val, maxlen);
}

static void do_enum(const char *varname)
{
	int	ret;
	unsigned int	numq, numa;
	char	**answer, *val;
	const	char	*query[4], *tmp;

	/* get current value */
	tmp = get_data("VAR", varname);

	if (!tmp) {
		printf("Unavailable\n");
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

	ret = upscli_list_start(&ups, numq, query);

	if (ret < 0) {
		printf("Unavailable\n");
		fprintf(stderr, "Error doing ENUM %s %s: %s\n",
			upsname, varname, upscli_strerror(&ups));
		free(val);
		return;
	}

	ret = upscli_list_next(&ups, numq, query, &numa, &answer);

	printf("<SELECT NAME=\"UPSVAR_%s\">\n", varname);

	while (ret == 1) {

		/* ENUM <upsname> <varname> <value> */

		if (numa < 4) {
			fprintf(stderr, "Error: insufficient data "
				"(got %u args, need at least 4)\n", numa);

			free(val);
			return;
		}

		printf("<OPTION VALUE=\"%s\" ", answer[3]);

		if (!strcmp(answer[3], val))
			printf(" SELECTED");

		printf(">%s</OPTION>\n", answer[3]);

		ret = upscli_list_next(&ups, numq, query, &numa, &answer);
	}

	free(val);
	printf("</SELECT>\n");
}

static void do_type(const char *varname)
{
	int	ret;
	unsigned int	i, numq, numa;
	char	**answer;
	const	char	*query[4];

	query[0] = "TYPE";
	query[1] = upsname;
	query[2] = varname;
	numq = 3;

	ret = upscli_get(&ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq)) {
		printf("Unknown type\n");
		return;
	}

	/* TYPE <upsname> <varname> <type>... */
	for (i = 3; i < numa; i++) {

		if (!strcasecmp(answer[i], "ENUM")) {
			do_enum(varname);
			return;
		}

		if (!strncasecmp(answer[i], "STRING:", 7)) {
			char	*ptr, len;

			/* split out the :<len> data */
			ptr = strchr(answer[i], ':');
			*ptr++ = '\0';
			len = strtol(ptr, (char **) NULL, 10);

			do_string(varname, len);
			return;
		}

		/* ignore this one */
		if (!strcasecmp(answer[i], "RW"))
			continue;

		printf("Unrecognized\n");
	}
}

static void print_rw(const char *upsname, const char *varname)
{
	const	char	*tmp;

	printf("<!-- <TR><TD>Device</TD><TD>%s</TD></TR> -->\n", upsname);

	printf("<TR BGCOLOR=\"#60B0B0\" ALIGN=\"CENTER\">\n");

	printf("<TD>");

	tmp = get_data("DESC", varname);

	if ((tmp) && (strcmp(tmp, "Unavailable") != 0))
		printf("%s", tmp);
	else
		printf("%s", varname);

	printf("</TD>\n");

	printf("<TD>\n");
	do_type(varname);
	printf("</TD>\n");

	printf("</TR>\n");
}

static void showsettings(void)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[2];
	char	**answer, *desc = NULL;
	struct	list_t	*lhead, *llast, *ltmp, *lnext;

	if (!checkhost(monups, &desc))
		error_page("showsettings", "Access denied",
			"Access to that host is not authorized");

	upsd_connect();

	query[0] = "RW";
	query[1] = upsname;
	numq = 2;

	ret = upscli_list_start(&ups, numq, query);

	if (ret < 0) {
		fprintf(stderr, "LIST RW %s failed: %s\n",
			upsname, upscli_strerror(&ups));

		error_page("showsettings", "Server protocol error",
			"LIST RW command failed");

		/* NOTREACHED */
	}

	llast = lhead = NULL;

	ret = upscli_list_next(&ups, numq, query, &numa, &answer);

	while (ret == 1) {

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

	do_header("Current settings");
	printf("<FORM ACTION=\"upsset.cgi\" METHOD=\"POST\">\n");
	start_table();

	/* include the description from checkhost() if present */
	if (desc)
		printf("<TR><TH BGCOLOR=\"#60B0B0\"COLSPAN=2>%s</TH></TR>\n",
			desc);

	printf("<TR BGCOLOR=\"#60B0B0\">\n");
	printf("<TH>Setting</TH>\n");
	printf("<TH>Value</TH></TR>\n");

	/* use the list to get descriptions and types */

	ltmp = lhead;

	while (ltmp) {
		lnext = ltmp->next;

		print_rw(upsname, ltmp->name);

		free(ltmp->name);
		free(ltmp);
		ltmp = lnext;
	}

	printf("<TR BGCOLOR=\"#60B0B0\">\n");
	printf("<TD COLSPAN=\"2\" ALIGN=\"CENTER\">\n");
	do_hidden("savesettings");
	printf("<INPUT TYPE=\"HIDDEN\" NAME=\"monups\" VALUE=\"%s\">\n", monups);
	printf("<INPUT TYPE=\"SUBMIT\" VALUE=\"Save changes\">\n");
	printf("<INPUT TYPE=\"RESET\" VALUE=\"Reset\">\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");
	printf("</FORM>\n");

	printf("<TR><TD ALIGN=\"CENTER\">\n");
	do_pickups("showsettings");
	printf("</TD></TR>\n");

	printf("</TABLE>\n");
	printf("</BODY></HTML>\n");

	upscli_disconnect(&ups);
	exit(EXIT_SUCCESS);
}

static int setvar(const char *var, const char *val)
{
	char	buf[SMALLBUF], enc[SMALLBUF];
	const	char	*tmp;

	/* get old value */
	tmp = get_data("VAR", var);

	if (!tmp) {
		printf("Can't get old value for %s, aborting SET\n", var);
		return 0;
	}

	/* don't send a SET if it hasn't chnaged */
	if (!strcmp(tmp, val))
		return 0;

	printf("set %s to %s (was %s)\n", var, val, tmp);

	snprintf(buf, sizeof(buf), "SET VAR %s %s \"%s\"\n",
		upsname, var, pconf_encode(val, enc, sizeof(enc)));

	if (upscli_sendline(&ups, buf, strlen(buf)) < 0) {
		printf("Error: SET failed: %s\n", upscli_strerror(&ups));
		return 0;
	}

	if (upscli_readline(&ups, buf, sizeof(buf)) < 0) {
		printf("Error: SET failed: %s\n", upscli_strerror(&ups));
		return 0;
	}

	if (strncmp(buf, "OK", 2) != 0) {
		printf("Unexpected response: %s\n", buf);
		return 0;
	}

	printf("OK\n");
	return 1;
}

/* turn a form submission of settings into SET commands for upsd */
static void savesettings(void)
{
	int	changed = 0;
	char	*desc;
	uvtype_t	*upsvar;

	if (!checkhost(monups, &desc))
		error_page("showsettings", "Access denied",
			"Access to that host is not authorized");

	upsd_connect();

	upsvar = firstuv;

	send_auth("showsettings");

	do_header("Saving settings");
	start_table();

	printf("<TR><TD><PRE>\n");

	while (upsvar) {
		changed += setvar(upsvar->var, upsvar->value);
		upsvar = upsvar->next;
	}

	if (changed == 0)
		printf("No settings changed.\n");
	else
		printf("Updated %d setting%s.\n",
			changed, changed == 1 ? "" : "s");

	printf("</PRE></TD></TR>\n");

	printf("<TR><TD ALIGN=\"CENTER\" COLSPAN=\"2\">\n");
	do_pickups("showsettings");
	printf("</TD></TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");
	printf("</BODY></HTML>\n");

	upscli_disconnect(&ups);
	exit(EXIT_SUCCESS);
}

static void initial_pickups(void)
{
	do_header("Select a UPS");
	start_table();

	printf("<TR><TD ALIGN=\"CENTER\" COLSPAN=\"2\">\n");
	do_pickups("");
	printf("</TD></TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");
	printf("</BODY></HTML>\n");

	upscli_disconnect(&ups);
	exit(EXIT_SUCCESS);
}

static void upsset_conf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(upsset.conf): %s", errmsg);
}

/* see if the user has confirmed their cgi directory's secure state */
static void check_conf(void)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	snprintf(fn, sizeof(fn), "%s/upsset.conf", confpath());

	pconf_init(&ctx, upsset_conf_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		printf("<PRE>\n");
		printf("Error: Can't open upsset.conf to verify security settings.\n");
		printf("Refusing to start until this is fixed.\n");
		printf("</PRE>\n");

		/* leave something in the httpd log for the admin */
		fprintf(stderr, "upsset.conf does not exist to permit execution\n");
		exit(EXIT_FAILURE);
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!strcmp(ctx.arglist[0], MAGIC_ENABLE_STRING))
			magic_string_set = 1;
	}

	pconf_finish(&ctx);

	/* if we've been enabled, jump out of here and go to work */
	if (magic_string_set == 1)
		return;

	printf("<PRE>\n");
	printf("Error: Secure mode has not been enabled in upsset.conf.\n");
	printf("Refusing to start until this is fixed.\n");
	printf("</PRE>\n");

	/* leave something in the httpd log for the admin */
	fprintf(stderr, "upsset.conf does not permit execution\n");

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	username = password = function = monups = NULL;

	printf("Content-type: text/html\n\n");

	/* see if the magic string is present in the config file */
	check_conf();

	/* see if there's anything waiting .. the server my not close STDIN properly */
	if (1) {
	    fd_set fds;
	    struct timeval tv;

	    FD_ZERO(&fds);
	    FD_SET(STDIN_FILENO, &fds);
	    tv.tv_sec = 0;
	    tv.tv_usec = 250000; /* wait for up to 250ms  for a POST response */
	    if ((select(STDIN_FILENO+1, &fds, 0, 0, &tv)) > 0)
		extractpostargs();
	}
	if ((!username) || (!password) || (!function))
		loginscreen();

	if ((!strcmp(function, "pickups")) || (!monups))
		initial_pickups();

	if (!strcmp(function, "showsettings"))
		showsettings();

	if (!strcmp(function, "savesettings"))
		savesettings();

#if 0		/* FUTURE */
	if (!strcmp(function, "showstatus"))
		showstatus();
#endif

	if (!strcmp(function, "showcmds"))
		showcmds();

	if (!strcmp(function, "docmd"))
		docmd();

	printf("Error: Unhandled function name [%s]\n", function);

	return 0;
}
