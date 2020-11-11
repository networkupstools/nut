/* upsstats - cgi program to generate the main ups info page

   Copyright (C) 1998  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2005  Arnaud Quette <http://arnaud.quette.free.fr/contact.html>

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
#include "upsclient.h"
#include "status.h"
#include "cgilib.h"
#include "parseconf.h"
#include "timehead.h"
#include "upsstats.h"
#include "upsimagearg.h"

#define MAX_CGI_STRLEN 128
#define MAX_PARSE_ARGS 16

static char	*monhost = NULL;
static int	use_celsius = 1, refreshdelay = -1, treemode = 0;

	/* from cgilib's checkhost() */
static char	*monhostdesc = NULL;

static int	port;
static char	*upsname, *hostname;
static char	*upsimgpath="upsimage.cgi", *upsstatpath="upsstats.cgi";
static UPSCONN_t	ups;

static FILE	*tf;
static long	forofs = 0;

static ulist_t	*ulhead = NULL, *currups = NULL;

static int	skip_clause = 0, skip_block = 0;

void parsearg(char *var, char *value)
{
	/* avoid bogus junk from evil people */
	if ((strlen(var) > MAX_CGI_STRLEN) || (strlen(value) > MAX_CGI_STRLEN))
		return;

	if (!strcmp(var, "host")) {
		free(monhost);
		monhost = xstrdup(value);
		return;
	}

	if (!strcmp(var, "refresh"))
		refreshdelay = (int) strtol(value, (char **) NULL, 10);

	if (!strcmp(var, "treemode")) {
		/* FIXME: Validate that treemode is allowed */
		treemode = 1;
	}
}

static void report_error(void)
{
	if (upscli_upserror(&ups) == UPSCLI_ERR_VARNOTSUPP)
		printf("Not supported\n");
	else
		printf("[error: %s]\n", upscli_strerror(&ups));
}

/* make sure we're actually connected to upsd */
static int check_ups_fd(int do_report)
{
	if (upscli_fd(&ups) == -1) {
		if (do_report)
			report_error();

		return 0;
	}

	/* also check for insanity in currups */

	if (!currups) {
		if (do_report)
			printf("No UPS specified for monitoring\n");

		return 0;
	}

	/* must be OK */
	return 1;
}

static int get_var(const char *var, char *buf, size_t buflen, int verbose)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[4];
	char	**answer;

	if (!check_ups_fd(1))
		return 0;

	if (!upsname) {
		if (verbose)
			printf("[No UPS name specified]\n");

		return 0;
	}

	query[0] = "VAR";
	query[1] = upsname;
	query[2] = var;

	numq = 3;

	ret = upscli_get(&ups, numq, query, &numa, &answer);

	if (ret < 0) {
		if (verbose)
			report_error();
		return 0;
	}

	if (numa < numq) {
		if (verbose)
			printf("[Invalid response]\n");

		return 0;
	}

	snprintf(buf, buflen, "%s", answer[3]);
	return 1;
}

static void parse_var(const char *var)
{
	char	answer[SMALLBUF];

	if (!get_var(var, answer, sizeof(answer), 1))
		return;

	printf("%s", answer);
}

static void do_status(void)
{
	int	i;
	char	status[SMALLBUF], *ptr, *last = NULL;

	if (!get_var("ups.status", status, sizeof(status), 1)) {
		return;
	}

	for (ptr = strtok_r(status, " \n", &last); ptr != NULL; ptr = strtok_r(NULL, " \n", &last)) {

		/* expand from table in status.h */
		for (i = 0; stattab[i].name != NULL; i++) {

			if (!strcasecmp(ptr, stattab[i].name)) {
				printf("%s<br>", stattab[i].desc);
			}
		}
	}
}

static void do_runtime(void)
{
	int 	total, hours, minutes, seconds;
	char	runtime[SMALLBUF];

	if (!get_var("battery.runtime", runtime, sizeof(runtime), 1))
		return;

	total = (int) strtol(runtime, (char **) NULL, 10);

	hours = total / 3600;
	minutes = (total - (hours * 3600)) / 60;
	seconds = total % 60;

	printf("%02d:%02d:%02d", hours, minutes, seconds);

}

static int do_date(const char *buf)
{
	char	datebuf[SMALLBUF];
	time_t	tod;

	time(&tod);
	if (strftime(datebuf, sizeof(datebuf), buf, localtime(&tod))) {
		printf("%s", datebuf);
		return 1;
	}

	return 0;
}

static int get_img_val(const char *var, const char *desc, const char *imgargs)
{
	char	answer[SMALLBUF];

	if (!get_var(var, answer, sizeof(answer), 1))
		return 1;

	printf("<IMG SRC=\"%s?host=%s&amp;display=%s",
		upsimgpath, currups->sys, var);

	if ((imgargs) && (strlen(imgargs) > 0))
		printf("&amp;%s", imgargs);

	printf("\" ALT=\"%s: %s\">", desc, answer);

	return 1;
}

/* see if <arg> is valid - table from upsimagearg.h */
static void check_imgarg(char *arg, char *out, size_t outlen)
{
	int	i;
	char	*ep;

	ep = strchr(arg, '=');

	if (!ep)
		return;

	*ep++= '\0';

	/* if it's allowed, append it so it can become part of the URL */
	for (i = 0; imgarg[i].name != NULL; i++) {
		if (!strcmp(imgarg[i].name, arg)) {

			if (strlen(out) == 0)
				snprintf(out, outlen, "%s=%s", arg, ep);
			else
				snprintfcat(out, outlen, "&amp;%s=%s", arg, ep);
			return;
		}
	}
}

/* split out the var=val commands from the IMG line */
static void split_imgarg(char *in, char *out, size_t outlen)
{
	char	*ptr, *sp;

	if (strlen(in) < 3)
		return;

	ptr = in;

	sp = strchr(ptr, ' ');

	/* split by spaces, then check each one (can't use parseconf...) */
	while (sp) {
		*sp++ = '\0';
		check_imgarg(ptr, out, outlen);

		ptr = sp;
		sp = strchr(ptr, ' ');
	}

	check_imgarg(ptr, out, outlen);
}

/* IMG <type> [<var>=<val] [<var>=<val>] ... */
static int do_img(char *buf)
{
	char	*type, *ptr, imgargs[SMALLBUF];

	memset(imgargs, '\0', sizeof(imgargs));

	type = buf;

	ptr = strchr(buf, ' ');

	if (ptr) {
		*ptr++ = '\0';
		split_imgarg(ptr, imgargs, sizeof(imgargs));
	}

	/* only allow known types through */

	if (!strcmp(type, "input.voltage")
			|| !strcmp(type, "input.L1-N.voltage")
			|| !strcmp(type, "input.L2-N.voltage")
			|| !strcmp(type, "input.L3-N.voltage")
			|| !strcmp(type, "input.L1-L2.voltage")
			|| !strcmp(type, "input.L2-L3.voltage")
			|| !strcmp(type, "input.L3-L1.voltage")) {
		return get_img_val(type, "Input voltage", imgargs);
	}

	if (!strcmp(type, "battery.voltage"))
		return get_img_val(type, "Battery voltage", imgargs);

	if (!strcmp(type, "battery.charge"))
		return get_img_val(type, "Battery charge", imgargs);

	if (!strcmp(type, "output.voltage")
			|| !strcmp(type, "output.L1-N.voltage")
			|| !strcmp(type, "output.L2-N.voltage")
			|| !strcmp(type, "output.L3-N.voltage")
			|| !strcmp(type, "output.L1-L2.voltage")
			|| !strcmp(type, "output.L2-L3.voltage")
			|| !strcmp(type, "output.L3-L1.voltage")) {
		return get_img_val(type, "Output voltage", imgargs);
	}

	if (!strcmp(type, "ups.load")
			|| !strcmp(type, "output.L1.power.percent")
			|| !strcmp(type, "output.L2.power.percent")
			|| !strcmp(type, "output.L3.power.percent")
			|| !strcmp(type, "output.L1.realpower.percent")
			|| !strcmp(type, "output.L2.realpower.percent")
			|| !strcmp(type, "output.L3.realpower.percent")) {
		return get_img_val(type, "UPS load", imgargs);
	}

	if (!strcmp(type, "input.frequency"))
		return get_img_val(type, "Input frequency", imgargs);

	if (!strcmp(type, "output.frequency"))
		return get_img_val(type, "Output frequency", imgargs);

	if (!strcmp(type, "ups.temperature"))
		return get_img_val(type, "UPS temperature", imgargs);

	if (!strcmp(type, "ambient.temperature"))
		return get_img_val(type, "Ambient temperature", imgargs);

	if (!strcmp(type, "ambient.humidity"))
		return get_img_val(type, "Ambient humidity", imgargs);

	return 0;
}

static void ups_connect(void)
{
	static ulist_t	*lastups = NULL;
	char	*newups, *newhost;
	int	newport;

	/* try to minimize reconnects */
	if (lastups) {

		/* don't reconnect if these are both the same UPS */
		if (!strcmp(lastups->sys, currups->sys)) {
			lastups = currups;
			return;
		}

		/* see if it's just on the same host */
		newups = newhost = NULL;

		if (upscli_splitname(currups->sys, &newups, &newhost,
			&newport) != 0) {
			printf("Unusable UPS definition [%s]\n", currups->sys);
			fprintf(stderr, "Unusable UPS definition [%s]\n",
				currups->sys);
			exit(EXIT_FAILURE);
		}

		if ((!strcmp(newhost, hostname)) && (port == newport)) {
			free(upsname);
			upsname = newups;

			free(newhost);
			lastups = currups;
			return;
		}

		/* not the same upsd, so disconnect */
		free(newups);
		free(newhost);
	}

	upscli_disconnect(&ups);

	free(upsname);
	free(hostname);

	if (upscli_splitname(currups->sys, &upsname, &hostname, &port) != 0) {
		printf("Unusable UPS definition [%s]\n", currups->sys);
		fprintf(stderr, "Unusable UPS definition [%s]\n", currups->sys);
		exit(EXIT_FAILURE);
	}

	if (upscli_connect(&ups, hostname, port, 0) < 0)
		fprintf(stderr, "UPS [%s]: can't connect to server: %s\n", currups->sys, upscli_strerror(&ups));

	lastups = currups;
}

static void do_hostlink(void)
{
	if (!currups) {
		return;
	}

	printf("<a href=\"%s?host=%s", upsstatpath, currups->sys);

	if (refreshdelay > 0) {
		printf("&amp;refresh=%d", refreshdelay);
	}

	printf("\">%s</a>", currups->desc);
}

static void do_treelink(void)
{
	if (!currups) {
		return;
	}

	printf("<a href=\"%s?host=%s&amp;treemode\">All data</a>", upsstatpath, currups->sys);
}

/* see if the UPS supports this variable - skip to the next ENDIF if not */
/* if val is not null, value returned by var must be equal to val to match */
static void do_ifsupp(const char *var, const char *val)
{
	char	dummy[SMALLBUF];

	/* if not connected, act like it's not supported and skip the rest */
	if (!check_ups_fd(0)) {
		skip_clause = 1;
		return;
	}

	if (!get_var(var, dummy, sizeof(dummy), 0)) {
		skip_clause = 1;
		return;
	}

	if(!val) {
		return;
	}

	if(strcmp(dummy, val)) {
		skip_clause = 1;
		return;
	}
}

static int breakargs(char *s, char **aargs)
{
	char	*p;
	int	i=0;

	aargs[i]=NULL;

	for(p=s; *p && i<(MAX_PARSE_ARGS-1); p++) {
		if(aargs[i] == NULL) {
			aargs[i] = p;
			aargs[i+1] = NULL;
		}
		if(*p==' ') {
			*p='\0';
			i++;
		}
	}

	/* Check how many valid args we got */
	for(i=0; aargs[i]; i++);

	return i;
}

static void do_ifeq(const char *s)
{
	char	var[SMALLBUF];
	char	*aa[MAX_PARSE_ARGS];
	int	nargs;

	strcpy(var, s);

	nargs = breakargs(var, aa);
	if(nargs != 2) {
		printf("upsstats: IFEQ: Argument error!\n");
		return;
	}

	do_ifsupp(aa[0], aa[1]);
}

/* IFBETWEEN var1 var2 var3. Skip if var3 not between var1
 * and var2 */
static void do_ifbetween(const char *s)
{
	char	var[SMALLBUF];
	char	*aa[MAX_PARSE_ARGS];
	char	tmp[SMALLBUF];
	int	nargs;
	long	v1, v2, v3;
	char	*isvalid=NULL;

	strcpy(var, s);

	nargs = breakargs(var, aa);
	if(nargs != 3) {
		printf("upsstats: IFBETWEEN: Argument error!\n");
		return;
	}

	if (!check_ups_fd(0)) {
		return;
	}

	if (!get_var(aa[0], tmp, sizeof(tmp), 0)) {
		return;
	}
	v1 = strtol(tmp, &isvalid, 10);
	if(tmp == isvalid) {
		return;
	}

	if (!get_var(aa[1], tmp, sizeof(tmp), 0)) {
		return;
	}
	v2 = strtol(tmp, &isvalid, 10);
	if(tmp == isvalid) {
		return;
	}

	if (!get_var(aa[2], tmp, sizeof(tmp), 0)) {
		return;
	}
	v3 = strtol(tmp, &isvalid, 10);
	if(tmp == isvalid) {
		return;
	}

	if(v1 > v3 || v2 < v3) {
		skip_clause = 1;
		return;
	}
}

static void do_upsstatpath(const char *s) {

	if(strlen(s)) {
		upsstatpath = strdup(s);
	}
}

static void do_upsimgpath(const char *s) {

	if(strlen(s)) {
		upsimgpath = strdup(s);
	}
}

static void do_temp(const char *var)
{
	char	tempc[SMALLBUF];
	float	tempf;

	if (!get_var(var, tempc, sizeof(tempc), 1))
		return;

	if (use_celsius) {
		printf("%s", tempc);
		return;
	}

	tempf = (strtod(tempc, (char **) NULL) * 1.8) + 32;
	printf("%.1f", tempf);
}

static void do_degrees(void)
{
	printf("&deg;");

	if (use_celsius)
		printf("C");
	else
		printf("F");
}

/* plug in the right color string (like #FF0000) for the UPS status */
static void do_statuscolor(void)
{
	int	severity, i;
	char	stat[SMALLBUF], *sp, *ptr;

	if (!check_ups_fd(0)) {

		/* can't print the warning here - give a red error condition */
		printf("#FF0000");
		return;
	}

	if (!get_var("ups.status", stat, sizeof(stat), 0)) {
		/* status not available - give yellow as a warning */
		printf("#FFFF00");
		return;
	}

	severity = 0;
	sp = stat;

	while (sp) {
		ptr = strchr(sp, ' ');
		if (ptr)
			*ptr++ = '\0';

		/* expand from table in status.h */
		for (i = 0; stattab[i].name != NULL; i++)
			if (!strcmp(stattab[i].name, sp))
				if (stattab[i].severity > severity)
					severity = stattab[i].severity;

		sp = ptr;
	}

	switch(severity) {
		case 0:	printf("#00FF00"); break;	/* green  : OK      */
		case 1:	printf("#FFFF00"); break;	/* yellow : warning */

		default: printf("#FF0000"); break;	/* red    : error   */
	}
}

static int do_command(char *cmd)
{
	/* ending an if block? */
	if (!strcmp(cmd, "ENDIF")) {
		skip_clause = 0;
		skip_block = 0;
		return 1;
	}

	/* Skipping a block means skip until ENDIF, so... */
	if (skip_block) {
		return 1;
	}

	/* Toggle state when we run across ELSE */
	if (!strcmp(cmd, "ELSE")) {
		if (skip_clause) {
			skip_clause = 0;
		} else {
			skip_block = 1;
		}
		return 1;
	}

	/* don't do any commands if skipping a section */
	if (skip_clause == 1) {
		return 1;
	}

	if (!strncmp(cmd, "VAR ", 4)) {
		parse_var(&cmd[4]);
		return 1;
	}

	if (!strcmp(cmd, "HOST")) {
		printf("%s", currups->sys);
		return 1;
	}

	if (!strcmp(cmd, "HOSTDESC")) {
		printf("%s", currups->desc);
		return 1;
	}

	if (!strcmp(cmd, "RUNTIME")) {
		do_runtime();
		return 1;
	}

	if (!strcmp(cmd, "STATUS")) {
		do_status();
		return 1;
	}

	if (!strcmp(cmd, "STATUSCOLOR")) {
		do_statuscolor();
		return 1;
	}

	if (!strcmp(cmd, "TEMPF")) {
		use_celsius = 0;
		return 1;
	}

	if (!strcmp(cmd, "TEMPC")) {
		use_celsius = 1;
		return 1;
	}

	if (!strncmp(cmd, "DATE ", 5)) {
		return do_date(&cmd[5]);
	}

	if (!strncmp(cmd, "IMG ", 4)) {
		return do_img(&cmd[4]);
	}

	if (!strcmp(cmd, "VERSION")) {
		printf("%s", UPS_VERSION);
		return 1;
	}

	if (!strcmp(cmd, "REFRESH")) {
		if (refreshdelay > 0) {
			printf("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"%d\">", refreshdelay);
		}
		return 1;
	}

	if (!strcmp(cmd, "FOREACHUPS")) {
		forofs = ftell(tf);

		currups = ulhead;
		ups_connect();
		return 1;
	}

	if (!strcmp(cmd, "ENDFOR")) {

		/* if not in a for, ignore this */
		if (forofs == 0) {
			return 1;
		}

		currups = currups->next;

		if (currups) {
			fseek(tf, forofs, SEEK_SET);
			ups_connect();
		}

		return 1;
	}

	if (!strcmp(cmd, "HOSTLINK")) {
		do_hostlink();
		return 1;
	}

	if (!strcmp(cmd, "TREELINK")) {
		do_treelink();
		return 1;
	}

	if (!strncmp(cmd, "IFSUPP ", 7)) {
		do_ifsupp(&cmd[7], NULL);
		return 1;
	}

	if (!strcmp(cmd, "UPSTEMP")) {
		do_temp("ups.temperature");
		return 1;
	}

	if (!strcmp(cmd, "BATTTEMP")) {
		do_temp("battery.temperature");
		return 1;
	}

	if (!strcmp(cmd, "AMBTEMP")) {
		do_temp("ambient.temperature");
		return 1;
	}

	if (!strcmp(cmd, "DEGREES")) {
		do_degrees();
		return 1;
	}

	if (!strncmp(cmd, "IFEQ ", 5)) {
		do_ifeq(&cmd[5]);
		return 1;
	}

	if (!strncmp(cmd, "IFBETWEEN ", 10)) {
		do_ifbetween(&cmd[10]);
		return 1;
	}

	if (!strncmp(cmd, "UPSSTATSPATH ", 13)) {
		do_upsstatpath(&cmd[13]);
		return 1;
	}

	if (!strncmp(cmd, "UPSIMAGEPATH ", 13)) {
		do_upsimgpath(&cmd[13]);
		return 1;
	}

	return 0;
}

static void parse_line(const char *buf)
{
	char	cmd[SMALLBUF];
	int	i, len, do_cmd = 0;

	for (i = 0; buf[i]; i += len) {

		len = strcspn(&buf[i], "@");

		if (len == 0) {
			if (do_cmd) {
				do_command(cmd);
				do_cmd = 0;
			} else {
				cmd[0] = '\0';
				do_cmd = 1;
			}
			i++;	/* skip over the '@' character */
			continue;
		}

		if (do_cmd) {
			snprintf(cmd, sizeof(cmd), "%.*s", len, &buf[i]);
			continue;
		}

		if (skip_clause || skip_block) {
			/* ignore this */
			continue;
		}

		/* pass it trough */
		printf("%.*s", len, &buf[i]);
	}
}

static void display_template(const char *tfn)
{
	char	fn[SMALLBUF], buf[LARGEBUF];

	snprintf(fn, sizeof(fn), "%s/%s", confpath(), tfn);

	tf = fopen(fn, "r");

	if (!tf) {
		fprintf(stderr, "upsstats: Can't open %s: %s\n", fn, strerror(errno));

		printf("Error: can't open template file (%s)\n", tfn);

		exit(EXIT_FAILURE);
	}

	while (fgets(buf, sizeof(buf), tf)) {
		parse_line(buf);
	}

	fclose(tf);
}

static void display_tree(int verbose)
{
	unsigned int	numq, numa;
	const	char	*query[4];
	char	**answer;

	if (!upsname) {
		if (verbose)
			printf("[No UPS name specified]\n");
		return;
	}

	query[0] = "VAR";
	query[1] = upsname;
	numq = 2;

	if (upscli_list_start(&ups, numq, query) < 0) {
		if (verbose)
			report_error();
		return;
	}

	printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n");
	printf("	\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n");
	printf("<HTML>\n");
	printf("<HEAD><TITLE>upsstat: data tree of %s</TITLE></HEAD>\n", currups->desc);

	printf("<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#0000EE\" VLINK=\"#551A8B\">\n");

	printf("<TABLE BGCOLOR=\"#50A0A0\" ALIGN=\"CENTER\">\n");
	printf("<TR><TD>\n");

	printf("<TABLE CELLPADDING=\"5\" CELLSPACING=\"0\" ALIGN=\"CENTER\" WIDTH=\"100%%\">\n");

	/* include the description from checkhost() if present */
	printf("<TR><TH COLSPAN=3 BGCOLOR=\"#50A0A0\">\n");
	printf("<FONT SIZE=\"+2\">%s</FONT>\n", currups->desc);
	printf("</TH></TR>\n");

	printf("<TR><TH COLSPAN=3 BGCOLOR=\"#60B0B0\"></TH></TR>\n");

	while (upscli_list_next(&ups, numq, query, &numa, &answer) == 1) {

		/* VAR <upsname> <varname> <val> */
		if (numa < 4) {
			if (verbose)
				printf("[Invalid response]\n");

			return;
		}

		printf("<TR BGCOLOR=\"#60B0B0\" ALIGN=\"LEFT\">\n");

		printf("<TD>%s</TD>\n", answer[2]);
		printf("<TD>:</TD>\n");
		printf("<TD>%s<br></TD>\n", answer[3]);

		printf("</TR>\n");
	}

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");

	/* FIXME (AQ): add a save button (?), and a checkbt for showing var.desc */
	printf("</BODY></HTML>\n");
}

static void add_ups(char *sys, char *desc)
{
	ulist_t	*tmp, *last;

	tmp = last = ulhead;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ulist_t));

	tmp->sys = xstrdup(sys);
	tmp->desc = xstrdup(desc);
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		ulhead = tmp;
}

/* called for fatal errors in parseconf like malloc failures */
static void upsstats_hosts_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(hosts.conf): %s", errmsg);
}

static void load_hosts_conf(void)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	snprintf(fn, sizeof(fn), "%s/hosts.conf", CONFPATH);

	pconf_init(&ctx, upsstats_hosts_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n");
		printf("	\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n");
		printf("<HTML><HEAD>\n");
		printf("<TITLE>Error: can't open hosts.conf</TITLE>\n");
		printf("</HEAD><BODY>\n");
		printf("Error: can't open hosts.conf\n");
		printf("</BODY></HTML>\n");

		/* leave something for the admin */
		fprintf(stderr, "upsstats: %s\n", ctx.errmsg);
		exit(EXIT_FAILURE);
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 3)
			continue;

		/* MONITOR <host> <desc> */
		if (!strcmp(ctx.arglist[0], "MONITOR"))
			add_ups(ctx.arglist[1], ctx.arglist[2]);

	}

	pconf_finish(&ctx);

	if (!ulhead) {
		printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n");
		printf("	\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n");
		printf("<HTML><HEAD>\n");
		printf("<TITLE>Error: no hosts to monitor</TITLE>\n");
		printf("</HEAD><BODY>\n");
		printf("Error: no hosts to monitor (check <CODE>hosts.conf</CODE>)\n");
		printf("</BODY></HTML>\n");

		/* leave something for the admin */
		fprintf(stderr, "upsstats: no hosts to monitor\n");
		exit(EXIT_FAILURE);
	}
}

static void display_single(void)
{
	if (!checkhost(monhost, &monhostdesc)) {
		printf("Access to that host [%s] is not authorized.\n",
			monhost);
		exit(EXIT_FAILURE);
	}

	add_ups(monhost, monhostdesc);

	currups = ulhead;
	ups_connect();

	/* switch between data tree view and standard single view */
	if (treemode)
		display_tree(1);
	else
		display_template("upsstats-single.html");

	upscli_disconnect(&ups);
}

int main(int argc, char **argv)
{
	NUT_UNUSED_VARIABLE(argc);
	NUT_UNUSED_VARIABLE(argv);

	extractcgiargs();

	printf("Content-type: text/html\n");
	printf("Pragma: no-cache\n");
	printf("\n");

	/* if a host is specified, use upsstats-single.html instead */
	if (monhost) {
		display_single();
		exit(EXIT_SUCCESS);
	}

	/* default: multimon replacement mode */

	load_hosts_conf();

	currups = ulhead;

	display_template("upsstats.html");

	upscli_disconnect(&ups);

	return 0;
}
