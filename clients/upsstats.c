/* upsstats - cgi program to generate the main ups info page

   Copyright (C) 1998  Russell Kroll <rkroll@exploits.org>

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

#define MAX_CGI_STRLEN 64

static	char	*monhost = NULL;
static	int	use_celsius = 1, refreshdelay = -1;

	/* from cgilib's checkhost() */
static	char	*monhostdesc = NULL;

static	int	port;
static	char	*upsname, *hostname;
static	UPSCONN	ups;

static	FILE	*tf;
static	long	forofs = 0;

static	ulist_t	*ulhead = NULL, *currups = NULL;

static	int	skip_to_endif = 0;

void parsearg(char *var, char *value)
{
	/* avoid bogus junk from evil people */
	if ((strlen(var) > MAX_CGI_STRLEN) || (strlen(value) > MAX_CGI_STRLEN))
		return;

	if (!strcmp(var, "host")) {
		if (monhost)
			free(monhost);

		monhost = xstrdup(value);
		return;
	}

	if (!strcmp(var, "refresh"))
		refreshdelay = (int) strtol(value, (char **) NULL, 10);
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

	printf("%s\n", answer);
}

static void do_status(void)
{
	int	i;
	char	status[SMALLBUF], *sp, *ptr;

	if (!get_var("ups.status", status, sizeof(status), 1))
		return;

	sp = status;

	while (sp) {
		ptr = strchr(sp, ' ');
		if (ptr)
			*ptr++ = '\0';

		/* expand from table in status.h */
		for (i = 0; stattab[i].name != NULL; i++)
			if (!strcmp(stattab[i].name, sp))
				printf("%s<BR>", stattab[i].desc);

		sp = ptr;
	}
}

static int do_date(const char *buf)
{
	char	datebuf[SMALLBUF];
	time_t	tod;

	time(&tod);
	if (strftime(datebuf, sizeof(datebuf), buf, localtime(&tod))) {
		printf("%s\n", datebuf);
		return 1;
	}

	return 0;
}

static int get_img_val(const char *var, const char *desc, const char *imgargs)
{
	char	answer[SMALLBUF];

	if (!get_var(var, answer, sizeof(answer), 1))
		return 1;

	printf("<IMG SRC=\"upsimage.cgi?host=%s&amp;display=%s",
		currups->sys, var);

	if ((imgargs) && (strlen(imgargs) > 0))
		printf("&amp;%s", imgargs);

	printf("\" WIDTH=\"100\" HEIGHT=\"350\" ALT=\"%s: %s\">\n",
		desc, answer);

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

	if (!strcmp(type, "input.voltage"))
		return get_img_val("input.voltage", "Input voltage", imgargs);

	if (!strcmp(type, "battery.voltage"))
		return get_img_val("battery.voltage", "Battery voltage", imgargs);

	if (!strcmp(type, "battery.charge"))
		return get_img_val("battery.charge", "Battery charge", imgargs);

	if (!strcmp(type, "output.voltage"))
		return get_img_val("output.voltage", "Output voltage", imgargs);

	if (!strcmp(type, "ups.load"))
		return get_img_val("ups.load", "UPS load", imgargs);

	if (!strcmp(type, "input.frequency"))
		return get_img_val("input.frequency", "Input frequency", imgargs);

	if (!strcmp(type, "output.frequency"))
		return get_img_val("output.frequency", "Output frequency", imgargs);

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

	if (upsname)
		free(upsname);
	if (hostname)
		free(hostname);

	if (upscli_splitname(currups->sys, &upsname, &hostname, &port) != 0) {
		printf("Unusable UPS definition [%s]\n", currups->sys);
		fprintf(stderr, "Unusable UPS definition [%s]\n", 
			currups->sys);
		exit(EXIT_FAILURE);
	}

	if (upscli_connect(&ups, hostname, port, 0) < 0)
		fprintf(stderr, "UPS [%s]: can't connect to server: %s\n",
			currups->sys, upscli_strerror(&ups));

	lastups = currups;
}

static void do_hostlink(void)
{
	if (!currups)
		return;

	printf("<a href=\"upsstats.cgi?host=%s\">%s</a>\n",
		currups->sys, currups->desc);
}

/* see if the UPS supports this variable - skip to the next ENDIF if not */
static void do_ifsupp(const char *var)
{
	char	dummy[SMALLBUF];

	/* if not connected, act like it's not supported and skip the rest */
	if (!check_ups_fd(0)) {
		skip_to_endif = 1;
		return;
	}

	if (!get_var(var, dummy, sizeof(dummy), 0)) {
		skip_to_endif = 1;
		return;
	}
}

static void do_temp(const char *var)
{
	char	tempc[SMALLBUF];
	float	tempf;

	if (!get_var(var, tempc, sizeof(tempc), 1))
		return;

	if (use_celsius) {
		printf("%s\n", tempc);
		return;
	}

	tempf = (strtod(tempc, (char **) NULL) * 1.8) + 32;
	printf("%.1f\n", tempf);
}

static void do_degrees(void)
{
	printf("&deg;");

	if (use_celsius)
		printf("C\n");
	else
		printf("F\n");
}

/* plug in the right color string (like #FF0000) for the UPS status */
static void do_statuscolor(void)
{
	int	severity, i;
	char	stat[SMALLBUF], *sp, *ptr;

	if (!check_ups_fd(0)) {

		/* can't print the warning here - give a red error condition */
		printf("#FF0000\n");
		return;
	}

	if (!get_var("ups.status", stat, sizeof(stat), 0)) {
		/* status not available - give yellow as a warning */
		printf("#FFFF00\n");
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
		case 0:	printf("#00FF00\n"); break;	/* green  : OK      */
		case 1:	printf("#FFFF00\n"); break;	/* yellow : warning */

		default: printf("#FF0000\n"); break;	/* red    : error   */
	}
}

/* use red if utility is outside lowxfer+highxfer bounds, else green */
static void do_utilitycolor(void)
{
	char	tmp[SMALLBUF];
	int	lowxfer, highxfer, utility;

	if (!check_ups_fd(0)) {

		/* can't print the warning here - give a red error condition */
		printf("#FF0000\n");
		return;
	}

	if (!get_var("input.voltage", tmp, sizeof(tmp), 0)) {
		/* nothing available - default is green */
		printf("#00FF00\n");
		return;
	}

	utility = strtol(tmp, (char **) NULL, 10);

	if (!get_var("input.transfer.low", tmp, sizeof(tmp), 0)) {

		/* not available = default to green */
		printf("#00FF00\n");
		return;
	}

	lowxfer = strtol(tmp, (char **) NULL, 10);

	if (!get_var("input.transfer.high", tmp, sizeof(tmp), 0)) {

		/* same idea */
		printf("#00FF00\n");
		return;
	}

	highxfer = strtol(tmp, (char **) NULL, 10);

	if ((utility < lowxfer) || (utility > highxfer))
		printf("#FF0000\n");
	else
		printf("#00FF00\n");
}

/* look for lines starting and ending with @ containing valid commands */
static int parse_line(const char *buf)
{
	static	char	*cmd = NULL;

	/* deal with extremely short lines as a special case */
	if (strlen(buf) < 3) {

		if (skip_to_endif == 1)
			return 1;

		return 0;
	}

	if (buf[0] != '@') {

		/* if skipping a section, act like we parsed the line */
		if (skip_to_endif == 1)
			return 1;
		
		/* otherwise pass it through for normal printing */
		return 0;
	}

	if (buf[strlen(buf) - 1] != '@')
		return 0;

	if (cmd)
		free(cmd);

	cmd = xstrdup(&buf[1]);

	/* strip off final @ */
	cmd[strlen(cmd) - 1] = '\0';

	/* ending an if block? */
	if (!strcmp(cmd, "ENDIF")) {
		skip_to_endif = 0;
		return 1;
	}

	/* don't do any commands if skipping a section */
	if (skip_to_endif == 1)
		return 1;

	if (!strncmp(cmd, "VAR ", 4)) {
		parse_var(&cmd[4]);
		return 1;
	}

	if (!strcmp(cmd, "HOST")) {
		printf("%s\n", currups->sys);
		return 1;
	}

	if (!strcmp(cmd, "HOSTDESC")) {
		printf("%s\n", currups->desc);
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

	if (!strcmp(cmd, "UTILITYCOLOR")) {
		do_utilitycolor();
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

	if (!strncmp(cmd, "DATE ", 5))
		return do_date(&cmd[5]);

	if (!strncmp(cmd, "IMG ", 4))
		return do_img(&cmd[4]);

	if (!strcmp(cmd, "VERSION")) {
		printf("%s\n", UPS_VERSION);
		return 1;
	}

	if (!strcmp(cmd, "REFRESH")) {
		if (refreshdelay > 0)
			printf("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"%d\">\n", 
				refreshdelay);

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
		if (forofs == 0)
			return 1;
			
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

	if (!strncmp(cmd, "IFSUPP ", 7)) {
		do_ifsupp(&cmd[7]);
		return 1;
	}

	if (!strcmp(cmd, "UPSTEMP")) {
		do_temp("ups.temperature");
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
		
	return 0;
}

static void display_template(const char *tfn)
{
	char	fn[SMALLBUF], buf[LARGEBUF];	

	snprintf(fn, sizeof(fn), "%s/%s", confpath(), tfn);

	tf = fopen(fn, "r");

	if (!tf) {
		fprintf(stderr, "upsstats: Can't open %s: %s\n", 
			fn, strerror(errno));

		printf("Error: can't open template file (%s)\n", tfn);

		exit(EXIT_FAILURE);
	}

	while (fgets(buf, sizeof(buf), tf)) {
		buf[strlen(buf) - 1] = '\0';

		if (!parse_line(buf))
			printf("%s\n", buf);
	}

	fclose(tf);
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
	PCONF_CTX	ctx;

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

	display_template("upsstats-single.html");

	upscli_disconnect(&ups);
}

int main(int argc, char **argv)
{
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
