/* upslog - log ups values to a file for later collection and analysis

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

/* Basic theory of operation:
 *
 * First we go through and parse as much of the status format string as
 * possible.  We used to do this parsing run every time, but that's a
 * waste of CPU since it can't change during the program's run.
 *
 * This version does the parsing pass once, and creates a linked list of
 * pointers to the functions that do the work and the arg they get.
 *
 * That means the main loop just has to run the linked list and call
 * anything it finds in there.  Everything happens from there, and we
 * don't have to pointlessly reparse the string every time around.
 */

#include "common.h"
#include "nut_platform.h"
#include "upsclient.h"

#include "config.h"
#include "timehead.h"
#include "upslog.h"

	static	int	port, reopen_flag = 0, exit_flag = 0;
	static	char	*upsname, *hostname;
	static	UPSCONN_t	ups;

	static	FILE	*logfile;
	static	const	char *logfn, *monhost;
	static	sigset_t	nut_upslog_sigmask;
	static	char	logbuffer[LARGEBUF], *logformat;

	static	flist_t	*fhead = NULL;

#define DEFAULT_LOGFORMAT "%TIME @Y@m@d @H@M@S% %VAR battery.charge% " \
		"%VAR input.voltage% %VAR ups.load% [%VAR ups.status%] " \
		"%VAR ups.temperature% %VAR input.frequency%"

static void reopen_log(void)
{
	if (logfile == stdout) {
		upslogx(LOG_INFO, "logging to stdout");
		return;
	}

	fclose(logfile);
	logfile = fopen(logfn, "a");
	if (logfile == NULL)
		fatal_with_errno(EXIT_FAILURE, "could not reopen logfile %s", logfn);
}

static void set_reopen_flag(int sig)
{
	reopen_flag = sig;
}

static void set_exit_flag(int sig)
{
	exit_flag = sig;
}

static void set_print_now_flag(int sig)
{
	NUT_UNUSED_VARIABLE(sig);

	/* no need to do anything, the signal will cause sleep to be interrupted */
}

/* handlers: reload on HUP, exit on INT/QUIT/TERM */
static void setup_signals(void)
{
	struct	sigaction	sa;

	sigemptyset(&nut_upslog_sigmask);
	sigaddset(&nut_upslog_sigmask, SIGHUP);
	sa.sa_mask = nut_upslog_sigmask;
	sa.sa_handler = set_reopen_flag;
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		fatal_with_errno(EXIT_FAILURE, "Can't install SIGHUP handler");

	sa.sa_handler = set_exit_flag;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		fatal_with_errno(EXIT_FAILURE, "Can't install SIGINT handler");
	if (sigaction(SIGQUIT, &sa, NULL) < 0)
		fatal_with_errno(EXIT_FAILURE, "Can't install SIGQUIT handler");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		fatal_with_errno(EXIT_FAILURE, "Can't install SIGTERM handler");

	sa.sa_handler = set_print_now_flag;
	if (sigaction(SIGUSR1, &sa, NULL) < 0)
		fatal_with_errno(EXIT_FAILURE, "Can't install SIGUSR1 handler");
}

static void help(const char *prog)
	__attribute__((noreturn));

static void help(const char *prog)
{
	printf("UPS status logger.\n");

	printf("\nusage: %s [OPTIONS]\n", prog);
	printf("\n");

	printf("  -f <format>	- Log format.  See below for details.\n");
	printf("		- Use -f \"<format>\" so your shell doesn't break it up.\n");
	printf("  -i <interval>	- Time between updates, in seconds\n");
	printf("  -l <logfile>	- Log file name, or - for stdout\n");
	printf("  -p <pidbase>  - Base name for PID file (defaults to \"%s\")\n", prog);
	printf("  -s <ups>	- Monitor UPS <ups> - <upsname>@<host>[:<port>]\n");
	printf("        	- Example: -s myups@server\n");
	printf("  -u <user>	- Switch to <user> if started as root\n");

	printf("\n");
	printf("Some valid format string escapes:\n");
	printf("\t%%%% insert a single %%\n");
	printf("\t%%TIME format%% insert the time with strftime formatting\n");
	printf("\t%%HOST%% insert the local hostname\n");
	printf("\t%%UPSHOST%% insert the host of the ups being monitored\n");
	printf("\t%%PID%% insert the pid of upslog\n");
	printf("\t%%VAR varname%% insert the value of ups variable varname\n\n");
	printf("format string defaults to:\n");
	printf("%s\n", DEFAULT_LOGFORMAT);

	printf("\n");
	printf("See the upslog(8) man page for more information.\n");

	exit(EXIT_SUCCESS);
}

/* print current host name */
static void do_host(const char *arg)
{
	int	ret;
	char	hn[LARGEBUF];
	NUT_UNUSED_VARIABLE(arg);

	ret = gethostname(hn, sizeof(hn));

	if (ret != 0) {
		upslog_with_errno(LOG_ERR, "gethostname failed");
		return;
	}

	snprintfcat(logbuffer, sizeof(logbuffer), "%s", hn);
}

static void do_upshost(const char *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	snprintfcat(logbuffer, sizeof(logbuffer), "%s", monhost);
}

static void do_pid(const char *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	snprintfcat(logbuffer, sizeof(logbuffer), "%ld", (long)getpid());
}

static void do_time(const char *arg)
{
	unsigned int	i;
	char	timebuf[SMALLBUF], *format;
	time_t	tod;
	struct tm tmbuf;

	format = xstrdup(arg);

	/* @s are used on the command line since % is taken */
	for (i = 0; i < strlen(format); i++)
		if (format[i] == '@')
			format[i] = '%';

	time(&tod);
	strftime(timebuf, sizeof(timebuf), format, localtime_r(&tod, &tmbuf));

	snprintfcat(logbuffer, sizeof(logbuffer), "%s", timebuf);

	free(format);
}

static void getvar(const char *var)
{
	int	ret;
	size_t	numq, numa;
	const	char	*query[4];
	char	**answer;

	query[0] = "VAR";
	query[1] = upsname;
	query[2] = var;
	numq = 3;

	ret = upscli_get(&ups, numq, query, &numa, &answer);

	if ((ret < 0) || (numa < numq)) {
		snprintfcat(logbuffer, sizeof(logbuffer), "NA");
		return;
	}

	snprintfcat(logbuffer, sizeof(logbuffer), "%s", answer[3]);
}

static void do_var(const char *arg)
{
	if ((!arg) || (strlen(arg) < 1)) {
		snprintfcat(logbuffer, sizeof(logbuffer), "INVALID");
		return;
	}

	/* old variable names are no longer supported */
	if (!strchr(arg, '.')) {
		snprintfcat(logbuffer, sizeof(logbuffer), "INVALID");
		return;
	}

	/* a UPS name is now required */
	if (!upsname) {
		snprintfcat(logbuffer, sizeof(logbuffer), "INVALID");
		return;
	}

	getvar(arg);
}

static void do_etime(const char *arg)
{
	time_t	tod;
	NUT_UNUSED_VARIABLE(arg);

	time(&tod);
	snprintfcat(logbuffer, sizeof(logbuffer), "%ld", (unsigned long) tod);
}

static void print_literal(const char *arg)
{
	snprintfcat(logbuffer, sizeof(logbuffer), "%s", arg);
}

/* register another parsing function to be called later */
static void add_call(void (*fptr)(const char *arg), const char *arg)
{
	flist_t	*tmp, *last;

	tmp = last = fhead;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(flist_t));

	tmp->fptr = fptr;

	if (arg)
		tmp->arg = xstrdup(arg);
	else
		tmp->arg = NULL;

	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		fhead = tmp;
}

/* turn the format string into a list of function calls with args */
static void compile_format(void)
{
	size_t	i;
	int	j, found;
	size_t	ofs;
	char	*cmd, *arg, *ptr;

	for (i = 0; i < strlen(logformat); i++) {

		/* if not a % sequence, append character and start over */
		if (logformat[i] != '%') {
			char	buf[4];

			/* we have to stuff it into a string first */
			snprintf(buf, sizeof(buf), "%c", logformat[i]);
			add_call(print_literal, buf);

			continue;
		}

		/* if a %%, append % and start over */
		if (logformat[i+1] == '%') {
			add_call(print_literal, "%");

			/* make sure we don't parse the second % next time */
			i++;
			continue;
		}

		/* it must start with a % now - %<cmd>[ <arg>]%*/

		cmd = xstrdup(&logformat[i+1]);
		ptr = strchr(cmd, '%');

		/* no trailing % = broken */
		if (!ptr) {
			add_call(print_literal, "INVALID");
			free(cmd);
			continue;
		}

		*ptr = '\0';

		/* remember length (plus first %) so we can skip over it */
		ofs = strlen(cmd) + 1;

		/* jump out to argument (if any) */
		arg = strchr(cmd, ' ');
		if (arg)
			*arg++ = '\0';

		found = 0;

		/* see if we know how to handle this command */

		for (j = 0; logcmds[j].name != NULL; j++) {
			if (strncasecmp(cmd, logcmds[j].name,
				strlen(logcmds[j].name)) == 0) {

				add_call(logcmds[j].func, arg);
				found = 1;
				break;
			}
		}

		free(cmd);

		if (!found)
			add_call(print_literal, "INVALID");

		/* now do the skip ahead saved from before */
		i += ofs;

	} /* for (i = 0; i < strlen(logformat); i++) */
}

/* go through the list of functions and call them in order */
static void run_flist(void)
{
	flist_t	*tmp;

	tmp = fhead;

	memset(logbuffer, 0, sizeof(logbuffer));

	while (tmp) {
		tmp->fptr(tmp->arg);

		tmp = tmp->next;
	}

	fprintf(logfile, "%s\n", logbuffer);
	fflush(logfile);
}

	/* -s <monhost>
	 * -l <log file>
	 * -i <interval>
	 * -f <format>
	 * -u <username>
	 */

int main(int argc, char **argv)
{
	int	interval = 30, i;
	const char	*prog = xbasename(argv[0]);
	time_t	now, nextpoll = 0;
	const char	*user = NULL;
	struct passwd	*new_uid = NULL;
	const char	*pidfilebase = prog;

	logformat = DEFAULT_LOGFORMAT;
	user = RUN_AS_USER;

	printf("Network UPS Tools %s %s\n", prog, UPS_VERSION);

	 while ((i = getopt(argc, argv, "+hs:l:i:f:u:Vp:")) != -1) {
		switch(i) {
			case 'h':
				help(prog);
#ifndef HAVE___ATTRIBUTE__NORETURN
				break;
#endif

			case 's':
				monhost = optarg;
				break;

			case 'l':
				logfn = optarg;
				break;

			case 'i':
				interval = atoi(optarg);
				break;

			case 'f':
				logformat = optarg;
				break;

			case 'u':
				user = optarg;
				break;

			case 'V':
				exit(EXIT_SUCCESS);

			case 'p':
				pidfilebase = optarg;
				break;
		}
	}

	argc -= optind;
	argv += optind;

	/* not enough args for the old way? */
	if ((argc == 1) || (argc == 2))
		help(prog);

	/* see if it's being called in the old style - 3 or 4 args */

	/* <system> <logfn> <interval> [<format>] */

	if (argc >= 3) {
		monhost = argv[0];
		logfn = argv[1];
		interval = atoi(argv[2]);
	}

	if (argc >= 4) {
		/* read out the remaining argv entries to the format string */

		logformat = xmalloc(LARGEBUF);
		memset(logformat, '\0', LARGEBUF);

		for (i = 3; i < argc; i++)
			snprintfcat(logformat, LARGEBUF, "%s ", argv[i]);
	}

	if (!monhost)
		fatalx(EXIT_FAILURE, "No UPS defined for monitoring - use -s <system>");

	if (!logfn)
		fatalx(EXIT_FAILURE, "No filename defined for logging - use -l <file>");

	/* shouldn't happen */
	if (!logformat)
		fatalx(EXIT_FAILURE, "No format defined - but this should be impossible");

	printf("logging status of %s to %s (%is intervals)\n",
		monhost, logfn, interval);

	if (upscli_splitname(monhost, &upsname, &hostname, &port) != 0) {
		fatalx(EXIT_FAILURE, "Error: invalid UPS definition.  Required format: upsname[@hostname[:port]]\n");
	}

	if (upscli_connect(&ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0)
		fprintf(stderr, "Warning: initial connect failed: %s\n",
			upscli_strerror(&ups));

	if (strcmp(logfn, "-") == 0)
		logfile = stdout;
	else
		logfile = fopen(logfn, "a");

	if (logfile == NULL)
		fatal_with_errno(EXIT_FAILURE, "could not open logfile %s", logfn);

	/* now drop root if we have it */
	new_uid = get_user_pwent(user);

	open_syslog(prog);

	if (logfile != stdout)
		background();

	setup_signals();

	writepid(pidfilebase);

	become_user(new_uid);

	compile_format();

	while (exit_flag == 0) {
		time(&now);

		if (nextpoll > now) {
			/* there is still time left, so sleep it off */
			sleep((unsigned int)(difftime(nextpoll, now)));
			nextpoll += interval;
		} else {
			/* we spent more time in polling than the interval allows */
			nextpoll = now + interval;
		}

		if (reopen_flag) {
			upslogx(LOG_INFO, "Signal %d: reopening log file",
				reopen_flag);
			reopen_log();
			reopen_flag = 0;
		}

		/* reconnect if necessary */
		if (upscli_fd(&ups) < 0) {
			upscli_connect(&ups, hostname, port, 0);
		}

		run_flist();

		/* don't keep connection open if we don't intend to use it shortly */
		if (interval > 30) {
			upscli_disconnect(&ups);
		}
	}

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);

	if (logfile != stdout)
		fclose(logfile);

	upscli_disconnect(&ups);

	exit(EXIT_SUCCESS);
}


/* Formal do_upsconf_args implementation to satisfy linker on AIX */
#if (defined NUT_PLATFORM_AIX)
void do_upsconf_args(char *upsname, char *var, char *val) {
        fatalx(EXIT_FAILURE, "INTERNAL ERROR: formal do_upsconf_args called");
}
#endif  /* end of #if (defined NUT_PLATFORM_AIX) */
