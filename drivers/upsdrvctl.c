/* upsdrvctl.c - UPS driver controller

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "config.h"
#include "proto.h"
#include "version.h"
#include "common.h"
#include "upsconf.h"

typedef struct {
	char	*upsname;
	char	*driver;
	char	*port;
	int	sdorder;
	int	maxstartdelay;
	void	*next;
}	ups_t;

static	ups_t	*upstable = NULL;

static	int	verbose = 0, maxsdorder = 0, testmode = 0, exec_error = 0;

	/* timer - keeps us from getting stuck if a driver hangs */
static	int	maxstartdelay = 45;

	/* Directory where driver executables live */
static	char	*driverpath = NULL;

	/* passthrough to the drivers: chroot path and new user name */
static	char	*pt_root = NULL, *pt_user = NULL;

static	sigset_t	nut_upsdrvctl_sigmask;
static	struct	sigaction	sa;

void do_upsconf_args(char *upsname, char *var, char *val)
{
	ups_t	*tmp, *last;

	/* handle global declarations */
	if (!upsname) {
		if (!strcmp(var, "maxstartdelay"))
			maxstartdelay = atoi(val);

		if (!strcmp(var, "driverpath")) {
			if (driverpath)
				free(driverpath);

			driverpath = xstrdup(val);
		}

		/* ignore anything else - it's probably for main */

		return;
	}

	last = tmp = upstable;

	while (tmp) {
		last = tmp;

		if (!strcmp(tmp->upsname, upsname)) {
			if (!strcmp(var, "driver")) 
				tmp->driver = xstrdup(val);

			if (!strcmp(var, "port")) 
				tmp->port = xstrdup(val);

			if (!strcmp(var, "maxstartdelay"))
				tmp->maxstartdelay = atoi(val);

			if (!strcmp(var, "sdorder")) {
				tmp->sdorder = atoi(val);

				if (tmp->sdorder > maxsdorder)
					maxsdorder = tmp->sdorder;
			}

			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ups_t));
	tmp->upsname = xstrdup(upsname);
	tmp->driver = NULL;
	tmp->port = NULL;
	tmp->next = NULL;
	tmp->sdorder = 0;
	tmp->maxstartdelay = -1;	/* use global value by default */

	if (!strcmp(var, "driver"))
		tmp->driver = xstrdup(val);

	if (!strcmp(var, "port"))
		tmp->port = xstrdup(val);

	if (last)
		last->next = tmp;
	else
		upstable = tmp;
}

static int get_driver_pid(const ups_t *ups)
{
	char	pidfn[SMALLBUF], buf[SMALLBUF];
	int	ret, pid;
	struct	stat	fs;
	FILE	*pidf;

	snprintf(pidfn, sizeof(pidfn), "%s/%s-%s.pid", altpidpath(),
		ups->driver, xbasename(ups->port));
	ret = stat(pidfn, &fs);

	if (ret != 0) {
		snprintf(pidfn, sizeof(pidfn), "%s/%s-%s.pid", altpidpath(),
			ups->driver, ups->upsname);
		ret = stat(pidfn, &fs);
	}

	if (ret != 0) {
		upslog(LOG_ERR, "Can't open %s", pidfn);
		exec_error++;
		return 0;
	}

	pidf = fopen(pidfn, "r");

	if (!pidf) {
		upslog(LOG_ERR, "Can't open %s", pidfn);
		exec_error++;
		return 0;
	}

	fgets(buf, sizeof(buf), pidf);
	buf[strlen(buf)-1] = '\0';

	pid = strtol(buf, (char **)NULL, 10);

	if (pid < 2) {
		upslogx(LOG_NOTICE, "Ignoring invalid pid %d in %s",
			pid, pidfn);
		exec_error++;
		return 0;
	}

	return pid;
}

/* handle sending the signal */
static void stop_driver(const ups_t *ups)
{
	int	ret, pid;

	printf("Stopping UPS: %s\n", ups->upsname);

	pid = get_driver_pid(ups);

	if (pid < 2)
		return;

	if (verbose)
		printf("Sending signal: kill -TERM %d\n", pid);

	if (testmode)
		return;

	ret = kill(pid, SIGTERM);

	if (ret < 0) {
		upslog(LOG_ERR, "kill pid %d failed", pid);
		exec_error++;
		return;
	}
}

static void status_driver(const ups_t *ups)
{
	/*
	 * Check if the pid file is present (this is not a real
	 * status check, but probably sufficient in most cases)
	 */
	get_driver_pid(ups);
}

static void waitpid_timeout(const int sig)
{
	/* do nothing */
	return;
}

static void forkexec(const char *prog, char **argv, const ups_t *ups)
{
	int	ret;
	pid_t	pid;

	pid = fork();

	if (pid < 0)
		fatal("fork");

	if (pid != 0) {			/* parent */
		int	wstat;

		sigemptyset(&nut_upsdrvctl_sigmask);
		sa.sa_mask = nut_upsdrvctl_sigmask;
		sa.sa_flags = 0;
		sa.sa_handler = waitpid_timeout;
		sigaction(SIGALRM, &sa, NULL);

		if (ups->maxstartdelay != -1)
			alarm(ups->maxstartdelay);
		else
			alarm(maxstartdelay);

		ret = waitpid(pid, &wstat, 0);

		signal(SIGALRM, SIG_IGN);
		alarm(0);

		if (ret == -1) {
			upslogx(LOG_WARNING, "Startup timer elapsed, continuing...");
			exec_error++;
			return;
		}

		if (WIFEXITED(wstat) == 0) {
			upslogx(LOG_WARNING, "Driver exited abnormally");
			exec_error++;
			return;
		}

		if (WEXITSTATUS(wstat) != 0) {
			upslogx(LOG_WARNING, "Driver failed to start"
			" (exit status=%d)", WEXITSTATUS(wstat));
			exec_error++;
			return;
		}

		/* the rest only work when WIFEXITED is nonzero */

		if (WIFSIGNALED(wstat)) {
			upslog(LOG_WARNING, "Driver died after signal %d",
				WTERMSIG(wstat));
			exec_error++;
		}

		return;
	}

	/* child */

	ret = execv(prog, argv);

	/* shouldn't get here */
	fatal("execv");
}		

static void start_driver(const ups_t *ups)
{
	char	dfn[SMALLBUF], *argv[8];
	int	ret, arg = 0;
	struct	stat	fs;

	snprintf(dfn, sizeof(dfn), "%s/%s", driverpath, ups->driver);
	ret = stat(dfn, &fs);

	if (ret < 0)
		fatal("Can't start %s", dfn);

	if (verbose)
	    	printf("exec: %s -a %s", dfn, ups->upsname);

	argv[arg++] = xstrdup(dfn);

	argv[arg++] = "-a";
	argv[arg++] = ups->upsname;

	/* stick on the chroot / user args if given to us */
	if (pt_root) {
		argv[arg++] = "-r";
		argv[arg++] = pt_root;

		if (verbose)
			printf(" -r %s", pt_root);
	}

	if (pt_user) {
		argv[arg++] = "-u";
		argv[arg++] = pt_user;

		if (verbose)
			printf(" -u %s", pt_user);
	}

	if (verbose)
		printf("\n");
	if (testmode)
	    	return;

	/* tie it off */
	argv[arg++] = NULL;

	forkexec(dfn, argv, ups);
}

static void help(const char *progname)
{
	printf("Starts and stops UPS drivers via ups.conf.\n\n");
	printf("usage: %s [OPTIONS] (start | stop | shutdown) [<ups>]\n\n", progname);

	printf("  -h			display this help\n");
	printf("  -r <path>		drivers will chroot to <path>\n");
	printf("  -t			testing mode - prints actions without doing them\n");
	printf("  -u <user>		drivers started will switch from root to <user>\n");
	printf("  -v			enable verbose messages\n");
	printf("  start			start all UPS drivers in ups.conf\n");
	printf("  start	<ups>		only start driver for UPS <ups>\n");
	printf("  stop			stop all UPS drivers in ups.conf\n");
	printf("  stop <ups>		only stop driver for UPS <ups>\n");
	printf("  status		status of all UPS drivers in ups.conf\n");
	printf("  status <ups>		only status of driver for UPS <ups>\n");
	printf("  shutdown		shutdown all UPS drivers in ups.conf\n");
	printf("  shutdown <ups>	only shutdown UPS <ups>\n");

	exit(EXIT_SUCCESS);
}

static void shutdown_driver(const ups_t *ups)
{
	char	*argv[7], dfn[SMALLBUF];

	snprintf(dfn, sizeof(dfn), "%s/%s", driverpath, ups->driver);

	if (verbose)
		printf("exec: %s -a %s -k\n", dfn, ups->upsname);
	if (testmode)
	    	return;

	argv[0] = dfn;
	argv[1] = "-a";
	argv[2] = xstrdup(ups->upsname);
	argv[3] = "-k";
	argv[4] = NULL;

	forkexec(dfn, argv, ups);
}

static void send_one_driver(void (*command)(const ups_t *), const char *upsname)
{
	ups_t	*ups = upstable;

	if (!ups)
		fatalx("Error: no UPS definitions found in ups.conf!\n");

	while (ups) {
		if (!strcmp(ups->upsname, upsname)) {
			command(ups);
			return;
		}

		ups = ups->next;
	}

	fatalx("UPS %s not found in ups.conf", upsname);
}

/* walk UPS table and send command to all UPSes according to sdorder */
static void send_all_drivers(void (*command)(const ups_t *))
{
	ups_t	*ups;
	int	i;

	if (!upstable)
		fatalx("Error: no UPS definitions found in ups.conf");

	for (i = 0; i <= maxsdorder; i++) {
		ups = upstable;

		while (ups) {
			if (ups->sdorder == i)
				command(ups);
			
			ups = ups->next;
		}
	}
}

static void exit_cleanup(void)
{
	ups_t	*tmp, *next;

	tmp = upstable;

	while (tmp) {
		next = tmp->next;

		if (tmp->driver)
			free(tmp->driver);

		if (tmp->port)
			free(tmp->port);

		if (tmp->upsname)
			free(tmp->upsname);
		free(tmp);

		tmp = next;
	}

	if (driverpath)
		free(driverpath);
}

int main(int argc, char **argv)
{
	int	i;
	char	*prog;
	void	(*command)(const ups_t *) = NULL;

	printf("Network UPS Tools - UPS driver controller %s\n",
		UPS_VERSION);

	prog = argv[0];
	while ((i = getopt(argc, argv, "+htvu:r:V")) != EOF) {
		switch(i) {
			case 'r':
				pt_root = optarg;
				break;

			case 't':
				testmode = 1;

				/* force verbose mode while testing */
				if (verbose == 0)
					verbose = 1;

				break;

			case 'u':
				pt_user = optarg;
				break;

			case 'v':
				verbose++;
				break;

			case 'V':
				exit(EXIT_SUCCESS);

			case 'h':
			default:
				help(prog);
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		help(prog);

	if (testmode)
		printf("*** Testing mode: not calling exec/kill\n");

        if (!strcmp(argv[0], "start"))
		command = &start_driver;

        if (!strcmp(argv[0], "stop"))
		command = &stop_driver;

        if (!strcmp(argv[0], "shutdown"))
		command = &shutdown_driver;

        if (!strcmp(argv[0], "status"))
		command = &status_driver;

	if (!command)
	        fatalx("Error: unrecognized command [%s]", argv[0]);

	driverpath = xstrdup(DRVPATH);  /* set default */

	atexit(exit_cleanup);

	read_upsconf();

	if (argc == 1)
		send_all_drivers(command);
	else
		send_one_driver(command, argv[1]);

	if (exec_error)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
