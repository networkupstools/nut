/* upsstats - cgi program to generate the main ups info page

   Copyright (C) 1998  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2005  Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
   Copyright (C) 2020-2026 Jim Klimov <jimklimov+nut@gmail.com>

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
#include "nut_stdint.h"
#include "timehead.h"
#include "upsclient.h"
#include "status.h"
#include "strjson.h"
#include "cgilib.h"
#include "parseconf.h"
#include "upsstats.h"
#include "upsimagearg.h"

#define MAX_CGI_STRLEN 128
#define MAX_PARSE_ARGS 16

/* network timeout for initial connection, in seconds */
#define UPSCLI_DEFAULT_CONNECT_TIMEOUT	"10"

static char	*monhost = NULL;
static int	use_celsius = 1, refreshdelay = -1, treemode = 0;
static int	output_json = 0;

/* call tracing for debug */
static int	call_depth = 0;
#define upsdebug_call_starting0()	upsdebugx(2, "[depth=%02d+] starting %s...", call_depth++, __func__)
#define upsdebug_call_starting1(msgfmt)	upsdebugx(2, "[depth=%02d+] starting %s " msgfmt "...", call_depth++, __func__)
#define upsdebug_call_starting2(msgfmt, arg1)	upsdebugx(2, "[depth=%02d+] starting %s " msgfmt "...", call_depth++, __func__, arg1)
#define upsdebug_call_starting3(msgfmt, arg1, arg2)	upsdebugx(2, "[depth=%02d+] starting %s " msgfmt "...", call_depth++, __func__, arg1, arg2)
#define upsdebug_call_starting4(msgfmt, arg1, arg2, arg3)	upsdebugx(2, "[depth=%02d+] starting %s " msgfmt "...", call_depth++, __func__, arg1, arg2, arg3)

#define upsdebug_call_starting_for_str1(arg1)	upsdebug_call_starting2("for '%s'", NUT_STRARG(arg1))
#define upsdebug_call_starting_for_str2(arg1, arg2)	upsdebug_call_starting3("for '%s' '%s'", NUT_STRARG(arg1), NUT_STRARG(arg2))
#define upsdebug_call_starting_for_str3(arg1, arg2, arg3)	upsdebug_call_starting4("for '%s' '%s' '%s'", NUT_STRARG(arg1), NUT_STRARG(arg2), NUT_STRARG(arg3))

#define upsdebug_call_finished0()	upsdebugx(2, "[depth=%02d-] finished %s", --call_depth, __func__)
#define upsdebug_call_finished1(msgfmt)	upsdebugx(2, "[depth=%02d-] finished %s " msgfmt, --call_depth, __func__)
#define upsdebug_call_finished2(msgfmt, arg1)	upsdebugx(2, "[depth=%02d-] finished %s " msgfmt, --call_depth, __func__, arg1)
#define upsdebug_call_finished3(msgfmt, arg1, arg2)	upsdebugx(2, "[depth=%02d-] finished %s " msgfmt, --call_depth, __func__, arg1, arg2)
#define upsdebug_call_finished4(msgfmt, arg1, arg2, arg3)	upsdebugx(2, "[depth=%02d-] finished %s " msgfmt, --call_depth, __func__, arg1, arg2, arg3)

	/* from cgilib's checkhost() */
static char	*monhostdesc = NULL;

static uint16_t	port;
static char	*upsname, *hostname;
static char	*upsimgpath = "upsimage.cgi" EXEEXT, *upsstatpath = "upsstats.cgi" EXEEXT,
	*template_single = NULL, *template_list = NULL;
static UPSCONN_t	ups;

static FILE	*tf;
static long	forofs = 0;

static ulist_t	*ulhead = NULL, *currups = NULL;

static int	skip_clause = 0, skip_block = 0;

void parsearg(char *var, char *value)
{
	upsdebug_call_starting_for_str2(var, value);

	/* avoid bogus junk from evil people */
	if ((strlen(var) > MAX_CGI_STRLEN) || (strlen(value) > MAX_CGI_STRLEN)) {
		upsdebug_call_finished1(": strings too long for CGI");
		return;
	}

	if (!strcmp(var, "host")) {
		free(monhost);
		monhost = xstrdup(value);
	}

	if (!strcmp(var, "refresh"))
		refreshdelay = (int) strtol(value, (char **) NULL, 10);

	if (!strcmp(var, "treemode")) {
		/* FIXME: Validate that treemode is allowed */
		treemode = 1;
	}

	if (!strcmp(var, "json")) {
		output_json = 1;
	}

	if (!strcmp(var, "template_single")) {
		/* Error-checking in display_template(), when we have all options in place */
		free(template_single);
		template_single = xstrdup(value);
	}

	if (!strcmp(var, "template_list")) {
		/* Error-checking in display_template(), when we have all options in place */
		free(template_list);
		template_list = xstrdup(value);
	}

	upsdebug_call_finished0();
}

static void report_error(void)
{
	upsdebug_call_starting0();

	if (upscli_upserror(&ups) == UPSCLI_ERR_VARNOTSUPP)
		printf("Not supported\n");
	else
		printf("[error: %s]\n", upscli_strerror(&ups));

	upsdebug_call_finished0();
}

/* make sure we're actually connected to upsd */
static int check_ups_fd(int do_report)
{
	upsdebug_call_starting0();

	if (upscli_fd(&ups) == -1) {
		if (do_report)
			report_error();

		upsdebug_call_finished1(": upscli_fd() failed");
		return 0;
	}

	/* also check for insanity in currups */

	if (!currups) {
		if (do_report)
			printf("No UPS specified for monitoring\n");

		upsdebug_call_finished1(": currups is null");
		return 0;
	}

	/* must be OK */
	upsdebug_call_finished0();
	return 1;
}

static int get_var(const char *var, char *buf, size_t buflen, int verbose)
{
	int	ret;
	size_t	numq, numa;
	const	char	*query[4];
	char	**answer;

	upsdebug_call_starting_for_str2(upsname, var);

	/* pass verbose to check_ups_fd */
	if (!check_ups_fd(verbose)) {
		upsdebug_call_finished0();
		return 0;
	}

	if (!upsname) {
		if (verbose)
			printf("[No UPS name specified]\n");

		upsdebug_call_finished1(": no UPS name");
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

		upsdebug_call_finished1(": upscli_get() failed");
		return 0;
	}

	if (numa < numq) {
		if (verbose)
			printf("[Invalid response]\n");

		upsdebug_call_finished1(": invalid response");
		return 0;
	}

	snprintf(buf, buflen, "%s", answer[3]);
	upsdebug_call_finished0();
	return 1;
}

static void parse_var(const char *var)
{
	char	answer[SMALLBUF];

	upsdebug_call_starting_for_str1(var);

	if (!get_var(var, answer, sizeof(answer), 1)) {
		upsdebug_call_finished1(": get_var() failed");
		return;
	}

	printf("%s", answer);
	upsdebug_call_finished0();
}

static void do_status(void)
{
	int	i;
	char	status[SMALLBUF], *ptr, *last = NULL;

	upsdebug_call_starting0();

	if (!get_var("ups.status", status, sizeof(status), 1)) {
		upsdebug_call_finished1(": get_var() failed");
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

	upsdebug_call_finished0();
}

static void do_runtime(void)
{
	int 	total, hours, minutes, seconds;
	char	runtime[SMALLBUF];

	upsdebug_call_starting0();

	if (!get_var("battery.runtime", runtime, sizeof(runtime), 1)) {
		upsdebug_call_finished1(": get_var() failed");
		return;
	}

	total = (int) strtol(runtime, (char **) NULL, 10);

	hours = total / 3600;
	minutes = (total - (hours * 3600)) / 60;
	seconds = total % 60;

	printf("%02d:%02d:%02d", hours, minutes, seconds);
	upsdebug_call_finished0();
}

static int do_date(const char *buf)
{
	char	datebuf[SMALLBUF];
	time_t	tod;
	struct tm tmbuf;

	upsdebug_call_starting0();

	time(&tod);
	if (strftime(datebuf, sizeof(datebuf), buf, localtime_r(&tod, &tmbuf))) {
		printf("%s", datebuf);
		upsdebug_call_finished0();
		return 1;
	}

	upsdebug_call_finished1(": failed");
	return 0;
}

static int get_img_val(const char *var, const char *desc, const char *imgargs)
{
	char	answer[SMALLBUF];

	upsdebug_call_starting_for_str3(var, desc, imgargs);

	if (!get_var(var, answer, sizeof(answer), 1)) {
		upsdebug_call_finished1(": get_var() failed");
		return 1;
	}

	printf("<IMG SRC=\"%s?host=%s&amp;display=%s",
		upsimgpath, currups->sys, var);

	if ((imgargs) && (strlen(imgargs) > 0))
		printf("&amp;%s", imgargs);

	printf("\" ALT=\"%s: %s\">", desc, answer);

	upsdebug_call_finished0();
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
	int	ret = 0;

	upsdebug_call_starting2("for type '%s'", buf);

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
			|| !strcmp(type, "input.L3-L1.voltage")
	) {
		ret = get_img_val(type, "Input voltage", imgargs);
		goto finish;
	}

	if (!strcmp(type, "battery.voltage")) {
		ret = get_img_val(type, "Battery voltage", imgargs);
		goto finish;
	}

	if (!strcmp(type, "battery.charge")) {
		ret = get_img_val(type, "Battery charge", imgargs);
		goto finish;
	}

	if (!strcmp(type, "output.voltage")
			|| !strcmp(type, "output.L1-N.voltage")
			|| !strcmp(type, "output.L2-N.voltage")
			|| !strcmp(type, "output.L3-N.voltage")
			|| !strcmp(type, "output.L1-L2.voltage")
			|| !strcmp(type, "output.L2-L3.voltage")
			|| !strcmp(type, "output.L3-L1.voltage")
	) {
		ret = get_img_val(type, "Output voltage", imgargs);
		goto finish;
	}

	if (!strcmp(type, "ups.load")
			|| !strcmp(type, "output.L1.power.percent")
			|| !strcmp(type, "output.L2.power.percent")
			|| !strcmp(type, "output.L3.power.percent")
			|| !strcmp(type, "output.L1.realpower.percent")
			|| !strcmp(type, "output.L2.realpower.percent")
			|| !strcmp(type, "output.L3.realpower.percent")
	) {
		ret = get_img_val(type, "UPS load", imgargs);
		goto finish;
	}

	if (!strcmp(type, "input.frequency")) {
		ret = get_img_val(type, "Input frequency", imgargs);
		goto finish;
	}

	if (!strcmp(type, "output.frequency")) {
		ret = get_img_val(type, "Output frequency", imgargs);
		goto finish;
	}

	if (!strcmp(type, "ups.temperature")) {
		ret = get_img_val(type, "UPS temperature", imgargs);
		goto finish;
	}

	if (!strcmp(type, "ambient.temperature")) {
		ret = get_img_val(type, "Ambient temperature", imgargs);
		goto finish;
	}

	if (!strcmp(type, "ambient.humidity")) {
		ret = get_img_val(type, "Ambient humidity", imgargs);
		goto finish;
	}

finish:
	upsdebug_call_finished0();
	return ret;
}

static void ups_connect(void)
{
	static ulist_t	*lastups = NULL;
	char	*newups, *newhost;
	uint16_t	newport = 0;

	upsdebug_call_starting0();

	/* try to minimize reconnects */
	if (lastups) {

		/* don't reconnect if these are both the same UPS */
		if (currups && !strcmp(lastups->sys, currups->sys)) {
			lastups = currups;
			upsdebug_call_finished1(": skip: lastups same as currups");
			return;
		}

		/* see if it's just on the same host */
		newups = newhost = NULL;

		if (currups
		 && upscli_splitname(currups->sys, &newups, &newhost, &newport) != 0
		) {
			printf("Unusable UPS definition [%s]\n", currups->sys);
			fprintf(stderr, "Unusable UPS definition [%s]\n",
				currups->sys);
			upsdebug_call_finished1(": Unusable UPS definition");
			exit(EXIT_FAILURE);
		}

		if (currups && hostname && (!strcmp(newhost, hostname)) && (port == newport)) {
			free(upsname);
			upsname = newups;

			free(newhost);
			lastups = currups;

			upsdebug_call_finished2(": pick next device on already connected data server [%s]", NUT_STRARG(currups->sys));
			return;
		}

		/* not the same upsd, so disconnect */
		free(newups);
		free(newhost);

		upsdebugx(2, "%s: not same data server as used by lastups: will connect to another", __func__);
	}

	upscli_disconnect(&ups);

	free(upsname);
	free(hostname);
	upsname = NULL;
	hostname = NULL;

	if (currups && upscli_splitname(currups->sys, &upsname, &hostname, &port) != 0) {
		printf("Unusable UPS definition [%s]\n", currups->sys);
		fprintf(stderr, "Unusable UPS definition [%s]\n", currups->sys);
		upsdebug_call_finished1(": Unusable UPS definition");
		exit(EXIT_FAILURE);
	}

	if (currups && upscli_connect(&ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0)
		fprintf(stderr, "UPS [%s]: can't connect to server: %s\n", currups->sys, upscli_strerror(&ups));

	lastups = currups;
	upsdebug_call_finished2(": pick first device on newly connected data server [%s]", NUT_STRARG(currups->sys));
}

static void do_hostlink(void)
{
	upsdebug_call_starting0();

	if (!currups) {
		upsdebug_call_finished1(": no-op: no currups!");
		return;
	}

	printf("<a href=\"%s?host=%s", upsstatpath, currups->sys);

	if (template_single && strcmp(template_single, "upsstats-single.html")) {
		printf("&amp;template_single=%s", template_single);
	}

	if (template_list && strcmp(template_list, "upsstats.html")) {
		printf("&amp;template_list=%s", template_list);
	}

	if (refreshdelay > 0) {
		printf("&amp;refresh=%d", refreshdelay);
	}

	printf("\">%s</a>", currups->desc);
	upsdebug_call_finished0();
}

static void do_treelink_json(const char *text)
{
	upsdebug_call_starting0();

	if (!currups) {
		upsdebug_call_finished1(": no-op: no currups!");
		return;
	}

	printf("<a href=\"%s?host=%s&amp;json",
		upsstatpath, currups->sys);

	if (template_single && strcmp(template_single, "upsstats-single.html")) {
		printf("&amp;template_single=%s", template_single);
	}

	if (template_list && strcmp(template_list, "upsstats.html")) {
		printf("&amp;template_list=%s", template_list);
	}

	if (refreshdelay > 0) {
		printf("&amp;refresh=%d", refreshdelay);
	}

	printf("\">%s</a>",
		((text && *text) ? text : "JSON"));

	upsdebug_call_finished0();
}

static void do_treelink(const char *text)
{
	upsdebug_call_starting0();

	if (!currups) {
		upsdebug_call_finished1(": no-op: no currups!");
		return;
	}

	printf("<a href=\"%s?host=%s&amp;treemode",
		upsstatpath, currups->sys);

	if (template_single && strcmp(template_single, "upsstats-single.html")) {
		printf("&amp;template_single=%s", template_single);
	}

	if (template_list && strcmp(template_list, "upsstats.html")) {
		printf("&amp;template_list=%s", template_list);
	}

	if (refreshdelay > 0) {
		printf("&amp;refresh=%d", refreshdelay);
	}

	printf("\">%s</a>",
		((text && *text) ? text : "All data"));

	upsdebug_call_finished0();
}

/* see if the UPS supports this variable - skip to the next ENDIF if not */
/* if val is not null, value returned by var must be equal to val to match */
static void do_ifsupp(const char *var, const char *val)
{
	char	dummy[SMALLBUF];

	upsdebug_call_starting3("for '%s' ( =? '%s')", NUT_STRARG(var), NUT_STRARG(val));

	/* if not connected, act like it's not supported and skip the rest */
	if (!check_ups_fd(0)) {
		skip_clause = 1;
		upsdebug_call_finished1(": check_ups_fd() failed");
		return;
	}

	if (!get_var(var, dummy, sizeof(dummy), 0)) {
		skip_clause = 1;
		upsdebug_call_finished1(": get_var() failed");
		return;
	}

	if (!val) {
		upsdebug_call_finished1(": ok, not checking val");
		return;
	}

	if (strcmp(dummy, val)) {
		skip_clause = 1;
		upsdebug_call_finished1(": get_var() returned unexpected val");
		return;
	}

	upsdebug_call_finished0();
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

	upsdebug_call_starting_for_str1(s);

	strncpy(var, s, sizeof(var) - 1);

	nargs = breakargs(var, aa);
	if(nargs != 2) {
		printf("upsstats: IFEQ: Argument error!\n");
		upsdebug_call_finished1(": arg error");
		return;
	}

	do_ifsupp(aa[0], aa[1]);
	upsdebug_call_finished0();
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

	upsdebug_call_starting_for_str1(s);

	strncpy(var, s, sizeof(var) - 1);

	nargs = breakargs(var, aa);
	if (nargs != 3) {
		printf("upsstats: IFBETWEEN: Argument error!\n");
		upsdebug_call_finished1(": Argument error");
		return;
	}

	if (!check_ups_fd(0)) {
		upsdebug_call_finished1(": check_ups_fd() failed");
		return;
	}

	if (!get_var(aa[0], tmp, sizeof(tmp), 0)) {
		upsdebug_call_finished0();
		return;
	}
	v1 = strtol(tmp, &isvalid, 10);
	if (tmp == isvalid) {
		upsdebug_call_finished0();
		return;
	}

	if (!get_var(aa[1], tmp, sizeof(tmp), 0)) {
		upsdebug_call_finished0();
		return;
	}
	v2 = strtol(tmp, &isvalid, 10);
	if (tmp == isvalid) {
		upsdebug_call_finished0();
		return;
	}

	if (!get_var(aa[2], tmp, sizeof(tmp), 0)) {
		upsdebug_call_finished0();
		return;
	}
	v3 = strtol(tmp, &isvalid, 10);
	if (tmp == isvalid) {
		upsdebug_call_finished0();
		return;
	}

	if (v1 > v3 || v2 < v3) {
		skip_clause = 1;
		upsdebug_call_finished0();
		return;
	}

	upsdebug_call_finished0();
}

static void do_upsstatpath(const char *s) {
	upsdebug_call_starting_for_str1(s);

	if(strlen(s)) {
		upsstatpath = strdup(s);
	}

	upsdebug_call_finished0();
}

static void do_upsimgpath(const char *s) {
	upsdebug_call_starting_for_str1(s);

	if(strlen(s)) {
		upsimgpath = strdup(s);
	}

	upsdebug_call_finished0();
}

static void do_temp(const char *var)
{
	char	tempc[SMALLBUF];
	double	tempf;

	upsdebug_call_starting_for_str1(var);

	if (!get_var(var, tempc, sizeof(tempc), 1)) {
		upsdebug_call_finished1(": get_var() failed");
		return;
	}

	if (use_celsius) {
		printf("%s", tempc);
		upsdebug_call_finished0();
		return;
	}

	tempf = (strtod(tempc, (char **) NULL) * 1.8) + 32;
	printf("%.1f", tempf);
	upsdebug_call_finished0();
}

static void do_degrees(void)
{
	upsdebug_call_starting0();

	printf("&deg;");

	if (use_celsius)
		printf("C");
	else
		printf("F");

	upsdebug_call_finished0();
}

/* plug in the right color string (like #FF0000) for the UPS status */
static void do_statuscolor(void)
{
	int	severity, i;
	char	stat[SMALLBUF], *ptr, *last = NULL;

	upsdebug_call_starting0();

	if (!check_ups_fd(0)) {
		/* can't print the warning here - give a red error condition */
		printf("#FF0000");
		upsdebug_call_finished1(": check_ups_fd() failed");
		return;
	}

	if (!get_var("ups.status", stat, sizeof(stat), 0)) {
		/* status not available - give yellow as a warning */
		printf("#FFFF00");
		upsdebug_call_finished1(": get_var() failed");
		return;
	}

	severity = 0;

	/* Use strtok_r for safety */
	for (ptr = strtok_r(stat, " \n", &last); ptr != NULL; ptr = strtok_r(NULL, " \n", &last)) {

		/* expand from table in status.h */
		for (i = 0; stattab[i].name != NULL; i++)
			if (stattab[i].name && ptr && !strcmp(stattab[i].name, ptr))
				if (stattab[i].severity > severity)
					severity = stattab[i].severity;
	}

	switch(severity) {
		case 0:	printf("#00FF00"); break;	/* green  : OK      */
		case 1:	printf("#FFFF00"); break;	/* yellow : warning */

		default: printf("#FF0000"); break;	/* red    : error   */
	}

	upsdebug_call_finished0();
}

static int do_command(char *cmd)
{
	upsdebug_call_starting_for_str1(cmd);

	/* ending an if block? */
	if (!strcmp(cmd, "ENDIF")) {
		skip_clause = 0;
		skip_block = 0;
		upsdebug_call_finished1(": ENDIF");
		return 1;
	}

	/* Skipping a block means skip until ENDIF, so... */
	if (skip_block) {
		upsdebug_call_finished1(": skip until ENDIF");
		return 1;
	}

	/* Toggle state when we run across ELSE */
	if (!strcmp(cmd, "ELSE")) {
		if (skip_clause) {
			skip_clause = 0;
		} else {
			skip_block = 1;
		}
		upsdebug_call_finished1(": ELSE (state toggle)");
		return 1;
	}

	/* don't do any commands if skipping a section */
	if (skip_clause == 1) {
		upsdebug_call_finished1(": SKIP in effect");
		return 1;
	}

	if (!strncmp(cmd, "VAR ", 4)) {
		parse_var(&cmd[4]);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "HOST")) {
		printf("%s", currups->sys);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "HOSTDESC")) {
		printf("%s", currups->desc);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "RUNTIME")) {
		do_runtime();
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "STATUS")) {
		do_status();
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "STATUSCOLOR")) {
		do_statuscolor();
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "TEMPF")) {
		use_celsius = 0;
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "TEMPC")) {
		use_celsius = 1;
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "DATE ", 5)) {
		upsdebug_call_finished0();
		return do_date(&cmd[5]);
	}

	if (!strncmp(cmd, "IMG ", 4)) {
		upsdebug_call_finished0();
		return do_img(&cmd[4]);
	}

	if (!strcmp(cmd, "VERSION")) {
		printf("%s", UPS_VERSION);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "REFRESH")) {
		if (refreshdelay > 0) {
			printf("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"%d\">", refreshdelay);
		}
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "FOREACHUPS")) {
		forofs = ftell(tf);

		currups = ulhead;
		upsdebugx(2, "%s: FOREACHUPS: begin with UPS [%s] [%s]", __func__, NUT_STRARG(currups->sys), NUT_STRARG(currups->desc));
		upsdebugx(2, "%s: current skip_clause=%d skip_block=%d", __func__, skip_clause, skip_block);
		ups_connect();
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "ENDFOR")) {
		/* if not in a for, ignore this */
		if (forofs == 0) {
			upsdebug_call_finished1(": not in FOR");
			return 1;
		}

		upsdebugx(2, "%s: ENDFOR: done with UPS [%s] [%s]", __func__, NUT_STRARG(currups->sys), NUT_STRARG(currups->desc));
		upsdebugx(2, "%s: current skip_clause=%d skip_block=%d", __func__, skip_clause, skip_block);
		currups = (ulist_t *)currups->next;

		if (currups) {
			upsdebugx(2, "%s: ENDFOR: proceed with next UPS [%s]", __func__, NUT_STRARG(currups->desc));
			fseek(tf, forofs, SEEK_SET);
			ups_connect();
		}

		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "HOSTLINK")) {
		do_hostlink();
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "TREELINK_JSON ", 14)) {
		do_treelink_json(&cmd[14]);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "TREELINK_JSON")) {
		do_treelink_json(NULL);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "TREELINK ", 9)) {
		do_treelink(&cmd[9]);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "TREELINK")) {
		do_treelink(NULL);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "IFSUPP ", 7)) {
		do_ifsupp(&cmd[7], NULL);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "UPSTEMP")) {
		do_temp("ups.temperature");
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "BATTTEMP")) {
		do_temp("battery.temperature");
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "AMBTEMP")) {
		do_temp("ambient.temperature");
		upsdebug_call_finished0();
		return 1;
	}

	if (!strcmp(cmd, "DEGREES")) {
		do_degrees();
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "IFEQ ", 5)) {
		do_ifeq(&cmd[5]);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "IFBETWEEN ", 10)) {
		do_ifbetween(&cmd[10]);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "UPSSTATSPATH ", 13)) {
		do_upsstatpath(&cmd[13]);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "UPSIMAGEPATH ", 13)) {
		do_upsimgpath(&cmd[13]);
		upsdebug_call_finished0();
		return 1;
	}

	if (!strncmp(cmd, "NUT_UPSSTATS_TEMPLATE ", 22) || !strcmp(cmd, "NUT_UPSSTATS_TEMPLATE")) {
		upsdebugx(2, "%s: saw magic token, ignoring", __func__);
		upsdebug_call_finished0();
		return 1;
	}

	upsdebug_call_finished2(": unknown cmd: '%s'", cmd);
	return 0;
}

static void parse_line(const char *buf)
{
	char	cmd[SMALLBUF];
	size_t	i, len;
	char	do_cmd = 0;

	upsdebug_call_starting_for_str1(buf);

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
		assert (len < INT_MAX);

		if (do_cmd) {
			snprintf(cmd, sizeof(cmd), "%.*s", (int)len, &buf[i]);
			continue;
		}

		if (skip_clause || skip_block) {
			/* ignore this */
			continue;
		}

		/* pass it trough */
		printf("%.*s", (int)len, &buf[i]);
	}

	upsdebug_call_finished0();
}

static void display_template(const char *tfn)
{
	char	fn[NUT_PATH_MAX + 1], buf[LARGEBUF];

	upsdebug_call_starting_for_str1(tfn);

	if (!tfn || !*tfn || strstr(tfn, "/") || strstr(tfn, "\\") || !strstr(tfn, ".htm")) {
		/* We only allow pre-configured templates in one managed location, with ".htm" in the name */
		fprintf(stderr, "upsstats: Can't open %s: %s: asked to look not exactly in the managed location\n", fn, strerror(errno));

		printf("Error: can't open template file (%s): asked to look not exactly in the managed location\n", tfn);

		upsdebug_call_finished1(": subdir in template");
		exit(EXIT_FAILURE);
	}

	snprintf(fn, sizeof(fn), "%s/%s", confpath(), tfn);

	tf = fopen(fn, "rb");

	if (!tf) {
		fprintf(stderr, "upsstats: Can't open %s: %s\n", fn, strerror(errno));

		printf("Error: can't open template file (%s)\n", tfn);

		upsdebug_call_finished1(": no template");
		exit(EXIT_FAILURE);
	}

	if (!fgets(buf, sizeof(buf), tf)) {
		fprintf(stderr, "upsstats: template file %s seems to be empty (fgets failed): %s\n", fn, strerror(errno));

		printf("Error: template file %s seems to be empty\n", tfn);

		upsdebug_call_finished1(": empty template");
		exit(EXIT_FAILURE);
	}

	/* Test first line for a bit of expected magic */
	if (!strncmp(buf, "@NUT_UPSSTATS_TEMPLATE", 22)) {
		parse_line(buf);
	} else {
		fprintf(stderr, "upsstats: template file %s does not start with NUT_UPSSTATS_TEMPLATE command\n", fn);

		printf("Error: template file %s does not start with NUT_UPSSTATS_TEMPLATE command\n", tfn);

		upsdebug_call_finished1(": not a valid template");
		exit(EXIT_FAILURE);
	}

	while (fgets(buf, sizeof(buf), tf)) {
		parse_line(buf);
	}

	fclose(tf);
	upsdebug_call_finished0();
}

static void display_tree(int verbose)
{
	size_t	numq, numa;
	const	char	*query[4];
	char	**answer;

	upsdebug_call_starting0();

	if (!upsname) {
		if (verbose)
			printf("[No UPS name specified]\n");
		upsdebug_call_finished1(": No UPS name specified");
		return;
	}

	query[0] = "VAR";
	query[1] = upsname;
	numq = 2;

	if (upscli_list_start(&ups, numq, query) < 0) {
		if (verbose)
			report_error();
		upsdebug_call_finished1(": upscli_list_start() failed");
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

			upsdebug_call_finished1(": invalid response");
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
	upsdebug_call_finished0();
}

static void add_ups(char *sys, char *desc)
{
	ulist_t	*tmp, *last;

	tmp = last = ulhead;

	while (tmp) {
		last = tmp;
		tmp = (ulist_t *)tmp->next;
	}

	tmp = (ulist_t *)xmalloc(sizeof(ulist_t));

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
	char	fn[NUT_PATH_MAX + 1];
	PCONF_CTX_t	ctx;

	snprintf(fn, sizeof(fn), "%s/hosts.conf", confpath());
	upsdebugx(1, "%s: considering configuration file %s", __func__, fn);

	pconf_init(&ctx, upsstats_hosts_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		/* Don't print HTML here if we are in JSON mode.
		 * The JSON function will handle the error.
		 */
		if (!output_json) {
			printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n");
			printf("	\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n");
			printf("<HTML><HEAD>\n");
			printf("<TITLE>Error: can't open hosts.conf</TITLE>\n");
			printf("</HEAD><BODY>\n");
			printf("Error: can't open hosts.conf\n");
			printf("</BODY></HTML>\n");
		}

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
		/* Don't print HTML here if we are in JSON mode.
		 * The JSON function will handle the error.
		 */
		if (!output_json) {
			printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n");
			printf("	\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n");
			printf("<HTML><HEAD>\n");
			printf("<TITLE>Error: no hosts to monitor</TITLE>\n");
			printf("</HEAD><BODY>\n");
			printf("Error: no hosts to monitor (check <CODE>hosts.conf</CODE>)\n");
			printf("</BODY></HTML>\n");
		}

		/* leave something for the admin */
		fprintf(stderr, "upsstats: no hosts to monitor\n");
		exit(EXIT_FAILURE);
	}
}

static void display_single(void)
{
	upsdebug_call_starting0();

	if (!checkhost(monhost, &monhostdesc)) {
		printf("Access to that host [%s] is not authorized.\n",
			monhost);
		upsdebug_call_finished1(": not auth");
		exit(EXIT_FAILURE);
	}

	add_ups(monhost, monhostdesc);

	currups = ulhead;
	ups_connect();

	/* switch between data tree view and standard single view */
	if (treemode)
		display_tree(1);
	else
		display_template(template_single);

	upscli_disconnect(&ups);
	upsdebug_call_finished0();
}

/* ------------------------------------------------------------- */
/* ---NEW FUNCTION FOR JSON API -------------------------------- */
/* ------------------------------------------------------------- */

/**
 * @brief Main JSON output function.
 * This function replaces all template logic and outputs a JSON object
 * containing data for one or all devices.
 */
static void display_json(void)
{
	size_t	numq, numa;
	const	char	*query[4];
	char	**answer;
	char	status_buf[SMALLBUF], status_copy[SMALLBUF];
	int i;
	int is_first_status;
	int is_first_ups = 1;
	int is_first_var = 1;
	char *ptr, *last = NULL;

	upsdebug_call_starting0();

	/* If monhost is set, we're in single-host mode.
	 * If not, we're in multi-host mode.
	 * We need to load hosts.conf ONLY in multi-host mode.
	 */
	if (monhost) {
		if (!checkhost(monhost, &monhostdesc)) {
			printf("{\"error\": \"Access to host %s is not authorized.\"}", monhost);
			upsdebug_call_finished1(": not auth");
			return;
		}
		add_ups(monhost, monhostdesc);
		currups = ulhead;
	} else {
		load_hosts_conf(); /* This populates ulhead */
		currups = ulhead;
	}

	if (!currups) {
		/* load_hosts_conf() would have exited, but check anyway */
		printf("{\"error\": \"No hosts to monitor.\"}");
		upsdebug_call_finished1(": No hosts to monitor");
		return;
	}

	/* In multi-host mode, wrap in a root object.
	 * In single-host mode, just output the single device object.
	 */
	if (!monhost) {
		printf("{\"devices\": [\n");
	}

	/* Loop through all devices (in single-host mode, this is just one) */
	for (currups = ulhead; currups != NULL; currups = (ulist_t *)currups->next) {
		ups_connect();

		if (!is_first_ups) printf(",\n");

		if (upscli_fd(&ups) == -1) {
			printf("  {\"host\": \"");
			json_print_esc(currups->sys);
			printf("\", \"desc\": \"");
			json_print_esc(currups->desc);
			printf("\", \"error\": \"Connection failed: %s\"}", upscli_strerror(&ups));
			is_first_ups = 0;
			continue;
		}

		printf("  {\n"); /* Start UPS object */
		printf("    \"host\": \"");
		json_print_esc(currups->sys);
		printf("\",\n");
		printf("    \"desc\": \"");
		json_print_esc(currups->desc);
		printf("\",\n");

		/* Add pre-processed status, as the old template did */
		if (get_var("ups.status", status_buf, sizeof(status_buf), 0)) {
			printf("    \"status_raw\": \"");
			json_print_esc(status_buf);
			printf("\",\n");
			printf("    \"status_parsed\": [");

			is_first_status = 1;
			/* Copy status_buf as strtok_r is destructive */
			strncpy(status_copy, status_buf, sizeof(status_copy));
			status_copy[sizeof(status_copy) - 1] = '\0';

			for (ptr = strtok_r(status_copy, " \n", &last); ptr != NULL; ptr = strtok_r(NULL, " \n", &last)) {
				for (i = 0; stattab[i].name != NULL; i++) {
					if (stattab[i].name && ptr && !strcasecmp(stattab[i].name, ptr)) {
						if (!is_first_status) printf(", ");
						printf("\"");
						json_print_esc(stattab[i].desc);
						printf("\"");
						is_first_status = 0;
					}
				}
			}
			printf("],\n");
		}

		printf("    \"vars\": {\n"); /* Start vars object */
		is_first_var = 1;

		/* Full tree mode: list all variables */
		query[0] = "VAR";
		query[1] = upsname;
		numq = 2;

		if (upscli_list_start(&ups, numq, query) < 0) {
			printf("      \"error\": \"Failed to list variables: %s\"", upscli_strerror(&ups));
		} else {
			while (upscli_list_next(&ups, numq, query, &numa, &answer) == 1) {
				if (numa < 4) continue; /* Invalid response */

				if (!is_first_var) printf(",\n");

				printf("      \"");
				json_print_esc(answer[2]); /* var name */
				printf("\": \"");
				json_print_esc(answer[3]); /* var value */
				printf("\"");

				is_first_var = 0;
			}
		}

		printf("\n    }\n"); /* End vars object */
		printf("  }"); /* End UPS object */

		is_first_ups = 0;
		upscli_disconnect(&ups); /* Disconnect after each UPS */
	}

	/* Close the root object in multi-host mode */
	if (!monhost) {
		printf("\n]}\n");
	}

	upsdebug_call_finished0();
}


/* ------------------------------------------------------------- */
/* --- END: NEW JSON FUNCTION ---------------------------------- */
/* ------------------------------------------------------------- */


int main(int argc, char **argv)
{
	char *s;
	int i;

#ifdef WIN32
	/* Required ritual before calling any socket functions */
	static WSADATA	WSAdata;
	static int	WSA_Started = 0;
	if (!WSA_Started) {
		WSAStartup(2, &WSAdata);
		atexit((void(*)(void))WSACleanup);
		WSA_Started = 1;
	}

	/* Avoid binary output conversions, e.g.
	 * mangling what looks like CRLF on WIN32 */
	setmode(STDOUT_FILENO, O_BINARY);
#endif

	NUT_UNUSED_VARIABLE(argc);
	NUT_UNUSED_VARIABLE(argv);

	/* NOTE: Caller must `export NUT_DEBUG_LEVEL` to see debugs for upsc
	 * and NUT methods called from it. This line aims to just initialize
	 * the subsystem, and set initial timestamp. Debugging the client is
	 * primarily of use to developers, so is not exposed via `-D` args.
	 */
	s = getenv("NUT_DEBUG_LEVEL");
	if (s && str_to_int(s, &i, 10) && i > 0) {
		nut_debug_level = i;
	}

	/* Built-in defaults */
	template_single = xstrdup("upsstats-single.html");
	template_list = xstrdup("upsstats.html");

#ifdef NUT_CGI_DEBUG_UPSSTATS
# if (NUT_CGI_DEBUG_UPSSTATS - 0 < 1)
#  undef NUT_CGI_DEBUG_UPSSTATS
#  define NUT_CGI_DEBUG_UPSSTATS 6
# endif
	/* Un-comment via make flags when developer-troubleshooting: */
	nut_debug_level = NUT_CGI_DEBUG_UPSSTATS;
#endif

	if (nut_debug_level > 0) {
		cgilogbit_set();
		printf("Content-type: text/html\n");
		printf("Pragma: no-cache\n");
		printf("\n");
		printf("<p>NUT CGI Debugging enabled, level: %d</p>\n\n", nut_debug_level);
	}

	extractcgiargs();

	upscli_init_default_connect_timeout(NULL, NULL, UPSCLI_DEFAULT_CONNECT_TIMEOUT);

	/*
	 * If json is in the query, bypass all HTML and call display_json()
	 */
	if (output_json) {
		printf("Content-type: application/json; charset=utf-8\n");
		printf("Pragma: no-cache\n");
		printf("\n");

		display_json();

		/* Clean up memory */
		free(monhost);
		while (ulhead) {
			currups = (ulist_t *)ulhead->next;
			free(ulhead->sys);
			free(ulhead->desc);
			free(ulhead);
			ulhead = currups;
		}
		free(upsname);
		free(hostname);
		free(template_single);
		free(template_list);

		exit(EXIT_SUCCESS);
	}

	/* --- Original HTML logic continues below --- */

	printf("Content-type: text/html\n");
	printf("Pragma: no-cache\n");
	printf("\n");

	/* if a host is specified, use upsstats-single.html instead */
	if (monhost) {
		display_single();
	} else {
		/* default: multimon replacement mode */
		load_hosts_conf();
		currups = ulhead;
		display_template(template_list);
	}

	/* Clean up memory */
	free(monhost);
	upscli_disconnect(&ups);
	free(upsname);
	free(hostname);
	while (ulhead) {
		currups = (ulist_t *)ulhead->next;
		free(ulhead->sys);
		free(ulhead->desc);
		free(ulhead);
		ulhead = currups;
	}
	free(template_single);
	free(template_list);

	return 0;
}
